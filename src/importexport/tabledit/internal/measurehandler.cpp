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

#include "measurehandler.h"

using namespace mu::engraving;
namespace mu::iex::tabledit {
MeasureHandler::MeasureHandler()
{
    LOGD("construcor");
}

void MeasureHandler::calculateMeasureStarts(const std::vector<TefMeasure>& tefMeasures)
{
    int measureStart { 0 };
    for (size_t i = 0; i < tefMeasures.size(); ++i) {
        measureStarts.push_back(measureStart);
        measureStart += 64 * tefMeasures.at(i).numerator / tefMeasures.at(i).denominator;
    }
    std::string s;
    for (const auto start : measureStarts) {
        s += ' ';
        s += std::to_string(start);
    }
    LOGD("measureStarts %s", s.c_str());
}
} // namespace mu::iex::tabledit
