//=============================================================================
//  MuseScore
//  Linux Music Score Editor
//
//  Copyright (C) 2007 Werner Schweer and others
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

#include "musedata.h"
#include "libmscore/score.h"
#include "libmscore/part.h"
#include "libmscore/staff.h"
#include "libmscore/barline.h"
#include "libmscore/clef.h"
#include "libmscore/key.h"
#include "libmscore/note.h"
#include "libmscore/chord.h"
#include "libmscore/rest.h"
#include "libmscore/text.h"
#include "libmscore/bracket.h"
#include "libmscore/tuplet.h"
#include "libmscore/slur.h"
#include "libmscore/dynamic.h"
#include "libmscore/lyrics.h"
#include "libmscore/articulation.h"
#include "libmscore/sig.h"
#include "libmscore/measure.h"
#include "libmscore/timesig.h"
#include "libmscore/segment.h"
#include "libmscore/sym.h"

namespace Ms {

//---------------------------------------------------------
//   musicalAttribute
//---------------------------------------------------------

void MuseData::musicalAttribute(QString s, Part* part)
      {
      QStringList al = s.mid(3).split(" ", QString::SkipEmptyParts);
      foreach(QString item, al) {
            if (item.startsWith("K:")) {
                  int key = item.midRef(2).toInt();
                  KeySigEvent ke;
                  ke.setKey(Key(key));
                  for (Staff* staff :*(part->staves()))
                        staff->setKey(curTick, ke);
                  }
            else if (item.startsWith("Q:")) {
                  _division = item.midRef(2).toInt();
                  }
            else if (item.startsWith("T:")) {
                  QStringList tl = item.mid(2).split("/");
                  if (tl.size() != 2) {
                        qDebug("bad time sig <%s>", qPrintable(item));
                        continue;
                        }
                  int z = tl[0].toInt();
                  int n = tl[1].toInt();
                  if ((z > 0) && (n > 0)) {
                        //TODO                        score->sigmap()->add(curTick, Fraction(z, n));
                        TimeSig* ts = new TimeSig(score);
                        Staff* staff = part->staff(0);
                        ts->setTrack(staff->idx() * VOICES);
                        Measure* mes = score->tick2measure(curTick);
                        Segment* seg = mes->getSegment(SegmentType::TimeSig, curTick);
                        seg->add(ts);
                        }
                  }
            else if (item.startsWith("X:"))
                  ;
            else if (item[0] == 'C') {
                  int staffIdx = 1;
                  //                  int col = 2;
                  if (item[1].isDigit()) {
                        staffIdx = item.midRef(1,1).toInt();
                        //                        col = 3;
                        }
                  staffIdx -= 1;
                  /*                  int clef = item.mid(col).toInt();
                                    ClefType mscoreClef = ClefType::G;
                                    switch(clef) {
                                          case 4:  mscoreClef = ClefType::G; break;
                                          case 22: mscoreClef = ClefType::F; break;
                                          case 13: mscoreClef = ClefType::C3; break;
                                          case 14: mscoreClef = ClefType::C2; break;
                                          case 15: mscoreClef = ClefType::C1; break;
                                          default:
                                                qDebug("unknown clef %d", clef);
                                                break;
                                          }
                                    */
                  //                  Staff* staff = part->staff(staffIdx);
                  //                  staff->setClef(curTick, mscoreClef);
                  }
            else
                  qDebug("unknown $key <%s>", qPrintable(item));
            }
      }

//---------------------------------------------------------
//   readChord
//---------------------------------------------------------

void MuseData::readChord(Part*, const QString& s)
      {
      //                       a  b   c  d  e  f  g
      static int table[7]  = { 9, 11, 0, 2, 4, 5, 7 };

      int step  = s[1].toLatin1() - 'A';
      int alter = 0;
      int octave = 0;
      for (int i = 2; i < 4; ++i) {
            if (s[i] == '#')
                  alter += 1;
            else if (s[i] == 'f')
                  alter -= 1;
            else if (s[i].isDigit()) {
                  octave = s.midRef(i,1).toInt();
                  break;
                  }
            }
      int staffIdx = 0;
      if (s.size() >= 24) {
            if (s[23].isDigit())
                  staffIdx = s.midRef(23,1).toInt() - 1;
            }
      int pitch = table[step] + alter + (octave + 1) * 12;
      if (pitch < 0)
            pitch = 0;
      if (pitch > 127)
            pitch = 127;

      Chord* chord = (Chord*)chordRest;
      Note* note = new Note(score);
      note->setPitch(pitch);
      note->setTpcFromPitch();
      note->setTrack(staffIdx * VOICES + voice);
      chord->add(note);
      }

//---------------------------------------------------------
//   openSlur
//---------------------------------------------------------

void MuseData::openSlur(int idx, const Fraction& tick, Staff* staff, int voc)
      {
      int staffIdx = staff->idx();
      if (slur[idx]) {
            qDebug("%06d: slur %d already open", tick.ticks(), idx+1);
            return;
            }
      slur[idx] = new Slur(score);
      slur[idx]->setTick(tick);
      slur[idx]->setTrack(staffIdx * VOICES + voc);
      score->addElement(slur[idx]);
      }

//---------------------------------------------------------
//   closeSlur
//---------------------------------------------------------

void MuseData::closeSlur(int idx, const Fraction& tick, Staff* staff, int voc)
      {
      int staffIdx = staff->idx();
      if (slur[idx]) {
            slur[idx]->setTick2(tick);
            slur[idx]->setTrack2(staffIdx * VOICES + voc);
            slur[idx] = 0;
            }
      else
            qDebug("%06d: slur %d not open", tick.ticks(), idx+1);
      }

//---------------------------------------------------------
//   readNote
//---------------------------------------------------------

void MuseData::readNote(Part* part, const QString& s)
      {
      //                       a  b   c  d  e  f  g
      static int table[7]  = { 9, 11, 0, 2, 4, 5, 7 };

      int step  = s[0].toLatin1() - 'A';
      int alter = 0;
      int octave = 0;
      for (int i = 1; i < 3; ++i) {
            if (s[i] == '#')
                  alter += 1;
            else if (s[i] == 'f')
                  alter -= 1;
            else if (s[i].isDigit()) {
                  octave = s.midRef(i,1).toInt();
                  break;
                  }
            }
      Direction dir = Direction::AUTO;
      if (s.size() >= 23) {
            if (s[22] == 'u')
                  dir = Direction::UP;
            else if (s[22] == 'd')
                  dir = Direction::DOWN;
            }

      int staffIdx = 0;
      if (s.size() >= 24) {
            if (s[23].isDigit())
                  staffIdx = s.midRef(23,1).toInt() - 1;
            }
      Staff* staff = part->staff(staffIdx);
      int gstaff   = staff->idx();

      int pitch = table[step] + alter + (octave + 1) * 12;
      if (pitch < 0)
            pitch = 0;
      if (pitch > 127)
            pitch = 127;
      Fraction ticks = Fraction::fromTicks((s.midRef(5, 3).toInt() * MScore::division + _division/2) / _division);
      Fraction tick  = curTick;
      curTick  += ticks;

      Tuplet* tuplet = 0;
      if (s.size() >= 22) {
            int a = 1;
            int b = 1;
            if (s[19] != ' ') {
                  a = s[19].toLatin1() - '0';
                  if (a == 3 && s[20] != ':')
                        b = 2;
                  else {
                        b = s[21].toLatin1() - '0';
                        }
                  }
            if (a == 3 && b == 2) {       // triplet
                  if (chordRest && chordRest->tuplet() && ntuplet) {
                        tuplet = chordRest->tuplet();
                        }
                  else {
                        tuplet = new Tuplet(score);
                        tuplet->setTrack(gstaff * VOICES);
                        tuplet->setTick(tick);
                        ntuplet = a;
                        tuplet->setRatio(Fraction(a, b));
                        measure->add(tuplet);
                        }
                  }
            else if (a == 1 && b == 1)
                  ;
            else
                  qDebug("unsupported tuple %d/%d", a, b);
            }

      Chord* chord = new Chord(score);
      chordRest = chord;
      chord->setTrack(gstaff * VOICES);
      chord->setStemDirection(dir);
      if (tuplet) {
            chord->setTuplet(tuplet);
            tuplet->add(chord);
            --ntuplet;
            }
      TDuration d;
      d.setVal(ticks.ticks());
      chord->setDurationType(d);

      Segment* segment = measure->getSegment(SegmentType::ChordRest, tick);

      voice = 0;
      for (; voice < VOICES; ++voice) {
            Element* e = segment->element(gstaff * VOICES + voice);
            if (e == 0) {
                  chord->setTrack(gstaff * VOICES + voice);
                  segment->add(chord);
                  break;
                  }
            }
      if (voice == VOICES) {
            qDebug("cannot allocate voice");
            delete chord;
            return;
            }
      Note* note = new Note(score);
      note->setPitch(pitch);
      note->setTpcFromPitch();
      note->setTrack(gstaff * VOICES + voice);
      chord->add(note);

      QString dynamics;
      QString an = s.mid(31, 11);
      for (int i = 0; i < an.size(); ++i) {
            if (an[i] == '(')
                  openSlur(0, tick, staff, voice);
            else if (an[i] == ')')
                  closeSlur(0, tick, staff, voice);
            else if (an[i] == '[')
                  openSlur(1, tick, staff, voice);
            else if (an[i] == ']')
                  closeSlur(1, tick, staff, voice);
            else if (an[i] == '{')
                  openSlur(2, tick, staff, voice);
            else if (an[i] == '}')
                  closeSlur(2, tick, staff, voice);
            else if (an[i] == 'z')
                  openSlur(3, tick, staff, voice);
            else if (an[i] == 'x')
                  closeSlur(3, tick, staff, voice);
            else if (an[i] == '.') {
                  Articulation* atr = new Articulation(score);
                  atr->setSymId(SymId::articStaccatoAbove);
                  chord->add(atr);
                  }
            else if (an[i] == '_') {
                  Articulation* atr = new Articulation(score);
                  atr->setSymId(SymId::articTenutoAbove);
                  chord->add(atr);
                  }
            else if (an[i] == 'v') {
                  Articulation* atr = new Articulation(score);
                  atr->setSymId(SymId::stringsUpBow);
                  chord->add(atr);
                  }
            else if (an[i] == 'n') {
                  Articulation* atr = new Articulation(score);
                  atr->setSymId(SymId::stringsDownBow);
                  chord->add(atr);
                  }
            else if (an[i] == 't') {
                  Articulation* atr = new Articulation(score);
                  atr->setSymId(SymId::ornamentTrill);
                  chord->add(atr);
                  }
            else if (an[i] == 'F') {
                  Articulation* atr = new Articulation(score);
                  atr->setUp(true);
                  atr->setSymId(SymId::fermataAbove);
                  chord->add(atr);
                  }
            else if (an[i] == 'E') {
                  Articulation* atr = new Articulation(score);
                  atr->setUp(false);
                  atr->setSymId(SymId::fermataBelow);
                  chord->add(atr);
                  }
            else if (an[i] == 'O') {
                  // Articulation* atr = new Articulation(score);
                  // atr->setArticulationType(ArticulationType::Downbow);
                  // chord->add(atr);
                  qDebug("%06d: open string '%c' not implemented", tick.ticks(), an[i].toLatin1());
                  }
            else if (an[i] == '&') {
                  // skip editorial level
                  if (i <= an.size() && an[i+1].isDigit())
                        ++i;
                  }
            else if (an[i] == 'p')
                  dynamics += "p";
            else if (an[i] == 'm')
                  dynamics += "m";
            else if (an[i] == 'f')
                  dynamics += "f";
            else if (an[i] == '-')        // tie
                  ;
            else if (an[i] == '*')        // start tuplet
                  ;
            else if (an[i] == '!')        // stop tuplet
                  ;
            else if (an[i] == '+')        // cautionary accidental
                  ;
            else if (an[i] == 'X')        // ???
                  ;
            else if (an[i] == ' ')
                  ;
            else {
                  qDebug("%06d: notation '%c' not implemented", tick.ticks(), an[i].toLatin1());
                  }
            }
      if (!dynamics.isEmpty()) {
            Dynamic* dyn = new Dynamic(score);
            dyn->setDynamicType(dynamics);
            dyn->setTrack(gstaff * VOICES);
            Segment* seg = measure->getSegment(SegmentType::ChordRest, tick);
            seg->add(dyn);
            }

      QString txt = s.mid(43, 36);
      if (!txt.isEmpty()) {
            QStringList sl = txt.split("|");
            int no = 0;
            foreach(QString w, sl) {
                  w = diacritical(w);
                  Lyrics* l = new Lyrics(score);
                  l->setPlainText(w);
                  l->setNo(no++);
                  l->setTrack(gstaff * VOICES);
                  Segment* seg = measure->tick2segment(tick);
                  seg->add(l);
                  }
            }
      }

//---------------------------------------------------------
//   diacritical
// TODO: not complete
//---------------------------------------------------------

QString MuseData::diacritical(QString s)
      {
      return s;
      }

//---------------------------------------------------------
//   readRest
//---------------------------------------------------------

void MuseData::readRest(Part* part, const QString& s)
      {
      Fraction ticks = Fraction::fromTicks((s.midRef(5, 3).toInt() * MScore::division + _division/2) / _division);

      Fraction tick  = curTick;
      curTick  += ticks;

      int staffIdx = 0;
      if (s.size() >= 24) {
            if (s[23].isDigit())
                  staffIdx = s.midRef(23,1).toInt() - 1;
            }
      Staff* staff = part->staff(staffIdx);
      int gstaff   = staff->idx();

      TDuration d;
      d.setVal(ticks.ticks());
      Rest* rest = new Rest(score, d);
      rest->setTicks(d.fraction());
      chordRest  = rest;
      rest->setTrack(gstaff * VOICES);
      Segment* segment = measure->getSegment(SegmentType::ChordRest, tick);

      voice = 0;
      for (; voice < VOICES; ++voice) {
            Element* e = segment->element(gstaff * VOICES + voice);
            if (e == 0) {
                  rest->setTrack(gstaff * VOICES + voice);
                  segment->add(rest);
                  break;
                  }
            }
      if (voice == VOICES) {
            qDebug("cannot allocate voice");
            delete rest;
            return;
            }
      }

//---------------------------------------------------------
//   readBackup
//---------------------------------------------------------

void MuseData::readBackup(const QString& s)
      {
      Fraction ticks = Fraction::fromTicks((s.midRef(5, 3).toInt() * MScore::division + _division/2) / _division);
      if (s[0] == 'b')
            curTick  -= ticks;
      else
            curTick += ticks;
      }

//---------------------------------------------------------
//   createMeasure
//---------------------------------------------------------

Measure* MuseData::createMeasure()
      {
      for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
            if (mb->type() != ElementType::MEASURE)
                  continue;
            Measure* m = (Measure*)mb;
            Fraction st = m->tick();
            Fraction l  = m->ticks();
            if (curTick == st)
                  return m;
            if (curTick > st && curTick < (st+l)) {
                  // irregular measure
#if 0 // TODO
                  Fraction f = score->sigmap()->timesig(st).fraction();
                  score->sigmap()->add(st, curTick - st, f);
                  score->sigmap()->add(curTick, f);
#endif
                  break;
                  }
            if (curTick < st + l) {
                  qDebug("cannot create measure at %d", curTick.ticks());
                  return 0;
                  }
            }
      Measure* mes  = new Measure(score);
      mes->setTick(curTick);

#if 0
      foreach(Staff* s, score->staves()) {
            if (s->isTop()) {
                  BarLine* barLine = new BarLine(score);
                  barLine->setStaff(s);
                  mes->setEndBarLine(barLine);
                  }
            }
#endif
      score->measures()->add(mes);
      return mes;
      }

