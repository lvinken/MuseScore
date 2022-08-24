#ifndef MXMLPARSER_H
#define MXMLPARSER_H

#include <optional>

#include <QIODevice>
#include <QXmlStreamReader>

#include "mxmldata.h"

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
    Clef parseClef();
    std::optional<unsigned int> parseDivisions();
    std::unique_ptr<Forward> parseForward();
    Key parseKey();
    Measure parseMeasure();
    std::unique_ptr<Note> parseNote();
    Part parsePart();
    void parsePartList();
    std::string parsePartName();
    Pitch parsePitch();
    ScorePart parseScorePart();
    void parseScorePartwise();
    Time parseTime();
    std::optional<TimeModification> parseTimeModification();
    void unexpectedElement();

    MxmlData m_data;
    QXmlStreamReader m_e;
    QString m_filename;
};

#endif // MXMLPARSER_H
