/****************************************************************************
**
** mxmlparser.cpp -- implementation of class MxmlParser
**
****************************************************************************/

// TODO: add error reporting to all "if (ok)" constructs

#include <iostream>
#include <QtDebug>
//#include "logger.h"
#include "mxmlparser.h"

namespace MusicXML {

void MxmlParser::log_error(const QString& text)
{
    /*
    MusicXMLError err { m_filename, m_e.lineNumber(), m_e.columnNumber(), "error: " + text };
    m_file.addError(err);
    */
    QString err { "%1:%2:%3: %4: %5" };
    std::cout << qPrintable(err
                            .arg(m_filename)
                            .arg(m_e.lineNumber())
                            .arg(m_e.columnNumber())
                            .arg("error")
                            .arg(text))
              << std::endl;
}

void MxmlParser::log_warning(const QString& text)
{
    /*
    MusicXMLError err { m_filename, m_e.lineNumber(), m_e.columnNumber(), "warning: " + text };
    m_file.addError(err);
    */
    QString err { "%1:%2:%3: %4: %5" };
    std::cout << qPrintable(err
                            .arg(m_filename)
                            .arg(m_e.lineNumber())
                            .arg(m_e.columnNumber())
                            .arg("warning")
                            .arg(text))
              << std::endl;
}

bool MxmlParser::parse()
{
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "score-partwise") {
            m_data.scorePartwise.isFound = true;
            m_data.scorePartwise.version = std::string { m_e.attributes().value("version").toUtf8().data() };
            parseScorePartwise();
        }
        else {
            const auto text = QString { "found '%1' instead of 'score-partwise'" }
                    .arg(m_e.name().toString());
            log_error(text);
            m_e.skipCurrentElement();
        }
    }

    if (!m_data.scorePartwise.isFound) {
        log_error("'score-partwise' not found");
    }
    return m_data.scorePartwise.isFound;
}

// read and parse the input file

int MxmlParser::parse(QIODevice* data, const QString &filename)
{
    m_e.setDevice(data);
    m_filename = filename;

    const auto res = parse() ? 0 : 10;
    // print result
    //std::cout << qPrintable(m_file.toQString());
    return res;
}

std::unique_ptr<Attributes> MxmlParser::parseAttributes()
{
    std::unique_ptr<MusicXML::Attributes> attributes(new Attributes);
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "divisions") {
            attributes->divisions = parseDivisions();
        }
        else if (m_e.name() == "clef") {
            const auto numberClef = parseClef();
            const auto number = numberClef.first;
            const auto clef = numberClef.second;
            if (attributes->clefs.count(number) == 0) {
                attributes->clefs[number] = clef;
            }
            else {
                // TODO: report non-unique number
            }
        }
        else if (m_e.name() == "key") {
            attributes->keys.push_back(parseKey());
        }
        else if (m_e.name() == "staves") {
              unsigned int staves;
              bool ok;
              staves = m_e.readElementText().toUInt(&ok);
              if (ok) {
                    attributes->staves = staves;
              }
        }
        else if (m_e.name() == "time") {
            attributes->times.push_back(parseTime());
        }
        else {
            unexpectedElement();
        }
    }
    return attributes;
}

std::unique_ptr<Backup> MxmlParser::parseBackup()
{
    std::unique_ptr<Backup> backup(new Backup);
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "duration") {
            unsigned int duration;
            bool ok;
            duration = m_e.readElementText().toUInt(&ok);
            if (ok) {
                backup->duration = duration;
            }
        }
        else {
            unexpectedElement();
        }
    }
    return backup;
}

// TODO: error detection and reporting
std::pair<unsigned int, Clef> MxmlParser::parseClef()
{
    bool ok1;
    unsigned int number = m_e.attributes().value("number").toUInt(&ok1);
    if (ok1) {
        number--;
    }

    Clef clef;
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "clef-octave-change") {
            m_e.skipCurrentElement();   // ignore
        }
        else if (m_e.name() == "line") {
            int line;
            bool ok2;
            line = m_e.readElementText().toUInt(&ok2);
            if (ok2) {
                clef.line = line;
            }
        }
        else if (m_e.name() == "sign") {
            clef.sign = m_e.readElementText().toUtf8().data();
        }
        else {
            unexpectedElement();
        }
    }
    return { number, clef };
}

