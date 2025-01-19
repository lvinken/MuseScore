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
#include "engraving/engravingerrors.h"
#include "io/iodevice.h"

namespace mu::iex::tabledit {

class TablEdit
{
    muse::io::IODevice* _file = nullptr;
    mu::engraving::MasterScore* score = nullptr;

public:
    TablEdit(muse::io::IODevice* f, mu::engraving::MasterScore* s)
        : _file(f), score(s) {}
    mu::engraving::Err import();
};

} // namespace mu::iex::tabledit

#endif // MU_IMPORTEXPORT_IMPORTTEF_H

