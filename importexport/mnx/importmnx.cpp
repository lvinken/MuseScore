//=============================================================================
//  MuseScore
//  Linux Music Score Editor
//
//  Copyright (C) 2017-2020 Werner Schweer and others
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

// to enable tracing, uncomment line containing logger.setLoggingLevel

#include <memory>
#include <vector>

#include <QRegExp>

#include "libmscore/box.h"
#include "libmscore/chord.h"
#include "libmscore/dynamic.h"
#include "libmscore/durationtype.h"
#include "libmscore/hairpin.h"
#include "libmscore/keysig.h"
#include "libmscore/lyrics.h"
#include "libmscore/measure.h"
#include "libmscore/ottava.h"
#include "libmscore/part.h"
#include "libmscore/rest.h"
#include "libmscore/slur.h"
#include "libmscore/staff.h"
#include "libmscore/stafftext.h"
#include "libmscore/tempotext.h"
#include "libmscore/timesig.h"
#include "libmscore/tie.h"
#include "libmscore/tuplet.h"
#include "importmxmllogger.h"
#include "importmnx.h"
#include "xmlstreamreader.h"

namespace Ms {
//---------------------------------------------------------
//   Constants
//---------------------------------------------------------

const int MAX_LYRICS       = 16;

//---------------------------------------------------------
//   RepeatDescription
//---------------------------------------------------------

struct RepeatDescription
{
    bool start { false };
    bool end { false };
    int times { 0 };
    RepeatDescription() = default;                                                              // Constructor
    RepeatDescription(const RepeatDescription&) = delete;                                       // Copy constructor
    RepeatDescription(RepeatDescription&&) = default;                                           // Move constructor
    RepeatDescription& operator=(const RepeatDescription&) = delete;                            // Copy assignment operator
    RepeatDescription& operator=(RepeatDescription&&) = default;                                // Move assignment operator
};

//---------------------------------------------------------
//   SlurDescription
//---------------------------------------------------------

struct SlurDescription
{
    std::unique_ptr<Slur> slur_ptr;
    QString target;
    // note that due to the unique_ptr, SlurDescription cannot be copied
    SlurDescription() = default;                                                      // Constructor
    SlurDescription(const SlurDescription&) = delete;                                 // Copy constructor
    SlurDescription(SlurDescription&&) = default;                                     // Move constructor
    SlurDescription& operator=(const SlurDescription&) = delete;                      // Copy assignment operator
    SlurDescription& operator=(SlurDescription&&) = default;                          // Move assignment operator
};

//---------------------------------------------------------
//   SpannerDescription
//---------------------------------------------------------

struct SpannerDescription
{
    std::unique_ptr<Spanner> spanner_ptr;
    QString end;
    // note that due to the unique_ptr, SpannerDescription cannot be copied
    SpannerDescription() = default;                                             // Constructor
    SpannerDescription(const SpannerDescription&) = delete;                     // Copy constructor
    SpannerDescription(SpannerDescription&&) = default;                         // Move constructor
    SpannerDescription& operator=(const SpannerDescription&) = delete;          // Copy assignment operator
    SpannerDescription& operator=(SpannerDescription&&) = default;              // Move assignment operator
};

//---------------------------------------------------------
//   MnxParserGlobal definition
//---------------------------------------------------------

class MnxParserGlobal
{
public:
    MnxParserGlobal(XmlStreamReader& e, Score* score, MxmlLogger* logger);
    QString instruction() const { return _instruction; }
    KeySigEvent keySig(const int measureNr) const;
    int measureNr(const Fraction time) const;
    void parse();
    Fraction startTime(const int measureNr) const;
    float tempoBpm() const { return _tempoBpm; }
    TDuration::DurationType tempoValue() const { return _tempoValue; }
    Fraction timeSig(const int measureNr) const;
private:
    // functions
    void directions(const int measureNr, const Fraction sTime, const int paramStaff = -1);
    void dirgroup(const int measureNr, const Fraction sTime, const int paramStaff = -1);
    void parseInstruction();
    void parseKey(const int measureNr);
    void parseRepeat(const int measureNr);
    void measure(const int measureNr);
    void tempo();
    void time(const int measureNr);

    // data
    XmlStreamReader& _e;
    Score* const _score;                                    ///< MuseScore score TODO: remove if MnxParserGlobal does not create score elements
    MxmlLogger* const _logger;                              ///< Error logger
    QString _instruction;
    int _nrOfMeasures { 0 };
    float _tempoBpm { 0.0 };                                ///< initial tempo beats per minute
    TDuration::DurationType _tempoValue { TDuration::DurationType::V_INVALID };     ///< initial tempo beat value
    std::map<int, KeySigEvent> _keySigs;                    ///< keysig changes (measure based)
    std::map<int, Fraction> _timeSigs;                      ///< timesig changes (measure based)
    std::map<int, RepeatDescription> _repeats;              ///< repeats (measure based)
};

//---------------------------------------------------------
//   MnxParserGlobal constructor
//---------------------------------------------------------

MnxParserGlobal::MnxParserGlobal(XmlStreamReader& e, Score* score, MxmlLogger* logger) :
    _e(e), _score(score), _logger(logger)
{
    // nothing
}

//---------------------------------------------------------
//   TieDescription
//---------------------------------------------------------

struct TieDescription
{
    TieDescription(Note* fromNote, const QString& target) : _fromNote(fromNote), _target(target) {}
    Note* _fromNote;
    QString _target;
};

//---------------------------------------------------------
//   MnxParserPart definition
//---------------------------------------------------------

class MnxParserPart
{
public:
    MnxParserPart(XmlStreamReader& e, Score* score, MxmlLogger* logger, const MnxParserGlobal& global);
    void parsePartAndAppendToScore();

private:
    // functions
    void clef(const int paramStaff);
    void debugDumpData();
    void directions(const Fraction sTime, const int paramStaff = -1);
    void dynamics(const Fraction sTime, const int paramStaff = -1);
    Fraction event(Measure* measure, const Fraction sTime, const int seqNr, Tuplet* tuplet);
    void lyric(ChordRest* cr);
    void measure(const int measureNr);
    std::unique_ptr<Note> note(const int seqNr);
    void octaveShift(const Fraction sTime, const int paramStaff = -1);
    Fraction parseTuplet(Measure* measure, const Fraction sTime, const int track);
    Rest* rest(Measure* measure, const bool measureRest, const QString& value, const int seqNr);
    void sequence(Measure* measure, const Fraction sTime, std::vector<int>& staffSeqCount);
    void slur(ChordRest* cr1);
    int staves();
    QString tied();
    void wedge();
    // data
    XmlStreamReader& _e;
    Part* _part = nullptr;                                  ///< The part (allocated in parsePartAndAppendToScore())
    Score* const _score;                                    ///< MuseScore score
    MxmlLogger* const _logger;                              ///< Error logger
    const MnxParserGlobal& _global;                         ///< Data extracted from the "global" tree
    std::vector<SlurDescription> _slurs;                    ///< Slurs already created with cr2 still unknown
    std::vector<SpannerDescription> _spanners;              ///< Spanners already created with tick2 still unknown
    std::vector<TieDescription> _ties;                      ///< Descriptions of ties to be created and connected
    std::map<QString, Element*> _ids;                       ///< IDs and elements they refer to
};

//---------------------------------------------------------
//   MnxParserPart constructor
//---------------------------------------------------------

MnxParserPart::MnxParserPart(XmlStreamReader& e, Score* score, MxmlLogger* logger, const MnxParserGlobal& global) :
    _e(e), _score(score), _logger(logger), _global(global)
{
    // nothing
}

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
    void creator();
    void head();
    void mnx();
    void mnxCommon();
    Score::FileError parse();
    //void part();
    void rights();
    void score();
    void subtitle();
    void tempo();
    void title();

