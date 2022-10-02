//=============================================================================
//  MuseScore
//  Linux Music Score Editor
//
//  Copyright (C) 2015 Werner Schweer and others
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
#include "libmscore/chordrest.h"
#include "libmscore/instrtemplate.h"
#include "libmscore/measure.h"
#include "libmscore/page.h"
#include "libmscore/part.h"
#include "libmscore/staff.h"
#include "libmscore/stringdata.h"
#include "libmscore/sym.h"
#include "libmscore/symbol.h"
#include "libmscore/timesig.h"
#include "libmscore/style.h"
#include "libmscore/spanner.h"
#include "libmscore/bracketItem.h"

#include "importmxmllogger.h"
#include "importmxmlnoteduration.h"
#include "importmxmlpass1.h"
#include "importmxmlpass2.h"

#include "mscore/preferences.h"

namespace Ms {

//---------------------------------------------------------
//   allocateStaves
//---------------------------------------------------------

/**
 Allocate MuseScore staff to MusicXML voices.
 For each staff, allocate at most VOICES voices to the staff.
 */

// for regular (non-overlapping) voices:
// 1) assign voice to a staff (allocateStaves)
// 2) assign voice numbers (allocateVoices)
// due to cross-staving, it is not a priori clear to which staff
// a voice has to be assigned
// allocate ordered by number of chordrests in the MusicXML voice
//
// for overlapping voices:
// 1) assign voice to staves it is found in (allocateStaves)
// 2) assign voice numbers (allocateVoices)

static void allocateStaves(VoiceList& vcLst)
      {
      // initialize
      int voicesAllocated[MAX_STAVES]; // number of voices allocated on each staff
      for (int i = 0; i < MAX_STAVES; ++i)
            voicesAllocated[i] = 0;

      // handle regular (non-overlapping) voices
      // note: outer loop executed vcLst.size() times, as each inner loop handles exactly one item
      for (int i = 0; i < vcLst.size(); ++i) {
            // find the regular voice containing the highest number of chords and rests that has not been handled yet
            int max = 0;
            QString key;
            for (VoiceList::const_iterator j = vcLst.constBegin(); j != vcLst.constEnd(); ++j) {
                  if (!j.value().overlaps() && j.value().numberChordRests() > max && j.value().staff() == -1) {
                        max = j.value().numberChordRests();
                        key = j.key();
                        }
                  }
            if (key != "") {
                  int prefSt = vcLst.value(key).preferredStaff();
                  if (voicesAllocated[prefSt] < VOICES) {
                        vcLst[key].setStaff(prefSt);
                        voicesAllocated[prefSt]++;
                        }
                  else
                        // out of voices: mark as used but not allocated
                        vcLst[key].setStaff(-2);
                  }
            }

      // handle overlapping voices
      // for every staff allocate remaining voices (if space allows)
      // the ones with the highest number of chords and rests get allocated first
      for (int h = 0; h < MAX_STAVES; ++h) {
            // note: middle loop executed vcLst.size() times, as each inner loop handles exactly one item
            for (int i = 0; i < vcLst.size(); ++i) {
                  // find the overlapping voice containing the highest number of chords and rests that has not been handled yet
                  int max = 0;
                  QString key;
                  for (VoiceList::const_iterator j = vcLst.constBegin(); j != vcLst.constEnd(); ++j) {
                        if (j.value().overlaps() && j.value().numberChordRests(h) > max && j.value().staffAlloc(h) == -1) {
                              max = j.value().numberChordRests(h);
                              key = j.key();
                              }
                        }
                  if (key != "") {
                        int prefSt = h;
                        if (voicesAllocated[prefSt] < VOICES) {
                              vcLst[key].setStaffAlloc(prefSt, 1);
                              voicesAllocated[prefSt]++;
                              }
                        else
                              // out of voices: mark as used but not allocated
                              vcLst[key].setStaffAlloc(prefSt, -2);
                        }
                  }
            }
      }

//---------------------------------------------------------
//   allocateVoices
//---------------------------------------------------------

/**
 Allocate MuseScore voice to MusicXML voices.
 For each staff, the voices are number 1, 2, 3, 4
 in the same order they are numbered in the MusicXML file.
 */

static void allocateVoices(VoiceList& vcLst)
      {
      int nextVoice[MAX_STAVES]; // number of voices allocated on each staff
      for (int i = 0; i < MAX_STAVES; ++i)
            nextVoice[i] = 0;
      // handle regular (non-overlapping) voices
      // a voice is allocated on one specific staff
      for (VoiceList::const_iterator i = vcLst.constBegin(); i != vcLst.constEnd(); ++i) {
            int staff = i.value().staff();
            QString key   = i.key();
            if (staff >= 0) {
                  vcLst[key].setVoice(nextVoice[staff]);
                  nextVoice[staff]++;
                  }
            }
      // handle overlapping voices
      // each voice may be in every staff
      for (VoiceList::const_iterator i = vcLst.constBegin(); i != vcLst.constEnd(); ++i) {
            for (int j = 0; j < MAX_STAVES; ++j) {
                  int staffAlloc = i.value().staffAlloc(j);
                  QString key   = i.key();
                  if (staffAlloc >= 0) {
                        vcLst[key].setVoice(j, nextVoice[j]);
                        nextVoice[j]++;
                        }
                  }
            }
      }


//---------------------------------------------------------
//   copyOverlapData
//---------------------------------------------------------

/**
 Copy the overlap data from the overlap detector to the voice list.
 */

static void copyOverlapData(VoiceOverlapDetector& vod, VoiceList& vcLst)
      {
      for (VoiceList::const_iterator i = vcLst.constBegin(); i != vcLst.constEnd(); ++i) {
            QString key = i.key();
            if (vod.stavesOverlap(key))
                  vcLst[key].setOverlap(true);
            }
      }

//---------------------------------------------------------
//   MusicXMLParserPass1
//---------------------------------------------------------

MusicXMLParserPass1::MusicXMLParserPass1(Score* score, MxmlLogger* logger)
      : _divs(0), _score(score), _logger(logger), _hasBeamingInfo(false)
      {
      // nothing
      }

//---------------------------------------------------------
//   initPartState
//---------------------------------------------------------

/**
 Initialize members as required for reading the MusicXML part element.
 TODO: factor out part reading into a separate class
 TODO: preferably use automatically initialized variables
 Note that Qt automatically initializes new elements in QVector (tuplets).
 */

void MusicXMLParserPass1::initPartState(const QString& /* partId */)
      {
      _timeSigDura = Fraction(0, 0);       // invalid
      _octaveShifts.clear();
      }

//---------------------------------------------------------
//   determineMeasureLength
//---------------------------------------------------------

/**
 Determine the length in ticks of each measure in all parts.
 Return false on error.
 */

bool MusicXMLParserPass1::determineMeasureLength(QVector<Fraction>& ml) const
      {
      ml.clear();

      // determine number of measures: max number of measures in any part
      int nMeasures = 0;
      foreach (const MusicXmlPart &part, _parts) {
            if (part.nMeasures() > nMeasures)
                  nMeasures = part.nMeasures();
            }

      // determine max length of a specific measure in all parts
      for (int i = 0; i < nMeasures; ++i) {
            Fraction maxMeasDur;
            foreach (const MusicXmlPart &part, _parts) {
                  if (i < part.nMeasures()) {
                        Fraction measDurPartJ = part.measureDuration(i);
                        if (measDurPartJ > maxMeasDur)
                              maxMeasDur = measDurPartJ;
                        }
                  }
            //qDebug("determineMeasureLength() measure %d %s (%d)", i, qPrintable(maxMeasDur.print()), maxMeasDur.ticks());
            ml.append(maxMeasDur);
            }
      return true;
      }

//---------------------------------------------------------
//   getVoiceList
//---------------------------------------------------------

/**
 Get the VoiceList for part \a id.
 Return an empty VoiceList on error.
 */

VoiceList MusicXMLParserPass1::getVoiceList(const QString id) const
      {
      if (_parts.contains(id))
            return _parts.value(id).voicelist;
      return VoiceList();
      }

//---------------------------------------------------------
//   getInstrList
//---------------------------------------------------------

/**
 Get the MusicXmlInstrList for part \a id.
 Return an empty MusicXmlInstrList on error.
 */

MusicXmlInstrList MusicXMLParserPass1::getInstrList(const QString id) const
      {
      if (_parts.contains(id))
            return _parts.value(id)._instrList;
      return MusicXmlInstrList();
      }

//---------------------------------------------------------
//   getIntervals
//---------------------------------------------------------

/**
 Get the MusicXmlIntervalList for part \a id.
 Return an empty MusicXmlIntervalList on error.
 */

MusicXmlIntervalList MusicXMLParserPass1::getIntervals(const QString id) const
      {
      if (_parts.contains(id))
            return _parts.value(id)._intervals;
      return MusicXmlIntervalList();
      }

//---------------------------------------------------------
//   determineMeasureLength
//---------------------------------------------------------

/**
 Set default notehead, line and stem direction
 for instrument \a instrId in part \a id.
 Called from pass 2, notehead, line and stemDirection are not read in pass 1.
 */

void MusicXMLParserPass1::setDrumsetDefault(const QString& id,
                                            const QString& instrId,
                                            const NoteHead::Group hg,
                                            const int line,
                                            const Direction sd)
      {
      if (_instruments.contains(id)
          && _instruments[id].contains(instrId)) {
            _instruments[id][instrId].notehead = hg;
            _instruments[id][instrId].line = line;
            _instruments[id][instrId].stemDirection = sd;
            }
      }


//---------------------------------------------------------
//   determineStaffMoveVoice
//---------------------------------------------------------

/**
 For part \a id, determine MuseScore (ms) staffmove, track and voice from MusicXML (mx) staff and voice
 MusicXML staff is 0 for the first staff, 1 for the second.
 Note: track is the first track of the ms staff in the score, add ms voice for elements in a voice
 Return true if OK, false on error
 TODO: finalize
 */

bool MusicXMLParserPass1::determineStaffMoveVoice(const QString& id, const int mxStaff, const QString& mxVoice,
                                                  int& msMove, int& msTrack, int& msVoice) const
      {
      VoiceList voicelist = getVoiceList(id);
      msMove = 0; // TODO
      msTrack = 0; // TODO
      msVoice = 0; // TODO


      // Musicxml voices are counted for all staves of an
      // instrument. They are not limited. In mscore voices are associated
      // with a staff. Every staff can have at most VOICES voices.

      // The following lines map musicXml voices to mscore voices.
      // If a voice crosses two staves, this is expressed with the
      // "move" parameter in mscore.

      // Musicxml voices are unique within a part, but not across parts.

      //qDebug("voice mapper before: voice='%s' staff=%d", qPrintable(mxVoice), mxStaff);
      int s; // staff mapped by voice mapper
      int v; // voice mapped by voice mapper
      if (voicelist.value(mxVoice).overlaps()) {
            // for overlapping voices, the staff does not change
            // and the voice is mapped and staff-dependent
            s = mxStaff;
            v = voicelist.value(mxVoice).voice(s);
            }
      else {
            // for non-overlapping voices, both staff and voice are
            // set by the voice mapper
            s = voicelist.value(mxVoice).staff();
            v = voicelist.value(mxVoice).voice();
            }

      //qDebug("voice mapper mapped: s=%d v=%d", s, v);
      if (s < 0 || v < 0) {
            qDebug("too many voices (staff=%d voice='%s' -> s=%d v=%d)",
                   mxStaff + 1, qPrintable(mxVoice), s, v);
            return false;
            }

      msMove  = mxStaff - s;
      msVoice = v;

      // make score-relative instead on part-relative
      Part* part = _partMap.value(id);
      Q_ASSERT(part);
      int scoreRelStaff = _score->staffIdx(part); // zero-based number of parts first staff in the score
      msTrack = (scoreRelStaff + s) * VOICES;

      //qDebug("voice mapper after: scoreRelStaff=%d partRelStaff=%d msMove=%d msTrack=%d msVoice=%d",
      //       scoreRelStaff, s, msMove, msTrack, msVoice);
      // note: relStaff is the staff number relative to the parts first staff
      //       voice is the voice number in the staff

      return true;
      }

//---------------------------------------------------------
//   hasPart
//---------------------------------------------------------

/**
 Check if part \a id is found.
 */

bool MusicXMLParserPass1::hasPart(const QString& id) const
      {
      return _parts.contains(id);
      }

//---------------------------------------------------------
//   trackForPart
//---------------------------------------------------------

/**
 Return the (score relative) track number for the first staff of part \a id.
 */

int MusicXMLParserPass1::trackForPart(const QString& id) const
      {
      Part* part = _partMap.value(id);
      Q_ASSERT(part);
      int scoreRelStaff = _score->staffIdx(part); // zero-based number of parts first staff in the score
      return scoreRelStaff * VOICES;
      }

//---------------------------------------------------------
//   getMeasureStart
//---------------------------------------------------------

/**
 Return the measure start time for measure \a i.
 */

Fraction MusicXMLParserPass1::getMeasureStart(const int i) const
      {
      if (0 <= i && i < _measureStart.size())
            return _measureStart.at(i);
      else
            return Fraction(0, 0);       // invalid
      }

//---------------------------------------------------------
//   octaveShift
//---------------------------------------------------------

/**
 Return the octave shift for part \a id in \a staff at \a f.
 */

int MusicXMLParserPass1::octaveShift(const QString& id, const int staff, const Fraction f) const
      {
      if (_parts.contains(id))
            return _parts.value(id).octaveShift(staff, f);

      return 0;
      }

//---------------------------------------------------------
//   skipLogCurrElem
//---------------------------------------------------------

/**
 Skip the current element, log debug as info.
 */

void MusicXMLParserPass1::skipLogCurrElem()
      {
      _logger->logDebugInfo(QString("skipping '%1'").arg(_e.name().toString()), &_e);
      _e.skipCurrentElement();
      }

//---------------------------------------------------------
//   addBreak
//---------------------------------------------------------

static void addBreak(Score* const score, MeasureBase* const mb, const LayoutBreak::Type type)
      {
      LayoutBreak* lb = new LayoutBreak(score);
      lb->setLayoutBreakType(type);
      mb->add(lb);
      }

//---------------------------------------------------------
//   addBreakToPreviousMeasureBase
//---------------------------------------------------------

static void addBreakToPreviousMeasureBase(Score* const score, MeasureBase* const mb, const LayoutBreak::Type type)
      {
      const auto pm = mb->prev();
      if (pm && preferences.getBool(PREF_IMPORT_MUSICXML_IMPORTBREAKS))
            addBreak(score, pm, type);
      }

//---------------------------------------------------------
//   addText
//---------------------------------------------------------

/**
 Add text \a strTxt to VBox \a vbx using Tid \a stl.
 */

static void addText(VBox* vbx, Score* s, const QString strTxt, const Tid stl)
      {
      if (!strTxt.isEmpty()) {
            Text* text = new Text(s, stl);
            text->setXmlText(strTxt);
            vbx->add(text);
            }
      }

//---------------------------------------------------------
//   addText2
//---------------------------------------------------------

/**
 Add text \a strTxt to VBox \a vbx using Tid \a stl.
 Also sets Align and Yoff.
 */

static void addText2(VBox* vbx, Score* s, const QString strTxt, const Tid stl, const Align align, const double yoffs)
      {
      if (!strTxt.isEmpty()) {
            Text* text = new Text(s, stl);
            text->setXmlText(strTxt);
            text->setAlign(align);
            text->setPropertyFlags(Pid::ALIGN, PropertyFlags::UNSTYLED);
            text->setOffset(QPointF(0.0, yoffs));
            text->setPropertyFlags(Pid::OFFSET, PropertyFlags::UNSTYLED);
            vbx->add(text);
            }
      }

//---------------------------------------------------------
//   findYMinYMaxInWords
//---------------------------------------------------------

static void findYMinYMaxInWords(const std::vector<const CreditWords*>& words, int& miny, int& maxy)
      {
      miny = 0;
      maxy = 0;

      if (words.empty())
            return;

      miny = words.at(0)->defaultY;
      maxy = words.at(0)->defaultY;
      for (const auto w : words) {
            if (w->defaultY < miny) miny = w->defaultY;
            if (w->defaultY > maxy) maxy = w->defaultY;
            }
      }

//---------------------------------------------------------
//   alignForCreditWords
//---------------------------------------------------------

static Align alignForCreditWords(const CreditWords* const w, const int pageWidth)
      {
      Align align = Align::LEFT;
      if (w->defaultX > (pageWidth / 3)) {
            if (w->defaultX < (2 * pageWidth / 3))
                  align = Align::HCENTER;
            else
                  align = Align::RIGHT;
            }
      return align;
      }

//---------------------------------------------------------
//   creditWordTypeToTid
//---------------------------------------------------------

static Tid creditWordTypeToTid(const QString& type)
      {
      if (type == "composer")
            return Tid::COMPOSER;
      else if (type == "lyricist")
            return Tid::POET;
      /*
      else if (type == "page number")
            return Tid::;
      else if (type == "rights")
            return Tid::;
       */
      else if (type == "subtitle")
            return Tid::SUBTITLE;
      else if (type == "title")
            return Tid::TITLE;
      else
            return Tid::DEFAULT;
      }

//---------------------------------------------------------
//   creditWordTypeGuess
//---------------------------------------------------------

static Tid creditWordTypeGuess(const CreditWords* const word, std::vector<const CreditWords*>& words, const int pageWidth)
      {
      const auto pw1 = pageWidth / 3;
      const auto pw2 = pageWidth * 2 / 3;
      const auto defx = word->defaultX;
      // composer is in the right column
      if (pw2 < defx) {
            // found composer
            return Tid::COMPOSER;
            }
      // poet is in the left column
      else if (defx < pw1) {
            // found poet/lyricist
            return Tid::POET;
            }
      // title is in the middle column
      else {
            // if another word in the middle column has a larger font size, this word is not the title
            for (const auto w : words) {
                  if (w == word) {
                        continue;         // it's me
                        }
                  if (w->defaultX < pw1 || pw2 < w->defaultX) {
                        continue;         // it's not in the middle column
                        }
                  if (word->fontSize < w->fontSize) {
                        return Tid::SUBTITLE;          // word does not have the largest font size, assume subtitle
                        }
                  }
            return Tid::TITLE;            // no better title candidate found
            }
      }

//---------------------------------------------------------
//   tidForCreditWords
//---------------------------------------------------------

static Tid tidForCreditWords(const CreditWords* const word, std::vector<const CreditWords*>& words, const int pageWidth)
      {
      const Tid tid = creditWordTypeToTid(word->type);
      if (tid != Tid::DEFAULT) {
            // type recognized, done
            return tid;
            }
      else {
            // type not recognized, guess
            return creditWordTypeGuess(word, words, pageWidth);
            }
      }

//---------------------------------------------------------
//   createAndAddVBoxForCreditWords
//---------------------------------------------------------

static VBox* createAndAddVBoxForCreditWords(Score* const score, const int miny = 0, const int maxy = 75)
      {
      auto vbox = new VBox(score);
      qreal vboxHeight = 10;                         // default height in tenths
      double diff = maxy - miny;                     // calculate height in tenths
      if (diff > vboxHeight)                         // and size is reasonable
            vboxHeight = diff;
      vboxHeight /= 10;                              // height in spatium
      vboxHeight += 2.5;                             // guesstimated correction for last line

      vbox->setBoxHeight(Spatium(vboxHeight));
      score->measures()->add(vbox);
      return vbox;
      }

//---------------------------------------------------------
//   mustAddWordToVbox
//---------------------------------------------------------

// determine if specific types of credit words must be added: do not add copyright and page number,
// as these typically conflict with MuseScore's style and/or layout

static bool mustAddWordToVbox(const QString& creditType)
      {
      return creditType != "rights" && creditType != "page number";
      }

//---------------------------------------------------------
//   addCreditWords
//---------------------------------------------------------

static VBox* addCreditWords(Score* const score, const CreditWordsList& crWords,
                            const int pageNr, const QSize pageSize,
                            const bool top)
      {
      VBox* vbox = nullptr;

      std::vector<const CreditWords*> headerWords;
      std::vector<const CreditWords*> footerWords;
      for (const auto w : crWords) {
            if (w->page == pageNr) {
                  if (w->defaultY > (pageSize.height() / 2))
                        headerWords.push_back(w);
                  else
                        footerWords.push_back(w);
                  }
            }

      std::vector<const CreditWords*> words;
      if (pageNr == 0) {
            // if there are more credit words in the footer than in header,
            // swap heaer and footer, assuming this will result in a vertical
            // frame with the title on top of the page.
            // Sibelius (direct export) typically exports no header
            // and puts the title etc. in the footer
            const bool doSwap = footerWords.size() > headerWords.size();
            if (top) {
                  words = doSwap ? footerWords : headerWords;
                  }
            else {
                  words = doSwap ? headerWords : footerWords;
                  }
            }
      else {
            words = top ? headerWords : footerWords;
            }

      int miny = 0;
      int maxy = 0;
      findYMinYMaxInWords(words, miny, maxy);

      for (const auto w : words) {
            if (mustAddWordToVbox(w->type)) {
                  const auto align = alignForCreditWords(w, pageSize.width());
                  const auto tid = (pageNr == 0 && top) ? tidForCreditWords(w, words, pageSize.width()) : Tid::DEFAULT;
                  double yoffs = (maxy - w->defaultY) * score->spatium() / 10;
                  if (!vbox)
                        vbox = createAndAddVBoxForCreditWords(score, miny, maxy);
                  addText2(vbox, score, w->words, tid, align, yoffs);
                  }
            }

      return vbox;
      }

//---------------------------------------------------------
//   createMeasuresAndVboxes
//---------------------------------------------------------

static void createDefaultHeader(Score* const score)
      {
#if 0
      QString strTitle;
      QString strSubTitle;
      QString strComposer;
      QString strPoet;
      QString strTranslator;

      if (!(score->metaTag("movementTitle").isEmpty() && score->metaTag("workTitle").isEmpty())) {
            strTitle = score->metaTag("movementTitle");
            if (strTitle.isEmpty())
                  strTitle = score->metaTag("workTitle");
            }
      if (!(score->metaTag("movementNumber").isEmpty() && score->metaTag("workNumber").isEmpty())) {
            strSubTitle = score->metaTag("movementNumber");
            if (strSubTitle.isEmpty())
                  strSubTitle = score->metaTag("workNumber");
            }
      QString metaComposer = score->metaTag("composer");
      QString metaPoet = score->metaTag("poet");
      QString metaTranslator = score->metaTag("translator");
      if (!metaComposer.isEmpty()) strComposer = metaComposer;
      if (metaPoet.isEmpty()) metaPoet = score->metaTag("lyricist");
      if (!metaPoet.isEmpty()) strPoet = metaPoet;
      if (!metaTranslator.isEmpty()) strTranslator = metaTranslator;

      const auto vbox = createAndAddVBoxForCreditWords(score);
      addText(vbox, score, strTitle.toHtmlEscaped(),      Tid::TITLE);
      addText(vbox, score, strSubTitle.toHtmlEscaped(),   Tid::SUBTITLE);
      addText(vbox, score, strComposer.toHtmlEscaped(),   Tid::COMPOSER);
      addText(vbox, score, strPoet.toHtmlEscaped(),       Tid::POET);
      addText(vbox, score, strTranslator.toHtmlEscaped(), Tid::TRANSLATOR);
#endif
      }

//---------------------------------------------------------
//   createMeasuresAndVboxes
//---------------------------------------------------------

/**
 Create required measures with correct number, start tick and length for Score \a score.
 */

static void createMeasuresAndVboxes(Score* const score,
                                    const QVector<Fraction>& ml, const QVector<Fraction>& ms,
                                    const std::set<int>& systemStartMeasureNrs,
                                    const std::set<int>& pageStartMeasureNrs,
                                    const CreditWordsList& crWords,
                                    const QSize pageSize)
      {
      if (crWords.empty())
            createDefaultHeader(score);

      int pageNr = 0;
      for (int i = 0; i < ml.size(); ++i) {

            VBox* vbox = nullptr;

            // add a header vbox if the this measure is the first in the score or the first on a new page
            if (pageStartMeasureNrs.count(i) || i == 0) {
                  vbox = addCreditWords(score, crWords, pageNr, pageSize, true);
                  ++pageNr;
                  }

            // create and add the measure
            Measure* measure  = new Measure(score);
            measure->setTick(ms.at(i));
            measure->setTicks(ml.at(i));
            measure->setNo(i);
            score->measures()->add(measure);

            // add break to previous measure or vbox
            MeasureBase* mb = vbox;
            if (!mb) mb = measure;
            if (pageStartMeasureNrs.count(i))
                  addBreakToPreviousMeasureBase(score, mb, LayoutBreak::Type::PAGE);
            else if (systemStartMeasureNrs.count(i))
                  addBreakToPreviousMeasureBase(score, mb, LayoutBreak::Type::LINE);

            // add a footer vbox if the next measure is on a new page or end of score has been reached
            if (pageStartMeasureNrs.count(i+1) || i == (ml.size() - 1))
                  addCreditWords(score, crWords, pageNr, pageSize, false);
            }
      }

//---------------------------------------------------------
//   determineMeasureStart
//---------------------------------------------------------

/**
 Determine the start ticks of each measure
 i.e. the sum of all previous measures length
 or start tick measure equals start tick previous measure plus length previous measure
 */

static void determineMeasureStart(const QVector<Fraction>& ml, QVector<Fraction>& ms)
      {
      ms.resize(ml.size());
      if (!(ms.size() > 0))
            return;  // no parts read

      // first measure starts at t = 0
      ms[0] = Fraction(0, 1);
      // all others start at start time previous measure plus length previous measure
      for (int i = 1; i < ml.size(); i++)
            ms[i] = ms.at(i - 1) + ml.at(i - 1);
      //for (int i = 0; i < ms.size(); i++)
      //      qDebug("measurestart ms[%d] %s", i + 1, qPrintable(ms.at(i).print()));
      }

//---------------------------------------------------------
//   dumpPageSize
//---------------------------------------------------------

static void dumpPageSize(const QSize& pageSize)
      {
#if 0
      qDebug("page size width=%d height=%d", pageSize.width(), pageSize.height());
#else
      Q_UNUSED(pageSize);
#endif
      }

//---------------------------------------------------------
//   dumpCredits
//---------------------------------------------------------

static void dumpCredits(const CreditWordsList& credits)
      {
#if 0
      for (const auto w : credits) {
            qDebug("credit-words pg=%d tp='%s' defx=%g defy=%g just=%s hal=%s val=%s words='%s'",
                   w->page,
                   qPrintable(w->type),
                   w->defaultX,
                   w->defaultY,
                   qPrintable(w->justify),
                   qPrintable(w->hAlign),
                   qPrintable(w->vAlign),
                   qPrintable(w->words));
            }
#else
      Q_UNUSED(credits);
#endif
      }

//---------------------------------------------------------
//   fixupSigmap
//---------------------------------------------------------

/**
 To enable error handling in pass2, ensure sigmap contains a valid entry at tick = 0.
 Required by TimeSigMap::tickValues(), called (indirectly) by Segment::add().
 */

static void fixupSigmap(MxmlLogger* logger, Score* score, const QVector<Fraction>& measureLength)
      {
      auto it = score->sigmap()->find(0);

      if (it == score->sigmap()->end()) {
            // no valid timesig at tick = 0
            logger->logDebugInfo("no valid time signature at tick = 0");
            // use length of first measure instead time signature.
            // if there is no first measure, we probably don't care,
            // but set a default anyway.
            Fraction tsig = measureLength.isEmpty() ? Fraction(4, 4) : measureLength.at(0);
            score->sigmap()->add(0, tsig);
            }
      }

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

/**
 Parse MusicXML in \a device and extract pass 1 data.
 */

Score::FileError MusicXMLParserPass1::parse(QIODevice* device, const MusicXML::MxmlData& mxmlData)
      {
      _logger->logDebugTrace("MusicXMLParserPass1::parse device");
      _parts.clear();

      auto res = parse(mxmlData);
      if (res != Score::FileError::FILE_NO_ERROR)
            return res;

      _e.setDevice(device);
#if 0
      res = parse();
#endif
      if (res != Score::FileError::FILE_NO_ERROR)
            return res;

      // Determine the start tick of each measure in the part
      determineMeasureLength(_measureLength);
      determineMeasureStart(_measureLength, _measureStart);
      // Fixup timesig at tick = 0 if necessary
      fixupSigmap(_logger, _score, _measureLength);
      // Debug: dump gae size and credits read
      dumpPageSize(_pageSize);
      dumpCredits(_credits);
      // Create the measures
      createMeasuresAndVboxes(_score, _measureLength, _measureStart, _systemStartMeasureNrs, _pageStartMeasureNrs, _credits, _pageSize);

      return res;
      }

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

/**
 Start the parsing process, after verifying the top-level node is score-partwise
 */

Score::FileError MusicXMLParserPass1::parse(const MusicXML::MxmlData& mxmlData)
      {
      qDebug("data:\n%s", mxmlData.scorePartwise.toString().data());
      scorePartwise(mxmlData.scorePartwise);
      return Score::FileError::FILE_NO_ERROR;
      }

//---------------------------------------------------------
//   allStaffGroupsIdentical
//---------------------------------------------------------

/**
 Return true if all staves in Part \a p have the same staff group
 */

static bool allStaffGroupsIdentical(Part const* const p)
      {
      for (int i = 1; i < p->nstaves(); ++i) {
            if (p->staff(0)->constStaffType(Fraction(0,1))->group() != p->staff(i)->constStaffType(Fraction(0,1))->group())
                  return false;
            }
      return true;
      }

//---------------------------------------------------------
//   setNonEmptyMetaTag
//---------------------------------------------------------

static void setNonEmptyMetaTag(Score* const score, const char* const tagName, const std::string tagValue)
      {
      if (!tagValue.empty()) {
            score->setMetaTag(tagName, tagValue.data());
            }
      }

//---------------------------------------------------------
//   movementWork
//---------------------------------------------------------

static void movementWork(const MusicXML::ScorePartwise& scorePartwise, Score* const score)
      {
      setNonEmptyMetaTag(score, "movementNumber", scorePartwise.movementNumber);
      setNonEmptyMetaTag(score, "movementTitle", scorePartwise.movementTitle);
      setNonEmptyMetaTag(score, "workNumber", scorePartwise.work.workNumber);
      setNonEmptyMetaTag(score, "workTitle", scorePartwise.work.workTitle);
      }

//---------------------------------------------------------
//   scorePartwise
//---------------------------------------------------------

void MusicXMLParserPass1::scorePartwise(const MusicXML::ScorePartwise& scorePartwise)
      {
      MusicXmlPartGroupList partGroupList;

      movementWork(scorePartwise, _score);
      identification(scorePartwise.identification);
      defaults(scorePartwise.defaults);
      credit(scorePartwise.credits, _credits);
      partList(scorePartwise.partList);
      for (const auto& part : scorePartwise.parts)
            MusicXMLParserPass1::part(part);

      // add brackets where required

      /*
       qDebug("partGroupList");
       for (size_t i = 0; i < partGroupList.size(); i++) {
       MusicXmlPartGroup* pg = partGroupList[i];
       qDebug("part-group span %d start %d type %hhd barlinespan %d",
       pg->span, pg->start, pg->type, pg->barlineSpan);
       }
       */

      // set of (typically multi-staff) parts containing one or more explicit brackets
      // spanning only that part: these won't get an implicit brace later
      // e.g. a two-staff piano part with an explicit brace
      QSet<Part const* const> partSet;

      // handle the explicit brackets
      const QList<Part*>& il = _score->parts();
      for (size_t i = 0; i < partGroupList.size(); i++) {
            MusicXmlPartGroup* pg = partGroupList[i];
            // add part to set
            if (pg->span == 1)
                  partSet << il.at(pg->start);
            // determine span in staves
            int stavesSpan = 0;
            for (int j = 0; j < pg->span; j++)
                  stavesSpan += il.at(pg->start + j)->nstaves();
            // add bracket and set the span
            // TODO: use group-symbol default-x to determine horizontal order of brackets
            Staff* staff = il.at(pg->start)->staff(0);
            if (pg->type != BracketType::NO_BRACKET) {
                  staff->setBracketType(pg->column, pg->type);
                  staff->setBracketSpan(pg->column, stavesSpan);
                  }
            if (pg->barlineSpan)
                  staff->setBarLineSpan(pg->span);
            }

      // handle the implicit brackets:
      // multi-staff parts w/o explicit brackets get a brace
      foreach(Part const* const p, il) {
            if (p->nstaves() > 1 && !partSet.contains(p)) {
                  const int column = p->staff(0)->bracketLevels() + 1;
                  p->staff(0)->setBracketType(column, BracketType::BRACE);
                  p->staff(0)->setBracketSpan(column, p->nstaves());
                  if (allStaffGroupsIdentical(p)) {
                        // span only if the same types
                        p->staff(0)->setBarLineSpan(p->nstaves());
                        }
                  }
            }
      }

//---------------------------------------------------------
//   identification
//---------------------------------------------------------

/**
 Parse the /score-partwise/identification node:
 read the metadata.
 */

void MusicXMLParserPass1::identification(const MusicXML::Identification& identification)
      {
#if 0
                  // _score->setMetaTag("encoding", _e.readElementText()); works with DOM but not with pull parser
                  // temporarily fake the encoding tag (compliant with DOM parser) to help the autotester
                  if (MScore::debugMode)
                        _score->setMetaTag("encoding", "MuseScore 0.7.02007-09-10");
                  }
#endif
      for (const auto& creator : identification.creators) {
            if (!creator.type.empty()) {
                  _score->setMetaTag(creator.type.data(), creator.text.data());
                  }
            }
      if (!identification.rightses.empty() && !identification.rightses.at(0).text.empty()) {
            // use only the first rights
            _score->setMetaTag("copyright", identification.rightses.at(0).text.data());
            }
      for (const auto& supports : identification.encoding.supportses) {
            if (supports.element == "beam" && supports.type == "yes") {
                  _hasBeamingInfo = true;
                  }
            }
      if (!identification.source.empty()) {
            _score->setMetaTag("source", identification.source.data());
            }
      }

