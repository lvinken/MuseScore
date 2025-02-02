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

void TablEdit::createMeasures()
{
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
            auto clef = Factory::createClef(s1);
            clef->setTrack(0);
            ClefType clefId { ClefType::G8_VB };
            clef->setClefType(clefId);
            s1->add(clef);

            auto s2 = measure->getSegment(mu::engraving::SegmentType::KeySig, tick);
            mu::engraving::KeySig* keysig = Factory::createKeySig(s2);
            keysig->setKey(Key(tefMeasure.key));
            keysig->setTrack(0);
            s2->add(keysig);

            auto s3 = measure->getSegment(mu::engraving::SegmentType::TimeSig, tick);
            mu::engraving::TimeSig* timesig = Factory::createTimeSig(s3);
            timesig->setSig(length);
            timesig->setTrack(0);
            s3->add(timesig);
        }

        tick += length;
    }
}

void TablEdit::createNotes()
{
    for (const auto& tefNote : tefContents) {
        if (tefInstruments.size() == 0) {
            LOGD("no instruments");
            return;
        }

        const TefInstrument& instrument { tefInstruments.at(0) };
        if (instrument.stringNumber < 1 || 12 < instrument.stringNumber) {
            LOGD("invalid instrument.stringNumber %d", instrument.stringNumber);
            return;
        }

        if (!tefNote.rest && (tefNote.string < 1 || instrument.stringNumber < tefNote.string)) {
            LOGD("invalid string %d", tefNote.string);
            continue;
        }

        if (tefNote.voice < 0 || 3 < tefNote.voice) {
            LOGD("invalid voice %d", tefNote.voice);
            continue;
        }

        ChordRest* cr { nullptr };
        Fraction length { tefNote.length, 64 }; // length is in 64th
        if (tefNote.dotted) {
            length *= Fraction{ 3, 2 };
        }
        // TODO: triplets
        LOGD("length %d/%d", length.numerator(), length.denominator());
        TDuration tDuration(length);
        if (tefNote.dotted) {
            tDuration.setDots(1);
        }

        if (tefNote.rest) {
            mu::engraving::Rest* rest = Factory::createRest(score->dummy()->segment());
            cr = rest;
            rest->setTrack(tefNote.voice); // TODO staff
            rest->setDurationType(tDuration);
            rest->setTicks(length);
            LOGD("rest");
        }
        else {
            // create chord
            mu::engraving::Chord* chord = Factory::createChord(score->dummy()->segment());
            cr = chord;
            chord->setTrack(tefNote.voice); // TODO staff
            chord->setDurationType(tDuration);
            chord->setTicks(length);
            // add note to chord
            mu::engraving::Note* note = Factory::createNote(chord);
            note->setTrack(tefNote.voice);
            int pitch = 96 - instrument.tuning.at(tefNote.string - 1)  + tefNote.fret;  // todo fix magical constant and code duplication
            LOGD("string %d fret %d pitch %d", tefNote.string, tefNote.fret, pitch);
            note->setPitch(pitch);
            note->setTpcFromPitch(Prefer::NEAREST);
            chord->add(note);
        }
        // add chord to measure
        Fraction tick { tefNote.position, 64 }; // position is in 64th
        LOGD("tick %d/%d", tick.numerator(), tick.denominator());
        Measure* measure { score->tick2measure(tick) };
        if (!measure) {
            LOGD("no measure");
            return;
        }
        Segment* segment { measure->getSegment(mu::engraving::SegmentType::ChordRest, tick) };
        if (!segment) {
            LOGD("no segment");
            return;
        }
        segment->add(cr);
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

void TablEdit::createScore()
{
    createParts();
    createTitleFrame();
    createMeasures();
    createNotes();
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
 */

// return note length in 64th

static int duration2length(const int duration) {
    if (0 <= duration && duration <= 18) {
        // remove dot and triplet
        int noteType { 0 };
        int dotOrTriplet { duration % 3 };
        switch (dotOrTriplet) {
        case 0: noteType = duration / 3; break;
        case 1: noteType = (duration + 2) / 3; break;
        case 2: noteType = (duration + 1) / 3; break;
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
    LOGD("invalid note duration %d", duration);
    return 0; // invalid
}

static bool duration2dotted(const int duration) {
    if (0 <= duration && duration <= 18) {
        return duration % 3 == 1;
    }
    else {
        LOGD("invalid note duration %d", duration);
        return false; // invalid
    }
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

// todo handle rest
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
            note.rest = true;
        }
        else {
            // not a note or rest
        }
        if (noteRestMarker <= 0x33) {
            note.duration = byte2 & 0x1F;
            note.length = duration2length(note.duration);
            note.dotted = duration2dotted(note.duration);
            note.triplet = duration2triplet(note.duration);
            note.voice = (byte3 & 0x30) / 0x10;
        }
        //LOGD("marker %d duration %d length %d dotted %d", noteRestMarker, note.duration, note.length, note.dotted);
        tefContents.push_back(note);
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
    LOGD("copyright '%s'", tefHeader.copyright.c_str());
    LOGD("tbed %d wOldNum %d wFormat %d", tefHeader.tbed, tefHeader.wOldNum, tefHeader.wFormat);
    if ((tefHeader.wFormat >> 8) != 10) {
        return Err::FileBadFormat;
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
        LOGD("position %d rest %d string %d fret %d duration %d length %d dotted %d triplet %d voice %d",
             note.position, note.rest, note.string, note.fret, note.duration, note.length, note.dotted, note.triplet, note.voice);
    }
    createScore();
    return Err::NoError;
}

} // namespace mu::iex::tabledit
