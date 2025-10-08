/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited
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

#include <string>

#include "measurehandler.h"

using namespace mu::engraving;
namespace mu::iex::tabledit {
MeasureHandler::MeasureHandler()
{
    LOGD("construcor");
}

// return the actual size of measure idx

int MeasureHandler::actualSize(const std::vector<TefMeasure>& tefMeasures, const size_t idx) const
{
    int size { 64 * tefMeasures.at(idx).numerator / tefMeasures.at(idx).denominator };
    size -= gapsLeft.at(idx) + gapsRight.at(idx);
    LOGD("idx %zu size %d", idx, size);
    return size;
}

void MeasureHandler::calculateMeasureStarts(const std::vector<TefMeasure>& tefMeasures)
{
    int measureStart { 0 };
    for (size_t i = 0; i < tefMeasures.size(); ++i) {
        measureStarts.push_back(measureStart);
        int measureSize { 64 * tefMeasures.at(i).numerator / tefMeasures.at(i).denominator };
        measureStart += measureSize;
        gapsLeft.push_back(measureSize);
        gapsRight.push_back(measureSize);
    }

    for (size_t i = 0; i < tefMeasures.size(); ++i) {
    }
    std::string s;
    for (const auto start : measureStarts) {
        s += ' ';
        s += std::to_string(start);
    }
    LOGD("measureStarts (nominal) %s", s.c_str());
    s.clear();
    for (const auto gapLeft : gapsLeft) {
        s += ' ';
        s += std::to_string(gapLeft);
    }
    LOGN("gapsLeft %s", s.c_str());
    s.clear();
    for (const auto gapRight : gapsRight) {
        s += ' ';
        s += std::to_string(gapRight);
    }
    LOGN("gapsRight %s", s.c_str());
}

// return the index of the measure containing tstart
// note O2 behaviour in score size
int MeasureHandler::measureIndex(int tstart, const std::vector<TefMeasure>& tefMeasures) const
{
    for (size_t i = 0; i < tefMeasures.size(); ++i) {
        auto start { measureStarts.at(i) };
        auto size { 64 * tefMeasures.at(i).numerator / tefMeasures.at(i).denominator };
        if (start <= tstart && tstart < start + size) {
            return i;
        }
    }
    return -1; // not found
}

// return the offset of tstart (distance from its measure's start)
int MeasureHandler::offsetInMeasure(int tstart, const std::vector<TefMeasure>& tefMeasures)
{
    auto index { measureIndex(tstart, tefMeasures) };
    if (0 <= index) {
        return tstart - measureStarts.at(index);
    }
    return -1; // not found
}

void MeasureHandler::updateGapLeft(std::vector<int>& gapLeft, const int position, const std::vector<TefMeasure>& tefMeasures)
{
    auto index { measureIndex(position, tefMeasures) };
    auto offset { offsetInMeasure(position, tefMeasures) };
    if (0 <= index && 0 <= offset) {
        if (offset < gapLeft[index]) {
            gapLeft[index] = offset;
        }
    }
    return;
}

// return TablEdit note length in 64th (including triplets rounded down to nearest note length)
// TODO: remove code duplication with importtef.cpp duration2length()

//static int durationToInt(uint8_t duration) // todo fix conflict: duplicated code with voiceallocator.cpp
static int durationToInt2(uint8_t duration)
{
    switch (duration) {
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
}

void MeasureHandler::updateGapRight(std::vector<int>& gapRight, const TefNote& note, const std::vector<TefMeasure>& tefMeasures)
{
    auto pos { note.position };
    auto index { measureIndex(pos, tefMeasures) };
    auto offset { offsetInMeasure(pos, tefMeasures) };
    LOGN("pos %d index %d offset %d", pos, index, offset);
    if (0 <= index && 0 <= offset) {
        auto dur { durationToInt2(note.duration) };
        auto end { offset + dur };
        auto size { 64 * tefMeasures.at(index).numerator / tefMeasures.at(index).denominator };
        auto gap { size - end };
        LOGN("dur %d end %d size %d gap %d", dur, end, size, gap);
        if (gap < gapRight[index]) {
            gapRight[index] = gap;
        }
    }
    return;
}

// start time correction to be subtracted from note position due to gaps:
// sum of gaps in previous measure(s) plus left gap in current measure

int MeasureHandler::sumPreviousGaps(const size_t idx) const
{
    auto corr { 0 };
    for (unsigned int j = 0; j < idx; ++j) {
        corr += gapsLeft.at(j) + gapsRight.at(j);
    }
    corr += gapsLeft.at(idx);
    LOGD("idx %zu corr %d", idx, corr);
    return corr;
}

void MeasureHandler::updateGaps(const std::vector<TefNote>& tefContents, const std::vector<TefMeasure>& tefMeasures)
{
    for (const TefNote& note : tefContents) {
        updateGapLeft(gapsLeft, note.position, tefMeasures);
        updateGapRight(gapsRight, note, tefMeasures);
    }

    std::string s;
    for (const auto gapLeft : gapsLeft) {
        s += ' ';
        s += std::to_string(gapLeft);
    }
    LOGD("gapsLeft %s", s.c_str());

    s.clear();
    for (const auto gapRight : gapsRight) {
        s += ' ';
        s += std::to_string(gapRight);
    }
    LOGD("gapsRight %s", s.c_str());

    s.clear();
    for (unsigned int i = 0; i < tefMeasures.size(); ++i) {
        auto size { 64 * tefMeasures.at(i).numerator / tefMeasures.at(i).denominator };
        s += ' ';
        s += std::to_string(size);
    }
    LOGD("nominal measure size %s", s.c_str());

    s.clear();
    for (unsigned int i = 0; i < tefMeasures.size(); ++i) {
        int pickup { tefMeasures.at(i).isPickup ? 1 : 0 };
        s += ' ';
        s += std::to_string(pickup);
    }
    LOGD("isPickup %s", s.c_str());

    s.clear();
    for (unsigned int i = 0; i < tefMeasures.size(); ++i) {
        auto size { 64 * tefMeasures.at(i).numerator / tefMeasures.at(i).denominator };
        s += ' ';
        s += std::to_string(size - gapsLeft.at(i) - gapsRight.at(i));
    }
    LOGD("actual measure size %s", s.c_str());

    s.clear();
    {
        int measureStart { 0 };
        for (unsigned int i = 0; i < tefMeasures.size(); ++i) {
            int measureSize { 64 * tefMeasures.at(i).numerator / tefMeasures.at(i).denominator };
            s += ' ';
            s += std::to_string(measureStart);
            measureStart += measureSize;
            measureStart -= gapsLeft.at(i) + gapsRight.at(i);
        }
    }
    LOGD("actual measure start %s", s.c_str());

    s.clear();
    for (unsigned int i = 0; i < tefMeasures.size(); ++i) {
        // sum of gaps in previous measure(s) plus left gap in current measure
        auto corr { 0 };
        for (unsigned int j = 0; j < i; ++j) {
            corr += gapsLeft.at(j) + gapsRight.at(j);
        }
        corr += gapsLeft.at(i);
        s += ' ';
        s += std::to_string(corr);
    }
    LOGD("note time correction %s", s.c_str());
}
} // namespace mu::iex::tabledit
