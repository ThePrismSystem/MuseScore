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

#include "midirecordingconfiguration.h"

#include <algorithm>
#include <vector>

#include "settings.h"

using namespace muse;
using namespace mu::midirecording;

static const std::string midiRecordingSettingsModule("midirecording");

static const Settings::Key GRID_KEY(midiRecordingSettingsModule, "application/midiRecording/grid");
static const Settings::Key TRIPLETS_KEY(midiRecordingSettingsModule, "application/midiRecording/triplets");
static const Settings::Key TRIPLET_UNIT_KEY(midiRecordingSettingsModule, "application/midiRecording/tripletUnit");
static const Settings::Key TIDY_GAPS_KEY(midiRecordingSettingsModule, "application/midiRecording/tidyGaps");
static const Settings::Key MIN_REST_KEY(midiRecordingSettingsModule, "application/midiRecording/minRest");
static const Settings::Key BRUSH_MS_KEY(midiRecordingSettingsModule, "application/midiRecording/brushMs");
static const Settings::Key OVERLAPS_KEY(midiRecordingSettingsModule, "application/midiRecording/overlaps");
static const Settings::Key REPLACE_MODE_KEY(midiRecordingSettingsModule, "application/midiRecording/replaceMode");
static const Settings::Key COUNT_IN_BARS_KEY(midiRecordingSettingsModule, "application/midiRecording/countInBars");
static const Settings::Key RECORD_SPEED_PERCENT_KEY(midiRecordingSettingsModule, "application/midiRecording/recordSpeedPercent");
static const Settings::Key PLAY_OTHER_STAVES_KEY(midiRecordingSettingsModule, "application/midiRecording/playOtherStaves");
static const Settings::Key LATENCY_MS_KEY(midiRecordingSettingsModule, "application/midiRecording/latencyMs");

//! The note values the quantizer is offered: quarter, eighth, 16th, 32nd
static const std::vector<int> MIDIRECORDING_GRID_TICKS = { 480, 240, 120, 60 };
//! Quarter, eighth and 16th triplets
static const std::vector<int> MIDIRECORDING_TRIPLET_UNIT_TICKS = { 320, 160, 80 };

static constexpr int MIDIRECORDING_MAX_COUNT_IN_BARS = 2;
static constexpr int MIDIRECORDING_MIN_SPEED_PERCENT = 10;
static constexpr int MIDIRECORDING_MAX_SPEED_PERCENT = 200;
static constexpr int MIDIRECORDING_MAX_MIN_REST_TICKS = 1920;
static constexpr double MIDIRECORDING_MAX_BRUSH_MS = 200.0;
static constexpr double MIDIRECORDING_MAX_LATENCY_MS = 500.0;

//! value when it is one of allowed, fallback when it is not: the advanced
//! preferences let any number in range be typed
static int midiRecordingConfigurationOneOf(int value, const std::vector<int>& allowed, int fallback)
{
    return std::find(allowed.cbegin(), allowed.cend(), value) != allowed.cend() ? value : fallback;
}

void MidiRecordingConfiguration::init()
{
    const QuantizeSettings defaults;

    settings()->setDefaultValue(GRID_KEY, Val(defaults.gridTicks));
    settings()->setCanBeManuallyEdited(GRID_KEY, true, Val(MIDIRECORDING_GRID_TICKS.back()), Val(MIDIRECORDING_GRID_TICKS.front()));
    settings()->setDefaultValue(TRIPLETS_KEY, Val(defaults.triplets));
    settings()->setCanBeManuallyEdited(TRIPLETS_KEY, true);
    settings()->setDefaultValue(TRIPLET_UNIT_KEY, Val(defaults.tripletUnitTicks));
    settings()->setCanBeManuallyEdited(TRIPLET_UNIT_KEY, true, Val(MIDIRECORDING_TRIPLET_UNIT_TICKS.back()),
                                       Val(MIDIRECORDING_TRIPLET_UNIT_TICKS.front()));
    settings()->setDefaultValue(TIDY_GAPS_KEY, Val(defaults.tidyGaps));
    settings()->setCanBeManuallyEdited(TIDY_GAPS_KEY, true);
    settings()->setDefaultValue(MIN_REST_KEY, Val(defaults.minRestTicks));
    settings()->setCanBeManuallyEdited(MIN_REST_KEY, true, Val(0), Val(MIDIRECORDING_MAX_MIN_REST_TICKS));
    settings()->setDefaultValue(BRUSH_MS_KEY, Val(defaults.brushMs));
    settings()->setCanBeManuallyEdited(BRUSH_MS_KEY, true, Val(0.0), Val(MIDIRECORDING_MAX_BRUSH_MS));
    settings()->setDefaultValue(OVERLAPS_KEY, Val("tied"));
    settings()->setCanBeManuallyEdited(OVERLAPS_KEY, true);
    settings()->setDefaultValue(REPLACE_MODE_KEY, Val("span"));
    settings()->setCanBeManuallyEdited(REPLACE_MODE_KEY, true);
    settings()->setDefaultValue(COUNT_IN_BARS_KEY, Val(1));
    settings()->setCanBeManuallyEdited(COUNT_IN_BARS_KEY, true, Val(0), Val(MIDIRECORDING_MAX_COUNT_IN_BARS));
    settings()->setDefaultValue(RECORD_SPEED_PERCENT_KEY, Val(100));
    settings()->setCanBeManuallyEdited(RECORD_SPEED_PERCENT_KEY, true, Val(MIDIRECORDING_MIN_SPEED_PERCENT),
                                       Val(MIDIRECORDING_MAX_SPEED_PERCENT));
    settings()->setDefaultValue(PLAY_OTHER_STAVES_KEY, Val(true));
    settings()->setCanBeManuallyEdited(PLAY_OTHER_STAVES_KEY, true);
    settings()->setDefaultValue(LATENCY_MS_KEY, Val(0.0));
    settings()->setCanBeManuallyEdited(LATENCY_MS_KEY, true, Val(-MIDIRECORDING_MAX_LATENCY_MS), Val(MIDIRECORDING_MAX_LATENCY_MS));

    for (const Settings::Key& key : { GRID_KEY, TRIPLETS_KEY, TRIPLET_UNIT_KEY, TIDY_GAPS_KEY, MIN_REST_KEY, BRUSH_MS_KEY, OVERLAPS_KEY,
                                      REPLACE_MODE_KEY, COUNT_IN_BARS_KEY, RECORD_SPEED_PERCENT_KEY, PLAY_OTHER_STAVES_KEY }) {
        settings()->valueChanged(key).onReceive(nullptr, [this](const Val&) {
            m_settingsChanged.notify();
        });
    }
    settings()->valueChanged(LATENCY_MS_KEY).onReceive(nullptr, [this](const Val&) {
        m_latencyMsChanged.notify();
    });
}

