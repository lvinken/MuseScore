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
#include "importmnx.h"

namespace Ms {

class MnxParser
      {
public:
      MnxParser(Score* score);
      Score::FileError parse(QIODevice* device);

private:
      // functions
      void attributes();
      void beam();
      void clef();
      void event();
      void head();
      void identification();
      void logDebugTrace(const QString& info);
      void logDebugInfo(const QString& info);
      void logError(const QString& error);
      void lyric();
      void measure();
      void mnx();
      void note();
      Score::FileError parse();
      void part();
      void rest();
      void score();
      void sequence();
      void skipLogCurrElem();
      void staff();
      void system();
      void tempo();
      void time();
      void title();

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
//   attributes
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure/attributes node.
 */

void MnxParser::attributes()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "attributes");
      logDebugTrace("MnxParser::attributes");

      while (_e.readNextStartElement()) {
            if (_e.name() == "staff")
                  staff();
            else if (_e.name() == "tempo") {
                  skipLogCurrElem();
                  }
            else if (_e.name() == "time") {
                  time();
                  }
            else
                  skipLogCurrElem();
            }
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

      int beamNo = _e.attributes().value("number").toInt();

      if (beamNo == 1) {
            QString s = _e.readElementText();
            if (s == "begin")
                  ;       // TODOD beamMode = Beam::Mode::BEGIN;
            else if (s == "end")
                  ;       // TODOD beamMode = Beam::Mode::END;
            else if (s == "continue")
                  ;       // TODOD beamMode = Beam::Mode::MID;
            else if (s == "backward hook")
                  ;
            else if (s == "forward hook")
                  ;
            else
                  logError(QString("unknown beam keyword '%1'").arg(s));
            }
      else
            _e.skipCurrentElement();
      }

//---------------------------------------------------------
//   clef
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure/attributes/staff/clef node.
 */

void MnxParser::clef()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "clef");
      logDebugTrace("MnxParser::clef");

      QString sign = _e.attributes().value("sign").toString();
      QString line = _e.attributes().value("line").toString();
      logDebugTrace(QString("clef sign '%1' line '%2'").arg(sign).arg(line));
      _e.skipCurrentElement();
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

      QString value = _e.attributes().value("value").toString();
      logDebugTrace(QString("event value '%1'").arg(value));

      while (_e.readNextStartElement()) {
            if (_e.name() == "lyric")
                  lyric();
            else if (_e.name() == "note") {
                  note();
                  }
            else if (_e.name() == "rest") {
                  rest();
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
                  identification();
            else if (_e.name() == "style") {
                  skipLogCurrElem();
                  }
            else
                  skipLogCurrElem();
            }
      }

//---------------------------------------------------------
//   identification
//---------------------------------------------------------

/**
 Parse the /mnx/head/identification node.
 */

void MnxParser::identification()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "identification");
      logDebugTrace("MnxParser::identification");


      while (_e.readNextStartElement()) {
            if (_e.name() == "title")
                  title();
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
            /* TODO
            if (_e.name() == "measure")
                  measure();
            else if (_e.name() == "part-name") {
                  logDebugTrace(QString("part-name '%1'").arg(_e.readElementText()));
                  }
            else
             */
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
                  attributes();
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

      QString pitch = _e.attributes().value("pitch").toString();
      logDebugTrace(QString("- note pitch '%1'").arg(pitch));

      // TODO _e.readNext();
      _e.skipCurrentElement();
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
//   rest
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure/sequence/event/rest node.
 */

void MnxParser::rest()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "rest");
      logDebugTrace("MnxParser::rest");

      logDebugTrace(QString("- rest"));

      // TODO _e.readNext();
      _e.skipCurrentElement();

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
                  system();
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
//   staff
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure/attributes/staff node.
 */

void MnxParser::staff()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "staff");
      logDebugTrace("MnxParser::staff");

      while (_e.readNextStartElement()) {
            if (_e.name() == "clef")
                  clef();
            else
                  skipLogCurrElem();
            }
      }

//---------------------------------------------------------
//   system
//---------------------------------------------------------

/**
 Parse the /mnx/score/system node.
 */

void MnxParser::system()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "system");
      logDebugTrace("MnxParser::system");

      while (_e.readNextStartElement()) {
            if (_e.name() == "measure")
                  measure();
            else
                  skipLogCurrElem();
            }
      }

//---------------------------------------------------------
//   time
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure/attributes/time node.
 */

void MnxParser::time()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "time");
      logDebugTrace("MnxParser::time");

      QString signature = _e.attributes().value("signature").toString();
      logDebugTrace(QString("time-sig '%1'").arg(signature));
      _e.skipCurrentElement();
      }

//---------------------------------------------------------
//   title
//---------------------------------------------------------

/**
 Parse the /mnx/head/identification/title node.
 */

void MnxParser::title()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "title");
      logDebugTrace("MnxParser::title");

      logDebugTrace(QString("title '%1'").arg(_e.readElementText()));
      }

//---------------------------------------------------------
//   importMnxFromBuffer
//---------------------------------------------------------

Score::FileError importMnxFromBuffer(Score* score, const QString& /*name*/, QIODevice* dev)
      {
      //qDebug("importMnxFromBuffer(score %p, name '%s', dev %p)",
      //       score, qPrintable(name), dev);

      MnxParser p(score);
      p.parse(dev);

      score->setSaved(false);
      score->setCreated(true);
      score->connectTies();
      qDebug("done");
      return Score::FileError::FILE_NO_ERROR;                  // OK
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

      return importMnxFromBuffer(score, path, &fp);
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
      }

} // namespace Ms
