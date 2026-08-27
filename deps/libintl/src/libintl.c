/*
 * libintl.c — minimal gettext (.mo) implementation for HarmonyOS / Celestia
 *
 * HarmonyOS NDK ships libintl.h but no libintl.so, and its libc has no
 * gettext symbols. This standalone implementation provides the subset of
 * the GNU gettext API used by Celestia, reading standard .mo catalogs.
 *
 * Catalog lookup path: <bindtextdomain dir>/<LANG>/LC_MESSAGES/<domain>.mo
 * LANG is taken from the environment (e.g. zh_CN.UTF-8); when absent or
 * unparseable, defaults to "zh_CN".
 */

#include <libintl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

/* Same renaming as celutil/gettext.h: musl's dynamic linker exports a
   gettext() stub that shadows our implementation; bind to cel_* names. */
#define gettext cel_gettext
#define dgettext cel_dgettext
#define dcgettext cel_dcgettext
#define ngettext cel_ngettext
#define dngettext cel_dngettext
#define dcngettext cel_dcngettext
#define bindtextdomain cel_bindtextdomain
#define bind_textdomain_codeset cel_bind_textdomain_codeset
#define textdomain cel_textdomain

#define MAX_DOMAINS 8
#define MAX_DOMAIN_NAME 64
#define MAX_DIR 512

typedef struct {
    char      domain[MAX_DOMAIN_NAME];
    char      dir[MAX_DIR];
    char      codeset[32];
    uint8_t  *mo;
    size_t    mo_size;
    int       loaded;      /* 0 = not tried, 1 = ok, -1 = failed */
} DomainState;

static DomainState g_domains[MAX_DOMAINS];
static int g_n_domains = 0;
static char g_current_domain[MAX_DOMAIN_NAME] = "messages";

static DomainState *
find_domain(const char *domain)
{
    for (int i = 0; i < g_n_domains; i++) {
        if (strcmp(g_domains[i].domain, domain) == 0)
            return &g_domains[i];
    }
    return NULL;
}

static DomainState *
get_domain(const char *domain)
{
    DomainState *ds = find_domain(domain);
    if (ds != NULL)
        return ds;
    if (g_n_domains >= MAX_DOMAINS)
        return NULL;
    ds = &g_domains[g_n_domains++];
    memset(ds, 0, sizeof(*ds));
    strncpy(ds->domain, domain, MAX_DOMAIN_NAME - 1);
    strncpy(ds->codeset, "UTF-8", sizeof(ds->codeset) - 1);
    return ds;
}