//---------------------------------------------------------
//   text2syms
//---------------------------------------------------------

/**
 Convert SMuFL code points to MuseScore <sym>...</sym>
 */

static QString text2syms(const QString& t)
      {
      //QTime time;
      //time.start();

      // first create a map from symbol (Unicode) text to symId
      // note that this takes about 1 msec on a Core i5,
      // caching does not gain much

      ScoreFont* sf = ScoreFont::fallbackFont();
      QMap<QString, SymId> map;
      int maxStringSize = 0;        // maximum string size found

      for (int i = int(SymId::noSym); i < int(SymId::lastSym); ++i) {
            SymId id((SymId(i)));
            QString string(sf->toString(id));
            // insert all syms except space to prevent matching all regular spaces
            if (id != SymId::space)
                  map.insert(string, id);
            if (string.size() > maxStringSize)
                  maxStringSize = string.size();
            }
      //qDebug("text2syms map count %d maxsz %d filling time elapsed: %d ms",
      //       map.size(), maxStringSize, time.elapsed());

      // then look for matches
      QString in = t;
      QString res;

      while (in != "") {
            // try to find the largest match possible
            int maxMatch = qMin(in.size(), maxStringSize);
            QString sym;
            while (maxMatch > 0) {
                  QString toBeMatched = in.left(maxMatch);
                  if (map.contains(toBeMatched)) {
                        sym = Sym::id2name(map.value(toBeMatched));
                        break;
                        }
                  maxMatch--;
                  }
            if (maxMatch > 0) {
                  // found a match, add sym to res and remove match from string in
                  res += "<sym>";
                  res += sym;
                  res += "</sym>";
                  in.remove(0, maxMatch);
                  }
            else {
                  // not found, move one char from res to in
                  res += in.leftRef(1);
                  in.remove(0, 1);
                  }
            }

      //qDebug("text2syms total time elapsed: %d ms, res '%s'", time.elapsed(), qPrintable(res));
      return res;
      }

