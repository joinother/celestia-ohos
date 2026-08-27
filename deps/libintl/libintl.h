/* libintl.h — minimal GNU gettext API declarations for HarmonyOS/Celestia.
   The OHOS NDK ships an empty libintl.h; this header declares the subset of
   the gettext API implemented by deps/libintl/src/libintl.c.

   HarmonyOS runs on musl, whose dynamic linker (ld-musl-*.so.1) exports a
   gettext() stub that returns msgid unchanged and shadows any implementation
   loaded later.  To bind Celestia's translation calls to OUR implementation,
   the whole API is renamed to cel_* (see celutil/gettext.h which maps the
   standard names onto cel_*).  This header therefore declares the cel_* names
   directly and also provides the standard-name -> cel_* macro mapping. */
#ifndef _LIBINTL_H
#define _LIBINTL_H

#ifdef __cplusplus
extern "C" {
#endif

#define __USE_GNU_GETTEXT 1
#define __GNU_GETTEXT_SUPPORTED_REVISION(major) ((major) == 0 ? 1 : -1)

char *cel_gettext(const char *msgid);
char *cel_dgettext(const char *domainname, const char *msgid);
char *cel_dcgettext(const char *domainname, const char *msgid, int category);
char *cel_ngettext(const char *msgid1, const char *msgid2, unsigned long int n);
char *cel_dngettext(const char *domainname, const char *msgid1,
                    const char *msgid2, unsigned long int n);
char *cel_dcngettext(const char *domainname, const char *msgid1,
                     const char *msgid2, unsigned long int n, int category);
char *cel_bindtextdomain(const char *domainname, const char *dirname);
char *cel_bind_textdomain_codeset(const char *domainname, const char *codeset);
char *cel_textdomain(const char *domainname);

/* Map the standard GNU gettext names onto the cel_* implementation. */
#define gettext cel_gettext
#define dgettext cel_dgettext
#define dcgettext cel_dcgettext
#define ngettext cel_ngettext
#define dngettext cel_dngettext
#define dcngettext cel_dcngettext
#define bindtextdomain cel_bindtextdomain
#define bind_textdomain_codeset cel_bind_textdomain_codeset
#define textdomain cel_textdomain

#ifdef __cplusplus
}
#endif

#endif
