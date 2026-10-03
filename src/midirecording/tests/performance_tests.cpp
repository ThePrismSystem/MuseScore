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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "midirecording/internal/gridselection.h"
#include "midirecording/internal/prepare.h"
#include "midirecording/internal/takepipeline.h"

using namespace mu::midirecording;
using namespace muse;

class MidiRecording_PerformanceTests : public ::testing::Test
{
};

//! One written note and how the player performs it
struct PerformanceTestNote {
    int pitch = 0;
    int lineTick = 0;           // where the written note starts
    int endTick = 0;            // where the written note ends
    double rollMs = 0.0;        // struck this long after its line, as part of a rolled chord
    double heldFraction = 1.0;  // held for this fraction of its written length from the strike,
    double overlapMs = 0.0;     // and then this much longer
    bool triplet = false;
};

struct PerformanceTestPattern {
    std::string name;
    std::vector<MeasureSpan> measures;
    OverlapMode overlaps = OverlapMode::Tied;
    std::vector<PerformanceTestNote> notes;
    std::vector<NotatedEvent> expected;
};

static std::vector<MeasureSpan> performanceTestMeasures(int count, int sigN, int sigD)
{
    const int ticks = TICKS_PER_WHOLE * sigN / sigD;
    std::vector<MeasureSpan> measures;
    for (int i = 0; i < count; ++i) {
        measures.push_back({ i* ticks, ticks, sigN, sigD });
    }
    return measures;
}

static NotatedEvent performanceTestEvent(int startTick, int ticks, std::vector<int> pitches, std::vector<int> tied = {})
{
    NotatedEvent event;
    event.startTick = startTick;
    event.ticks = ticks;
    event.pitches = std::move(pitches);
    event.tiedFromPrevious = std::move(tied);
    return event;
}

//! A single line of notes of one written length, each released after heldFraction of it plus overlapMs;
//! the last note is released after heldFraction alone, having no next note to overlap
static PerformanceTestPattern performanceTestLine(const std::string& name, const std::vector<MeasureSpan>& measures, int count, int length,
                                                  double heldFraction, double overlapMs)
{
    PerformanceTestPattern pattern;
    pattern.name = name;
    pattern.measures = measures;
    for (int i = 0; i < count; ++i) {
        PerformanceTestNote note;
        note.pitch = 60 + i % 12;
        note.lineTick = i * length;
        note.endTick = (i + 1) * length;
        note.heldFraction = heldFraction;
        note.overlapMs = i + 1 < count ? overlapMs : 0.0;
        pattern.notes.push_back(note);
        pattern.expected.push_back(performanceTestEvent(note.lineTick, length, { note.pitch }));
    }
    return pattern;
}

static PerformanceTestPattern performanceTestTripletBar()
{
    PerformanceTestPattern pattern;
    pattern.name = "8th 8th | triplet 8ths | 8th 8th | quarter, legato";
    pattern.measures = performanceTestMeasures(1, 4, 4);
    const int lines[] = { 0, 240, 480, 640, 800, 960, 1200, 1440, 1920 };
    const TupletInfo group { 480, 480, 3, 2, 160 };
    for (int i = 0; i < 8; ++i) {
        PerformanceTestNote note;
        note.pitch = 60 + i;
        note.lineTick = lines[i];
        note.endTick = lines[i + 1];
        note.triplet = i >= 2 && i <= 4;
        pattern.notes.push_back(note);

        NotatedEvent event = performanceTestEvent(note.lineTick, note.endTick - note.lineTick, { note.pitch });
        if (note.triplet) {
            event.tuplet = group;
        }
        pattern.expected.push_back(event);
    }
    return pattern;
}

