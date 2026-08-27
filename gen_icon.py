import struct, zlib

def decode_png(path):
    data = open(path,'rb').read(); pos=8; idat=b''; w=h=bitd=colort=None
    while pos < len(data):
        ln=struct.unpack('>I',data[pos:pos+4])[0]; typ=data[pos+4:pos+8]; chunk=data[pos+8:pos+8+ln]; pos+=12+ln
        if typ==b'IHDR': w,h,bitd,colort=struct.unpack('>IIBB',chunk[:10])
        elif typ==b'IDAT': idat+=chunk
        elif typ==b'IEND': break
    raw=zlib.decompress(idat); ch=4 if colort==6 else 3; stride=w*ch
    out=bytearray(w*h*4); prev=bytearray(stride); o=0; p=0
    for y in range(h):
        f=raw[p]; p+=1; line=bytearray(raw[p:p+stride]); p+=stride
        if f==1:
            for i in range(ch,stride): line[i]=(line[i]+line[i-ch])&255
        elif f==2:
            for i in range(stride): line[i]=(line[i]+prev[i])&255
        elif f==3:
            for i in range(stride):
                a=line[i-ch] if i>=ch else 0; line[i]=(line[i]+((a+prev[i])>>1))&255
        elif f==4:
            for i in range(stride):
                a=line[i-ch] if i>=ch else 0; b=prev[i]; c=prev[i-ch] if i>=ch else 0
                pa=abs(b-c); pb=abs(a-c); pc=abs(a+b-2*c)
                pr=a if (pa<=pb and pa<=pc) else (b if pb<=pc else c)
                line[i]=(line[i]+pr)&255
        for i in range(stride):
            out[o+4*(i//ch)+(i%ch)]=line[i]
        if ch==3:
            for i in range(w): out[o+4*i+3]=255
        prev=line; o+=w*4
    return w,h,out

def encode_png(w,h,rgba):
    raw=b''
    for y in range(h):
        raw+=b'\x00'+bytes(rgba[y*w*4:(y+1)*w*4])
    def chunk(typ,d):
        return struct.pack('>I',len(d))+typ+d+struct.pack('>I',zlib.crc32(typ+d)&0xffffffff)
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b'')
    return png

def scale_bilinear(src,sw,sh,dw,dh):
    out=bytearray(dw*dh*4)
    for y in range(dh):
        fy=y*sh/dh-0.5; y0=int(fy); y1=y0+1; wy=fy-y0
        y0=max(0,min(sh-1,y0)); y1=max(0,min(sh-1,y1))
        for x in range(dw):
            fx=x*sw/dw-0.5; x0=int(fx); x1=x0+1; wx=fx-x0
            x0=max(0,min(sw-1,x0)); x1=max(0,min(sw-1,x1))
            for c in range(4):
                p00=src[(y0*sw+x0)*4+c]; p10=src[(y0*sw+x1)*4+c]
                p01=src[(y1*sw+x0)*4+c]; p11=src[(y1*sw+x1)*4+c]
                top=p00*(1-wx)+p10*wx; bot=p01*(1-wx)+p11*wx
                v=int(top*(1-wy)+bot*wy)
                out[(y*dw+x)*4+c]=max(0,min(255,v))
    return out

def white_canvas(w,h):
    out=bytearray(w*h*4)
    out[0::4]=b'\xff'*(w*h); out[1::4]=b'\xff'*(w*h); out[2::4]=b'\xff'*(w*h); out[3::4]=b'\xff'*(w*h)
    return out

def composite_on_white(logo, sw, sh, dw, dh, ox=0, oy=0):
    """把 logo(sw,sh,透明底) 画到 dw×dh 白底画布上，偏移 ox,oy"""
    canvas=white_canvas(dw,dh)
    lg=scale_bilinear(logo,sw,sh,dw,dh)
    for yy in range(dh):
        for xx in range(dw):
            a=lg[(yy*dw+xx)*4+3]
            if a>0:
                inv=255-a; i=(yy*dw+xx)*4
                canvas[i+0]=(lg[i+0]*a+255*inv)//255
                canvas[i+1]=(lg[i+1]*a+255*inv)//255
                canvas[i+2]=(lg[i+2]*a+255*inv)//255
                canvas[i+3]=255
    return canvas

w,h,ico=decode_png('/Users/jiexuanyang/celestia-ohos/celestia-icon-256.png')
print('源图标', w, h, '(背景已透明)')

# app_icon.png 1024 白底全幅 logo（单层图标，需不透明）
open('/Users/jiexuanyang/celestia-ohos/app_icon_new.png','wb').write(encode_png(1024,1024,composite_on_white(ico,w,h,1024,1024)))
print('app_icon_new.png 1024 白底')

# foreground.png 1280 透明底，logo 845px 居中（安全区 66%）
fg=bytearray(1280*1280*4); s=845
lgs=scale_bilinear(ico,w,h,s,s); off=(1280-s)//2
for yy in range(s):
    for xx in range(s):
        a=lgs[(yy*s+xx)*4+3]
        if a>0:
            i=((off+yy)*1280+off+xx)*4
            fg[i:i+4]=lgs[(yy*s+xx)*4:(yy*s+xx)*4+4]
open('/Users/jiexuanyang/celestia-ohos/foreground_new.png','wb').write(encode_png(1280,1280,fg))
print('foreground_new.png 1280 透明底 845 安全区')

# background.png 1280 纯白
open('/Users/jiexuanyang/celestia-ohos/background_new.png','wb').write(encode_png(1280,1280,white_canvas(1280,1280)))
print('background_new.png 1280 纯白')

# startIcon.png 1280 白底全幅 logo
open('/Users/jiexuanyang/celestia-ohos/startIcon_new.png','wb').write(encode_png(1280,1280,composite_on_white(ico,w,h,1280,1280)))
print('startIcon_new.png 1280 白底')
