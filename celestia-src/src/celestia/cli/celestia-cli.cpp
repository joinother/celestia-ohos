// celestia-cli.cpp
//
// Celestia headless command line interface (HarmonyOS port companion).
//
// Modes:
//   celestia-cli --script <celx>            run a celx script and exit
//   celestia-cli --render-to-image <png>    render one frame and exit
//   celestia-cli --exec <command>           run a single console command and exit
//   celestia-cli (no args)                  interactive console on stdin
//
// Options:
//   --dir <path>        Celestia data directory (contains celestia.cfg)
//   --conf <name>       config file name (default celestia.cfg)
//   --select <object>   object to select before rendering / script
//   --size WxH          output size (default 1920x1080)
//   --set-time <jd>     set simulation time (Julian day)
//
// Rendering is performed off-screen using a QOffscreenSurface + FBO, so no
// window is created.

// Celestia core headers first so that epoxy (gl.h) is included before any
// Qt/GL header, avoiding typedef clashes with the platform OpenGL headers.
#include <celengine/observer.h>
#include <celengine/render.h>
#include <celengine/simulation.h>
#include <celengine/texmanager.h>
#include <celestia/celestiacore.h>
#include <celutil/logger.h>

#include "../qt/qtcli.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QTextStream>

#include <filesystem>
#include <iostream>
#include <vector>

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#endif

using namespace celestia;
using namespace celestia::engine;
using namespace celestia::qt;

