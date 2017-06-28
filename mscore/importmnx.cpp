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
#include "libmscore/rest.h"
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
      void clef(const int track);
      void creator();
      Fraction event(Measure* measure, const Fraction sTime, const int seqNr);
      void head();
      void identification();
      bool inRealPart() const { return _inRealPart; }
      void logDebugTrace(const QString& info);
      void logDebugInfo(const QString& info);
      void logError(const QString& error);
      void lyric();
      void measure(const int measureNr);
      void mnx();
      Note* note(const int seqNr);
      Score::FileError parse();
      void part();
      Rest* rest(Score* score, const QString& value, const int seqNr);
      void score();
      void sequence(Measure* measure, const Fraction sTime, QVector<int>& staffSeqCount);
      void setInRealPart() { _inRealPart = true; }
      void skipLogCurrElem();
      void staff(const int staffNr);
      void subtitle();
      void system();
      void tempo();
      void time();
      void title();

      // data
      QXmlStreamReader _e;
      QString _parseStatus;                           ///< Parse status (typicallay a short error message)
      Score* _score;                                  ///< MuseScore score
      Part* _part;                                    ///< current part (TODO: remove ?)
      int _beats;                                     ///< number of beats
      int _beatType;                                  ///< beat type
      QString _composer;                              ///< metadata: composer
      QString _subtitle;                              ///< metadata: subtitle
      QString _title;                                 ///< metadata: title
      bool _inRealPart;
      };

//---------------------------------------------------------
//   MnxParser constructor
//---------------------------------------------------------

MnxParser::MnxParser(Score* score)
      : _score(score),
      _beats(4),
      _beatType(4),
      _inRealPart(false)
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
 Parse MNX in \a device.
 */

