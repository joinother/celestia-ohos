#!/usr/bin/env python3
"""Convert a gettext .po file to a minimal .mo (little-endian, no hash table).

Handles: msgctxt (C_ contexts -> "ctx\\x04msgid" keys), multi-line strings,
escapes (\\n \\t \\" \\\\ \\r and \\xNN).
"""
import re, struct, sys

def unescape(s):
    out = []
    i = 0
    while i < len(s):
        c = s[i]
        if c == '\\' and i + 1 < len(s):
            n = s[i+1]
            if n == 'n': out.append('\n'); i += 2; continue
            if n == 't': out.append('\t'); i += 2; continue
            if n == 'r': out.append('\r'); i += 2; continue
            if n == '"': out.append('"'); i += 2; continue
            if n == '\\': out.append('\\'); i += 2; continue
            if n == 'x' and i + 3 < len(s):
                h = s[i+2:i+4]
                if re.fullmatch(r'[0-9a-fA-F]{2}', h):
                    out.append(chr(int(h, 16))); i += 4; continue
            out.append(c); i += 1
        else:
            out.append(c); i += 1
    return ''.join(out)

def read_string(lines, i, kw):
    """Starting at a line beginning with 'kw ', consume it plus following
    continuation lines. Return (concatenated_raw, next_index)."""
    if not lines[i].startswith(kw + ' '):
        raise ValueError(f'expected {kw} at line {i}: {lines[i]!r}')
    first = lines[i][len(kw)+1:].strip()
    m0 = re.fullmatch(r'"((?:[^"\\]|\\.)*)"', first)
    if not m0:
        raise ValueError(f'bad {kw} first line {i}: {first!r}')
    parts = [m0.group(1)]
    i += 1
    while i < len(lines):
        t = lines[i].strip()
        m = re.fullmatch(r'"((?:[^"\\]|\\.)*)"', t)
        if not m:
            break
        parts.append(m.group(1))
        i += 1
    return ''.join(parts), i

def parse_po(path):
    with open(path, encoding='utf-8') as f:
        text = f.read()
    lines = text.split('\n')
    entries = []
    i, n = 0, len(lines)
    while i < n:
        line = lines[i].strip()
        if line.startswith('msgctxt '):
            ctx, i = read_string(lines, i, 'msgctxt')
            msgid, i = read_string(lines, i, 'msgid')
            msgstr, i = read_string(lines, i, 'msgstr')
            key = unescape(ctx) + '\x04' + unescape(msgid)
            entries.append((key, unescape(msgstr)))
        elif line.startswith('msgid '):
            msgid, i = read_string(lines, i, 'msgid')
            msgstr, i = read_string(lines, i, 'msgstr')
            entries.append((unescape(msgid), unescape(msgstr)))
        else:
            i += 1
    return entries

def build_mo(entries, out_path):
    ents = [(k.encode('utf-8'), v.encode('utf-8'))
            for k, v in entries if v != '']
    ents.sort(key=lambda x: x[0])
    N = len(ents)
    orig_off = 28
    trans_off = orig_off + 8 * N
    data_start = trans_off + 8 * N
    pos = data_start
    orig = []
    for ob, tb in ents:
        orig.append((len(ob), pos)); pos += len(ob) + 1
    trans = []
    for ob, tb in ents:
        trans.append((len(tb), pos)); pos += len(tb) + 1
    buf = bytearray()
    buf += struct.pack('<IIIIIII', 0x950412de, 0, N, orig_off, trans_off, 0, 0)
    for ln, off in orig: buf += struct.pack('<II', ln, off)
    for ln, off in trans: buf += struct.pack('<II', ln, off)
    for ob, tb in ents: buf += ob + b'\x00'
    for ob, tb in ents: buf += tb + b'\x00'
    with open(out_path, 'wb') as f:
        f.write(buf)
    print(f'{out_path}: {len(buf)} bytes, {N} translated entries')

if __name__ == '__main__':
    po, mo = sys.argv[1], sys.argv[2]
    entries = parse_po(po)
    build_mo(entries, mo)
