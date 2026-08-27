/***************************************************************************
                          qtmain.cpp  -  description
                             -------------------
    begin                : Tue Jul 16 22:28:19 CEST 2002
    copyright            : (C) 2002 by Christophe Teyssier
    email                : chris@teyssier.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <optional>

#include <QtGlobal>
#include <QApplication>
#include <QBitmap>
#include <QColor>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <dlfcn.h>
#include <QLibraryInfo>
#include <QLocale>
#include <QObject>
#include <QPixmap>
#include <QOffscreenSurface>
#include <QSplashScreen>
#include <QString>
#include <QSurfaceFormat>
#include <QTranslator>

#include <celutil/gettext.h>
#include <celutil/localeutil.h>
#include "qtappwin.h"
#include "qtcommandline.h"
#include "qtgettext.h"

#ifdef CELESTIA_OHOS
#include <hilog/log.h>
#define CEL_LOG(fmt, ...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xCE11, "Celestia", "[qtmain] " fmt, ##__VA_ARGS__)
#else
#define CEL_LOG(fmt, ...) qDebug(fmt, ##__VA_ARGS__)
#endif

#ifdef CELESTIA_OHOS
#include "qtprecheck.h"
#endif

#ifdef ENABLE_NLS
namespace
{

inline void
bindTextDomainUTF8(const char* domainName, const QString& directory)
{
#ifdef _WIN32
    wbindtextdomain(domainName, directory.toStdWString().c_str());
#else
    bindtextdomain(domainName, directory.toUtf8().data());
#endif
    bind_textdomain_codeset(domainName, "UTF-8");
}

} // end unnamed namespace
#endif

int main(int argc, char *argv[])
{
    using namespace celestia::qt;

#ifdef CELESTIA_OHOS
    // On HarmonyOS, Qt's widget backing-store compositor routes through the RHI.
    // On the emulator its GLES2 swapchain creation can crash on a NULL surface
    // (QRhiGles2InitParams::newFallbackSurface -> SIGSEGV) when the window has
    // no valid native surface yet. Pre-warm the offscreen-surface / EGL pbuffer
    // infrastructure up front so the compositor's fallback surface creation
    // succeeds later.
    qputenv("QT_DISABLE_SHADER_DISK_CACHE", "1");
    qputenv("QT_LOGGING_RULES",
            "qt.qpa.backingstore.debug=true;"
            "qt.qpa.compositor.debug=true;"
            "qt.rhi.*.debug=true;"
            "qt.qpa.*.debug=true;"
            "qt.gl.*.debug=true");
    CEL_LOG("qtmain: RHI/backingstore debug logging enabled");
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
    fmt.setVersion(3, 0);
    fmt.setRedBufferSize(8);
    fmt.setGreenBufferSize(8);
    fmt.setBlueBufferSize(8);
    fmt.setAlphaBufferSize(8);
    fmt.setDepthBufferSize(0);
    fmt.setStencilBufferSize(0);
    fmt.setSamples(1);
    QSurfaceFormat::setDefaultFormat(fmt);
#endif

#ifndef GL_ES
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
#else
    QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
#endif
    QApplication app(argc, argv);

#ifdef CELESTIA_OHOS
    // Pre-warm the offscreen-surface / EGL pbuffer path. The widget backing-store
    // compositor's GLES2 swapchain falls back to an offscreen surface when the
    // window has no native surface yet; if that offscreen path is not yet
    // initialized the fallback creation crashes. Creating one here at startup
    // forces the platform to set it up while things are simple.
    CEL_LOG("qtmain: pre-warming offscreen GL surface");
    {
        QOffscreenSurface offscreen;
        offscreen.setFormat(QSurfaceFormat::defaultFormat());
        offscreen.create();
        CEL_LOG("qtmain: offscreen created, handle=%p valid=%d",
                (void*)offscreen.handle(), offscreen.isValid() ? 1 : 0);
        celestia::qt::qtPrecheckContextFormat();
        celestia::qt::qtPrecheckResource();
    }
    CEL_LOG("qtmain: offscreen pre-warm done");
#endif

    // Gettext integration
    CelestiaCore::initLocale();
#ifdef ENABLE_NLS
    QString localeDir = LOCALEDIR;
    bindTextDomainUTF8("celestia", localeDir);
    bindTextDomainUTF8("celestia-data", localeDir);
    textdomain("celestia");
#endif

    if (QTranslator qtTranslator;
        qtTranslator.load("qt_" + QLocale::system().name(),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
    {
        app.installTranslator(&qtTranslator);
    }

    CelestiaQTranslator celestiaTranslator;
    app.installTranslator(&celestiaTranslator);

    Q_INIT_RESOURCE(icons);

    QCoreApplication::setOrganizationName("Celestia Development Team");
    QCoreApplication::setOrganizationDomain("celestiaproject.space");
    QCoreApplication::setApplicationName("Celestia QT");
    QCoreApplication::setApplicationVersion(VERSION);

    CelestiaCommandLineOptions options = ParseCommandLine(app);

    CEL_LOG("main: options parsed, startDirectory='%s' skipSplash=%d", qPrintable(options.startDirectory), options.skipSplashScreen ? 1 : 0);

#ifdef CELESTIA_OHOS
#ifdef ENABLE_NLS
    // On HarmonyOS the data directory (sandbox) is only known at runtime and
    // is passed via --dir. Bind gettext domains to the ABSOLUTE locale dir
    // <dataDir>/locale so we do not depend on the process CWD.
    if (!options.startDirectory.isEmpty())
    {
        QDir::setCurrent(options.startDirectory);
        QString localeDir = options.startDirectory + QStringLiteral("/locale");
        bindTextDomainUTF8("celestia", localeDir);
        bindTextDomainUTF8("celestia-data", localeDir);
        textdomain("celestia");
        CEL_LOG("main: OHOS gettext bound to %s/locale", qPrintable(options.startDirectory));
    }
#endif
#endif

    std::optional<QSplashScreen> splash{std::nullopt};
#ifndef CELESTIA_OHOS
    if (!options.skipSplashScreen)
    {
        QDir splashDir(CONFIG_DATA_DIR "/splash");
        if (!options.startDirectory.isEmpty())
        {
            QDir newSplashDir = QString("%1/splash").arg(options.startDirectory);
            if (newSplashDir.exists("splash.png"))
                splashDir = std::move(newSplashDir);
        }

        QPixmap pixmap(splashDir.filePath("splash.png"));
        splash.emplace(pixmap);
        splash->setMask(pixmap.mask());


        // TODO: resolve issues with pixmap alpha channel
        splash->show();
        app.processEvents();
        CEL_LOG("main: splash shown");
    }
#endif

    CelestiaAppWindow window;

    if (splash.has_value())
    {
        // Connect the splash screen to the main window so that it
        // can receive progress notifications as Celestia files required
        // for startup are loaded.
        QObject::connect(&window, SIGNAL(progressUpdate(const QString&, int, const QColor&)),
                         &*splash, SLOT(showMessage(const QString&, int, const QColor&)));
    }

    CEL_LOG("main: window created, calling init");
    window.init(options);
    CEL_LOG("main: init returned");
#ifdef CELESTIA_OHOS
    {
        const QSurfaceFormat d = QSurfaceFormat::defaultFormat();
        CEL_LOG("OHOS: default fmt rt=%{public}d ver=%{public}d.%{public}d", (int)d.renderableType(),
                d.majorVersion(), d.minorVersion());
    }
#endif
    window.show();
    CEL_LOG("main: show returned");
    window.startAppCore();
    CEL_LOG("main: startAppCore returned");

    if (splash.has_value())
    {
#ifndef CELESTIA_OHOS
        splash->finish(&window);
#endif
    }

    // Set the main window to be the cel url handler
    QDesktopServices::setUrlHandler("cel", &window, "handleCelUrl");

    int ret = app.exec();
    QDesktopServices::unsetUrlHandler("cel");
    return ret;
}
