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
#if 1
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


//---------------------------------------------------------
//   dump_measure
//---------------------------------------------------------

void dump_measure(const musicxml::measure1& measure)
{
    std::string number {measure.number()};
    std::cout << "  measure number \"" << number << "\"" << std::endl;
    const auto& content_orders = measure.content_order();
    for (auto & content_order : content_orders) {
        switch (content_order.id) {
        case musicxml::measure1::note_id:
        {
            std::cout << "    "
                      << "note"
                      << std::endl;
        }
            break;
        case musicxml::measure1::backup_id:
        {
            std::cout << "    "
                      << "backup"
                      << std::endl;
        }
            break;
        case musicxml::measure1::forward_id:
        {
            std::cout << "    "
                      << "forward"
                      << std::endl;
        }
            break;
        case musicxml::measure1::attributes_id:
        {
            std::cout << "    "
                      << "attributes"
                      << std::endl;
        }
            break;
        case musicxml::measure1::barline_id:
        {
            std::cout << "    "
                      << "barline"
                      << std::endl;
        }
            break;
        default:
            // ignore
            break;
        }
    }
}

//---------------------------------------------------------
//   dump_parts
//---------------------------------------------------------

void dump_parts(const xsd::cxx::tree::sequence<musicxml::part>& parts)
{
    for (const auto& part : parts) {
        std::string id {part.id()};
        std::cout << "part"
                  << " id \"" << id << "\""
                  << std::endl;
        for (const auto& measure : part.measure()) {
            dump_measure(measure);
        }
    }
}

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

static void movementWork(const musicxml::score_partwise& mxmlScorePartwise, Score* const score);

Score::FileError MusicXMLParserPass1::parse(const musicxml::score_partwise& score_partwise)
{
    dump_parts(score_partwise.part());
    movementWork(score_partwise, _score);
    if (score_partwise.defaults()) {
        newDefaults(*score_partwise.defaults());
    }
    for (const auto& credit : score_partwise.credit()) {
        newCredit(credit, _credits);
    }
    newPartList(score_partwise.part_list());
    for (const auto& part : score_partwise.part()) {
        newPart(part);
    }

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

    return Score::FileError::FILE_NO_ERROR;
}

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

/**
 Start the parsing process, after verifying the top-level node is score-partwise
 */


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

static void movementWork(const musicxml::score_partwise& mxmlScorePartwise, Score* const score)
{
    if (mxmlScorePartwise.work()) {
        const auto& work = *mxmlScorePartwise.work();
        if (work.work_number()) {
            setNonEmptyMetaTag(score, "workNumber", (*work.work_number()).data());
        }
        if (work.work_title()) {
            setNonEmptyMetaTag(score, "workTitle", (*work.work_title()).data());
        }
    }
    if (mxmlScorePartwise.movement_number()) {
        setNonEmptyMetaTag(score, "movementNumber", (*mxmlScorePartwise.movement_number()).data());
    }
    if (mxmlScorePartwise.movement_title()) {
        setNonEmptyMetaTag(score, "movementTitle", (*mxmlScorePartwise.movement_title()).data());
    }
}

//---------------------------------------------------------
//   scorePartwise
//---------------------------------------------------------

/**
 Parse the MusicXML top-level (XPath /score-partwise) node.
 */


//---------------------------------------------------------
//   identification
//---------------------------------------------------------

/**
 Parse the /score-partwise/identification node:
 read the metadata.
 */


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

