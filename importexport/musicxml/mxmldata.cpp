/****************************************************************************
**
** mxmldata.cpp -- implementation of TBD
**
****************************************************************************/

#include "mxmldata.h"

namespace MusicXML {

Attributes::Attributes()
    : Element(ElementType::ATTRIBUTES)
{
    // nothing
}

std::string Attributes::toString() const
{
    std::string result;
    result += "   attributes";
    if (divisions) {
        result += "\n    divisions \"" + std::to_string(divisions) + "\"";
    }
    for (const auto& key : keys) {
        result += "\n    key";
        result += "\n     fifths \"" + std::to_string(key.fifths) + "\"";
    }
    for (const auto& time : times) {
        result += "\n    time";
        if (!time.beats.empty()) {
            result += "\n     beats \"" + time.beats + "\"";
        }
        if (!time.beatType.empty()) {
            result += "\n     beat-type \"" + time.beatType + "\"";
        }
    }
    for (const auto& clef : clefs) {
        result += "\n    clef";
        if (!clef.sign.empty()) {
            result += "\n     sign \"" + clef.sign + "\"";
        }
        if (clef.line) {
            result += "\n     line \"" + std::to_string(clef.line) + "\"";
        }
    }
    return result;
}

Backup::Backup()
    : Element(ElementType::BACKUP)
{
    // nothing
}

std::string Backup::toString() const
{
    std::string result;
    result += "   backup";
    if (duration) {
        result += "\n    duration \"" + std::to_string(duration) + "\"";
    }
    return result;
}

Clef::Clef()
    : Element(ElementType::CLEF)
{
    // nothing
}

Element::Element(const ElementType& p_elementType)
{
    elementType = p_elementType;
}

Forward::Forward()
    : Element(ElementType::FORWARD)
{
    // nothing
}

std::string Forward::toString() const
{
    std::string result;
    result += "   forward";
    if (duration) {
        result += "\n    duration \"" + std::to_string(duration) + "\"";
    }
    return result;
}

Key::Key()
    : Element(ElementType::KEY)
{
    // nothing
}

Measure::Measure()
    : Element(ElementType::MEASURE)
{
    // nothing
}

std::string Measure::toString() const
{
    std::string result;
    result += "\n  measure number=\"" + number + "\"";
    for (const auto& element : elements) {
        result += "\n" + element->toString();
    }
    return result;
}

Note::Note()
    : Element(ElementType::NOTE)
{
    // nothing
}

std::string Note::toString() const
{
    std::string result;
    result += "   note";
    if (grace) {
        result += "\n    grace";
    }
    if (chord) {
        result += "\n    chord";
    }
    if (rest) {
        result += "\n    rest measure=\"";
        result += measureRest ? "yes" : "no";
        result += "\"";
    }
    else {
        result += "\n    pitch";
        result += "\n     step \"" + std::string { pitch.step } + "\"";
        result += "\n     octave \"" + std::to_string(pitch.octave) + "\"";
    }
    if (duration) {
        result += "\n    duration \"" + std::to_string(duration) + "\"";
    }
    if (!voice.empty()) {
        result += "\n    voice \"" + voice + "\"";
    }
    if (!type.empty()) {
        result += "\n    type \"" + type + "\"";
    }
    for (unsigned int i = 0; i < dots; ++i) {
        result += "\n    dot";
    }
    if (timeModification.isValid()) {
        result += "\n    time-modification \"";
        result += timeModification.print().toStdString();
        result += "\"";
    }
    return result;
}

Part::Part()
    : Element(ElementType::PART)
{
    // nothing
}

std::string Part::toString() const
{
    std::string result;
    result += "\n part id=\"" + id + "\"";
    for (const auto& measure : measures) {
        result += measure.toString();
    }
    return result;
}

PartList::PartList()
    : Element(ElementType::PARTLIST)
{
    // nothing
}

std::string PartList::toString() const
{
    std::string result;
    result += "\n part-list";
    for (const auto& scorePart : scoreParts) {
        result += scorePart.toString();
    }
    return result;
}

Pitch::Pitch()
    : Element(ElementType::PITCH)
{
    // nothing
}

ScorePart::ScorePart()
    : Element(ElementType::SCOREPART)
{
    // nothing
}

std::string ScorePart::toString() const
{
    std::string result;
    result += "\n  score-part id=\"" + id + "\"";
    result += "\n   part-name \"" + partName + "\"";
    return result;
}

ScorePartwise::ScorePartwise()
    : Element(ElementType::SCOREPARTWISE)
{
    // nothing
}

std::string ScorePartwise::toString() const
{
    std::string result;
    result += "score-partwise version=\"" + version + "\"";
    result += partList.toString();
    for (const auto& part : parts) {
        result += part.toString();
    }
    return result;
}

Time::Time()
    : Element(ElementType::TIME)
{
    // nothing
}

TimeModification::TimeModification()
    : Element(ElementType::TIMEMODIFICATION)
{
    // nothing
}

std::string TimeModification::toString() const
{
    std::string result;
    result += "\n    time-modification";
    result += "\n     actual-notes \"" + std::to_string(actualNotes) + "\"";
    result += "\n     normal-notes \"" + std::to_string(normalNotes) + "\"";
    return result;
}

} // namespace MusicXML
