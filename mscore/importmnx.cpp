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

/*
 Issue list
 - check for valid staff number before putting anything on the staff
 - other sanity checks are mostly missing
 - make all pointers passed to functions const where possible (to const or non-const as required)
 - have factory functions return unique_ptr
 */

#include "libmscore/box.h"
#include "libmscore/chord.h"
#include "libmscore/durationtype.h"
#include "libmscore/keysig.h"
#include "libmscore/lyrics.h"
#include "libmscore/measure.h"
#include "libmscore/part.h"
#include "libmscore/rest.h"
#include "libmscore/staff.h"
#include "libmscore/tuplet.h"
#include "musescore.h"
#include "importmxmllogger.h"
#include "importmnx.h"

namespace Ms {

//---------------------------------------------------------
//   Constants
//---------------------------------------------------------

const int MAX_LYRICS       = 16;

//---------------------------------------------------------
//   MnxParser definition
//---------------------------------------------------------

class MnxParser
      {
public:
      MnxParser(Score* score, MxmlLogger* logger);
      Score::FileError parse(QIODevice* device);

private:
      // functions
      Fraction beamed(Measure* measure, const Fraction sTime, const int track, Tuplet* tuplet);
      void clef(const int track);
      void creator();
      void cwmnx();
      void directions();
      Fraction event(Measure* measure, const Fraction sTime, const int seqNr, Tuplet* tuplet);
      void global();
      void head();
      bool inRealPart() const { return _inRealPart; }
      void key();
      void lyric(ChordRest* cr);
      void measure(const int measureNr);
      void mnx();
      Note* note(const int seqNr);
      Score::FileError parse();
      void part();
      Rest* rest(Measure* measure, const bool measureRest, const QString& value, const int seqNr);
      void score();
      void sequence(Measure* measure, const Fraction sTime, QVector<int>& staffSeqCount);
      void setInRealPart() { _inRealPart = true; }
      void skipLogCurrElem();
      int staves();
      void subtitle();
      void tempo();
      void time();
      void title();
      Fraction parseTuplet(Measure* measure, const Fraction sTime, const int track);

      // data
      QXmlStreamReader _e;
      QString _parseStatus;                           ///< Parse status (typicallay a short error message)
      Score* const _score;                            ///< MuseScore score
      MxmlLogger* const _logger;                      ///< Error logger
      Part* _part;                                    ///< current part (TODO: remove ?)
      int _beats;                                     ///< initial number of beats
      int _beatType;                                  ///< initial beat type
      KeySigEvent _key;                               ///< initial key signature
      QString _composer;                              ///< metadata: composer
      QString _subtitle;                              ///< metadata: subtitle
      QString _title;                                 ///< metadata: title
      bool _inRealPart;
      };

//---------------------------------------------------------
//   MnxParser constructor
//---------------------------------------------------------

MnxParser::MnxParser(Score* score, MxmlLogger* logger)
      : _score(score), _logger(logger),
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

      MxmlLogger logger;
      logger.setLoggingLevel(MxmlLogger::Level::MXML_INFO);
      //logger.setLoggingLevel(MxmlLogger::Level::MXML_TRACE); // also include tracing

      MnxParser p(score, &logger);
      auto res = p.parse(dev);
      if (res != Score::FileError::FILE_NO_ERROR)
            return res;

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
      _logger->logDebugTrace("MnxParser::parse device");
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
      _logger->logDebugTrace("MnxParser::parse");

      bool found = false;
      while (_e.readNextStartElement()) {
            if (_e.name() == "mnx") {
                  found = true;
                  mnx();
                  }
            else {
                  _logger->logError(QString("this is not an MNX file (top-level node '%1')")
                                    .arg(_e.name().toString()));
                  _e.skipCurrentElement();
                  return Score::FileError::FILE_BAD_FORMAT;
                  }
            }

      if (!found) {
            _logger->logError("this is not an MNX file, node <mnx> not found");
            return Score::FileError::FILE_BAD_FORMAT;
            }

      return Score::FileError::FILE_NO_ERROR;
      }

//---------------------------------------------------------
//   forward references
//---------------------------------------------------------

