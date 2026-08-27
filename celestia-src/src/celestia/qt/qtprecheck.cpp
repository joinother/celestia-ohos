// HarmonyOS-specific Qt RHI diagnostics.
// This TU includes only Qt headers (never Celestia/epoxy headers) to avoid
// the Qt-GL vs epoxy type clash that occurs when both are in one TU.
#ifdef CELESTIA_OHOS
#include "qtprecheck.h"

#include <cstdarg>
#include <cstdio>

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>

#include <QFile>
#include <QResource>
#include <QByteArray>

#include <QtGui/qopengl.h>

#include <hilog/log.h>

static void prlog(const char* fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    OH_LOG_Print(LOG_APP, LOG_INFO, 0xCE11, "CelestiaPre", "%{public}s", buf);
}

namespace celestia::qt {
int qtPrecheckContextFormat()
{
    const QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    prlog("precheck: default rt=%d ver=%d.%d", (int)fmt.renderableType(), fmt.majorVersion(), fmt.minorVersion());

    // Test A: default (OpenGLES) format.
    {
        QOffscreenSurface surf;
        surf.setFormat(fmt);
        surf.create();
        QOpenGLContext ctx;
        ctx.setFormat(fmt);
        const bool ok = ctx.create();
        prlog("precheck A(default fmt): create=%d", ok ? 1 : 0);
        if (ok) {
            const QSurfaceFormat actual = ctx.format();
            prlog("precheck A: ctx.format rt=%d ver=%d.%d makeCurrent=%d",
                  (int)actual.renderableType(), actual.majorVersion(), actual.minorVersion(),
                  ctx.makeCurrent(&surf) ? 1 : 0);
        }
    }

    // Test B: EMPTY format (this is what the widget QWidgetWindow uses, and
    // what QBackingStoreRhiSupport receives via window->format()).
    {
        QSurfaceFormat empty;
        QOffscreenSurface surf;
        surf.setFormat(empty);
        surf.create();
        QOpenGLContext ctx;
        ctx.setFormat(empty);
        const bool ok = ctx.create();
        prlog("precheck B(empty fmt): create=%d", ok ? 1 : 0);
        if (ok) {
            const QSurfaceFormat actual = ctx.format();
            prlog("precheck B: ctx.format rt=%d ver=%d.%d makeCurrent=%d",
                  (int)actual.renderableType(), actual.majorVersion(), actual.minorVersion(),
                  ctx.makeCurrent(&surf) ? 1 : 0);
        }
    }

    // Test C: replicate what QRhiGles2::create() does with
    // QBackingStoreRhiSupport (shareContext = globalShareContext).
    {
        QSurfaceFormat empty;
        QOffscreenSurface surf;
        surf.setFormat(empty);
        surf.create();
        QOpenGLContext ctx;
        ctx.setFormat(empty);
        QOpenGLContext* share = QOpenGLContext::globalShareContext();
        prlog("precheck C: globalShareContext=%p", (void*)share);
        if (share) {
            ctx.setShareContext(share);
            ctx.setScreen(share->screen());
        }
        const bool ok = ctx.create();
        prlog("precheck C(share): create=%d", ok ? 1 : 0);
        if (ok) {
            const QSurfaceFormat actual = ctx.format();
            prlog("precheck C: ctx.format rt=%d ver=%d.%d",
                  (int)actual.renderableType(), actual.majorVersion(), actual.minorVersion());
        }
    }

    // Test D: shader compile capability on the emulator GLES driver.
    // Reproduce whether #version 100 (ES 1.00) fails and #version 300 es works.
    {
        QSurfaceFormat fmt;
        QOffscreenSurface surf;
        surf.setFormat(fmt);
        surf.create();
        QOpenGLContext ctx;
        ctx.setFormat(fmt);
        if (ctx.create() && ctx.makeCurrent(&surf)) {
            // minimal ES 1.00 vs ES 3.00 vertex shaders
            const char* vs100 =
                "#version 100\n"
                "precision highp float;\n"
                "attribute vec2 position;\n"
                "void main() { gl_Position = vec4(position,0.0,1.0); }\n";
            const char* vs300 =
                "#version 300 es\n"
                "precision highp float;\n"
                "layout(location=0) in vec2 position;\n"
                "void main() { gl_Position = vec4(position,0.0,1.0); }\n";
            const char* fs300 =
                "#version 300 es\n"
                "precision highp float;\n"
                "layout(location=0) out vec4 fragColor;\n"
                "void main() { fragColor = vec4(1.0); }\n";
            const char* fs100 =
                "#version 100\n"
                "precision highp float;\n"
                "void main() { gl_FragColor = vec4(1.0); }\n";

            // Minimal GL function access via QOpenGLFunctions
            auto compileTest = [&](const char* tag, GLenum type, const char* src) -> bool {
                QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
                GLuint sh = f->glCreateShader(type);
                f->glShaderSource(sh, 1, &src, nullptr);
                f->glCompileShader(sh);
                GLint ok = 0;
                f->glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
                if (!ok) {
                    char log[512] = {0};
                    f->glGetShaderInfoLog(sh, 511, nullptr, log);
                    prlog("precheck D[%s]: COMPILE FAIL: %s", tag, log);
                } else {
                    prlog("precheck D[%s]: compile OK", tag);
                }
                f->glDeleteShader(sh);
                return ok != 0;
            };
            compileTest("vert100", GL_VERTEX_SHADER, vs100);
            compileTest("frag100", GL_FRAGMENT_SHADER, fs100);
            compileTest("vert300", GL_VERTEX_SHADER, vs300);
            compileTest("frag300", GL_FRAGMENT_SHADER, fs300);

            // Test E: the EXACT compositor shaders Qt feeds (from hilog dump).
            const char* compVert100 =
                "#version 100\n"
                "precision highp float;\n"
                "precision highp int;\n"
                "attribute vec3 position;\n"
                "attribute vec2 texcoord;\n"
                "varying vec2 v_texcoord;\n"
                "uniform buf { mat4 vertexTransform; mat3 textureTransform; float opacity; int textureSwizzle; } _17;\n"
                "void main()\n"
                "{\n"
                "    v_texcoord = (textureTransform * vec3(texcoord, 1.0)).xy;\n"
                "    gl_Position = vertexTransform * vec4(position, 1.0);\n"
                "}\n";
            const char* compFrag100 =
                "#version 100\n"
                "precision highp float;\n"
                "precision highp int;\n"
                "struct buf\n"
                "{\n"
                "    mat4 vertexTransform;\n"
                "    mat3 textureTransform;\n"
                "    float opacity;\n"
                "    int textureSwizzle;\n"
                "};\n"
                "uniform buf _26;\n"
                "uniform highp sampler2D textureSampler;\n"
                "varying vec2 v_texcoord;\n"
                "void main()\n"
                "{\n"
                "    vec4 tmpFragColor = texture2D(textureSampler, v_texcoord);\n"
                "    tmpFragColor.w *= _26.opacity;\n"
                "    if (_26.textureSwizzle == 0)\n"
                "    {\n"
                "        gl_FragData[0] = tmpFragColor;\n"
                "    }\n"
                "    else if (_26.textureSwizzle == 2)\n"
                "    {\n"
                "        gl_FragData[0] = vec4(tmpFragColor.y, tmpFragColor.z, tmpFragColor.w, tmpFragColor.x);\n"
                "    }\n"
                "    else\n"
                "    {\n"
                "        gl_FragData[0] = vec4(tmpFragColor.z, tmpFragColor.y, tmpFragColor.x, tmpFragColor.w);\n"
                "    }\n"
                "}\n";
            compileTest("compVert100", GL_VERTEX_SHADER, compVert100);
            compileTest("compFrag100", GL_FRAGMENT_SHADER, compFrag100);

            // Test F: which GLSL flavors does this driver accept?
            const char* v150 =
                "#version 150\n"
                "struct buf\n"
                "{\n"
                "    mat4 vertexTransform;\n"
                "    mat3 textureTransform;\n"
                "    float opacity;\n"
                "    int textureSwizzle;\n"
                "};\n"
                "uniform buf _17;\n"
                "out vec2 v_texcoord;\n"
                "in vec2 texcoord;\n"
                "in vec3 position;\n"
                "void main()\n"
                "{\n"
                "    v_texcoord = (_17.textureTransform * vec3(texcoord, 1.0)).xy;\n"
                "    gl_Position = _17.vertexTransform * vec4(position, 1.0);\n"
                "}\n";
            const char* f150 =
                "#version 150\n"
                "struct buf\n"
                "{\n"
                "    mat4 vertexTransform;\n"
                "    mat3 textureTransform;\n"
                "    float opacity;\n"
                "    int textureSwizzle;\n"
                "};\n"
                "uniform buf _26;\n"
                "uniform sampler2D textureSampler;\n"
                "in vec2 v_texcoord;\n"
                "out vec4 fragColor;\n"
                "void main()\n"
                "{\n"
                "    vec4 tmpFragColor = texture(textureSampler, v_texcoord);\n"
                "    tmpFragColor.w *= _26.opacity;\n"
                "    if (_26.textureSwizzle == 0)\n"
                "    {\n"
                "        fragColor = tmpFragColor;\n"
                "    }\n"
                "    else\n"
                "    {\n"
                "        if (_26.textureSwizzle == 2)\n"
                "        {\n"
                "            fragColor = vec4(tmpFragColor.y, tmpFragColor.z, tmpFragColor.w, tmpFragColor.x);\n"
                "        }\n"
                "        else\n"
                "        {\n"
                "            fragColor = vec4(tmpFragColor.z, tmpFragColor.y, tmpFragColor.x, tmpFragColor.w);\n"
                "        }\n"
                "    }\n"
                "}\n";
            const char* v140 =
                "#version 140\n"
                "uniform buf\n"
                "{\n"
                "    mat4 vertexTransform;\n"
                "    mat3 textureTransform;\n"
                "    float opacity;\n"
                "    int textureSwizzle;\n"
                "};\n"
                "out vec2 v_texcoord;\n"
                "in vec2 texcoord;\n"
                "in vec3 position;\n"
                "void main()\n"
                "{\n"
                "    v_texcoord = (textureTransform * vec3(texcoord, 1.0)).xy;\n"
                "    gl_Position = vertexTransform * vec4(position, 1.0);\n"
                "}\n";
            const char* f140 =
                "#version 140\n"
                "uniform buf\n"
                "{\n"
                "    mat4 vertexTransform;\n"
                "    mat3 textureTransform;\n"
                "    float opacity;\n"
                "    int textureSwizzle;\n"
                "};\n"
                "uniform sampler2D textureSampler;\n"
                "in vec2 v_texcoord;\n"
                "out vec4 fragColor;\n"
                "void main()\n"
                "{\n"
                "    vec4 tmpFragColor = texture(textureSampler, v_texcoord);\n"
                "    tmpFragColor.w *= opacity;\n"
                "    if (textureSwizzle == 0)\n"
                "    {\n"
                "        fragColor = tmpFragColor;\n"
                "    }\n"
                "    else if (textureSwizzle == 2)\n"
                "    {\n"
                "        fragColor = vec4(tmpFragColor.y, tmpFragColor.z, tmpFragColor.w, tmpFragColor.x);\n"
                "    }\n"
                "    else\n"
                "    {\n"
                "        fragColor = vec4(tmpFragColor.z, tmpFragColor.y, tmpFragColor.x, tmpFragColor.w);\n"
                "    }\n"
                "}\n";
            compileTest("v150", GL_VERTEX_SHADER, v150);
            compileTest("f150", GL_FRAGMENT_SHADER, f150);
            compileTest("v140", GL_VERTEX_SHADER, v140);
            compileTest("f140", GL_FRAGMENT_SHADER, f140);

            // Test G: can we create a DESKTOP GL context on this driver?
            {
                QOffscreenSurface surf;
                QSurfaceFormat dfmt;
                dfmt.setRenderableType(QSurfaceFormat::OpenGL);
                dfmt.setVersion(4, 5);
                surf.setFormat(dfmt);
                surf.create();
                QOpenGLContext ctx;
                ctx.setFormat(dfmt);
                const bool ok = ctx.create();
                prlog("precheck G(desktop ctx): create=%d", ok ? 1 : 0);
                if (ok) {
                    const QSurfaceFormat a = ctx.format();
                    prlog("precheck G: ctx.format rt=%d ver=%d.%d",
                          (int)a.renderableType(), a.majorVersion(), a.minorVersion());
                }
            }

            // Test H: EXACT minimal-change desktop-140 replacements of the
            // compositor's ES 1.00 shaders (keep struct uniform + gl_FragData[0]).
            const char* replVert140 =
                "#version 140\n"
                "\n"
                "struct buf\n"
                "{\n"
                "    mat4 vertexTransform;\n"
                "    mat3 textureTransform;\n"
                "    float opacity;\n"
                "    int textureSwizzle;\n"
                "};\n"
                "\n"
                "uniform buf _17;\n"
                "\n"
                "out vec2 v_texcoord;\n"
                "in vec2 texcoord;\n"
                "in vec3 position;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    v_texcoord = (_17.textureTransform * vec3(texcoord, 1.0)).xy;\n"
                "    gl_Position = _17.vertexTransform * vec4(position, 1.0);\n"
                "}\n";
            const char* replFrag140 =
                "#version 140\n"
                "\n"
                "struct buf\n"
                "{\n"
                "    mat4 vertexTransform;\n"
                "    mat3 textureTransform;\n"
                "    float opacity;\n"
                "    int textureSwizzle;\n"
                "};\n"
                "\n"
                "uniform buf _26;\n"
                "\n"
                "uniform sampler2D textureSampler;\n"
                "\n"
                "in vec2 v_texcoord;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    vec4 tmpFragColor = texture2D(textureSampler, v_texcoord);\n"
                "    tmpFragColor.w *= _26.opacity;\n"
                "    if (_26.textureSwizzle == 0)\n"
                "    {\n"
                "        gl_FragData[0] = tmpFragColor;\n"
                "    }\n"
                "    else\n"
                "    {\n"
                "        if (_26.textureSwizzle == 2)\n"
                "        {\n"
                "            gl_FragData[0] = vec4(tmpFragColor.y, tmpFragColor.z, tmpFragColor.w, tmpFragColor.x);\n"
                "        }\n"
                "        else\n"
                "        {\n"
                "            gl_FragData[0] = vec4(tmpFragColor.z, tmpFragColor.y, tmpFragColor.x, tmpFragColor.w);\n"
                "        }\n"
                "    }\n"
                "}\n";
            compileTest("replVert140", GL_VERTEX_SHADER, replVert140);
            compileTest("replFrag140", GL_FRAGMENT_SHADER, replFrag140);

            // Test I: isolate which frag construct breaks the driver.
            const char* replFragA =  // texture() instead of texture2D()
                "#version 140\n"
                "struct buf { mat4 vertexTransform; mat3 textureTransform; float opacity; int textureSwizzle; };\n"
                "uniform buf _26;\n"
                "uniform sampler2D textureSampler;\n"
                "in vec2 v_texcoord;\n"
                "void main()\n"
                "{\n"
                "    vec4 tmpFragColor = texture(textureSampler, v_texcoord);\n"
                "    tmpFragColor.w *= _26.opacity;\n"
                "    if (_26.textureSwizzle == 0) { gl_FragData[0] = tmpFragColor; }\n"
                "    else { gl_FragData[0] = vec4(tmpFragColor.z, tmpFragColor.y, tmpFragColor.x, tmpFragColor.w); }\n"
                "}\n";
            const char* replFragB =  // out fragColor instead of gl_FragData[0]
                "#version 140\n"
                "struct buf { mat4 vertexTransform; mat3 textureTransform; float opacity; int textureSwizzle; };\n"
                "uniform buf _26;\n"
                "uniform sampler2D textureSampler;\n"
                "in vec2 v_texcoord;\n"
                "out vec4 fragColor;\n"
                "void main()\n"
                "{\n"
                "    vec4 tmpFragColor = texture2D(textureSampler, v_texcoord);\n"
                "    tmpFragColor.w *= _26.opacity;\n"
                "    if (_26.textureSwizzle == 0) { fragColor = tmpFragColor; }\n"
                "    else { fragColor = vec4(tmpFragColor.z, tmpFragColor.y, tmpFragColor.x, tmpFragColor.w); }\n"
                "}\n";
            const char* replFragC =  // both texture() + out fragColor
                "#version 140\n"
                "struct buf { mat4 vertexTransform; mat3 textureTransform; float opacity; int textureSwizzle; };\n"
                "uniform buf _26;\n"
                "uniform sampler2D textureSampler;\n"
                "in vec2 v_texcoord;\n"
                "out vec4 fragColor;\n"
                "void main()\n"
                "{\n"
                "    vec4 tmpFragColor = texture(textureSampler, v_texcoord);\n"
                "    tmpFragColor.w *= _26.opacity;\n"
                "    if (_26.textureSwizzle == 0) { fragColor = tmpFragColor; }\n"
                "    else { fragColor = vec4(tmpFragColor.z, tmpFragColor.y, tmpFragColor.x, tmpFragColor.w); }\n"
                "}\n";
            compileTest("replFragA(texture)", GL_FRAGMENT_SHADER, replFragA);
            compileTest("replFragB(out)", GL_FRAGMENT_SHADER, replFragB);
            compileTest("replFragC(both)", GL_FRAGMENT_SHADER, replFragC);
        } else {
            prlog("precheck D: could not make context current");
        }
    }
    return 0;
}
void qtPrecheckResource()
{
    const char* names[] = {
        ":/qt-project.org/gui/painting/shaders/backingstorecompose.vert.qsb",
        ":/qt-project.org/gui/painting/shaders/backingstorecompose.frag.qsb"
    };
    for (const char* name : names) {
        QFile f(QString::fromLatin1(name));
        if (!f.open(QIODevice::ReadOnly)) {
            prlog("resource: FAILED to open %s", name);
            continue;
        }
        QByteArray raw = f.readAll();
        // 关键: 判断 QFile 返回的是 zlib 压缩流还是解压后的 qsb 内容
        bool zlibLike = raw.size() >= 2
                && (unsigned char)raw[0] == 0x78
                && ((unsigned char)raw[1] == 0x9c || (unsigned char)raw[1] == 0xda
                    || (unsigned char)raw[1] == 0x01 || (unsigned char)raw[1] == 0x5e);
        prlog("resource: %s read %lld bytes, zlibLike=%d, first16=%02x%02x%02x%02x %02x%02x%02x%02x",
              name, (long long)raw.size(), zlibLike ? 1 : 0,
              (unsigned char)raw[0], (unsigned char)raw[1], (unsigned char)raw[2], (unsigned char)raw[3],
              (unsigned char)raw[4], (unsigned char)raw[5], (unsigned char)raw[6], (unsigned char)raw[7]);
    }
}

} // namespace celestia::qt
#endif // CELESTIA_OHOS
