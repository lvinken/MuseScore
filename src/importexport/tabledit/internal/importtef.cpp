/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited
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
#include "importtef.h"

#include "engraving/dom/box.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/measurebase.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/text.h"
#include "engraving/dom/timesig.h"
#include "log.h"

using namespace mu::engraving;

namespace mu::iex::tabledit {

int8_t TablEdit::readInt8()
{
    int8_t result;
    _file->read((uint8_t*)&result, 1);
    return result;
}

uint8_t TablEdit::readUInt8()
{
    uint8_t result;
    _file->read((uint8_t*)&result, 1);
    return result;
}

uint16_t TablEdit::readUInt16()
{
    uint16_t result;
    _file->read((uint8_t*)&result, 2);
    return result;
}

uint32_t TablEdit::readUInt32()
{
    uint32_t result;
    _file->read((uint8_t*)&result, 4);
    return result;
}

// read sized utf8 text
// input is the position where the text's position in the file is stored

string TablEdit::readUtf8Text(uint32_t positionOfPosition)
{
    string result;
    _file->seek(positionOfPosition);
    uint32_t position = readUInt32();
    _file->seek(position);
    uint16_t size = readUInt16();
    LOGD("position %d size %d", position, size);
    for (uint16_t i = 0; i < size - 1; ++i) {
        result += readUInt8();
    }
    return result;
}

// return the part index for the instrument containing stringIdx

engraving::part_idx_t TablEdit::partIdx(size_t stringIdx, bool &ok) const
{
    ok = true;
    engraving::part_idx_t result { 0 };
    engraving::track_idx_t lowerBound { 1 };
    engraving::track_idx_t upperBound { 0 };

    for (const auto& instrument : tefInstruments) {
        upperBound += instrument.stringNumber;
        if (lowerBound <= stringIdx && stringIdx <= upperBound) {
            LOGN("string %zu lower %zu upper %zu found result %zu", stringIdx, lowerBound, upperBound, result);
            return result;
        }
        ++result;
        lowerBound += instrument.stringNumber;
    }
    ok = false;
    result = 0;
    LOGD("string %zu not found result %zu", stringIdx, result);
    return result;
}

// return total number of strings in previous parts

int TablEdit::stringNumberPreviousParts(part_idx_t partIdx) const
{
    part_idx_t result { 0 };
    for (part_idx_t i = 0; i < partIdx; ++i) {
        result += tefInstruments.at(i).stringNumber;
    }
    LOGN("partIdx %zu result %zu", partIdx, result);
    return result;

}

bool TablEdit::VoiceAllocator::canAddTefNoteToVoice(const TefNote* const note, const int voice)
{
    // is there room after the previous note ?
    if (stopPosition(voice) <= note->position) {
        LOGD("add string %d fret %d to voice %d", note->string, note->fret, voice);
        return true;
    }
    // can the note go into a chord ?
    const auto notePlaying = notesPlaying.at(voice);
    if (notePlaying
        && !notePlaying->rest
        && !note->rest
        && notePlaying->position == note->position
        && notePlaying->duration == note->duration ) {
        LOGD("add string %d fret %d to voice %d as chord", note->string, note->fret, voice);
        return true;
    }
    return false;
}

int TablEdit::VoiceAllocator::findFirstPossibleVoice(const TefNote* const note, const array<int, 3> voices)
{
    for (const auto v : voices) {
        if (canAddTefNoteToVoice(note, v)) {
            return v;
        }
    }
    return -1;
}

int durationToInt(uint8_t duration)
{
    switch(duration) {
    case  0: return 64; //"whole";
    case  1: return 48; //"half dotted";
    case  2: return 32; //"whole triplet";
    case  3: return 32; //"half";
    case  4: return 24; //"quarter dotted";
    case  5: return 16; //"half triplet";
    case  6: return 16; //"quarter";
    case  7: return 12; //"eighth dotted";
    case  8: return 8; //"quarter triplet";
    case  9: return 8; //"eighth";
    case 10: return 6; //"16th dotted";
    case 11: return 4; //"eighth triplet";
    case 12: return 4; //"16th";
    case 13: return 3; //"32nd dotted";
    case 14: return 2; //"16th triplet";
    case 15: return 2; //"32nd";
    //case 16: return "64th dotted";
    case 17: return 1; //"32nd triplet";
    case 18: return 1; //"64th";
    case 19: return 56; //"half double dotted";
    //case 20: return "16th quintuplet";
    case 22: return 28; //"quarter double dotted";
    case 25: return 14; //"eighth double dotted";
    case 28: return 7; //"16th double dotted";
    default: return 0; //"undefined";
    }
    return 0; //"undefined";
}

int TablEdit::VoiceAllocator::stopPosition(const size_t voice)
{
    if (VOICES <= voice) {
        LOGD("incorrect voice %zu", voice);
        return -1;
    }

    const auto note = notesPlaying.at(voice);
    if (note) {
        return note->position + durationToInt(note->duration);
    }
    return 0;
}

void TablEdit::VoiceAllocator::appendNoteToVoice(const TefNote* const note, int voice)
{
    LOGD("position %d string %d fret %d voice %d", note->position, note->string, note->fret, voice);
    const auto nChords {voiceContents[voice].size()};
    LOGD("voice %d nChords %zu", voice, nChords);
    if (nChords == 0) {
        LOGD("create first chord");
        vector<const TefNote*> chord;
        chord.push_back(note);
        voiceContents[voice].push_back(chord);
    }
    else {
        const auto position {voiceContents[voice].at(nChords - 1).at(0)->position};
        LOGD("chord %zu position %d", nChords - 1, position);
        if (position == note->position) {
            LOGD("add to last chord");
            voiceContents[voice].at(nChords - 1).push_back(note);
        }
        else {
            LOGD("create next chord at position %d", note->position);
            vector<const TefNote*> chord;
            chord.push_back(note);
            voiceContents[voice].push_back(chord);
        }
    }
    LOGD("done");
}

// debug: dump voices

void TablEdit::VoiceAllocator::dump()
{
    for (size_t i = 0; i < mu::engraving::VOICES; ++i) {
        LOGD("- voice %zu", i);
        for (size_t j = 0; j < voiceContents.at(i).size(); ++j) {
            LOGD("  - chord %zu", j);
            for (const auto note : voiceContents.at(i).at(j)) {
                LOGD("    - position %d string %d fret %d", note->position, note->string, note->fret);
            }
        }
    }
}

void TablEdit::VoiceAllocator::allocateVoice(const TefNote* const note, int voice)
{
    if (voice >= 0) {
        // do actual allocation
        // note chord info is lost
        if (allocations.count(note) == 0) {
            allocations[note] = voice;
            notesPlaying[voice] = note;
            appendNoteToVoice(note, voice);
        }
        else {
            LOGD("duplicate note allocation");
        }
    }
    else {
        LOGD("cannot add string %d fret %d to voice %d", note->string, note->fret, voice);
    }
}

void TablEdit::VoiceAllocator::addColumn(const vector<const TefNote* const>& column)
{
    if (column.empty()) {
        return;
    }

    // first add the highest note to voice 0
    addNote(column.at(0), true);
    if (column.size() >= 2) {
        // then add the lowest note to voice 1
        addNote(column.at(column.size() - 1), false);
        // finally add the remaining notes where possible
        for (unsigned int i = 1; i < column.size() - 1; ++i) {
            addNote(column.at(i), true);
        }
    }
}

void TablEdit::VoiceAllocator::addNote(const TefNote* const note, const bool preferVoice0)
{
    int voice { -1 };
    LOGD("note position %d voice %d", note->position, note->voice);
    if (note->voice == 2) { // TODO: fix magic constant
        voice = findFirstPossibleVoice(note, { 0, 2, 3 });
    }
    else if (note->voice == 3) { // TODO: fix magic constant
        voice = findFirstPossibleVoice(note, { 1, 2, 3 });
    }
    else {
        if (preferVoice0) {
            voice = findFirstPossibleVoice(note, { 0, 1, 2 /* TODO , 3 */ });
        }
        else {
            voice = findFirstPossibleVoice(note, { 1, 0, 2 /* TODO , 3 */ });
        }
    }
    allocateVoice(note, voice);
}

int TablEdit::VoiceAllocator::voice(const TefNote* const note)
{
    int res { -1 }; // TODO -1 ?
    if (allocations.count(note) > 0) {
        res = allocations[note];
    }
    else {
        LOGD("no voice allocated for note %p", note);
    }

    LOGD("note %p voice %d res %d", note, note->voice, res);
    return res;
}

// debug: use color cr to show voice

static muse::draw::Color toColor(const int voice)
{
#if 0
    // no debug: color notes black
    return muse::draw::Color::BLACK;
#else
    // debug: color notes based on voice number
    switch (voice) {
    case 0: return muse::draw::Color::BLUE;
    case 1: return muse::draw::Color::GREEN;
    case 2: return muse::draw::Color::RED;
    case 3: return { 150, 150, 0, 255 };
    default: return muse::draw::Color::BLACK;
    }
#endif
}

// create a VoiceAllocator for every instrument

void TablEdit::initializeVoiceAllocators(vector<VoiceAllocator>& allocators)
{
    for (size_t i = 0; i < tefInstruments.size(); ++i) {
        VoiceAllocator allocator;
        allocators.push_back(allocator);
    }
}

void TablEdit::allocateVoices(vector<VoiceAllocator>& allocators)
{
    vector<const TefNote* const> column;
    int currentPosition { -1 };
    engraving::part_idx_t currentPart { 0 };
    for (const TefNote& tefNote : tefContents) {
        // a new column is started when either the postion or the part changes
        bool ok { true };
        const auto part = partIdx(tefNote.string, ok);
        if (!ok) {
            LOGD("error: invalid string %d", tefNote.string);
            continue;
        }
        if (tefNote.position != currentPosition || part != currentPart) {
            // handle the previous column
            allocators[currentPart].addColumn(column);
            // start a new column
            currentPosition = tefNote.position;
            currentPart = part;
            column.clear();
        }
        column.push_back(&tefNote);
    }
    // handle the last column
    allocators[currentPart].addColumn(column);
}

static void addNoteToChord(mu::engraving::Chord* chord, track_idx_t track, int pitch, int fret, int string, muse::draw::Color color)
{
    mu::engraving::Note* note = Factory::createNote(chord);
    if (note) {
        note->setTrack(track);
        note->setPitch(pitch);
        note->setTpcFromPitch(Prefer::NEAREST);
        note->setFret(fret);
        note->setString(string);
        note->setColor(color);
        chord->add(note);
    }
}

static void addRest(Segment* segment, track_idx_t track, TDuration tDuration, Fraction length, muse::draw::Color color)
{
    mu::engraving::Rest* rest = Factory::createRest(segment);
    if (rest) {
        rest->setTrack(track);
        rest->setDurationType(tDuration);
        rest->setTicks(length);
        rest->setColor(color);
        segment->add(rest);
    }
}

// start/stop tuplet and calculate timing correction
// for a triplet of quarters, the length of each note is 2/3 * 1/4 = 1/6
// TablEdit uses 1/8 instead, required next note position correction
// is 1/6 - 1/8 = 4/24 - 3/24 = 1/24

Fraction TablEdit::TupletHandler::doTuplet(const TefNote* const tefNote)
{
    Fraction res {0, 1};
    Fraction correction {1, 24};
    LOGD("position %d string %d fret %d triplet %d",
         tefNote->position, tefNote->string, tefNote->fret, tefNote->triplet);
    LOGD("before inTuplet %d count %d", inTuplet, count);
    if (tefNote->triplet) {
        if (!inTuplet) {
            LOGD("start triplet");
        }
        inTuplet = true;
        res = Fraction {count, 1} * correction;
        ++count;
    }
    if (!tefNote->triplet || (inTuplet && count == 3)) {
        if (inTuplet) {
            LOGD("stop triplet");
        }
        inTuplet = false;
        count = 0;
    }
    LOGD("after inTuplet %d count %d res %d/%d", inTuplet, count, res.numerator(), res.denominator());
    return res;
}

// add ChordRest to tuplet
// needs measure, track and ratio (take from CR ?)
// baselen TBD

void TablEdit::TupletHandler::addCr(Measure* measure, ChordRest* cr)
{
    if (inTuplet && !tuplet) {
        tuplet = Factory::createTuplet(measure);
        LOGD("new tuplet %p", tuplet);
        tuplet->setParent(measure); // may not be required
        tuplet->setTrack(cr->track());
        const Fraction l {1, 4}; // todo: calculate
        tuplet->setBaseLen(l);
        tuplet->setRatio({3, 2});
        //tuplet->setTicks(l * tuple->ratio().denominator()); // may not be required
    }
    if (tuplet) {
        LOGD("add cr to tuplet %p", tuplet);
        cr->setTuplet(tuplet);
        tuplet->add(cr);
    }
    if (!inTuplet) {
        tuplet = nullptr;
    }
    LOGD("tuplet %p", tuplet);
}

void TablEdit::createContents()
{
    if (tefInstruments.size() == 0) {
        LOGD("error: no instruments");
        return;
    }

    vector<VoiceAllocator> voiceAllocators;
    initializeVoiceAllocators(voiceAllocators);
    allocateVoices(voiceAllocators);

    for (size_t part = 0; part < tefInstruments.size(); ++part) {

        LOGD("part %zu", part);
        for (size_t voice = 0; voice < mu::engraving::VOICES; ++voice) {

            LOGD("- voice %zu", voice);
            auto& voiceContent {voiceAllocators.at(part).voiceContent(voice)};
            TupletHandler tupletHandler;
            for (size_t k = 0; k < voiceContent.size(); ++k) {

                LOGD("  - chord %zu", k);
                // tefNotes is either a rest or a chord of one or more notes
                const vector<const TefNote*>& tefNotes {voiceContent.at(k)};

                if (tefNotes.size() == 0) {
                    continue; // shouldn't happen
                }

                const TefNote* const firstNote {tefNotes.at(0)};
                Fraction length { firstNote->length, 64 }; // length is in 64th
                if (firstNote->dots == 1) {
                    length *= Fraction{ 3, 2 };
                }
                else if (firstNote->dots == 2) {
                    length *= Fraction{ 7, 4 };
                }
                // TODO: triplets
                TDuration tDuration(length);
                if (firstNote->dots) {
                    tDuration.setDots(firstNote->dots);
                }
                const auto positionCorrection = tupletHandler.doTuplet(firstNote);

                Fraction tick { firstNote->position, 64 }; // position is in 64th
                tick += positionCorrection;
                LOGD("    positionCorrection %d/%d tick %d/%d length %d/%d",
                     positionCorrection.numerator(), positionCorrection.denominator(),
                     tick.numerator(), tick.denominator(),
                     length.numerator(), length.denominator()
                     );

                Measure* measure { score->tick2measure(tick) };
                if (!measure) {
                    LOGD("error: no measure");
                    continue;
                }
                else {
                    LOGD("measure %p", measure);
                }
                Segment* segment { measure->getSegment(mu::engraving::SegmentType::ChordRest, tick) };
                if (!segment) {
                    LOGD("error: no segment");
                    continue;
                }

                const auto track = part * VOICES + voice;
                if (segment->element(track)) {
                    LOGD("segment not empty");
                    continue;
                }

                if (firstNote->rest) {
                    LOGD("    - rest position %d string %d fret %d", firstNote->position, firstNote->string, firstNote->fret);
                    addRest(segment, track, tDuration, length, toColor(voice));
                }
                else {
                    LOGD("    - note(s) position %d string %d fret %d", firstNote->position, firstNote->string, firstNote->fret);
                    mu::engraving::Chord* chord {Factory::createChord(segment)};
                    if (chord) {
                        chord->setTrack(track);
                        chord->setDurationType(tDuration);
                        chord->setTicks(length);

                        const TefInstrument& instrument { tefInstruments.at(part) };
                        if (instrument.stringNumber < 1 || 12 < instrument.stringNumber) {
                            LOGD("error: invalid instrument.stringNumber %d", instrument.stringNumber);
                            continue;
                        }

                        for (const auto note : tefNotes) {
                            const auto stringOffset = stringNumberPreviousParts(part);
                            // todo fix magical constant 96 and code duplication
                            int pitch = 96 - instrument.tuning.at(note->string - stringOffset - 1)  + note->fret;
                            LOGD("      -> string %d fret %d pitch %d", note->string, note->fret, pitch);
                            // note TableEdit's strings start at 1, MuseScore's at 0
                            addNoteToChord(chord, track, pitch, note->fret, note->string - 1, toColor(voice));
                        }
                        segment->add(chord);
                        tupletHandler.addCr(measure, chord);
                    }
                }
            }
        }
    }
}

void TablEdit::createMeasures()
{
    int lastKey { 0 };               // safe default
    Fraction lastTimeSig { -1, -1 }; // impossible value
    Fraction tick { 0, 1 };
    for (const auto& tefMeasure : tefMeasures) {
        // create measure
        auto measure = Factory::createMeasure(score->dummy()->system());
        measure->setTick(tick);
        Fraction length{ tefMeasure.numerator, tefMeasure.denominator };
        measure->setTimesig(length);
        measure->setTicks(length);
        measure->setEndBarLineType(BarLineType::NORMAL, 0);
        score->measures()->add(measure);

        if (tick == Fraction { 0, 1 }) {
            auto s1 = measure->getSegment(mu::engraving::SegmentType::HeaderClef, tick);
            for (size_t i = 0; i < tefInstruments.size(); ++i) {
                auto clef = Factory::createClef(s1);
                clef->setTrack(i * VOICES);
                ClefType clefId { ClefType::G8_VB };
                clef->setClefType(clefId);
                s1->add(clef);
            }

            auto s2 = measure->getSegment(mu::engraving::SegmentType::KeySig, tick);
            for (size_t i = 0; i < tefInstruments.size(); ++i) {
                mu::engraving::KeySig* keysig = Factory::createKeySig(s2);
                keysig->setKey(Key(tefMeasure.key));
                keysig->setTrack(i * VOICES);
                s2->add(keysig);
            }
            lastKey = tefMeasure.key;

            auto s3 = measure->getSegment(mu::engraving::SegmentType::TimeSig, tick);
            for (size_t i = 0; i < tefInstruments.size(); ++i) {
                mu::engraving::TimeSig* timesig = Factory::createTimeSig(s3);
                timesig->setSig(length);
                timesig->setTrack(i * VOICES);
                s3->add(timesig);
            }
            lastTimeSig = length;
            createTempo();
        }
        else {
            if (tefMeasure.key != lastKey) {
                auto s2 = measure->getSegment(mu::engraving::SegmentType::KeySig, tick);
                for (size_t i = 0; i < tefInstruments.size(); ++i) {
                    mu::engraving::KeySig* keysig = Factory::createKeySig(s2);
                    keysig->setKey(Key(tefMeasure.key));
                    keysig->setTrack(i * VOICES);
                    s2->add(keysig);
                }
                lastKey = tefMeasure.key;
            }
            if (length != lastTimeSig) {
                auto s3 = measure->getSegment(mu::engraving::SegmentType::TimeSig, tick);
                for (size_t i = 0; i < tefInstruments.size(); ++i) {
                    mu::engraving::TimeSig* timesig = Factory::createTimeSig(s3);
                    timesig->setSig(length);
                    timesig->setTrack(i * VOICES);
                    s3->add(timesig);
                }
                lastTimeSig = length;
            }
        }

        tick += length;
    }
}

void TablEdit::createNotesFrame()
{
    if (!tefHeader.notes.empty()) {
        VBox* vbox = Factory::createTitleVBox(score->dummy()->system());
        vbox->setTick(mu::engraving::Fraction(0, 1)); // TODO find correct value (0/1 seems to work OK)
        score->measures()->add(vbox);
        Text* s = Factory::createText(vbox, TextStyleType::FRAME);
        s->setPlainText(String::fromUtf8(tefHeader.notes.c_str()));
        vbox->add(s);
    }
}

void TablEdit::createParts()
{
    for (const auto& instrument : tefInstruments) {
        Part* part = new Part(score);
        score->appendPart(part);
        String staffName { String::fromUtf8(instrument.name.c_str()) };
        part->setPartName(staffName);
        part->setPlainLongName(staffName);

        StringData stringData;
        stringData.setFrets(25); // reasonable default (?)
        for (int i = 0; i < instrument.stringNumber; ++i) {
            int pitch = 96 - instrument.tuning.at(instrument.stringNumber - i - 1);
            LOGD("pitch %d", pitch);
            instrString str { pitch };
            stringData.stringList().push_back(str);
        }
        part->instrument()->setStringData(stringData);

        part->setMidiProgram(instrument.midiVoice);
        part->setMidiChannel(instrument.midiBank);

        Staff* staff = Factory::createStaff(part);
        score->appendStaff(staff);
    }
}

void TablEdit::createProperties()
{
    if (!tefHeader.title.empty()) {
        score->setMetaTag(u"workTitle", String::fromUtf8(tefHeader.title.c_str()));
    }
    if (!tefHeader.subTitle.empty()) {
        score->setMetaTag(u"subtitle", String::fromUtf8(tefHeader.subTitle.c_str()));
    }
    if (!tefHeader.comment.empty()) {
        score->setMetaTag(u"comment", String::fromUtf8(tefHeader.comment.c_str()));
    }
    if (!tefHeader.internetLink.empty()) {
        score->setMetaTag(u"source", String::fromUtf8(tefHeader.internetLink.c_str()));
    }
    if (!tefHeader.copyright.empty()) {
        score->setMetaTag(u"copyright", String::fromUtf8(tefHeader.copyright.c_str()));
    }
}

void TablEdit::createScore()
{
    createProperties();
    createParts();
    createTitleFrame();
    createMeasures();
    createNotesFrame();
    createContents();
}

void TablEdit::createTempo()
{
    mu::engraving::Measure* measure = score->firstMeasure();
    mu::engraving::Segment* segment = measure->getSegment(mu::engraving::SegmentType::ChordRest, mu::engraving::Fraction(0, 1));
    mu::engraving::TempoText* tt = new mu::engraving::TempoText(segment);
    tt->setTempo(double(tefHeader.tempo) / 60.0);
    tt->setTrack(0);
    tt->setFollowText(true);
    muse::String tempoText = mu::engraving::TempoText::duration2tempoTextString(mu::engraving::DurationType::V_QUARTER);
    tempoText += u" = ";
    tempoText += muse::String::number(tefHeader.tempo);
    tt->setXmlText(tempoText);
    segment->add(tt);
}

void TablEdit::createTitleFrame()
{
    VBox* vbox = Factory::createTitleVBox(score->dummy()->system());
    vbox->setTick(mu::engraving::Fraction(0, 1));
    score->measures()->add(vbox);
    if (!tefHeader.title.empty()) {
        Text* s = Factory::createText(vbox, TextStyleType::TITLE);
        s->setPlainText(String::fromUtf8(tefHeader.title.c_str()));
        vbox->add(s);
    }
    if (!tefHeader.subTitle.empty()) {
        Text* s = Factory::createText(vbox, TextStyleType::SUBTITLE);
        s->setPlainText(String::fromUtf8(tefHeader.subTitle.c_str()));
        vbox->add(s);
    }
}

/*
 * encoding of note durations
 * 0 whole
 * 1 half dotted
 * 2 whole triplet
 * 3 half
 * 4 quarter dotted
 * 5 half triplet
 * 6 quarter
 * 7 eight dotted
 * 8 quarter triplet
 * ...
 * 18 1/64th note
 * 19 half double dotted
 * 22 quarter double dotted
 * 25 eighth double dotted
 * 28 16th double dotted
 */

// return note length in 64th
// TODO: check for code duplication

static int duration2length(const int duration) {
    if (0 <= duration && duration <= 18) {
        // remove dot and triplet
        int noteType { 0 };
        int dotOrTriplet { duration % 3 };
        switch (dotOrTriplet) {
        case 0: noteType = duration / 3; break;
        case 1: noteType = (duration + 2) / 3; break;
        case 2: noteType = (duration - 2) / 3; break;
        default: LOGD("impossible value %d", dotOrTriplet);
        }
        switch (noteType) {
        case 0: return 64; // 1/1
        case 1: return 32; // 1/2
        case 2: return 16; // 1/4
        case 3: return  8; // 1/8
        case 4: return  4; // 1/16
        case 5: return  2; // 1/32
        case 6: return  1; // 1/64
        default: LOGD("impossible value %d", dotOrTriplet);
        }
    }
    else {
        switch (duration) {
        case 19: return 32; // 1/2
        case 22: return 16; // 1/4
        case 25: return  8; // 1/8
        case 28: return  4; // 1/16
        default: LOGD("impossible value %d", duration);
        }
    }
    LOGD("invalid note duration %d", duration);
    return 0; // invalid (result is weird layout)
}

// TODO: check for code duplication

static int duration2dots(const int duration) {
    if (0 <= duration && duration <= 18 && (duration % 3) == 0) {
        return 0;
    }
    else if (0 <= duration && duration <= 18 && (duration % 3) == 1) {
        return 1;
    }
    else if (duration == 19 || duration == 22 || duration == 25 || duration == 28) {
        return 2;
    }
    LOGD("invalid note duration %d", duration);
    return 0; // invalid
}

static bool duration2triplet(const int duration) {
    if (0 <= duration && duration <= 18) {
        return duration % 3 == 2;
    }
    else {
        LOGD("invalid note duration %d", duration);
        return false; // invalid
    }
}

void TablEdit::readTefContents()
{
    // calculate the total number of strings
    // instruments must have been read before reading contents
    if (tefInstruments.empty()) {
        LOGD("no instruments");
        return;
    }
    int totalNumberOfStrings { 0 };
    for (const auto& instrument : tefInstruments) {
        totalNumberOfStrings += instrument.stringNumber;
    }
    LOGD("totalNumberOfStrings %d", totalNumberOfStrings);

    _file->seek(0x3c);
    uint32_t position = readUInt32();
    _file->seek(position);
    uint32_t offset = readUInt32();
    LOGD("position %d offset %d", position, offset);
    while (offset != 0xFFFFFFFF) {
        uint8_t byte1 = readUInt8();
        uint8_t byte2 = readUInt8();
        uint8_t byte3 = readUInt8();
        /* uint8_t byte4 = */ readUInt8();
        /* uint8_t byte5 = */ readUInt8();
        /* uint8_t byte6 = */ readUInt8();
        /* uint8_t byte7 = */ readUInt8();
        /* uint8_t byte8 = */ readUInt8();
        TefNote note;
        note.position = (offset >> 3) / totalNumberOfStrings;
        const auto noteRestMarker = byte1 & 0x3F;
        if (noteRestMarker < 0x33) {
            note.string = ((offset >> 3) % totalNumberOfStrings) + 1;
            note.fret = noteRestMarker - 1;
        }
        else if (noteRestMarker == 0x33) {
            note.string = ((offset >> 3) % totalNumberOfStrings) + 1;
            note.rest = true;
        }
        else {
            // not a note or rest
            //LOGD("marker %d duration %d length %d dots %d", noteRestMarker, note.duration, note.length, note.dots);
        }
        if (noteRestMarker <= 0x33) {
            note.duration = byte2 & 0x1F;
            note.length = duration2length(note.duration);
            note.dots = duration2dots(note.duration);
            note.triplet = duration2triplet(note.duration);
            note.voice = (byte3 & 0x30) / 0x10;
            tefContents.push_back(note);
        }
        offset = readUInt32();
    }
}

// tuning to MIDI: 96 - tuning[string] with string 0 is highest
// MIDI E2 = 40 E4 = 64
// todo: check interaction with fMiddleC

void TablEdit::readTefInstruments()
{
    _file->seek(0x60);
    uint32_t position = readUInt32();
    _file->seek(position);
    uint16_t structSize = readUInt16();
    uint16_t numberOfInstruments = readUInt16();
    //uint32_t zero = readUInt16();
    LOGD("structSize %d numberOfInstruments %d", structSize, numberOfInstruments);
    for (uint16_t i = 0; i < numberOfInstruments; ++i) {
        TefInstrument instrument;
        instrument.stringNumber = readUInt16();
        instrument.firstString = readUInt16();
        instrument.available16U = readUInt16();
        instrument.verticalSpacing = readUInt16();
        instrument.midiVoice = readUInt8();
        instrument.midiBank = readUInt8();
        instrument.nBanjo5 = readUInt8();
        instrument.uSpec = readUInt8();
        instrument.nCapo = readUInt16();
        instrument.fMiddleC = readUInt8();
        instrument.fClef = readUInt8();
        instrument.output = readUInt16();
        instrument.options = readUInt16();
        for (uint16_t i = 0; i < 12; ++i) {
            auto n = readUInt8();
            instrument.tuning[i] = n;
        }
        // name is a zero-terminated utf8 string
        bool atEnd = false;
        for (uint16_t i = 0; i < 36; ++i) {
            auto c = readUInt8();
            if (c == 0) {
                // stop appending, but continue reading
                atEnd = true;
            }
            if (0x20 <= c && c <= 0x7E && !atEnd) {
                instrument.name += static_cast<char>(c);
            }
        }
        tefInstruments.push_back(instrument);
    }
}

void TablEdit::readTefMeasures()
{
    _file->seek(0x5c);
    uint32_t position = readUInt32();
    _file->seek(position);
    /* uint16_t structSize = */ readUInt16();
    uint16_t numberOfMeasures = readUInt16();
    /* uint32_t zero = */ readUInt32();
    for (uint16_t i = 0; i < numberOfMeasures; ++i) {
        TefMeasure measure;
        measure.flag = readUInt8();
        /* uint8_t uTmp = */ readUInt8();
        measure.key = readInt8();
        measure.size = readUInt8();
        measure.denominator = readUInt8();
        measure.numerator = readUInt8();
        /* uint16_t margins = */ readUInt16();
        tefMeasures.push_back(measure);
    }
}

void TablEdit::readTefHeader()
{
    readUInt16(); // skip private01
    tefHeader.version = readUInt16();
    tefHeader.subVersion = readUInt16();
    tefHeader.tempo = readUInt16();
    tefHeader.chorus = readUInt16();
    tefHeader.reverb = readUInt16();
    readUInt16(); // todo syncope (is signed)
    readUInt16(); // skip private02
    tefHeader.securityCode = readUInt32();
    tefHeader.securityFlags = readUInt32();
    _file->seek(0x38);
    tefHeader.tbed = readUInt32();
    readUInt32(); // skip contents
    auto titlePtr = readUInt32();
    LOGD("titlePtr %d", titlePtr);
    _file->seek(titlePtr);
    tefHeader.title = readUtf8Text(0x40);
    tefHeader.subTitle = readUtf8Text(0x44);
    tefHeader.comment = readUtf8Text(0x48);
    tefHeader.notes = readUtf8Text(0x4c);
    tefHeader.internetLink = readUtf8Text(0x84);
    tefHeader.copyright = readUtf8Text(0x8c);
    _file->seek(202);
    tefHeader.wOldNum = readUInt16();
    tefHeader.wFormat = readUInt16();
}

//---------------------------------------------------------
//   import
//---------------------------------------------------------

Err TablEdit::import()
{
    readTefHeader();
    LOGD("version %d subversion %d", tefHeader.version, tefHeader.subVersion);
    LOGD("tempo %d chorus %d reverb %d", tefHeader.tempo, tefHeader.chorus, tefHeader.reverb);
    LOGD("securityCode %d securityFlags %d", tefHeader.securityCode, tefHeader.securityFlags);
    LOGD("title '%s'", tefHeader.title.c_str());
    LOGD("subTitle '%s'", tefHeader.subTitle.c_str());
    LOGD("comment '%s'", tefHeader.comment.c_str());
    LOGD("notes '%s'", tefHeader.notes.c_str());
    LOGD("internetLink '%s'", tefHeader.internetLink.c_str());
    LOGD("copyright '%s'", tefHeader.copyright.c_str());
    LOGD("tbed %d wOldNum %d wFormat %d", tefHeader.tbed, tefHeader.wOldNum, tefHeader.wFormat);
    if ((tefHeader.wFormat >> 8) < 10) {
        return Err::FileBadFormat;
        //return Err::FileTooOld; // TODO: message is too specific for MuseScore format
    }
    if ((tefHeader.wFormat >> 8) > 10) {
        return Err::FileBadFormat;
        //return Err::FileTooNew;
    }
    if (tefHeader.securityCode != 0) {
        return Err::FileBadFormat; // todo "file is protected" message ?
    }
    readTefMeasures();
    for (const auto& measure : tefMeasures) {
        LOGD("flag %d key %d size %d numerator %d denominator %d",
             measure.flag, measure.key, measure.size, measure.numerator, measure.denominator);
    }
    readTefInstruments();
    for (const auto& instrument : tefInstruments) {
        LOGD("stringNumber %d firstString %d midiVoice %d midiBank %d",
             instrument.stringNumber, instrument.firstString, instrument.midiVoice, instrument.midiBank);
    }
    readTefContents();
    for (const auto& note : tefContents) {
        LOGD("position %d rest %d string %d fret %d duration %d length %d dots %d triplet %d voice %d",
             note.position, note.rest, note.string, note.fret, note.duration, note.length, note.dots, note.triplet, note.voice);
    }
    createScore();
    return Err::NoError;
}

} // namespace mu::iex::tabledit
