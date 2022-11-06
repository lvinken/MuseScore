#ifndef MXMLPARSER_H
#define MXMLPARSER_H

#include <optional>

#include <QIODevice>
#include <QXmlStreamReader>

#include "mxmldata.h"

namespace MusicXML {

class MxmlParser
{
public:
    int parse(QIODevice* data, const QString& filename);
    const MxmlData& getData() { return m_data; }

private:
    void log_error(const QString& text);
    void log_warning(const QString& text);
    bool parse();
    Accidental parseAccidental();
    std::unique_ptr<Attributes> parseAttributes();
    std::unique_ptr<Backup> parseBackup();
    std::unique_ptr<Barline> parseBarline();
    std::pair<unsigned int, Clef> parseClef();
    Creator parseCreator();
    Credit parseCredit();
    Defaults parseDefaults(bool& read);
    unsigned int parseDivisions();
    Encoding parseEncoding();
    Font parseFont();
    std::unique_ptr<Forward> parseForward();
    Identification parseIdentification();
    Key parseKey();
    Measure parseMeasure();
    MidiDevice parseMidiDevice();
    MidiInstrument parseMidiInstrument();
    Lyric parseLyric();
    Notations parseNotations();
    std::unique_ptr<Note> parseNote();
    PageLayout parsePageLayout(bool& read);
    Part parsePart();
    void parsePartList();
    std::string parsePartName();
    Pitch parsePitch();
    Rights parseRights();
    Scaling parseScaling(bool& read);
    ScoreInstrument parseScoreInstrument();
    ScorePart parseScorePart();
    std::unique_ptr<Sound> parseSound();
    StaffLayout parseStaffLayout();
    Supports parseSupports();
    SystemLayout parseSystemLayout();
    void parseScorePartwise();
    Time parseTime();
    TimeModification parseTimeModification();
    Transpose parseTranspose();
    std::unique_ptr<Tuplet> parseTuplet();
    TupletPortion parseTupletPortion();
    Work parseWork();
    void unexpectedElement();

    MxmlData m_data;
    QXmlStreamReader m_e;
    QString m_filename;
};

} // namespace MusicXML

#endif // MXMLPARSER_H
