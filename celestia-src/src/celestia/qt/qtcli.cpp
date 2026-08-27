// qtcli.cpp
//
// Celestia CLI command dispatcher for the HarmonyOS port.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "qtcli.h"

#include <Eigen/Geometry>

#include <celestia/celestiacore.h>
#include <celengine/observer.h>
#include <celengine/render.h>
#include <celengine/renderflags.h>
#include <celengine/simulation.h>

#include <QRegularExpression>

namespace celestia::qt
{

namespace
{

// Toggle a single render flag. Supports "all"/"none"/"default" and flag names.
bool toggleRenderFlag(Renderer* renderer, const QString& name, bool force, bool value)
{
    RenderFlags flags = renderer->getRenderFlags();
    RenderFlags flag = RenderFlags::ShowNothing;

    QString n = name.toLower();
    if (n == "all")      { renderer->setRenderFlags(RenderFlags::DefaultRenderFlags); return true; }
    if (n == "none")     { renderer->setRenderFlags(RenderFlags::ShowNothing); return true; }
    if (n == "default")  { renderer->setRenderFlags(RenderFlags::DefaultRenderFlags); return true; }

    struct { const char* name; RenderFlags flag; } map[] = {
        { "stars",             RenderFlags::ShowStars },
        { "planets",           RenderFlags::ShowPlanets },
        { "galaxies",          RenderFlags::ShowGalaxies },
        { "diagrams",          RenderFlags::ShowDiagrams },
        { "clouds",            RenderFlags::ShowCloudMaps },
        { "cloudshadow",       RenderFlags::ShowCloudShadows },
        { "orbits",            RenderFlags::ShowOrbits },
        { "orbit",             RenderFlags::ShowOrbits },
        { "celestialsphere",   RenderFlags::ShowCelestialSphere },
        { "nightmaps",         RenderFlags::ShowNightMaps },
        { "atmospheres",       RenderFlags::ShowAtmospheres },
        { "smoothlines",       RenderFlags::ShowSmoothLines },
        { "eclipseshadows",    RenderFlags::ShowEclipseShadows },
        { "ringshadows",       RenderFlags::ShowRingShadows },
        { "boundaries",        RenderFlags::ShowBoundaries },
        { "automag",           RenderFlags::ShowAutoMag },
        { "comettails",        RenderFlags::ShowCometTails },
        { "markers",           RenderFlags::ShowMarkers },
        { "nebulae",           RenderFlags::ShowNebulae },
        { "openclusters",      RenderFlags::ShowOpenClusters },
        { "globulars",         RenderFlags::ShowGlobulars },
        { "galacticgrid",      RenderFlags::ShowGalacticGrid },
        { "eclipticgrid",      RenderFlags::ShowEclipticGrid },
        { "horizongrid",       RenderFlags::ShowHorizonGrid },
        { "ecliptic",          RenderFlags::ShowEcliptic },
        { "dwarfplanets",      RenderFlags::ShowDwarfPlanets },
        { "moons",             RenderFlags::ShowMoons },
        { "minormoons",        RenderFlags::ShowMinorMoons },
        { "asteroids",         RenderFlags::ShowAsteroids },
        { "comets",            RenderFlags::ShowComets },
        { "spacecraft",        RenderFlags::ShowSpacecrafts },
        { "planetrings",       RenderFlags::ShowPlanetRings },
    };

    for (const auto& e : map)
    {
        if (n == e.name)
        {
            flag = e.flag;
            break;
        }
    }

    if (flag == RenderFlags::ShowNothing)
        return false;

    if (force)
    {
        if (value) flags |= flag; else flags &= ~flag;
    }
    else
    {
        flags ^= flag;
    }
    renderer->setRenderFlags(flags);
    return true;
}

// Toggle a single label flag.
bool toggleLabelFlag(Renderer* renderer, const QString& name, bool force, bool value)
{
    RenderLabels labels = renderer->getLabelMode();
    RenderLabels label = RenderLabels::NoLabels;

    QString n = name.toLower();
    if (n == "all")      { renderer->setLabelMode(RenderLabels::NoLabels | RenderLabels::StarLabels | RenderLabels::PlanetLabels | RenderLabels::MoonLabels | RenderLabels::ConstellationLabels | RenderLabels::GalaxyLabels | RenderLabels::AsteroidLabels | RenderLabels::SpacecraftLabels | RenderLabels::LocationLabels | RenderLabels::CometLabels | RenderLabels::NebulaLabels | RenderLabels::OpenClusterLabels | RenderLabels::I18nConstellationLabels | RenderLabels::DwarfPlanetLabels | RenderLabels::MinorMoonLabels | RenderLabels::GlobularLabels); return true; }
    if (n == "none")     { renderer->setLabelMode(RenderLabels::NoLabels); return true; }

    struct { const char* name; RenderLabels label; } map[] = {
        { "stars",          RenderLabels::StarLabels },
        { "planets",        RenderLabels::PlanetLabels },
        { "moons",          RenderLabels::MoonLabels },
        { "dwarfplanets",   RenderLabels::DwarfPlanetLabels },
        { "minormoons",     RenderLabels::MinorMoonLabels },
        { "asteroids",      RenderLabels::AsteroidLabels },
        { "comets",         RenderLabels::CometLabels },
        { "constellations", RenderLabels::ConstellationLabels },
        { "galaxies",       RenderLabels::GalaxyLabels },
        { "nebulae",        RenderLabels::NebulaLabels },
        { "openclusters",   RenderLabels::OpenClusterLabels },
        { "globulars",      RenderLabels::GlobularLabels },
        { "locations",      RenderLabels::LocationLabels },
        { "spacecraft",     RenderLabels::SpacecraftLabels },
    };

    for (const auto& e : map)
    {
        if (n == e.name)
        {
            label = e.label;
            break;
        }
    }

    if (label == RenderLabels::NoLabels)
        return false;

    if (force)
    {
        if (value) labels |= label; else labels &= ~label;
    }
    else
    {
        labels ^= label;
    }
    renderer->setLabelMode(labels);
    return true;
}

} // end unnamed namespace

QString
CelestiaCli::helpText()
{
    return QStringLiteral(
        "可用命令：\n"
        "  goto <天体> / go <天体>      飞往天体\n"
        "  select <天体>                选择天体\n"
        "  center <天体>                居中天体\n"
        "  follow <天体>                跟随天体\n"
        "  syncorbit <天体>             同步轨道跟随\n"
        "  home                        返回太阳系\n"
        "  time scale <倍率>            设置时间倍率（如 10000x / 0.5x）\n"
        "  time <儒略日>                设置时间\n"
        "  setsurface <名称>            切换表面\n"
        "  render <标记>                开关渲染（orbit/clouds/atmospheres/boundaries/markers/...）\n"
        "  label <标记>                 开关标签（stars/planets/moons/constellations/...）\n"
        "  location <纬度> <经度> [高度] 飞到地表位置\n"
        "  print <文字>                 显示消息\n"
        "  script <路径>                执行 celx 脚本\n"
        "  version / help / clear");
}

bool
CelestiaCli::execute(CelestiaCore* core, const QString& command)
{
    if (core == nullptr)
        return false;

    const QString cmd = command.trimmed();
    if (cmd.isEmpty())
        return true;

    const QStringList parts = cmd.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return true;

    const QString verb = parts[0].toLower();
    const QStringList args = parts.mid(1);

    Simulation* sim = core->getSimulation();
    Renderer* renderer = core->getRenderer();
    if (sim == nullptr || renderer == nullptr)
        return false;

    auto flash = [core](const QString& msg) { core->flash(msg.toStdString(), 2.0); };
    auto selectPath = [&](const QString& obj) -> bool {
        if (obj.isEmpty())
            return false;
        const Selection sel = sim->findObjectFromPath(obj.toStdString(), true);
        if (sel.empty())
        {
            flash(QStringLiteral("未找到天体: %1").arg(obj));
            return false;
        }
        sim->setSelection(sel);
        return true;
    };

    if (verb == "help")
    {
        flash(helpText());
        return true;
    }
    if (verb == "clear" || verb == "cls")
    {
        // nothing to clear on this UI; acknowledge
        return true;
    }
    if (verb == "version")
    {
        flash(QStringLiteral("Celestia %1 (HarmonyOS)").arg(QStringLiteral(VERSION)));
        return true;
    }

    if (verb == "goto" || verb == "go")
    {
        if (selectPath(args.value(0)))
        {
            sim->gotoSelection(5.0,
                               Eigen::Vector3f::UnitY(),
                               ObserverFrame::CoordinateSystem::ObserverLocal);
            flash(QStringLiteral("飞往 %1").arg(args[0]));
        }
        return true;
    }
    if (verb == "select")
    {
        selectPath(args.value(0));
        return true;
    }
    if (verb == "center")
    {
        if (selectPath(args.value(0)))
            sim->centerSelection(2.0);
        return true;
    }
    if (verb == "follow")
    {
        if (selectPath(args.value(0)))
        {
            sim->follow();
            flash(QStringLiteral("跟随 %1").arg(args[0]));
        }
        return true;
    }
    if (verb == "syncorbit")
    {
        if (selectPath(args.value(0)))
        {
            sim->geosynchronousFollow();
            flash(QStringLiteral("同步轨道 %1").arg(args[0]));
        }
        return true;
    }
    if (verb == "home")
    {
        if (selectPath(QStringLiteral("Sol")))
        {
            sim->follow();
            sim->gotoSelection(2.0,
                               Eigen::Vector3f::UnitY(),
                               ObserverFrame::CoordinateSystem::ObserverLocal);
            sim->setTimeScale(1.0);
        }
        return true;
    }

    if (verb == "time")
    {
        if (args.value(0) == "scale" || args.value(0) == "rate")
        {
            QString rateStr = args.value(1);
            bool ok = false;
            double rate = 1.0;
            if (rateStr.endsWith(QStringLiteral("x"), Qt::CaseInsensitive))
            {
                rateStr.chop(1);
                rate = rateStr.toDouble(&ok);
            }
            else
            {
                rate = rateStr.toDouble(&ok);
            }
            if (!ok)
            {
                flash(QStringLiteral("时间倍率格式错误: %1").arg(args.value(1)));
                return true;
            }
            sim->setTimeScale(rate);
            flash(QStringLiteral("时间倍率 %1x").arg(rate));
        }
        else
        {
            bool ok = false;
            const double jd = args.value(0).toDouble(&ok);
            if (!ok)
            {
                flash(QStringLiteral("时间格式错误，应为儒略日"));
                return true;
            }
            sim->setTime(jd);
        }
        return true;
    }
    if (verb == "ts")
    {
        bool ok = false;
        QString rateStr = args.value(0);
        double rate = 1.0;
        if (rateStr.endsWith(QStringLiteral("x"), Qt::CaseInsensitive))
        {
            rateStr.chop(1);
            rate = rateStr.toDouble(&ok);
        }
        else
        {
            rate = rateStr.toDouble(&ok);
        }
        if (ok)
        {
            sim->setTimeScale(rate);
            flash(QStringLiteral("时间倍率 %1x").arg(rate));
        }
        return true;
    }

    if (verb == "setsurface" || verb == "surface")
    {
        const QString surface = args.value(0);
        if (!surface.isEmpty())
        {
            sim->getActiveObserver()->setDisplayedSurface(surface.toStdString());
            flash(QStringLiteral("表面: %1").arg(surface));
        }
        return true;
    }

    if (verb == "render")
    {
        const QString flag = args.value(0);
        if (flag.isEmpty())
            return true;
        const bool force = flag.startsWith(QStringLiteral("+")) || flag.startsWith(QStringLiteral("-"));
        const bool value = flag.startsWith(QStringLiteral("+"));
        const QString name = force ? flag.mid(1) : flag;
        if (toggleRenderFlag(renderer, name, force, value))
            flash(QStringLiteral("渲染: %1").arg(flag));
        else
            flash(QStringLiteral("未知渲染标记: %1").arg(flag));
        return true;
    }

    if (verb == "label")
    {
        const QString flag = args.value(0);
        if (flag.isEmpty())
            return true;
        const bool force = flag.startsWith(QStringLiteral("+")) || flag.startsWith(QStringLiteral("-"));
        const bool value = flag.startsWith(QStringLiteral("+"));
        const QString name = force ? flag.mid(1) : flag;
        if (toggleLabelFlag(renderer, name, force, value))
            flash(QStringLiteral("标签: %1").arg(flag));
        else
            flash(QStringLiteral("未知标签标记: %1").arg(flag));
        return true;
    }

    if (verb == "location")
    {
        bool okLat = false, okLng = false, okAlt = false;
        const double lat = args.value(0).toDouble(&okLat);
        const double lng = args.value(1).toDouble(&okLng);
        double alt = args.value(2).toDouble(&okAlt);
        if (!okAlt)
            alt = 1000.0;
        if (okLat && okLng)
        {
            double distance = 0.0, curLat = 0.0, curLng = 0.0;
            sim->getSelectionLongLat(distance, curLat, curLng);
            sim->gotoSelectionLongLat(5.0, distance + alt,
                                      static_cast<float>(lng), static_cast<float>(lat),
                                      Eigen::Vector3f::UnitY());
            flash(QStringLiteral("飞到 %1° %2°").arg(lat).arg(lng));
        }
        else
        {
            flash(QStringLiteral("用法: location <纬度> <经度> [高度]"));
        }
        return true;
    }

    if (verb == "print")
    {
        flash(parts.mid(1).join(QStringLiteral(" ")));
        return true;
    }

    if (verb == "script")
    {
        const QString path = parts.mid(1).join(QStringLiteral(" "));
        if (!path.isEmpty())
            core->runScript(std::filesystem::path(path.toStdString()));
        return true;
    }

    return false;
}

} // end namespace celestia::qt