//---------------------------------------------------------
//   readPart
//---------------------------------------------------------

void MuseData::readPart(QStringList sl, Part* part)
      {
      int line = 10;
      QString s;
      for (; line < sl.size(); ++line) {
            s = sl[line];
            if (!s.isEmpty() && s[0] == '$')
                  break;
            }
      if (line >= sl.size()) {
            qDebug(" $ not found in part");
            return;
            }
      curTick = Fraction(0,1);
      slur[0] = 0;
      slur[1] = 0;
      slur[2] = 0;
      slur[3] = 0;
      measure = 0;
      measure = createMeasure();
      for (; line < sl.size(); ++line) {
            s = sl[line];
            // qDebug("%6d: <%s>", curTick.ticks(), qPrintable(s));
            char c = s[0].toLatin1();
            switch (c) {
                  case 'A':
                  case 'B':
                  case 'C':
                  case 'D':
                  case 'E':
                  case 'F':
                  case 'G':
                        readNote(part, s);
                        break;
                  case ' ':         // chord
                        readChord(part, s);
                        break;
                  case 'r':
                        readRest(part, s);
                        break;
                  case 'g':         // grace note
                  case 'c':         // cue note
                  case 'f':         // basso continuo
                        break;
                  case 'b':         // backspace
                  case 'i':         // forward space
                        readBackup(s);
                        break;
                  case 'm':         // measure line / bar line
                        measure = createMeasure();
                        break;
                  case '*':         // musical direction
                        break;
                  case 'P':         // print suggestion
                        break;
                  case 'S':         // sound record
                        break;
                  case '$':
                        musicalAttribute(s, part);
                        break;
                  default:
                        qDebug("unknown record <%s>", qPrintable(s));
                        break;
                  }
            }
      }

