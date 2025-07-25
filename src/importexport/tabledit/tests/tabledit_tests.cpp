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

#include <gtest/gtest.h>

#include "engraving/engravingerrors.h"
#include "engraving/dom/masterscore.h"

#include "engraving/tests/utils/scorecomp.h"
#include "engraving/tests/utils/scorerw.h"

#include "project/inotationreader.h"

#include "importexport/tabledit/internal/tableditreader.h"

using namespace mu;
using namespace mu::engraving;

static const String TABLEDIT_DIR("data/");

class TablEdit_Tests : public ::testing::Test
{
public:
    void tefReadTest(const char* file);
};

//---------------------------------------------------------
//   tefReadTest
//   read a TablEdit file, write to a MuseScore file and verify against reference
//---------------------------------------------------------

void TablEdit_Tests::tefReadTest(const char* file)
{
    auto importFunc = [](MasterScore* score, const muse::io::path_t& path) -> engraving::Err {
        mu::iex::tabledit::TablEditReader tablEditReader;
        return tablEditReader.import(score, path);
    };

    String fileName = String::fromUtf8(file);
    MasterScore* score = ScoreRW::readScore(TABLEDIT_DIR + fileName + ".tef", false, importFunc);
    EXPECT_TRUE(score);
    score->setMetaTag(u"originalFormat", u"tef");

    EXPECT_TRUE(ScoreComp::saveCompareScore(score, fileName + u".mscx", TABLEDIT_DIR + fileName + u".mscx"));
    delete score;
}

TEST_F(TablEdit_Tests, tefTest1) {
    tefReadTest("metadata");
}
