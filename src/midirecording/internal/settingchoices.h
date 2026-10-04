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

#include <functional>
#include <string>
#include <vector>

#include "types/translatablestring.h"

#include "recordingtypes.h"

namespace mu::midirecording {
//! One item of the MIDI recording menu: an action that sets a value, or that
//! turns a setting on or off
struct SettingChoice {
    std::string code;                       // the action's code
    muse::TranslatableString title;         // as the menu shows it
    muse::TranslatableString description;   // as the shortcut list shows it
    std::function<bool(const RecordingSettings&)> isChosen;
    std::function<void(RecordingSettings&)> choose;
};

//! A submenu of choices, one of which is chosen at a time. With no title, its
//! choices turn settings on and off from the menu itself.
struct SettingMenu {
    std::string id;
    muse::TranslatableString title;
    bool startsSection = false;   // a separator goes before it
    std::vector<SettingChoice> choices;
};

//! The settings in the MIDI recording menu, in menu order
const std::vector<SettingMenu>& settingMenus();

//! The choice with that action code, or nullptr
const SettingChoice* findSettingChoice(const std::string& code);
}
