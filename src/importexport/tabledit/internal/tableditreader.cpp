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

#include "engraving/dom/score.h"
#include "engraving/engravingerrors.h"
#include "io/file.h"

#include "importtef.h"
#include "tableditreader.h"

using namespace mu::iex::tabledit;
using namespace mu::engraving;

namespace mu::iex::tabledit {
extern Err importTablEdit(MasterScore* score, const QString& name);
}

muse::Ret TablEditReader::read(MasterScore* score, const muse::io::path_t& path, const Options&)
{
    LOGD("path %s", muPrintable(path.toString()));
    Err err = import(score, path);
    return make_ret(err, path);
}


Err TablEditReader::import(MasterScore* score, const muse::io::path_t& path, const Options& options)
{
    LOGD("begin import");
    if (!fileSystem()->exists(path)) {
        return Err::FileNotFound;
    }

    muse::io::File file(path);
    if (!file.open(muse::io::IODevice::ReadOnly)) {
        return Err::FileOpenError;
    }
    TablEdit tablEdit{&file, score};
    Err err = tablEdit.import();

    return err;
}
