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
#include "takefile.h"

#include <charconv>
#include <string>

#include "serialization/json.h"

using namespace muse;
using namespace mu::midirecording;

static const char* takeFileOverlapName(OverlapMode mode)
{
    return mode == OverlapMode::Cut ? "cut" : "tied";
}

static bool takeFileReadNs(const JsonValue& value, int64_t& ns)
{
    if (!value.isString()) {
        return false;
    }
    const std::string& text = value.toStdString();
    const char* end = text.data() + text.size();
    const std::from_chars_result parsed = std::from_chars(text.data(), end, ns);
    return !text.empty() && parsed.ec == std::errc() && parsed.ptr == end;
}

static RetVal<TakeFile> takeFileError(const std::string& message)
{
    RetVal<TakeFile> result;
    result.ret = make_ret(Ret::Code::UnknownError, message);
    return result;
}

ByteArray mu::midirecording::takeToJson(const TakeFile& take)
{
    JsonObject settings;
    settings.set("gridTicks", take.settings.gridTicks);
    settings.set("triplets", take.settings.triplets);
    settings.set("tripletUnitTicks", take.settings.tripletUnitTicks);
    settings.set("tidyGaps", take.settings.tidyGaps);
    settings.set("minRestTicks", take.settings.minRestTicks);
    settings.set("brushMs", take.settings.brushMs);
    settings.set("overlaps", takeFileOverlapName(take.settings.overlaps));

    JsonArray events;
    for (const RawEvent& event : take.events) {
        events.append(JsonArray({ JsonValue(std::to_string(event.ns)), JsonValue(event.on),
                                  JsonValue(event.pitch), JsonValue(event.velocity) }));
    }

    JsonArray clock;
    for (const ClockSample& sample : take.clock) {
        clock.append(JsonArray({ JsonValue(std::to_string(sample.hostNs)), JsonValue(sample.playbackSecs) }));
    }

    JsonArray measures;
    for (const MeasureSpan& measure : take.measures) {
        measures.append(JsonArray({ JsonValue(measure.startTick), JsonValue(measure.ticks), JsonValue(measure.sigN),
                                    JsonValue(measure.sigD) }));
    }

    JsonArray timeMap;
    for (const TimeKnot& knot : take.timeMap) {
        timeMap.append(JsonArray({ JsonValue(knot.secs), JsonValue(knot.tick) }));
    }

    JsonObject root;
    root.set("version", take.version);
    root.set("startTick", take.startTick);
    root.set("staffIdx", take.staffIdx);
    root.set("voice", take.voice);
    root.set("replaceMode", take.replaceMode);
    root.set("countInBars", take.countInBars);
    root.set("recordSpeedPercent", take.recordSpeedPercent);
    root.set("latencyMs", take.latencyMs);
    root.set("stopNs", std::to_string(take.stopNs));
    root.set("settings", settings);
    root.set("events", events);
    root.set("clock", clock);
    root.set("measures", measures);
    root.set("timeMap", timeMap);

    return JsonDocument(root).toJson();
}