//---------------------------------------------------------
//   countStaves
//---------------------------------------------------------

int MuseData::countStaves(const QStringList& sl)
      {
      int staves = 1;
      for (int i = 10; i < sl.size(); ++i) {
            QString s = sl[i];
            char c = s[0].toLatin1();
            switch (c) {
                  case 'A':
                  case 'B':
                  case 'C':
                  case 'D':
                  case 'E':
                  case 'F':
                  case 'G':
                  case 'r':
                        {
                        int staffIdx = 1;
                        if (s.size() >= 24) {
                              if (s[23].isDigit())
                                    staffIdx = s.midRef(23,1).toInt();
                              }
                        if (staffIdx > staves)
                              staves = staffIdx;
                        }
                        break;
                  }
            }
      return staves;
      }

//---------------------------------------------------------
//   read
//    return false on error
//---------------------------------------------------------

bool MuseData::read(const QString& name)
      {
      QFile fp(name);
      if (!fp.open(QIODevice::ReadOnly)) {
            qDebug("Cannot open file <%s>", qPrintable(name));
            return false;
            }
      QTextStream ts(&fp);
      QStringList part;
      bool commentMode = false;
      for (;; ) {
            QString s(ts.readLine());
            if (s.isNull())
                  break;
            if (s.isEmpty()) {
                  if (!commentMode)
                        part.append(QString(""));
                  continue;
                  }
            if (s[0] == '&') {
                  commentMode = !commentMode;
                  continue;
                  }
            if (commentMode)
                  continue;
            if (s[0] == '@')
                  continue;
            if (s[0] == '/') {
                  parts.append(part);

                  Part* mpart = new Part(score);
                  int staves  = countStaves(part);
                  for (int i = 0; i < staves; ++i) {
                        Staff* staff = new Staff(score);
                        staff->setPart(mpart);
                        mpart->insertStaff(staff, i);
                        score->staves().push_back(staff);
                        if ((staves == 2) && (i == 0)) {
                              staff->setBracketType(0, BracketType::BRACE);
                              staff->setBracketSpan(0, 2);
                              }
                        }
                  score->appendPart(mpart);
                  if (part.size() > 8)
                        mpart->setPlainLongName(part[8]);
                  part.clear();
                  continue;
                  }
            if (s[0] == 'a') {
                  part.back().append(s.midRef(1));
                  continue;
                  }
            part.append(s);
            }
      fp.close();
      return true;
      }