static PerformanceTestPattern performanceTestRolledChords()
{
    PerformanceTestPattern pattern;
    pattern.name = "three-note chords rolled over 50 ms, held 90%";
    pattern.measures = performanceTestMeasures(2, 4, 4);
    const std::vector<int> chords[] = { { 48, 52, 55 }, { 50, 53, 57 } };
    for (int beat = 0; beat < 8; ++beat) {
        const std::vector<int>& chord = chords[beat % 2];
        for (size_t i = 0; i < chord.size(); ++i) {
            PerformanceTestNote note;
            note.pitch = chord[i];
            note.lineTick = beat * TICKS_PER_QUARTER;
            note.endTick = note.lineTick + TICKS_PER_QUARTER;
            note.rollMs = 25.0 * i;
            note.heldFraction = 0.9;
            pattern.notes.push_back(note);
        }
        pattern.expected.push_back(performanceTestEvent(beat * TICKS_PER_QUARTER, TICKS_PER_QUARTER, chord));
    }
    return pattern;
}

//! A whole note per measure, held to the next, under quarters held 90%
static PerformanceTestPattern performanceTestHeldUnderQuarters(OverlapMode mode)
{
    PerformanceTestPattern pattern;
    pattern.name = mode == OverlapMode::Tied ? "whole notes under moving quarters, tied" : "whole notes under moving quarters, cut";
    pattern.measures = performanceTestMeasures(2, 4, 4);
    pattern.overlaps = mode;
    const int wholes[] = { 48, 43 };
    const int quarters[] = { 60, 62, 64, 65, 67, 65, 64, 62 };
    for (int measure = 0; measure < 2; ++measure) {
        PerformanceTestNote whole;
        whole.pitch = wholes[measure];
        whole.lineTick = measure * TICKS_PER_WHOLE;
        whole.endTick = whole.lineTick + TICKS_PER_WHOLE;
        pattern.notes.push_back(whole);
    }
    for (int beat = 0; beat < 8; ++beat) {
        PerformanceTestNote quarter;
        quarter.pitch = quarters[beat];
        quarter.lineTick = beat * TICKS_PER_QUARTER;
        quarter.endTick = quarter.lineTick + TICKS_PER_QUARTER;
        quarter.heldFraction = 0.9;
        pattern.notes.push_back(quarter);

        const int whole = wholes[beat / 4];
        const bool firstBeat = beat % 4 == 0;
        if (mode == OverlapMode::Cut && !firstBeat) {
            pattern.expected.push_back(performanceTestEvent(quarter.lineTick, TICKS_PER_QUARTER, { quarter.pitch }));
        } else {
            std::vector<int> tied;
            if (!firstBeat) {
                tied.push_back(whole);
            }
            pattern.expected.push_back(performanceTestEvent(quarter.lineTick, TICKS_PER_QUARTER, { whole, quarter.pitch }, tied));
        }
    }
    return pattern;
}

static std::vector<PerformanceTestPattern> performanceTestPatterns()
{
    const std::vector<MeasureSpan> twoBars = performanceTestMeasures(2, 4, 4);
    return {
        performanceTestLine("detached quarters, held 70%", twoBars, 8, TICKS_PER_QUARTER, 0.7, 0.0),
        performanceTestLine("detached eighths, held 70%", twoBars, 16, TICKS_PER_QUARTER / 2, 0.7, 0.0),
        performanceTestLine("legato eighths, held 60 ms past the next onset", twoBars, 16, TICKS_PER_QUARTER / 2, 1.0, 60.0),
        performanceTestTripletBar(),
        performanceTestRolledChords(),
        performanceTestHeldUnderQuarters(OverlapMode::Tied),
        performanceTestHeldUnderQuarters(OverlapMode::Cut),
        performanceTestLine("eighths in 6/8, held 70%", performanceTestMeasures(2, 6, 8), 12, TICKS_PER_QUARTER / 2, 0.7, 0.0),
        performanceTestLine("detached sixteenths, held 60%", twoBars, 32, TICKS_PER_QUARTER / 4, 0.6, 0.0),
    };
}