static void addKeySig(Score* score, const int tick, const int track, const KeySigEvent key);
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
            auto vbox = new VBox(score);
            if (!composer.isEmpty()) {
                  auto text = new Text(score, Tid::COMPOSER);
                  text->setPlainText(composer);
                  vbox->add(text);
                  }
            if (!subtitle.isEmpty()) {
                  auto text = new Text(score, Tid::SUBTITLE);
                  text->setPlainText(subtitle);
                  vbox->add(text);
                  }
            if (!title.isEmpty()) {
                  auto text = new Text(score, Tid::TITLE);
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

static Measure* addFirstMeasure(Score* score, const KeySigEvent key, const int bts, const int bttp)
      {
      const auto tick = 0;
      const auto nr = 1;
      auto m = addMeasure(score, tick, bts, bttp, nr);
      // keysig and timesig
      const int track = 0;
      addKeySig(score, tick, track, key);
      addTimeSig(score, tick, track, bts, bttp);
      return m;
      }

//---------------------------------------------------------
//   addKeySig
//---------------------------------------------------------

/**
 Add a key signature to the score.
 */

static void addKeySig(Score* score, const int tick, const int track, const KeySigEvent key)
      {
      if (key.isValid()) {
            auto keysig = new KeySig(score);
            keysig->setTrack(track);
            keysig->setKeySigEvent(key);
            auto measure = score->tick2measure(tick);
            auto s = measure->getSegment(SegmentType::KeySig, tick);
            s->add(keysig);
            }
      }

//---------------------------------------------------------
//   addLyric
//---------------------------------------------------------

/**
 Add a single lyric to the score (unless the number is too high)
 */

static void addLyric(ChordRest* cr, int lyricNo, const QString& text)
      {
      if (!cr) {
            qDebug("no chord for lyric");
            return;
            }

      if (lyricNo > MAX_LYRICS) {
            qDebug("too much lyrics (>%d)", MAX_LYRICS);       // TODO
            return;
            }

      auto l = new Lyrics(cr->score());
      // TODO in addlyrics: l->setTrack(trk);

      l->setNo(lyricNo);
      l->setPlainText(text);
      cr->add(l);
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

static Part* appendPart(Score* score, const KeySigEvent key, const int bts, const int bttp)
      {
      // create part and first staff
      QString id("importMnx");
      auto part = new Part(score);
      part->setId(id);
      score->appendPart(part);
      part->setStaves(1);

      // if not the first part, add the key and time signature
      const auto staff = 0;
      const auto voice = 0;
      auto track = determineTrack(part, staff, voice);
      if (track > 0) {
            const auto tick = 0;
            addKeySig(score, tick, track, key);
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
//   createCompleteMeasureRest
//---------------------------------------------------------

/*
 * Create a complete measure rest for measure \a measure in track \a track.
 */

Rest* createCompleteMeasureRest(Measure* measure, const int track)
      {
      auto rest = new Rest(measure->score());
      rest->setDurationType(TDuration::DurationType::V_MEASURE);
      rest->setDuration(measure->len());
      rest->setTrack(track);
      return rest;
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
//   createTuplet
//---------------------------------------------------------

/*
 * Create a tuplet in measure \a measure and track \a track.
 */

Tuplet* createTuplet(Measure* measure, const int track)
      {
      auto tuplet = new Tuplet(measure->score());
      tuplet->setTrack(track);
      return tuplet;
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
/* TODO */
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
/**/
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
//   mnxKeyToKeySigEvent
//---------------------------------------------------------

/*
 * Convert MNX key type to MuseScore KeySigEvent.
 */

static KeySigEvent mnxKeyToKeySigEvent(const QString& fifths, const QString& mode)
      {
      KeySigEvent res;

      res.setKey(Key(fifths.toInt()));       // TODO: move toInt to caller

      if (mode == "major") {
            res.setMode(KeyMode::MAJOR);
            }
      else if (mode == "minor") {
            res.setMode(KeyMode::MINOR);
            }

      if (!res.isValid()) {
            qDebug("unknown key signature: fifths '%s' mode '%s'",
                   qPrintable(fifths), qPrintable(mode));
            }

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
      if (s == "/4")
            return TDuration::DurationType::V_QUARTER;
      else if (s == "/8")
            return TDuration::DurationType::V_EIGHTH;
      else if (s == "/1024")
            return TDuration::DurationType::V_1024TH;
      else if (s == "/512")
            return TDuration::DurationType::V_512TH;
      else if (s == "/256")
            return TDuration::DurationType::V_256TH;
      else if (s == "/128")
            return TDuration::DurationType::V_128TH;
      else if (s == "/64")
            return TDuration::DurationType::V_64TH;
      else if (s == "/32")
            return TDuration::DurationType::V_32ND;
      else if (s == "/16")
            return TDuration::DurationType::V_16TH;
      else if (s == "/2")
            return TDuration::DurationType::V_HALF;
      else if (s == "/1")
            return TDuration::DurationType::V_WHOLE;
      else if (s == "*2")
            return TDuration::DurationType::V_BREVE;
      else if (s == "*4")
            return TDuration::DurationType::V_LONG;
      else {
            qDebug("mnxValueUnitToDurationType(%s): unknown", qPrintable(s));
            //return TDuration::DurationType::V_INVALID;
            return TDuration::DurationType::V_QUARTER;
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

      while (valueWithoutDots.endsWith('d')) {
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
//   setTupletParameters (TODO: enhance too trivial implementation)
//---------------------------------------------------------

static void setTupletParameters(Tuplet* tuplet, const QString& actual, const QString& normal)
      {
      Q_ASSERT(tuplet);

      auto actualNotes = 3;
      auto normalNotes = 2;
      auto td = TDuration::DurationType::V_INVALID;

      if (actual == "3/4" && normal == "2/4") {
            actualNotes = 3; normalNotes = 2; td = TDuration::DurationType::V_QUARTER;
            }
      else
      if (actual == "3/8" && normal == "2/8") {
            actualNotes = 3; normalNotes = 2; td = TDuration::DurationType::V_EIGHTH;
            }
      else
            {
            qDebug("invalid actual '%s' and/or normal '%s'", qPrintable(actual), qPrintable(normal));
            return;
            }

      tuplet->setRatio(Fraction(actualNotes, normalNotes));
      tuplet->setBaseLen(td);
      }

//---------------------------------------------------------
//   parser: support functions
//---------------------------------------------------------

// read staff attribute, not distinguishing between missing and invalid
// return 0-based staff number

static int readStaff(QXmlStreamReader& e, MxmlLogger* const logger, bool& ok)
      {
      auto attrStaff = e.attributes().value("staff").toString();
      logger->logDebugTrace(QString("staff '%1'").arg(attrStaff));
      if (attrStaff.isEmpty()) {
            ok = true;
            return 0;
            }
      auto staff = attrStaff.toInt(&ok) - 1;
      if (!ok || staff < 0 || staff >= MAX_STAVES) {
            ok = false;
            staff = 0;
            logger->logError(QString("invalid staff '%1'").arg(attrStaff), &e);
            }
      return staff;
      }

//---------------------------------------------------------
//   parser: node handlers
//---------------------------------------------------------

//---------------------------------------------------------
//   beamed
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure/sequence/beamed node.
 */

// TODO: actually set the beam (currently the notes are read, but beam handling is still automatic)

Fraction MnxParser::beamed(Measure* measure, const Fraction sTime, const int track, Tuplet* tuplet)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "beamed");
      _logger->logDebugTrace("MnxParser::beamed");

      Fraction seqTime(0, 1);       // time in this sequence

      while (_e.readNextStartElement()) {
            if (_e.name() == "event") {
                  seqTime += event(measure, sTime + seqTime, track, tuplet);
                  }
            else if (_e.name() == "tuplet") {
                  seqTime += parseTuplet(measure, sTime + seqTime, track);
                  }
            else
                  skipLogCurrElem();
            }

      Q_ASSERT(_e.isEndElement() && _e.name() == "beamed");

      return seqTime;
      }

//---------------------------------------------------------
//   clef
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure/directions/clef node.
 */

void MnxParser::clef(const int track)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "clef");
      _logger->logDebugTrace("MnxParser::clef");

      bool ok = true;
      auto sign = _e.attributes().value("sign").toString();
      auto line = _e.attributes().value("line").toString();
      auto staff = readStaff(_e, _logger, ok);
      _logger->logDebugTrace(QString("clef sign '%1' line '%2' staff '%3'").arg(sign).arg(line).arg(staff));

      if (ok) {
            auto ct = mnxClefToClefType(sign, line);

            if (ct != ClefType::INVALID) {
                  const int tick = 0; // TODO
                  addClef(_score, tick, track + staff * VOICES, ct);
                  }
            }

      _e.skipCurrentElement();

      Q_ASSERT(_e.isEndElement() && _e.name() == "clef");
      }

//---------------------------------------------------------
//   creator
//---------------------------------------------------------

/**
 Parse the /mnx/head/creator node.
 */

void MnxParser::creator()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "creator");
      _logger->logDebugTrace("MnxParser::creator");

      auto creatorType = _e.attributes().value("type").toString();
      auto creatorValue = _e.readElementText();
      _logger->logDebugTrace(QString("creator '%1' '%2'").arg(creatorType).arg(creatorValue));

      if (creatorType == "composer" && !creatorValue.isEmpty())
            _composer = creatorValue;

      Q_ASSERT(_e.isEndElement() && _e.name() == "creator");
      }

//---------------------------------------------------------
//   cwmnx
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx node.
 */

void MnxParser::cwmnx()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "cwmnx");
      _logger->logDebugTrace("MnxParser::cwmnx");

      while (_e.readNextStartElement()) {
            if (_e.name() == "global")
                  global();
            else if (_e.name() == "part") {
                  part();
                  }
            else
                  skipLogCurrElem();
            }

      Q_ASSERT(_e.isEndElement() && _e.name() == "cwmnx");
      }

//---------------------------------------------------------
//   directions
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure/directions node.
 */

void MnxParser::directions()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "directions");
      _logger->logDebugTrace("MnxParser::directions");

      while (_e.readNextStartElement()) {
            if (_e.name() == "clef") {
                  auto track = determineTrack(_part, 0, 0);
                  clef(track);
                  }
            else if (_e.name() == "key") {
                  key();
                  }
            else if (_e.name() == "staves") {
                  // TODO: factor out
                  auto oldStaves = _part->nstaves();
                  auto newStaves = staves();
                  setStavesForPart(_part, newStaves);
                  for (auto i = oldStaves; i < newStaves; ++i) {
                        auto voice = 0;
                        auto track = determineTrack(_part, i, voice);

                        if (i > 0) {
                              const int tick = 0;
                              addKeySig(_score, tick, track, _key);
                              addTimeSig(_score, tick, track, _beats, _beatType);
                              }

                        }
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

      Q_ASSERT(_e.isEndElement() && _e.name() == "directions");
      }

//---------------------------------------------------------
//   event
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure/sequence/event node.
 */

Fraction MnxParser::event(Measure* measure, const Fraction sTime, const int seqNr, Tuplet* tuplet)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "event");
      _logger->logDebugTrace("MnxParser::event");

      auto attrMeasure = _e.attributes().value("measure").toString();
      bool measureRest = attrMeasure == "yes";
      auto attrValue = _e.attributes().value("value").toString();
      _logger->logDebugTrace(QString("event measure '%1' value '%2'").arg(attrMeasure).arg(attrValue));

      ChordRest* cr = nullptr;

      while (_e.readNextStartElement()) {
            if (_e.name() == "lyric")
                  lyric(cr);
            else if (_e.name() == "note") {
                  if (!cr)
                        cr = createChord(_score, attrValue, seqNr);
                  cr->add(note(seqNr));
                  }
            else if (_e.name() == "rest") {
                  if (!cr)
                        cr = rest(measure, measureRest, attrValue, seqNr);
                  }
            else
                  skipLogCurrElem();
            }

      auto s = measure->getSegment(SegmentType::ChordRest, sTime.ticks());
      s->add(cr);

      if (tuplet) {
            cr->setTuplet(tuplet);
            tuplet->add(cr);
            }

      Q_ASSERT(_e.isEndElement() && _e.name() == "event");

      return cr->actualFraction();
      }

