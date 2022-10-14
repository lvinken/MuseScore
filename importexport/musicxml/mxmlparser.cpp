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
        else if (m_e.name() == "transpose") {
              attributes->transpose = parseTranspose();
              attributes->transposeRead = true;
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

std::unique_ptr<Barline> MxmlParser::parseBarline()
{
    // TODO add error detection and handling
    std::unique_ptr<Barline> barline(new Barline);
    barline->location = m_e.attributes().value("location").toUtf8().data();

    while (m_e.readNextStartElement()) {
        if (m_e.name() == "bar-style")
            barline->barStyle = m_e.readElementText().toUtf8().data();
        else if (m_e.name() == "ending") {
            barline->endingNumber = m_e.attributes().value("number").toUtf8().data();
            barline->endingType   = m_e.attributes().value("type").toUtf8().data();
            barline->endingText = m_e.readElementText().toUtf8().data();
        }
        else if (m_e.name() == "repeat") {
            barline->repeatDirection = m_e.attributes().value("direction").toUtf8().data();
            barline->repeatTimes = m_e.attributes().value("number").toUInt();
            m_e.skipCurrentElement();
        }
        else
            unexpectedElement();
    }
    return barline;
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

Creator MxmlParser::parseCreator()
{
    Creator creator;
    creator.type = m_e.attributes().value("type").toUtf8().data();
    creator.text = m_e.readElementText().toUtf8().data();
    return creator;
}

Credit MxmlParser::parseCredit()
{
    Credit credit;

    unsigned int page;
    bool ok;
    page = m_e.attributes().value("page").toUInt(&ok);
    if (ok && page > 0) {
        credit.page = page - 1;
    }

    while (m_e.readNextStartElement()) {
        if (m_e.name() == "credit-words") {
            CreditWords creditWords;
            creditWords.defaultX = m_e.attributes().value("default-x").toFloat();
            creditWords.defaultY = m_e.attributes().value("default-y").toFloat();
            creditWords.fontSize = m_e.attributes().value("font-size").toFloat();
            creditWords.justify = m_e.attributes().value("justify").toUtf8().data();
            creditWords.halign = m_e.attributes().value("halign").toUtf8().data();
            creditWords.valign = m_e.attributes().value("valign").toUtf8().data();
            creditWords.text = m_e.readElementText().toUtf8().data();
            credit.creditWordses.push_back(creditWords);
        }
        else if (m_e.name() == "credit-type") {
            credit.creditTypes.push_back(m_e.readElementText().toUtf8().data());
        }
        else {
            unexpectedElement();
        }
    }
    return credit;
}

Defaults MxmlParser::parseDefaults(bool& read)
{
    Defaults defaults;
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "lyric-font") {
            defaults.lyricFont = parseFont();
            defaults.lyricFontRead = true;
        }
        else if (m_e.name() == "page-layout") {
            defaults.pageLayout = parsePageLayout(defaults.pageLayoutRead);
        }
        else if (m_e.name() == "scaling") {
            defaults.scaling = parseScaling(defaults.scalingRead);
        }
        else if (m_e.name() == "staff-layout") {
              defaults.staffLayout = parseStaffLayout();
        }
        else if (m_e.name() == "system-layout") {
              defaults.systemLayout = parseSystemLayout();
        }
        else if (m_e.name() == "word-font") {
            defaults.wordFont = parseFont();
            defaults.wordFontRead = true;
        }
        else {
            unexpectedElement();
        }
    }
    read = true;
    return defaults;
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

Encoding MxmlParser::parseEncoding()
{
    Encoding encoding;
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "supports") {
            encoding.supportses.push_back(parseSupports());
        }
        else {
            unexpectedElement();
        }
    }
    return encoding;
}

Font MxmlParser::parseFont()
{
    Font font;
    font.fontFamily = std::string { m_e.attributes().value("font-family").toUtf8().data() };
    font.fontSize = std::string { m_e.attributes().value("font-size").toUtf8().data() };
    m_e.skipCurrentElement();
    return font;
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

Identification MxmlParser::parseIdentification()
{
    Identification identification;
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "creator") {
            identification.creators.push_back(parseCreator());
        }
        else if (m_e.name() == "encoding") {
            identification.encoding = parseEncoding();
        }
        else if (m_e.name() == "rights") {
            identification.rightses.push_back(parseRights());
        }
        else {
            unexpectedElement();
        }
    }
    return identification;
}

