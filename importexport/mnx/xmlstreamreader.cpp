#include <QLoggingCategory>

#include <qdebug.h>

#include "xmlstreamreader.h"

static QString attr2str(const QXmlStreamAttributes& attrs)
{
    QString res;
    for (auto attr: attrs) {
        if (res != "") {
            res += " ";
        }
        res += attr.name().toString() + ":" + attr.value().toString();
    }
    return res;
}

static QString attr2sortedstr(const QXmlStreamAttributes& attrs)
{
    QMap<QString, QString> map;
    QString res;
    for (auto attr: attrs) {
        map.insert(attr.name().toString(), attr.value().toString());
    }
    for (auto attr: map.keys()) {
        res += " " + attr + "=\"" + map.value(attr) + "\"";
    }
    return res;
}

static void dumpState(const QXmlStreamReader& xmlr, const QString& path)
{
#if 0
    qDebug()
        << "token type" << xmlr.tokenType()
        << "string" << xmlr.tokenString()
    ;
    if (xmlr.isCharacters()) {
        qDebug()
            << " "
            << "text" << xmlr.text()
            << "white-space" << xmlr.isWhitespace()
            << "CDATA" << xmlr.isCDATA()
        ;
    } else if (xmlr.isDTD()) {
        qDebug()
            << " "
            << "DTD" << xmlr.text()
        ;
    } else if (xmlr.isEndDocument()) {
        // ignore
    } else if (xmlr.isEndElement()) {
        qDebug()
            << " "
            << "name" << xmlr.name()
        ;
    } else if (xmlr.isStartDocument()) {
        qDebug()
            << " "
            << "document version" << xmlr.documentVersion()
            << "encoding" << xmlr.documentEncoding()
            << "standalone" << xmlr.isStandaloneDocument()
        ;
    } else if (xmlr.isStartElement()) {
        qDebug()
            << " "
            << "path" << path
            << "name" << xmlr.name()
            << "attributes" << attr2str(xmlr.attributes())
        ;
    } else {
        qDebug() << "dumpState() TODO";
    }
#endif
}

static void logDebugTrace(const QString& s)
{
    QLoggingCategory trace("importxml.trace");
    qCDebug(trace) << s;
}

static void logElementIgnored(const QString& s)
{
    QLoggingCategory trace("importxml.ignored");
    qCDebug(trace) << s;
}

static void logReadNext(const QString& s)
{
    //QLoggingCategory trace("importxml.readnext");
    //qCDebug(trace) << s;
}

QString XmlStreamReader::errorString() const
{
    if (m_qxmlr.hasError()) {
        return QString("error at line %1 column %2: ").arg(m_qxmlr.lineNumber()).arg(m_qxmlr.columnNumber())
               + m_qxmlr.errorString();
    } else {
        return "";
    }
}

// move to the next token and maintain the path
// Note startElement / endElement mismatch handled by QXmlStreamReader.
// results in QXmlStreamReader::TokenType QXmlStreamReader::Invalid
// (either start = name1 and end is name2 or start missing or end missing)

QXmlStreamReader::TokenType XmlStreamReader::readNext()
{
    /*
    logReadNext(QString("begin tokenString '%1' name '%2' path '%3'")
            .arg(m_qxmlr.tokenString())
            .arg(m_qxmlr.name().toString())
            .arg(m_path.join("/"))
            );
            */
    const auto res = m_qxmlr.readNext();
    if (m_qxmlr.isStartElement()) {
        m_path.append(m_qxmlr.name().toString());
        //        m_handled_elements.insert(m_path.join("/"));
    } else if (m_qxmlr.isEndElement()) {
        m_path.removeLast();
    }
    logReadNext(QString("tokenString '%1' name '%2' path '%3'")
                .arg(m_qxmlr.tokenString())
                .arg(m_qxmlr.name().toString())
                .arg(m_path.join("/"))
                );
    return res;
}

bool XmlStreamReader::readNextStartElement()
{
    qDebug()
        << "readNextStartElement()"
        << "begin"
        << "name:" << m_qxmlr.name().toString()
        << "tokenString:" << m_qxmlr.tokenString()
        << "line" << m_qxmlr.lineNumber()
        << "column" << m_qxmlr.columnNumber()
        << "->"
        << "path:" << m_path.join("/")
    ;
    while (!m_qxmlr.atEnd()) {
        readNext();
        if (m_qxmlr.isStartElement()) {
            qDebug()
                << "readNextStartElement()"
                << "startElement"
                << "name:" << m_qxmlr.name().toString()
                << "->"
                << "path:" << m_path.join("/")
                << "return true"
            ;
            return true;
        } else if (m_qxmlr.isEndElement()) {
            qDebug()
                << "readNextStartElement()"
                << "endElement"
                << "name:" << m_qxmlr.name().toString()
                << "->"
                << "path:" << m_path.join("/")
                << "return false"
            ;
            return false;
        }
    }
    qDebug()
        << "readNextStartElement()"
        << "tokenString:" << m_qxmlr.tokenString()
        << "atEnd" << m_qxmlr.atEnd()
        << "hasError" << m_qxmlr.hasError()
        << "name:" << m_qxmlr.name().toString()
        << "->"
        << "path:" << m_path.join("/")
        << "return false"
    ;
    return false;
}

