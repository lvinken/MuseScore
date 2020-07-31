#ifndef XMLSTREAMREADER_H
#define XMLSTREAMREADER_H

#include <set>

//#include <QDebug>
#include <QStringList>
#include <QTextStream>
#include <QXmlStreamReader>

class XmlStreamReader
{
public:
    XmlStreamReader() {}
    const std::set<QString>& handledElements() const { return m_handled_elements; }
    const std::set<QString>& ignoredElements() const { return m_ignored_elements; }
    const std::set<QString>& unknownElements() const { return m_unknown_elements; }
    void setDevice(QIODevice* device) { m_qxmlr.setDevice(device); }
    bool readNextStartElement();
    QXmlStreamAttributes attributes() const { return m_qxmlr.attributes(); }
    QStringRef name() const { return m_qxmlr.name(); }
    QString handleElementText();
    void handleEmptyElement();
    void handleIgnoredChild();
    void handleUnknownChild();
    bool isEndElement() const { return m_qxmlr.isEndElement(); }
    bool isStartElement() const { return m_qxmlr.isStartElement(); }
    QString errorString() const;
    int pathDepth() const { return m_path.size(); }
private:
    QXmlStreamReader::TokenType readNext();
    void handleElementsCommon(const char* const functionName, std::set<QString>& elements1,
                              std::set<QString>& elements2);
    void updateHandledElements();
    std::set<QString> m_unknown_elements;
    std::set<QString> m_handled_elements;
    std::set<QString> m_ignored_elements;
    QStringList m_path;
    QXmlStreamReader m_qxmlr;
};

#endif // XMLSTREAMREADER_H
