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

#include "midirecordingcontroller.h"

#include <algorithm>
#include <vector>

#include "engraving/dom/chord.h"
#include "engraving/dom/chordrest.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/tuplet.h"
#include "io/path.h"
#include "midi/midiclock.h"
#include "translation.h"

#include "midirecording/internal/notatedtext.h"
#include "midirecording/internal/rawevents.h"
#include "midirecording/internal/takepipeline.h"
#include "midirecording/internal/takesetup.h"
#include "midirecording/internal/timemap.h"

#include "log.h"

using namespace muse;
using namespace muse::actions;
using namespace mu::notation;
using namespace mu::midirecording;

static const ActionCode RECORD_MIDI_CODE("record-midi");
static const ActionCode EXPORT_TAKE_CODE("midi-recording-export-take");
static const ActionCode REPLAY_TAKE_CODE("midi-recording-replay-take");
static const ActionCode METRONOME_CODE("metronome");
static const ActionCode REPEAT_CODE("repeat");
static const ActionCode MIDI_ON_CODE("midi-on");
static const ActionCode CANCEL_CODE("action://cancel");
static const ActionCode NOTATION_CANCEL_CODE("action://notation/cancel");

//! A knot of the take's time map on every 16th
static constexpr int MIDIRECORDINGCONTROLLER_TIME_MAP_STEP_TICKS = 120;
static constexpr double MIDIRECORDINGCONTROLLER_PERCENT = 100.0;

//! The chord or rest a take starts from: the selected note's chord, the
//! selected chord or rest, or for a range the chord or rest at its start on
//! the first voice of its top staff
static mu::engraving::ChordRest* midiRecordingControllerSelectedChordRest(const INotationPtr& notation,
                                                                          const mu::engraving::Score* score)
{
    const INotationSelectionPtr selection = notation->interaction()->selection();
    if (selection->isRange()) {
        const mu::engraving::staff_idx_t staffIdx = selection->range()->startStaffIndex();
        return score->findCR(selection->range()->startTick(), staffIdx * mu::engraving::VOICES);
    }

    mu::engraving::EngravingItem* item = selection->element();
    if (!item) {
        return nullptr;
    }
    if (item->isNote()) {
        return mu::engraving::toNote(item)->chord();
    }
    if (item->isChordRest()) {
        return mu::engraving::toChordRest(item);
    }
    return nullptr;
}

//! The measures from the one holding fromTick to the end of the score
static std::vector<MeasureSpan> midiRecordingControllerMeasures(const mu::engraving::Score* score, int fromTick)
{
    std::vector<MeasureSpan> measures;
    for (const mu::engraving::Measure* measure = score->tick2measure(mu::engraving::Fraction::fromTicks(fromTick)); measure;
         measure = measure->nextMeasure()) {
        const mu::engraving::Fraction timesig = measure->timesig();
        measures.push_back({ measure->tick().ticks(), measure->ticks().ticks(), timesig.numerator(), timesig.denominator() });
    }
    return measures;
}

MidiRecordingController::MidiRecordingController(const muse::modularity::ContextPtr& iocCtx)
    : muse::Contextable(iocCtx)
{
}

void MidiRecordingController::init()
{
    dispatcher()->reg(this, RECORD_MIDI_CODE, this, &MidiRecordingController::toggleRecord);
    dispatcher()->reg(this, EXPORT_TAKE_CODE, this, &MidiRecordingController::exportTake);
    dispatcher()->reg(this, REPLAY_TAKE_CODE, this, &MidiRecordingController::replayTake);

    midiInPort()->timestampedEventReceived().onReceive(this, [this](int64_t ns, const muse::midi::Event& event) {
        if (!m_recorder.isActive()) {
            return;
        }
        for (const RawEvent& raw : rawEventsFromMidi(ns, event)) {
            m_recorder.addEvent(raw);
        }
    });

    midiInPort()->deviceChanged().onNotify(this, [this]() {
        onDeviceChanged();
    });
    midiInPort()->availableDevicesChanged().onNotify(this, [this]() {
        onDeviceChanged();
    });

    playbackController()->currentPlaybackPositionChanged().onReceive(this, [this](muse::audio::secs_t secs, muse::midi::tick_t) {
        onPositionChanged(secs.raw());
    });

    playbackController()->isPlayingChanged().onNotify(this, [this]() {
        onPlayingChanged();
    });

    dispatcher()->preDispatch().onReceive(this, [this](const ActionCode& code) {
        if (m_recorder.isActive() && (code == CANCEL_CODE || code == NOTATION_CANCEL_CODE)) {
            cancelTake();
        }
    });

    globalContext()->currentNotationChanged().onNotify(this, [this]() {
        onNotationChanged();
    });
}