//---------------------------------------------------------
//   convert
//---------------------------------------------------------

void MuseData::convert()
      {
      for (int pn = 0; pn < parts.size(); ++pn) {
            Part* part = (score->parts())[pn];
            readPart(parts[pn], part);
            }
#if 0
      // crash if system does not fit on page (too many instruments)

      Measure* measure = score->tick2measure(0);
      if (measure) {
            Text* text = new Text(score);
            text->setSubtype(TEXT_TITLE);
            text->setText(parts[0][6]);
            text->setText("mops");
            measure->add(text);
            text = new Text(score);
            text->setSubtype(TEXT_SUBTITLE);
            text->setText(parts[0][6]);
            measure->add(text);
            }
#endif
      }

//---------------------------------------------------------
//   mnxValueUnitToDurationType
//---------------------------------------------------------

/**
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
            return TDuration::DurationType::V_INVALID;
            }
      }

//---------------------------------------------------------
//   mnxEventValueToTDuration
//---------------------------------------------------------

/**
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
//   addCrToTuplet
//---------------------------------------------------------

static void addCrToTuplet(ChordRest* cr, Tuplet* tuplet)
      {
      cr->setTuplet(tuplet);
      tuplet->add(cr);
      }

//---------------------------------------------------------
//   createChord
//---------------------------------------------------------

Chord* createChord(Score* score, const QString& value, const Fraction& duration)
      {
      auto dur = mnxEventValueToTDuration(value);
      auto chord = new Chord(score);
      chord->setTrack(0);             // TODO
      chord->setDurationType(dur);
      chord->setTicks(duration.isValid() ? duration : dur.fraction());        // specified duration overrules value-based
      chord->setDots(dur.dots());
      return chord;
      }

//---------------------------------------------------------
//   createNote
//---------------------------------------------------------

Note* createNote(Score* score, const int pitch)
      {
      auto note = new Note(score);
      note->setTrack(0);            // TODO
      note->setPitch(pitch);
      note->setTpcFromPitch();
      return note;
      }

//---------------------------------------------------------
//   createTimeSig
//---------------------------------------------------------

TimeSig* createTimeSig(Score* score, const Fraction& sig)
      {
      auto timesig = new TimeSig(score);
      timesig->setSig(sig);
      timesig->setTrack(0);                  // TODO
      return timesig;
      }

//---------------------------------------------------------
//   createTuplet
//---------------------------------------------------------

Tuplet* createTuplet(Score* score, const int track)
      {
      auto tuplet = new Tuplet(score);
      tuplet->setTrack(track);
      return tuplet;
      }

//---------------------------------------------------------
//   setTupletParameters
//---------------------------------------------------------

static void setTupletParameters(Tuplet* tuplet, const int actual, const int normal, const TDuration::DurationType base)
      {
      tuplet->setRatio({ actual, normal });
      tuplet->setBaseLen(base);
      }

//---------------------------------------------------------
//   JsonTuplet
//---------------------------------------------------------

class JsonEvent
      {
public:
      JsonEvent() {}
      Fraction read(const QJsonObject& json, Measure* const measure, const Fraction& tick, Tuplet* tuplet);
private:
      };

class JsonTuplet
      {
public:
      JsonTuplet() {}
      Fraction read(const QJsonObject& json, Measure* const measure, const Fraction& tick, Tuplet* tuplet);
private:
      };

Fraction JsonTuplet::read(const QJsonObject& json, Measure* const measure, const Fraction& tick, Tuplet* tuplet)
      {
      qDebug("JsonTuplet::read() rtick %s",
             qPrintable(tick.print())
             );
      Fraction tupTime { 0, 1 };             // time in this tuplet
      QJsonArray array = json["events"].toArray();
      for (int i = 0; i < array.size(); ++i) {
            QJsonObject object = array[i].toObject();
            JsonEvent event;
            tupTime += event.read(object, measure, tick + tupTime, tuplet);
            //qDebug("tupTime %s", qPrintable(tupTime.print()));
            }
      return tupTime;
      }

//---------------------------------------------------------
//   JsonEvent
//---------------------------------------------------------

// read a single event and return its length
// pitch is 0-based, see pitchIsValid() in pitchspelling.h
// => C4 = 60
// note type, duration and increment can be set (kind of) independently
// duration: sets DurationElement::_duration (defaults to "as calculated from value")
// set by (in order of decreasing priority):
// - the duration specified
// - the value specified
// increment: sets the time increment to the next note (current to next note's Segment::_tick )
// set by (in order of decreasing priority):
// - the increment specified
// - the duration specified
// - the value specified
Fraction JsonEvent::read(const QJsonObject& json, Measure* const measure, const Fraction& tick, Tuplet* tuplet)
      {
      qDebug("JsonEvent::read() rtick %s tuplet %p value '%s' duration '%s' increment '%s' pitch '%s' baselen '%s' ratio '%s'",
             qPrintable(tick.print()),
             tuplet,
             qPrintable(json["value"].toString()),
             qPrintable(json["duration"].toString()),
             qPrintable(json["increment"].toString()),
             qPrintable(json["pitch"].toString()),
             qPrintable(json["baselen"].toString()),
             qPrintable(json["ratio"].toString())
             );
      if (json.contains("baselen") && json.contains("ratio")) {
            // tuplet found
            const auto baselen = mnxEventValueToTDuration(json["baselen"].toString());
            const auto ratio = Fraction::fromString(json["ratio"].toString());
            //qDebug("baselen %s ratio %s", qPrintable(baselen.fraction().print()), qPrintable(ratio.print()));
            // create the tuplet
            auto newTuplet = createTuplet(measure->score(), 0 /* TODO track */);
            //qDebug("newTuplet %p ratio %s", newTuplet, qPrintable(ratio.print()));
            newTuplet->setParent(measure);
            newTuplet->setTuplet(tuplet);
            setTupletParameters(newTuplet, ratio.numerator(), ratio.denominator(), baselen.type());
            if (tuplet) {
                  newTuplet->setTuplet(tuplet);
                  tuplet->add(newTuplet);
            }
            JsonTuplet jsonTuplet;
            return jsonTuplet.read(json, measure, tick, newTuplet);
            }
      else {
            Fraction duration { 0, 0 };   // initialize invalid to catch missing duration
            if (json.contains("duration")) {
                  duration = Fraction::fromString(json["duration"].toString());
                  qDebug("duration %s", qPrintable(duration.print()));
                  }
            Fraction increment { 0, 0 };   // initialize invalid to catch missing increment
            if (json.contains("increment")) {
                  increment = Fraction::fromString(json["increment"].toString());
                  qDebug("increment %s", qPrintable(increment.print()));
                  }
            auto cr = createChord(measure->score(), json["value"].toString(), duration);
            bool ok { true };
            int pitch { json["pitch"].toString().toInt(&ok) };
            //qDebug("ok %d pitch %d", ok, pitch);
            cr->add(createNote(measure->score(), pitch));
            auto s = measure->getSegment(SegmentType::ChordRest, tick);
            s->add(cr);
            if (tuplet) {
                  cr->setTuplet(tuplet);
                  tuplet->add(cr);
                  }

            // todo: check if following supports using increment in a tuplet
            auto res = increment.isValid() ? increment : cr->ticks();
            //qDebug("res %s", qPrintable(res.print()));
            for (Tuplet* t = cr-> tuplet(); t; t = t->tuplet()) {
                  res /= t->ratio();
                  //qDebug("t %p ratio %s res %s", t, qPrintable(t->ratio().print()), qPrintable(res.print()));
            }
            //qDebug("res %s", qPrintable(res.print()));
            return res;
            }
      }

