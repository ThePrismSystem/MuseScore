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
#include "engraving/dom/staff.h"
#include "io/path.h"
#include "midi/midiclock.h"
#include "translation.h"

#include "midirecording/internal/notatedtext.h"
#include "midirecording/internal/rawevents.h"
#include "midirecording/internal/takepipeline.h"
#include "midirecording/internal/takesetup.h"
#include "midirecording/internal/takewriter.h"
#include "midirecording/internal/timemap.h"

#include "log.h"

using namespace muse;
using namespace muse::actions;
using namespace mu::notation;
using namespace mu::midirecording;

static const ActionCode RECORD_MIDI_CODE("record-midi");
static const ActionCode EXPORT_TAKE_CODE("midi-recording-export-take");
static const ActionCode REPLAY_TAKE_CODE("midi-recording-replay-take");
static const ActionCode REAPPLY_TAKE_CODE("midi-recording-reapply-take");
static const ActionCode METRONOME_CODE("metronome");
static const ActionCode REPEAT_CODE("repeat");
static const ActionCode MIDI_ON_CODE("midi-on");
static const ActionCode CANCEL_CODE("action://cancel");
static const ActionCode NOTATION_CANCEL_CODE("action://notation/cancel");

//! A knot of the take's time map on every 16th
static constexpr int MIDIRECORDINGCONTROLLER_TIME_MAP_STEP_TICKS = 120;
static constexpr double MIDIRECORDINGCONTROLLER_PERCENT = 100.0;
static const muse::TranslatableString MIDIRECORDINGCONTROLLER_UNDO_NAME("undoableAction", "Record MIDI");

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

