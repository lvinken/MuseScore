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

#include "libmscore/box.h"
#include "libmscore/chord.h"
#include "libmscore/durationtype.h"
#include "libmscore/measure.h"
#include "libmscore/part.h"
#include "libmscore/staff.h"
#include "musescore.h"
#include "importmnx.h"

namespace Ms {

//---------------------------------------------------------
//   MnxParser definition
//---------------------------------------------------------

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
      Fraction event(Measure* measure, const Fraction sTime);
      void head();
      void identification();
      void logDebugTrace(const QString& info);
      void logDebugInfo(const QString& info);
      void logError(const QString& error);
      void lyric();
      void measure();
      void mnx();
      Note* note();
      Score::FileError parse();
      void part();
      void rest();
      void score();
      void sequence(Measure* measure, const Fraction sTime);
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
      int _beats;       ///< number of beats
      int _beatType;       ///< beat type
      QString _title;
      };

//---------------------------------------------------------
//   MnxParser constructor
//---------------------------------------------------------

MnxParser::MnxParser(Score* score)
      : _score(score),
      _beats(4),
      _beatType(4)
      {
      // nothing
      }

//---------------------------------------------------------
//   importMnxFromBuffer
//---------------------------------------------------------

/**
 The public interface of this module: import the MNX document dev into score
 */