    // data
    XmlStreamReader _e;
    QString _parseStatus;                             ///< Parse status (typicallay a short error message)
    Score* const _score;                              ///< MuseScore score
    MxmlLogger* const _logger;                        ///< Error logger
    QString _composer;                                ///< metadata: composer
    QString _lyricist;                                ///< metadata: lyricist
    QString _rights;                                  ///< metadata: rights
    QString _subtitle;                                ///< metadata: subtitle
    QString _title;                                   ///< metadata: title
    MnxParserGlobal _global;                          ///< data extracted from the "global" tree
};

//---------------------------------------------------------
//   MnxParser constructor
//---------------------------------------------------------

MnxParser::MnxParser(Score* score, MxmlLogger* logger) :
    _score(score), _logger(logger), _global(_e, _score, _logger)
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
    //logger.setLoggingLevel(MxmlLogger::Level::MXML_TRACE); // also include tracing

    MnxParser p(score, &logger);
    auto res = p.parse(dev);
    if (res != Score::FileError::FILE_NO_ERROR) {
        return res;
    }

    score->setSaved(false);
    score->setCreated(true);
    score->connectTies();
    qDebug("done");
    return Score::FileError::FILE_NO_ERROR;                          // OK
}

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

/**
 Parse MNX in \a device.
 */

