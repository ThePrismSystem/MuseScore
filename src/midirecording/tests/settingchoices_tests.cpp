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

#include <gtest/gtest.h>

#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "midirecording/internal/settingchoices.h"

using namespace mu::midirecording;

class MidiRecording_SettingChoicesTests : public ::testing::Test
{
};

//! The codes of the menu's choices that hold for settings, in menu order
static std::vector<std::string> settingChoicesTestChosen(const RecordingSettings& settings)
{
    std::vector<std::string> codes;
    for (const SettingMenu& menu : settingMenus()) {
        for (const SettingChoice& choice : menu.choices) {
            if (choice.isChosen(settings)) {
                codes.push_back(choice.code);
            }
        }
    }
    return codes;
}

static const SettingMenu& settingChoicesTestMenu(const std::string& id)
{
    for (const SettingMenu& menu : settingMenus()) {
        if (menu.id == id) {
            return menu;
        }
    }
    ADD_FAILURE() << "no menu " << id;
    return settingMenus().front();
}

TEST_F(MidiRecording_SettingChoicesTests, CodesAreUniqueAndNamespaced)
{
    std::set<std::string> codes;
    for (const SettingMenu& menu : settingMenus()) {
        EXPECT_FALSE(menu.choices.empty()) << menu.id;
        for (const SettingChoice& choice : menu.choices) {
            EXPECT_EQ(choice.code.rfind("midi-recording-", 0), 0u) << choice.code;
            EXPECT_TRUE(codes.insert(choice.code).second) << choice.code;
            EXPECT_FALSE(choice.title.isEmpty()) << choice.code;
            EXPECT_FALSE(choice.description.isEmpty()) << choice.code;
        }
    }
}

TEST_F(MidiRecording_SettingChoicesTests, DefaultsChooseTheSpecDefaults)
{
    EXPECT_EQ(settingChoicesTestChosen(RecordingSettings()), std::vector<std::string>({
        "midi-recording-grid-120",
        "midi-recording-triplets-160",
        "midi-recording-min-rest-240",
        "midi-recording-brush-40",
        "midi-recording-overlaps-tied",
        "midi-recording-tidy-gaps",
        "midi-recording-replace-span",
        "midi-recording-count-in-1",
        "midi-recording-speed-100",
        "midi-recording-play-other-staves",
    }));
}

TEST_F(MidiRecording_SettingChoicesTests, ChoosingAChoiceChoosesOnlyIt)
{
    for (const SettingMenu& menu : settingMenus()) {
        if (menu.title.isEmpty()) {
            continue;
        }
        for (const SettingChoice& choice : menu.choices) {
            RecordingSettings settings;
            choice.choose(settings);
            for (const SettingChoice& other : menu.choices) {
                EXPECT_EQ(other.isChosen(settings), other.code == choice.code) << choice.code << " chosen, " << other.code;
            }
        }
    }
}

TEST_F(MidiRecording_SettingChoicesTests, TogglesTurnOffAndOn)
{
    const SettingChoice* tidy = findSettingChoice("midi-recording-tidy-gaps");
    ASSERT_TRUE(tidy);
    RecordingSettings settings;
    tidy->choose(settings);
    EXPECT_FALSE(settings.quantize.tidyGaps);
    EXPECT_FALSE(tidy->isChosen(settings));
    tidy->choose(settings);
    EXPECT_TRUE(settings.quantize.tidyGaps);

    const SettingChoice* otherStaves = findSettingChoice("midi-recording-play-other-staves");
    ASSERT_TRUE(otherStaves);
    otherStaves->choose(settings);
    EXPECT_FALSE(settings.playOtherStaves);
}

