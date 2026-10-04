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

#include "settingchoices.h"

#include "realfn.h"

using namespace muse;
using namespace mu::midirecording;

using SettingChoicesIsChosen = std::function<bool (const RecordingSettings&)>;
using SettingChoicesChoose = std::function<void (RecordingSettings&)>;

static SettingChoice settingChoicesChoice(const std::string& code, const TranslatableString& title,
                                          const TranslatableString& description, const SettingChoicesIsChosen& isChosen,
                                          const SettingChoicesChoose& choose)
{
    SettingChoice choice;
    choice.code = code;
    choice.title = title;
    choice.description = description;
    choice.isChosen = isChosen;
    choice.choose = choose;
    return choice;
}

static SettingMenu settingChoicesMenu(const std::string& id, const TranslatableString& title, bool startsSection,
                                      const std::vector<SettingChoice>& choices)
{
    SettingMenu menu;
    menu.id = id;
    menu.title = title;
    menu.startsSection = startsSection;
    menu.choices = choices;
    return menu;
}

static SettingChoice settingChoicesGrid(int ticks, const TranslatableString& title, const TranslatableString& description)
{
    return settingChoicesChoice("midi-recording-grid-" + std::to_string(ticks), title, description,
                                [ticks](const RecordingSettings& settings) { return settings.quantize.gridTicks == ticks; },
                                [ticks](RecordingSettings& settings) { settings.quantize.gridTicks = ticks; });
}

static SettingChoice settingChoicesTriplets(int unitTicks, const TranslatableString& title, const TranslatableString& description)
{
    return settingChoicesChoice("midi-recording-triplets-" + std::to_string(unitTicks), title, description,
                                [unitTicks](const RecordingSettings& settings) {
        return settings.quantize.triplets && settings.quantize.tripletUnitTicks == unitTicks;
    },
                                [unitTicks](RecordingSettings& settings) {
        settings.quantize.triplets = true;
        settings.quantize.tripletUnitTicks = unitTicks;
    });
}

static SettingChoice settingChoicesMinRest(int ticks, const TranslatableString& title, const TranslatableString& description)
{
    return settingChoicesChoice("midi-recording-min-rest-" + std::to_string(ticks), title, description,
                                [ticks](const RecordingSettings& settings) { return settings.quantize.minRestTicks == ticks; },
                                [ticks](RecordingSettings& settings) { settings.quantize.minRestTicks = ticks; });
}

static SettingChoice settingChoicesBrush(int ms)
{
    return settingChoicesChoice("midi-recording-brush-" + std::to_string(ms),
                                TranslatableString("action", "%1 ms").arg(ms),
                                TranslatableString("action", "MIDI recording: notes played within %1 ms are one chord").arg(ms),
                                [ms](const RecordingSettings& settings) { return RealIsEqual(settings.quantize.brushMs, double(ms)); },
                                [ms](RecordingSettings& settings) { settings.quantize.brushMs = ms; });
}

static SettingChoice settingChoicesCountIn(int bars, const TranslatableString& title, const TranslatableString& description)
{
    return settingChoicesChoice("midi-recording-count-in-" + std::to_string(bars), title, description,
                                [bars](const RecordingSettings& settings) { return settings.countInBars == bars; },
                                [bars](RecordingSettings& settings) { settings.countInBars = bars; });
}

static SettingChoice settingChoicesSpeed(int percent)
{
    return settingChoicesChoice("midi-recording-speed-" + std::to_string(percent),
                                TranslatableString("action", "%1%").arg(percent),
                                TranslatableString("action", "MIDI recording: record at %1% of the score’s tempo").arg(percent),
                                [percent](const RecordingSettings& settings) { return settings.recordSpeedPercent == percent; },
                                [percent](RecordingSettings& settings) { settings.recordSpeedPercent = percent; });
}