//---------------------------------------------------------
//   decodeEntities
//---------------------------------------------------------

/**
 Decode &#...; in string \a src into UNICODE (utf8) character.
 */

static QString decodeEntities( const QString& src )
      {
      QString ret(src);
      QRegExp re("&#([0-9]+);");
      re.setMinimal(true);

      int pos = 0;
      while ( (pos = re.indexIn(src, pos)) != -1 ) {
            ret = ret.replace(re.cap(0), QChar(re.cap(1).toInt(0,10)));
            pos += re.matchedLength();
            }
      return ret;
      }

//---------------------------------------------------------
//   nextPartOfFormattedString
//---------------------------------------------------------

// TODO: probably should be shared between pass 1 and 2

/**
 Read the next part of a MusicXML formatted string and convert to MuseScore internal encoding.
 */

static QString nextPartOfFormattedString(QXmlStreamReader& e)
      {
      //QString lang       = e.attribute(QString("xml:lang"), "it");
      QString fontWeight = e.attributes().value("font-weight").toString();
      QString fontSize   = e.attributes().value("font-size").toString();
      QString fontStyle  = e.attributes().value("font-style").toString();
      QString underline  = e.attributes().value("underline").toString();
      QString fontFamily = e.attributes().value("font-family").toString();
      // TODO: color, enclosure, yoffset in only part of the text, ...

      QString txt        = e.readElementText();
      // replace HTML entities
      txt = decodeEntities(txt);
      QString syms       = text2syms(txt);

      QString importedtext;

      if (!fontSize.isEmpty()) {
            bool ok = true;
            float size = fontSize.toFloat(&ok);
            if (ok)
                  importedtext += QString("<font size=\"%1\"/>").arg(size);
            }

      bool needUseDefaultFont = preferences.getBool(PREF_MIGRATION_APPLY_EDWIN_FOR_XML_FILES);

      if (!fontFamily.isEmpty() && txt == syms && !needUseDefaultFont) {
            // add font family only if no <sym> replacement made
            importedtext += QString("<font face=\"%1\"/>").arg(fontFamily);
            }
      if (fontWeight == "bold")
            importedtext += "<b>";
      if (fontStyle == "italic")
            importedtext += "<i>";
      if (!underline.isEmpty()) {
            bool ok = true;
            int lines = underline.toInt(&ok);
            if (ok && (lines > 0))  // 1,2, or 3 underlines are imported as single underline
                  importedtext += "<u>";
            else
                  underline = "";
            }
      if (txt == syms) {
            txt.replace(QString("\r"), QString("")); // convert Windows line break \r\n -> \n
            importedtext += txt.toHtmlEscaped();
            }
      else {
            // <sym> replacement made, should be no need for line break or other conversions
            importedtext += syms;
            }
      if (underline != "")
            importedtext += "</u>";
      if (fontStyle == "italic")
            importedtext += "</i>";
      if (fontWeight == "bold")
            importedtext += "</b>";
      //qDebug("importedtext '%s'", qPrintable(importedtext));
      return importedtext;
      }