Score::FileError MnxParser::parse(QIODevice* device)
{
    _e.setDevice(device);
    Score::FileError res = parse();
    if (res != Score::FileError::FILE_NO_ERROR) {
        return res;
    }

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
    bool found = false;
    while (_e.readNextStartElement()) {
        if (_e.name() == "mnx") {
            found = true;
            mnx();
        } else {
            _logger->logError(QString("this is not an MNX file (top-level node '%1')")
                              .arg(_e.name().toString()));
            _e.handleUnknownChild();
            //return Score::FileError::FILE_BAD_FORMAT;
        }
    }

    const auto errors = _e.errorString();
    if (errors != "") {
        qDebug() << errors;
    }

    const auto& handled = _e.handledElements();
    if (!handled.empty()) {
        qDebug() << "handled elements";
        for (const auto& child : handled) {
            qDebug() << " " << child;
        }
    }

    const auto& ignored = _e.ignoredElements();
    if (!ignored.empty()) {
        qDebug() << "ignored elements";
        for (const auto& child : ignored) {
            qDebug() << " " << child;
        }
    }

    const auto& unknowns = _e.unknownElements();
    if (!unknowns.empty()) {
        qDebug() << "unknown elements";
        for (const auto& child : unknowns) {
            qDebug() << " " << child;
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

static void addKeySig(Score* score, const Fraction tick, const int track, const KeySigEvent key);
static Measure* addMeasure(Score* score, const Fraction tick, const Fraction sig, const int no);
static void addStaffText(Score* score, const Fraction tick, const int track, const QString& text);
static void addTempoText(Score* score, const Fraction tick, const float bpm, const TDuration::DurationType val);
static void addTimeSig(Score* score, const Fraction tick, const int track, const Fraction sig);
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
    auto measure = score->tick2measure(Fraction::fromTicks(tick));
    auto s = measure->getSegment(tick ? SegmentType::Clef : SegmentType::HeaderClef, Fraction::fromTicks(tick));
    s->add(clef);
}

//---------------------------------------------------------
//   addElementToSegmentChordRest
//---------------------------------------------------------

/**
 Add an element to the score in a ChordRest segment.
 */

static void addElementToSegmentChordRest(Score* score, const int tick, const int track, Element* el)
{
    el->setTrack(track);
    auto measure = score->tick2measure(Fraction::fromTicks(tick));
    auto s = measure->getSegment(SegmentType::ChordRest, Fraction::fromTicks(tick));
    s->add(el);
}

//---------------------------------------------------------
//   addMetaData
//---------------------------------------------------------

/**
 Add (part of) the metadata to the score.
 */

static void addMetaData(Score* score, const QString& composer, const QString& lyricist, const QString& rights,
                        const QString& subtitle, const QString& title)
{
    if (!title.isEmpty()) {
        score->setMetaTag("workTitle", title);
    }
    if (!subtitle.isEmpty()) {
        score->setMetaTag("workNumber", subtitle);
    }
    if (!composer.isEmpty()) {
        score->setMetaTag("composer", composer);
    }
    if (!lyricist.isEmpty()) {
        score->setMetaTag("lyricist", lyricist);
    }
    if (!rights.isEmpty()) {
        score->setMetaTag("copyright", rights);
    }
}

//---------------------------------------------------------
//   addVBoxWithMetaData
//---------------------------------------------------------

/**
 Add a vbox containing (part of) the metadata to the score.
 */

static void addVBoxWithMetaData(Score* score, const QString& composer, const QString& lyricist, const QString& subtitle,
                                const QString& title)
{
    auto vbox = new VBox(score);
    if (!composer.isEmpty()) {
        auto text = new Text(score, Tid::COMPOSER);
        text->setPlainText(composer);
        vbox->add(text);
    }
    if (!lyricist.isEmpty()) {
        auto text = new Text(score, Tid::POET);
        text->setPlainText(lyricist);
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
    vbox->setTick(Fraction(0, 1));
    score->measures()->add(vbox);
}

//---------------------------------------------------------
//   addFirstMeasure
//---------------------------------------------------------

/**
 Add the first measure to the score.
 */

static void addFirstTempoAndStaffText(Score* score, const float bpm, TDuration::DurationType val, const QString& instr)
{
    const auto epsilon { 0.001 };
    const auto tick = Fraction { 0, 1 };
    const int track = 0;
    if (bpm > epsilon && val != TDuration::DurationType::V_INVALID) {
        addTempoText(score, tick, bpm, val);
    }
    if (instr != "") {
        addStaffText(score, tick, track, instr);
    }
}

//---------------------------------------------------------
//   addKeySig
//---------------------------------------------------------

/**
 Add a key signature to the score.
 */

static void addKeySig(Score* score, const Fraction tick, const int track, const KeySigEvent key)
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
        qDebug("too much lyrics (>%d)", MAX_LYRICS);           // TODO
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

static Measure* addMeasure(Score* score, const Fraction tick, const Fraction sig, const int no)
{
    auto m = new Measure(score);
    m->setTick(tick);
    m->setTimesig(sig);
    m->setNo(no);
    m->setTicks(sig);
    score->measures()->add(m);
    return m;
}

//---------------------------------------------------------
//   addSlur
//---------------------------------------------------------

/**
 Add a slur to the score between chord1 and chord2.
 Assumes chord1 and chord1 have been initialized (tick and track valid).
 Also assumes slur has been initialized (score and properties).
 */

static void addSlur(Chord* const chord1, Chord* const chord2, Slur* const slur)
{
    slur->setStartElement(chord1);
    slur->setTick(chord1->tick());
    slur->setTrack(chord1->track());
    slur->setEndElement(chord2);
    slur->setTick2(chord2->tick());
    slur->setTrack2(chord2->track());

    auto score = slur->score();
    score->addElement(slur);
}

//---------------------------------------------------------
//   addSpanner
//---------------------------------------------------------

/**
 Add a spanner to the score between tick1 and tick2.
 Assumes spanner has been initialized (score and track).
 */

static void addSpanner(Spanner* const sp, const Fraction tick1, const Fraction tick2)
{
    sp->setTick(tick1);
    sp->setTick2(tick2);
    sp->score()->addElement(sp);
}

//---------------------------------------------------------
//   addStaffText
//---------------------------------------------------------

/**
 Add a staff text to a track.
 */

static void addStaffText(Score* score, const Fraction tick, const int track, const QString& text)
{
    auto t = new StaffText(score);
    t->setXmlText(text);
    t->setPlacement(Placement::ABOVE);
    t->setTrack(track);
    auto measure = score->tick2measure(tick);
    auto s = measure->getSegment(SegmentType::ChordRest, tick);
    s->add(t);
}

//---------------------------------------------------------
//   addTempoText
//---------------------------------------------------------

/**
 Add a tempo text to track 0.
 */

static void addTempoText(Score* score, const Fraction tick, const float bpm, const TDuration::DurationType val)
{
    double tpo = bpm / 60;
    score->setTempo(tick, tpo);

    auto t = new TempoText(score);
    t->setXmlText(QString("%1 = %2").arg(TempoText::duration2tempoTextString(TDuration(val))).arg(bpm));
    t->setTempo(tpo);
    t->setFollowText(true);

    t->setPlacement(Placement::ABOVE);
    t->setTrack(0);            // TempoText must be in track 0
    auto measure = score->tick2measure(tick);
    auto s = measure->getSegment(SegmentType::ChordRest, tick);
    s->add(t);
}

//---------------------------------------------------------
//   addTimeSig
//---------------------------------------------------------

/**
 Add a time signature to a track.
 */

static void addTimeSig(Score* score, const Fraction tick, const int track, const Fraction sig)
{
    auto timesig = new TimeSig(score);
    timesig->setSig(sig);
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

static Part* appendPart(Score* score, const KeySigEvent key, const Fraction sig)
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
        const auto tick = Fraction(0, 1);
        addKeySig(score, tick, track, key);
        addTimeSig(score, tick, track, sig);
    }

    return part;
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
    chord->setTicks(dur.fraction());
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
    rest->setTicks(measure->ticks());
    rest->setTrack(track);
    return rest;
}

//---------------------------------------------------------
//   createDynamic
//---------------------------------------------------------

/*
 * Create a MuseScore Dynamic of the specified MNX type.
 */

static std::unique_ptr<Dynamic> createDynamic(Score* score, const QString& type)
{
    std::unique_ptr<Dynamic> dynamic(new Dynamic(score));
    dynamic->setDynamicType(type);
    return dynamic;
}

//---------------------------------------------------------
//   createNote
//---------------------------------------------------------

/*
 * Create a note with pitch \a pitch in track \a track.
 */

static std::unique_ptr<Note> createNote(Score* score, const QString& pitch, const int track)
{
    std::unique_ptr<Note> note(new Note(score));
    note->setTrack(track);
    auto tpc2 = 0;
    auto msPitch = mnxToMidiPitch(pitch, tpc2);
    //note->setTpcFromPitch();       // TODO
    //int tpc1 = Ms::transposeTpc(tpc2, Interval(), true);
    note->setPitch(msPitch, tpc2, tpc2);
    return note;
}

//---------------------------------------------------------
//   createOttava
//---------------------------------------------------------

/*
 * Create a MuseScore Ottava of the specified MNX type.
 */

static std::unique_ptr<Ottava> createOttava(Score* score, const QString& /* type */)
{
    std::unique_ptr<Ottava> ottava(new Ottava(score));
    ottava->setOttavaType(OttavaType::OTTAVA_8VA);         // TODO
    return ottava;
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
//   createSlur
//---------------------------------------------------------

/*
 * Create a slur.
 */

static std::unique_ptr<Slur> createSlur(Score* score)
{
    std::unique_ptr<Slur> slur(new Slur(score));
    return slur;
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
//   createHairpin
//---------------------------------------------------------

/*
 * Create a hairpin.
 */

static std::unique_ptr<Hairpin> createHairpin(Score* score, const int track)
{
    std::unique_ptr<Hairpin> hairpin(new Hairpin(score));
    hairpin->setTrack(track);
    hairpin->setTrack2(track);
    return hairpin;
}

//---------------------------------------------------------
//   determineTrack
//---------------------------------------------------------

/*
 * Calculate track from \a part, \a staff and \a voice.
 */

static int determineTrack(const Part* const part, const int staff, const int voice = 0)
{
    // make score-relative instead on part-relative
    Q_ASSERT(part);
    Q_ASSERT(part->score());
    Q_ASSERT(staff >= 0);
    auto scoreRelStaff = part->score()->staffIdx(part);   // zero-based number of part's first staff in the score
    auto res = (scoreRelStaff + staff) * VOICES + voice;
    return res;
}

//---------------------------------------------------------
//   findMeasure
//---------------------------------------------------------

/**
 In Score \a score find the measure starting at \a tick.
 */

static Measure* findMeasure(const Score* const score, const Fraction tick)
{
    auto m = score->firstMeasure();
    while (m) {
        if (m && m->tick() == tick) {
            return m;
        }
        m = m->nextMeasure();
    }
    return nullptr;
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
    if (staves > part->nstaves()) {
        part->setStaves(staves);
    }
}

//---------------------------------------------------------
//   type conversions
//---------------------------------------------------------

//---------------------------------------------------------
//   mnxClefToClefType
//---------------------------------------------------------

/**
 * Convert MNX clef type to MuseScore ClefType.
 */

static ClefType mnxClefToClefType(const QString& sign, const QString& line)
{
    ClefType res = ClefType::INVALID;

    if (sign == "G" && line == "2") {
        res = ClefType::G;
    } else if (sign == "F" && line == "4") {
        res = ClefType::F;
    } else {
        qDebug("unknown clef sign: '%s' line: '%s' oct ch=%d>",
               qPrintable(sign), qPrintable(line), 0);
    }

    return res;
}

//---------------------------------------------------------
//   mnxKeyToKeySigEvent
//---------------------------------------------------------

/**
 * Convert MNX key type to MuseScore KeySigEvent.
 */

static KeySigEvent mnxKeyToKeySigEvent(const QString& fifths, const QString& mode)
{
    KeySigEvent res;

    res.setKey(Key(fifths.toInt()));         // TODO: move toInt to caller

    if (mode == "major") {
        res.setMode(KeyMode::MAJOR);
    } else if (mode == "minor") {
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

/**
 * Convert MNX time signature to beats and beat type.
 * TODO: support more time signatures
 */

static Fraction mnxTSigToBtsBtp(const QString& tsig)
{
    if (tsig == "2/4") {
        return Fraction { 2, 4 };
    } else if (tsig == "3/4") {
        return Fraction { 3, 4 };
    } else if (tsig == "4/4") {
        return Fraction { 4, 4 };
    }

    qDebug("mnxTSigToBtsBtp: unknown '%s'", qPrintable(tsig));
    return {};
}

//---------------------------------------------------------
//   mnxValueUnitToDurationType
//---------------------------------------------------------

/**
 * Convert MNX note value unit to MuseScore DurationType.
 */

static TDuration::DurationType mnxValueUnitToDurationType(const QString& s)
{
    if (s == "/4") {
        return TDuration::DurationType::V_QUARTER;
    } else if (s == "/8") {
        return TDuration::DurationType::V_EIGHTH;
    } else if (s == "/1024") {
        return TDuration::DurationType::V_1024TH;
    } else if (s == "/512") {
        return TDuration::DurationType::V_512TH;
    } else if (s == "/256") {
        return TDuration::DurationType::V_256TH;
    } else if (s == "/128") {
        return TDuration::DurationType::V_128TH;
    } else if (s == "/64") {
        return TDuration::DurationType::V_64TH;
    } else if (s == "/32") {
        return TDuration::DurationType::V_32ND;
    } else if (s == "/16") {
        return TDuration::DurationType::V_16TH;
    } else if (s == "/2") {
        return TDuration::DurationType::V_HALF;
    } else if (s == "/1") {
        return TDuration::DurationType::V_WHOLE;
    } else if (s == "*2") {
        return TDuration::DurationType::V_BREVE;
    } else if (s == "*4") {
        return TDuration::DurationType::V_LONG;
    } else {
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
//   mnxMeasurePositionToFraction
//---------------------------------------------------------

/**
 * Convert MNX measure position to Fraction.
 */

static Fraction mnxMeasurePositionToFraction(const QString& value)
{
    const QRegExp noteValueQuantity { "(\\d+)(/\\d+)" };      // TODO: support dots
    Fraction res { 0, 0 };          // invalid

    // parse floating point and note value separately
    if (noteValueQuantity.exactMatch(value)) {
        //qDebug("value '%s' ('%s' '%s') is noteValueQuantity ", qPrintable(value), qPrintable(noteValueQuantity.cap(1)), qPrintable(noteValueQuantity.cap(2)));
        const auto val = mnxValueUnitToDurationType(noteValueQuantity.cap(2));
        if (val != TDuration::DurationType::V_INVALID) {
            TDuration dur { val };
            res = noteValueQuantity.cap(1).toInt() * dur.ticks();
        }
    } else {
        qDebug("unknown value '%s'", qPrintable(value));
        // TODO support floating point
    }

    //qDebug("res '%s'", qPrintable(res.print()));
    return res;
}

//---------------------------------------------------------
//   mnxMeasureLocationToTick
//---------------------------------------------------------

/**
 Convert MNX measure location to tick in Fraction.
 Does not (yet) support real data.
 Possible content:
 - position in the current measure: floating point (e.g. 0.75) or note value quantity (e.g. 3/4)
 - position in an arbitrary measure: measure index, ":" and floating point (e.g. 1:0.75) or note value quantity (e.g. 1:3/4)
 - an element location: "#" and the element's XMLID (e.g. #p1m3n21)
 */

static Fraction mnxMeasureLocationToTick(const QString& location, const MnxParserGlobal& global)
{
    const QRegExp elementLocation { "#\\S+" };      // TODO: check if this matches all XML ID legal characters
    const QRegExp arbitraryMeasurePosition { "(\\d+):(\\S+)" };

    if (elementLocation.exactMatch(location)) {
        //qDebug("loc '%s' is elementLocation", qPrintable(location));
    } else if (arbitraryMeasurePosition.exactMatch(location)) {
        //qDebug("loc '%s' is arbitraryMeasurePosition", qPrintable(location));
        const auto positionInMeasure = mnxMeasurePositionToFraction(arbitraryMeasurePosition.cap(2));
        const auto startOfMeasure = global.startTime(arbitraryMeasurePosition.cap(1).toInt() - 1);
        const auto res = startOfMeasure + positionInMeasure;
        //qDebug("res '%s'", qPrintable(res.print()));
        return res;
    } else {
        //qDebug("loc '%s' is currentMeasurePosition", qPrintable(location));
        //return mnxMeasurePositionToFraction(location); // TODO: interpret as measure-relative
    }

    qDebug("unknown value '%s'", qPrintable(location));
    return { 0, 0 };         // force invalid result
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
    if (ok) {
        return table[step] + alt + (oct + 1) * 12;
    }

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
        actualNotes = 3;
        normalNotes = 2;
        td = TDuration::DurationType::V_QUARTER;
    } else if (actual == "3/8" && normal == "2/8") {
        actualNotes = 3;
        normalNotes = 2;
        td = TDuration::DurationType::V_EIGHTH;
    } else {
        qDebug("invalid actual '%s' and/or normal '%s'", qPrintable(actual), qPrintable(normal));
        return;
    }

    tuplet->setRatio(Fraction(actualNotes, normalNotes));
    tuplet->setBaseLen(td);
}

//---------------------------------------------------------
//   setTupletTicks
//---------------------------------------------------------

static void setTupletTicks(Tuplet* tuplet)
{
    Q_ASSERT(tuplet);

    tuplet->setTicks(tuplet->elementsDuration() / tuplet->ratio());
}

//---------------------------------------------------------
//   parser: support functions
//---------------------------------------------------------

// read staff attribute, return 0-based staff number

static int readStaff(XmlStreamReader& e, MxmlLogger* const logger, bool& ok)
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
        //logger->logError(QString("invalid staff '%1'").arg(attrStaff), &e);
    }
    return staff;
}

//---------------------------------------------------------
//   parsers: node handlers
//---------------------------------------------------------

//---------------------------------------------------------
//   MnxParserGlobal
//---------------------------------------------------------

//---------------------------------------------------------
//   directions
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/global/measure/directions node.
 */

void MnxParserGlobal::directions(const int measureNr, const Fraction sTime, const int paramStaff)
{
    while (_e.readNextStartElement()) {
        if (_e.name() == "dirgroup") {
            dirgroup(measureNr, sTime, paramStaff);
        } else if (_e.name() == "instruction") {
            parseInstruction();
        } else if (_e.name() == "key") {
            parseKey(measureNr);
        } else if (_e.name() == "repeat") {
            parseRepeat(measureNr);
        } else if (_e.name() == "tempo") {
            tempo();
        } else if (_e.name() == "time") {
            time(measureNr);
        } else {
            _e.handleUnknownChild();
        }
    }
}

//---------------------------------------------------------
//   dirgroup
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/global/measure/directions/dirgroup node.
 TODO: minimize duplicate code with MnxParserGlobal::directions().
 */

void MnxParserGlobal::dirgroup(const int measureNr, const Fraction sTime, const int paramStaff)
{
    while (_e.readNextStartElement()) {
        if (_e.name() == "instruction") {
            parseInstruction();
        } else if (_e.name() == "key") {
            parseKey(measureNr);
        } else if (_e.name() == "tempo") {
            tempo();
        } else if (_e.name() == "time") {
            time(measureNr);
        } else {
            _e.handleUnknownChild();
        }
    }
}

//---------------------------------------------------------
//   keySig
//---------------------------------------------------------

/**
 Find the key signature for measure number measureNr.
 */

KeySigEvent MnxParserGlobal::keySig(const int measureNr) const
{
    const auto it = _keySigs.find(measureNr);

    if (it != _keySigs.end()) {
        return it->second;
    }

    return { };         // invalid
}

//---------------------------------------------------------
//   parseInstruction
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/global/measure/directions/instruction node.
 */

void MnxParserGlobal::parseInstruction()
{
    auto text = _e.handleElementText();
    _logger->logDebugTrace(QString("instruction '%1'").arg(text));
    _instruction = text;
}

//---------------------------------------------------------
//   key
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/global/measure/directions/key node.
 */

void MnxParserGlobal::parseKey(const int measureNr)
{
    auto mode = _e.attributes().value("mode").toString();
    auto fifths = _e.attributes().value("fifths").toString();
    _logger->logDebugTrace(QString("key-sig '%1' '%2'").arg(fifths).arg(mode));
    _e.handleEmptyElement();

    _keySigs[measureNr] = mnxKeyToKeySigEvent(fifths, mode);
}

//---------------------------------------------------------
//   parseRepeat
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/global/measure/directions/repeat node.
 */

void MnxParserGlobal::parseRepeat(const int measureNr)
{
    const auto times = _e.attributes().value("times").toInt();
    const auto type = _e.attributes().value("type");
    _logger->logDebugTrace(QString("repeat type '%1' times '%2'").arg(type.toString()).arg(times));
    _e.handleEmptyElement();

    if (type == "start") {
        _repeats[measureNr].start = true;
    } else if (type == "end") {
        _repeats[measureNr].end = true;
        _repeats[measureNr].times = times > 0 ? times : 2;
    }
}

//---------------------------------------------------------
//   measure
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/global/measure node.
 */

void MnxParserGlobal::measure(const int measureNr)
{
    auto startTick = startTime(measureNr);

    while (_e.readNextStartElement()) {
        if (_e.name() == "directions") {
            directions(measureNr, startTick);
        } else {
            _e.handleUnknownChild();
        }
    }
}

//---------------------------------------------------------
//   measureNr
//---------------------------------------------------------

int MnxParserGlobal::measureNr(const Fraction time) const
{
    for (int i = 0; i < _nrOfMeasures; ++i) {
        if (time == startTime(i)) {
            return i;
        }
    }
    return -1;
}

//---------------------------------------------------------
//   parse
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/global node.
 */

void MnxParserGlobal::parse()
{
    while (_e.readNextStartElement()) {
        if (_e.name() == "measure") {
            measure(_nrOfMeasures);
            _nrOfMeasures++;
        } else {
            _e.handleUnknownChild();
        }
    }

    Fraction cTime { 0, 1 };
    Fraction timeSig { 1, 1 };
    for (int i = 0; i < _nrOfMeasures; ++i) {
        const auto it = _timeSigs.find(i);
        if (it != _timeSigs.end()) {
            timeSig = it->second;
        }
        Measure* const m = addMeasure(_score, cTime, timeSig, i + 1);
        if (_repeats.count(i)) {
            const auto& repeat = _repeats.at(i);
            _logger->logDebugTrace(QString("measure index %1 repeat start %2 end %3 times %4")
                                   .arg(i).arg(repeat.start).arg(repeat.end).arg(repeat.times));
            m->setRepeatStart(repeat.start);
            m->setRepeatEnd(repeat.end);
            m->setRepeatCount(repeat.times);
        }
        cTime += timeSig;
    }
}

//---------------------------------------------------------
//   startTime
//---------------------------------------------------------

/**
 Calculate the start time for measure number measureNr.
 */

Fraction MnxParserGlobal::startTime(const int measureNr) const
{
    Fraction cTime { 0, 1 };
    Fraction timeSig { 1, 1 };      // default in case no time signature found
    for (int i = 0; i < measureNr && i < _nrOfMeasures; ++i) {
        const auto it = _timeSigs.find(i);
        if (it != _timeSigs.end()) {
            timeSig = it->second;
        }
        cTime += timeSig;
    }
    return cTime;
}

//---------------------------------------------------------
//   tempo
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/global/measure/directions/tempo node.
 */

void MnxParserGlobal::tempo()
{
    const auto bpm = _e.attributes().value("bpm").toString();
    const auto value = _e.attributes().value("value").toString();
    _logger->logDebugTrace(QString("tempo bpm '%1' value '%2'").arg(bpm).arg(value));
    _e.handleEmptyElement();

    bool ok = false;
    _tempoBpm = bpm.toFloat(&ok);
    if (!ok) {
        _tempoBpm = -1;
    }
    _tempoValue = mnxValueUnitToDurationType(value);
}

//---------------------------------------------------------
//   time
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/global/measure/directions/time node.
 */

void MnxParserGlobal::time(const int measureNr)
{
    QString signature = _e.attributes().value("signature").toString();
    _logger->logDebugTrace(QString("measure %1 time-sig '%2'").arg(measureNr).arg(signature));
    _e.handleEmptyElement();

    _timeSigs[measureNr] = mnxTSigToBtsBtp(signature);
}

//---------------------------------------------------------
//   timeSig
//---------------------------------------------------------

/**
 Find the time signature for measure number measureNr.
 */

Fraction MnxParserGlobal::timeSig(const int measureNr) const
{
    const auto it = _timeSigs.find(measureNr);

    if (it != _timeSigs.end()) {
        return it->second;
    }

    return { 0, 0 };   // invalid
}

//---------------------------------------------------------
//   MnxParserPart
//---------------------------------------------------------

//---------------------------------------------------------
//   debugDumpData
//---------------------------------------------------------

void MnxParserPart::debugDumpData()
{
    // dump ids read
    /*
    qDebug("ids");
    for (const auto& kv : _ids)
          qDebug("id '%s' element %p", qPrintable(kv.first), kv.second);
    */

    // dump slurs read
    /*
    qDebug("slurs");
    for (const auto& sd : _slurs)
          qDebug("sp %p end '%s'", sd.slur_ptr.get(), qPrintable(sd.target));
    */

    // dump spanners read
    /*
    qDebug("spanners");
    for (const auto& sd : _spanners)
          qDebug("sp %p end '%s'", sd.sp.get(), qPrintable(sd.end));
    */

    // dump ties read
    /*
    qDebug("ties");
    for (const auto& tieDesc : _ties)
          qDebug("note %p target '%s'", tieDesc._fromNote, qPrintable(tieDesc._target));
    */
}

//---------------------------------------------------------
//   clef
//---------------------------------------------------------

/**
 Parse the clef node.
 The staff number may be:
 - implicit (single-staff parts)
 - specified in the staff attribute (typically when clef is in a directions in a measure)
 - specified in the enclosing seqence's staff attribute
   (when clef is in a directions in a sequence in a measure)
 */

void MnxParserPart::clef(const int paramStaff)
{
    Q_ASSERT(_e.isStartElement() && _e.name() == "clef");
    _logger->logDebugTrace("MnxParserPart::clef");

    auto sign = _e.attributes().value("sign").toString();
    auto line = _e.attributes().value("line").toString();

    bool ok { true };
    auto attributeStaff = readStaff(_e, _logger, ok);
    int staff { 0 };
    if (paramStaff >= 0) {
        staff = paramStaff;
    } else if (ok) {
        staff = attributeStaff;
    }
    qDebug("paramStaff %d attributeStaff %d (ok %d) -> staff %d",
           paramStaff, attributeStaff, ok, staff);

    _logger->logDebugTrace(QString("clef sign '%1' line '%2' staff '%3'").arg(sign).arg(line).arg(staff));

    if (ok) {
        auto ct = mnxClefToClefType(sign, line);

        if (ct != ClefType::INVALID) {
            const int tick = 0;       // TODO
            auto track = determineTrack(_part, 0, 0);
            addClef(_score, tick, track + staff * VOICES, ct);
        }
    }

    _e.handleEmptyElement();

    Q_ASSERT(_e.isEndElement() && _e.name() == "clef");
}

//---------------------------------------------------------
//   directions
//---------------------------------------------------------

/**
 Parse the directions node, which may be found:
 - within a measure in a part
 - within a sequence in a measure in a part
 */

void MnxParserPart::directions(const Fraction sTime, const int paramStaff)
{
    while (_e.readNextStartElement()) {
        if (_e.name() == "clef") {
            clef(paramStaff);
        } else if (_e.name() == "dynamics") {
            dynamics(sTime, paramStaff);
        } else if (_e.name() == "octave-shift") {
            octaveShift(sTime, paramStaff);
        } else if (_e.name() == "staves") {
            // TODO: factor out
            auto oldStaves = _part->nstaves();
            auto newStaves = staves();
            setStavesForPart(_part, newStaves);
            for (auto i = oldStaves; i < newStaves; ++i) {
                auto voice = 0;
                auto track = determineTrack(_part, i, voice);

                if (i > 0) {
                    // TODO: check if this works if measureNr does not contain a key or time signature change
                    const auto measureNr = _global.measureNr(sTime);
                    const auto ksig = _global.keySig(measureNr);
                    addKeySig(_score, sTime, track, ksig);
                    const auto tsig = _global.timeSig(measureNr);
                    addTimeSig(_score, sTime, track, tsig);
                }
            }
        } else if (_e.name() == "tempo") {
            _e.handleIgnoredChild();
        } else if (_e.name() == "wedge") {
            wedge();
        } else {
            _e.handleUnknownChild();
        }
    }
}

//---------------------------------------------------------
//   dynamics
//---------------------------------------------------------

/**
 Parse the dynamics node, which may be found:
 - within a sequence in a measure in a part
 - within a directions in a sequence in a measure in a part
 */

void MnxParserPart::dynamics(const Fraction sTime, const int paramStaff)
{
    auto type = _e.attributes().value("type").toString();

    _logger->logDebugTrace(QString("dynamics type '%1' paramStaff '%2'").arg(type).arg(paramStaff));

    _e.handleEmptyElement();

    auto track = determineTrack(_part, 0, 0);
    auto dyn = createDynamic(_score, type);
    addElementToSegmentChordRest(_score, sTime.ticks(), track + paramStaff * VOICES, dyn.release());
}

//---------------------------------------------------------
//   event
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure/sequence/event node.
 */

Fraction MnxParserPart::event(Measure* measure, const Fraction sTime, const int seqNr, Tuplet* tuplet)
{
    auto attrId = _e.attributes().value("id").toString();
    auto attrMeasure = _e.attributes().value("measure").toString();
    bool measureRest = attrMeasure == "yes";
    auto attrValue = _e.attributes().value("value").toString();
    _logger->logDebugTrace(QString("event measure '%1' value '%2'").arg(attrMeasure).arg(attrValue));

    ChordRest* cr = nullptr;

    while (_e.readNextStartElement()) {
        if (_e.name() == "lyric") {
            lyric(cr);
        } else if (_e.name() == "note") {
            if (!cr) {
                cr = createChord(_score, attrValue, seqNr);
            }
            if (cr->isChord()) {
                cr->add(note(seqNr).release());
            } else if (cr->isRest()) {
                _logger->logError("cannot add note to rest");
                _e.handleIgnoredChild(); // TODO check
            }
        } else if (_e.name() == "rest") {
            if (!cr) {
                cr = rest(measure, measureRest, attrValue, seqNr);
            } else {
                if (cr->isChord()) {
                    _logger->logError("cannot add rest to chord");
                } else if (cr->isRest()) {
                    _logger->logError("cannot add rest to rest");
                }
                _e.handleIgnoredChild();   // TODO check
            }
        } else if (_e.name() == "slur") {
            slur(cr);
        } else {
            _e.handleUnknownChild();
        }
    }

    // store chordrest created plus id for later use
    if (attrId != "") {
        _ids.insert({ attrId, cr });                    // TODO: check for duplicate ID
    }
    auto s = measure->getSegment(SegmentType::ChordRest, sTime);
    s->add(cr);

    if (tuplet) {
        cr->setTuplet(tuplet);
        tuplet->add(cr);
    }

    return cr->actualTicks();
}

//---------------------------------------------------------
//   lyric
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure/sequence/event/lyric node.
 */

void MnxParserPart::lyric(ChordRest* cr)
{
    while (_e.readNextStartElement()) {
        if (_e.name() == "syllabic") {
            _e.handleIgnoredChild();
        } else if (_e.name() == "text") {
            auto lyricText = _e.handleElementText();
            addLyric(cr, 0, lyricText);
        } else {
            _e.handleUnknownChild();
        }
    }
}

//---------------------------------------------------------
//   measure
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure node.
 */

void MnxParserPart::measure(const int measureNr)
{
    Measure* currMeasure = nullptr;
    const auto startTick = _global.startTime(measureNr);

    if (_score->staffIdx(_part) == 0) {
        // for first part only: create key and time signatures
        // note: adding these requires at least one staff
        const int track = 0;
        const auto tsig = _global.timeSig(measureNr);
        if (tsig.isValid()) {
            addTimeSig(_score, startTick, track, tsig);
        }
        const auto ksig = _global.keySig(measureNr);
        if (ksig.isValid()) {
            addKeySig(_score, startTick, track, ksig);
        }
        if (!measureNr) {
            addFirstTempoAndStaffText(_score, _global.tempoBpm(), _global.tempoValue(), _global.instruction());
        }
    }

    // for the other parts, just find the measure
    // note: currently only supporting profile standard, which implies the following contraint:
    // "The measure content in all global or part elements consists of an identical number of measures."
    currMeasure = findMeasure(_score, startTick);
    if (!currMeasure) {
        _logger->logError(QString("measure at tick %1 not found!").arg(qPrintable(startTick.print())));
        //skipLogCurrElem(); TODO check
    }

    std::vector<int> staffSeqCount(MAX_STAVES, 0);         // sequence count per staff

    while (_e.readNextStartElement()) {
        if (_e.name() == "directions") {
            directions(startTick);
        } else if (_e.name() == "sequence") {
            sequence(currMeasure, startTick, staffSeqCount);
        } else {
            _e.handleUnknownChild();
        }
    }
}

//---------------------------------------------------------
//   note
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure/sequence/event/note node.
 */

// TODO: handle accidental (value is currently ignored)

std::unique_ptr<Note> MnxParserPart::note(const int seqNr)
{
    auto accidental = _e.attributes().value("accidental").toString();
    auto id = _e.attributes().value("id").toString();
    auto pitch = _e.attributes().value("pitch").toString();
    QString tiedTarget;

    while (_e.readNextStartElement()) {
        if (_e.name() == "tied") {
            tiedTarget = tied();
        } else {
            _e.handleUnknownChild();
        }
    }

    _logger->logDebugTrace(QString("- note pitch '%1' accidental '%2' id '%3' tiedTarget '%4'")
                           .arg(pitch).arg(accidental).arg(id).arg(tiedTarget));

    auto note = createNote(_score, pitch, seqNr /*, accidental*/);
    // store note created plus id for later use
    if (id != "") {
        _ids.insert({ id, note.get() });              // TODO: check for duplicate ID
    }
    // store note created plus tied target for later use
    if (tiedTarget != "") {
        _ties.emplace_back(note.get(), tiedTarget);
    }
    return note;
}

//---------------------------------------------------------
//   octaveShift
//---------------------------------------------------------

/**
 Parse the octave-shift node, which may be found:
 - TBD (?)
 - within a sequence in a measure in a part
 */

void MnxParserPart::octaveShift(const Fraction sTime, const int paramStaff)
{
    auto type = _e.attributes().value("type").toString();
    // note: end may contain a reference to a specific, future, element (unknown here)
    auto end = _e.attributes().value("end").toString();
    _logger->logDebugTrace(QString("- octave-shift type '%1' end '%2'").arg(type).arg(end));

    auto sp = createOttava(_score, type);
    sp->setTick(sTime);
    sp->setTrack(determineTrack(_part, paramStaff));
    SpannerDescription sd { std::move(sp), end };
    _spanners.push_back(std::move(sd));

    _e.handleEmptyElement();
}

//---------------------------------------------------------
//   updateNotePitchForOttava
//---------------------------------------------------------

/**
 Add pitchDelta to all notes between firstTick and lastTick in the staff starting at startTrack.
 Both ticks are included.
 */

static void updateNotePitchForOttava(Score* score, const int startTrack, const Fraction firstTick,
                                     const Fraction lastTick, const int pitchDelta)
{
    auto firstseg = score->tick2segment(firstTick, true, SegmentType::ChordRest);
    if (!firstseg) {
        return;
    }

    for (auto seg = firstseg; seg && seg->tick() < lastTick; seg = seg->next1(SegmentType::ChordRest)) {
        for (int track = startTrack; track < startTrack + VOICES; ++track) {
            auto elem = seg->element(track);
            if (elem && elem->isChord()) {
                auto chord = toChord(elem);
                for (auto note : chord->notes()) {
                    note->setPitch(note->pitch() - pitchDelta);
                }
            }
        }
    }
}

//---------------------------------------------------------
//   findLastChordForSpanner
//---------------------------------------------------------

/**
 Find chord in staff at tick.
 */

static Chord* findLastChordForSpanner(Score* score, const int staff, const Fraction tick)
{
    auto seg = score->tick2segment(tick, true, SegmentType::ChordRest);

    if (seg) {
        const int startTrack = staff * VOICES;
        for (int track = startTrack; track < (startTrack + VOICES); ++track) {
            auto elem = seg->element(track);
            if (elem && elem->isChord()) {
                auto chord = toChord(elem);
                return chord;
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   parsePartAndAppendToScore
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part node and append the part to the score.
 */

void MnxParserPart::parsePartAndAppendToScore()
{
    auto measureNr = 0;
    const auto ksig = _global.keySig(measureNr);            // TODO: when to check for valid ksig ?
    const auto tsig = _global.timeSig(measureNr);           // TODO: when to check for valid tsig ?
    _part = appendPart(_score, ksig, tsig);

    while (_e.readNextStartElement()) {
        if (_e.name() == "measure") {
            measure(measureNr);
            measureNr++;
        } else if (_e.name() == "part-name") {
            auto partName = _e.handleElementText();
            _logger->logDebugTrace(QString("part-name '%1'").arg(partName));
            _part->setPlainLongName(partName);
            _part->setPartName(partName);
        } else {
            _e.handleUnknownChild();
        }
    }

    debugDumpData();

    // add slurs read to score
    for (auto& sd : _slurs) {
        const auto result = _ids.find(sd.target);
        if (result == _ids.end()) {
            _logger->logError(QString("slur end '%1' not found").arg(sd.target));
        } else {
            Element* const elem = result->second;
            if (!(elem->isChord())) {
                _logger->logError(QString("slur end '%1' is not a chord").arg(sd.target));
            } else {
                auto sp = sd.slur_ptr.release();
                Chord* const chord1 = toChord(sp->startElement());                   // TODO: type check required ?
                Chord* const chord2 = toChord(elem);
                addSlur(chord1, chord2, sp);
            }
        }
    }

    // add spanners read to score
    for (auto& sd : _spanners) {
        auto tick2 = mnxMeasureLocationToTick(sd.end, _global);
        if (tick2.isValid()) {
            auto sp = sd.spanner_ptr.release();
            // tick2 is the start tick of the last note affected, MuseScore needs the end tick
            auto lastChord = findLastChordForSpanner(_score, sp->track() / VOICES, tick2);
            if (lastChord) {
                tick2 += lastChord->ticks();
            } else {
                _logger->logError(QString("chord not found for spanner end '%1'").arg(sd.end));
            }
            sp->setTick2(tick2);
            updateNotePitchForOttava(_score, sp->track(), sp->tick(), tick2, 12 /* TODO */);
            _score->addElement(sp);
        } else {
            _logger->logError(QString("invalid spanner end '%1'").arg(sd.end));
        }
    }

    // connect ties
    for (const auto& tieDesc : _ties) {
        auto note = tieDesc._fromNote;
        auto tie = new Tie(note->score());
        note->setTieFor(tie);
        tie->setStartNote(note);
        tie->setTrack(note->track());
    }

    // set end barline to normal
    if (_score->measures()->size() > 1) {         // there is always a vbox ...
        const auto lastMeas = _score->lastMeasure();
        const int voice = 0;
        for (int staff = 0; staff < _part->nstaves(); ++staff) {
            const auto track = determineTrack(_part, staff, voice);
            lastMeas->setEndBarLineType(BarLineType::NORMAL, track);
        }
    }
}

//---------------------------------------------------------
//   rest
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure/sequence/event/rest node.
 */

Rest* MnxParserPart::rest(Measure* measure, const bool measureRest, const QString& value, const int seqNr)
{
    _logger->logDebugTrace(QString("- rest"));

    _e.handleEmptyElement();

    return measureRest
           ? createCompleteMeasureRest(measure, seqNr)
           : createRest(measure->score(), value, seqNr);
}

//---------------------------------------------------------
//   sequence
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure/sequence node.
 */

void MnxParserPart::sequence(Measure* measure, const Fraction sTime, std::vector<int>& staffSeqCount)
{
    bool ok = false;
    auto staff = readStaff(_e, _logger, ok);

    if (!ok) {
        _e.handleIgnoredChild(); // TODO check
    } else {
        Fraction seqTime(0, 1);           // time in this sequence
        auto track = determineTrack(_part, staff, staffSeqCount.at(staff));

        while (_e.readNextStartElement()) {
            if (_e.name() == "directions") {
                directions(sTime + seqTime, staff);
            } else if (_e.name() == "dynamics") {
                dynamics(sTime + seqTime, staff);
            } else if (_e.name() == "event") {
                seqTime += event(measure, sTime + seqTime, track, nullptr);
            } else if (_e.name() == "tuplet") {
                seqTime += parseTuplet(measure, sTime + seqTime, track);
            } else {
                _e.handleUnknownChild();
            }
        }
        staffSeqCount.at(staff)++;
    }
}

//---------------------------------------------------------
//   slur
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/.../slur node.
 */

void MnxParserPart::slur(ChordRest* cr1)
{
    const auto endNote = _e.attributes().value("end-note").toString();
    const auto location = _e.attributes().value("location").toString();
    const auto startNote = _e.attributes().value("start-note").toString();
    const auto target = _e.attributes().value("target").toString();
    qDebug("slur target '%s'", qPrintable(target));

    if (!cr1) {
        _logger->logError("no cr for slur");
    } else if (!(cr1->isChord())) {
        _logger->logError("no chord for slur");
    } else if (target == "") {
        _logger->logError("no target for slur");
    } else if ((endNote != "") || (startNote != "")) {
        _logger->logError("start or end note not supported for slur");
    } else if (location != "") {
        _logger->logError("location note not supported for slur");
    } else {
        auto slur = createSlur(_score);
        slur->setStartElement(toChord(cr1));
        SlurDescription slurdesc { std::move(slur), target };
        _slurs.push_back(std::move(slurdesc));
    }

    _e.handleEmptyElement();
}

//---------------------------------------------------------
//   staves
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure/directions/staves node.
 */

/*
 * File FaurReveSample-common.xml in commit 02ab667 of 26 June 2018
 * uses "index" instead of "number" for the number of staves, while
 * the 28 August 2018 spec still refers to "number".
 * For now, accept either.
 */

int MnxParserPart::staves()
{
    auto number = _e.attributes().value("number").toString();
    auto index = _e.attributes().value("index").toString();

    _e.handleEmptyElement();

    bool ok = false;
    auto res = number.toInt(&ok);
    if (!ok) {
        res = index.toInt(&ok);
    }
    if (!ok) {
        _logger->logError(QString("invalid number (and index) of staves '%1' '%2'")
                          .arg(number)
                          .arg(index));
    }

    return res;
}

//---------------------------------------------------------
//   tied
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/.../tied node.
 */

QString MnxParserPart::tied()
{
    auto target = _e.attributes().value("target").toString();

    _e.handleEmptyElement();

    return target;
}

//---------------------------------------------------------
//   tuplet
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/part/measure/sequence/tuplet node.
 */

Fraction MnxParserPart::parseTuplet(Measure* measure, const Fraction sTime, const int track)
{
    auto actual = _e.attributes().value("actual").toString();
    auto normal = _e.attributes().value("normal").toString();
    _logger->logDebugTrace(QString("tuplet actual '%1' normal '%2'").arg(actual).arg(normal));

    Fraction tupTime(0, 1);               // time in this tuplet

    // create the tuplet
    auto tuplet = createTuplet(measure, track);
    tuplet->setParent(measure);
    setTupletParameters(tuplet, actual, normal);

    while (_e.readNextStartElement()) {
        if (_e.name() == "event") {
            tupTime += event(measure, sTime + tupTime, track, tuplet);
        } else {
            _e.handleUnknownChild();
        }
    }

    setTupletTicks(tuplet);

    return tupTime;
}

//---------------------------------------------------------
//   wedge
//---------------------------------------------------------

/**
 Parse the /mnx/score/cwmnx/.../wedge node.
 */

void MnxParserPart::wedge()
{
    auto end = _e.attributes().value("end").toString();
    auto location = _e.attributes().value("location").toString();
    auto start = _e.attributes().value("start").toString();
    auto type = _e.attributes().value("type").toString();
    qDebug("wedge end '%s' location '%s' start '%s' type '%s'",
           qPrintable(end),
           qPrintable(location),
           qPrintable(start),
           qPrintable(type)
           );

    _e.handleEmptyElement();
}

//---------------------------------------------------------
//   MnxParser
//---------------------------------------------------------

//---------------------------------------------------------
//   creator
//---------------------------------------------------------

/**
 Parse the /mnx/head/creator node.
 */

void MnxParser::creator()
{
    //Q_ASSERT(_e.isStartElement() && _e.name() == "creator");
    //_logger->logDebugTrace("MnxParser::creator");

    auto creatorType = _e.attributes().value("type").toString();
    auto creatorValue = _e.handleElementText();
    _logger->logDebugTrace(QString("creator '%1' '%2'").arg(creatorType).arg(creatorValue));

    if (!creatorValue.isEmpty()) {
        if (creatorType == "composer") {
            _composer = creatorValue;
        } else if (creatorType == "lyricist") {
            _lyricist = creatorValue;
        }
    }

    //Q_ASSERT(_e.isEndElement() && _e.name() == "creator");
}

//---------------------------------------------------------
//   head
//---------------------------------------------------------

/**
 Parse the /mnx/head node.
 */

void MnxParser::head()
{
    //Q_ASSERT(_e.isStartElement() && _e.name() == "head");
    //_logger->logDebugTrace("MnxParser::head");

    while (_e.readNextStartElement()) {
        if (_e.name() == "creator") {
            creator();
        } else if (_e.name() == "rights") {
            rights();
        } else if (_e.name() == "subtitle") {
            subtitle();
        } else if (_e.name() == "title") {
            title();
        } else {
            _e.handleUnknownChild();
        }
    }

    addVBoxWithMetaData(_score, _composer, _lyricist, _subtitle, _title);
    addMetaData(_score, _composer, _lyricist, _rights, _subtitle, _title);

    //Q_ASSERT(_e.isEndElement() && _e.name() == "head");
}

//---------------------------------------------------------
//   mnx
//---------------------------------------------------------

/**
 Parse the /mnx node.
 */

void MnxParser::mnx()
{
    bool hasHead = false;
    while (_e.readNextStartElement()) {
        if (_e.name() == "head") {
            head();
            hasHead = true;
        } else if (_e.name() == "score") {
            // make sure a VBox is always present
            if (!hasHead) {
                addVBoxWithMetaData(_score, "", "", "", "");
                hasHead = true;
            }
            score();
        } else {
            _e.handleUnknownChild();
        }
    }
}

//---------------------------------------------------------
//   mnxCommon
//---------------------------------------------------------

/**
 Parse the /mnx/score/mnx-common node.
 */

void MnxParser::mnxCommon()
{
    while (_e.readNextStartElement()) {
        if (_e.name() == "global") {
            _global.parse();
        } else if (_e.name() == "part") {
            MnxParserPart part(_e, _score, _logger, _global);
            part.parsePartAndAppendToScore();
        } else {
            _e.handleUnknownChild();
        }
    }
}

//---------------------------------------------------------
//   rights
//---------------------------------------------------------

/**
 Parse the /mnx/head/rights node.
 */

void MnxParser::rights()
{
    auto rightsValue = _e.handleElementText();
    _logger->logDebugTrace(QString("rights '%1'").arg(rightsValue));

    if (!rightsValue.isEmpty()) {
        _rights = rightsValue;
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
    while (_e.readNextStartElement()) {
        if (_e.name() == "mnx-common") {
            mnxCommon();
        } else {
            _e.handleUnknownChild();
        }
    }
}

//---------------------------------------------------------
//   subtitle
//---------------------------------------------------------

/**
 Parse the /mnx/head/subtitle node.
 */

void MnxParser::subtitle()
{
    _subtitle = _e.handleElementText();
    _logger->logDebugTrace(QString("subtitle '%1'").arg(_title));
}

//---------------------------------------------------------
//   title
//---------------------------------------------------------

/**
 Parse the /mnx/head/title node.
 */

void MnxParser::title()
{
    _title = _e.handleElementText();
    _logger->logDebugTrace(QString("title '%1'").arg(_title));
}
} // namespace Ms