//---------------------------------------------------------
//   global
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/global node.
 */

void MnxParser::global()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "global");
      _logger->logDebugTrace("MnxParser::head");

      auto measureNr = 0;

      while (_e.readNextStartElement()) {
            if (_e.name() == "measure") {
                  measure(measureNr);
                  measureNr++;
                  } else
                  skipLogCurrElem();
            }

      Q_ASSERT(_e.isEndElement() && _e.name() == "global");
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
      _logger->logDebugTrace("MnxParser::head");


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

      Q_ASSERT(_e.isEndElement() && _e.name() == "head");
      }

//---------------------------------------------------------
//   key
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/global/measure/directions/key node.
 */

void MnxParser::key()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "key");
      _logger->logDebugTrace("MnxParser::key");

      auto mode = _e.attributes().value("mode").toString();
      auto fifths = _e.attributes().value("fifths").toString();
      _logger->logDebugTrace(QString("key-sig '%1' '%2'").arg(fifths).arg(mode));
      _e.skipCurrentElement();

      _key = mnxKeyToKeySigEvent(fifths, mode);

      Q_ASSERT(_e.isEndElement() && _e.name() == "key");
      }

//---------------------------------------------------------
//   lyric
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure/sequence/event/lyric node.
 */

void MnxParser::lyric(ChordRest* cr)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "lyric");
      _logger->logDebugTrace("MnxParser::lyric");

      while (_e.readNextStartElement()) {
            if (_e.name() == "syllabic") {
                  // ignore for now
                  _e.skipCurrentElement();
                  }
            else if (_e.name() == "text") {
                  auto lyricText = _e.readElementText();
                  addLyric(cr, 0, lyricText);
                  }
            else
                  skipLogCurrElem();
            }

      Q_ASSERT(_e.isEndElement() && _e.name() == "lyric");
      }