Score::FileError MnxParser::parse(QIODevice* device)
      {
      logDebugTrace("MnxParser::parse device");
      _e.setDevice(device);
      Score::FileError res = parse();
      if (res != Score::FileError::FILE_NO_ERROR)
            return res;

      // Do post-parse fixups (none at this time)
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

static Measure* addMeasure(Score* score, const int tick, const int bts, const int bttp, const int no);
static void addTimeSig(Score* score, const int tick, const int track, const int bts, const int bttp);
static int determineTrack(const Part* const part, const int staff, const int voice);
static TDuration mnxEventValueToTDuration(const QString& value);
static int mnxToMidiPitch(const QString& value, int& tpc);

//---------------------------------------------------------
//   score creation
//---------------------------------------------------------

//---------------------------------------------------------
//   addClef
//---------------------------------------------------------

/**
 Add a clef of type \a ct to the score.
 */

static void addClef(Score* score, const int tick, const int track, const ClefType ct)
      {
      auto clef = new Clef(score);
      clef->setClefType(ct);
      clef->setTrack(track);
      auto measure = score->tick2measure(tick);
      auto s = measure->getSegment(tick ? SegmentType::Clef : SegmentType::HeaderClef, tick);
      s->add(clef);
      }

//---------------------------------------------------------
//   addMetaData
//---------------------------------------------------------

/**
 Add (part of) the metadata to the score.
 */

static void addMetaData(Score* score, const QString& composer, const QString& subtitle, const QString& title)
      {
      if (!title.isEmpty()) score->setMetaTag("workTitle", title);
      if (!subtitle.isEmpty()) score->setMetaTag("workNumber", subtitle);
      if (!composer.isEmpty()) score->setMetaTag("composer", composer);
      }

//---------------------------------------------------------
//   addVBoxWithMetaData
//---------------------------------------------------------

/**
 Add a vbox containing (part of) the metadata to the score.
 */

static void addVBoxWithMetaData(Score* score, const QString& composer, const QString& subtitle, const QString& title)
      {
      if (!composer.isEmpty() || !subtitle.isEmpty() || !title.isEmpty()) {
            VBox* vbox = new VBox(score);
            if (!composer.isEmpty()) {
                  Text* text = new Text(SubStyle::COMPOSER, score);
                  text->setPlainText(composer);
                  vbox->add(text);
                  }
            if (!subtitle.isEmpty()) {
                  Text* text = new Text(SubStyle::SUBTITLE, score);
                  text->setPlainText(subtitle);
                  vbox->add(text);
                  }
            if (!title.isEmpty()) {
                  Text* text = new Text(SubStyle::TITLE, score);
                  text->setPlainText(title);
                  vbox->add(text);
                  }
            vbox->setTick(0);
            score->measures()->add(vbox);
            }
      }

//---------------------------------------------------------
//   addFirstMeasure
//---------------------------------------------------------

/**
 Add the first measure to the score.
 */

static Measure* addFirstMeasure(Score* score, const int bts, const int bttp)
      {
      const auto tick = 0;
      const auto nr = 1;
      auto m = addMeasure(score, tick, bts, bttp, nr);
      // timesig
      const int track = 0;
      addTimeSig(score, tick, track, bts, bttp);
      return m;
      }

//---------------------------------------------------------
//   addMeasure
//---------------------------------------------------------

/**
 Add a measure to the score.
 */

static Measure* addMeasure(Score* score, const int tick, const int bts, const int bttp, const int no)
      {
      auto m = new Measure(score);
      m->setTick(tick);
      m->setTimesig(Fraction(bts, bttp));
      m->setNo(no);
      m->setLen(Fraction(bts, bttp));
      score->measures()->add(m);
      return m;
      }

//---------------------------------------------------------
//   addTimeSig
//---------------------------------------------------------

/**
 Add a time signature to a track.
 */

static void addTimeSig(Score* score, const int tick, const int track, const int bts, const int bttp)
      {
      // timesig
      auto timesig = new TimeSig(score);
      timesig->setSig(Fraction(bts, bttp));
      timesig->setTrack(track);
      auto measure = score->tick2measure(tick);
      auto s = measure->getSegment(SegmentType::TimeSig, tick);
      s->add(timesig);
      }

//---------------------------------------------------------
//   appendPart
//---------------------------------------------------------

/**
 Append a new (single staff) part to the score.
 */

static Part* appendPart(Score* score, const int bts, const int bttp)
      {
      // create part and first staff
      QString id("importMnx");
      auto part = new Part(score);
      part->setId(id);
      score->appendPart(part);
      part->setStaves(1);

      // if not the first part, add the time signature
      const auto staff { 0 };
      const auto voice { 0 };
      auto track = determineTrack(part, staff, voice);
      if (track > 0) {
            const auto tick { 0 };
            addTimeSig(score, tick, track, bts, bttp);
            }

      return part;
      }

//---------------------------------------------------------
//   calculateMeasureStartTick
//---------------------------------------------------------

/**
 Calculate the start tick of measure \a no.
 TODO: assumes no timesig changes
 */

static int calculateMeasureStartTick(const int bts, const int bttp, const int no)
      {
      Fraction f(bts, bttp);
      return no * f.ticks();
      }

//---------------------------------------------------------
//   createChord
//---------------------------------------------------------

/*
 * Create a chord with duration \a value in track \a track.
 */

Chord* createChord(Score* score, const QString& value, const int track)
      {
      auto dur = mnxEventValueToTDuration(value);
      auto chord = new Chord(score);
      chord->setTrack(track);
      chord->setDurationType(dur);
      chord->setDuration(dur.fraction());
      chord->setDots(dur.dots());
      return chord;
      }

//---------------------------------------------------------
//   createNote
//---------------------------------------------------------

/*
 * Create a note with pitch \a pitch in track \a track.
 */

Note* createNote(Score* score, const QString& pitch, const int track)
      {
      auto note = new Note(score);
      note->setTrack(track);
      auto tpc2 = 0;
      auto msPitch = mnxToMidiPitch(pitch, tpc2);
      //note->setTpcFromPitch();       // TODO
      //int tpc1 = Ms::transposeTpc(tpc2, Interval(), true);
      note->setPitch(msPitch, tpc2, tpc2);
      return note;
      }

//---------------------------------------------------------
//   createRest
//---------------------------------------------------------

/*
 * Create a rest with duration \a value in track \a track.
 */

Rest* createRest(Score* score, const QString& value, const int track)
      {
      auto dur = mnxEventValueToTDuration(value);
      auto rest = new Rest(score, dur);
      rest->setTrack(track);
      return rest;
      }

//---------------------------------------------------------
//   determineTrack
//---------------------------------------------------------

/*
 * Calculate track from \a part, \a staff and \a voice.
 */

static int determineTrack(const Part* const part, const int staff, const int voice)
      {
      // make score-relative instead on part-relative
      Q_ASSERT(part);
      Q_ASSERT(part->score());
      auto scoreRelStaff = part->score()->staffIdx(part); // zero-based number of part's first staff in the score
      auto res = (scoreRelStaff + staff) * VOICES + voice;
      return res;
      }

//---------------------------------------------------------
//   findMeasure
//---------------------------------------------------------

/**
 In Score \a score find the measure starting at \a tick.
 */

static Measure* findMeasure(const Score* const score, const int tick)
      {
      for (Measure* m = score->firstMeasure();; m = m->nextMeasure()) {
            if (m && m->tick() == tick)
                  return m;
            }
      return 0;
      }

//---------------------------------------------------------
//   setStavesForPart
//---------------------------------------------------------

/**
 Set number of staves for part \a part to the max value of the current value
 and the value in \a staves.
 */

static void setStavesForPart(Part* part, const int staves)
      {
      if (!(staves > 0 && staves <= MAX_STAVES)) {
            qDebug("illegal number of staves: %d", staves);
            return;
            }

      Q_ASSERT(part);
      if (staves > part->nstaves())
            part->setStaves(staves);
      }

//---------------------------------------------------------
//   type conversions
//---------------------------------------------------------

//---------------------------------------------------------
//   mnxClefToClefType
//---------------------------------------------------------

/*
 * Convert MNX clef type to MuseScore ClefType.
 */

static ClefType mnxClefToClefType(const QString& sign, const QString& line)
      {
      ClefType res = ClefType::INVALID;

      if (sign == "G" && line == "2")
            res = ClefType::G;
      else if (sign == "F" && line == "4")
            res = ClefType::F;
      else
            qDebug("unknown clef sign: '%s' line: '%s' oct ch=%d>",
                   qPrintable(sign), qPrintable(line), 0);

      return res;
      }

//---------------------------------------------------------
//   mnxTSigToBtsBtp
//---------------------------------------------------------

/*
 * Convert MNX time signature to beats and beat type.
 */

static void mnxTSigToBtsBtp(const QString& tsig, int& beats, int& beattp)
      {
      if (tsig == "3/4") {
            beats = 3; beattp = 4;
            }
      else if (tsig == "4/4") {
            beats = 4; beattp = 4;
            }
      else
            qDebug("mnxTSigToBtsBtp: unknown '%s'", qPrintable(tsig));
      }

//---------------------------------------------------------
//   mnxValueUnitToDurationType
//---------------------------------------------------------

/*
 * Convert MNX note value unit to MuseScore DurationType.
 */

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

//---------------------------------------------------------
//   mnxEventValueToTDuration
//---------------------------------------------------------

/*
 * Convert MNX note value (unit plus optional dots) to MuseScore TDuration.
 */

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

//---------------------------------------------------------
//   mnxToMidiPitch
//---------------------------------------------------------

/**
 Convert MNX pitch to MIDI note number and TPC.
 Does not (yet) support non-integer alteration.
 */

static int mnxToMidiPitch(const QString& value, int& tpc)
      {
      tpc = Tpc::TPC_INVALID;

      if (value.size() < 2) {
            qDebug("mnxToMidiPitch invalid value '%s'", qPrintable(value));
            return -1;
            }

      // handle step
      QString steps("CDEFGAB");
      auto stepChar = value.at(0);
      auto step = steps.indexOf(stepChar);

      if (step < 0 || 6 < step) {
            qDebug("mnxToMidiPitch invalid value '%s'", qPrintable(value));
            return -1;
            }

      auto altOct = value.right(value.length() - 1);

      // handle alt
      auto alt = 0;
      while (altOct.startsWith('#')) {
            altOct.remove(0, 1);
            alt++;
            }
      while (altOct.startsWith('b')) {
            altOct.remove(0, 1);
            alt--;
            }

      // handle oct
      bool ok = false;
      auto oct = altOct.toInt(&ok);

      // calculate TPC
      tpc = step2tpc(step, AccidentalVal(alt));

      //                       c  d  e  f  g  a   b
      static int table[7]  = { 0, 2, 4, 5, 7, 9, 11 };

      // if all is well, return result
      if (ok)
            return table[step] + alt + (oct + 1) * 12;

      return -1;
      }

//---------------------------------------------------------
//   parser: node handlers
//---------------------------------------------------------

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

      auto nStaves = 0;

      while (_e.readNextStartElement()) {
            if (_e.name() == "staff") {
                  setStavesForPart(_part, nStaves + 1);
                  staff(nStaves); // note orde: staff createdby setStavesForPart()
                  ++nStaves;
                  }
            else if (_e.name() == "tempo") {
                  skipLogCurrElem();
                  }
            else if (_e.name() == "time") {
                  time();
                  }
            else
                  skipLogCurrElem();
            }

      Q_ASSERT(_e.isEndElement() && _e.name() == "attributes");
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

      Q_ASSERT(_e.isEndElement() && _e.name() == "beam");
      }

