//=============================================================================
//  MuseScore
//  Linux Music Score Editor
//
//  Copyright (C) 2017 Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================

#include "musescore.h"

namespace Ms {
      
      //---------------------------------------------------------
      //   importBww
      //---------------------------------------------------------
      
      Score::FileError importMnx(MasterScore* score, const QString& path)
      {
            qDebug("Score::importMnx(%s)", qPrintable(path));
            
/*
            QFile fp(path);
            if(!fp.exists())
                  return Score::FileError::FILE_NOT_FOUND;
            if (!fp.open(QIODevice::ReadOnly))
                  return Score::FileError::FILE_OPEN_ERROR;
            
            QString id("importMnx");
            Part* part = new Part(score);
            part->setId(id);
            score->appendPart(part);
            Staff* staff = new Staff(score);
            staff->setPart(part);
            part->staves()->push_back(staff);
            score->staves().push_back(staff);
            
            Bww::Lexer lex(&fp);
            Bww::MsScWriter wrt;
            wrt.setScore(score);
            score->style().set(StyleIdx::measureSpacing, 1.0);
            Bww::Parser p(lex, wrt);
            p.parse();
            
            score->setSaved(false);
            score->setCreated(true);
            score->connectTies();
 */
            qDebug("Score::importMnx() done");
            return Score::FileError::FILE_NO_ERROR;      // OK
      }
      
} // namespace Ms