bool MidiRecordingController::isRecording() const
{
    return m_recorder.isActive();
}

muse::async::Notification MidiRecordingController::isRecordingChanged() const
{
    return m_isRecordingChanged;
}

void MidiRecordingController::toggleRecord()
{
    if (m_recorder.isActive()) {
        requestStop();
    } else {
        startTake();
    }
}

void MidiRecordingController::refuse(const std::string& reason)
{
    interactive()->warning(muse::trc("midirecording", "Cannot record MIDI"), reason);
}

void MidiRecordingController::startTake()
{
    const IMasterNotationPtr masterNotation = globalContext()->currentMasterNotation();
    const INotationPtr notation = globalContext()->currentNotation();
    if (!masterNotation || !notation) {
        return;
    }

    if (!midiInPort()->isConnected()) {
        refuse(muse::trc("midirecording", "Connect a MIDI keyboard to record."));
        return;
    }
    if (playbackController()->isPlaying() || m_restorePending) {
        refuse(muse::trc("midirecording", "Stop playback before recording."));
        return;
    }
    if (!playbackController()->isPlayAllowed()) {
        refuse(muse::trc("midirecording", "Playback is not ready yet. Try again in a moment."));
        return;
    }
    if (notation != masterNotation->notation()) {
        refuse(muse::trc("midirecording", "Record in the full score, not in a part."));
        return;
    }
    if (masterNotation->playback()->isLoopEnabled()) {
        refuse(muse::trc("midirecording", "Turn off the playback loop to record."));
        return;
    }

    const mu::engraving::Score* score = masterNotation->masterScore();
    mu::engraving::ChordRest* chordRest = midiRecordingControllerSelectedChordRest(notation, score);
    if (!chordRest) {
        refuse(muse::trc("midirecording", "Select a note or rest to start recording from."));
        return;
    }

    const QuantizeSettings settings = configuration()->quantizeSettings();
    const mu::engraving::Tuplet* tuplet = chordRest->topTuplet();
    const int chordRestTick = tuplet ? tuplet->tick().ticks() : chordRest->tick().ticks();
    const int measureStartTick = score->tick2measure(mu::engraving::Fraction::fromTicks(chordRestTick))->tick().ticks();

    TakeFile context;
    context.startTick = takeStartTick(chordRestTick, measureStartTick, settings.gridTicks);
    context.staffIdx = static_cast<int>(chordRest->staffIdx());
    context.voice = static_cast<int>(chordRest->voice());
    context.replaceMode = configuration()->replaceMode();
    context.countInBars = configuration()->countInBars();
    context.recordSpeedPercent = configuration()->recordSpeedPercent();
    context.latencyMs = configuration()->latencyMs();
    context.settings = settings;

    if (notation->interaction()->selection()->isRange()) {
        // Range playback mutes every part outside the range, so select the start element instead
        mu::engraving::EngravingItem* startItem = chordRest;
        if (chordRest->isChord()) {
            startItem = mu::engraving::toChord(chordRest)->upNote();
        }
        notation->interaction()->select({ startItem }, mu::engraving::SelectType::SINGLE);
    }

    const INotationNoteInputPtr noteInput = notation->interaction()->noteInput();
    if (noteInput->isNoteInputMode()) {
        noteInput->endNoteInput();
    }

    m_takeMasterNotation = masterNotation;
    m_takeDeviceId = midiInPort()->deviceID();
    m_sawPlaying = false;
    m_stopNs.reset();

    notation->midiInput()->setPreviewOnly(true);

    m_metronomeForced = !playbackController()->actionChecked(METRONOME_CODE);
    if (m_metronomeForced) {
        dispatcher()->dispatch(METRONOME_CODE);
    }
    m_repeatsForced = playbackController()->actionChecked(REPEAT_CODE);
    if (m_repeatsForced) {
        dispatcher()->dispatch(REPEAT_CODE);
    }
    m_midiInputForced = !playbackController()->actionChecked(MIDI_ON_CODE);
    if (m_midiInputForced) {
        dispatcher()->dispatch(MIDI_ON_CODE);
    }

    m_savedTempoMultiplier = playbackController()->tempoMultiplier();
    playbackController()->setTempoMultiplier(context.recordSpeedPercent / MIDIRECORDINGCONTROLLER_PERCENT);

    masterNotation->playback()->setExcludedTracks(takeExcludedTracks(chordRest->staffIdx(), context.voice, context.replaceMode,
                                                                     configuration()->playOtherStaves(), score->nstaves()));

    m_recorder.start(context);
    m_isRecordingChanged.notify();

    playbackController()->playFromTick(static_cast<muse::midi::tick_t>(context.startTick), context.countInBars);
}

