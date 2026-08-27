#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 Celestia 的 celestia-data.mo（天体名中文本地化）
==================================================
Celestia 天体名显示用 dgettext("celestia-data", name)，需独立 .mo：
locale/zh_CN/LC_MESSAGES/celestia-data.mo（上游未提供，这里补齐）。
仅影响显示，不影响内部引用（findObjectFromPath 用非本地化名）。

用法：
  python3 tools/gen-celestia-data-mo.py
产物：
  app/entry/src/main/resources/rawfile/celestia/locale/zh_CN/LC_MESSAGES/celestia-data.mo
"""
import struct
import sys
import os

# 天体名 → 中文（标准天文学译名）
TRANSLATIONS = {
    # 恒星
    "Sun": "太阳",
    # 行星
    "Mercury": "水星", "Venus": "金星", "Earth": "地球",
    "Mars": "火星", "Jupiter": "木星", "Saturn": "土星",
    "Uranus": "天王星", "Neptune": "海王星",
    # 月球
    "Moon": "月球",
    # 火星卫星
    "Phobos": "火卫一", "Deimos": "火卫二",
    # 木星伽利略卫星
    "Io": "木卫一", "Europa": "木卫二",
    "Ganymede": "木卫三", "Callisto": "木卫四",
    # 土星主要卫星
    "Mimas": "土卫一", "Enceladus": "土卫二", "Tethys": "土卫三",
    "Dione": "土卫四", "Rhea": "土卫五", "Titan": "土卫六",
    "Hyperion": "土卫七", "Iapetus": "土卫八", "Phoebe": "土卫九",
    # 天王星主要卫星
    "Ariel": "天卫一", "Umbriel": "天卫二", "Titania": "天卫三",
    "Oberon": "天卫四", "Miranda": "天卫五",
    # 海王星卫星
    "Triton": "海卫一", "Nereid": "海卫二",
    # 冥王星
    "Pluto": "冥王星", "Charon": "冥卫一",
    # 矮行星
    "Ceres": "谷神星", "Vesta": "灶神星", "Pallas": "智神星",
    "Hygiea": "健神星", "Eris": "阋神星", "Makemake": "鸟神星",
    "Haumea": "妊神星", "Quaoar": "创神星", "Sedna": "塞德娜",
    "Orcus": "亡神星", "Gonggong": "共工星",
    # 小行星（选录）
    "Apophis": "毁神星", "Bennu": "贝努", "Itokawa": "糸川",
    "Psyche": "灵神星", "Lutetia": "鲁特西亚", "Mathilde": "玛蒂尔达",
    "Gaspra": "加斯普拉", "Ida": "艾达", "Dinkinesh": "丁基内什",
    # 彗星（选录）
    "Halley": "哈雷彗星", "Hale-Bopp": "海尔-波普彗星",
    "Hyakutake": "百武彗星", "Encke": "恩克彗星",
    "Tempel 1": "坦普尔1号彗星", "Churyumov-Gerasimenko": "丘留莫夫-格拉西缅科彗星",
    "Wild 2": "维尔特2号彗星",
    # 大陆 / 大洋（earth_locs）
    "AFRICA": "非洲", "ASIA": "亚洲", "EUROPE": "欧洲",
    "NORTH AMERICA": "北美洲", "SOUTH AMERICA": "南美洲",
    "ANTARCTICA": "南极洲", "AUSTRALIA": "大洋洲",
    "ATLANTIC OCEAN": "大西洋", "PACIFIC OCEAN": "太平洋",
    "INDIAN OCEAN": "印度洋", "ARCTIC OCEAN": "北冰洋",
    "SOUTHERN OCEAN": "南大洋",
}

def write_mo(path, catalog):
    """把 {msgid: msgstr} 写成 .mo（小端，无 hash 表，msgid 排序）。
    标准 .mo 头 28 字节（7 个 uint32），且首条为 header（msgid=""）。
    """
    header_msg = (
        "Project-Id-Version: celestia-data\n"
        "Report-Msgid-Bugs-To: \n"
        "MIME-Version: 1.0\n"
        "Content-Type: text/plain; charset=UTF-8\n"
        "Content-Transfer-Encoding: 8bit\n"
        "Plural-Forms: nplurals=1; plural=0;\n"
    )
    items = [("", header_msg)] + sorted(catalog.items())
    n = len(items)

    # 计算偏移：头 28 字节 + 两张表
    header_len = 28
    orig_table = header_len
    trans_table = orig_table + n * 8
    data_start = trans_table + n * 8

    # 先分开收集字符串，再统一布局：
    #   头(28) | orig_table(n*8) | trans_table(n*8) | 全部 orig_data | 全部 trans_data
    orig_parts = []
    trans_parts = []
    for k, v in items:
        orig_parts.append(k.encode("utf-8"))
        trans_parts.append(v.encode("utf-8"))

    orig_total = sum(len(kb) + 1 for kb in orig_parts)
    data_start = trans_table + n * 8

    orig_offsets = []
    trans_offsets = []
    o_off = data_start
    t_off = data_start + orig_total
    for kb, vb in zip(orig_parts, trans_parts):
        orig_offsets.append((len(kb), o_off))
        o_off += len(kb) + 1
        trans_offsets.append((len(vb), t_off))
        t_off += len(vb) + 1

    orig_data = b"".join(kb + b"\x00" for kb in orig_parts)
    trans_data = b"".join(vb + b"\x00" for vb in trans_parts)

    # 7 个 uint32：magic, version, n, orig_table, trans_table, hash_size, hash_off
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
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = os.path.join(root, "app/entry/src/main/resources/rawfile/celestia",
                       "locale/zh_CN/LC_MESSAGES/celestia-data.mo")
    n = write_mo(out, TRANSLATIONS)
    print(f"已生成 {out}")
    print(f"翻译条目: {n}")

    # 自校验：用 python gettext 读回
    import gettext
    localedir = os.path.join(root, "app/entry/src/main/resources/rawfile/celestia", "locale")
    mo = gettext.translation("celestia-data", localedir, languages=["zh_CN"])
    t = mo.gettext
    for k in ["Earth", "Moon", "Mars", "Sun", "Titan", "AFRICA"]:
        print(f"  校验 {k!r} -> {t(k)!r}")

if __name__ == "__main__":
    main()