Score::FileError importMnxFromBuffer(Score* score, const QString& /*name*/, QIODevice* dev)
      {
      //qDebug("importMnxFromBuffer(score %p, name '%s', dev %p)",
      //       score, qPrintable(name), dev);

      // TODO move temporary part / staff creation
      QString id("importMnx");
      auto part = new Part(score);
      part->setId(id);
      score->appendPart(part);
      auto staff = new Staff(score);
      staff->setPart(part);
      part->staves()->push_back(staff);
      score->staves().push_back(staff);
      // end TODO

      MnxParser p(score);
      p.parse(dev);

      score->setSaved(false);
      score->setCreated(true);
      score->connectTies();
      qDebug("done");
      return Score::FileError::FILE_NO_ERROR;                        // OK
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
//   forward references
//---------------------------------------------------------

static TDuration mnxEventValueToTDuration(const QString& value);
static int mnxToMidiPitch(const QString& value);

//---------------------------------------------------------
//   addVBoxWithTitle
//---------------------------------------------------------

/**
 Add a vbox containing the title to the score.
 */

static void addVBoxWithTitle(Score* score, const QString& title)
      {
      VBox* vbox = new VBox(score);
      Text* text = new Text(SubStyle::TITLE, score);
      text->setPlainText(title);
      vbox->add(text);
      vbox->setTick(0);
      score->measures()->add(vbox);
      }

//---------------------------------------------------------
//   addFirstMeasure
//---------------------------------------------------------

/**
 Add the first measure to the score.
 */

static void addFirstMeasure(Score* score, const int bts, const int bttp)
      {
      auto m = new Measure(score);
      m->setTick(0);
      m->setTimesig(Fraction(bts, bttp));
      m->setNo(1);
      m->setLen(Fraction(bts, bttp));
      score->measures()->add(m);
      // clef (TODO remove temporary code)
      // note that this results in a double clef
      /*
      auto clef = new Clef(score);
      clef->setClefType(ClefType::G);
      clef->setTrack(0);
      auto s = m->getSegment(SegmentType::Clef, 0);
      s->add(clef);
       */
      // timesig
      auto timesig = new TimeSig(score);
      timesig->setSig(Fraction(bts, bttp));
      timesig->setTrack(0);
      auto s = m->getSegment(SegmentType::TimeSig, 0);
      s->add(timesig);
      }

//---------------------------------------------------------
//   createChord
//---------------------------------------------------------

Chord* createChord(Score* score, const QString& value)
      {
      auto dur = mnxEventValueToTDuration(value);
      auto chord = new Chord(score);
      chord->setTrack(0);       // TODO
      chord->setDurationType(dur);
      chord->setDuration(dur.fraction());
      chord->setDots(dur.dots());
      //return nullptr;
      return chord;
      }

//---------------------------------------------------------
//   createNote
//---------------------------------------------------------

Note* createNote(Score* score, const QString& pitch)
      {
      auto note = new Note(score);
      note->setTrack(0);       // TODO
      note->setPitch(mnxToMidiPitch(pitch));       // TODO
      note->setTpcFromPitch();       // TODO
      //return nullptr;
      return note;
      }

//---------------------------------------------------------
//   setType
//---------------------------------------------------------

static TDuration::DurationType mnxValueUnitToDurationType(const QString& s)
      {
      if (s == "4")
            return TDuration::DurationType::V_QUARTER;
      else if (s == "8")
            return TDuration::DurationType::V_EIGHTH;
      else if (s == "1024")
            return TDuration::DurationType::V_1024TH;
      else if (s == "512")
            return TDuration::DurationType::V_512TH;
      else if (s == "256")
            return TDuration::DurationType::V_256TH;
      else if (s == "128")
            return TDuration::DurationType::V_128TH;
      else if (s == "64")
            return TDuration::DurationType::V_64TH;
      else if (s == "32")
            return TDuration::DurationType::V_32ND;
      else if (s == "16")
            return TDuration::DurationType::V_16TH;
      else if (s == "2")
            return TDuration::DurationType::V_HALF;
      else if (s == "1")
            return TDuration::DurationType::V_WHOLE;
      else if (s == "breve")
            return TDuration::DurationType::V_BREVE;
      else if (s == "long")
            return TDuration::DurationType::V_LONG;
      /*
      else if (s == "measure")
            return TDuration::DurationType::V_MEASURE;
       */
      else {
            qDebug("mnxValueUnitToDurationType(%s): unknown", qPrintable(s));
            return TDuration::DurationType::V_INVALID;
            // _val = DurationType::V_QUARTER;
            }
      }

static TDuration mnxEventValueToTDuration(const QString& value)
      {
      int dots = 0;
      QString valueWithoutDots = value;

      while (valueWithoutDots.endsWith('*')) {
            ++dots;
            valueWithoutDots.resize(valueWithoutDots.size() - 1);
            }

      auto val = mnxValueUnitToDurationType(valueWithoutDots);

      TDuration res(val);
      res.setDots(dots);

      return res;
      }

static int mnxToMidiPitch(const QString& value)
      {
      if (value.size() < 2) {
            qDebug("mnxToMidiPitch invalid value '%s'", qPrintable(value));
            return -1;
            }

      QString steps("C_D_EF_G_A_B");
      auto stepChar = value.at(0);
      auto step = steps.indexOf(stepChar);

      auto altOct = value.right(value.length() - 1);
      bool ok = false;
      auto oct = altOct.toInt(&ok);

      if (stepChar != '_' && step > -1 && ok)
            return step + (oct + 1) * 12;

      return -1;
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

Fraction MnxParser::event(Measure* measure, const Fraction sTime)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "event");
      logDebugTrace("MnxParser::event");

      QString value = _e.attributes().value("value").toString();
      logDebugTrace(QString("event value '%1'").arg(value));

      ChordRest* cr = createChord(_score, value);

      while (_e.readNextStartElement()) {
            if (_e.name() == "lyric")
                  lyric();
            else if (_e.name() == "note") {
                  cr->add(note());
                  }
            else if (_e.name() == "rest") {
                  rest();
                  }
            else
                  skipLogCurrElem();
            }

      auto s = _score->firstMeasure()->getSegment(SegmentType::ChordRest, sTime.ticks());
      s->add(cr);

            return cr->actualFraction();
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
            else if (_e.name() == "sequence") {
                  sequence(_score->firstMeasure() /* TODO */, Fraction(0, 1));
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

Note* MnxParser::note()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "note");
      logDebugTrace("MnxParser::note");

      QString pitch = _e.attributes().value("pitch").toString();
      logDebugTrace(QString("- note pitch '%1'").arg(pitch));

      // TODO _e.readNext();
      _e.skipCurrentElement();

      return createNote(_score, pitch);
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
                  auto partName = _e.readElementText();
                  logDebugTrace(QString("part-name '%1'").arg(partName));
                  auto part = _score->staff(0)->part(); // TODO
                  part->setPlainLongName(partName);
                  part->setPartName(partName);
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

void MnxParser::sequence(Measure* measure, const Fraction sTime)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "sequence");
      logDebugTrace("MnxParser::sequence");

            Fraction seqTime(0, 1); // time in this sequence
            
      while (_e.readNextStartElement()) {
            if (_e.name() == "event")
                  seqTime += event(measure, sTime + seqTime);
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

      addFirstMeasure(_score, _beats, _beatType);
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

      _title = _e.readElementText();
      logDebugTrace(QString("title '%1'").arg(_title));
      if (_title != "")
            addVBoxWithTitle(_score, _title);
      }

//---------------------------------------------------------
//   logDebugTrace
//---------------------------------------------------------

/**
 Log debug (function) trace.
 */

void MnxParser::logDebugTrace(const QString& /* info */)
      {
      //qDebug("Trace %s", qPrintable(info));
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

} // namespace Ms