//---------------------------------------------------------
//   measure
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure node.
 */

void MnxParser::measure(const int measureNr)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "measure");
      _logger->logDebugTrace("MnxParser::measure");

      Measure* currMeasure = nullptr;

      if (inRealPart()) {
            auto startTick = calculateMeasureStartTick(_beats, _beatType, measureNr);
            if (_score->staffIdx(_part) == 0) {
                  // for first part only: create a measure
                  currMeasure = measureNr
                        ? addMeasure(_score, startTick, _beats, _beatType, measureNr + 1)
                        : addFirstMeasure(_score, _key, _beats, _beatType);
                  }
            else {
                  // for the other parts, just find the measure
                  // TODO: also support the first part having less measures than the others
                  currMeasure = findMeasure(_score, startTick);
                  if (!currMeasure) {
                        _logger->logError(QString("measure at tick %1 not found!").arg(startTick));
                        skipLogCurrElem();
                        }

                  }
            }

      QVector<int> staffSeqCount(MAX_STAVES);       // sequence count per staff

      while (_e.readNextStartElement()) {
            if (_e.name() == "directions")
                  directions();
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
      _logger->logDebugTrace("MnxParser::mnx");


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
 Parse the /mnx/score/cwmnx/part/measure/sequence/event/note node.
 */

Note* MnxParser::note(const int seqNr)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "note");
      _logger->logDebugTrace("MnxParser::note");

      auto accidental = _e.attributes().value("accidental").toString();
      auto pitch = _e.attributes().value("pitch").toString();
      _logger->logDebugTrace(QString("- note pitch '%1' accidental '%2'").arg(pitch).arg(accidental));

      // TODO which is correct ? _e.readNext();
      _e.skipCurrentElement();

      Q_ASSERT(_e.isEndElement() && _e.name() == "note");

      return createNote(_score, pitch, seqNr /*, accidental*/);
      }

