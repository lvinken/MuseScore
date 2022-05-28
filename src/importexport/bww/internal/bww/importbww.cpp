/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// TODO LVI 2011-10-30: determine how to report import errors.
// Currently all output (both debug and error reports) are done using LOGD.

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#if 0
#include "lexer.h"
#include "writer.h"
#include "parser.h"
#endif

#include "engraving/types/fraction.h"

#if 0
#include "libmscore/factory.h"
#include "libmscore/barline.h"
#include "libmscore/box.h"
#include "libmscore/chord.h"
#include "libmscore/clef.h"
#include "libmscore/keysig.h"
#include "libmscore/layoutbreak.h"
#include "libmscore/measure.h"
#include "libmscore/note.h"
#include "libmscore/part.h"
#include "libmscore/pitchspelling.h"
#include "libmscore/masterscore.h"
#include "libmscore/slur.h"
#include "libmscore/tie.h"
#include "libmscore/staff.h"
#include "libmscore/tempotext.h"
#include "libmscore/timesig.h"
#include "libmscore/tuplet.h"
#include "libmscore/volta.h"
#include "libmscore/segment.h"
#endif

#include "libmscore/chord.h"
#include "libmscore/factory.h"
#include "libmscore/masterscore.h"
#include "libmscore/measure.h"
#include "libmscore/part.h"
#include "libmscore/score.h"
#include "libmscore/staff.h"
#include "libmscore/timesig.h"
#include "libmscore/tuplet.h"

#include "log.h"

using namespace mu::engraving;