//! The staff and the staves of the same score linked to it, such as a TAB staff
static std::vector<size_t> midiRecordingControllerLinkedStaves(const mu::engraving::Staff* staff)
{
    std::vector<size_t> staves;
    for (const mu::engraving::Staff* linked : staff->staffList()) {
        if (linked->score() == staff->score()) {
            staves.push_back(linked->idx());
        }
    }
    return staves;
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

//! Where a take is written, from the take's own context
static TakeTarget midiRecordingControllerTarget(const TakeFile& take, bool useWrittenPitch)
{
    TakeTarget target;
    target.staffIdx = static_cast<mu::engraving::staff_idx_t>(take.staffIdx);
    target.voice = static_cast<mu::engraving::voice_idx_t>(take.voice);
    target.replaceVoice = take.replaceMode == "voice";
    target.useWrittenPitch = useWrittenPitch;
    return target;
}

//! Selects what the take wrote, from its first chord or rest to its last
static void midiRecordingControllerSelectWritten(const INotationPtr& notation, const mu::engraving::Score* score,
                                                 const QuantizeResult& result, const TakeTarget& target)
{
    mu::engraving::ChordRest* first = score->findCR(mu::engraving::Fraction::fromTicks(result.takeStartTick), target.track());
    mu::engraving::ChordRest* last = score->findCR(mu::engraving::Fraction::fromTicks(result.takeEndTick - 1), target.track());
    if (!first || !last) {
        return;
    }

    // A chord is selected through a note of it
    auto selectable = [](mu::engraving::ChordRest* chordRest) -> mu::engraving::EngravingItem* {
        if (chordRest->isChord()) {
            return mu::engraving::toChord(chordRest)->upNote();
        }
        return chordRest;
    };
    notation->interaction()->select({ selectable(first) }, mu::engraving::SelectType::SINGLE);
    notation->interaction()->select({ selectable(last) }, mu::engraving::SelectType::RANGE);
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
    dispatcher()->reg(this, REAPPLY_TAKE_CODE, this, &MidiRecordingController::reapplyTake);

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
    const bool rangeSelected = notation->interaction()->selection()->isRange();

    TakeFile context;
    context.staffIdx = static_cast<int>(chordRest->staffIdx());
    context.voice = static_cast<int>(chordRest->voice());
    context.replaceMode = configuration()->replaceMode();

    // A range starts where it starts, not at the voice-1 chord sounding there, so nothing before it is replaced
    const int fromTick = rangeSelected ? notation->interaction()->selection()->range()->startTick().ticks() : chordRest->tick().ticks();
    context.startTick = takeStartTickInScore(score, midiRecordingControllerTarget(context, false).track(), fromTick, settings.gridTicks);
    context.countInBars = configuration()->countInBars();
    context.recordSpeedPercent = configuration()->recordSpeedPercent();
    context.latencyMs = configuration()->latencyMs();
    context.settings = settings;

    if (rangeSelected) {
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
    m_takeFromTick = fromTick;
    m_writtenTake.reset();

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
    m_playNotesWhenEditingForced = !playbackConfiguration()->playNotesWhenEditing();
    if (m_playNotesWhenEditingForced) {
        playbackConfiguration()->setPlayNotesWhenEditing(true);
    }
    m_playNotesOnMidiInputForced = !playbackConfiguration()->playNotesOnMidiInput();
    if (m_playNotesOnMidiInputForced) {
        playbackConfiguration()->setPlayNotesOnMidiInput(true);
    }

    m_savedTempoMultiplier = playbackController()->tempoMultiplier();
    playbackController()->setTempoMultiplier(context.recordSpeedPercent / MIDIRECORDINGCONTROLLER_PERCENT);

    masterNotation->playback()->setExcludedTracks(takeExcludedTracks(midiRecordingControllerLinkedStaves(chordRest->staff()),
                                                                     context.voice, context.replaceMode,
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
    if (m_writtenTake && globalContext()->currentMasterNotation() != m_writtenTake->masterNotation) {
        m_writtenTake.reset();
    }

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

    // restorePlayback forgets the take's score, and the take is written into it afterwards
    const IMasterNotationPtr masterNotation = m_takeMasterNotation;

    std::optional<TakeFile> take = m_recorder.stop(stopNs);
    if (take) {
        // While repeats are still off and the record speed still set
        take->measures = midiRecordingControllerMeasures(masterNotation->masterScore(), take->startTick);
        if (!take->measures.empty()) {
            const INotationPlaybackPtr playback = masterNotation->playback();
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
        reportTake(masterNotation, *take, m_takeFromTick);
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

    //! A toggle the user flipped during the take is left as they set it
    if (m_metronomeForced && playbackController()->actionChecked(METRONOME_CODE)) {
        dispatcher()->dispatch(METRONOME_CODE);
    }
    if (m_repeatsForced && !playbackController()->actionChecked(REPEAT_CODE)) {
        dispatcher()->dispatch(REPEAT_CODE);
    }
    if (m_midiInputForced && playbackController()->actionChecked(MIDI_ON_CODE)) {
        dispatcher()->dispatch(MIDI_ON_CODE);
    }
    if (m_playNotesWhenEditingForced) {
        playbackConfiguration()->setPlayNotesWhenEditing(false);
    }
    if (m_playNotesOnMidiInputForced) {
        playbackConfiguration()->setPlayNotesOnMidiInput(false);
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
    if (m_playNotesWhenEditingForced) {
        playbackConfiguration()->setPlayNotesWhenEditing(false);
    }
    if (m_playNotesOnMidiInputForced) {
        playbackConfiguration()->setPlayNotesOnMidiInput(false);
    }

    forgetTakeState();
}

void MidiRecordingController::forgetTakeState()
{
    m_restorePending = false;
    m_metronomeForced = false;
    m_repeatsForced = false;
    m_midiInputForced = false;
    m_playNotesWhenEditingForced = false;
    m_playNotesOnMidiInputForced = false;
    m_takeMasterNotation.reset();
}

void MidiRecordingController::reportTake(const IMasterNotationPtr& masterNotation, const TakeFile& take, int fromTick)
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
    writeTakeIntoScore(masterNotation, take, fromTick, result.val);
}

void MidiRecordingController::writeTakeIntoScore(const IMasterNotationPtr& masterNotation, const TakeFile& take, int fromTick,
                                                 const QuantizeResult& result)
{
    const INotationPtr notation = masterNotation->notation();
    const INotationUndoStackPtr undoStack = notation->undoStack();
    const TakeTarget target = midiRecordingControllerTarget(take, notationConfiguration()->midiUseWrittenPitch().val);

    undoStack->prepareChanges(MIDIRECORDINGCONTROLLER_UNDO_NAME);
    const Ret ret = writeTake(masterNotation->masterScore(), result, target);
    if (!ret) {
        undoStack->rollbackChanges();
        m_writtenTake.reset();
        LOGE() << "MIDI take could not be written: " << ret.text();
        interactive()->warning(muse::trc("midirecording", "The take could not be written"), ret.text());
        return;
    }
    undoStack->commitChanges();
    notation->notationChanged().notify();

    midiRecordingControllerSelectWritten(notation, masterNotation->masterScore(), result, target);
    m_writtenTake = WrittenTake { masterNotation, take, fromTick, undoStack->currentStateIndex() };
}

void MidiRecordingController::reapplyTake()
{
    if (m_recorder.isActive()) {
        return;
    }

    const IMasterNotationPtr masterNotation = globalContext()->currentMasterNotation();
    const INotationPtr notation = globalContext()->currentNotation();
    if (m_writtenTake && masterNotation == m_writtenTake->masterNotation && notation != masterNotation->notation()) {
        interactive()->info(muse::trc("midirecording", "Re-apply works in the full score"), std::string());
        return;
    }
    const bool lastChange = m_writtenTake && masterNotation == m_writtenTake->masterNotation && notation == masterNotation->notation()
                            && notation->undoStack()->currentStateIndex() == m_writtenTake->undoStateIndex
                            && notation->undoStack()->topMostUndoActionName() == MIDIRECORDINGCONTROLLER_UNDO_NAME;
    if (!lastChange) {
        m_writtenTake.reset();
        interactive()->info(muse::trc("midirecording", "There is no take to re-apply"),
                            muse::trc("midirecording",
                                      "Re-apply works on the last take, in the full score, until the score is changed again."));
        return;
    }
    const mu::engraving::Score* score = masterNotation->masterScore();

    TakeFile take = m_writtenTake->take;
    const int fromTick = m_writtenTake->fromTick;
    take.settings = configuration()->quantizeSettings();
    take.replaceMode = configuration()->replaceMode();
    take.latencyMs = configuration()->latencyMs();

    const INotationNoteInputPtr noteInput = notation->interaction()->noteInput();
    if (noteInput->isNoteInputMode()) {
        noteInput->endNoteInput();
    }

    // The start is worked out again on the score as it was before the take, with the grid as it is now
    notation->interaction()->undo();
    if (notation->undoStack()->currentStateIndex() >= m_writtenTake->undoStateIndex) {
        interactive()->warning(muse::trc("midirecording", "The take could not be re-applied"),
                               muse::trc("midirecording", "Finish the current edit and try again."));
        return;
    }
    take.startTick = takeStartTickInScore(score, midiRecordingControllerTarget(take, false).track(), fromTick, take.settings.gridTicks);

    const RetVal<QuantizeResult> result = quantizeTake(take);
    if (!result.ret) {
        m_writtenTake.reset();
        LOGE() << "MIDI take could not be quantized: " << result.ret.text();
        interactive()->warning(muse::trc("midirecording", "The take could not be read"), result.ret.text());
        return;
    }

    writeTakeIntoScore(masterNotation, take, fromTick, result.val);
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