//---------------------------------------------------------
//   part
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part node.
 */

void MnxParser::part()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "part");
      _logger->logDebugTrace("MnxParser::part");

      _part = appendPart(_score, _key, _beats, _beatType);

      setInRealPart();
      auto measureNr = 0;

      while (_e.readNextStartElement()) {
            if (_e.name() == "measure") {
                  measure(measureNr);
                  measureNr++;
                  }
            else if (_e.name() == "part-name") {
                  auto partName = _e.readElementText();
                  _logger->logDebugTrace(QString("part-name '%1'").arg(partName));
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
 Parse the /mnx/score/cwmnx/part/measure/sequence/event/rest node.
 */

Rest* MnxParser::rest(Measure* measure, const bool measureRest, const QString& value, const int seqNr)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "rest");
      _logger->logDebugTrace("MnxParser::rest");

      _logger->logDebugTrace(QString("- rest"));

      // TODO _e.readNext();
      _e.skipCurrentElement();

      Q_ASSERT(_e.isEndElement() && _e.name() == "rest");

      return measureRest
             ? createCompleteMeasureRest(measure, seqNr)
             : createRest(measure->score(), value, seqNr);
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
      _logger->logDebugTrace("MnxParser::score");

      while (_e.readNextStartElement()) {
            if (_e.name() == "cwmnx")
                  cwmnx();
            else
                  skipLogCurrElem();
            }

      Q_ASSERT(_e.isEndElement() && _e.name() == "score");
      }

//---------------------------------------------------------
//   sequence
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure/sequence node.
 */