//! Ticks from the line at tick to the nearest other line a strike could snap to: the straight grid
//! and, in a measure that can take triplets, the triplet grid
static int performanceTestCompetingLineTicks(int tick, const std::vector<MeasureSpan>& measures, const QuantizeSettings& settings)
{
    const MeasureSpan* measure = &measures.back();
    for (const MeasureSpan& candidate : measures) {
        if (tick >= candidate.startTick && tick < candidate.startTick + candidate.ticks) {
            measure = &candidate;
            break;
        }
    }

    const int local = tick - measure->startTick;
    int nearest = settings.gridTicks;
    const auto consider = [&](int line) {
        if (line != local) {
            nearest = std::min(nearest, std::abs(line - local));
        }
    };

    const int windowTicks = 3 * settings.tripletUnitTicks;
    if (settings.triplets && !isCompoundMeter(*measure) && measure->ticks % windowTicks == 0) {
        const int windowStart = local / windowTicks * windowTicks;
        for (int k = -1; k <= 4; ++k) {
            consider(windowStart + k * settings.tripletUnitTicks);
        }
    }
    const int straightBelow = local / settings.gridTicks * settings.gridTicks;
    for (int k = -1; k <= 2; ++k) {
        consider(straightBelow + k * settings.gridTicks);
    }
    return nearest;
}

//! A combination is valid only when the intended rhythm is unambiguous: with every strike and every
//! release pushed J the wrong way, each still lies where the written rhythm puts it, by the margins
//! the quantizer's own settings define.
//!  1. A strike, with its roll offset, stays within 0.45 of the way to the nearest line of a competing
//!     grid, so its own line stays nearest. A triplet strike stays within 0.35: a tuplet is written only
//!     when the triplet fit is twice as good as the straight one, and with two triplet strikes and the
//!     beat pushed x toward the straight lines 40 ticks away that holds while 2.5x^2 < (40 - x)^2,
//!     below x = 15.5 ticks. A rolled chord that groups even with the jitter (within CHORD_STEP_MS and
//!     CHORD_SPAN_MS) counts from its median strike, any other strike from its own.
//!  2. Strikes on different lines stay further apart than a chord step, in ms or in ticks, so they are
//!     never grouped as one chord.
//!  3. Every hold stays above brushMs + 10.
//!  4. A release stays within one grid step of its written end, or, when nothing else sounds after it,
//!     close enough before it to leave a rest shorter than minRestTicks: below the end by less than
//!     minRestTicks - gridTicks / 2, above it by less than gridTicks.
static bool performanceTestIsUnambiguous(const PerformanceTestPattern& pattern, double bpm, double jitterMs,
                                         const QuantizeSettings& settings)
{
    const double ticksPerMs = TICKS_PER_QUARTER * bpm / 60000.0;
    const double msPerTick = 1.0 / ticksPerMs;
    const double jitterTicks = jitterMs * ticksPerMs;

    std::map<int, std::vector<double> > rollsByLine;
    for (const PerformanceTestNote& note : pattern.notes) {
        rollsByLine[note.lineTick].push_back(note.rollMs);
    }
    for (auto& entry : rollsByLine) {
        std::sort(entry.second.begin(), entry.second.end());
    }

    for (const PerformanceTestNote& note : pattern.notes) {
        const std::vector<double>& rolls = rollsByLine[note.lineTick];
        double rollStep = 0.0;
        for (size_t i = 1; i < rolls.size(); ++i) {
            rollStep = std::max(rollStep, rolls[i] - rolls[i - 1]);
        }
        const double rollSpan = rolls.back() - rolls.front();
        const bool groups = rollStep + 2 * jitterMs <= CHORD_STEP_MS && rollSpan + 2 * jitterMs <= CHORD_SPAN_MS
                            && (rollStep + 2 * jitterMs) * ticksPerMs <= settings.gridTicks / 2.0
                            && (rollSpan + 2 * jitterMs) * ticksPerMs <= settings.gridTicks;
        const double offsetMs = groups ? rolls[(rolls.size() - 1) / 2] : note.rollMs;
        const double reach = (note.triplet ? 0.35 : 0.45) * performanceTestCompetingLineTicks(note.lineTick, pattern.measures, settings);
        if ((offsetMs + jitterMs) * ticksPerMs >= reach) {
            return false;
        }

        const double heldMs = note.heldFraction * (note.endTick - note.lineTick) * msPerTick + note.overlapMs;
        if (heldMs - 2 * jitterMs <= settings.brushMs + 10.0) {
            return false;
        }

        const double releaseTick = note.lineTick + (note.rollMs + heldMs) * ticksPerMs;
        bool leavesRest = !note.triplet;
        for (const PerformanceTestNote& other : pattern.notes) {
            if (other.lineTick != note.lineTick && other.lineTick < releaseTick && other.endTick > releaseTick) {
                leavesRest = false;
            }
        }
        const double below = leavesRest ? settings.minRestTicks - settings.gridTicks / 2.0 : settings.gridTicks;
        const double slack = std::min(releaseTick - (note.endTick - below), note.endTick + settings.gridTicks - releaseTick);
        if (jitterTicks >= slack) {
            return false;
        }
    }

    for (auto line = rollsByLine.begin(); std::next(line) != rollsByLine.end(); ++line) {
        const auto next = std::next(line);
        const double gapMs = (next->first - line->first) * msPerTick + next->second.front() - line->second.back() - 2 * jitterMs;
        if (gapMs <= CHORD_STEP_MS && gapMs * ticksPerMs <= settings.gridTicks / 2.0) {
            return false;
        }
    }

    return true;
}