//---------------------------------------------------------
//   credit
//---------------------------------------------------------

/**
 Parse the /score-partwise/credit node:
 read the credits for later handling by doCredits().
 */

void MusicXMLParserPass1::credit(const std::vector<MusicXML::Credit>& credits, CreditWordsList& creditWordsList)
      {
      for (const auto& credit : credits) {
            QString crwords;
            for (const auto& creditWords : credit.creditWordses) {
                  crwords += creditWords.text.data();
            }
            if (crwords != "") {
                  // multiple credit-words elements may be present,
                  // which are appended
                  // use the position info from the first one
                  // font information is ignored, credits will be styled
                  double defaultx = 0;
                  double defaulty = 0;
                  double fontSize = 0;
                  QString justify;
                  QString halign;
                  QString valign;
                  if (credit.creditWordses.size() > 0) {
                        defaultx = credit.creditWordses.at(0).defaultX;
                        defaulty = credit.creditWordses.at(0).defaultY;
                        fontSize = credit.creditWordses.at(0).fontSize;
                        justify = credit.creditWordses.at(0).justify.data();
                        halign = credit.creditWordses.at(0).halign.data();
                        valign = credit.creditWordses.at(0).valign.data();
                        }
                  // multiple credit-type elements may be present, supported by
                  // e.g. Finale v26.3 for Mac.
                  // as the meaning of multiple credit-types is undocumented,
                  // use credit-type only if exactly one was found
                  QString crtype = (credit.creditTypes.size() == 1) ? credit.creditTypes.at(0).data() : "";
                  CreditWords* cw = new CreditWords(credit.page, crtype, defaultx, defaulty, fontSize, justify, halign, valign, crwords);
                  creditWordsList.append(cw);
                  }
            }
      }

//---------------------------------------------------------
//   isTitleFrameStyle
//---------------------------------------------------------

/**
 Determine if tid is a style type used in a title frame
 */

static bool isTitleFrameStyle(const Tid tid)
      {
      return tid == Tid::TITLE
             || tid == Tid::SUBTITLE
             || tid == Tid::COMPOSER
             || tid == Tid::POET;
      }

//---------------------------------------------------------
//   updateStyles
//---------------------------------------------------------

/**
 Update the style definitions to match the MusicXML word-font and lyric-font.
 */

static void updateStyles(Score* score,
                         const QString& wordFamily, const QString& wordSize,
                         const QString& lyricFamily, const QString& lyricSize)
      {
      const auto dblWordSize = wordSize.toDouble();   // note conversion error results in value 0.0
      const auto dblLyricSize = lyricSize.toDouble(); // but avoid comparing (double) floating point number with exact value later
      const auto epsilon = 0.001;                     // use epsilon instead

      bool needUseDefaultFont = preferences.getBool(PREF_MIGRATION_APPLY_EDWIN_FOR_XML_FILES);

      // loop over all text styles (except the empty, always hidden, first one)
      // set all text styles to the MusicXML defaults
      for (const auto tid : allTextStyles()) {

            // The MusicXML specification does not specify to which kinds of text
            // the word-font setting applies. Setting all sizes to the size specified
            // gives bad results, so a selection is made:
            // exclude lyrics odd and even lines (handled separately),
            // Roman numeral analysis (special case, leave untouched)
            // and text types used in the title frame
            // Some further tweaking may still be required.

            if (tid == Tid::LYRICS_ODD || tid == Tid::LYRICS_EVEN
                || tid == Tid::HARMONY_ROMAN
                || isTitleFrameStyle(tid))
                  continue;
            const TextStyle* ts = textStyle(tid);
            for (const StyledProperty& a :* ts) {
                  if (a.pid == Pid::FONT_FACE && wordFamily != "" && !needUseDefaultFont)
                        score->style().set(a.sid, wordFamily);
                  else if (a.pid == Pid::FONT_SIZE && dblWordSize > epsilon)
                        score->style().set(a.sid, dblWordSize);
                  }
            }

      // handle lyrics odd and even lines separately
      if (lyricFamily != "" && !needUseDefaultFont) {
            score->style().set(Sid::lyricsOddFontFace, lyricFamily);
            score->style().set(Sid::lyricsEvenFontFace, lyricFamily);
            }
      if (dblLyricSize > epsilon) {
            score->style().set(Sid::lyricsOddFontSize, QVariant(dblLyricSize));
            score->style().set(Sid::lyricsEvenFontSize, QVariant(dblLyricSize));
            }
      }

//---------------------------------------------------------
//   setPageFormat
//---------------------------------------------------------

static void setPageFormat(Score* score, const PageFormat& pf)
      {
      score->style().set(Sid::pageWidth, pf.size.width());
      score->style().set(Sid::pageHeight, pf.size.height());
      score->style().set(Sid::pagePrintableWidth, pf.printableWidth);
      score->style().set(Sid::pageEvenLeftMargin, pf.evenLeftMargin);
      score->style().set(Sid::pageOddLeftMargin, pf.oddLeftMargin);
      score->style().set(Sid::pageEvenTopMargin, pf.evenTopMargin);
      score->style().set(Sid::pageEvenBottomMargin, pf.evenBottomMargin);
      score->style().set(Sid::pageOddTopMargin, pf.oddTopMargin);
      score->style().set(Sid::pageOddBottomMargin, pf.oddBottomMargin);
      score->style().set(Sid::pageTwosided, pf.twosided);
      }

//---------------------------------------------------------
//   defaults
//---------------------------------------------------------

/**
 Parse the /score-partwise/defaults node:
 read the general score layout settings.
 */

void MusicXMLParserPass1::defaults(const MusicXML::Defaults& defaults)
      {
      double millimeter = _score->spatium()/10.0;
      double tenths = 1.0;
      QString lyricFontFamily;
      QString lyricFontSize;
      QString wordFontFamily;
      QString wordFontSize;

      if (defaults.scalingRead) {
            millimeter = defaults.scaling.millimeters;
            tenths = defaults.scaling.tenths;
            double _spatium = DPMM * (millimeter * 10.0 / tenths);
            if (preferences.getBool(PREF_IMPORT_MUSICXML_IMPORTLAYOUT))
                  _score->setSpatium(_spatium);

            PageFormat pf;
            pageLayout(defaults.pageLayout, pf, millimeter / (tenths * INCH));
            if (preferences.getBool(PREF_IMPORT_MUSICXML_IMPORTLAYOUT))
                  setPageFormat(_score, pf);

            }

#if 0
            else if (_e.name() == "system-layout") {
                  while (_e.readNextStartElement()) {
                        else if (_e.name() == "system-distance") {
                              Spatium val(_e.readElementText().toDouble() / 10.0);
                              if (preferences.getBool(PREF_IMPORT_MUSICXML_IMPORTLAYOUT)) {
                                    _score->style().set(Sid::minSystemDistance, val);
                                    //qDebug("system distance %f", val.val());
                                    }
                              }
            else if (_e.name() == "staff-layout") {
                  while (_e.readNextStartElement()) {
                        if (_e.name() == "staff-distance") {
                              Spatium val(_e.readElementText().toDouble() / 10.0);
                              if (preferences.getBool(PREF_IMPORT_MUSICXML_IMPORTLAYOUT))
                                    _score->style().set(Sid::staffDistance, val);
                              }
                  }
            else if (_e.name() == "word-font") {
                  wordFontFamily = _e.attributes().value("font-family").toString();
                  wordFontSize = _e.attributes().value("font-size").toString();
                  _e.skipCurrentElement();
                  }
            else if (_e.name() == "lyric-font") {
                  lyricFontFamily = _e.attributes().value("font-family").toString();
                  lyricFontSize = _e.attributes().value("font-size").toString();
                  _e.skipCurrentElement();
                  }
            }

      /*
      qDebug("word font family '%s' size '%s' lyric font family '%s' size '%s'",
             qPrintable(wordFontFamily), qPrintable(wordFontSize),
             qPrintable(lyricFontFamily), qPrintable(lyricFontSize));
      */
      updateStyles(_score, wordFontFamily, wordFontSize, lyricFontFamily, lyricFontSize);

      _score->setDefaultsRead(true); // TODO only if actually succeeded ?
#endif
      }

//---------------------------------------------------------
//   pageLayout
//---------------------------------------------------------

/**
 Parse the /score-partwise/defaults/page-layout node: read the page layout.
 Note that MuseScore does not support a separate value for left and right margins
 for odd and even pages. Only odd and even left margins are used, together  with
 the printable width, which is calculated from the left and right margins in the
 MusicXML file.
 */

void MusicXMLParserPass1::pageLayout(const MusicXML::PageLayout& pageLayout, PageFormat& pf, const qreal conversion)
      {
      qreal _oddRightMargin  = 0.0;
      qreal _evenRightMargin = 0.0;
      QSizeF size;

      if (pageLayout.pageSizeRead) {
            size.rheight() = pageLayout.pageHeight * conversion;
            size.rwidth() = pageLayout.pageWidth * conversion;
            // set pageHeight and pageWidth for use by doCredits()
            _pageSize.setHeight(static_cast<int>(pageLayout.pageHeight + 0.5));
            _pageSize.setWidth(static_cast<int>(pageLayout.pageWidth + 0.5));
            }
      if (pageLayout.oddMarginsRead) {
            pf.oddLeftMargin = pageLayout.oddLeftMargin * conversion;
            _oddRightMargin = pageLayout.oddRightMargin * conversion;
            pf.oddTopMargin = pageLayout.oddTopMargin * conversion;
            pf.oddBottomMargin = pageLayout.oddBottomMargin * conversion;
            }
      if (pageLayout.evenMarginsRead) {
            pf.evenLeftMargin = pageLayout.evenLeftMargin * conversion;
            _evenRightMargin = pageLayout.evenRightMargin * conversion;
            pf.evenTopMargin = pageLayout.evenTopMargin * conversion;
            pf.evenBottomMargin = pageLayout.evenBottomMargin * conversion;
            qDebug("odd lm %g rm %g tm %g bm %g", pf.evenLeftMargin, _evenRightMargin, pf.evenTopMargin, pf.evenBottomMargin);
            }

      pf.size = size;
      qreal w1 = size.width() - pf.oddLeftMargin - _oddRightMargin;
      qreal w2 = size.width() - pf.evenLeftMargin - _evenRightMargin;
      pf.printableWidth = qMax(w1, w2);   // silently adjust right margins
      }

//---------------------------------------------------------
//   partList
//---------------------------------------------------------

/**
 Create the parts and for each part set id and name.
 Also handle the part-groups.
 */

void MusicXMLParserPass1::partList(const MusicXML::PartList& partList)
      {
      for (const auto& scorePart : partList.scoreParts)
            MusicXMLParserPass1::scorePart(scorePart);
      }

//---------------------------------------------------------
//   createPart
//---------------------------------------------------------

/**
 Create the part, set its \a id and insert it in PartMap \a pm.
 Part name (if any) will be set later.
 */

static void createPart(Score* score, const QString& id, PartMap& pm)
      {
      Part* part = new Part(score);
      pm.insert(id, part);
      part->setId(id);
      score->appendPart(part);
      Staff* staff = new Staff(score);
      staff->setPart(part);
      part->staves()->push_back(staff);
      score->staves().push_back(staff);
      // TODO TBD tuplets.resize(VOICES); // part now contains one staff, thus VOICES voices
      }

//---------------------------------------------------------
//   partGroupStart
//---------------------------------------------------------

typedef std::map<int,MusicXmlPartGroup*> MusicXmlPartGroupMap;

/**
 Store part-group start with number \a n, first part \a p and symbol / \a s in the partGroups
 map \a pgs for later reference, as at this time insufficient information is available to be able
 to generate the brackets.
 */

static void partGroupStart(MusicXmlPartGroupMap& pgs, int n, int p, QString s, bool barlineSpan)
      {
      //qDebug("partGroupStart number=%d part=%d symbol=%s", n, p, qPrintable(s));

      if (pgs.count(n) > 0) {
            qDebug("part-group number=%d already active", n);
            return;
            }

      BracketType bracketType = BracketType::NO_BRACKET;
      if (s == "")
            ;        // ignore (handle as NO_BRACKET)
      else if (s == "none")
            ;        // already set to NO_BRACKET
      else if (s == "brace")
            bracketType = BracketType::BRACE;
      else if (s == "bracket")
            bracketType = BracketType::NORMAL;
      else if (s == "line")
            bracketType = BracketType::LINE;
      else if (s == "square")
            bracketType = BracketType::SQUARE;
      else {
            qDebug("part-group symbol=%s not supported", qPrintable(s));
            return;
            }

      MusicXmlPartGroup* pg = new MusicXmlPartGroup;
      pg->span = 0;
      pg->start = p;
      pg->barlineSpan = barlineSpan,
      pg->type = bracketType;
      pg->column = n;
      pgs[n] = pg;
      }

//---------------------------------------------------------
//   partGroupStop
//---------------------------------------------------------

/**
 Handle part-group stop with number \a n and part \a p.

 For part group n, the start part, span (in parts) and type are now known.
 To generate brackets, the span in staves must also be known.
 */

static void partGroupStop(MusicXmlPartGroupMap& pgs, int n, int p,
                          MusicXmlPartGroupList& pgl)
      {
      if (pgs.count(n) == 0) {
            qDebug("part-group number=%d not active", n);
            return;
            }

      pgs[n]->span = p - pgs[n]->start;
      //qDebug("partgroupstop number=%d start=%d span=%d type=%hhd",
      //       n, pgs[n]->start, pgs[n]->span, pgs[n]->type);
      pgl.push_back(pgs[n]);
      pgs.erase(n);
      }

//---------------------------------------------------------
//   partGroup
//---------------------------------------------------------

/**
 Parse the /score-partwise/part-list/part-group node.
 */

void MusicXMLParserPass1::partGroup(const int scoreParts,
                                    MusicXmlPartGroupList& partGroupList,
                                    MusicXmlPartGroupMap& partGroups)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "part-group");
      _logger->logDebugTrace("MusicXMLParserPass1::partGroup", &_e);
      bool barlineSpan = true;
      int number = _e.attributes().value("number").toInt();
      if (number > 0) number--;
      QString symbol = "";
      QString type = _e.attributes().value("type").toString();

      while (_e.readNextStartElement()) {
            if (_e.name() == "group-name")
                  _e.skipCurrentElement();  // skip but don't log
            else if (_e.name() == "group-abbreviation")
                  symbol = _e.readElementText();
            else if (_e.name() == "group-symbol")
                  symbol = _e.readElementText();
            else if (_e.name() == "group-barline") {
                  if (_e.readElementText() == "no")
                        barlineSpan = false;
                  }
            else
                  skipLogCurrElem();
            }

      if (type == "start")
            partGroupStart(partGroups, number, scoreParts, symbol, barlineSpan);
      else if (type == "stop")
            partGroupStop(partGroups, number, scoreParts, partGroupList);
      else
            _logger->logError(QString("part-group type '%1' not supported").arg(type), &_e);
      }

