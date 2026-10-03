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

#include "midirecording/internal/takefile.h"

using namespace mu::midirecording;
using namespace muse;

class MidiRecording_TakeFileTests : public ::testing::Test
{
};

//! Three hours of nanoseconds: well past int32
static constexpr int64_t TAKEFILE_TEST_THREE_HOURS_NS = 10800000000000;

static TakeFile takeFileTestTake()
{
    TakeFile take;
    take.startTick = 960;
    take.staffIdx = 2;
    take.voice = 1;
    take.replaceMode = "voice";
    take.countInBars = 2;
    take.recordSpeedPercent = 60;
    take.latencyMs = 37.5;
    take.stopNs = TAKEFILE_TEST_THREE_HOURS_NS + 123;
    take.settings.gridTicks = 240;
    take.settings.triplets = false;
    take.settings.tripletUnitTicks = 320;
    take.settings.tidyGaps = false;
    take.settings.minRestTicks = 480;
    take.settings.brushMs = 25.0;
    take.settings.overlaps = OverlapMode::Cut;
    take.events = { { TAKEFILE_TEST_THREE_HOURS_NS, true, 60, 101 }, { TAKEFILE_TEST_THREE_HOURS_NS + 77, false, 60, 0 } };
    take.clock = { { 1000, 0.0 }, { TAKEFILE_TEST_THREE_HOURS_NS + 50, 1.25 } };
    return take;
}

static const char* TAKEFILE_TEST_HEADER
    = "\"version\":1,\"startTick\":0,\"staffIdx\":0,\"voice\":0,\"replaceMode\":\"span\",\"countInBars\":1,"
      "\"recordSpeedPercent\":100,\"latencyMs\":0,\"stopNs\":\"0\"";

TEST_F(MidiRecording_TakeFileTests, RoundTripKeepsEverything)
{
    const TakeFile take = takeFileTestTake();

    const RetVal<TakeFile> loaded = takeFromJson(takeToJson(take));

    ASSERT_TRUE(loaded.ret) << loaded.ret.text();
    const TakeFile& back = loaded.val;
    EXPECT_EQ(back.version, TakeFile::CURRENT_VERSION);
    EXPECT_EQ(back.startTick, 960);
    EXPECT_EQ(back.staffIdx, 2);
    EXPECT_EQ(back.voice, 1);
    EXPECT_EQ(back.replaceMode, "voice");
    EXPECT_EQ(back.countInBars, 2);
    EXPECT_EQ(back.recordSpeedPercent, 60);
    EXPECT_DOUBLE_EQ(back.latencyMs, 37.5);
    EXPECT_EQ(back.stopNs, TAKEFILE_TEST_THREE_HOURS_NS + 123);
    EXPECT_EQ(back.settings.gridTicks, 240);
    EXPECT_FALSE(back.settings.triplets);
    EXPECT_EQ(back.settings.tripletUnitTicks, 320);
    EXPECT_FALSE(back.settings.tidyGaps);
    EXPECT_EQ(back.settings.minRestTicks, 480);
    EXPECT_DOUBLE_EQ(back.settings.brushMs, 25.0);
    EXPECT_EQ(back.settings.overlaps, OverlapMode::Cut);
    ASSERT_EQ(back.events.size(), 2u);
    EXPECT_EQ(back.events[0].ns, TAKEFILE_TEST_THREE_HOURS_NS);
    EXPECT_TRUE(back.events[0].on);
    EXPECT_EQ(back.events[0].pitch, 60);
    EXPECT_EQ(back.events[0].velocity, 101);
    EXPECT_EQ(back.events[1].ns, TAKEFILE_TEST_THREE_HOURS_NS + 77);
    EXPECT_FALSE(back.events[1].on);
    ASSERT_EQ(back.clock.size(), 2u);
    EXPECT_EQ(back.clock[1].hostNs, TAKEFILE_TEST_THREE_HOURS_NS + 50);
    EXPECT_DOUBLE_EQ(back.clock[1].playbackSecs, 1.25);
}

TEST_F(MidiRecording_TakeFileTests, RejectsInvalidJson)
{
    EXPECT_FALSE(takeFromJson(ByteArray("not json")).ret);
}