namespace Bww {
/**
 The writer that imports into MuseScore.
 */

#if 0
//---------------------------------------------------------
//   addText
//   copied from importxml.cpp
//   TODO: remove duplicate code
//---------------------------------------------------------

static void addText(Ms::VBox*& vbx, Ms::Score* s, QString strTxt, Ms::TextStyleType stl)
{
    if (!strTxt.isEmpty()) {
        if (vbx == 0) {
            vbx = new Ms::VBox(s->dummy()->system());
        }
        Ms::Text* text = Factory::createText(vbx, stl);
        text->setPlainText(strTxt);
        vbx->add(text);
    }
}

//---------------------------------------------------------
//   xmlSetPitch
//   copied and adapted from importxml.cpp
//   TODO: remove duplicate code
//---------------------------------------------------------

/**
 Convert MusicXML \a step / \a alter / \a octave to midi pitch,
 set pitch and tpc.
 */

static void xmlSetPitch(Ms::Note* n, char step, int alter, int octave)
{
    int istep = step - 'A';
    //                       a  b   c  d  e  f  g
    static int table[7]  = { 9, 11, 0, 2, 4, 5, 7 };
    if (istep < 0 || istep > 6) {
        LOGD("xmlSetPitch: illegal pitch %d, <%c>", istep, step);
        return;
    }
    int pitch = table[istep] + alter + (octave + 1) * 12;

    if (pitch < 0) {
        pitch = 0;
    }
    if (pitch > 127) {
        pitch = 127;
    }

    n->setPitch(pitch);

    //                        a  b  c  d  e  f  g
    static int table1[7]  = { 5, 6, 0, 1, 2, 3, 4 };
    int tpc  = step2tpc(table1[istep], Ms::AccidentalVal(alter));
    n->setTpc(tpc);
}

//---------------------------------------------------------
//   setTempo
//   copied and adapted from importxml.cpp
//   TODO: remove duplicate code
//---------------------------------------------------------

static void setTempo(Ms::Score* score, int tempo)
{
    Ms::Measure* measure = score->firstMeasure();
    Ms::Segment* segment = measure->getSegment(Ms::SegmentType::ChordRest, Ms::Fraction(0, 1));
    Ms::TempoText* tt = new Ms::TempoText(segment);
    tt->setTempo(double(tempo) / 60.0);
    tt->setTrack(0);
    QString tempoText = Ms::TempoText::duration2tempoTextString(Ms::DurationType::V_QUARTER);
    tempoText += QString(" = %1").arg(tempo);
    tt->setPlainText(tempoText);
    segment->add(tt);
}

class MsScWriter : public Writer
{
public:
    MsScWriter();
    void beginMeasure(const Bww::MeasureBeginFlags mbf);
    void endMeasure(const Bww::MeasureEndFlags mef);
    void header(const QString title, const QString type, const QString composer, const QString footer, const unsigned int temp);
    void note(const QString pitch, const QVector<Bww::BeamType> beamList, const QString type, const int dots, bool tieStart = false,
              bool tieStop = false, StartStop triplet = StartStop::ST_NONE, bool grace = false);
    void setScore(Ms::Score* s) { score = s; }
    void tsig(const int beats, const int beat);
    void trailer();
private:
    void doTriplet(Ms::Chord* cr, StartStop triplet = StartStop::ST_NONE);
    static const int WHOLE_DUR = 64;                    ///< Whole note duration
    struct StepAlterOct {                               ///< MusicXML step/alter/oct values
        QChar s;
        int a;
        int o;
        StepAlterOct(QChar step = QChar('C'), int alter = 0, int oct = 1)
            : s(step), a(alter), o(oct) {}
    };
    Ms::Score* score;                                       ///< The score
    int beats;                                          ///< Number of beats
    int beat;                                           ///< Beat type
    QMap<QString, StepAlterOct> stepAlterOctMap;        ///< Map bww pitch to step/alter/oct
    QMap<QString, QString> typeMap;                     ///< Map bww note types to MusicXML
    unsigned int measureNumber;                         ///< Current measure number
    Ms::Fraction tick;                                      ///< Current tick
    Ms::Measure* currentMeasure;                            ///< Current measure
    Ms::Tuplet* tuplet;                                     ///< Current tuplet
    Ms::Volta* lastVolta;                                   ///< Current volta
    unsigned int tempo;                                 ///< Tempo (0 = not specified)
    unsigned int ending;                                ///< Current ending
    QList<Ms::Chord*> currentGraceNotes;
};

/**
 MsScWriter constructor.
 */

MsScWriter::MsScWriter()
    : score(0),
    beats(4),
    beat(4),
    measureNumber(0),
    tick(Ms::Fraction(0, 1)),
    currentMeasure(0),
    tuplet(0),
    lastVolta(0),
    tempo(0),
    ending(0)
{
    LOGD() << "MsScWriter::MsScWriter()";

    stepAlterOctMap["LG"] = StepAlterOct('G', 0, 4);
    stepAlterOctMap["LA"] = StepAlterOct('A', 0, 4);
    stepAlterOctMap["B"] = StepAlterOct('B', 0, 4);
    stepAlterOctMap["C"] = StepAlterOct('C', 1, 5);
    stepAlterOctMap["D"] = StepAlterOct('D', 0, 5);
    stepAlterOctMap["E"] = StepAlterOct('E', 0, 5);
    stepAlterOctMap["F"] = StepAlterOct('F', 1, 5);
    stepAlterOctMap["HG"] = StepAlterOct('G', 0, 5);
    stepAlterOctMap["HA"] = StepAlterOct('A', 0, 5);

    typeMap["1"] = "whole";
    typeMap["2"] = "half";
    typeMap["4"] = "quarter";
    typeMap["8"] = "eighth";
    typeMap["16"] = "16th";
    typeMap["32"] = "32nd";
}

/**
 Begin a new measure.
 */

void MsScWriter::beginMeasure(const Bww::MeasureBeginFlags mbf)
{
    LOGD() << "MsScWriter::beginMeasure()";
    ++measureNumber;

    // create a new measure
    currentMeasure  = Factory::createMeasure(score->dummy()->system());
    currentMeasure->setTick(tick);
    currentMeasure->setTimesig(Ms::Fraction(beats, beat));
    currentMeasure->setNo(measureNumber);
    score->measures()->add(currentMeasure);

    if (mbf.repeatBegin) {
        currentMeasure->setRepeatStart(true);
    }

    if (mbf.irregular) {
        currentMeasure->setIrregular(true);
    }

    if (mbf.endingFirst || mbf.endingSecond) {
        Ms::Volta* volta = new Ms::Volta(score->dummy());
        volta->setTrack(0);
        volta->endings().clear();
        if (mbf.endingFirst) {
            volta->setText("1");
            volta->endings().push_back(1);
            ending = 1;
        } else {
            volta->setText("2");
            volta->endings().push_back(2);
            ending = 2;
        }
        volta->setTick(currentMeasure->tick());
        score->addElement(volta);
        lastVolta = volta;
    }

    // set clef, key and time signature in the first measure
    if (measureNumber == 1) {
        // clef
        Ms::Segment* s = currentMeasure->getSegment(Ms::SegmentType::Clef, tick);
        Ms::Clef* clef = Factory::createClef(s);
        clef->setClefType(Ms::ClefType::G);
        clef->setTrack(0);
        s->add(clef);
        // keysig
        Ms::KeySigEvent key;
        key.setKey(Ms::Key::D);
        s = currentMeasure->getSegment(Ms::SegmentType::KeySig, tick);
        Ms::KeySig* keysig = Factory::createKeySig(s);
        keysig->setKeySigEvent(key);
        keysig->setTrack(0);
        s->add(keysig);
        // timesig
        s = currentMeasure->getSegment(Ms::SegmentType::TimeSig, tick);
        Ms::TimeSig* timesig = Factory::createTimeSig(s);
        timesig->setSig(Ms::Fraction(beats, beat));
        timesig->setTrack(0);
        s->add(timesig);
        LOGD("tempo %d", tempo);
    }
}

/**
 End the current measure.
 */

void MsScWriter::endMeasure(const Bww::MeasureEndFlags mef)
{
    LOGD() << "MsScWriter::endMeasure()";
    if (mef.repeatEnd) {
        currentMeasure->setRepeatEnd(true);
    }

    if (mef.endingEnd) {
        if (lastVolta) {
            LOGD("adding volta");
            if (ending == 1) {
                lastVolta->setVoltaType(Ms::Volta::Type::CLOSED);
            } else {
                lastVolta->setVoltaType(Ms::Volta::Type::OPEN);
            }
            lastVolta->setTick2(tick);
            lastVolta = 0;
        } else {
            LOGD("lastVolta == 0 on stop");
        }
    }

    if (mef.lastOfSystem) {
        Ms::LayoutBreak* lb = Factory::createLayoutBreak(currentMeasure);
        lb->setTrack(0);
        lb->setLayoutBreakType(Ms::LayoutBreakType::LINE);
        currentMeasure->add(lb);
    }

    if (mef.lastOfPart && !mef.repeatEnd) {
//TODO            currentMeasure->setEndBarLineType(Ms::BarLineType::END, false, true);
    } else if (mef.doubleBarLine) {
//TODO            currentMeasure->setEndBarLineType(Ms::BarLineType::DOUBLE, false, true);
    }
    // BarLine* barLine = new BarLine(score);
    // bool visible = true;
    // barLine->setSubtype(BarLineType::NORMAL);
    // barLine->setTrack(0);
    // currentMeasure->setEndBarLineType(barLine->subtype(), false, visible);
}

/**
 Write a single note.
 */

void MsScWriter::note(const QString pitch, const QVector<Bww::BeamType> beamList,
                      const QString type, const int dots,
                      bool tieStart, bool /*TODO tieStop */,
                      StartStop triplet,
                      bool grace)
{
    LOGD() << "MsScWriter::note()"
           << "type:" << type
           << "dots:" << dots
           << "grace" << grace
    ;

    if (!stepAlterOctMap.contains(pitch)
        || !typeMap.contains(type)) {
        // TODO: error message
        return;
    }
    StepAlterOct sao = stepAlterOctMap.value(pitch);

    int ticks = 4 * Ms::Constant::division / type.toInt();
    if (dots) {
        ticks = 3 * ticks / 2;
    }
    LOGD() << "ticks:" << ticks;
    Ms::TDuration durationType(Ms::DurationType::V_INVALID);
    durationType.setVal(ticks);
    if (triplet != StartStop::ST_NONE) {
        ticks = 2 * ticks / 3;
    }

    Ms::BeamMode bm  = (beamList.at(0) == Bww::BeamType::BM_BEGIN) ? Ms::BeamMode::BEGIN : Ms::BeamMode::AUTO;
    Ms::DirectionV sd = Ms::DirectionV::AUTO;

    // create chord
    Ms::Chord* cr = Factory::createChord(score->dummy()->segment());
    //ws cr->setTick(tick);
    cr->setBeamMode(bm);
    cr->setTrack(0);
    if (grace) {
        cr->setNoteType(Ms::NoteType::GRACE32);
        cr->setDurationType(Ms::DurationType::V_32ND);
        sd = Ms::DirectionV::UP;
    } else {
        if (durationType.type() == Ms::DurationType::V_INVALID) {
            durationType.setType(Ms::DurationType::V_QUARTER);
        }
        cr->setDurationType(durationType);
        sd = Ms::DirectionV::DOWN;
    }
    cr->setTicks(durationType.fraction());
    cr->setDots(dots);
    cr->setStemDirection(sd);
    // add note to chord
    Ms::Note* note = Factory::createNote(cr);
    note->setTrack(0);
    xmlSetPitch(note, sao.s.toLatin1(), sao.a, sao.o);
    if (tieStart) {
        Ms::Tie* tie = new Ms::Tie(score->dummy());
        note->setTieFor(tie);
        tie->setStartNote(note);
        tie->setTrack(0);
    }
    cr->add(note);
    // add chord to measure
    if (!grace) {
        Ms::Segment* s = currentMeasure->getSegment(Ms::SegmentType::ChordRest, tick);
        s->add(cr);
        if (!currentGraceNotes.isEmpty()) {
            for (int i = currentGraceNotes.size() - 1; i >= 0; i--) {
                cr->add(currentGraceNotes.at(i));
            }
            currentGraceNotes.clear();
        }
        doTriplet(cr, triplet);
        Ms::Fraction tickBefore = tick;
        tick += Ms::Fraction::fromTicks(ticks);
        Ms::Fraction nl(tick - currentMeasure->tick());
        currentMeasure->setTicks(nl);
        LOGD() << "MsScWriter::note()"
               << "tickBefore:" << tickBefore.toString()
               << "tick:" << tick.toString()
               << "nl:" << nl.toString()
        ;
    } else {
        currentGraceNotes.append(cr);
    }
}

/**
 Write the header.
 */

void MsScWriter::header(const QString title, const QString type,
                        const QString composer, const QString footer,
                        const unsigned int temp)
{
    LOGD() << "MsScWriter::header()"
           << "title:" << title
           << "type:" << type
           << "composer:" << composer
           << "footer:" << footer
           << "temp:" << temp
    ;

    // save tempo for later use
    tempo = temp;

    if (!title.isEmpty()) {
        score->setMetaTag("workTitle", title);
    }
    // TODO re-enable following statement
    // currently disabled because it breaks the bww iotest
    // if (!type.isEmpty()) score->setMetaTag("workNumber", type);
    if (!composer.isEmpty()) {
        score->setMetaTag("composer", composer);
    }
    if (!footer.isEmpty()) {
        score->setMetaTag("copyright", footer);
    }

    //  score->setWorkTitle(title);
    Ms::VBox* vbox  = 0;
    Bww::addText(vbox, score, title, Ms::TextStyleType::TITLE);
    Bww::addText(vbox, score, type, Ms::TextStyleType::SUBTITLE);
    Bww::addText(vbox, score, composer, Ms::TextStyleType::COMPOSER);
    // addText(vbox, score, strPoet, Ms::TextStyleName::POET);
    // addText(vbox, score, strTranslator, Ms::TextStyleName::TRANSLATOR);
    if (vbox) {
        vbox->setTick(Ms::Fraction(0, 1));
        score->measures()->add(vbox);
    }
    if (!footer.isEmpty()) {
        score->style().set(Ms::Sid::oddFooterC, footer);
    }

    Ms::Part* part = score->staff(0)->part();
    part->setPlainLongName(instrumentName());
    part->setPartName(instrumentName());
    part->instrument()->setTrackName(instrumentName());
    part->setMidiProgram(midiProgram() - 1);
}

/**
 Store beats and beat type for later use.
 */

void MsScWriter::tsig(const int bts, const int bt)
{
    LOGD() << "MsScWriter::tsig()"
           << "beats:" << bts
           << "beat:" << bt
    ;

    beats = bts;
    beat  = bt;
}

/**
 Write the trailer.
 */

void MsScWriter::trailer()
{
    LOGD() << "MsScWriter::trailer()"
    ;

    if (tempo) {
        setTempo(score, tempo);
    }
}

/**
 Handle the triplet.
 */

void MsScWriter::doTriplet(Ms::Chord* cr, StartStop triplet)
{
    LOGD() << "MsScWriter::doTriplet(" << static_cast<int>(triplet) << ")"
    ;

    if (triplet == StartStop::ST_START) {
        tuplet = new Ms::Tuplet(currentMeasure);
        tuplet->setTrack(0);
        tuplet->setRatio(Ms::Fraction(3, 2));
//            tuplet->setTick(tick);
        currentMeasure->add(tuplet);
    } else if (triplet == StartStop::ST_STOP) {
        if (tuplet) {
            cr->setTuplet(tuplet);
            tuplet->add(cr);
            tuplet = 0;
        } else {
            LOGD("BWW::import: triplet stop without triplet start");
        }
    } else if (triplet == StartStop::ST_CONTINUE) {
        if (!tuplet) {
            LOGD("BWW::import: triplet continue without triplet start");
        }
    } else if (triplet == StartStop::ST_NONE) {
        if (tuplet) {
            LOGD("BWW::import: triplet none inside triplet");
        }
    } else {
        LOGD("unknown triplet type %d", static_cast<int>(triplet));
    }
    if (tuplet) {
        cr->setTuplet(tuplet);
        tuplet->add(cr);
    }
}

#endif
} // namespace Bww

