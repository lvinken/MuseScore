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
#include "expreader.h"

#include "engraving/dom/chord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/score.h"
#include "engraving/dom/timesig.h"
#include "engraving/engravingerrors.h"

using namespace mu::iex::exp;
using namespace mu::engraving;

namespace mu::iex::exp {
extern Err importExp(MasterScore* score, const QString& name);
}

static Err hello(MasterScore* score)
{
    Part* part = new Part(score);
    score->appendPart(part);

    Staff* staff = Factory::createStaff(part);
    score->appendStaff(staff);

    auto measure = Factory::createMeasure(score->dummy()->system());
    Fraction length{4, 4};
    Fraction tick{0, 4};
    measure->setTick(tick);
    measure->setTimesig(length);
    measure->setTicks(length);
    measure->setEndBarLineType(BarLineType::NORMAL, 0);
    score->measures()->add(measure);

    auto s1 = measure->getSegment(mu::engraving::SegmentType::TimeSig, tick);
    mu::engraving::TimeSig* timesig = Factory::createTimeSig(s1);
    timesig->setSig(length);
    timesig->setTrack(0);
    s1->add(timesig);

    // create chord
    mu::engraving::Chord* cr = Factory::createChord(score->dummy()->segment());
    cr->setTrack(0);
    cr->setDurationType(DurationType::V_WHOLE);
    cr->setTicks(length);
    // add note to chord
    mu::engraving::Note* note = Factory::createNote(cr);
    note->setTrack(0);
    int pitch {60};
    note->setPitch(pitch);
    note->setTpcFromPitch(Prefer::NEAREST);
    cr->add(note);
    // add chord to measure
    auto s2 = measure->getSegment(mu::engraving::SegmentType::ChordRest, tick);
    s2->add(cr);

    return Err::NoError;
}

muse::Ret NotationExpReader::read(MasterScore* score, const muse::io::path_t& path, const Options&)
{
    LOGD("path %s", muPrintable(path.toString()));
    Err err = hello(score);
    return make_ret(err, path);
}
