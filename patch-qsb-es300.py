#!/usr/bin/env python3
"""
Celestia 鸿蒙移植 · Qt6 合成器 shader qsb 补丁
================================================
【背景】模拟器（OpenGL ES 3.0 / Metal 模拟 Mali-G77）无法编译 Qt 合成器
backingstorecompose 的 ES 1.00 shader（uniform block 具名语法在 140 语义下炸，
报 'version 450 is not supported'）→ pipeline 创建失败 → setGraphicsPipeline(null)
崩溃（0x6517e4）。

【方案】把 qsb 里 100-ES 变体升级为 #version 300 es 语法（key.version 100→300，
源码 varying/attribute→in/out、gl_FragData[0]→out fragColor、texture2D→texture）。
模拟器支持 300 es（qtprecheck 已验证）。

【关键坑（踩过的）】
1. QResource 存储格式 = [4B 未压缩大小][zlib 流]，QFile::readAll 原样返回，
   QShader::fromSerialized 内部用 Qt qUncompress（要求 [4B 大端长度][zlib]）。
   size 字段必须按【大端】写入，否则 qUncompress 校验失败 → 'unknown version 0'。
2. 新 zlib 压缩流必须 ≤ 旧槽位（vert 1404B / frag 1653B），用 0x00 填充。
   - vert: var0+var1 都用 300-es（提高 zlib 重复度）→ 1393B
   - frag: var0 基于 var2(150) 改造 → 1653B（正好）
3. 备份原版到 libQt6Gui.so.orig.bak。

【验证】部署后合成器无 "Failed to build graphics pipeline"，Celestia 完整 GUI
（菜单/浏览器面板/对象信息）正常渲染，进程稳定。

【用法】python3 patch-qsb-es300.py  <libQt6Gui.so 路径>
"""
import struct, zlib, shutil, sys

# 偏移（libQt6Gui.so 内 backingstorecompose 两个 qsb 的资源数据位置）
OFFSETS = {
    'vert': (0x184824, 0x184828, 1404),   # (size 字段偏移, zlib 偏移, 旧 zlib 槽位长度)
    'frag': (0x184da8, 0x184dac, 1653),
}


def parse_variants(qsb, version_positions):
    blocks = []
    for vs in version_positions:
        bstart = vs - 20
        key = qsb[bstart:bstart + 16]
        srclen = struct.unpack('>I', qsb[vs - 4:vs])[0]
        src = qsb[vs:vs + srclen]
        pos = vs + srclen
        entrylen = struct.unpack('>I', qsb[pos:pos + 4])[0]
        entry = qsb[pos + 4:pos + 4 + entrylen]
        bend = pos + 4 + entrylen
        blocks.append((bstart, bend, key, srclen, src, entrylen, entry))
    return blocks


def build_var(key, src, entry):
    return key + struct.pack('>I', len(src)) + src + struct.pack('>I', len(entry)) + entry


def make_qsb(ud, mode):
    vers = [x for x in [ud.find(b'#version 100'), ud.find(b'#version 120'),
                        ud.find(b'#version 150')] if x >= 0]
    blocks = parse_variants(ud, vers)
    b0, b1, b2 = blocks[0], blocks[1], blocks[2]
    prefix = ud[:b0[0]]
    suffix = ud[blocks[-1][1]:]
    if mode == 'vert':
        es3 = b0[4]
        for a, b in [(b'#version 100', b'#version 300 es'),
                     (b'varying vec2 v_texcoord;', b'out vec2 v_texcoord;'),
                     (b'attribute vec2 texcoord;', b'in vec2 texcoord;'),
                     (b'attribute vec3 position;', b'in vec3 position;')]:
            es3 = es3.replace(a, b)
        ka = struct.unpack('>IIII', b0[2])
        kb = struct.unpack('>IIII', b1[2])
        v0 = build_var(struct.pack('>IIII', ka[0], 300, ka[2], ka[3]), es3, b0[6])
        v1 = build_var(struct.pack('>IIII', kb[0], 300, kb[2], kb[3]), es3, b1[6])
        v2 = build_var(b2[2], b2[4], b2[6])
        return prefix + v0 + v1 + v2 + suffix, es3
    else:
        es3 = b2[4].replace(b'#version 150', b'#version 300 es')
        es3 = es3.replace(b'gl_FragData[0]', b'fragColor')
        es3 = es3.replace(b'texture2D(textureSampler', b'texture(textureSampler')
        es3 = es3.replace(b'#version 300 es', b'#version 300 es\nprecision highp float;\nprecision highp int;\n')
        if b'out vec4 fragColor' not in es3:
            es3 = es3.replace(b'uniform buf _26;', b'uniform buf _26;\nout vec4 fragColor;')
        ka = struct.unpack('>IIII', b0[2])
        v0 = build_var(struct.pack('>IIII', ka[0], 300, ka[2], ka[3]), es3, b0[6])
        v1 = build_var(b1[2], b1[4], b1[6])
        v2 = build_var(b2[2], b2[4], b2[6])
        return prefix + v0 + v1 + v2 + suffix, es3


def main():
    so = sys.argv[1] if len(sys.argv) > 1 else 'app/entry/libs/arm64-v8a/libQt6Gui.so'
    bak = so + '.orig.bak'
    if not __import__('os').path.exists(bak):
        shutil.copy(so, bak)
        print(f'备份原版 -> {bak}')
    data = bytearray(open(so, 'rb').read())
    for name, (sizeoff, zoff, oldzl) in OFFSETS.items():
        ud = zlib.decompress(data[zoff:zoff + 4000])
        new_qsb, es3 = make_qsb(ud, name)
        newz = zlib.compress(new_qsb, 9)
        assert len(newz) <= oldzl, f'{name} 压缩后 {len(newz)} > 槽位 {oldzl}'
        struct.pack_into('>I', data, sizeoff, len(new_qsb))
        data[zoff:zoff + len(newz)] = newz
        if len(newz) < oldzl:
            data[zoff + len(newz):zoff + oldzl] = b'\x00' * (oldzl - len(newz))
        print(f'{name}: qsb={len(new_qsb)} zlib={len(newz)} pad={oldzl - len(newz)}')
    open(so, 'wb').write(data)
    # 验证
    d2 = open(so, 'rb').read()
    for name, (sizeoff, zoff, _) in OFFSETS.items():
        ln = struct.unpack('>I', d2[sizeoff:sizeoff + 4])[0]
        ud = zlib.decompress(d2[zoff:zoff + 4000])
        assert len(ud) == ln, f'{name} 解压长度 {len(ud)} != size {ln}'
        assert b'#version 300 es' in ud, f'{name} 缺 300 es 变体'
        print(f'verify {name}: OK (size={ln}, 含 300-es)')


if __name__ == '__main__':
    main()