// to be called when StartElement has been read
// returns element text and insert any child elements found into unknown_elements

QString XmlStreamReader::handleElementText()
{
    qDebug()
        << "handleElementText()"
        << "begin"
        << "name:" << m_qxmlr.name().toString()
        << "tokenString:" << m_qxmlr.tokenString()
        << "->"
        << "path" << m_path.join("/")
    ;

    updateHandledElements();
    const auto name = m_path.last();
    QString res;
    while (!m_qxmlr.atEnd()) {
        readNext();
        if (m_qxmlr.isStartElement()) {
            const auto childname = m_qxmlr.name().toString();
            handleUnknownChild();
            qDebug()
                << "handleElementText()"
                << "startElement"
                << "unknown child name:" << childname
                << "name:" << m_qxmlr.name().toString()
                << "tokenString:" << m_qxmlr.tokenString()
            ;
            m_qxmlr.raiseError("Unexpected child element");
            qDebug()
                << "handleElementText()"
                << "startElement"
                << "unknown child name:" << childname
                << "name:" << m_qxmlr.name().toString()
                << "tokenString:" << m_qxmlr.tokenString()
            ;
            return "";
        } else if (m_qxmlr.isCharacters()) {
            const auto text = m_qxmlr.text().toString();
            res += text;
            qDebug()
                << "handleElementText()"
                << "characters"
                << "text:" << text
                << "->"
                << "path:" << m_path.join("/")
                << "res:" << res
            ;
        } else if (m_qxmlr.isEndElement()) {
            if (!m_path.isEmpty()) {
                if (m_qxmlr.name() == name) {
                    qDebug()
                        << "handleElementText()"
                        << "endElement"
                        << "name:" << name
                        << "->"
                        << "path:" << m_path.join("/")
                        << "res:" << res
                    ;
                    return res;
                }
            }
        }
    }
    m_qxmlr.raiseError("Unexpected token type");
    qDebug()
        << "handleElementText()"
        << "name:" << m_qxmlr.name().toString()
        << "tokenString:" << m_qxmlr.tokenString()
        << "error exit"
        << "->"
        << "path:" << m_path.join("/")
        << "res:" << res
    ;
    return res;
}

// read until the end element of the current element (i.e. the one at the top of the stack)
// add the current element to the handled elements and its children to the unknown elements

static void updateElements(const QStringList& path, std::set<QString>& elements)
{
    elements.insert(path.join("/"));
}

void XmlStreamReader::handleElementsCommon(const char* const functionName, std::set<QString>& elements1,
                                           std::set<QString>& elements2)
{
    qDebug()
        << functionName
        << "begin"
        << "name:" << m_qxmlr.name().toString()
        << "tokenString:" << m_qxmlr.tokenString()
        << "->"
        << "path" << m_path.join("/")
    ;
    if (m_path.isEmpty()) {
        qDebug()
            << functionName
            << "return"
        ;
        return;
    }
    updateElements(m_path, elements1);
    const auto name = m_path.last();
    while (!m_qxmlr.atEnd()) {
        readNext();
        if (m_qxmlr.isStartElement()) {
            qDebug()
                << functionName
                << "startElement"
                << "name:" << name
                << "->"
                << "path:" << m_path.join("/")
            ;
            updateElements(m_path, elements2);
        } else if (m_qxmlr.isEndElement()) {
            if (!m_path.isEmpty()) {
                if (m_qxmlr.name() == name) {
                    qDebug()
                        << functionName
                        << "endElement"
                        << "name:" << name
                        << "->"
                        << "path:" << m_path.join("/")
                    ;
                    return;
                }
            }
        }
    }
    qDebug()
        << functionName
        << "name:" << name
        << "tokenString:" << m_qxmlr.tokenString()
        << "error exit"
        << "->"
        << "path:" << m_path.join("/")
    ;
}

// read until the end element of the current element (i.e. the one at the top of the stack)
// add the current element to the handled elements and its children to the unknown elements

void XmlStreamReader::handleEmptyElement()
{
    handleElementsCommon("handleEmptyElement()", m_handled_elements, m_unknown_elements);
}

// read until the end element of the current element (i.e. the one at the top of the stack)
// add the current element and its children to the ignored elements

void XmlStreamReader::handleIgnoredChild()
{
    handleElementsCommon("handleIgnoredChild()", m_ignored_elements, m_ignored_elements);
}

// read until the end element of the current element (i.e. the one at the top of the stack)
// add the current element and its children to the unknown elements

void XmlStreamReader::handleUnknownChild()
{
    handleElementsCommon("handleUnknownChild()", m_unknown_elements, m_unknown_elements);
}

void XmlStreamReader::updateHandledElements()
{
    for (int i = 0; i < m_path.size(); ++i) {
        m_handled_elements.insert(m_path.mid(0, i + 1).join("/"));
    }
}