//---------------------------------------------------------
//   findInstrument
//---------------------------------------------------------

/**
 Find the first InstrumentTemplate with musicXMLid instrSound
 and a non-empty set of channels.
 */

#if 0 // not used
static const InstrumentTemplate* findInstrument(const QString& instrSound)
      {
      const InstrumentTemplate* instr = nullptr;

      for (const InstrumentGroup* group : instrumentGroups) {
            for (const InstrumentTemplate* templ : group->instrumentTemplates) {
                  if (templ->musicXMLid == instrSound && !templ->channel.isEmpty()) {
                        return templ;
                        }
                  }
            }
      return instr;
      }
#endif

//---------------------------------------------------------
//   scorePart
//---------------------------------------------------------

/**
 Create the part and sets id and name.
 Note that a part is created even if no part-name is present
 which is invalid MusicXML but is (sometimes ?) generated by NWC2MusicXML.
 */

void MusicXMLParserPass1::scorePart(const MusicXML::ScorePart& scorePart)
      {
      const QString id = scorePart.id.data();

      if (_parts.contains(id)) {
            // TODO _logger->logError(QString("duplicate part id '%1'").arg(id), &_e);
            return;
            }
      else {
            _parts.insert(id, MusicXmlPart(id));
            _instruments.insert(id, MusicXMLInstruments());
            createPart(_score, id, _partMap);
            }

      // Element part-name contains the displayed (full) part name
      // It is displayed by default, but can be suppressed (print-object=”no”)
      // As of MusicXML 3.0, formatting is deprecated, with part-name in plain text
      // and the formatted version in the part-name-display element
      // TODO _parts[id].setPrintName(!(_e.attributes().value("print-object") == "no"));
      QString name = scorePart.partName.data();
      _parts[id].setName(name);
      // Element part-name contains the displayed (abbreviated) part name
      // It is displayed by default, but can be suppressed (print-object=”no”)
      // As of MusicXML 3.0, formatting is deprecated, with part-name in plain text
      // and the formatted version in the part-abbreviation-display element
      _parts[id].setPrintAbbr(scorePart.partAbbreviationPrintObject);
      _parts[id].setAbbr(scorePart.partAbbreviation.data());
      for (const auto& scoreInstr : scorePart.scoreInstruments) {
            scoreInstrument(scoreInstr, id);
            }
      for (const auto& midiInstr : scorePart.midiInstruments) {
            midiInstrument(midiInstr, id);
            }

#if 0
      // TODO else if (_e.name() == "midi-device") {
      if (!_e.attributes().hasAttribute("port")) {
            _e.readElementText();       // empty string
            continue;
            }
      QString instrId = _e.attributes().value("id").toString();
      QString port = _e.attributes().value("port").toString();
      // If instrId is missing, the device assignment affects all
      // score-instrument elements in the score-part
      if (instrId.isEmpty()) {
            for (auto it = _instruments[id].cbegin(); it != _instruments[id].cend(); ++it)
                  _instruments[id][it.key()].midiPort = port.toInt() - 1;
            }
      else if (_instruments[id].contains(instrId))
            _instruments[id][instrId].midiPort = port.toInt() - 1;

      _e.readElementText();       // empty string
      }
#endif
      }

//---------------------------------------------------------
//   scoreInstrument
//---------------------------------------------------------

/**
 Parse the /score-partwise/part-list/score-part/score-instrument node.
 */

void MusicXMLParserPass1::scoreInstrument(const MusicXML::ScoreInstrument& scoreInstrument, const QString& partId)
      {
      QString instrId { scoreInstrument.id.data() };
      QString instrName { scoreInstrument.instrumentName.data() };
      _instruments[partId].insert(instrId, MusicXMLInstrument(instrName));
      if (_instruments[partId].contains(instrId)) {
            _instruments[partId][instrId].name = instrName;
            _instruments[partId][instrId].sound = scoreInstrument.instrumentSound.data();
            _instruments[partId][instrId].virtLib = scoreInstrument.virtualLibrary.data();
            _instruments[partId][instrId].virtName = scoreInstrument.virtualName.data();
            }
      }

//---------------------------------------------------------
//   midiInstrument
//---------------------------------------------------------

/**
 Parse the /score-partwise/part-list/score-part/midi-instrument node.
 */