unsigned int MxmlParser::parseDivisions()
{
    unsigned int divisions;
    bool ok;
    divisions = m_e.readElementText().toUInt(&ok);
    if (ok) {
        return divisions;
    }
    return 0;
}

std::unique_ptr<Forward> MxmlParser::parseForward()
{
    std::unique_ptr<Forward> forward(new Forward);
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "duration") {
            unsigned int duration;
            bool ok;
            duration = m_e.readElementText().toUInt(&ok);
            if (ok) {
                forward->duration = duration;
            }
        }
        else if (m_e.name() == "voice") {
            m_e.skipCurrentElement();   // ignore
        }
        else {
            unexpectedElement();
        }
    }
    return forward;
}

Key MxmlParser::parseKey()
{
    Key key;
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "fifths") {
            int fifths;
            bool ok;
            fifths = m_e.readElementText().toUInt(&ok);
            if (ok) {
                key.fifths = fifths;
            }
        }
        else if (m_e.name() == "mode") {
            m_e.skipCurrentElement();   // ignore
        }
        else {
            unexpectedElement();
        }
    }
    return key;
}

Measure MxmlParser::parseMeasure()
{
    // TODO: verify (and handle) non-empty attribute number
    Measure measure;
    measure.number = std::string { m_e.attributes().value("number").toUtf8().data() };
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "attributes") {
            measure.elements.push_back(parseAttributes());
        }
        else if (m_e.name() == "backup") {
            measure.elements.push_back(parseBackup());
        }
        else if (m_e.name() == "barline") {
            m_e.skipCurrentElement();   // ignore
        }
        else if (m_e.name() == "direction") {
            m_e.skipCurrentElement();   // ignore
        }
        else if (m_e.name() == "forward") {
            measure.elements.push_back(parseForward());
        }
        else if (m_e.name() == "note") {
            measure.elements.push_back(parseNote());
        }
        else {
            unexpectedElement();
        }
    }
    return measure;
}

std::unique_ptr<Note> MxmlParser::parseNote()
{
    // TODO add error detection and handling
    std::unique_ptr<Note> note(new Note);
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "accidental") {
            m_e.skipCurrentElement();   // ignore
        }
        else if (m_e.name() == "beam") {
            m_e.skipCurrentElement();   // ignore
        }
        else if (m_e.name() == "chord") {
            note->chord = true;
            m_e.skipCurrentElement();
        }
        else if (m_e.name() == "dot") {
            note->dots++;
            m_e.skipCurrentElement();
        }
        else if (m_e.name() == "duration") {
            unsigned int duration;
            bool ok;
            duration = m_e.readElementText().toUInt(&ok);
            if (ok) {
                note->duration = duration;
            }
        }
        else if (m_e.name() == "grace") {
            note->grace = true;
            m_e.skipCurrentElement();
        }
        else if (m_e.name() == "lyric") {
            m_e.skipCurrentElement();   // ignore
        }
        else if (m_e.name() == "notations") {
            m_e.skipCurrentElement();   // ignore
        }
        else if (m_e.name() == "pitch") {
            note->pitch = parsePitch();
        }
        else if (m_e.name() == "rest") {
            note->rest = true;
            note->measureRest = m_e.attributes().value("measure") == "yes";
            m_e.skipCurrentElement();
        }
        else if (m_e.name() == "staff") {
              unsigned int staff;
              bool ok;
              staff = m_e.readElementText().toUInt(&ok);
              if (ok) {
                    note->staff = staff;
              }
        }
        else if (m_e.name() == "stem") {
            m_e.skipCurrentElement();   // ignore
        }
        else if (m_e.name() == "time-modification") {
            note->timeModification = parseTimeModification();
        }
        else if (m_e.name() == "type") {
            note->type = m_e.readElementText().toUtf8().data();
        }
        else if (m_e.name() == "voice") {
            note->voice = m_e.readElementText().toUtf8().data();
        }
        else {
            unexpectedElement();
        }
    }
    return note;
}