QuantizeSettings MidiRecordingConfiguration::quantizeSettings() const
{
    const QuantizeSettings defaults;

    QuantizeSettings result;
    result.gridTicks = midiRecordingConfigurationOneOf(settings()->value(GRID_KEY).toInt(), MIDIRECORDING_GRID_TICKS,
                                                       defaults.gridTicks);
    result.triplets = settings()->value(TRIPLETS_KEY).toBool();
    result.tripletUnitTicks = midiRecordingConfigurationOneOf(settings()->value(TRIPLET_UNIT_KEY).toInt(),
                                                              MIDIRECORDING_TRIPLET_UNIT_TICKS, defaults.tripletUnitTicks);
    result.tidyGaps = settings()->value(TIDY_GAPS_KEY).toBool();
    result.minRestTicks = std::max(0, settings()->value(MIN_REST_KEY).toInt());
    result.brushMs = std::max(0.0, settings()->value(BRUSH_MS_KEY).toDouble());
    result.overlaps = settings()->value(OVERLAPS_KEY).toString() == "cut" ? OverlapMode::Cut : OverlapMode::Tied;
    return result;
}

std::string MidiRecordingConfiguration::replaceMode() const
{
    return settings()->value(REPLACE_MODE_KEY).toString() == "voice" ? "voice" : "span";
}

int MidiRecordingConfiguration::countInBars() const
{
    return std::clamp(settings()->value(COUNT_IN_BARS_KEY).toInt(), 0, MIDIRECORDING_MAX_COUNT_IN_BARS);
}

int MidiRecordingConfiguration::recordSpeedPercent() const
{
    return std::clamp(settings()->value(RECORD_SPEED_PERCENT_KEY).toInt(), MIDIRECORDING_MIN_SPEED_PERCENT,
                      MIDIRECORDING_MAX_SPEED_PERCENT);
}

bool MidiRecordingConfiguration::playOtherStaves() const
{
    return settings()->value(PLAY_OTHER_STAVES_KEY).toBool();
}

double MidiRecordingConfiguration::latencyMs() const
{
    return std::clamp(settings()->value(LATENCY_MS_KEY).toDouble(), -MIDIRECORDING_MAX_LATENCY_MS, MIDIRECORDING_MAX_LATENCY_MS);
}

RecordingSettings MidiRecordingConfiguration::recordingSettings() const
{
    RecordingSettings recording;
    recording.quantize = quantizeSettings();
    recording.replaceMode = replaceMode();
    recording.countInBars = countInBars();
    recording.recordSpeedPercent = recordSpeedPercent();
    recording.playOtherStaves = playOtherStaves();
    return recording;
}

void MidiRecordingConfiguration::setRecordingSettings(const RecordingSettings& recording)
{
    settings()->setSharedValue(GRID_KEY, Val(recording.quantize.gridTicks));
    settings()->setSharedValue(TRIPLETS_KEY, Val(recording.quantize.triplets));
    settings()->setSharedValue(TRIPLET_UNIT_KEY, Val(recording.quantize.tripletUnitTicks));
    settings()->setSharedValue(TIDY_GAPS_KEY, Val(recording.quantize.tidyGaps));
    settings()->setSharedValue(MIN_REST_KEY, Val(recording.quantize.minRestTicks));
    settings()->setSharedValue(BRUSH_MS_KEY, Val(recording.quantize.brushMs));
    settings()->setSharedValue(OVERLAPS_KEY, Val(recording.quantize.overlaps == OverlapMode::Cut ? "cut" : "tied"));
    settings()->setSharedValue(REPLACE_MODE_KEY, Val(recording.replaceMode));
    settings()->setSharedValue(COUNT_IN_BARS_KEY, Val(recording.countInBars));
    settings()->setSharedValue(RECORD_SPEED_PERCENT_KEY, Val(recording.recordSpeedPercent));
    settings()->setSharedValue(PLAY_OTHER_STAVES_KEY, Val(recording.playOtherStaves));
}

muse::async::Notification MidiRecordingConfiguration::settingsChanged() const
{
    return m_settingsChanged;
}

void MidiRecordingConfiguration::setLatencyMs(double ms)
{
    settings()->setSharedValue(LATENCY_MS_KEY, Val(std::clamp(ms, -MIDIRECORDING_MAX_LATENCY_MS, MIDIRECORDING_MAX_LATENCY_MS)));
}

muse::async::Notification MidiRecordingConfiguration::latencyMsChanged() const
{
    return m_latencyMsChanged;
}
