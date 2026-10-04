/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <string>
#include <vector>

#include "actions/actiontypes.h"
#include "async/notification.h"
#include "modularity/imoduleinterface.h"
#include "types/translatablestring.h"

namespace mu::midirecording {
//! One entry of the MIDI recording menu: an action, a submenu, or a separator
struct MidiRecordingMenuEntry {
    muse::actions::ActionCode code;   // the action; empty for a submenu or a separator
    std::string menuId;               // a submenu's id
    muse::TranslatableString title;   // a submenu's title
    std::vector<MidiRecordingMenuEntry> subitems;

    bool isSeparator() const { return code.empty() && subitems.empty(); }
};

//! What the playback toolbar shows for MIDI recording: the Record button and
//! the menu beside it. Header only, so the playback module can resolve it
//! without linking this one; when the MIDI recording module is not built,
//! nothing is registered and the toolbar shows neither.
class IMidiRecordingMenu : MODULE_GLOBAL_INTERFACE
{
    INTERFACE_ID(IMidiRecordingMenu)

public:
    virtual ~IMidiRecordingMenu() = default;

    virtual muse::actions::ActionCode recordActionCode() const = 0;

    virtual std::vector<MidiRecordingMenuEntry> settingsMenu() const = 0;

    //! The menu has to be built again: a title in it changed
    virtual muse::async::Notification settingsMenuChanged() const = 0;
};
}