namespace
{

CelestiaCore core;

#ifdef __APPLE__
CGLContextObj gCGLContext = nullptr;
#endif

// Original working directory, captured at startup. Config loading chdir()s to
// the data dir so relative paths in celestia.cfg resolve; output paths are
// resolved against this original cwd instead.
std::filesystem::path g_originalCwd;

// Resolve an output path relative to the original cwd.
std::filesystem::path resolveOutputPath(const QString& outPath)
{
    std::filesystem::path p(outPath.toStdString());
    if (p.is_relative())
        return g_originalCwd / p;
    return p;
}

// ---------------------------------------------------------------------------
// GL / offscreen setup
//
// On macOS we create a native CGL off-screen context (QOffscreenSurface +
// QOpenGLContext is not supported by the Qt offscreen platform plugin), and
// epoxy resolves GL entry points against the current CGL context.
// ---------------------------------------------------------------------------
bool setupOffscreenGL()
{
#ifdef __APPLE__
    const CGLPixelFormatAttribute attrs[] = {
        kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,
        kCGLPFAColorSize,     (CGLPixelFormatAttribute)24,
        kCGLPFADepthSize,     (CGLPixelFormatAttribute)24,
        kCGLPFAStencilSize,   (CGLPixelFormatAttribute)8,
        (CGLPixelFormatAttribute)0
    };
    CGLPixelFormatObj pix = nullptr;
    GLint npix = 0;
    if (CGLChoosePixelFormat(attrs, &pix, &npix) != kCGLNoError || pix == nullptr)
    {
        std::cerr << "celestia-cli: CGLChoosePixelFormat failed\n";
        return false;
    }
    if (CGLCreateContext(pix, nullptr, &gCGLContext) != kCGLNoError || gCGLContext == nullptr)
    {
        std::cerr << "celestia-cli: CGLCreateContext failed\n";
        return false;
    }
    CGLDestroyPixelFormat(pix);
    if (CGLSetCurrentContext(gCGLContext) != kCGLNoError)
    {
        std::cerr << "celestia-cli: CGLSetCurrentContext failed\n";
        return false;
    }

    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* glver = glGetString(GL_VERSION);
    const GLubyte* glsl = glGetString(GL_SHADING_LANGUAGE_VERSION);
    fprintf(stderr, "celestia-cli: GL %s | GLSL %s | vendor=%s renderer=%s\n",
            glver ? reinterpret_cast<const char*>(glver) : "?",
            glsl ? reinterpret_cast<const char*>(glsl) : "?",
            vendor ? reinterpret_cast<const char*>(vendor) : "?",
            renderer ? reinterpret_cast<const char*>(renderer) : "?");
    fflush(stderr);
    return true;
#else
    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSamples(0);
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(fmt);

    QOffscreenSurface* surface = new QOffscreenSurface;
    surface->setFormat(fmt);
    surface->create();
    if (!surface->isValid())
    {
        std::cerr << "celestia-cli: failed to create offscreen surface\n";
        return false;
    }
    QOpenGLContext* ctx = new QOpenGLContext;
    ctx->setFormat(fmt);
    if (!ctx->create())
    {
        std::cerr << "celestia-cli: failed to create OpenGL context\n";
        return false;
    }
    if (!ctx->makeCurrent(surface))
    {
        std::cerr << "celestia-cli: failed to make GL context current\n";
        return false;
    }
    const GLubyte* vendor = glGetString(GL_VENDOR);
    std::cerr << "celestia-cli: GL vendor=" << (vendor ? reinterpret_cast<const char*>(vendor) : "?") << "\n";
    return true;
#endif
}

// ---------------------------------------------------------------------------
// Core initialization
// ---------------------------------------------------------------------------
bool initCore(const QString& dataDir, const QString& confName)
{
    std::filesystem::path cfg = (dataDir + "/" + confName).toStdString();
    if (!std::filesystem::exists(cfg))
    {
        fprintf(stderr, "celestia-cli: config not found: %s\n", cfg.c_str());
        fflush(stderr);
        return false;
    }

    // Relative paths inside the config (e.g. "data/stars.dat") are resolved
    // against the current working directory, so switch to the config dir.
    std::error_code ec;
    std::filesystem::current_path(cfg.parent_path(), ec);
    if (ec)
        fprintf(stderr, "celestia-cli: warning: cannot chdir to %s: %s\n",
                cfg.parent_path().c_str(), ec.message().c_str());

    if (!core.initSimulation(cfg))
    {
        fprintf(stderr, "celestia-cli: failed to init simulation\n");
        fflush(stderr);
        return false;
    }
    if (!core.initRenderer(TextureResolution::medres))
    {
        fprintf(stderr, "celestia-cli: failed to init renderer\n");
        fflush(stderr);
        return false;
    }
    core.start();
    return true;
}

// ---------------------------------------------------------------------------
// Render one frame to a PNG
// ---------------------------------------------------------------------------
bool renderFrame(int width, int height, const QString& outPath, const QString& selectObj)
{
#ifdef __APPLE__
    if (gCGLContext == nullptr)
        return false;
#endif

    if (!selectObj.isEmpty())
    {
        Simulation* sim = core.getSimulation();
        Selection sel = sim->findObjectFromPath(selectObj.toStdString(), true);
        if (!sel.empty())
        {
            sim->setSelection(sel);
            sim->centerSelection(0.0);
        }
    }

    Renderer* renderer = core.getRenderer();
    if (renderer == nullptr)
        return false;

    // Set the core window size so that metrics (used to compute the render
    // region) and the renderer viewport match the target image size.
    core.resize(width, height);

    // Create an off-screen framebuffer with depth/stencil attachment.
    GLuint fbo = 0, tex = 0, rbo = 0;
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "celestia-cli: FBO incomplete\n";
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &tex);
        glDeleteRenderbuffers(1, &rbo);
        return false;
    }

    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    core.draw();

    // Read back the framebuffer (GL is bottom-up).
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    QImage img(pixels.data(), width, height, QImage::Format_RGBA8888);
    img = img.copy();
    img = img.mirrored();

    glDeleteRenderbuffers(1, &rbo);
    glDeleteTextures(1, &tex);
    glDeleteFramebuffers(1, &fbo);

    if (img.isNull())
    {
        std::cerr << "celestia-cli: readback image is null\n";
        return false;
    }

    if (!img.save(QString::fromStdString(resolveOutputPath(outPath).string())))
    {
        fprintf(stderr, "celestia-cli: failed to save image %s\n",
                resolveOutputPath(outPath).c_str());
        fflush(stderr);
        return false;
    }

    fprintf(stderr, "celestia-cli: rendered %d x %d -> %s\n", width, height,
            resolveOutputPath(outPath).c_str());
    fflush(stderr);
    return true;
}

// ---------------------------------------------------------------------------
// Interactive console
// ---------------------------------------------------------------------------
int runInteractive()
{
    QTextStream in(stdin);
    QTextStream out(stdout);
    QString line;

    out << "Celestia CLI (headless) - type 'help' for commands, 'quit' to exit.\n";
    out.flush();

    while (true)
    {
        out << "celestia> ";
        out.flush();
        line = in.readLine();
        if (line.isNull())
            break;
        QString t = line.trimmed();
        if (t.isEmpty())
            continue;
        if (t == "quit" || t == "exit" || t == "q")
            break;

        if (t == "render")
        {
            out << "saving screenshot.png\n";
            out.flush();
            renderFrame(1920, 1080, QStringLiteral("screenshot.png"), QString());
            continue;
        }

        if (!CelestiaCli::execute(&core, t))
        {
            out << "unknown command: " << t << "\n";
            out << "try 'help'\n";
        }
    }
    return 0;
}

} // end unnamed namespace

// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    // Route the Celestia logger to stderr so we can see errors headlessly.
    class StderrBuf : public std::streambuf
    {
    protected:
        int_type overflow(int_type c) override
        {
            if (c != traits_type::eof())
            {
                fputc(static_cast<char>(c), stderr);
                return c;
            }
            return traits_type::not_eof(c);
        }
    };
    static StderrBuf stderrBuf;
    static std::ostream stderrStream(&stderrBuf);
    celestia::util::CreateLogger(celestia::util::Level::Debug, stderrStream, stderrStream);

    // Headless: never show windows. The offscreen platform plugin does not
    // support Qt OpenGL contexts, but we use a native CGL context on macOS.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    g_originalCwd = std::filesystem::current_path();
    QCoreApplication::setApplicationName(QStringLiteral("celestia-cli"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.7.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Celestia headless command line interface"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption dataDirOpt(QStringList() << "d" << "dir",
                                  QStringLiteral("Celestia data directory (contains celestia.cfg)"),
                                  QStringLiteral("path"));
    QCommandLineOption confOpt(QStringList() << "c" << "conf",
                               QStringLiteral("config file name (default: celestia.cfg)"),
                               QStringLiteral("name"));
    QCommandLineOption scriptOpt(QStringList() << "s" << "script",
                                 QStringLiteral("run a celx script and exit"),
                                 QStringLiteral("path"));
    QCommandLineOption renderOpt(QStringList() << "r" << "render-to-image",
                                 QStringLiteral("render one frame to image and exit"),
                                 QStringLiteral("file"));
    QCommandLineOption selectOpt(QStringList() << "t" << "select",
                                 QStringLiteral("object to select before render/script"),
                                 QStringLiteral("object"));
    QCommandLineOption sizeOpt(QStringLiteral("size"),
                               QStringLiteral("render size WxH (default 1920x1080)"),
                               QStringLiteral("WxH"));
    QCommandLineOption timeOpt(QStringLiteral("set-time"),
                               QStringLiteral("set simulation time (Julian day)"),
                               QStringLiteral("jd"));
    QCommandLineOption execOpt(QStringList() << "e" << "exec",
                               QStringLiteral("execute a single console command and exit"),
                               QStringLiteral("cmd"));

    parser.addOption(dataDirOpt);
    parser.addOption(confOpt);
    parser.addOption(scriptOpt);
    parser.addOption(renderOpt);
    parser.addOption(selectOpt);
    parser.addOption(sizeOpt);
    parser.addOption(timeOpt);
    parser.addOption(execOpt);
    parser.process(app);

    // Resolve data directory.
    QString dataDir = parser.value(dataDirOpt);
    if (dataDir.isEmpty())
        dataDir = QString::fromStdString(std::filesystem::current_path().string());
    QString confName = parser.value(confOpt);
    if (confName.isEmpty())
        confName = QStringLiteral("celestia.cfg");

    // Offscreen GL must be ready before core renderer init.
    if (!setupOffscreenGL())
    {
        return 1;
    }

    if (!initCore(dataDir, confName))
    {
        return 1;
    }

    if (parser.isSet(timeOpt))
    {
        bool ok = false;
        double jd = parser.value(timeOpt).toDouble(&ok);
        if (ok)
            core.getSimulation()->setTime(jd);
    }

    // --select applies to render mode; also apply generally.
    const QString selectObj = parser.value(selectOpt);

    if (parser.isSet(scriptOpt))
    {
        const QString scriptPath = parser.value(scriptOpt);
        core.runScript(std::filesystem::path(scriptPath.toStdString()));
        // Run a few frames so scripted camera moves settle, then exit.
        return 0;
    }

    if (parser.isSet(renderOpt))
    {
        int width = 1920, height = 1080;
        const QString sizeStr = parser.value(sizeOpt);
        if (!sizeStr.isEmpty())
        {
            int xpos = sizeStr.indexOf('x');
            if (xpos > 0)
            {
                bool okW = false, okH = false;
                int w = sizeStr.left(xpos).toInt(&okW);
                int h = sizeStr.mid(xpos + 1).toInt(&okH);
                if (okW && okH && w > 0 && h > 0)
                {
                    width = w;
                    height = h;
                }
            }
        }
        return renderFrame(width, height, parser.value(renderOpt), selectObj) ? 0 : 1;
    }

    if (parser.isSet(execOpt))
    {
        if (!CelestiaCli::execute(&core, parser.value(execOpt)))
        {
            std::cerr << "celestia-cli: unknown command: " << parser.value(execOpt).toStdString() << "\n";
            return 1;
        }
        return 0;
    }

    // Interactive mode.
    return runInteractive();
}