Part MxmlParser::parsePart()
{
    // TODO: verify (and handle) non-empty attribute id
    Part part;
    part.id = std::string { m_e.attributes().value("id").toUtf8().data() };
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "measure") {
            part.measures.push_back(parseMeasure());
        }
        else {
            unexpectedElement();
        }
    }
    // TODO: verify (and handle) non-empty part.measures
    return part;
}

void MxmlParser::parsePartList()
{
    // TODO: verify (and handle) at least one score-part
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "part-group") {
            m_e.skipCurrentElement();   // ignore
        }
        else if (m_e.name() == "score-part") {
            m_data.scorePartwise.partList.scoreParts.push_back(parseScorePart());
        }
        else {
            unexpectedElement();
        }
    }
}

std::string MxmlParser::parsePartName()
{
    const auto partName = m_e.readElementText().toUtf8().data();
    return partName;
}

Pitch MxmlParser::parsePitch()
{
    // TODO add error detection and handling
    Pitch pitch;
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "alter") {
            m_e.skipCurrentElement();   // ignore
        }
        else if (m_e.name() == "octave") {
            int octave;
            bool ok;
            octave = m_e.readElementText().toUInt(&ok);
            if (ok) {
                pitch.octave = octave;
            }
        }
        else if (m_e.name() == "step") {
            const auto step = m_e.readElementText();
            if (step.size() == 1) {
                pitch.step = step.at(0).toLatin1();
            }
        }
        else {
            unexpectedElement();
        }
    }
    return pitch;
}


ScorePart MxmlParser::parseScorePart()
{
    // TODO: verify (and handle) non-empty attribute id
    ScorePart scorePart;
    scorePart.id = std::string { m_e.attributes().value("id").toUtf8().data() };
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "midi-device") {
            m_e.skipCurrentElement();   // ignore
        }
        else if (m_e.name() == "midi-instrument") {
            m_e.skipCurrentElement();   // ignore
        }
        else if (m_e.name() == "part-abbreviation") {
            m_e.skipCurrentElement();   // ignore
        }
        else if (m_e.name() == "part-name") {
            scorePart.partName = parsePartName();
        }
        else if (m_e.name() == "score-instrument") {
            m_e.skipCurrentElement();   // ignore
        }
        else {
            unexpectedElement();
        }
    }
    return scorePart;
}

void MxmlParser::parseScorePartwise()
{
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "identification") {
            m_e.skipCurrentElement();   // ignore
        }
        else if (m_e.name() == "part") {
            m_data.scorePartwise.parts.push_back(parsePart());
        }
        else if (m_e.name() == "part-list") {
            parsePartList();
        }
        else if (m_e.name() == "work") {
            m_e.skipCurrentElement();   // ignore
        }
        else {
            unexpectedElement();
        }
    }
}

Time MxmlParser::parseTime()
{
    Time time;
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "beat-type") {
            time.beatType = m_e.readElementText().toUtf8().data();
        }
        else if (m_e.name() == "beats") {
            time.beats = m_e.readElementText().toUtf8().data();
        }
        else
            unexpectedElement();
    }
    return time;
}

TimeModification MxmlParser::parseTimeModification()
{
    TimeModification timeModification;
    bool actualOk { false };
    bool normalOk { false };
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "actual-notes") {
            timeModification.actualNotes = m_e.readElementText().toInt(&actualOk);
        }
        else if (m_e.name() == "normal-notes") {
            timeModification.normalNotes = m_e.readElementText().toInt(&normalOk);
        }
        else {
            unexpectedElement();
        }
    }
    // TODO: error reporting
    if (timeModification.isValid()) {
        return timeModification;
    }
    return {};
}

void MxmlParser::unexpectedElement()
{
    const auto text = QString { "found unexpected element '%1'" }
            .arg(m_e.name().toString());
    log_warning(text);
    m_e.skipCurrentElement();
}

} // namespace MusicXML