//---------------------------------------------------------
//   clef
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure/attributes/staff/clef node.
 */

void MnxParser::clef(const int track)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "clef");
      logDebugTrace("MnxParser::clef");

      auto sign = _e.attributes().value("sign").toString();
      auto line = _e.attributes().value("line").toString();
      logDebugTrace(QString("clef sign '%1' line '%2'").arg(sign).arg(line));

      auto ct = mnxClefToClefType(sign, line);

      if (ct != ClefType::INVALID) {
            const int tick = 0;       // TODO
            addClef(_score, tick, track, ct);
            }

      _e.skipCurrentElement();

      Q_ASSERT(_e.isEndElement() && _e.name() == "clef");
      }

//---------------------------------------------------------
//   creator
//---------------------------------------------------------

/**
 Parse the /mnx/head/identification/creator node.
 */

void MnxParser::creator()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "creator");
      logDebugTrace("MnxParser::creator");

      auto creatorType = _e.attributes().value("type").toString();
      auto creatorValue = _e.readElementText();
      logDebugTrace(QString("creator '%1' '%2'").arg(creatorType).arg(creatorValue));

      if (creatorType == "composer" && !creatorValue.isEmpty())
            _composer = creatorValue;

      Q_ASSERT(_e.isEndElement() && _e.name() == "creator");
      }

