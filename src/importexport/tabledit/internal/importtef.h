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
#ifndef MU_IMPORTEXPORT_IMPORTTEF_H
#define MU_IMPORTEXPORT_IMPORTTEF_H

#include "engraving/dom/masterscore.h"
#include "engraving/dom/tuplet.h"
#include "engraving/engravingerrors.h"
#include "io/iodevice.h"

using namespace std;
namespace mu::iex::tabledit {

class TablEdit
{
    muse::io::IODevice* _file = nullptr;
    mu::engraving::MasterScore* score = nullptr;

    int8_t readInt8();
    uint8_t readUInt8();
    uint16_t readUInt16();
    uint32_t readUInt32();
    string readUtf8Text();
    string readUtf8TextIndirect(uint32_t positionOfPosition);

    struct TefHeader {
        int version { 0 };
        int subVersion { 0 };
        int tempo { 0 };
        int chorus { 0 };
        int reverb { 0 };
        int syncope { 0 };
        unsigned int securityCode { 0 }; // open file only if 0
        unsigned int securityFlags { 0 };
        int tbed { 0 }; // should be 0x64656274 ("tbed"), but is sometimes reversed (?)
        int wOldNum { 0 }; // must be 4
        int wFormat { 0 }; // hibyte > 9: v3.00 file, hibyte > 10: may no longer match v3.00 spec
        std::string title;
        std::string subTitle;
        std::string comment;
        std::string notes;
        std::string internetLink;
        std::string copyright;
    };

    struct TefInstrument {
        int stringNumber { 0 };
        int firstString { 0 };
        int available16U { 0 };
        int verticalSpacing { 0 };
        int midiVoice { 0 };
        int midiBank { 0 };
        int nBanjo5 { 0 };
        int uSpec { 0 };
        int nCapo { 0 };
        int fMiddleC { 0 };
        int fClef { 0 };
        int output { 0 };
        int options { 0 };
        array<int, 12> tuning = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        std::string name;
    };

    struct TefMeasure {
        int flag { 0 };
        int key { 0 };
        int size { 0 };
        int numerator { 0 };
        int denominator { 0 };
    };

    struct TefNote {
        int position { 0 };
        int string { 0 };
        int fret { 0 };
        bool tie { false };
        bool rest { false };    // this is a bit of a hack
        int duration { 0 };
        int length { 0 };
        int dots { 0 };
        bool triplet { false };
        int voice { 0 };        // 0: default, 2: upper, 3: lower
        bool hasGrace { false };
        int graceEffect{ -1 }; // invalid
        int graceFret { -1 }; // invalid
    };

    struct TefTextMarker {
        int position { 0 };
        int string { 0 };
        int index { 0 };
    };

    class VoiceAllocator
    {
    public:
        void addColumn(const vector<const TefNote* const>& column);
        void addNote(const TefNote* const note, const bool preferVoice0);
        void allocateVoice(const TefNote* const note, int voice);
        bool canAddTefNoteToVoice(const TefNote* const note, const int voice);
        void dump();
        int findFirstPossibleVoice(const TefNote* const note, const array<int, 3> voices);
        int stopPosition(const size_t voice);
        int voice(const TefNote* const note);
        const vector<vector<const TefNote*>>& voiceContent(int voice) const { return voiceContents.at(voice); }

    private:
        void appendNoteToVoice(const TefNote* const note, int voice);
        map<const TefNote*, int> allocations;
        array<const TefNote*, mu::engraving::VOICES> notesPlaying = { nullptr, nullptr, nullptr, nullptr };
        array<vector<vector<const TefNote*>>, mu::engraving::VOICES> voiceContents;
    };

    class TupletHandler
    {
    public:
        engraving::Fraction doTuplet(const TefNote* const tefNote); // todo rename
        void addCr(engraving::Measure* measure, engraving::ChordRest* cr);
    private:
        int count {0};  // support overly simple algorithm: simply count notes
        bool inTuplet {false};
        int totalLength {0}; // sum of note duration in TablEdit units
        engraving::Tuplet* tuplet {nullptr};
    };

    void allocateVoices(vector<VoiceAllocator>& allocator);
    void createContents();
    void createMeasures();
    void createNotesFrame();
    void createParts();
    void createProperties();
    void createScore();
    void createTempo();
    void createTitleFrame();
    void initializeVoiceAllocators(vector<VoiceAllocator>& allocators);
    engraving::part_idx_t partIdx(size_t stringIdx, bool& ok) const;
    int stringNumberPreviousParts(engraving::part_idx_t partIdx) const;
    void readTefContents();
    void readTefHeader();
    void readTefInstruments();
    void readTefMeasures();
    void readTefTexts();


    TefHeader tefHeader;
    vector<TefTextMarker> tefTextMarkers;
    vector<TefNote> tefContents; // notes (and rests) only
    vector<TefInstrument> tefInstruments;
    vector<TefMeasure> tefMeasures;
    vector<string> tefTexts;

public:
    TablEdit(muse::io::IODevice* f, mu::engraving::MasterScore* s)
        : _file(f), score(s) {}
    mu::engraving::Err import();
};

} // namespace mu::iex::tabledit

#endif // MU_IMPORTEXPORT_IMPORTTEF_H