void MnxParser::sequence(Measure* measure, const Fraction sTime, QVector<int>& staffSeqCount)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "sequence");
      _logger->logDebugTrace("MnxParser::sequence");

      bool ok = false;
      auto staff = readStaff(_e, _logger, ok);

      if (!ok) {
            skipLogCurrElem();
            }
      else {
            Fraction seqTime(0, 1); // time in this sequence
            auto track = determineTrack(_part, staff, staffSeqCount.at(staff));

            while (_e.readNextStartElement()) {
                  if (_e.name() == "beamed") {
                        seqTime += beamed(measure, sTime + seqTime, track, nullptr);
                        }
                  else if (_e.name() == "clef") {
                        clef(track);
                        }
                  else if (_e.name() == "event") {
                        seqTime += event(measure, sTime + seqTime, track, nullptr);
                        }
                  else if (_e.name() == "tuplet") {
                        seqTime += parseTuplet(measure, sTime + seqTime, track);
                        }
                  else
                        skipLogCurrElem();
                  }
            staffSeqCount[staff]++;
            }

      Q_ASSERT(_e.isEndElement() && _e.name() == "sequence");
      }

//---------------------------------------------------------
//   staves
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure/directions/staves node.
 */

int MnxParser::staves()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "staves");
      _logger->logDebugTrace("MnxParser::staves");

      auto number = _e.attributes().value("number").toString();

      _e.skipCurrentElement();

      bool ok = false;
      auto res = number.toInt(&ok);
      if (!ok)
            _logger->logError(QString("invalid number of staves '%1'").arg(number));

      Q_ASSERT(_e.isEndElement() && _e.name() == "staves");

      return res;
      }

//---------------------------------------------------------
//   subtitle
//---------------------------------------------------------

/**
 Parse the /mnx/head/subtitle node.
 */

void MnxParser::subtitle()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "subtitle");
      _logger->logDebugTrace("MnxParser::subtitle");

      _subtitle = _e.readElementText();
      _logger->logDebugTrace(QString("subtitle '%1'").arg(_title));

      Q_ASSERT(_e.isEndElement() && _e.name() == "subtitle");
      }

//---------------------------------------------------------
//   time
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/global/measure/directions/time node.
 */

void MnxParser::time()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "time");
      _logger->logDebugTrace("MnxParser::time");

      QString signature = _e.attributes().value("signature").toString();
      _logger->logDebugTrace(QString("time-sig '%1'").arg(signature));
      _e.skipCurrentElement();

      mnxTSigToBtsBtp(signature, _beats, _beatType);

      Q_ASSERT(_e.isEndElement() && _e.name() == "time");
      }

//---------------------------------------------------------
//   title
//---------------------------------------------------------

/**
 Parse the /mnx/head/title node.
 */

void MnxParser::title()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "title");
      _logger->logDebugTrace("MnxParser::title");

      _title = _e.readElementText();
      _logger->logDebugTrace(QString("title '%1'").arg(_title));

      Q_ASSERT(_e.isEndElement() && _e.name() == "title");
      }

//---------------------------------------------------------
//   tuplet
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure/sequence/tuplet node.
 */

Fraction MnxParser::parseTuplet(Measure* measure, const Fraction sTime, const int track)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "tuplet");
      _logger->logDebugTrace("MnxParser::tuplet");

      auto actual = _e.attributes().value("actual").toString();
      auto normal = _e.attributes().value("normal").toString();
      _logger->logDebugTrace(QString("tuplet actual '%1' normal '%2'").arg(actual).arg(normal));

      Fraction tupTime(0, 1);       // time in this tuplet

      // create the tuplet
      auto tuplet = createTuplet(measure, track);
      tuplet->setParent(measure);
      setTupletParameters(tuplet, actual, normal);

      while (_e.readNextStartElement()) {
            if (_e.name() == "beamed") {
                  tupTime += beamed(measure, sTime + tupTime, track, tuplet);
                  }
            else if (_e.name() == "event") {
                  tupTime += event(measure, sTime + tupTime, track, tuplet);
                  }
            else
                  skipLogCurrElem();
            }

      Q_ASSERT(_e.isEndElement() && _e.name() == "tuplet");

      return tupTime;
      }

//---------------------------------------------------------
//   skipLogCurrElem
//---------------------------------------------------------

/**
 Skip the current element, log debug as info.
 */

void MnxParser::skipLogCurrElem()
      {
      _logger->logDebugInfo(QString("skipping '%1'").arg(_e.name().toString()), &_e);
      _e.skipCurrentElement();
      }

} // namespace Ms
