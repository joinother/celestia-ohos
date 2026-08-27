// qtcli.h
//
// Celestia CLI command dispatcher for the HarmonyOS port.
//
// Provides a small command language (goto/select/follow/time/render/...)
// executed against a running CelestiaCore, mirroring the classic Celestia
// console commands. Commands are fed from the ArkTS UI via a command file
// (see CelestiaAppWindow::processCliCommands).
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#pragma once

#include <QString>

class CelestiaCore;

namespace celestia::qt
{

class CelestiaCli
{
public:
    /*! Execute a single CLI command against the core. Returns true when the
     *  command was recognised and handled, false otherwise. */
    static bool execute(CelestiaCore* core, const QString& command);

    /*! Multi-line help text describing the supported commands. */
    static QString helpText();
};

} // end namespace celestia::qt