//---------------------------------------------------------
//   JsonSequence
//---------------------------------------------------------

class JsonSequence
      {
public:
      JsonSequence() {}
      Fraction read(const QJsonObject& json, Measure* const measure, const Fraction& startTick);
private:
      };

// read a single sequence (a.k.a. voice)
// assume all voices start at relative tick 0 in its measure
Fraction JsonSequence::read(const QJsonObject& json, Measure* const measure, const Fraction& startTick)
      {
      qDebug("JsonSequence::read()");
      Fraction tick { startTick };
      QJsonArray array = json["events"].toArray();
      for (int i = 0; i < array.size(); ++i) {
            QJsonObject object = array[i].toObject();
            JsonEvent event;
            tick += event.read(object, measure, tick, nullptr);
            }
      return {};       // TODO to suppurt nested sequences
      }

//---------------------------------------------------------
//   JsonMeasure
//---------------------------------------------------------

class JsonMeasure
      {
public:
      JsonMeasure() {}
      Fraction read(MasterScore* score, const QJsonObject& json, const Fraction& timeSig, const Fraction& startTick);
private:
      };

// read a single measure and return its length
Fraction JsonMeasure::read(MasterScore* const score, const QJsonObject& json, const Fraction& timeSig, const Fraction& startTick)
      {
      qDebug("JsonMeasure::read()");
      auto m = new Measure(score);
      m->setTick(startTick);
      m->setTimesig(timeSig);
      score->measures()->add(m);
      if (startTick == Fraction { 0, 1 }) {
            auto ts = createTimeSig(score, timeSig);
            auto s = m->getSegment(SegmentType::TimeSig, { 0, 1 });
            s->add(ts);
            }
      QJsonArray array = json["sequences"].toArray();
      for (int i = 0; i < array.size(); ++i) {
            QJsonObject object = array[i].toObject();
            JsonSequence sequence;
            sequence.read(object, m, startTick);
            }
      const auto length = timeSig;             // TODO: use real length instead ?
      m->setTicks(length);
      return length;
      }