void MusicXMLParserPass1::midiInstrument(const MusicXML::MidiInstrument& midiInstrument, const QString& partId)
      {
#if 0
            else if (_e.name() == "midi-program") {
                  // Bug fix for Cubase 6.5.5 which generates <midi-program>0</midi-program>
                  // Check program number range
                  if (program < 1) {
                        _logger->logError(QString("incorrect midi-program: %1").arg(program), &_e);
                        program = 1;
                        }
                  else if (program > 128) {
                        _logger->logError(QString("incorrect midi-program: %1").arg(program), &_e);
                        program = 128;
                        }
#endif
      QString instrId { midiInstrument.id.data() };
      if (_instruments[partId].contains(instrId)) {
            if (midiInstrument.midiChannelRead) {
                  _instruments[partId][instrId].midiChannel = midiInstrument.midiChannel;
                  }
            if (midiInstrument.midiProgramRead) {
                  _instruments[partId][instrId].midiProgram = midiInstrument.midiProgram;
                  }
            if (midiInstrument.midiUnpitchedRead) {
                  _instruments[partId][instrId].unpitched = midiInstrument.midiUnpitched;
                  }
            if (midiInstrument.panRead) {
                  _instruments[partId][instrId].midiPan = static_cast<int>(((midiInstrument.pan + 90) / 180) * 127);
                  }
            if (midiInstrument.volumeRead) {
                  _instruments[partId][instrId].midiVolume = static_cast<int>((midiInstrument.volume / 100) * 127);
                  }
            }
      }

//---------------------------------------------------------
//   setNumberOfStavesForPart
//---------------------------------------------------------

/**
 Set number of staves for part \a partId to the max value
 of the current value \a staves.
 */

static void setNumberOfStavesForPart(Part* const part, const int staves)
      {
      Q_ASSERT(part);
      if (staves > part->nstaves())
            part->setStaves(staves);
      }

//---------------------------------------------------------
//   part
//---------------------------------------------------------

/**
 Read the parts data to determine measure timing and octave shifts.
 Assign voices and staves.
 */

void MusicXMLParserPass1::part(const MusicXML::Part& part)
      {
      const QString id = part.id.data();

      if (!_parts.contains(id)) {
            // TODO _logger->logError(QString("duplicate part id '%1'").arg(id), &_e);
            }

      initPartState(id);

      VoiceOverlapDetector vod;
      Fraction time;  // current time within part
      Fraction mdur;  // measure duration

      int measureNr = 0;
      for (const auto& measure : part.measures) {
            MusicXMLParserPass1::measure(measure, id, time, mdur, vod, measureNr);
            time += mdur;
            ++measureNr;
            }

      // Bug fix for Cubase 6.5.5..9.5.10 which generate <staff>2</staff> in a single staff part
      setNumberOfStavesForPart(_partMap.value(id), _parts[id].maxStaff());
      // allocate MuseScore staff to MusicXML voices
      allocateStaves(_parts[id].voicelist);
      // allocate MuseScore voice to MusicXML voices
      allocateVoices(_parts[id].voicelist);
      // calculate the octave shifts
      _parts[id].calcOctaveShifts();
      // determine the lyric numbers for this part
      _parts[id].lyricNumberHandler().determineLyricNos();

      // debug: print results
#if 1
      for (const auto& str : _parts[id].toString().split('\n')) {
            qDebug("%s", qPrintable(str));
            }
      qDebug("lyric numbers: %s", qPrintable(_parts[id].lyricNumberHandler().toString()));
      qDebug("instrument map:");
      for (auto& instr : _parts[id]._instrList) {
            qDebug("- %s '%s'", qPrintable(instr.first.print()), qPrintable(instr.second));
            }
      qDebug("transpose map:");
      for (auto& it : _parts[id]._intervals) {
            qDebug("- %s %d %d", qPrintable(it.first.print()), it.second.diatonic, it.second.chromatic);
            }
      qDebug("instrument transpositions:");
      if (_parts[id]._instrList.empty()) {
            const Fraction tick { 0, 1 };
            const QString name { "none" };
            const auto interval = _parts[id]._intervals.interval(tick);
            qDebug("- %s '%s' -> %d %d",
                   qPrintable(tick.print()), qPrintable(name), interval.diatonic, interval.chromatic);
            }
      else {
            for (auto& instr : _parts[id]._instrList) {
                  const auto& tick = instr.first;
                  const auto& name = instr.second;
                  const auto interval = _parts[id].interval(tick);
                  qDebug("- %s '%s' -> %d %d",
                         qPrintable(tick.print()), qPrintable(name), interval.diatonic, interval.chromatic);
                  }
            }
#endif

      /*
       qDebug("voiceMapperStats: new staff");
       VoiceList& vl = _parts[id].voicelist;
       for (auto i = vl.constBegin(); i != vl.constEnd(); ++i) {
       qDebug("voiceMapperStats: voice %s staff data %s",
       qPrintable(i.key()), qPrintable(i.value().toString()));
       }
       */
      }

//---------------------------------------------------------
//   measureDurationAsFraction
//---------------------------------------------------------

/**
 Determine a suitable measure duration value given the time signature
 by setting the duration denominator to be greater than or equal
 to the time signature denominator
 */

static Fraction measureDurationAsFraction(const Fraction length, const int tsigtype)
      {
      if (tsigtype <= 0)
            // invalid tsigtype
            return length;

      Fraction res = length;
      while (res.denominator() < tsigtype) {
            res.setNumerator(res.numerator() * 2);
            res.setDenominator(res.denominator() * 2);
            }
      return res;
      }

//---------------------------------------------------------
//   measure
//---------------------------------------------------------

/**
 Read the measures data as required to determine measure timing, octave shifts
 and assign voices and staves.
 */

void MusicXMLParserPass1::measure(const MusicXML::Measure& measure, const QString& partId, const Fraction cTime, Fraction& mdur, VoiceOverlapDetector& vod, const int measureNr)
      {
      qDebug("part %s measure %d", qPrintable(partId), measureNr);
      const QString number = measure.number.data();

      Fraction mTime; // current time stamp within measure
      Fraction mDura; // current total measure duration
      vod.newMeasure();
      MxmlTupletStates tupletStates;

      for (const auto& element : measure.elements) {
            if (element->elementType == MusicXML::ElementType::ATTRIBUTES) {
                  const MusicXML::Attributes& attributes = *static_cast<MusicXML::Attributes*>(element.get());
                  MusicXMLParserPass1::attributes(attributes, partId, cTime + mTime);
                  }
            else if (element->elementType == MusicXML::ElementType::BACKUP) {
                  const MusicXML::Backup& backup = *static_cast<MusicXML::Backup*>(element.get());
                  Fraction dura;
                  MusicXMLParserPass1::backup(backup.duration, dura);
                  if (dura.isValid()) {
                        if (dura <= mTime)
                              mTime -= dura;
                        else {
                              _logger->logError("backup beyond measure start", &_e);
                              mTime.set(0, 1);
                              }
                        }
                  }
            else if (element->elementType == MusicXML::ElementType::FORWARD) {
                  const MusicXML::Forward& forward = *static_cast<MusicXML::Forward*>(element.get());
                  Fraction dura;
                  MusicXMLParserPass1::forward(forward.duration, dura);
                  if (dura.isValid()) {
                        mTime += dura;
                        if (mTime > mDura)
                              mDura = mTime;
                        }
                  }
            else if (element->elementType == MusicXML::ElementType::NOTE) {
                  const auto& note = *static_cast<MusicXML::Note*>(element.get());
                  Fraction missingPrev;
                  Fraction dura;
                  Fraction missingCurr;
                  // note: chord and grace note handling done in note()
                  MusicXMLParserPass1::note(note, partId, cTime + mTime, missingPrev, dura, missingCurr, vod, tupletStates);
                  if (missingPrev.isValid()) {
                        mTime += missingPrev;
                        }
                  if (dura.isValid()) {
                        mTime += dura;
                        }
                  if (missingCurr.isValid()) {
                        mTime += missingCurr;
                        }
                  if (mTime > mDura)
                        mDura = mTime;
                  }
            // TODO direction(partId, cTime + mTime);
            // TODO print(measureNr);

            /*
             qDebug("mTime %s (%s) mDura %s (%s)",
             qPrintable(mTime.print()),
             qPrintable(mTime.reduced().print()),
             qPrintable(mDura.print()),
             qPrintable(mDura.reduced().print()));
             */
            }

      // debug vod
      // vod.dump();
      // copy overlap data from vod to voicelist
      copyOverlapData(vod, _parts[partId].voicelist);

      // measure duration fixups
      mDura.reduce();

      // fix for PDFtoMusic Pro v1.3.0d Build BF4E and PlayScore / ReadScoreLib Version 3.11
      // which sometimes generate empty measures
      // if no valid length found and length according to time signature is known,
      // use length according to time signature
      if (mDura.isZero() && _timeSigDura.isValid() && _timeSigDura > Fraction(0, 1))
            mDura = _timeSigDura;
      // if no valid length found and time signature is unknown, use default
      if (mDura.isZero() && !_timeSigDura.isValid())
            mDura = Fraction(4, 4);

      // if necessary, round up to an integral number of 1/64s,
      // to comply with MuseScores actual measure length constraints
      Fraction length = mDura * Fraction(64,1);
      Fraction correctedLength = mDura;
      length.reduce();
      if (length.denominator() != 1) {
            Fraction roundDown = Fraction(length.numerator() / length.denominator(), 64);
            Fraction roundUp = Fraction(length.numerator() / length.denominator() + 1, 64);
            // mDura is not an integer multiple of 1/64;
            // first check if the duration is larger than an integer multiple of 1/64
            // by an amount smaller than the minimum division resolution
            // in that case, round down (rounding errors have possibly occurred),
            // otherwise, round up
            if ((_divs > 0) && ((mDura - roundDown) < Fraction(1, 4*_divs))) {
                  _logger->logError(QString("rounding down measure duration %1 to %2")
                                    .arg(qPrintable(mDura.print())).arg(qPrintable(roundDown.print())),
                                    &_e);
                  correctedLength = roundDown;
                  }
            else {
                  _logger->logError(QString("rounding up measure duration %1 to %2")
                                    .arg(qPrintable(mDura.print())).arg(qPrintable(roundUp.print())),
                                    &_e);
                  correctedLength = roundUp;
                  }
            mDura = correctedLength;
            }

      // set measure duration to a suitable value given the time signature
      if (_timeSigDura.isValid() && _timeSigDura > Fraction(0, 1)) {
            int btp = _timeSigDura.denominator();
            if (btp > 0)
                  mDura = measureDurationAsFraction(mDura, btp);
            }

      // set return value(s)
      mdur = mDura;

      // set measure number and duration
      /*
       qDebug("part %s measure %s dura %s (%d)",
       qPrintable(partId), qPrintable(number), qPrintable(mdur.print()), mdur.ticks());
       */
      _parts[partId].addMeasureNumberAndDuration(number, mdur);
      }

//---------------------------------------------------------
//   print
//---------------------------------------------------------

void MusicXMLParserPass1::print(const int measureNr)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "print");
      _logger->logDebugTrace("MusicXMLParserPass1::print", &_e);

      const QString newPage = _e.attributes().value("new-page").toString();
      const QString newSystem = _e.attributes().value("new-system").toString();
      if (newPage == "yes")
            _pageStartMeasureNrs.insert(measureNr);
      if (newSystem == "yes")
            _systemStartMeasureNrs.insert(measureNr);

      _e.skipCurrentElement();        // skip but don't log
      }

//---------------------------------------------------------
//   attributes
//---------------------------------------------------------

void MusicXMLParserPass1::attributes(const MusicXML::Attributes& attributes, const QString& partId, const Fraction cTime)
      {
#if 0
      // TODO
            else if (_e.name() == "transpose")
                  transpose(partId, cTime);
#endif
      if (attributes.divisions > 0)
            _divs = attributes.divisions;
      setNumberOfStavesForPart(_partMap.value(partId), attributes.staves);
      if (attributes.times.size() == 1)
            time(attributes.times[0], cTime);
      }

//---------------------------------------------------------
//   clef
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/clef node.
 Note: Currently this is a NOP
 TODO: Store the clef type, to simplify staff type setting in pass 2.
 */

void MusicXMLParserPass1::clef(const QString& /* partId */)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "clef");
      _logger->logDebugTrace("MusicXMLParserPass1::clef", &_e);

      QString number = _e.attributes().value("number").toString();
      int n = 0;
      if (number != "") {
            n = number.toInt();
            if (n <= 0) {
                  _logger->logError(QString("invalid number %1").arg(number), &_e);
                  n = 0;
                  }
            else
                  n--;              // make zero-based
            }

      while (_e.readNextStartElement()) {
            if (_e.name() == "line")
                  _e.skipCurrentElement();  // skip but don't log
            else if (_e.name() == "sign")
                  QString sign = _e.readElementText();
            else
                  skipLogCurrElem();
            }
      }

//---------------------------------------------------------
//   determineTimeSig
//---------------------------------------------------------

/**
 Determine the time signature based on \a beats, \a beatType and \a timeSymbol.
 Sets return parameters \a st, \a bts, \a btp.
 Return true if OK, false on error.
 */

// TODO: share between pass 1 and pass 2

static bool determineTimeSig(MxmlLogger* logger, const QXmlStreamReader* const xmlreader,
                             const QString beats, const QString beatType, const QString timeSymbol,
                             TimeSigType& st, int& bts, int& btp)
      {
      // initialize
      st  = TimeSigType::NORMAL;
      bts = 0;             // the beats (max 4 separated by "+") as integer
      btp = 0;             // beat-type as integer
      // determine if timesig is valid
      if (beats == "2" && beatType == "2" && timeSymbol == "cut") {
            st = TimeSigType::ALLA_BREVE;
            bts = 2;
            btp = 2;
            return true;
            }
      else if (beats == "4" && beatType == "4" && timeSymbol == "common") {
            st = TimeSigType::FOUR_FOUR;
            bts = 4;
            btp = 4;
            return true;
            }
      else if (beats == "2" && beatType == "2" && timeSymbol == "cut2") {
            st = TimeSigType::CUT_BACH;
            bts = 2;
            btp = 2;
            return true;
            }
      else if (beats == "9" && beatType == "8" && timeSymbol == "cut3") {
            st = TimeSigType::CUT_TRIPLE;
            bts = 9;
            btp = 8;
            return true;
            }
      else {
            if (!timeSymbol.isEmpty() && timeSymbol != "normal") {
                  logger->logError(QString("time symbol '%1' not recognized with beats=%2 and beat-type=%3")
                                   .arg(timeSymbol, beats, beatType), xmlreader);
                  return false;
                  }

            btp = beatType.toInt();
            QStringList list = beats.split("+");
            for (int i = 0; i < list.size(); i++)
                  bts += list.at(i).toInt();
            }

      // determine if bts and btp are valid
      if (bts <= 0 || btp <=0) {
            logger->logError(QString("beats=%1 and/or beat-type=%2 not recognized")
                             .arg(beats, beatType), xmlreader);
            return false;
            }

      return true;
      }

//---------------------------------------------------------
//   time
//---------------------------------------------------------

void MusicXMLParserPass1::time(const MusicXML::Time& time, const Fraction cTime)
      {
      QString beats = time.beats.data();
      QString beatType = time.beatType.data();
      QString timeSymbol;       // TODO = _e.attributes().value("symbol").toString();

      if (beats != "" && beatType != "") {
            // determine if timesig is valid
            TimeSigType st  = TimeSigType::NORMAL;
            int bts = 0;             // total beats as integer (beats may contain multiple numbers, separated by "+")
            int btp = 0;             // beat-type as integer
            if (determineTimeSig(_logger, &_e, beats, beatType, timeSymbol, st, bts, btp)) {
                  _timeSigDura = Fraction(bts, btp);
                  _score->sigmap()->add(cTime.ticks(), _timeSigDura);
                  }
            }
      }

//---------------------------------------------------------
//   transpose
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/transpose node.
 */

void MusicXMLParserPass1::transpose(const QString& partId, const Fraction& tick)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "transpose");

      Interval interval;
      while (_e.readNextStartElement()) {
            int i = _e.readElementText().toInt();
            if (_e.name() == "diatonic") {
                  interval.diatonic = i;
                  }
            else if (_e.name() == "chromatic") {
                  interval.chromatic = i;
                  }
            else if (_e.name() == "octave-change") {
                  interval.diatonic += i * 7;
                  interval.chromatic += i * 12;
                  }
            else
                  skipLogCurrElem();
            }

      if (_parts[partId]._intervals.count(tick) == 0)
            _parts[partId]._intervals[tick] = interval;
      else
            qDebug("duplicate transpose at tick %s", qPrintable(tick.print()));
      }

//---------------------------------------------------------
//   divisions
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/divisions node.
 */

void MusicXMLParserPass1::divisions()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "divisions");

      _divs = _e.readElementText().toInt();
      if (!(_divs > 0))
            _logger->logError("illegal divisions", &_e);
      }

//---------------------------------------------------------
//   staves
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/staves node.
 */