TEST_F(MidiRecording_TakeFileTests, RejectsUnknownVersion)
{
    TakeFile take = takeFileTestTake();
    take.version = 99;

    EXPECT_FALSE(takeFromJson(takeToJson(take)).ret);
}

TEST_F(MidiRecording_TakeFileTests, RejectsMissingKey)
{
    const std::string json = std::string("{") + TAKEFILE_TEST_HEADER + ",\"settings\":{},\"events\":[]}";

    EXPECT_FALSE(takeFromJson(ByteArray(json.c_str())).ret);
}

TEST_F(MidiRecording_TakeFileTests, RejectsMalformedEvent)
{
    const std::string json = std::string("{") + TAKEFILE_TEST_HEADER
                             + ",\"settings\":{},\"events\":[[1,true,60]],\"clock\":[]}";

    EXPECT_FALSE(takeFromJson(ByteArray(json.c_str())).ret);
}

TEST_F(MidiRecording_TakeFileTests, RejectsTimeThatIsNotAWholeNumberString)
{
    const std::string head = std::string("{") + TAKEFILE_TEST_HEADER + ",\"settings\":{},";

    EXPECT_FALSE(takeFromJson(ByteArray((head + "\"events\":[[5,true,60,100]],\"clock\":[]}").c_str())).ret);
    EXPECT_FALSE(takeFromJson(ByteArray((head + "\"events\":[[\"5x\",true,60,100]],\"clock\":[]}").c_str())).ret);
    EXPECT_FALSE(takeFromJson(ByteArray((head + "\"events\":[],\"clock\":[[\"\",0.5]]}").c_str())).ret);

    std::string stopAsNumber = head + "\"events\":[],\"clock\":[]}";
    stopAsNumber.replace(stopAsNumber.find("\"stopNs\":\"0\""), 12, "\"stopNs\":0");
    EXPECT_FALSE(takeFromJson(ByteArray(stopAsNumber.c_str())).ret);

    EXPECT_TRUE(takeFromJson(ByteArray((head + "\"events\":[[\"5\",true,60,100]],\"clock\":[]}").c_str())).ret);
}

TEST_F(MidiRecording_TakeFileTests, MissingSettingsTakeDefaults)
{
    const std::string json = std::string("{") + TAKEFILE_TEST_HEADER + ",\"settings\":{},\"events\":[],\"clock\":[]}";

    const RetVal<TakeFile> loaded = takeFromJson(ByteArray(json.c_str()));

    ASSERT_TRUE(loaded.ret) << loaded.ret.text();
    EXPECT_EQ(loaded.val.settings.gridTicks, 120);
    EXPECT_TRUE(loaded.val.settings.triplets);
    EXPECT_EQ(loaded.val.settings.overlaps, OverlapMode::Tied);
}

TEST_F(MidiRecording_TakeFileTests, RejectsSettingsOutOfRange)
{
    const auto load = [](const std::string& settings) {
        const std::string json = std::string("{") + TAKEFILE_TEST_HEADER + ",\"settings\":{" + settings + "},\"events\":[],\"clock\":[]}";
        return takeFromJson(ByteArray(json.c_str()));
    };

    const std::pair<const char*, const char*> outOfRange[] = {
        { "gridTicks", "\"gridTicks\":0" },
        { "tripletUnitTicks", "\"tripletUnitTicks\":0" },
        { "minRestTicks", "\"minRestTicks\":-1" },
        { "brushMs", "\"brushMs\":-0.5" },
    };
    for (const auto& [field, settings] : outOfRange) {
        const RetVal<TakeFile> loaded = load(settings);
        EXPECT_FALSE(loaded.ret) << field;
        EXPECT_NE(loaded.ret.text().find(field), std::string::npos) << loaded.ret.text();
    }

    const RetVal<TakeFile> edges = load("\"gridTicks\":1,\"tripletUnitTicks\":1,\"minRestTicks\":0,\"brushMs\":0");
    ASSERT_TRUE(edges.ret) << edges.ret.text();
    EXPECT_EQ(edges.val.settings.gridTicks, 1);
    EXPECT_EQ(edges.val.settings.tripletUnitTicks, 1);
    EXPECT_EQ(edges.val.settings.minRestTicks, 0);
    EXPECT_DOUBLE_EQ(edges.val.settings.brushMs, 0.0);
}