//---------------------------------------------------------
//   event
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure/sequence/event node.
 */

Fraction MnxParser::event(Measure* measure, const Fraction sTime, const int seqNr)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "event");
      logDebugTrace("MnxParser::event");

      QString value = _e.attributes().value("value").toString();
      logDebugTrace(QString("event value '%1'").arg(value));

      ChordRest* cr = nullptr;

      while (_e.readNextStartElement()) {
            if (_e.name() == "lyric")
                  lyric();
            else if (_e.name() == "note") {
                  if (!cr)
                        cr = createChord(_score, value, seqNr);
                  cr->add(note(seqNr));
                  }
            else if (_e.name() == "rest") {
                  if (!cr)
                        cr = rest(_score, value, seqNr);
                  }
            else
                  skipLogCurrElem();
            }

      auto s = measure->getSegment(SegmentType::ChordRest, sTime.ticks());
      s->add(cr);

      Q_ASSERT(_e.isEndElement() && _e.name() == "event");

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

      Q_ASSERT(_e.isEndElement() && _e.name() == "head");
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
            if (_e.name() == "creator")
                  creator();
            else if (_e.name() == "subtitle")
                  subtitle();
            else if (_e.name() == "title")
                  title();
            else
                  skipLogCurrElem();
            }

      addVBoxWithMetaData(_score, _composer, _subtitle, _title);
      addMetaData(_score, _composer, _subtitle, _title);

      Q_ASSERT(_e.isEndElement() && _e.name() == "identification");
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

      Q_ASSERT(_e.isEndElement() && _e.name() == "lyric");
      }

//---------------------------------------------------------
//   measure
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure node.
 */

void MnxParser::measure(const int measureNr)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "measure");
      logDebugTrace("MnxParser::measure");

      Measure* currMeasure = nullptr;

      if (inRealPart()) {
            auto startTick = calculateMeasureStartTick(_beats, _beatType, measureNr);
            if (_score->staffIdx(_part) == 0) {
                  // for first part only: create a measure
                  currMeasure = measureNr
                        ? addMeasure(_score, startTick, _beats, _beatType, measureNr + 1)
                        : addFirstMeasure(_score, _beats, _beatType);
                  }
            else {
                  // for the other parts, just find the measure
                  // TODO: also support the first part having less measures than the others
                  currMeasure = findMeasure(_score, startTick);
                  if (!currMeasure) {
                        logError(QString("measure at tick %1 not found!").arg(startTick));
                        skipLogCurrElem();
                        }

                  }
            }

      QVector<int> staffSeqCount(MAX_STAVES);       // sequence count per staff

      while (_e.readNextStartElement()) {
            if (_e.name() == "attributes")
                  attributes();
            else if (_e.name() == "sequence") {
                  auto sequenceStartTick = Fraction(measureNr * _beats, _beatType); // TODO: assumes no timesig changes
                  sequence(currMeasure, sequenceStartTick, staffSeqCount);
                  }
            else
                  skipLogCurrElem();
            }

      Q_ASSERT(_e.isEndElement() && _e.name() == "measure");
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

      Q_ASSERT(_e.isEndElement() && _e.name() == "mnx");
      }

//---------------------------------------------------------
//   note
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure/sequence/event/note node.
 */