void MusicXMLParserPass1::staves(const QString& partId)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "staves");
      _logger->logDebugTrace("MusicXMLParserPass1::staves", &_e);

      int staves = _e.readElementText().toInt();
      if (!(staves > 0 && staves <= MAX_STAVES)) {
            _logger->logError("illegal staves", &_e);
            return;
            }

      setNumberOfStavesForPart(_partMap.value(partId), staves);
      }

//---------------------------------------------------------
//   direction
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction node
 to be able to handle octave-shifts, as these must be interpreted
 in musical order instead of in MusicXML file order.
 */

void MusicXMLParserPass1::direction(const QString& partId, const Fraction cTime)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "direction");

      // note: file order is direction-type first, then staff
      // this means staff is still unknown when direction-type is handled

      QList<MxmlOctaveShiftDesc> starts;
      QList<MxmlOctaveShiftDesc> stops;
      int staff = 0;

      while (_e.readNextStartElement()) {
            if (_e.name() == "direction-type")
                  directionType(cTime, starts, stops);
            else if (_e.name() == "staff") {
                  int nstaves = getPart(partId)->nstaves();
                  QString strStaff = _e.readElementText();
                  staff = strStaff.toInt() - 1;
                  if (0 <= staff && staff < nstaves)
                        ;  //qDebug("direction staff %d", staff + 1);
                  else {
                        _logger->logError(QString("invalid staff %1").arg(strStaff), &_e);
                        staff = 0;
                        }
                  }
            else
                  _e.skipCurrentElement();
            }

      // handle the stops first
      foreach (auto desc, stops) {
            if (_octaveShifts.contains(desc.num)) {
                  MxmlOctaveShiftDesc prevDesc = _octaveShifts.value(desc.num);
                  if (prevDesc.tp == MxmlOctaveShiftDesc::Type::UP
                      || prevDesc.tp == MxmlOctaveShiftDesc::Type::DOWN) {
                        // a complete pair
                        _parts[partId].addOctaveShift(staff, prevDesc.size, prevDesc.time);
                        _parts[partId].addOctaveShift(staff, -prevDesc.size, desc.time);
                        }
                  else
                        _logger->logError("double octave-shift stop", &_e);
                  _octaveShifts.remove(desc.num);
                  }
            else
                  _octaveShifts.insert(desc.num, desc);
            }

      // then handle the starts
      foreach (auto desc, starts) {
            if (_octaveShifts.contains(desc.num)) {
                  MxmlOctaveShiftDesc prevDesc = _octaveShifts.value(desc.num);
                  if (prevDesc.tp == MxmlOctaveShiftDesc::Type::STOP) {
                        // a complete pair
                        _parts[partId].addOctaveShift(staff, desc.size, desc.time);
                        _parts[partId].addOctaveShift(staff, -desc.size, prevDesc.time);
                        }
                  else
                        _logger->logError("double octave-shift start", &_e);
                  _octaveShifts.remove(desc.num);
                  }
            else
                  _octaveShifts.insert(desc.num, desc);
            }
      }

//---------------------------------------------------------
//   directionType
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type node.
 */

void MusicXMLParserPass1::directionType(const Fraction cTime,
                                        QList<MxmlOctaveShiftDesc>& starts,
                                        QList<MxmlOctaveShiftDesc>& stops)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "direction-type");

      while (_e.readNextStartElement()) {
            if (_e.name() == "octave-shift") {
                  QString number = _e.attributes().value("number").toString();
                  int n = 0;
                  if (number != "") {
                        n = number.toInt();
                        if (n <= 0)
                              _logger->logError(QString("invalid number %1").arg(number), &_e);
                        else
                              n--;  // make zero-based
                        }

                  if (0 <= n && n < MAX_NUMBER_LEVEL) {
                        short size = _e.attributes().value("size").toShort();
                        QString type = _e.attributes().value("type").toString();
                        //qDebug("octave-shift type '%s' size %d number %d", qPrintable(type), size, n);
                        MxmlOctaveShiftDesc osDesc;
                        handleOctaveShift(cTime, type, size, osDesc);
                        osDesc.num = n;
                        if (osDesc.tp == MxmlOctaveShiftDesc::Type::UP
                            || osDesc.tp == MxmlOctaveShiftDesc::Type::DOWN)
                              starts.append(osDesc);
                        else if (osDesc.tp == MxmlOctaveShiftDesc::Type::STOP)
                              stops.append(osDesc);
                        }
                  else {
                        _logger->logError(QString("invalid octave-shift number %1").arg(number), &_e);
                        }
                  _e.skipCurrentElement();
                  }
            else
                  _e.skipCurrentElement();
            }

      Q_ASSERT(_e.isEndElement() && _e.name() == "direction-type");
      }

//---------------------------------------------------------
//   handleOctaveShift
//---------------------------------------------------------

void MusicXMLParserPass1::handleOctaveShift(const Fraction cTime,
                                            const QString& type, short size,
                                            MxmlOctaveShiftDesc& desc)
      {
      MxmlOctaveShiftDesc::Type tp = MxmlOctaveShiftDesc::Type::NONE;
      short sz = 0;

      switch (size) {
            case   8: sz =  1; break;
            case  15: sz =  2; break;
            default:
                  _logger->logError(QString("invalid octave-shift size %1").arg(size), &_e);
                  return;
            }

      if (!cTime.isValid() || cTime < Fraction(0, 1))
            _logger->logError("invalid current time", &_e);

      if (type == "up")
            tp = MxmlOctaveShiftDesc::Type::UP;
      else if (type == "down") {
            tp = MxmlOctaveShiftDesc::Type::DOWN;
            sz *= -1;
            }
      else if (type == "stop")
            tp = MxmlOctaveShiftDesc::Type::STOP;
      else {
            _logger->logError(QString("invalid octave-shift type '%1'").arg(type), &_e);
            return;
            }

      desc = MxmlOctaveShiftDesc(tp, sz, cTime);
      }

//---------------------------------------------------------
//   notations
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/notations node.
 */

void MusicXMLParserPass1::notations(MxmlStartStop& tupletStartStop)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "notations");
      //_logger->logDebugTrace("MusicXMLParserPass1::note", &_e);

      while (_e.readNextStartElement()) {
            if (_e.name() == "tuplet") {
                  QString tupletType       = _e.attributes().value("type").toString();

                  // ignore possible children (currently not supported)
                  _e.skipCurrentElement();

                  if (tupletType == "start")
                        tupletStartStop = MxmlStartStop::START;
                  else if (tupletType == "stop")
                        tupletStartStop = MxmlStartStop::STOP;
                  else if (tupletType != "" && tupletType != "start" && tupletType != "stop") {
                        _logger->logError(QString("unknown tuplet type '%1'").arg(tupletType), &_e);
                        }
                  }
            else {
                  _e.skipCurrentElement();        // skip but don't log
                  }
            }

      Q_ASSERT(_e.isEndElement() && _e.name() == "notations");
      }

//---------------------------------------------------------
//   smallestTypeAndCount
//---------------------------------------------------------

/**
 Determine the smallest note type and the number of those
 present in a ChordRest.
 For a note without dots the type equals the note type
 and count is one.
 For a single dotted note the type equals half the note type
 and count is three.
 A double dotted note is similar.
 Note: code assumes when duration().type() is incremented,
 the note length is divided by two, checked by tupletAssert().
 */

static void smallestTypeAndCount(const TDuration durType, int& type, int& count)
      {
      type = int(durType.type());
      count = 1;
      switch (durType.dots()) {
            case 0:
                  // nothing to do
                  break;
            case 1:
                  type += 1;       // next-smaller type
                  count = 3;
                  break;
            case 2:
                  type += 2;       // next-next-smaller type
                  count = 7;
                  break;
            default:
                  qDebug("smallestTypeAndCount() does not support more than 2 dots");
            }
      }

//---------------------------------------------------------
//   matchTypeAndCount
//---------------------------------------------------------

/**
 Given two note types and counts, if the types are not equal,
 make them equal by successively doubling the count of the
 largest type.
 */

static void matchTypeAndCount(int& type1, int& count1, int& type2, int& count2)
      {
      while (type1 < type2) {
            type1++;
            count1 *= 2;
            }
      while (type2 < type1) {
            type2++;
            count2 *= 2;
            }
      }

//---------------------------------------------------------
//   addDurationToTuplet
//---------------------------------------------------------

/**
 Add duration to tuplet duration
 Determine type and number of smallest notes in the tuplet
 */

void MxmlTupletState::addDurationToTuplet(const Fraction duration, const Fraction timeMod)
      {
      /*
      qDebug("1 duration %s timeMod %s -> state.tupletType %d state.tupletCount %d state.actualNotes %d state.normalNotes %d",
             qPrintable(duration.print()),
             qPrintable(timeMod.print()),
             m_tupletType,
             m_tupletCount,
             m_actualNotes,
             m_normalNotes
             );
      */
      if (m_duration <= Fraction(0, 1)) {
            // first note: init variables
            m_actualNotes = timeMod.denominator();
            m_normalNotes = timeMod.numerator();
            smallestTypeAndCount(duration / timeMod, m_tupletType, m_tupletCount);
            }
      else {
            int noteType = 0;
            int noteCount = 0;
            smallestTypeAndCount(duration / timeMod, noteType, noteCount);
            // match the types
            matchTypeAndCount(m_tupletType, m_tupletCount, noteType, noteCount);
            m_tupletCount += noteCount;
            }
      m_duration += duration;
      /*
      qDebug("2 duration %s -> state.tupletType %d state.tupletCount %d state.actualNotes %d state.normalNotes %d",
             qPrintable(duration.print()),
             m_tupletType,
             m_tupletCount,
             m_actualNotes,
             m_normalNotes
             );
      */
      }

//---------------------------------------------------------
//   determineTupletFractionAndFullDuration
//---------------------------------------------------------

/**
 Split duration into two factors where fullDuration is note sized
 (i.e. the denominator is a power of 2), 1/2 < fraction <= 1/1
 and fraction * fullDuration equals duration.
 */

void determineTupletFractionAndFullDuration(const Fraction duration, Fraction& fraction, Fraction& fullDuration)
      {
      fraction = duration;
      fullDuration = Fraction(1, 1);
      // move denominator's powers of 2 from fraction to fullDuration
      while (fraction.denominator() % 2 == 0) {
            fraction *= 2;
            fraction.reduce();
            fullDuration *= Fraction(1, 2);
            }
      // move numerator's powers of 2 from fraction to fullDuration
      while ( fraction.numerator() % 2 == 0) {
            fraction *= Fraction(1, 2);
            fraction.reduce();
            fullDuration *= 2;
            fullDuration.reduce();
            }
      // make sure 1/2 < fraction <= 1/1
      while (fraction <= Fraction(1, 2)) {
            fullDuration *= Fraction(1, 2);
            fraction *= 2;
            }
      fullDuration.reduce();
      fraction.reduce();

      /*
      Examples (note result when denominator is not a power of two):
      3:2 tuplet of 1/4 results in fraction 1/1 and fullDuration 1/2
      2:3 tuplet of 1/4 results in fraction 3/1 and fullDuration 1/4
      4:3 tuplet of 1/4 results in fraction 3/1 and fullDuration 1/4
      3:4 tuplet of 1/4 results in fraction 1/1 and fullDuration 1/1

       Bring back fraction in 1/2 .. 1/1 range.
       */

      if (fraction > Fraction(1, 1) && fraction.denominator() == 1) {
            fullDuration *= fraction;
            fullDuration.reduce();
            fraction = Fraction(1, 1);
            }

      /*
      qDebug("duration %s fraction %s fullDuration %s",
             qPrintable(duration.toString()),
             qPrintable(fraction.toString()),
             qPrintable(fullDuration.toString())
             );
      */
      }

//---------------------------------------------------------
//   isTupletFilled
//---------------------------------------------------------

/**
 Determine if the tuplet is completely filled,
 because either (1) it is at least the same duration
 as the specified number of the specified normal type notes
 or (2) the duration adds up to a normal note duration.

 Example (1): a 3:2 tuplet with a 1/4 and a 1/8 note
 is filled if normal type is 1/8,
 it is not filled if normal type is 1/4.

 Example (2): a 3:2 tuplet with a 1/4 and a 1/8 note is filled.
 */

