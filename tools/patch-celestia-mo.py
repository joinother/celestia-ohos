#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
修正 celestia.mo 的翻译瑕疵（保留全部 931 条）
==========================================
读现有 celestia.mo → 修正指定 key → 写回（无 plural 条目，可安全重写）。
当前修正：Equatorial (polar) radii 翻译丢第二个占位符。

用法：
  python3 tools/patch-celestia-mo.py
"""
import gettext
import struct
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MO_PATH = os.path.join(ROOT, "app/entry/src/main/resources/rawfile/celestia",
                       "locale/zh_CN/LC_MESSAGES/celestia.mo")
# locale 目录 = LC_MESSAGES -> zh_CN -> locale
LOCALEDIR = os.path.dirname(os.path.dirname(os.path.dirname(MO_PATH)))

# 需要修正的翻译：{msgid: 新 msgstr}
FIXES = {
    "Equatorial (polar) radii: {} ({})\n": "赤道（极）半径：{} ({})\n",
}


def write_mo(path, catalog):
    """把 {msgid: msgstr} 写成 .mo（无 plural，无 hash 表，msgid 排序）。"""
    header_msg = (
        "Project-Id-Version: celestia\n"
        "Report-Msgid-Bugs-To: \n"
        "MIME-Version: 1.0\n"
        "Content-Type: text/plain; charset=UTF-8\n"
        "Content-Transfer-Encoding: 8bit\n"
        "Plural-Forms: nplurals=1; plural=0;\n"
    )
    items = [("", header_msg)] + sorted(catalog.items())
    n = len(items)
    header_len = 28
    orig_table = header_len
    trans_table = orig_table + n * 8
    data_start = trans_table + n * 8

    orig_parts = []
    trans_parts = []
    for k, v in items:
        orig_parts.append(k.encode("utf-8"))
        trans_parts.append(v.encode("utf-8"))

    orig_total = sum(len(kb) + 1 for kb in orig_parts)
    orig_offsets, trans_offsets = [], []
    o_off, t_off = data_start, data_start + orig_total
    for kb, vb in zip(orig_parts, trans_parts):
        orig_offsets.append((len(kb), o_off))
        o_off += len(kb) + 1
        trans_offsets.append((len(vb), t_off))
        t_off += len(vb) + 1

    orig_data = b"".join(kb + b"\x00" for kb in orig_parts)
    trans_data = b"".join(vb + b"\x00" for vb in trans_parts)

    out = struct.pack("<IIIIIII", 0x950412de, 0, n, orig_table, trans_table, 0, 0)
    for l, o in orig_offsets:
        out += struct.pack("<II", l, o)
    for l, o in trans_offsets:
        out += struct.pack("<II", l, o)
    out += orig_data + trans_data

    with open(path, "wb") as f:
        f.write(out)
    return n


def main():
    # 备份原始 mo
    bak = MO_PATH + ".bak"
    if not os.path.exists(bak):
        import shutil
        shutil.copy2(MO_PATH, bak)
        print(f"已备份到 {bak}")

    mo = gettext.translation("celestia", LOCALEDIR, languages=["zh_CN"])
    catalog = dict(mo._catalog)
    print(f"读入 {len(catalog)} 条（含 plural 0 条）")

    n_fixed = 0
    for msgid, newstr in FIXES.items():
        if msgid in catalog:
            old = catalog[msgid]
            catalog[msgid] = newstr
            n_fixed += 1
            print(f"  修正 {msgid!r}: {old!r} -> {newstr!r}")
        else:
            print(f"  [未找到] {msgid!r}")

    n = write_mo(MO_PATH, catalog)
    print(f"已写回 {MO_PATH}，共 {n} 条")

    # 校验
    mo2 = gettext.translation("celestia", LOCALEDIR, languages=["zh_CN"])
    t = mo2.gettext
    for k in ["Equatorial (polar) radii: {} ({})\n", "Distance: %s\n", "Real time"]:
        print(f"  校验 {k!r:.35} -> {t(k)!r}")


if __name__ == "__main__":
    main()
