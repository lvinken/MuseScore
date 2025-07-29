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

#include "tuplethandler.h"

#include "engraving/dom/factory.h"
#include "engraving/dom/tuplet.h"
#include "engraving/types/fraction.h"

using namespace std;
using namespace mu::engraving;
namespace mu::iex::tabledit {

// start/stop tuplet and calculate timing correction
// for a triplet of quarters, the length of each note is 2/3 * 1/4 = 1/6
// TablEdit uses 1/8 instead, required next note position correction
// is 1/6 - 1/8 = 4/24 - 3/24 = 1/24

Fraction TupletHandler::doTuplet(const TefNote* const tefNote)
{
    Fraction res {0, 1};
    Fraction correction {tefNote->length, 64};
    correction *= {1, 6};
    LOGD("position %d string %d fret %d length %d triplet %d",
         tefNote->position, tefNote->string, tefNote->fret, tefNote->length, tefNote->triplet);
    LOGD("before inTuplet %d count %d totalLength %d", inTuplet, count, totalLength);
    if (tefNote->triplet) {
        if (!inTuplet) {
            LOGD("start triplet");
        }
        inTuplet = true;
        res = Fraction {totalLength, 2 * 3 * 64};
        LOGD("res %d/%d", res.numerator(), res.denominator());
        ++count;
        totalLength += tefNote->length;
        LOGD("totalLength %d", totalLength);
    }
    if (!tefNote->triplet || (inTuplet && (totalLength % 3) == 0)) {
        if (inTuplet) {
            LOGD("stop triplet");
            const Fraction l {totalLength, 3 * 64};
            LOGD("baselen %d/%d", l.numerator(), l.denominator());
            tuplet->setBaseLen(l);
        }
        inTuplet = false;
        count = 0;
        totalLength = 0;
    }
    LOGD("after inTuplet %d count %d totalLength %d res %d/%d", inTuplet, count, totalLength, res.numerator(), res.denominator());
    return res;
}

// add ChordRest to tuplet
// needs measure, track and ratio

void TupletHandler::addCr(Measure* measure, ChordRest* cr)
{
    if (inTuplet && !tuplet) {
        tuplet = Factory::createTuplet(measure);
        LOGD("new tuplet %p cr ticks %d/%d", tuplet, cr->ticks().numerator(), cr->ticks().denominator());
        tuplet->setParent(measure);
        tuplet->setTrack(cr->track());
        tuplet->setRatio({3, 2});
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

} // namespace mu::iex::tabledit