void MidiRecordingController::requestStop()
{
    if (!m_stopNs) {
        m_stopNs = muse::midi::midiClockNowNs();
    }

    if (playbackController()->isPlaying()) {
        playbackController()->reset();
    } else {
        finishTake();
    }
}

void MidiRecordingController::onPositionChanged(double secs)
{
    if (!m_recorder.isActive() || m_stopNs) {
        return;
    }

    const TakeRecorder::SampleResult result = m_recorder.addSample({ muse::midi::midiClockNowNs(), secs });
    if (result == TakeRecorder::SampleResult::PlaybackMoved) {
        LOGI() << "playback moved during the take; the take ends where it moved";
        requestStop();
    }
}

void MidiRecordingController::onPlayingChanged()
{
    if (playbackController()->isPlaying()) {
        m_sawPlaying = m_recorder.isActive();
        return;
    }

    if (m_recorder.isActive()) {
        if (m_sawPlaying) {
            finishTake();
        }
        return;
    }

    if (m_restorePending) {
        restorePlayback();
    }
}

void MidiRecordingController::onDeviceChanged()
{
    if (!m_recorder.isActive()) {
        return;
    }

    const muse::midi::MidiDeviceList devices = midiInPort()->availableDevices();
    const bool listed = std::any_of(devices.cbegin(), devices.cend(), [this](const muse::midi::MidiDevice& device) {
        return device.id == m_takeDeviceId;
    });

    if (!listed || !midiInPort()->isConnected() || midiInPort()->deviceID() != m_takeDeviceId) {
        LOGW() << "the MIDI input device went away during the take; the take ends here";
        requestStop();
    }
}

void MidiRecordingController::onNotationChanged()
{
    if (!m_takeMasterNotation || globalContext()->currentNotation() == m_takeMasterNotation->notation()) {
        return;
    }

    if (globalContext()->currentMasterNotation() == m_takeMasterNotation) {
        // Another part of the same score: cancel, and put playback back once it has stopped
        if (m_recorder.isActive()) {
            cancelTake();
        }
        return;
    }

    dropTake();
    forgetClosedScore();
}

void MidiRecordingController::finishTake()
{
    const int64_t stopNs = m_stopNs ? *m_stopNs : muse::midi::midiClockNowNs();
    m_stopNs.reset();

    std::optional<TakeFile> take = m_recorder.stop(stopNs);
    if (take) {
        // While repeats are still off and the record speed still set
        take->measures = midiRecordingControllerMeasures(m_takeMasterNotation->masterScore(), take->startTick);
        if (!take->measures.empty()) {
            const INotationPlaybackPtr playback = m_takeMasterNotation->playback();
            const MeasureSpan& last = take->measures.back();
            take->timeMap = buildTimeMap(take->measures.front().startTick, last.startTick + last.ticks,
                                         MIDIRECORDINGCONTROLLER_TIME_MAP_STEP_TICKS, [playback](int tick) {
                return playback->playedTickToSec(static_cast<muse::midi::tick_t>(tick)).raw();
            });
        }
    }

    restorePlayback();
    m_isRecordingChanged.notify();

    if (take) {
        reportTake(*take);
    }
}

void MidiRecordingController::dropTake()
{
    if (!m_recorder.isActive()) {
        return;
    }

    m_recorder.cancel();
    m_stopNs.reset();
    m_isRecordingChanged.notify();
}

void MidiRecordingController::cancelTake()
{
    dropTake();

    if (playbackController()->isPlaying()) {
        m_restorePending = true;
        playbackController()->reset();
        return;
    }

    restorePlayback();
}