static bool isTupletFilled(const MxmlTupletState& state, const TDuration normalType, const Fraction timeMod)
      {
      Q_UNUSED(timeMod);
      bool res { false };
      const auto actualNotes = state.m_actualNotes;
      /*
      const auto normalNotes = state.m_normalNotes;
      qDebug("duration %s normalType %s timeMod %s normalNotes %d actualNotes %d",
             qPrintable(state.m_duration.toString()),
             qPrintable(normalType.fraction().toString()),
             qPrintable(timeMod.toString()),
             normalNotes,
             actualNotes
             );
      */

      auto tupletType = state.m_tupletType;
      auto tupletCount = state.m_tupletCount;

      if (normalType.isValid()) {
            int matchedNormalType  = int(normalType.type());
            int matchedNormalCount = actualNotes;
            // match the types
            matchTypeAndCount(tupletType, tupletCount, matchedNormalType, matchedNormalCount);
            // ... result scenario (1)
            res = tupletCount >= matchedNormalCount;
            /*
            qDebug("normalType valid tupletType %d tupletCount %d matchedNormalType %d matchedNormalCount %d res %d",
                   tupletType,
                   tupletCount,
                   matchedNormalType,
                   matchedNormalCount,
                   res
                   );
             */
            }
      else {
            // ... result scenario (2)
            res = tupletCount >= actualNotes;
            /*
            qDebug("normalType not valid tupletCount %d actualNotes %d res %d",
                   tupletCount,
                   actualNotes,
                   res
                   );
             */
            }
      return res;
      }

//---------------------------------------------------------
//   missingTupletDuration
//---------------------------------------------------------

Fraction missingTupletDuration(const Fraction duration)
      {
      Fraction tupletFraction;
      Fraction tupletFullDuration;

      determineTupletFractionAndFullDuration(duration, tupletFraction, tupletFullDuration);
      auto missing = (Fraction(1, 1) - tupletFraction) * tupletFullDuration;

      return missing;
      }

//---------------------------------------------------------
//   determineTupletAction
//---------------------------------------------------------

/**
 Update tuplet state using parse result tupletDesc.
 Tuplets with <actual-notes> and <normal-notes> but without <tuplet>
 are handled correctly.
 TODO Nested tuplets are not (yet) supported.
 */

MxmlTupletFlags MxmlTupletState::determineTupletAction(const Fraction noteDuration,
                                                       const Fraction timeMod,
                                                       const MxmlStartStop tupletStartStop,
                                                       const TDuration normalType,
                                                       Fraction& missingPreviousDuration,
                                                       Fraction& missingCurrentDuration
                                                       )
      {
      const auto actualNotes = timeMod.denominator();
      const auto normalNotes = timeMod.numerator();
      MxmlTupletFlags res = MxmlTupletFlag::NONE;

      // check for unexpected termination of previous tuplet
      if (m_inTuplet && timeMod == Fraction(1, 1)) {
            // recover by simply stopping the current tuplet first
            if (!isTupletFilled(*this, normalType, timeMod)) {
                  missingPreviousDuration = missingTupletDuration(m_duration);
                  //qDebug("tuplet incomplete, missing %s", qPrintable(missingPreviousDuration.print()));
                  }
            *this = {};
            res |= MxmlTupletFlag::STOP_PREVIOUS;
            }

      // check for obvious errors
      if (m_inTuplet && tupletStartStop == MxmlStartStop::START) {
            qDebug("tuplet already started");
            // recover by simply stopping the current tuplet first
            if (!isTupletFilled(*this, normalType, timeMod)) {
                  missingPreviousDuration = missingTupletDuration(m_duration);
                  //qDebug("tuplet incomplete, missing %s", qPrintable(missingPreviousDuration.print()));
                  }
            *this = {};
            res |= MxmlTupletFlag::STOP_PREVIOUS;
            }
      if (tupletStartStop == MxmlStartStop::STOP && !m_inTuplet) {
            qDebug("tuplet stop but no tuplet started");       // TODO
            // recovery handled later (automatically, no special case needed)
            }

      // Tuplet are either started by the tuplet start
      // or when the time modification is first found.
      if (!m_inTuplet) {
            if (tupletStartStop == MxmlStartStop::START
                || (!m_inTuplet && (actualNotes != 1 || normalNotes != 1))) {
                  if (tupletStartStop != MxmlStartStop::START) {
                        m_implicit = true;
                        }
                  else {
                        m_implicit = false;
                        }
                  // create a new tuplet
                  m_inTuplet = true;
                  res |= MxmlTupletFlag::START_NEW;
                  }
            }

      // Add chord to the current tuplet.
      // Must also check for actual/normal notes to prevent
      // adding one chord too much if tuplet stop is missing.
      if (m_inTuplet && !(actualNotes == 1 && normalNotes == 1)) {
            addDurationToTuplet(noteDuration, timeMod);
            res |= MxmlTupletFlag::ADD_CHORD;
            }

      // Tuplets are stopped by the tuplet stop
      // or when the tuplet is filled completely
      // (either with knowledge of the normal type
      // or as a last resort calculated based on
      // actual and normal notes plus total duration)
      // or when the time-modification is not found.

      if (m_inTuplet) {
            if (tupletStartStop == MxmlStartStop::STOP
                || (m_implicit && isTupletFilled(*this, normalType, timeMod))
                || (actualNotes == 1 && normalNotes == 1)) {       // incorrect ??? check scenario incomplete tuplet w/o start
                  if (actualNotes > normalNotes && !isTupletFilled(*this, normalType, timeMod)) {
                        missingCurrentDuration = missingTupletDuration(m_duration);
                        qDebug("current tuplet incomplete, missing %s", qPrintable(missingCurrentDuration.print()));
                        }

                  *this = {};
                  res |= MxmlTupletFlag::STOP_CURRENT;
                  }
            }

      return res;
      }

//---------------------------------------------------------
//   note
//---------------------------------------------------------

void MusicXMLParserPass1::note(const MusicXML::Note& note, const QString& partId, const Fraction sTime, Fraction& missingPrev, Fraction& dura, Fraction& missingCurr, VoiceOverlapDetector& vod, MxmlTupletStates& tupletStates)
      {
      /* TODO
       if (_e.attributes().value("print-spacing") == "no") {
       notePrintSpacingNo(dura);
       return;
       }
       */

      const QString type = note.type.data();
      const QString voice = note.voice.data();
      QString instrId; // TODO
      MxmlStartStop tupletStartStop { MxmlStartStop::NONE };

      mxmlNoteDuration mnd(_divs, _logger);
      const Fraction timeModification { static_cast<int>(note.timeModification.normalNotes), static_cast<int>(note.timeModification.actualNotes) };
      mnd.setProperties(note.duration, note.dots, timeModification);

      /* TODO
       while (_e.readNextStartElement()) {
       else if (_e.name() == "beam") {
       _hasBeamingInfo = true;
       _e.skipCurrentElement();  // skip but don't log
       }
       else if (_e.name() == "instrument") {
       instrId = _e.attributes().value("id").toString();
       _e.readNext();
       }
       else if (_e.name() == "lyric") {
       const auto number = _e.attributes().value("number").toString();
       _parts[partId].lyricNumberHandler().addNumber(number);
       _e.skipCurrentElement();
       }
       else if (_e.name() == "notations")
       notations(tupletStartStop);
       }
       */

      _parts[partId].setMaxStaff(note.staff);
      Part* part = _partMap.value(partId);
      Q_ASSERT(part); // TODO replace
      if (note.staff <= 0 || note.staff > static_cast<unsigned int>(part->nstaves()))
            _logger->logError(QString("illegal staff '%1'").arg(note.staff), &_e);
      const int staff = note.staff - 1;    // convert staff to zero-based

      // multi-instrument handling
      QString prevInstrId = _parts[partId]._instrList.instrument(sTime);
      bool mustInsert = instrId != prevInstrId;
      /*
       qDebug("tick %s (%d) staff %d voice '%s' previnst='%s' instrument '%s' mustInsert %d",
       qPrintable(sTime.print()),
       sTime.ticks(),
       staff + 1,
       qPrintable(voice),
       qPrintable(prevInstrId),
       qPrintable(instrId),
       mustInsert
       );
       */
      if (mustInsert)
            _parts[partId]._instrList.setInstrument(instrId, sTime);

      // check for timing error(s) and set dura
      // keep in this order as checkTiming() might change dura
      auto errorStr = mnd.checkTiming(type, note.rest, note.grace);
      dura = mnd.dura();
      if (errorStr != "")
            _logger->logError(errorStr, &_e);

      // don't count chord or grace note duration
      // note that this does not check the MusicXML requirement that notes in a chord
      // cannot have a duration longer than the first note in the chord
      missingPrev.set(0, 1);
      if (note.chord || note.grace)
            dura.set(0, 1);

      if (!note.chord && !note.grace) {
            // do tuplet
            auto timeMod = mnd.timeMod();
            auto& tupletState = tupletStates[voice];
            tupletState.determineTupletAction(mnd.dura(), timeMod, tupletStartStop, mnd.normalType(), missingPrev, missingCurr);
            }

      // store result
      if (dura.isValid() && dura > Fraction(0, 1)) {
            // count the chords
            if (!_parts.value(partId).voicelist.contains(voice)) {
                  VoiceDesc vs;
                  _parts[partId].voicelist.insert(voice, vs);
                  }
            _parts[partId].voicelist[voice].incrChordRests(staff);
            // determine note length for voice overlap detection
            vod.addNote((sTime + missingPrev).ticks(), (sTime + missingPrev + dura).ticks(), voice, staff);
            }
      }

//---------------------------------------------------------
//   notePrintSpacingNo
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note node for a note with print-spacing="no".
 These are handled like a forward: only moving the time forward.
 */

void MusicXMLParserPass1::notePrintSpacingNo(Fraction& dura)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "note");
      //_logger->logDebugTrace("MusicXMLParserPass1::notePrintSpacingNo", &_e);

      bool chord = false;
      bool grace = false;

      while (_e.readNextStartElement()) {
            if (_e.name() == "chord") {
                  chord = true;
                  _e.readNext();
                  }
            else if (_e.name() == "duration")
                  ;  // TODO duration(dura);
            else if (_e.name() == "grace") {
                  grace = true;
                  _e.readNext();
                  }
            else
                  _e.skipCurrentElement();        // skip but don't log
            }

      // don't count chord or grace note duration
      // note that this does not check the MusicXML requirement that notes in a chord
      // cannot have a duration longer than the first note in the chord
      if (chord || grace)
            dura.set(0, 1);

      Q_ASSERT(_e.isEndElement() && _e.name() == "note");
      }

//---------------------------------------------------------
//   duration
//---------------------------------------------------------

void MusicXMLParserPass1::duration(const unsigned int duration, Fraction& dura)
      {
      dura.set(0, 0);  // invalid unless set correctly
      if (duration > 0) {
            if (_divs > 0) {
                  dura.set(duration, 4 * _divs);
                  dura.reduce(); // prevent overflow in later Fraction operations
                  }
            else
                  _logger->logError("illegal or uninitialized divisions", &_e);
            }
      else
            _logger->logError("illegal duration", &_e);
      }

//---------------------------------------------------------
//   forward
//---------------------------------------------------------

void MusicXMLParserPass1::forward(const unsigned int duration, Fraction& dura)
      {
      MusicXMLParserPass1::duration(duration, dura);
      }

//---------------------------------------------------------
//   backup
//---------------------------------------------------------

void MusicXMLParserPass1::backup(const unsigned int duration, Fraction& dura)
      {
      MusicXMLParserPass1::duration(duration, dura);
      }

//---------------------------------------------------------
//   timeModification
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/time-modification node.
 */

void MusicXMLParserPass1::timeModification(Fraction& timeMod)
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "time-modification");
      //_logger->logDebugTrace("MusicXMLParserPass1::timeModification", &_e);

      int intActual = 0;
      int intNormal = 0;
      QString strActual;
      QString strNormal;

      while (_e.readNextStartElement()) {
            if (_e.name() == "actual-notes")
                  strActual = _e.readElementText();
            else if (_e.name() == "normal-notes")
                  strNormal = _e.readElementText();
            else
                  skipLogCurrElem();
            }

      intActual = strActual.toInt();
      intNormal = strNormal.toInt();
      if (intActual > 0 && intNormal > 0)
            timeMod.set(intNormal, intActual);
      else {
            timeMod.set(1, 1);
            _logger->logError(QString("illegal time-modification: actual-notes %1 normal-notes %2")
                              .arg(strActual, strNormal), &_e);
            }
      }

//---------------------------------------------------------
//   rest
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/rest node.
 */

void MusicXMLParserPass1::rest()
      {
      Q_ASSERT(_e.isStartElement() && _e.name() == "rest");
      //_logger->logDebugTrace("MusicXMLParserPass1::rest", &_e);

      while (_e.readNextStartElement()) {
            if (_e.name() == "display-octave")
                  _e.skipCurrentElement();  // skip but don't log
            else if (_e.name() == "display-step")
                  _e.skipCurrentElement();  // skip but don't log
            else
                  skipLogCurrElem();
            }
      }

} // namespace Ms