Key MxmlParser::parseKey()
{
    Key key;
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "fifths") {
            int fifths;
            bool ok;
            fifths = m_e.readElementText().toInt(&ok);
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

Lyric MxmlParser::parseLyric()
{
      Lyric lyric;
      lyric.number = std::string { m_e.attributes().value("number").toUtf8().data() };
      while (m_e.readNextStartElement()) {
            if (m_e.name() == "text") {
                  lyric.text = m_e.readElementText().toUtf8().data();
            }
            else {
                  unexpectedElement();
            }
      }
      return lyric;
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
            measure.elements.push_back(parseBarline());
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
        else if (m_e.name() == "sound") {
              measure.elements.push_back(parseSound());
        }
        else {
            unexpectedElement();
        }
    }
    return measure;
}


MidiDevice MxmlParser::parseMidiDevice()
{
    // TODO add error detection and handling
    // TODO: verify (and handle) non-empty attribute id
    MidiDevice midiDevice;
    // IDREF required
    midiDevice.id = std::string { m_e.attributes().value("id").toUtf8().data() };
    // content: midi-16
    midiDevice.port = m_e.attributes().value("port").toUInt(&midiDevice.portRead);
    if (midiDevice.portRead) {
        --midiDevice.port;
    }
    m_e.skipCurrentElement();
    return midiDevice;
}

MidiInstrument MxmlParser::parseMidiInstrument()
{
    // TODO add error detection and handling
    // TODO: verify (and handle) non-empty attribute id
    MidiInstrument midiInstrument;
    // IDREF required
    midiInstrument.id = std::string { m_e.attributes().value("id").toUtf8().data() };
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "midi-channel") {
            // content: midi-16
            midiInstrument.midiChannel = m_e.readElementText().toUInt(&midiInstrument.midiChannelRead);
            if (midiInstrument.midiChannelRead) {
                --midiInstrument.midiChannel;
            }
        }
        else if (m_e.name() == "midi-program") {
            // content: midi-128
            midiInstrument.midiProgram = m_e.readElementText().toUInt(&midiInstrument.midiProgramRead);
                if (midiInstrument.midiProgramRead) {
                    --midiInstrument.midiProgram;
                }
        }
        else if (m_e.name() == "midi-unpitched") {
            // content: midi-128
            midiInstrument.midiUnpitched = m_e.readElementText().toUInt(&midiInstrument.midiUnpitchedRead);
            if (midiInstrument.midiUnpitchedRead) {
                --midiInstrument.midiUnpitched;
            }
        }
        else if (m_e.name() == "pan") {
            // content: rotation-degrees
            midiInstrument.pan = m_e.readElementText().toFloat(&midiInstrument.panRead);
        }
        else if (m_e.name() == "volume") {
            // content: percent
            midiInstrument.volume = m_e.readElementText().toFloat(&midiInstrument.volumeRead);
        }
        else {
            unexpectedElement();
        }
    }
    return midiInstrument;
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
            if (m_e.attributes().value("number") == "1") {
                note->beam = m_e.readElementText().toUtf8().data();
            }
            else {
                m_e.skipCurrentElement();   // ignore
            }
        }
        else if (m_e.name() == "chord") {
            note->chord = true;
            m_e.skipCurrentElement();
        }
        else if (m_e.name() == "cue") {
            note->cue = true;
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
        else if (m_e.name() == "instrument") {
            note->instrument = m_e.attributes().value("id").toUtf8().data();
            m_e.skipCurrentElement();
        }
        else if (m_e.name() == "lyric") {
            note->lyrics.push_back(parseLyric());
        }
        else if (m_e.name() == "notations") {
            m_e.skipCurrentElement();   // ignore
        }
        else if (m_e.name() == "notehead") {
            note->noteheadColor = m_e.attributes().value("color").toUtf8().data();
            note->noteheadFilled = m_e.attributes().value("filled").toUtf8().data();
            note->noteheadParentheses = m_e.attributes().value("parentheses").toUtf8().data();
            note->noteheadText = m_e.readElementText().toUtf8().data();
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
            note->stem = m_e.readElementText().toUtf8().data();
        }
        else if (m_e.name() == "time-modification") {
            note->timeModification = parseTimeModification();
        }
        else if (m_e.name() == "type") {
            note->typeSize = m_e.attributes().value("size").toUtf8().data();
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

PageLayout MxmlParser::parsePageLayout(bool& read)
{
    // page-height and page-width are both either present or not present
    // page-margins is optional, but all its children are required
    PageLayout pageLayout;
    bool pageHeightOk { false };
    bool pageWidthOk { false };
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "page-margins") {
            bool leftMarginOk { false };
            bool rightMarginOk { false };
            bool topMarginOk { false };
            bool bottomMarginOk { false };
            QString type = m_e.attributes().value("type").toString();
            if (type == "")
                type = "both";
            qreal lm = 0.0, rm = 0.0, tm = 0.0, bm = 0.0;
            while (m_e.readNextStartElement()) {
                if (m_e.name() == "left-margin")
                    lm = m_e.readElementText().toFloat(&leftMarginOk);
                else if (m_e.name() == "right-margin")
                    rm = m_e.readElementText().toFloat(&rightMarginOk);
                else if (m_e.name() == "top-margin")
                    tm = m_e.readElementText().toFloat(&topMarginOk);
                else if (m_e.name() == "bottom-margin")
                    bm = m_e.readElementText().toFloat(&bottomMarginOk);
                else
                    unexpectedElement();
            }
            pageLayout.twoSided = type == "odd" || type == "even";
            if (type == "odd" || type == "both") {
                pageLayout.oddLeftMargin = lm;
                pageLayout.oddRightMargin = rm;
                pageLayout.oddTopMargin = tm;
                pageLayout.oddBottomMargin = bm;
                pageLayout.oddMarginsRead = leftMarginOk && rightMarginOk && topMarginOk && bottomMarginOk;
            }
            if (type == "even" || type == "both") {
                pageLayout.evenLeftMargin = lm;
                pageLayout.evenRightMargin = rm;
                pageLayout.evenTopMargin = tm;
                pageLayout.evenBottomMargin = bm;
                pageLayout.evenMarginsRead = leftMarginOk && rightMarginOk && topMarginOk && bottomMarginOk;
            }
        }
        else if (m_e.name() == "page-height") {
            pageLayout.pageHeight = m_e.readElementText().toFloat(&pageHeightOk);
        }
        else if (m_e.name() == "page-width") {
            pageLayout.pageWidth = m_e.readElementText().toFloat(&pageWidthOk);
        }
        else {
            unexpectedElement();
        }
    }
    pageLayout.pageSizeRead = pageHeightOk && pageWidthOk;
    read = true;
    return pageLayout;
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
            int alter;
            bool ok;
            alter = m_e.readElementText().toInt(&ok);
            if (ok) {
                pitch.alter = alter;
            }
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

Rights MxmlParser::parseRights()
{
    Rights rights;
    rights.type = m_e.attributes().value("type").toUtf8().data();
    rights.text = m_e.readElementText().toUtf8().data();
    return rights;
}

Scaling MxmlParser::parseScaling(bool& read)
{
    // TODO error handling
    Scaling scaling;
    bool millimetersOk { false };
    bool tenthsOk { false };
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "millimeters") {
            scaling.millimeters = m_e.readElementText().toFloat(&millimetersOk);
        }
        else if (m_e.name() == "tenths") {
            scaling.tenths = m_e.readElementText().toFloat(&tenthsOk);
        }
        else {
            unexpectedElement();
        }
    }
    if (millimetersOk && tenthsOk) {
        read = true;
    }
    return scaling;
}