TEST_F(MidiRecording_SettingChoicesTests, ChoicesSetTheValuesTheyName)
{
    RecordingSettings settings;
    const std::vector<std::pair<std::string, std::function<bool(const RecordingSettings&)> > > cases = {
        { "midi-recording-grid-480", [](const RecordingSettings& s) { return s.quantize.gridTicks == 480; } },
        { "midi-recording-grid-60", [](const RecordingSettings& s) { return s.quantize.gridTicks == 60; } },
        { "midi-recording-triplets-off", [](const RecordingSettings& s) { return !s.quantize.triplets; } },
        { "midi-recording-triplets-320", [](const RecordingSettings& s) {
                return s.quantize.triplets && s.quantize.tripletUnitTicks == 320;
            } },
        { "midi-recording-min-rest-120", [](const RecordingSettings& s) { return s.quantize.minRestTicks == 120; } },
        { "midi-recording-brush-80", [](const RecordingSettings& s) { return s.quantize.brushMs == 80.0; } },
        { "midi-recording-overlaps-cut", [](const RecordingSettings& s) { return s.quantize.overlaps == OverlapMode::Cut; } },
        { "midi-recording-replace-voice", [](const RecordingSettings& s) { return s.replaceMode == "voice"; } },
        { "midi-recording-count-in-0", [](const RecordingSettings& s) { return s.countInBars == 0; } },
        { "midi-recording-count-in-2", [](const RecordingSettings& s) { return s.countInBars == 2; } },
        { "midi-recording-speed-25", [](const RecordingSettings& s) { return s.recordSpeedPercent == 25; } },
    };
    for (const auto& testCase : cases) {
        const SettingChoice* choice = findSettingChoice(testCase.first);
        ASSERT_TRUE(choice) << testCase.first;
        choice->choose(settings);
        EXPECT_TRUE(testCase.second(settings)) << testCase.first;
    }
}

TEST_F(MidiRecording_SettingChoicesTests, MenusOfferTheValuesTheConfigurationAllows)
{
    std::vector<int> grids;
    for (const SettingChoice& choice : settingChoicesTestMenu("grid").choices) {
        RecordingSettings settings;
        choice.choose(settings);
        grids.push_back(settings.quantize.gridTicks);
    }
    EXPECT_EQ(grids, std::vector<int>({ 480, 240, 120, 60 }));

    std::vector<int> units;
    for (const SettingChoice& choice : settingChoicesTestMenu("triplets").choices) {
        RecordingSettings settings;
        choice.choose(settings);
        if (settings.quantize.triplets) {
            units.push_back(settings.quantize.tripletUnitTicks);
        }
    }
    EXPECT_EQ(units, std::vector<int>({ 320, 160, 80 }));

    std::vector<int> countIns;
    for (const SettingChoice& choice : settingChoicesTestMenu("count-in").choices) {
        RecordingSettings settings;
        choice.choose(settings);
        countIns.push_back(settings.countInBars);
    }
    EXPECT_EQ(countIns, std::vector<int>({ 0, 1, 2 }));
}

TEST_F(MidiRecording_SettingChoicesTests, AValueOffThePresetsChoosesNothing)
{
    // As typed in Advanced preferences
    RecordingSettings settings;
    settings.quantize.brushMs = 25.0;
    settings.quantize.minRestTicks = 360;
    settings.recordSpeedPercent = 60;

    for (const char* id : { "brush", "min-rest", "speed" }) {
        for (const SettingChoice& choice : settingChoicesTestMenu(id).choices) {
            EXPECT_FALSE(choice.isChosen(settings)) << choice.code;
        }
    }

    findSettingChoice("midi-recording-brush-40")->choose(settings);
    EXPECT_TRUE(findSettingChoice("midi-recording-brush-40")->isChosen(settings));
}

TEST_F(MidiRecording_SettingChoicesTests, FindsAChoiceByItsCode)
{
    const SettingChoice* choice = findSettingChoice("midi-recording-grid-240");
    ASSERT_TRUE(choice);
    EXPECT_EQ(choice->code, "midi-recording-grid-240");
    EXPECT_EQ(findSettingChoice("midi-recording-grid-7"), nullptr);
    EXPECT_EQ(findSettingChoice("record-midi"), nullptr);
}

TEST_F(MidiRecording_SettingChoicesTests, SectionsStartAtReplace)
{
    std::vector<std::string> ids;
    std::vector<std::string> sectionStarts;
    for (const SettingMenu& menu : settingMenus()) {
        ids.push_back(menu.id);
        if (menu.startsSection) {
            sectionStarts.push_back(menu.id);
        }
    }
    EXPECT_EQ(ids, std::vector<std::string>({ "grid", "triplets", "min-rest", "brush", "overlaps", "tidy-gaps",
                                              "replace", "count-in", "speed", "play-other-staves" }));
    EXPECT_EQ(sectionStarts, std::vector<std::string>({ "replace" }));
}