void MidiRecordingController::restorePlayback()
{
    if (!m_takeMasterNotation) {
        return;
    }

    m_takeMasterNotation->notation()->midiInput()->setPreviewOnly(false);
    m_takeMasterNotation->playback()->setExcludedTracks({});
    playbackController()->setTempoMultiplier(m_savedTempoMultiplier);

    if (m_metronomeForced) {
        dispatcher()->dispatch(METRONOME_CODE);
    }
    if (m_repeatsForced) {
        dispatcher()->dispatch(REPEAT_CODE);
    }
    if (m_midiInputForced) {
        dispatcher()->dispatch(MIDI_ON_CODE);
    }

    forgetTakeState();
}

//! The score the take ran in has closed. Its playback is gone with it, so
//! only the settings every score shares are put back.
void MidiRecordingController::forgetClosedScore()
{
    if (m_metronomeForced) {
        notationConfiguration()->setIsMetronomeEnabled(false);
    }
    if (m_repeatsForced) {
        notationConfiguration()->setIsPlayRepeatsEnabled(true);
    }
    if (m_midiInputForced) {
        notationConfiguration()->setIsMidiInputEnabled(false);
    }

    forgetTakeState();
}

void MidiRecordingController::forgetTakeState()
{
    m_restorePending = false;
    m_metronomeForced = false;
    m_repeatsForced = false;
    m_midiInputForced = false;
    m_takeMasterNotation.reset();
}

void MidiRecordingController::reportTake(const TakeFile& take)
{
    const bool hasNotes = std::any_of(take.events.cbegin(), take.events.cend(), [](const RawEvent& event) {
        return event.on;
    });
    if (!hasNotes) {
        interactive()->info(muse::trc("midirecording", "No notes were recorded"), std::string());
        return;
    }

    m_lastTake = take;

    const RetVal<QuantizeResult> result = quantizeTake(take);
    if (!result.ret) {
        LOGE() << "MIDI take could not be quantized: " << result.ret.text();
        interactive()->warning(muse::trc("midirecording", "The take could not be read"), result.ret.text());
        return;
    }

    LOGI() << "MIDI take:\n" << notatedEventsText(result.val);
    interactive()->info(muse::trc("midirecording", "Take recorded"),
                        muse::qtrc("midirecording", "Notes recorded: %1. Writing them into the score comes in a later build; "
                                                    "export the take from Diagnostics > MIDI recording.")
                        .arg(struckNoteCount(result.val)).toStdString());
}

void MidiRecordingController::exportTake()
{
    if (!m_lastTake) {
        interactive()->info(muse::trc("midirecording", "There is no take to export yet"), std::string());
        return;
    }

    const io::path_t path = interactive()->selectSavingFileSync(muse::trc("midirecording", "Export MIDI take"),
                                                                globalConfiguration()->homePath() + "/midi-take.json",
                                                                { muse::trc("midirecording", "MIDI take") + " (*.json)" });
    if (path.empty()) {
        return;
    }

    const Ret ret = fileSystem()->writeFile(path, takeToJson(*m_lastTake));
    if (!ret) {
        interactive()->warning(muse::trc("midirecording", "The take could not be saved"), ret.text());
    }
}

void MidiRecordingController::replayTake()
{
    const io::path_t path = interactive()->selectOpeningFileSync(muse::trc("midirecording", "Replay MIDI take"),
                                                                 globalConfiguration()->homePath(),
                                                                 { muse::trc("midirecording", "MIDI take") + " (*.json)" });
    if (path.empty()) {
        return;
    }

    const RetVal<ByteArray> data = fileSystem()->readFile(path);
    if (!data.ret) {
        interactive()->warning(muse::trc("midirecording", "The take file could not be read"), data.ret.text());
        return;
    }

    const RetVal<TakeFile> take = takeFromJson(data.val);
    if (!take.ret) {
        interactive()->warning(muse::trc("midirecording", "The take file could not be read"), take.ret.text());
        return;
    }

    const RetVal<QuantizeResult> result = quantizeTake(take.val);
    if (!result.ret) {
        interactive()->warning(muse::trc("midirecording", "The take could not be read"), result.ret.text());
        return;
    }

    const io::path_t textPath = io::dirpath(path) + "/" + io::completeBasename(path) + ".notated.txt";
    const std::string text = notatedEventsText(result.val);
    const Ret ret = fileSystem()->writeFile(textPath, ByteArray(text.c_str()));
    if (!ret) {
        interactive()->warning(muse::trc("midirecording", "The notated text could not be saved"), ret.text());
        return;
    }

    interactive()->info(muse::trc("midirecording", "Take replayed"), textPath.toString().toStdString());
}