ScoreInstrument MxmlParser::parseScoreInstrument()
{
    // TODO: verify (and handle) non-empty attribute id
    ScoreInstrument scoreInstrument;
    scoreInstrument.id = std::string { m_e.attributes().value("id").toUtf8().data() };
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "instrument-name") {
            scoreInstrument.instrumentName = m_e.readElementText().toUtf8().data();
        }
        else if (m_e.name() == "instrument-sound") {
            scoreInstrument.instrumentSound = m_e.readElementText().toUtf8().data();
        }
        else if (m_e.name() == "virtual-instrument") {
            while (m_e.readNextStartElement()) {
                if (m_e.name() == "virtual-library") {
                    scoreInstrument.virtualLibrary = m_e.readElementText().toUtf8().data();
                }
                else if (m_e.name() == "virtual-name") {
                    scoreInstrument.virtualName = m_e.readElementText().toUtf8().data();
                }
                else {
                    unexpectedElement();
                }
            }
        }
        else {
            unexpectedElement();
        }
    }
    return scoreInstrument;
}

ScorePart MxmlParser::parseScorePart()
{
    // TODO: verify (and handle) non-empty attribute id
    ScorePart scorePart;
    scorePart.id = std::string { m_e.attributes().value("id").toUtf8().data() };
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "midi-device") {
            scorePart.midiDevice = parseMidiDevice();
            scorePart.midiDeviceRead = true;
        }
        else if (m_e.name() == "midi-instrument") {
            scorePart.midiInstruments.push_back(parseMidiInstrument());
        }
        else if (m_e.name() == "part-abbreviation") {
            scorePart.partAbbreviationPrintObject = !(m_e.attributes().value("print-object") == "no");
            scorePart.partAbbreviation = m_e.readElementText().toUtf8().data();
        }
        else if (m_e.name() == "part-name") {
            scorePart.partName = parsePartName();
        }
        else if (m_e.name() == "score-instrument") {
            scorePart.scoreInstruments.push_back(parseScoreInstrument());
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
        if (m_e.name() == "credit") {
            m_data.scorePartwise.credits.push_back(parseCredit());
        }
        else if (m_e.name() == "defaults") {
            m_data.scorePartwise.defaults = parseDefaults(m_data.scorePartwise.defaultsRead);
        }
        else if (m_e.name() == "identification") {
            m_data.scorePartwise.identification = parseIdentification();
        }
        else if (m_e.name() == "movement-number") {
            m_data.scorePartwise.movementNumber = m_e.readElementText().toUtf8().data();
        }
        else if (m_e.name() == "movement-title") {
            m_data.scorePartwise.movementTitle = m_e.readElementText().toUtf8().data();
        }
        else if (m_e.name() == "part") {
            m_data.scorePartwise.parts.push_back(parsePart());
        }
        else if (m_e.name() == "part-list") {
            parsePartList();
        }
        else if (m_e.name() == "work") {
            m_data.scorePartwise.work = parseWork();
        }
        else {
            unexpectedElement();
        }
    }
}