static std::vector<SettingMenu> settingChoicesMenus()
{
    std::vector<SettingMenu> menus;

    menus.push_back(settingChoicesMenu("grid", TranslatableString("action", "Grid"), false, {
        settingChoicesGrid(480, TranslatableString("action", "Quarter note"),
                           TranslatableString("action", "MIDI recording: quarter-note grid")),
        settingChoicesGrid(240, TranslatableString("action", "Eighth note"),
                           TranslatableString("action", "MIDI recording: eighth-note grid")),
        settingChoicesGrid(120, TranslatableString("action", "16th note"),
                           TranslatableString("action", "MIDI recording: 16th-note grid")),
        settingChoicesGrid(60, TranslatableString("action", "32nd note"),
                           TranslatableString("action", "MIDI recording: 32nd-note grid")),
    }));

    menus.push_back(settingChoicesMenu("triplets", TranslatableString("action", "Triplets"), false, {
        settingChoicesChoice("midi-recording-triplets-off", TranslatableString("action", "Off"),
                             TranslatableString("action", "MIDI recording: no triplets"),
                             [](const RecordingSettings& settings) { return !settings.quantize.triplets; },
                             [](RecordingSettings& settings) { settings.quantize.triplets = false; }),
        settingChoicesTriplets(320, TranslatableString("action", "Quarter-note triplets"),
                               TranslatableString("action", "MIDI recording: quarter-note triplets")),
        settingChoicesTriplets(160, TranslatableString("action", "Eighth-note triplets"),
                               TranslatableString("action", "MIDI recording: eighth-note triplets")),
        settingChoicesTriplets(80, TranslatableString("action", "16th-note triplets"),
                               TranslatableString("action", "MIDI recording: 16th-note triplets")),
    }));

    menus.push_back(settingChoicesMenu("min-rest", TranslatableString("action", "Shortest rest"), false, {
        settingChoicesMinRest(120, TranslatableString("action", "16th note"),
                              TranslatableString("action", "MIDI recording: shortest rest a 16th")),
        settingChoicesMinRest(240, TranslatableString("action", "Eighth note"),
                              TranslatableString("action", "MIDI recording: shortest rest an eighth")),
        settingChoicesMinRest(480, TranslatableString("action", "Quarter note"),
                              TranslatableString("action", "MIDI recording: shortest rest a quarter")),
    }));

    menus.push_back(settingChoicesMenu("brush", TranslatableString("action", "Chord spread"), false, {
        settingChoicesBrush(20),
        settingChoicesBrush(40),
        settingChoicesBrush(60),
        settingChoicesBrush(80),
    }));

    menus.push_back(settingChoicesMenu("overlaps", TranslatableString("action", "Overlapping notes"), false, {
        settingChoicesChoice("midi-recording-overlaps-tied", TranslatableString("action", "Tie", "MIDI recording: overlapping notes"),
                             TranslatableString("action", "MIDI recording: tie overlapping notes"),
                             [](const RecordingSettings& settings) { return settings.quantize.overlaps == OverlapMode::Tied; },
                             [](RecordingSettings& settings) { settings.quantize.overlaps = OverlapMode::Tied; }),
        settingChoicesChoice("midi-recording-overlaps-cut", TranslatableString("action", "Cut", "MIDI recording: overlapping notes"),
                             TranslatableString("action", "MIDI recording: cut overlapping notes"),
                             [](const RecordingSettings& settings) { return settings.quantize.overlaps == OverlapMode::Cut; },
                             [](RecordingSettings& settings) { settings.quantize.overlaps = OverlapMode::Cut; }),
    }));

    menus.push_back(settingChoicesMenu("tidy-gaps", TranslatableString(), false, {
        settingChoicesChoice("midi-recording-tidy-gaps", TranslatableString("action", "Tidy gaps"),
                             TranslatableString("action", "MIDI recording: tidy short gaps and overlaps"),
                             [](const RecordingSettings& settings) { return settings.quantize.tidyGaps; },
                             [](RecordingSettings& settings) { settings.quantize.tidyGaps = !settings.quantize.tidyGaps; }),
    }));

    menus.push_back(settingChoicesMenu("replace", TranslatableString("action", "Replace"), true, {
        settingChoicesChoice("midi-recording-replace-span", TranslatableString("action", "All voices"),
                             TranslatableString("action", "MIDI recording: replace all voices"),
                             [](const RecordingSettings& settings) { return settings.replaceMode == "span"; },
                             [](RecordingSettings& settings) { settings.replaceMode = "span"; }),
        settingChoicesChoice("midi-recording-replace-voice", TranslatableString("action", "Selected voice"),
                             TranslatableString("action", "MIDI recording: replace the selected voice"),
                             [](const RecordingSettings& settings) { return settings.replaceMode == "voice"; },
                             [](RecordingSettings& settings) { settings.replaceMode = "voice"; }),
    }));

    menus.push_back(settingChoicesMenu("count-in", TranslatableString("action", "Count-in"), false, {
        settingChoicesCountIn(0, TranslatableString("action", "None"),
                              TranslatableString("action", "MIDI recording: no count-in")),
        settingChoicesCountIn(1, TranslatableString("action", "1 bar"),
                              TranslatableString("action", "MIDI recording: count in 1 bar")),
        settingChoicesCountIn(2, TranslatableString("action", "2 bars"),
                              TranslatableString("action", "MIDI recording: count in 2 bars")),
    }));

    menus.push_back(settingChoicesMenu("speed", TranslatableString("action", "Record speed"), false, {
        settingChoicesSpeed(25),
        settingChoicesSpeed(50),
        settingChoicesSpeed(75),
        settingChoicesSpeed(100),
    }));

    menus.push_back(settingChoicesMenu("play-other-staves", TranslatableString(), false, {
        settingChoicesChoice("midi-recording-play-other-staves", TranslatableString("action", "Play other staves"),
                             TranslatableString("action", "MIDI recording: play the other staves during a take"),
                             [](const RecordingSettings& settings) { return settings.playOtherStaves; },
                             [](RecordingSettings& settings) { settings.playOtherStaves = !settings.playOtherStaves; }),
    }));

    return menus;
}

const std::vector<SettingMenu>& mu::midirecording::settingMenus()
{
    static const std::vector<SettingMenu> menus = settingChoicesMenus();
    return menus;
}

const SettingChoice* mu::midirecording::findSettingChoice(const std::string& code)
{
    for (const SettingMenu& menu : settingMenus()) {
        for (const SettingChoice& choice : menu.choices) {
            if (choice.code == code) {
                return &choice;
            }
        }
    }
    return nullptr;
}
