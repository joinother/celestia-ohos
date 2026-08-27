#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Celestia 鸿蒙移植 — 资源完整性审计脚本
======================================
扫描 rawfile/celestia 数据目录，核对：
  1. celestia.cfg 引用的路径是否存在
  2. 所有 .ssc 引用的 Mesh/Texture 模型是否可解析（Celestia 搜索路径）
  3. 输出体积统计（供上架体积评估）

用法：
  python3 tools/check-assets.py [数据目录]

默认数据目录：app/entry/src/main/resources/rawfile/celestia
退出码：0 = 无真实缺失；1 = 存在缺失
"""
import os
import re
import sys

DATA = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                        "app/entry/src/main/resources/rawfile/celestia")

def audit_cfg():
    """审计 celestia.cfg 引用的路径。"""
    cfg = os.path.join(DATA, "celestia.cfg")
    text = open(cfg, encoding="utf-8", errors="replace").read()
    paths = re.findall(r'"([^"]+)"', text)
    checked, missing = set(), []
    for p in paths:
        if p in checked or '*' in p or '\\' in p or '#' in p or ':' in p:
            continue
        checked.add(p)
        # 过滤明显非路径的 cfg 值（纯扩展名、字体格式、可执行段等）
        if p.startswith('.') or ',' in p or p in ("extras", "Run Demo"):
            continue
        if not ('.' in p or '/' in p):
            continue
        if not os.path.exists(os.path.join(DATA, p)):
            # 可选资产（运行时生成 / 旧版 extras 扩展），不判为缺失
            if p in ("favorites.cel", "extras/wostars.stc"):
                print(f"  [可选] {p}（运行时自动生成 / 旧版 extras 扩展，可忽略）")
                continue
            missing.append(p)
    return checked, missing

def audit_ssc():
    """审计所有 .ssc 引用的 Mesh/Texture。"""
    missing, checked = [], 0
    tex_roots = ["textures/medres", "textures/lores", "textures/hires"]
    for root, _, files in os.walk(DATA):
        for fn in files:
            if not fn.endswith(".ssc"):
                continue
            fp = os.path.join(root, fn)
            text = open(fp, encoding="utf-8", errors="replace").read()
            for m in re.finditer(r'(Mesh|Texture)\s+"([^"]+)"', text, re.I):
                asset = m.group(2)
                checked += 1
                if '*' in asset or asset.lower().endswith('.dds'):
                    continue
                cands = [
                    os.path.normpath(os.path.join(root, asset)),
                    os.path.normpath(os.path.join(root, "models", asset)),
                    os.path.normpath(os.path.join(DATA, "models", asset)),
                ]
                # 全局纹理目录（Celestia 对 Texture 的默认搜索路径）
                for tr in tex_roots:
                    cands.append(os.path.normpath(os.path.join(DATA, tr, asset)))
                if not any(os.path.exists(c) for c in cands):
                    missing.append((os.path.relpath(fp, DATA), asset))
    return checked, missing

def volume_report():
    """体积统计。"""
    sizes, total = {}, 0
    for root, _, files in os.walk(DATA):
        for f in files:
            fp = os.path.join(root, f)
            sz = os.path.getsize(fp)
            total += sz
            top = os.path.relpath(root, DATA).split(os.sep)[0]
            sizes[top] = sizes.get(top, 0) + sz
    return total, sizes

def main():
    if not os.path.exists(os.path.join(DATA, "celestia.cfg")):
        print(f"错误：{DATA} 下未找到 celestia.cfg")
        return 2

    ok = True
    # 1. cfg 引用
    cfg_checked, cfg_missing = audit_cfg()
    print(f"[cfg] 引用 {len(cfg_checked)} 个路径，缺失 {len(cfg_missing)}")
    for m in cfg_missing:
        print(f"  MISSING: {m}")
        ok = False

    # 2. ssc 资产
    ssc_checked, ssc_missing = audit_ssc()
    print(f"[ssc] 引用 Mesh/Texture {ssc_checked} 个，缺失 {len(set(a for _,a in ssc_missing))} 个唯一资产")
    seen = set()
    for src, a in ssc_missing:
        if a in seen:
            continue
        seen.add(a)
        print(f"  MISSING: [{src}] -> {a}")
        ok = False

    # 3. 体积
    total, sizes = volume_report()
    print(f"\n总体积 {total/1e6:.1f} MB")
    for k in sorted(sizes, key=lambda x: -sizes[x]):
        print(f"  {k:20s} {sizes[k]/1e6:8.1f} MB")

    print("\n结论: " + ("全部资源完整" if ok else "存在缺失，需修复"))
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