Note* MnxParser::note(const int seqNr)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "note");
      logDebugTrace("MnxParser::note");

      auto accidental = _e.attributes().value("accidental").toString();
      auto pitch = _e.attributes().value("pitch").toString();
      logDebugTrace(QString("- note pitch '%1' accidental '%2'").arg(pitch).arg(accidental));

      // TODO which is correct ? _e.readNext();
      _e.skipCurrentElement();

      Q_ASSERT(_e.isEndElement() && _e.name() == "note");

      return createNote(_score, pitch, seqNr /*, accidental*/);
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

      _part = appendPart(_score, _beats, _beatType);

      setInRealPart();
      auto measureNr = 0;

      while (_e.readNextStartElement()) {
            if (_e.name() == "measure") {
                  measure(measureNr);
                  measureNr++;
                  }
            else if (_e.name() == "part-name") {
                  auto partName = _e.readElementText();
                  logDebugTrace(QString("part-name '%1'").arg(partName));
                  _part->setPlainLongName(partName);
                  _part->setPartName(partName);
                  }
            else
                  skipLogCurrElem();
            }

      Q_ASSERT(_e.isEndElement() && _e.name() == "part");
      }

//---------------------------------------------------------
//   rest
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure/sequence/event/rest node.
 */

Rest* MnxParser::rest(Score* score, const QString& value, const int seqNr)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "rest");
      logDebugTrace("MnxParser::rest");

      logDebugTrace(QString("- rest"));

      // TODO _e.readNext();
      _e.skipCurrentElement();

      Q_ASSERT(_e.isEndElement() && _e.name() == "rest");

      return createRest(_score, value, seqNr);
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

      Q_ASSERT(_e.isEndElement() && _e.name() == "score");
      }

//---------------------------------------------------------
//   sequence
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure/sequence node.
 */

void MnxParser::sequence(Measure* measure, const Fraction sTime, QVector<int>& staffSeqCount)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "sequence");
      logDebugTrace("MnxParser::sequence");

      // read staff attribute, not distinguishing between missing and invalid
      auto ok = false;
      auto staff = _e.attributes().value("staff").toString().toInt(&ok);
      logDebugTrace(QString("staff '%1'").arg(staff));
      staff = ok ? (staff - 1) : 0;       // convert to zero-based or set default

      if (staff < 0 || staff >= MAX_STAVES) {
            logError("invalid staff");
            skipLogCurrElem();
            }
      else {
            Fraction seqTime(0, 1); // time in this sequence

            while (_e.readNextStartElement()) {
                  if (_e.name() == "event") {
                        auto track = determineTrack(_part, staff, staffSeqCount.at(staff));
                        seqTime += event(measure, sTime + seqTime, track);
                        }
                  else
                        skipLogCurrElem();
                  }
            staffSeqCount[staff]++;
            }

      Q_ASSERT(_e.isEndElement() && _e.name() == "sequence");
      }

//---------------------------------------------------------
//   staff
//---------------------------------------------------------

/**
 Parse the /mnx/score/part/measure/attributes/staff node.
 */

void MnxParser::staff(const int staffNr)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "staff");
      logDebugTrace("MnxParser::staff");

      auto voice { 0 };
      auto track = determineTrack(_part, staffNr, voice);

      if (staffNr > 0) {
            const int tick = 0;
            addTimeSig(_score, tick, track, _beats, _beatType);
            }

      while (_e.readNextStartElement()) {
            if (_e.name() == "clef")
                  clef(track);
            else
                  skipLogCurrElem();
            }

      Q_ASSERT(_e.isEndElement() && _e.name() == "staff");
      }

//---------------------------------------------------------
//   subtitle
//---------------------------------------------------------

/**
 Parse the /mnx/head/identification/subtitle node.
 */

void MnxParser::subtitle()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "subtitle");
      logDebugTrace("MnxParser::subtitle");

      _subtitle = _e.readElementText();
      logDebugTrace(QString("subtitle '%1'").arg(_title));

      Q_ASSERT(_e.isEndElement() && _e.name() == "subtitle");
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

      auto measureNr = 0;

      while (_e.readNextStartElement()) {
            if (_e.name() == "measure") {
                  measure(measureNr);
                  measureNr++;
                  } else
                  skipLogCurrElem();
            }

      Q_ASSERT(_e.isEndElement() && _e.name() == "system");
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

      mnxTSigToBtsBtp(signature, _beats, _beatType);

      Q_ASSERT(_e.isEndElement() && _e.name() == "time");
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

      Q_ASSERT(_e.isEndElement() && _e.name() == "title");
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
