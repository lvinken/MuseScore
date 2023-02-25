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

#ifndef __IMPORTMXMLPASS1_H__
#define __IMPORTMXMLPASS1_H__

#include "libmscore/score.h"
#include "importxmlfirstpass.h"
#include "musicxml.hxx"
#include "mxml.h" // for the creditwords and MusicXmlPartGroupList definitions
#include "musicxmlsupport.h"

namespace Ms {

//---------------------------------------------------------
//   PageFormat
//---------------------------------------------------------

struct PageFormat {
      QSizeF size;
      qreal printableWidth;        // _width - left margin - right margin
      qreal evenLeftMargin;        // values in inch
      qreal oddLeftMargin;
      qreal evenTopMargin;
      qreal evenBottomMargin;
      qreal oddTopMargin;
      qreal oddBottomMargin;
      bool twosided;
      };

typedef QMap<QString, Part*> PartMap;
typedef std::map<int,MusicXmlPartGroup*> MusicXmlPartGroupMap;

//---------------------------------------------------------
//   MxmlOctaveShiftDesc
//---------------------------------------------------------

struct MxmlOctaveShiftDesc {
      enum class Type : char { UP, DOWN, STOP, NONE };
      Type tp;
      short size;
      Fraction time;
      short num;
      MxmlOctaveShiftDesc() : tp(Type::NONE), size(0), num(-1) {}
      MxmlOctaveShiftDesc(Type _tp, short _size, Fraction _tm) : tp(_tp), size(_size), time(_tm), num(-1) {}
      };

//---------------------------------------------------------
//   MxmlStartStop (also used in pass 2)
//---------------------------------------------------------

enum class MxmlStartStop : char {
      NONE, START, STOP
      };

enum class MxmlTupletFlag : char {
      NONE = 0,
      STOP_PREVIOUS = 1,
      START_NEW = 2,
      ADD_CHORD = 4,
      STOP_CURRENT = 8
      };

typedef QFlags<MxmlTupletFlag> MxmlTupletFlags;

struct MxmlTupletState {
      void addDurationToTuplet(const Fraction duration, const Fraction timeMod);
      MxmlTupletFlags determineTupletAction(const Fraction noteDuration,
                                            const Fraction timeMod,
                                            const MxmlStartStop tupletStartStop,
                                            const TDuration normalType,
                                            Fraction& missingPreviousDuration,
                                            Fraction& missingCurrentDuration
                                            );
      bool m_inTuplet { false };
      bool m_implicit { false };
      int m_actualNotes { 1 };
      int m_normalNotes { 1 };
      Fraction m_duration { 0, 1 };
      int m_tupletType { 0 }; // smallest note type in the tuplet // TODO_NOW rename ?
      int m_tupletCount { 0 }; // number of smallest notes in the tuplet // TODO_NOW rename ?
      };

using MxmlTupletStates = std::map<QString, MxmlTupletState>;

//---------------------------------------------------------
//   declarations
//---------------------------------------------------------

void determineTupletFractionAndFullDuration(const Fraction duration, Fraction& fraction, Fraction& fullDuration);
Fraction missingTupletDuration(const Fraction duration);


//---------------------------------------------------------
//   MusicXMLParserPass1
//---------------------------------------------------------

class MxmlLogger;

class MusicXMLParserPass1 {
public:
      MusicXMLParserPass1(Score* score, MxmlLogger* logger);
      void initPartState(const QString& partId);
      Score::FileError parse(const musicxml::score_partwise& score_partwise);
      void handleOctaveShift(const Fraction cTime, const QString& type, short size, MxmlOctaveShiftDesc& desc);
      bool determineMeasureLength(QVector<Fraction>& ml) const;
      VoiceList getVoiceList(const QString id) const;
      bool determineStaffMoveVoice(const QString& id, const int mxStaff, const QString& mxVoice,
                                   int& msMove, int& msTrack, int& msVoice) const;
      int trackForPart(const QString& id) const;
      bool hasPart(const QString& id) const;
      Part* getPart(const QString& id) const { return _partMap.value(id); }
      MusicXmlPart getMusicXmlPart(const QString& id) const { return _parts.value(id); }
      MusicXMLInstruments getInstruments(const QString& id) const { return _instruments.value(id); }
      void setDrumsetDefault(const QString& id, const QString& instrId, const NoteHead::Group hg, const int line, const Direction sd);
      MusicXmlInstrList getInstrList(const QString id) const;
      MusicXmlIntervalList getIntervals(const QString id) const;
      Fraction getMeasureStart(const int i) const;
      int octaveShift(const QString& id, const int staff, const Fraction f) const;
      const CreditWordsList& credits() const { return _credits; }
      bool hasBeamingInfo() const { return _hasBeamingInfo; }

private:
      // functions
      void newAttributes(const musicxml::attributes& attributes, const QString& partId, const Fraction cTime);
      void newBackup(const unsigned int duration, Fraction& dura);
      void newDuration(const unsigned int duration, Fraction& dura);
      void newForward(const unsigned int duration, Fraction& dura);
      void newMeasure(const musicxml::measure1& measure, const QString& partId, const Fraction cTime, Fraction& mdur, VoiceOverlapDetector& vod, const int measureNr);
      void newNote(const musicxml::note& note, const QString& partId, const Fraction sTime, Fraction& missingPrev, Fraction& dura, Fraction& missingCurr, VoiceOverlapDetector& vod, MxmlTupletStates& tupletStates);
      void newPart(const musicxml::part& part);
      void newPartList(const musicxml::part_list& part_list /*TODO , MusicXmlPartGroupList& partGroupList */);
      void newScorePart(const musicxml::score_part& score_part);
      void newTime(const musicxml::time& time, const Fraction cTime);

      // generic pass 1 data
      QXmlStreamReader _e; // TODO remove
      int _divs;                                ///< Current MusicXML divisions value
      QMap<QString, MusicXmlPart> _parts;       ///< Parts data, mapped on part id
      std::set<int> _systemStartMeasureNrs;     ///< Measure numbers of measures starting a page
      std::set<int> _pageStartMeasureNrs;       ///< Measure numbers of measures starting a page
      QVector<Fraction> _measureLength;         ///< Length of each measure
      QVector<Fraction> _measureStart;          ///< Start time of each measure
      CreditWordsList _credits;                 ///< All credits collected
      PartMap _partMap;                         ///< TODO merge into MusicXmlPart ??
      QMap<QString, MusicXMLInstruments> _instruments; ///< instruments for each part, mapped on part id
      Score* _score;                            ///< MuseScore score
      MxmlLogger* _logger;                      ///< Error logger
      bool _hasBeamingInfo;                     ///< Whether the score supports or contains beaming info

      // part specific data (TODO: move to part-specific class)
      Fraction _timeSigDura;                    ///< Measure duration according to last timesig read
      QMap<int, MxmlOctaveShiftDesc> _octaveShifts; ///< Pending octave-shifts
      QSize _pageSize;                          ///< Page width read from defaults
      };

} // namespace Ms
#endif
