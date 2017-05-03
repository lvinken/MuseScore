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

#include "libmscore/part.h"
#include "libmscore/staff.h"
#include "musescore.h"

namespace Ms {

class MnxParser
      {
public:
      MnxParser(Score* score);
      Score::FileError parse(QIODevice* device);

private:
      // functions
      void beam();
      void event();
      void head();
      void logDebugTrace(const QString& info);
      void logDebugInfo(const QString& info);
      void logError(const QString& error);
      void lyric();
      void measure();
      void mnx();
      void note();
      Score::FileError parse();
      void part();
      void score();
      void sequence();
      void skipLogCurrElem();

      // data
      QXmlStreamReader _e;
      QString _parseStatus;                           ///< Parse status (typicallay a short error message)
      Score* _score;                                  ///< MuseScore score
      };

//---------------------------------------------------------
//   MusicXMLParserPass1
//---------------------------------------------------------

MnxParser::MnxParser(Score* score)
      : _score(score)
      {
      // nothing
      }

//---------------------------------------------------------
//   beam
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure/sequence/event/beam node.
 */

void MnxParser::beam()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "beam");
      logDebugTrace("MnxParser::beam");

      while (_e.readNextStartElement()) {
            if (_e.name() == "measure")
                  measure();
            else if (_e.name() == "part-name") {
                  logDebugTrace(QString("part-name '%1'").arg(_e.readElementText()));
                  }
            else
                  skipLogCurrElem();
            }
      }

//---------------------------------------------------------
//   event
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure/sequence/event node.
 */

void MnxParser::event()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "event");
      logDebugTrace("MnxParser::event");

      while (_e.readNextStartElement()) {
            if (_e.name() == "lyric")
                  lyric();
            else if (_e.name() == "note") {
                  note();
                  }
            else if (_e.name() == "rest") {
                  // TODO
                  }
            else
                  skipLogCurrElem();
            }
      }

//---------------------------------------------------------
//   head
//---------------------------------------------------------

/**
 Parse the /mnx/head node.
 */

void MnxParser::head()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "head");
      logDebugTrace("MnxParser::head");


      while (_e.readNextStartElement()) {
            if (_e.name() == "identification")
                  skipLogCurrElem();
            else if (_e.name() == "style") {
                  skipLogCurrElem();
                  }
            else
                  skipLogCurrElem();
            }
      }

//---------------------------------------------------------
//   logDebugTrace
//---------------------------------------------------------

/**
 Log debug (function) trace.
 */

void MnxParser::logDebugTrace(const QString& info)
      {
      qDebug("Trace %s", qPrintable(info));
      }

//---------------------------------------------------------
//   logDebugInfo
//---------------------------------------------------------

/**
 Log debug \a info (non-fatal events relevant for debugging).
 */

void MnxParser::logDebugInfo(const QString& info)
      {
      qDebug("Info at line %lld col %lld: %s",
             _e.lineNumber(), _e.columnNumber(), qPrintable(info));
      }

//---------------------------------------------------------
//   logError
//---------------------------------------------------------

/**
 Log \a error (possibly non-fatal but to be reported to the user anyway).
 */

void MnxParser::logError(const QString& error)
      {
      QString err;
      err = QString("Error at line %1 col %2: %3").arg(_e.lineNumber()).arg(_e.columnNumber()).arg(error);
      qDebug("%s", qPrintable(err));
      _parseStatus += err;
      }

//---------------------------------------------------------
//   lyric
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure/sequence/event/lyric node.
 */

void MnxParser::lyric()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "lyric");
      logDebugTrace("MnxParser::lyric");

      while (_e.readNextStartElement()) {
            if (_e.name() == "measure")
                  measure();
            else if (_e.name() == "part-name") {
                  logDebugTrace(QString("part-name '%1'").arg(_e.readElementText()));
                  }
            else
                  skipLogCurrElem();
            }
      }

//---------------------------------------------------------
//   measure
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure node.
 */

void MnxParser::measure()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "measure");
      logDebugTrace("MnxParser::measure");

      while (_e.readNextStartElement()) {
            if (_e.name() == "attributes")
                  skipLogCurrElem();
            /*
            else if (_e.name() == "sequence") {
                  skipLogCurrElem();
            }
             */
            else if (_e.name() == "sequence") {
                  sequence();
                  }
            else
                  skipLogCurrElem();
            }
      }

//---------------------------------------------------------
//   mnx
//---------------------------------------------------------

/**
 Parse the /mnx node.
 */

void MnxParser::mnx()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "mnx");
      logDebugTrace("MnxParser::mnx");


      while (_e.readNextStartElement()) {
            if (_e.name() == "head")
                  head();
            else if (_e.name() == "score") {
                  score();
                  }
            else
                  skipLogCurrElem();
            }
      }

