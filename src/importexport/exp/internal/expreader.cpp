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

#include "engraving/dom/score.h"
#include "engraving/engravingerrors.h"

using namespace mu::iex::exp;
using namespace mu::engraving;

namespace mu::iex::exp {
extern Err importExp(MasterScore* score, const QString& name);
}

muse::Ret NotationExpReader::read(MasterScore* score, const muse::io::path_t& path, const Options&)
{
    LOGD("path %s", muPrintable(path.toString()));
    Err err = Err::FileUnknownType; // TODO
    return make_ret(err, path);
}