namespace Ms {
#if 0
//---------------------------------------------------------
//   importBww
//---------------------------------------------------------

Score::FileError importBww(MasterScore* score, const QString& path)
{
    LOGD("Score::importBww(%s)", qPrintable(path));

    QFile fp(path);
    if (!fp.exists()) {
        return Score::FileError::FILE_NOT_FOUND;
    }
    if (!fp.open(QIODevice::ReadOnly)) {
        return Score::FileError::FILE_OPEN_ERROR;
    }

    Part* part = new Part(score);
    score->appendPart(part);
    Staff* staff = Factory::createStaff(part);
    score->appendStaff(staff);

    Bww::Lexer lex(&fp);
    Bww::MsScWriter wrt;
    wrt.setScore(score);
    score->resetStyleValue(Sid::measureSpacing);
    Bww::Parser p(lex, wrt);
    p.parse();

    score->setSaved(false);
    score->setNewlyCreated(true);
    score->connectTies();
    LOGD("Score::importBww() done");
    return Score::FileError::FILE_NO_ERROR;        // OK
#endif

//---------------------------------------------------------
//   mnxValueUnitToDurationType
//---------------------------------------------------------

/**
 * Convert MNX note value unit to MuseScore DurationType.
 */

static Ms::DurationType mnxValueUnitToDurationType(const QString& s)
{
    if (s == "/4") {
        return Ms::DurationType::V_QUARTER;
    } else if (s == "/8") {
        return Ms::DurationType::V_EIGHTH;
    } else if (s == "/1024") {
        return Ms::DurationType::V_1024TH;
    } else if (s == "/512") {
        return Ms::DurationType::V_512TH;
    } else if (s == "/256") {
        return Ms::DurationType::V_256TH;
    } else if (s == "/128") {
        return Ms::DurationType::V_128TH;
    } else if (s == "/64") {
        return Ms::DurationType::V_64TH;
    } else if (s == "/32") {
        return Ms::DurationType::V_32ND;
    } else if (s == "/16") {
        return Ms::DurationType::V_16TH;
    } else if (s == "/2") {
        return Ms::DurationType::V_HALF;
    } else if (s == "/1") {
        return Ms::DurationType::V_WHOLE;
    } else if (s == "*2") {
        return Ms::DurationType::V_BREVE;
    } else if (s == "*4") {
        return Ms::DurationType::V_LONG;
    } else {
        qDebug("mnxValueUnitToDurationType(%s): unknown", qPrintable(s));
        return Ms::DurationType::V_INVALID;
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
    auto chord = Factory::createChord(score->dummy()->segment());
    chord->setTrack(0);               // TODO
    chord->setDurationType(dur);
    chord->setTicks(duration.isValid() ? duration : dur.fraction());          // specified duration overrules value-based
    chord->setDots(dur.dots());
    return chord;
}

//---------------------------------------------------------
//   createNote
//---------------------------------------------------------

Note* createNote(Chord* c, const int pitch)
{
    auto note = Factory::createNote(c);
    note->setTrack(0);              // TODO
    note->setPitch(pitch);
    note->setTpcFromPitch();
    return note;
}

//---------------------------------------------------------
//   createTimeSig
//---------------------------------------------------------

TimeSig* createTimeSig(Segment* seg, const Fraction& sig)
{
    auto timesig = Factory::createTimeSig(seg);
    timesig->setSig(sig);
    timesig->setTrack(0);                    // TODO
    return timesig;
}

//---------------------------------------------------------
//   createTuplet
//---------------------------------------------------------

Tuplet* createTuplet(Measure* measure, const int track)
{
    auto tuplet = new Tuplet(measure);
    tuplet->setTrack(track);
    return tuplet;
}

//---------------------------------------------------------
//   setTupletParameters
//---------------------------------------------------------

static void setTupletParameters(Tuplet* tuplet, const int actual, const int normal, const Ms::DurationType base)
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
/* TODO
    qDebug("JsonTuplet::read() rtick %s",
           qPrintable(tick.print())
           );
*/
    Fraction tupTime { 0, 1 };               // time in this tuplet
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
/* TODO
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
*/
    if (json.contains("baselen") && json.contains("ratio")) {
        // tuplet found
        const auto baselen = mnxEventValueToTDuration(json["baselen"].toString());
        const auto ratio = Fraction::fromString(json["ratio"].toString());
        //qDebug("baselen %s ratio %s", qPrintable(baselen.fraction().print()), qPrintable(ratio.print()));
        // create the tuplet
        auto newTuplet = createTuplet(measure, 0 /* TODO track */);
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
    } else {
        Fraction duration { 0, 0 };       // initialize invalid to catch missing duration
        if (json.contains("duration")) {
            duration = Fraction::fromString(json["duration"].toString());
            //qDebug("duration %s", qPrintable(duration.print()));
        }
        Fraction increment { 0, 0 };       // initialize invalid to catch missing increment
        if (json.contains("increment")) {
            increment = Fraction::fromString(json["increment"].toString());
            //qDebug("increment %s", qPrintable(increment.print()));
        }
        auto c = createChord(measure->score(), json["value"].toString(), duration);
        bool ok { true };
        int pitch { json["pitch"].toString().toInt(&ok) };
        //qDebug("ok %d pitch %d", ok, pitch);
        c->add(createNote(c, pitch));
        auto s = measure->getSegment(SegmentType::ChordRest, tick);
        s->add(c);
        if (tuplet) {
            c->setTuplet(tuplet);
            tuplet->add(c);
        }

        // todo: check if following supports using increment in a tuplet
        auto res = increment.isValid() ? increment : c->ticks();
        //qDebug("res %s", qPrintable(res.print()));
        for (Tuplet* t = c->tuplet(); t; t = t->tuplet()) {
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
    return {};         // TODO to suppurt nested sequences
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
    auto m = Factory::createMeasure(score->dummy()->system());
    m->setTick(startTick);
    m->setTimesig(timeSig);
    score->measures()->add(m);
    if (startTick == Fraction { 0, 1 }) {
        auto s = m->getSegment(SegmentType::TimeSig, { 0, 1 });
        auto ts = createTimeSig(s, timeSig);
        s->add(ts);
    }
    QJsonArray array = json["sequences"].toArray();
    for (int i = 0; i < array.size(); ++i) {
        QJsonObject object = array[i].toObject();
        JsonSequence sequence;
        sequence.read(object, m, startTick);
    }
    const auto length = timeSig;               // TODO: use real length instead ?
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
    // TODO (?) part->setId("dbg");
    score->appendPart(part);
    auto staff = Factory::createStaff(part);
    score->appendStaff(staff);
    Fraction timeSig { 4, 4 };         // default: assume all measures are 4/4
    if (json.contains("time")) {
        timeSig = Fraction::fromString(json["time"].toString());
        // TODO qDebug("timesig %s", qPrintable(timeSig.print()));
    }
    QJsonArray array = json["measures"].toArray();
    for (int i = 0; i < array.size(); ++i) {
        QJsonObject object = array[i].toObject();
        JsonMeasure measure;
        measure.read(score, object, timeSig, timeSig * i);
    }
}

//---------------------------------------------------------
//   importBww
//---------------------------------------------------------

Score::FileError importBww(MasterScore* score, const QString& path)
{
    LOGD("Score::importBww(%s) importing DBG", qPrintable(path));

    QFile fp(path);
    if (!fp.exists()) {
        return Score::FileError::FILE_NOT_FOUND;
    }
    if (!fp.open(QIODevice::ReadOnly)) {
        return Score::FileError::FILE_OPEN_ERROR;
    }

    QByteArray data = fp.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    JsonScore jsonScore;
    jsonScore.read(score, doc.object());

    LOGD("Score::importBww() done");
    return Score::FileError::FILE_NO_ERROR;        // OK
}
} // namespace Ms