//---------------------------------------------------------
//   JsonScore
//---------------------------------------------------------

class JsonScore
      {
public:
      JsonScore() {}
      void read(MasterScore* score, const QJsonObject& json);
private:
      };

void JsonScore::read(MasterScore* const score, const QJsonObject& json)
      {
      qDebug("JsonScore::read()");
      // TODO move temporary part / staff creation
      auto part = new Part(score);
      part->setId("dbg");
      score->appendPart(part);
      auto staff = new Staff(score);
      staff->setPart(part);
      part->staves()->push_back(staff);
      score->staves().push_back(staff);
      Fraction timeSig { 4, 4 };       // default: assume all measures are 4/4
      if (json.contains("time")) {
            timeSig = Fraction::fromString(json["time"].toString());
            qDebug("timesig %s", qPrintable(timeSig.print()));
            }
      QJsonArray array = json["measures"].toArray();
      for (int i = 0; i < array.size(); ++i) {
            QJsonObject object = array[i].toObject();
            JsonMeasure measure;
            measure.read(score, object, timeSig, timeSig * i);
            }
      }

//---------------------------------------------------------
//   importMuseData
//    return true on success
//---------------------------------------------------------

Score::FileError importMuseData(MasterScore* score, const QString& name)
      {
      qDebug("Score::importMuseData(%s)", qPrintable(name));
#if 0
      if (!QFileInfo::exists(name))
            return Score::FileError::FILE_NOT_FOUND;
      MuseData md(score);
      if (!md.read(name))
            return Score::FileError::FILE_ERROR;
      md.convert();
#else
      QFile file(name);
      if (!file.open(QIODevice::ReadOnly)) {
            return Score::FileError::FILE_ERROR;
            }
      QByteArray data = file.readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      JsonScore jsonScore;
      jsonScore.read(score, doc.object());
#endif
      // all done
      qDebug("Score::importMuseData() done");
      return Score::FileError::FILE_NO_ERROR;
      }
}