RetVal<TakeFile> mu::midirecording::takeFromJson(const ByteArray& data)
{
    std::string error;
    const JsonDocument document = JsonDocument::fromJson(data, &error);
    if (!error.empty() || !document.isObject()) {
        return takeFileError("take file is not a JSON object: " + error);
    }

    const JsonObject root = document.rootObject();
    static const char* const REQUIRED_KEYS[] = {
        "version", "startTick", "staffIdx", "voice", "replaceMode", "countInBars",
        "recordSpeedPercent", "latencyMs", "stopNs", "settings", "events", "clock", "measures", "timeMap"
    };
    for (const char* key : REQUIRED_KEYS) {
        if (!root.contains(key)) {
            return takeFileError(std::string("take file has no ") + key);
        }
    }

    TakeFile take;
    take.version = root.value("version").toInt();
    if (take.version != TakeFile::CURRENT_VERSION) {
        return takeFileError("unsupported take file version " + std::to_string(take.version));
    }

    take.startTick = root.value("startTick").toInt();
    take.staffIdx = root.value("staffIdx").toInt();
    take.voice = root.value("voice").toInt();
    take.replaceMode = root.value("replaceMode").toStdString();
    take.countInBars = root.value("countInBars").toInt();
    take.recordSpeedPercent = root.value("recordSpeedPercent").toInt();
    take.latencyMs = root.value("latencyMs").toDouble();
    if (!takeFileReadNs(root.value("stopNs"), take.stopNs)) {
        return takeFileError("take file stopNs is not a whole number in a string");
    }

    const JsonObject settings = root.value("settings").toObject();
    const QuantizeSettings defaults;
    take.settings.gridTicks = settings.value("gridTicks", defaults.gridTicks).toInt();
    take.settings.triplets = settings.value("triplets", defaults.triplets).toBool();
    take.settings.tripletUnitTicks = settings.value("tripletUnitTicks", defaults.tripletUnitTicks).toInt();
    take.settings.tidyGaps = settings.value("tidyGaps", defaults.tidyGaps).toBool();
    take.settings.minRestTicks = settings.value("minRestTicks", defaults.minRestTicks).toInt();
    take.settings.brushMs = settings.value("brushMs", defaults.brushMs).toDouble();
    take.settings.overlaps = settings.value("overlaps", "tied").toStdString() == "cut" ? OverlapMode::Cut : OverlapMode::Tied;
    if (take.settings.gridTicks <= 0) {
        return takeFileError("take file settings gridTicks must be above 0");
    }
    if (take.settings.tripletUnitTicks <= 0) {
        return takeFileError("take file settings tripletUnitTicks must be above 0");
    }
    if (take.settings.minRestTicks < 0) {
        return takeFileError("take file settings minRestTicks must not be negative");
    }
    if (take.settings.brushMs < 0.0) {
        return takeFileError("take file settings brushMs must not be negative");
    }

    const JsonArray events = root.value("events").toArray();
    for (size_t i = 0; i < events.size(); ++i) {
        const JsonArray fields = events.at(i).toArray();
        if (fields.size() != 4) {
            return takeFileError("take file event " + std::to_string(i) + " does not have 4 fields");
        }
        int64_t ns = 0;
        if (!takeFileReadNs(fields.at(0), ns)) {
            return takeFileError("take file event " + std::to_string(i) + " has a time that is not a whole number in a string");
        }
        take.events.push_back({ ns, fields.at(1).toBool(), fields.at(2).toInt(), fields.at(3).toInt() });
    }

    const JsonArray clock = root.value("clock").toArray();
    for (size_t i = 0; i < clock.size(); ++i) {
        const JsonArray fields = clock.at(i).toArray();
        if (fields.size() != 2) {
            return takeFileError("take file clock sample " + std::to_string(i) + " does not have 2 fields");
        }
        int64_t hostNs = 0;
        if (!takeFileReadNs(fields.at(0), hostNs)) {
            return takeFileError("take file clock sample " + std::to_string(i) + " has a time that is not a whole number in a string");
        }
        take.clock.push_back({ hostNs, fields.at(1).toDouble() });
    }

    const JsonArray measures = root.value("measures").toArray();
    for (size_t i = 0; i < measures.size(); ++i) {
        const JsonArray fields = measures.at(i).toArray();
        if (fields.size() != 4) {
            return takeFileError("take file measure " + std::to_string(i) + " does not have 4 fields");
        }
        const MeasureSpan measure { fields.at(0).toInt(), fields.at(1).toInt(), fields.at(2).toInt(), fields.at(3).toInt() };
        if (measure.ticks <= 0 || measure.sigN <= 0 || measure.sigD <= 0) {
            return takeFileError("take file measure " + std::to_string(i) + " has a length or time signature not above 0");
        }
        take.measures.push_back(measure);
    }

    const JsonArray timeMap = root.value("timeMap").toArray();
    for (size_t i = 0; i < timeMap.size(); ++i) {
        const JsonArray fields = timeMap.at(i).toArray();
        if (fields.size() != 2) {
            return takeFileError("take file time map entry " + std::to_string(i) + " does not have 2 fields");
        }
        take.timeMap.push_back({ fields.at(0).toDouble(), fields.at(1).toInt() });
    }

    RetVal<TakeFile> result;
    result.ret = make_ret(Ret::Code::Ok);
    result.val = take;
    return result;
}