static uint32_t
rd32(const uint8_t *p, int bigendian)
{
    if (bigendian)
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
             | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void
strip_encoding(char *lang)
{
    char *dot = strchr(lang, '.');
    if (dot != NULL)
        *dot = '\0';
    char *at = strchr(lang, '@');
    if (at != NULL)
        *at = '\0';
}

static int
load_mo(DomainState *ds)
{
    if (ds->loaded != 0)
        return ds->loaded;

    /* Locale candidates: environment LANG (stripped), then zh_CN fallback.
       HarmonyOS sandboxes usually have no LANG, so zh_CN is used. */
    char langs[2][64];
    memset(langs, 0, sizeof(langs));
    int n_langs = 0;
    const char *env = getenv("LANG");
    if (env != NULL && env[0] != '\0') {
        strncpy(langs[n_langs], env, sizeof(langs[0]) - 1);
        langs[n_langs][sizeof(langs[0]) - 1] = '\0';
        strip_encoding(langs[n_langs]);
        if (langs[n_langs][0] != '\0')
            n_langs++;
    }
    if (n_langs == 0 || (n_langs == 1 && strcmp(langs[0], "zh_CN") != 0))
        strcpy(langs[n_langs++], "zh_CN");

    for (int li = 0; li < n_langs; li++) {
        char path[1536];
        snprintf(path, sizeof(path), "%s/%s/LC_MESSAGES/%s.mo",
                 ds->dir, langs[li], ds->domain);

        int fd = open(path, O_RDONLY);
        if (fd < 0)
            continue;
        struct stat st;
        if (fstat(fd, &st) != 0 || st.st_size < 20) {
            close(fd);
            continue;
        }
        ds->mo_size = (size_t)st.st_size;
        ds->mo = (uint8_t *)malloc(ds->mo_size);
        if (ds->mo == NULL) {
            close(fd);
            continue;
        }
        ssize_t r = read(fd, ds->mo, ds->mo_size);
        close(fd);
        if (r != (ssize_t)ds->mo_size) {
            free(ds->mo);
            ds->mo = NULL;
            continue;
        }
        ds->loaded = 1;
        return 1;
    }
    ds->loaded = -1;
    return -1;
}

static const char *
lookup(DomainState *ds, const char *msgid)
{
    if (load_mo(ds) != 1 || ds->mo_size < 20)
        return NULL;

    const uint8_t *p = ds->mo;
    int bigendian = 0;
    /* bytes de 12 04 95 = magic 0x950412de stored little-endian */
    if (p[0] == 0xde && p[1] == 0x12 && p[2] == 0x04 && p[3] == 0x95) {
        bigendian = 0;
    /* bytes 95 04 12 de = magic 0x950412de stored big-endian */
    } else if (p[0] == 0x95 && p[1] == 0x04 && p[2] == 0x12 && p[3] == 0xde) {
        bigendian = 1;
    } else {
        return NULL;
    }

    uint32_t nstrings  = rd32(p + 8,  bigendian);
    uint32_t orig_off  = rd32(p + 12, bigendian);
    uint32_t trans_off = rd32(p + 16, bigendian);

    if (nstrings == 0 || orig_off + (uint64_t)nstrings * 8 > ds->mo_size ||
        trans_off + (uint64_t)nstrings * 8 > ds->mo_size)
        return NULL;

    for (uint32_t i = 0; i < nstrings; i++) {
        const uint8_t *oe = p + orig_off + i * 8;
        uint32_t olen = rd32(oe, bigendian);
        uint32_t ooff = rd32(oe + 4, bigendian);
        if (ooff + olen > ds->mo_size)
            continue;
        if (olen == strlen(msgid) && memcmp(p + ooff, msgid, olen) == 0) {
            const uint8_t *te = p + trans_off + i * 8;
            uint32_t tlen = rd32(te, bigendian);
            uint32_t toff = rd32(te + 4, bigendian);
            if (toff + tlen > ds->mo_size || tlen == 0)
                return NULL;
            return (const char *)(p + toff);
        }
    }
    return NULL;
}

/* ---------------- exported API ---------------- */

char *dgettext(const char *domainname, const char *msgid); /* forward decl */

char *
gettext(const char *msgid)
{
    return dgettext(g_current_domain, msgid);
}

char *
dgettext(const char *domainname, const char *msgid)
{
    if (domainname == NULL || msgid == NULL)
        return (char *)msgid;
    DomainState *ds = get_domain(domainname);
    if (ds == NULL)
        return (char *)msgid;
    const char *tr = lookup(ds, msgid);
    return (char *)(tr != NULL ? tr : msgid);
}

char *
dcgettext(const char *domainname, const char *msgid, int category)
{
    (void)category;
    return dgettext(domainname, msgid);
}

char *
ngettext(const char *msgid1, const char *msgid2, unsigned long int n)
{
    return dgettext(g_current_domain, n == 1 ? msgid1 : msgid2);
}

char *
dngettext(const char *domainname, const char *msgid1, const char *msgid2,
          unsigned long int n)
{
    return dgettext(domainname, n == 1 ? msgid1 : msgid2);
}

char *
dcngettext(const char *domainname, const char *msgid1, const char *msgid2,
           unsigned long int n, int category)
{
    (void)category;
    return dgettext(domainname, n == 1 ? msgid1 : msgid2);
}

char *
bindtextdomain(const char *domainname, const char *dirname)
{
    if (domainname == NULL)
        return NULL;
    DomainState *ds = get_domain(domainname);
    if (ds == NULL)
        return NULL;
    if (dirname != NULL)
        strncpy(ds->dir, dirname, MAX_DIR - 1);
    if (ds->mo != NULL) {
        free(ds->mo);
        ds->mo = NULL;
    }
    ds->loaded = 0;
    return ds->dir[0] != '\0' ? ds->dir : NULL;
}

char *
bind_textdomain_codeset(const char *domainname, const char *codeset)
{
    if (domainname == NULL)
        return NULL;
    DomainState *ds = get_domain(domainname);
    if (ds == NULL)
        return NULL;
    if (codeset != NULL)
        strncpy(ds->codeset, codeset, sizeof(ds->codeset) - 1);
    return ds->codeset;
}

char *
textdomain(const char *domainname)
{
    if (domainname == NULL)
        return g_current_domain;
    strncpy(g_current_domain, domainname, MAX_DOMAIN_NAME - 1);
    return g_current_domain;
}