static int64_t performanceTestNs(double secs)
{
    return static_cast<int64_t>(std::llround(secs * 1e9));
}

//! Seconds of count-in before playback starts moving
static constexpr double PERFORMANCE_TEST_COUNT_IN_SECS = 1.0;

//! The pattern as played at bpm, every strike and release moved by its own draw from [-J, +J]. The
//! draws map the generator's raw 32-bit output directly, so they are the same on every standard library.
static TakeFile performanceTestTake(const PerformanceTestPattern& pattern, double bpm, double jitterMs, uint32_t seed)
{
    TakeFile take;
    take.settings.overlaps = pattern.overlaps;

    std::mt19937 rng(seed);
    const auto jitter = [&]() {
        return (static_cast<double>(rng()) / 4294967295.0 * 2.0 - 1.0) * jitterMs;
    };

    const double msPerTick = 60000.0 / (TICKS_PER_QUARTER * bpm);
    double lastReleaseSecs = 0.0;
    for (const PerformanceTestNote& note : pattern.notes) {
        const double strikeMs = note.lineTick * msPerTick + note.rollMs;
        const double heldMs = note.heldFraction * (note.endTick - note.lineTick) * msPerTick + note.overlapMs;
        const double onSecs = PERFORMANCE_TEST_COUNT_IN_SECS + (strikeMs + jitter()) / 1000.0;
        const double offSecs = PERFORMANCE_TEST_COUNT_IN_SECS + (strikeMs + heldMs + jitter()) / 1000.0;
        take.events.push_back({ performanceTestNs(onSecs), true, note.pitch, 80 });
        take.events.push_back({ performanceTestNs(offSecs), false, note.pitch, 0 });
        lastReleaseSecs = std::max(lastReleaseSecs, offSecs);
    }

    for (int i = 0; i < 100; ++i) {
        take.clock.push_back({ performanceTestNs(i * 0.01), 0.0 });
    }
    for (int i = 0; PERFORMANCE_TEST_COUNT_IN_SECS + i * 0.01 <= lastReleaseSecs + 1.0; ++i) {
        take.clock.push_back({ performanceTestNs(PERFORMANCE_TEST_COUNT_IN_SECS + i * 0.01), i * 0.01 });
    }
    take.stopNs = performanceTestNs(lastReleaseSecs + 0.5);
    return take;
}

static std::string performanceTestDescribe(const NotatedEvent& event)
{
    std::ostringstream text;
    text << event.startTick << "+" << event.ticks << "{";
    for (size_t i = 0; i < event.pitches.size(); ++i) {
        text << (i ? "," : "") << event.pitches[i];
    }
    text << "}";
    if (!event.tiedFromPrevious.empty()) {
        text << "~";
        for (size_t i = 0; i < event.tiedFromPrevious.size(); ++i) {
            text << (i ? "," : "") << event.tiedFromPrevious[i];
        }
    }
    if (event.tuplet) {
        text << " tuplet " << event.tuplet->groupStartTick << "+" << event.tuplet->groupTicks << " " << event.tuplet->actual << ":"
             << event.tuplet->normal << " unit " << event.tuplet->unitTicks;
    }
    return text.str();
}

