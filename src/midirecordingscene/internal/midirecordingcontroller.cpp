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
#include <cmath>
#include <set>
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

#include "midirecording/internal/calibration.h"
#include "midirecording/internal/notatedtext.h"
#include "midirecording/internal/rawevents.h"
#include "midirecording/internal/settingchoices.h"
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
static const ActionCode CALIBRATE_CODE("midi-recording-calibrate");
static const ActionCode LATENCY_RESET_CODE("midi-recording-latency-reset");
static const ActionCode METRONOME_CODE("metronome");
static const ActionCode REPEAT_CODE("repeat");
static const ActionCode MIDI_ON_CODE("midi-on");
static const ActionCode CANCEL_CODE("action://cancel");
static const ActionCode NOTATION_CANCEL_CODE("action://notation/cancel");

//! A knot of the take's time map on every 16th
static constexpr int MIDIRECORDINGCONTROLLER_TIME_MAP_STEP_TICKS = 120;
static constexpr double MIDIRECORDINGCONTROLLER_PERCENT = 100.0;
static constexpr int MIDIRECORDINGCONTROLLER_CALIBRATION_SPEED_PERCENT = 100;
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

//! Range playback mutes every part outside the range, so a take starts from
//! the range's first chord or rest instead
static void midiRecordingControllerLeaveRange(const INotationPtr& notation, mu::engraving::ChordRest* chordRest)
{
    mu::engraving::EngravingItem* startItem = chordRest;
    if (chordRest->isChord()) {
        startItem = mu::engraving::toChord(chordRest)->upNote();
    }
    notation->interaction()->select({ startItem }, mu::engraving::SelectType::SINGLE);
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
    dispatcher()->reg(this, CALIBRATE_CODE, this, &MidiRecordingController::startCalibration);

    for (const SettingMenu& menu : settingMenus()) {
        for (const SettingChoice& choice : menu.choices) {
            dispatcher()->reg(this, choice.code, [this, code = choice.code]() {
                applySetting(code);
            });
        }
    }
    dispatcher()->reg(this, LATENCY_RESET_CODE, [this]() {
        configuration()->setLatencyMs(0.0);
    });

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

    playbackController()->currentPlaybackPositionChanged().onReceive(this, [this](muse::audio::secs_t secs, muse::midi::tick_t tick) {
        onPositionChanged(secs.raw(), static_cast<int>(tick));
    });

    playbackController()->isPlayingChanged().onNotify(this, [this]() {
        onPlayingChanged();
        m_canToggleRecordChanged.notify();
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

bool MidiRecordingController::canToggleRecord() const
{
    return m_recorder.isActive() || !(playbackController()->isPlaying() || m_restorePending);
}

muse::async::Notification MidiRecordingController::canToggleRecordChanged() const
{
    return m_canToggleRecordChanged;
}

void MidiRecordingController::applySetting(const ActionCode& code)
{
    const SettingChoice* choice = findSettingChoice(code);
    if (!choice) {
        return;
    }

    // A take that is running keeps the settings it started with
    RecordingSettings recording = configuration()->recordingSettings();
    choice->choose(recording);
    configuration()->setRecordingSettings(recording);
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

//! The checks every take makes before it starts; says why and returns false when one fails
bool MidiRecordingController::canStartTake(const IMasterNotationPtr& masterNotation, const INotationPtr& notation)
{
    if (!masterNotation || !notation) {
        return false;
    }

    if (!midiInPort()->isConnected()) {
        refuse(muse::trc("midirecording", "Connect a MIDI keyboard to record."));
        return false;
    }
    if (playbackController()->isPlaying() || m_restorePending) {
        refuse(muse::trc("midirecording", "Stop playback before recording."));
        return false;
    }
    if (!playbackController()->isPlayAllowed()) {
        refuse(muse::trc("midirecording", "Playback is not ready yet. Try again in a moment."));
        return false;
    }
    if (notation != masterNotation->notation()) {
        refuse(muse::trc("midirecording", "Record in the full score, not in a part."));
        return false;
    }
    if (masterNotation->playback()->isLoopEnabled()) {
        refuse(muse::trc("midirecording", "Turn off the playback loop to record."));
        return false;
    }
    return true;
}

void MidiRecordingController::startTake()
{
    const IMasterNotationPtr masterNotation = globalContext()->currentMasterNotation();
    const INotationPtr notation = globalContext()->currentNotation();
    if (!canStartTake(masterNotation, notation)) {
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
        midiRecordingControllerLeaveRange(notation, chordRest);
    }

    m_takeFromTick = fromTick;
    m_writtenTake.reset();

    beginTake(masterNotation, notation, context,
              takeExcludedTracks(midiRecordingControllerLinkedStaves(chordRest->staff()), context.voice, context.replaceMode,
                                 configuration()->playOtherStaves(), score->nstaves()));
}

//! Sets playback up for a take (count-in, click, the excluded tracks silent,
//! record speed, repeats off, monitoring on) and starts it
void MidiRecordingController::beginTake(const IMasterNotationPtr& masterNotation, const INotationPtr& notation, const TakeFile& context,
                                        const std::set<size_t>& excludedTracks)
{
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

    masterNotation->playback()->setExcludedTracks(excludedTracks);

    m_recorder.start(context);
    m_isRecordingChanged.notify();

    playbackController()->playFromTick(static_cast<muse::midi::tick_t>(context.startTick), context.countInBars);
}

//! A take that writes nothing: two measures of the click alone, from the
//! measure holding the selection or from the start of the score. The offset
//! of the notes from the beats is offered as the latency.
void MidiRecordingController::startCalibration()
{
    if (m_recorder.isActive()) {
        return;
    }

    const IMasterNotationPtr masterNotation = globalContext()->currentMasterNotation();
    const INotationPtr notation = globalContext()->currentNotation();
    if (!canStartTake(masterNotation, notation)) {
        return;
    }

    const mu::engraving::Score* score = masterNotation->masterScore();
    mu::engraving::ChordRest* chordRest = midiRecordingControllerSelectedChordRest(notation, score);
    // The measure itself, not a multimeasure rest standing for it
    const mu::engraving::Measure* first = chordRest ? score->tick2measure(chordRest->tick()) : score->firstMeasure();
    if (!first) {
        return;
    }
    // Two measures: in the last measure of the score, the one before it and the last
    if (!first->nextMeasure() && first->prevMeasure()) {
        first = first->prevMeasure();
    }
    const mu::engraving::Measure* last = first->nextMeasure() ? first->nextMeasure() : first;

    TakeFile context;
    context.startTick = first->tick().ticks();
    context.countInBars = std::max(1, configuration()->countInBars());
    context.recordSpeedPercent = MIDIRECORDINGCONTROLLER_CALIBRATION_SPEED_PERCENT;
    context.latencyMs = 0.0;
    context.settings = configuration()->quantizeSettings();

    if (chordRest && notation->interaction()->selection()->isRange()) {
        midiRecordingControllerLeaveRange(notation, chordRest);
    }

    m_calibrating = true;
    m_calibrationEndTick = last->endTick().ticks();
    beginTake(masterNotation, notation, context, takeExcludedTracks({}, 0, "span", false, score->nstaves()));
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

void MidiRecordingController::onPositionChanged(double secs, int tick)
{
    if (!m_recorder.isActive() || m_stopNs) {
        return;
    }

    const TakeRecorder::SampleResult result = m_recorder.addSample({ muse::midi::midiClockNowNs(), secs });
    if (result == TakeRecorder::SampleResult::PlaybackMoved) {
        LOGI() << "playback moved during the take; the take ends where it moved";
        requestStop();
        return;
    }

    if (m_calibrating && tick >= m_calibrationEndTick) {
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

    const bool calibrating = m_calibrating;
    const int calibrationEndTick = m_calibrationEndTick;

    restorePlayback();
    m_isRecordingChanged.notify();

    if (!take) {
        return;
    }
    if (calibrating) {
        reportCalibration(*take, calibrationEndTick);
        return;
    }
    reportTake(masterNotation, *take, m_takeFromTick);
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
    m_calibrating = false;
    m_metronomeForced = false;
    m_repeatsForced = false;
    m_midiInputForced = false;
    m_playNotesWhenEditingForced = false;
    m_playNotesOnMidiInputForced = false;
    m_takeMasterNotation.reset();
    m_canToggleRecordChanged.notify();
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

void MidiRecordingController::reportCalibration(const TakeFile& take, int endTick)
{
    // Kept for Export, to look into a calibration that went wrong
    m_lastTake = take;

    const CalibrationResult result = measureCalibration(take, endTick);
    if (!calibrationIsUsable(result)) {
        interactive()->info(muse::trc("midirecording", "Calibration needs another try"),
                            muse::trc("midirecording", "Play quarter notes with the click for two bars, as evenly as you can."));
        return;
    }

    const long offsetMs = std::lround(result.offsetMs);
    const QString offsetText = (offsetMs > 0 ? QStringLiteral("+") : QString()) + QString::number(offsetMs);
    const std::string question = muse::qtrc("midirecording", "Measured %1 ms (%2 notes, ±%3 ms). Use this?")
                                 .arg(offsetText).arg(result.notes).arg(std::lround(result.spreadMs)).toStdString();

    interactive()->question(muse::trc("midirecording", "Calibrate latency"), question,
                            { IInteractive::Button::No, IInteractive::Button::Yes })
    .onResolve(this, [this, offsetMs](const IInteractive::Result& answer) {
        if (answer.isButton(IInteractive::Button::Yes)) {
            configuration()->setLatencyMs(static_cast<double>(offsetMs));
        }
    });
}

bool MidiRecordingController::writeTakeIntoScore(const IMasterNotationPtr& masterNotation, const TakeFile& take, int fromTick,
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
        return false;
    }
    undoStack->commitChanges();
    notation->notationChanged().notify();

    midiRecordingControllerSelectWritten(notation, masterNotation->masterScore(), result, target);
    m_writtenTake = WrittenTake { masterNotation, take, fromTick, undoStack->currentStateIndex() };
    return true;
}

//! Re-apply undid the take and could not write it again: redo puts the take
//! back as it was, and Re-apply can be tried again
void MidiRecordingController::restoreReappliedTake(const INotationPtr& notation, const WrittenTake& previous)
{
    notation->interaction()->redo();
    if (notation->undoStack()->currentStateIndex() == previous.undoStateIndex) {
        m_writtenTake = previous;
    } else {
        m_writtenTake.reset();
    }
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

    const WrittenTake previous = *m_writtenTake;
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
        LOGE() << "MIDI take could not be quantized: " << result.ret.text();
        restoreReappliedTake(notation, previous);
        interactive()->warning(muse::trc("midirecording", "The take could not be read"), result.ret.text());
        return;
    }

    if (!writeTakeIntoScore(masterNotation, take, fromTick, result.val)) {
        restoreReappliedTake(notation, previous);
    }
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