std::unique_ptr<Sound> MxmlParser::parseSound()
{
    // TODO add error detection and handling
    std::unique_ptr<Sound> sound(new Sound);
    sound->capo = std::string { m_e.attributes().value("capo").toUtf8().data() };
    sound->coda = std::string { m_e.attributes().value("coda").toUtf8().data() };
    sound->dacapo = std::string { m_e.attributes().value("dacapo").toUtf8().data() };
    sound->dalsegno = std::string { m_e.attributes().value("dalsegno").toUtf8().data() };
    sound->dynamics = std::string { m_e.attributes().value("dynamics").toUtf8().data() };
    sound->fine = std::string { m_e.attributes().value("fine").toUtf8().data() };
    sound->segno = std::string { m_e.attributes().value("segno").toUtf8().data() };
    sound->tempo = m_e.attributes().value("tempo").toFloat();
    m_e.skipCurrentElement();
    return sound;
}

StaffLayout MxmlParser::parseStaffLayout()
{
    StaffLayout staffLayout;
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "staff-distance") {
            staffLayout.staffDistance = m_e.readElementText().toFloat(&staffLayout.staffDistanceRead);
        }
        else {
            unexpectedElement();
        }
    }
    return staffLayout;
}

Supports MxmlParser::parseSupports()
{
    Supports supports;
    supports.attribute = std::string { m_e.attributes().value("attribute").toUtf8().data() };
    supports.element = std::string { m_e.attributes().value("element").toUtf8().data() };
    supports.type = std::string { m_e.attributes().value("type").toUtf8().data() };
    supports.value = std::string { m_e.attributes().value("value").toUtf8().data() };
    m_e.skipCurrentElement();
    return supports;
}

SystemLayout MxmlParser::parseSystemLayout()
{
     SystemLayout systemLayout;
     while (m_e.readNextStartElement()) {
         if (m_e.name() == "system-distance") {
             systemLayout.systemDistance = m_e.readElementText().toFloat(&systemLayout.systemDistanceRead);
         }
         else {
             unexpectedElement();
         }
     }
     return systemLayout;
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

Transpose MxmlParser::parseTranspose()
{
    Transpose transpose;
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "chromatic") {
            transpose.chromatic = m_e.readElementText().toInt();
        }
        else if (m_e.name() == "diatonic") {
            transpose.diatonic = m_e.readElementText().toInt();
        }
        else if (m_e.name() == "octave-change") {
            transpose.octaveChange = m_e.readElementText().toInt();
        }
        else {
            unexpectedElement();
        }
    }
    return transpose;
}

Work MxmlParser::parseWork()
{
    Work work;
    while (m_e.readNextStartElement()) {
        if (m_e.name() == "work-number") {
            work.workNumber = m_e.readElementText().toUtf8().data();
        }
        else if (m_e.name() == "work-title") {
            work.workTitle = m_e.readElementText().toUtf8().data();
        }
        else {
            unexpectedElement();
        }
    }
    return work;
}

void MxmlParser::unexpectedElement()
{
    const auto text = QString { "found unexpected element '%1'" }
            .arg(m_e.name().toString());
    log_warning(text);
    m_e.skipCurrentElement();
}

} // namespace MusicXML