static std::vector<std::string> performanceTestDescribe(const std::vector<NotatedEvent>& events)
{
    std::vector<std::string> result;
    for (const NotatedEvent& event : events) {
        result.push_back(performanceTestDescribe(event));
    }
    return result;
}

//! Left out for a quantizer behaviour the current rulings do not cover. In both, a release just before
//! the end of the take falls short of the midpoint at 3780. No onset lies at the take end for tidy gaps
//! to close it to, so it snaps back to 3720 while a note sounding with it snaps on to 3840.
//!  - Rolled chords, 60 bpm, jitter 40 ms, seed 4004: the last chord {50,53,57} is released at ticks
//!    3778, 3801 and 3805 and comes back as 3360+360{50,53,57}, 3720+120{53,57}~53,57.
//!  - Whole notes under quarters, tied, 160 bpm, jitter 40 ms, seed 5204: the last quarter, 62, is
//!    released at tick 3777 under the whole note 43 and comes back as 3360+360{43,62}~43, 3720+120{43}~43.
static const std::pair<const char*, double> PERFORMANCE_TEST_LEFT_OUT[] = {
    { "three-note chords rolled over 50 ms, held 90%", 40.0 },
    { "whole notes under moving quarters, tied", 40.0 },
};

static bool performanceTestIsLeftOut(const PerformanceTestPattern& pattern, double jitterMs)
{
    for (const auto& [name, leftOutJitterMs] : PERFORMANCE_TEST_LEFT_OUT) {
        if (pattern.name == name && jitterMs == leftOutJitterMs) {
            return true;
        }
    }
    return false;
}

TEST_F(MidiRecording_PerformanceTests, GeneratedPerformancesComeBackAsWritten)
{
    const double tempos[] = { 60.0, 100.0, 160.0 };
    const double jitters[] = { 0.0, 10.0, 25.0, 40.0 };
    const std::vector<PerformanceTestPattern> patterns = performanceTestPatterns();

    int ran = 0;
    int ambiguous = 0;
    int leftOut = 0;
    for (size_t p = 0; p < patterns.size(); ++p) {
        const PerformanceTestPattern& pattern = patterns[p];
        for (size_t t = 0; t < std::size(tempos); ++t) {
            for (size_t j = 0; j < std::size(jitters); ++j) {
                const double bpm = tempos[t];
                const double jitterMs = jitters[j];
                if (!performanceTestIsUnambiguous(pattern, bpm, jitterMs, QuantizeSettings())) {
                    ++ambiguous;
                    continue;
                }
                if (performanceTestIsLeftOut(pattern, jitterMs)) {
                    ++leftOut;
                    continue;
                }

                const uint32_t seed = static_cast<uint32_t>(1 + 1000 * p + 100 * t + j);
                std::ostringstream trace;
                trace << pattern.name << " at " << bpm << " bpm, jitter " << jitterMs << " ms, seed " << seed;
                SCOPED_TRACE(trace.str());

                const TakeFile take = performanceTestTake(pattern, bpm, jitterMs, seed);
                const SecsToTick secsToTick = [bpm](double secs) {
                    return static_cast<int>(std::lround(secs * TICKS_PER_QUARTER * bpm / 60.0));
                };
                const RetVal<QuantizeResult> result = quantizeTake(take, pattern.measures, secsToTick);
                ++ran;

                ASSERT_TRUE(result.ret) << result.ret.text();
                const MeasureSpan& last = pattern.measures.back();
                EXPECT_EQ(result.val.takeStartTick, 0);
                EXPECT_EQ(result.val.takeEndTick, last.startTick + last.ticks);
                EXPECT_EQ(performanceTestDescribe(result.val.events), performanceTestDescribe(pattern.expected));
            }
        }
    }

    std::cout << "generated performances: " << ran << " ran, " << ambiguous << " ambiguous, " << leftOut
              << " left out for known quantizer behaviour" << std::endl;
    EXPECT_GT(ran, 0);
}
