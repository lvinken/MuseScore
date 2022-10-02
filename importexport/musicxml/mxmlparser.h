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
    std::unique_ptr<Attributes> parseAttributes();
    std::unique_ptr<Backup> parseBackup();
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
    MidiInstrument parseMidiInstrument();
    Lyric parseLyric();
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
    Supports parseSupports();
    void parseScorePartwise();
    Time parseTime();
    TimeModification parseTimeModification();
    Work parseWork();
    void unexpectedElement();

    MxmlData m_data;
    QXmlStreamReader m_e;
    QString m_filename;
};

} // namespace MusicXML

#endif // MXMLPARSER_H