void MusicXMLParserPass1::newCredit(const musicxml::credit& mxmlCredit, CreditWordsList& credits)
{
    qDebug("credit page %lld", mxmlCredit.page() ? *mxmlCredit.page() : 0);
    const int page = mxmlCredit.page() ? (*mxmlCredit.page() - 1) : 0; // defaults to 0 (the first page)
    // multiple credit-words elements may be present,
    // which are appended
    // use the position info from the first one
    // font information is ignored, credits will be styled
    bool creditWordsRead = false;
    double defaultx = 0;
    double defaulty = 0;
    double fontSize = 0;
    QString justify;
    QString halign;
    QString valign;
    QStringList crtypes;
    QString crwords;
#if 0
    while (_e.readNextStartElement()) {
          if (_e.name() == "credit-words") {
                // IMPORT_LAYOUT
                if (!creditWordsRead) {
                      defaultx = _e.attributes().value("default-x").toString().toDouble();
                      defaulty = _e.attributes().value("default-y").toString().toDouble();
                      fontSize = _e.attributes().value("font-size").toString().toDouble();
                      justify  = _e.attributes().value("justify").toString();
                      halign   = _e.attributes().value("halign").toString();
                      valign   = _e.attributes().value("valign").toString();
                      creditWordsRead = true;
                      }
                crwords += nextPartOfFormattedString(_e);
                }
          else if (_e.name() == "credit-type") {
                // multiple credit-type elements may be present, supported by
                // e.g. Finale v26.3 for Mac.
                crtypes += _e.readElementText();
                }
          else
                skipLogCurrElem();
          }
#endif
    // TODO: check ordering depencies credit-words, -type and -symbol
    for (const auto& words : mxmlCredit.credit_words()) {
        qDebug("credit page %d words '%s'", page, words.data());
        // IMPORT_LAYOUT
        if (!creditWordsRead) {
              if (words.default_x()) {
                  defaultx = *words.default_x();
              }
              // does not compile: defaulty = words.default_x() ? *words.default_x() : 0.0;
              if (words.default_y()) {
                  defaulty = *words.default_y();
              }
              /* TODO
              // does not compile:
              if (words.font_size()) {
                  fontSize = *words.font_size();
              }
              justify  = _e.attributes().value("justify").toString();
              halign   = _e.attributes().value("halign").toString();
              valign   = _e.attributes().value("valign").toString();
              */
              creditWordsRead = true;
              }
        crwords += words.data(); // TODO: formatting ? (nextPartOfFormattedString()-like)
    }
    if (crwords != "") {
          // as the meaning of multiple credit-types is undocumented,
          // use credit-type only if exactly one was found
          QString crtype = (crtypes.size() == 1) ? crtypes.at(0) : "";
          CreditWords* cw = new CreditWords(page, crtype, defaultx, defaulty, fontSize, justify, halign, valign, crwords);
          credits.append(cw);
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

void MusicXMLParserPass1::newDefaults(const musicxml::defaults& mxmlDefaults)
{
    double millimeters = _score->spatium()/10.0;
    double tenths = 1.0;
    QString lyricFontFamily;
    QString lyricFontSize;
    QString wordFontFamily;
    QString wordFontSize;

#if 0
    // TODO
    while (_e.readNextStartElement()) {
          else if (_e.name() == "system-layout") {
                while (_e.readNextStartElement()) {
                      if (_e.name() == "system-dividers")
                            _e.skipCurrentElement();  // skip but don't log
                      else if (_e.name() == "system-margins")
                            _e.skipCurrentElement();  // skip but don't log
                      else if (_e.name() == "system-distance") {
                            Spatium val(_e.readElementText().toDouble() / 10.0);
                            if (preferences.getBool(PREF_IMPORT_MUSICXML_IMPORTLAYOUT)) {
                                  _score->style().set(Sid::minSystemDistance, val);
                                  //qDebug("system distance %f", val.val());
                                  }
                            }
                      else if (_e.name() == "top-system-distance")
                            _e.skipCurrentElement();  // skip but don't log
                      else
                            skipLogCurrElem();
                      }
                }
          else if (_e.name() == "staff-layout") {
                while (_e.readNextStartElement()) {
                      if (_e.name() == "staff-distance") {
                            Spatium val(_e.readElementText().toDouble() / 10.0);
                            if (preferences.getBool(PREF_IMPORT_MUSICXML_IMPORTLAYOUT))
                                  _score->style().set(Sid::staffDistance, val);
                            }
                      else
                            skipLogCurrElem();
                      }
                }
          else if (_e.name() == "music-font")
                _e.skipCurrentElement();  // skip but don't log
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
          else if (_e.name() == "lyric-language")
                _e.skipCurrentElement();  // skip but don't log
          else
                skipLogCurrElem();
          }
#endif

    if (mxmlDefaults.scaling()) {
        millimeters = (*mxmlDefaults.scaling()).millimeters();
        tenths = (*mxmlDefaults.scaling()).tenths();
        double _spatium = DPMM * (millimeters * 10.0 / tenths);
        if (preferences.getBool(PREF_IMPORT_MUSICXML_IMPORTLAYOUT)) {
            _score->setSpatium(_spatium);
        }
    }
    qDebug("millimeters %g tenths %g", millimeters, tenths);
    if (mxmlDefaults.page_layout()) {
        PageFormat pf;
        newPageLayout(*mxmlDefaults.page_layout(), pf, millimeters / (tenths * INCH));
        if (preferences.getBool(PREF_IMPORT_MUSICXML_IMPORTLAYOUT)) {
            setPageFormat(_score, pf);
        }
    }

    /*
    qDebug("word font family '%s' size '%s' lyric font family '%s' size '%s'",
           qPrintable(wordFontFamily), qPrintable(wordFontSize),
           qPrintable(lyricFontFamily), qPrintable(lyricFontSize));
    */
    updateStyles(_score, wordFontFamily, wordFontSize, lyricFontFamily, lyricFontSize);

    _score->setDefaultsRead(true); // TODO only if actually succeeded ?
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

void MusicXMLParserPass1::newPageLayout(const musicxml::page_layout& mxmlPageLayout, PageFormat& pf, const qreal conversion)
{
    qreal _oddRightMargin  = 0.0;
    qreal _evenRightMargin = 0.0;
    QSizeF size;

    if (mxmlPageLayout.page_height() && mxmlPageLayout.page_width()) {
        double ph { *mxmlPageLayout.page_height() };
        double pw { *mxmlPageLayout.page_width() };
        qDebug("page height %g width %g", ph, pw);
        size.rheight() = ph * conversion;
        size.rwidth() = pw * conversion;
        // set pageHeight and pageWidth for use by doCredits()
        _pageSize.setHeight(static_cast<int>(ph + 0.5));
        _pageSize.setWidth(static_cast<int>(pw + 0.5));
    }
    for (const auto& pm : mxmlPageLayout.page_margins()) {
        musicxml::margin_type::value tp = musicxml::margin_type::both;
        if (pm.type()) {
            tp = *pm.type();
        }
        double lm { pm.left_margin() };
        double rm { pm.right_margin() };
        double tm { pm.top_margin() };
        double bm { pm.bottom_margin() };
        qDebug("tp %d lm %g rm %g tm %g bm %g", tp, lm, rm, tm, bm);
        lm *= conversion;
        rm *= conversion;
        tm *= conversion;
        bm *= conversion;
        pf.twosided = tp == musicxml::margin_type::odd || tp == musicxml::margin_type::even;
        if (tp == musicxml::margin_type::odd || tp == musicxml::margin_type::both) {
            qDebug("odd");
            pf.oddLeftMargin = lm;
            _oddRightMargin = rm;
            pf.oddTopMargin = tm;
            pf.oddBottomMargin = bm;
            qDebug("oddLeftMargin %g", pf.oddLeftMargin);
        }
        if (tp == musicxml::margin_type::even || tp == musicxml::margin_type::both) {
            qDebug("even");
            pf.evenLeftMargin = lm;
            _evenRightMargin = rm;
            pf.evenTopMargin = tm;
            pf.evenBottomMargin = bm;
            qDebug("evenLeftMargin %g", pf.evenLeftMargin);
        }
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
 Parse the /score-partwise/part-list:
 create the parts and for each part set id and name.
 Also handle the part-groups.
 */

void MusicXMLParserPass1::newPartList(const musicxml::part_list& part_list /*TODO , MusicXmlPartGroupList& partGroupList */)
{
    const auto& content_orders = part_list.content_order();
    for (auto & content_order : content_orders) {
        switch (content_order.id) {
        case musicxml::part_list::part_group_id:
        {
            /* TODO
            const auto& part_group {part_list.part_group()[content_order.index]};
            std::string number {part_group.number()};
            std::string type;
            if (part_group.type() == musicxml::start_stop::start) {
                type = "start";
            }
            else if (part_group.type() == musicxml::start_stop::stop) {
                type = "stop";
            }
            std::cout << "part-group"
                      << " number \"" << number << "\""
                      << " type " << type
                      << std::endl;
                      */
        }
            break;
        case musicxml::part_list::score_part_id:
        {
            const auto& score_part {part_list.score_part()[content_order.index]};
            newScorePart(score_part);
        }
            break;
        default:
            assert(false); // throw ?
            break;
        }
    }
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
 Parse the /score-partwise/part-list/score-part node:
 create the part and sets id and name.
 Note that a part is created even if no part-name is present
 which is invalid MusicXML but is (sometimes ?) generated by NWC2MusicXML.
 */

void MusicXMLParserPass1::newScorePart(const musicxml::score_part& score_part)
{
    QString id { score_part.id().data() };

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
    QString name { score_part.part_name().data() };
    _parts[id].setName(name);

#if 0
    // TODO else if (_e.name() == "part-abbreviation") {
    // Element part-name contains the displayed (abbreviated) part name
    // It is displayed by default, but can be suppressed (print-object=”no”)
    // As of MusicXML 3.0, formatting is deprecated, with part-name in plain text
    // and the formatted version in the part-abbreviation-display element
    _parts[id].setPrintAbbr(!(_e.attributes().value("print-object") == "no"));
    QString name = _e.readElementText();
    _parts[id].setAbbr(name);

    // TODO scoreInstrument(id);
    // TODO else if (_e.name() == "midi-device") {
    if (!_e.attributes().hasAttribute("port")) {
        _e.readElementText(); // empty string
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

    _e.readElementText(); // empty string
}
// TODO midiInstrument(id);
#endif
}

//---------------------------------------------------------
//   scoreInstrument
//---------------------------------------------------------

/**
 Parse the /score-partwise/part-list/score-part/score-instrument node.
 */


//---------------------------------------------------------
//   midiInstrument
//---------------------------------------------------------

/**
 Parse the /score-partwise/part-list/score-part/midi-instrument node.
 */


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
 Parse the /score-partwise/part node:
 read the parts data to determine measure timing and octave shifts.
 Assign voices and staves.
 */

void MusicXMLParserPass1::newPart(const musicxml::part& part)
{
    QString id {part.id().data()};
    qDebug("id '%s'", qPrintable(id));

    if (!_parts.contains(id)) {
        // TODO _logger->logError(QString("duplicate part id '%1'").arg(id), &_e);
    }

    initPartState(id);

    VoiceOverlapDetector vod;
    Fraction time;  // current time within part
    Fraction mdur;  // measure duration

    int measureNr = 0;
    for (const auto& measure : part.measure()) {
        newMeasure(measure, id, time, mdur, vod, measureNr);
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
//   note_type_value_to_string
//---------------------------------------------------------

// TODO: make strongly typed and improve compatibility with mxml-bdg-exp-3.6.2 design
// TODO: remove duplication of code between pass 1 and 2

static std::string note_type_value_to_string(::musicxml::note_type_value v)
{
    if (v == ::musicxml::note_type_value::cxx_1024th) {
        return "1024th";
    }
    else if (v == ::musicxml::note_type_value::cxx_512th) {
        return "512th";
    }
    else if (v == ::musicxml::note_type_value::cxx_256th) {
        return "256th";
    }
    else if (v == ::musicxml::note_type_value::cxx_128th) {
        return "128th";
    }
    else if (v == ::musicxml::note_type_value::cxx_64th) {
        return "64th";
    }
    else if (v == ::musicxml::note_type_value::cxx_32nd) {
        return "32nd";
    }
    else if (v == ::musicxml::note_type_value::cxx_16th) {
        return "16th";
    }
    else if (v == ::musicxml::note_type_value::eighth) {
        return "eighth";
    }
    else if (v == ::musicxml::note_type_value::quarter) {
        return "quarter";
    }
    else if (v == ::musicxml::note_type_value::half) {
        return "half";
    }
    else if (v == ::musicxml::note_type_value::whole) {
        return "whole";
    }
    else if (v == ::musicxml::note_type_value::breve) {
        return "breve";
    }
    else if (v == ::musicxml::note_type_value::long_) {
        return "long";
    }
    else if (v == ::musicxml::note_type_value::maxima) {
        return "maxima";
    }
    else {
        return "note_type_value_unknown";
    }
}

//---------------------------------------------------------
//   measure
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure node:
 read the measures data as required to determine measure timing, octave shifts
 and assign voices and staves.
 */

void MusicXMLParserPass1::newMeasure(const musicxml::measure1& measure, const QString& partId, const Fraction cTime, Fraction& mdur, VoiceOverlapDetector& vod, const int measureNr)
{
    qDebug("part %s measure %d", qPrintable(partId), measureNr);
    const QString number { measure.number().data() };

    Fraction mTime; // current time stamp within measure
    Fraction mDura; // current total measure duration
    vod.newMeasure();
    MxmlTupletStates tupletStates;

    const auto& content_orders = measure.content_order();
    for (auto & content_order : content_orders) {
        switch (content_order.id) {
        case musicxml::measure1::note_id:
        {
            qDebug("note");
            const auto& note { measure.note()[content_order.index] };
            if (note.type()) {
                qDebug(" type '%s'", note_type_value_to_string(*note.type()).data());
            }
            Fraction missingPrev;
            Fraction dura;
            Fraction missingCurr;
            // note: chord and grace note handling done in note()
            newNote(note, partId, cTime + mTime, missingPrev, dura, missingCurr, vod, tupletStates);
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
            break;
        case musicxml::measure1::backup_id:
        {
            qDebug("backup");
            const auto& backup { measure.backup()[content_order.index] };
            Fraction dura;
            newBackup(backup.duration(), dura);
            if (dura.isValid()) {
                if (dura <= mTime)
                    mTime -= dura;
                else {
                    _logger->logError("backup beyond measure start", &_e);
                    mTime.set(0, 1);
                }
            }
        }
            break;
        case musicxml::measure1::forward_id:
        {
            qDebug("forward");
            const auto& forward { measure.forward()[content_order.index] };
            Fraction dura;
            newForward(forward.duration(), dura);
            if (dura.isValid()) {
                mTime += dura;
                if (mTime > mDura)
                    mDura = mTime;
            }
        }
            break;
        case musicxml::measure1::attributes_id:
        {
            qDebug("attributes");
            const auto& attributes { measure.attributes()[content_order.index] };
            newAttributes(attributes, partId, cTime + mTime);
        }
            break;
        case musicxml::measure1::barline_id:
        {
            qDebug("barline");
        }
            break;
        default:
            qDebug("default");
            // ignore
            break;
        }
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

#if 1
    // TODO

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
#endif
}

//---------------------------------------------------------
//   print
//---------------------------------------------------------


//---------------------------------------------------------
//   attributes
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes node.
 */

void MusicXMLParserPass1::newAttributes(const musicxml::attributes& attributes, const QString& partId, const Fraction cTime)
{
#if 0
    // TODO
    while (_e.readNextStartElement()) {
        if (_e.name() == "clef")
            clef(partId);
        else if (_e.name() == "key")
            _e.skipCurrentElement();  // skip but don't log
        else if (_e.name() == "instruments")
            _e.skipCurrentElement();  // skip but don't log
        else if (_e.name() == "staff-details")
            _e.skipCurrentElement();  // skip but don't log
        else if (_e.name() == "transpose")
            transpose(partId, cTime);
    }
#endif
    if (attributes.divisions()) {
        _divs = *attributes.divisions();
    }
    if (attributes.staves()) {
        setNumberOfStavesForPart(_partMap.value(partId), *attributes.staves());
    }
    // following code handles only one time element
    if (attributes.time().size() >= 1) {
        newTime(attributes.time()[0], cTime);
    }
}

//---------------------------------------------------------
//   clef
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/clef node.
 TODO: Store the clef type, to simplify staff type setting in pass 2.
 */


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

/**
 Parse the /score-partwise/part/measure/attributes/time node.
 */

void MusicXMLParserPass1::newTime(const musicxml::time& time, const Fraction cTime)
{
    // following code handles only one beats and beat-type element
    QString beats;
    if (time.beats().size() >= 1) {
        beats = time.beats()[0].data();
    }
    QString beatType;
    if (time.beat_type().size() >= 1) {
        beatType = time.beat_type()[0].data();
    }
    //qDebug("beats '%s' beatType '%s'", qPrintable(beats), qPrintable(beatType));
    QString timeSymbol; // TODO = _e.attributes().value("symbol").toString();
    const QXmlStreamReader dummy; // TODO

    if (beats != "" && beatType != "") {
        // determine if timesig is valid
        TimeSigType st  = TimeSigType::NORMAL;
        int bts = 0;       // total beats as integer (beats may contain multiple numbers, separated by "+")
        int btp = 0;       // beat-type as integer
        if (determineTimeSig(_logger, &dummy, beats, beatType, timeSymbol, st, bts, btp)) {
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


//---------------------------------------------------------
//   divisions
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/divisions node.
 */


//---------------------------------------------------------
//   staves
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/attributes/staves node.
 */


//---------------------------------------------------------
//   direction
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction node
 to be able to handle octave-shifts, as these must be interpreted
 in musical order instead of in MusicXML file order.
 */


//---------------------------------------------------------
//   directionType
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/direction/direction-type node.
 */



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

/**
 Parse the /score-partwise/part/measure/note node.
 */

void MusicXMLParserPass1::newNote(const musicxml::note& note, const QString& partId, const Fraction sTime, Fraction& missingPrev, Fraction& dura, Fraction& missingCurr, VoiceOverlapDetector& vod, MxmlTupletStates& tupletStates)
{
    /* TODO
    if (_e.attributes().value("print-spacing") == "no") {
          notePrintSpacingNo(dura);
          return;
    }
     */

    QString type;
    if (note.type()) {
        type = note_type_value_to_string(*note.type()).data();
    }
    QString voice { "1" };
    if (note.voice()) {
        voice = (*note.voice()).data();
    }
    QString instrId; // TODO
    MxmlStartStop tupletStartStop { MxmlStartStop::NONE };

    mxmlNoteDuration mnd(_divs, _logger);
    { // limit scope
        int duration { 0 };
        if (note.duration()) {
            duration = *note.duration();
        }
        Fraction timeModification { 1, 1 };
        if (note.time_modification()) {
            int actual {(*note.time_modification()).actual_notes()};
            int normal {(*note.time_modification()).normal_notes()};
            timeModification.set(normal, actual);
        }
        mnd.setProperties(duration, note.dot().size(), timeModification);
    }

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

    int staff { 1 }; // default
    if (note.staff()) {
        staff = *note.staff();
        _parts[partId].setMaxStaff(staff);
        Part* part = _partMap.value(partId);
        Q_ASSERT(part); // TODO replace
        if (staff <= 0 || staff > part->nstaves())
            _logger->logError(QString("illegal staff '%1'").arg(*note.staff()), &_e);
    }
    --staff;    // convert staff to zero-based

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
    auto errorStr = mnd.checkTiming(type, note.rest(), note.grace());
    dura = mnd.dura();
    if (errorStr != "")
        _logger->logError(errorStr, &_e);

    // don't count chord or grace note duration
    // note that this does not check the MusicXML requirement that notes in a chord
    // cannot have a duration longer than the first note in the chord
    missingPrev.set(0, 1);
    if (note.chord() || note.grace())
        dura.set(0, 1);

    if (!note.chord() && !note.grace()) {
        // do tuplet
        auto timeMod = mnd.timeMod();
        auto& tupletState = tupletStates[voice];
        tupletState.determineTupletAction(mnd.dura(), timeMod, tupletStartStop, mnd.normalType(), missingPrev, missingCurr);
    }

    // store result
    qDebug("dura isValid %d", dura.isValid());
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


//---------------------------------------------------------
//   duration
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/duration node.
 */

void MusicXMLParserPass1::newDuration(const unsigned int duration, Fraction& dura)
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
    //qDebug("duration %s valid %d", qPrintable(dura.print()), dura.isValid());
}

//---------------------------------------------------------
//   forward
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/forward node.
 */

void MusicXMLParserPass1::newForward(const unsigned int duration, Fraction& dura)
{
    newDuration(duration, dura);
}

//---------------------------------------------------------
//   backup
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/backup node.
 */

void MusicXMLParserPass1::newBackup(const unsigned int duration, Fraction& dura)
{
    newDuration(duration, dura);
}

//---------------------------------------------------------
//   timeModification
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/time-modification node.
 */


//---------------------------------------------------------
//   rest
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/rest node.
 */

} // namespace Ms