//---------------------------------------------------------
//   note
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure/sequence/event/note node.
 */

void MnxParser::note()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "note");
      logDebugTrace("MnxParser::note");

      while (_e.readNextStartElement()) {
            if (_e.name() == "measure")
                  measure();
            else if (_e.name() == "part-name") {
                  logDebugTrace(QString("part-name '%1'").arg(_e.readElementText()));
                  }
            else
                  skipLogCurrElem();
            }
      }

//---------------------------------------------------------
//   part
//---------------------------------------------------------

/**
 Parse the /mnx/score/part node.
 */

void MnxParser::part()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "part");
      logDebugTrace("MnxParser::part");

      while (_e.readNextStartElement()) {
            if (_e.name() == "measure")
                  measure();
            else if (_e.name() == "part-name") {
                  logDebugTrace(QString("part-name '%1'").arg(_e.readElementText()));
                  }
            else
                  skipLogCurrElem();
            }
      }

//---------------------------------------------------------
//   score
//---------------------------------------------------------

/**
 Parse the /mnx/score node.
 */

void MnxParser::score()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "score");
      logDebugTrace("MnxParser::score");

      while (_e.readNextStartElement()) {
            if (_e.name() == "system")
                  skipLogCurrElem();
            else if (_e.name() == "part") {
                  part();
                  }
            else
                  skipLogCurrElem();
            }
      }

//---------------------------------------------------------
//   sequence
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure/sequence node.
 */

void MnxParser::sequence()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "sequence");
      logDebugTrace("MnxParser::sequence");

      while (_e.readNextStartElement()) {
            if (_e.name() == "event")
                  event();
            /*
             else if (_e.name() == "sequence") {
             skipLogCurrElem();
             }
            else if (_e.name() == "sequence") {
                  skipLogCurrElem();
            }
             */
            else
                  skipLogCurrElem();
            }
      }

//---------------------------------------------------------
//   skipLogCurrElem
//---------------------------------------------------------

/**
 Skip the current element, log debug as info.
 */

void MnxParser::skipLogCurrElem()
      {
      logDebugInfo(QString("skipping '%1'").arg(_e.name().toString()));
      _e.skipCurrentElement();
      }

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

/**
 Parse MNX in \a device and extract pass 1 data.
 */

Score::FileError MnxParser::parse(QIODevice* device)
      {
      logDebugTrace("MnxParser::parse device");
      //_parts.clear();
      _e.setDevice(device);
      Score::FileError res = parse();
      if (res != Score::FileError::FILE_NO_ERROR)
            return res;

      // Determine the start tick of each measure in the part
      /*
      determineMeasureLength(_measureLength);
      determineMeasureStart(_measureLength, _measureStart);
      createMeasures(_score, _measureLength, _measureStart);
       */
      return res;
      }

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

/**
 Start the parsing process, after verifying the top-level node is mnx
 */

Score::FileError MnxParser::parse()
      {
      logDebugTrace("MnxParser::parse");

      bool found = false;
      while (_e.readNextStartElement()) {
            if (_e.name() == "mnx") {
                  found = true;
                  mnx();
                  }
            else {
                  logError(QString("this is not an MNX file (top-level node '%1')")
                           .arg(_e.name().toString()));
                  _e.skipCurrentElement();
                  return Score::FileError::FILE_BAD_FORMAT;
                  }
            }

      if (!found) {
            logError("this is not an MNX file, node <mnx> not found");
            return Score::FileError::FILE_BAD_FORMAT;
            }

      return Score::FileError::FILE_NO_ERROR;
      }

//---------------------------------------------------------
//   importMnx
//---------------------------------------------------------

Score::FileError importMnx(MasterScore* score, const QString& path)
      {
      qDebug("Score::importMnx(%s)", qPrintable(path));

      QFile fp(path);
      if (!fp.exists())
            return Score::FileError::FILE_NOT_FOUND;
      if (!fp.open(QIODevice::ReadOnly))
            return Score::FileError::FILE_OPEN_ERROR;

      /*
      QString id("importMnx");
      Part* part = new Part(score);
      part->setId(id);
      score->appendPart(part);
      Staff* staff = new Staff(score);
      staff->setPart(part);
      part->staves()->push_back(staff);
      score->staves().push_back(staff);
       */

      /*
      Bww::Lexer lex(&fp);
      Bww::MsScWriter wrt;
      wrt.setScore(score);
      score->style().set(StyleIdx::measureSpacing, 1.0);
      Bww::Parser p(lex, wrt);
      p.parse();
       */

      MnxParser p(score);
      p.parse(&fp);

      score->setSaved(false);
      score->setCreated(true);
      score->connectTies();
      qDebug("Score::importMnx() done");
      return Score::FileError::FILE_NO_ERROR;            // OK
      }

} // namespace Ms
