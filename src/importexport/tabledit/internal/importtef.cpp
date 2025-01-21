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

#include "log.h"

using namespace mu::engraving;

namespace mu::iex::tabledit {


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
// input is the offset where the text's offset is stored

string TablEdit::readText(uint32_t offsetOffset)
{
    string result;
    _file->seek(offsetOffset);
    uint32_t offset = readUInt32();
    _file->seek(offset);
    uint16_t size = readUInt16();
    LOGD("offset %d size %d", offset, size);
    for (uint16_t i = 0; i < size - 1; ++i) {
        auto c = readUInt8();
        if (0x20 <= c && c <= 0x7E) {
            result += static_cast<char>(c);
        }
        else {
            result += '.';
        }
    }
    return result;
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
    tefHeader.title = readText(0x40);
    tefHeader.subTitle = readText(0x44);
    tefHeader.comment = readText(0x48);
    tefHeader.notes = readText(0x4c);
    tefHeader.copyright = readText(0x8c);
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
    //if (!readVersion()) {
        return Err::FileBadFormat;
    //}
    return Err::NoError;
}

} // namespace mu::iex::tabledit
