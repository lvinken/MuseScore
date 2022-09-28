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
    result += "\n    staves \"" + std::to_string(staves) + "\"";
    for (const auto& pair : clefs) {
        result += "\n    clef number=\"" + std::to_string(pair.first + 1) + "\"";
        if (!pair.second.sign.empty()) {
            result += "\n     sign \"" + pair.second.sign + "\"";
        }
        if (pair.second.line) {
            result += "\n     line \"" + std::to_string(pair.second.line) + "\"";
        }
    }
    return result;
}

Backup::Backup()
    : Element(ElementType::BACKUP)
{
    // nothing
}

Credit::Credit()
    : Element(ElementType::CREDIT)
{
    // nothing
}

std::string Credit::toString() const
{
    std::string result;
    result += "\n credit page=\"" + std::to_string(page + 1) + "\"";
    for (const auto& creditType : creditTypes) {
        result += "\n  credit-type \"" + creditType + "\"";
    }
    for (const auto& creditWords : creditWordses) {
        result += "\n  credit-words";
        if (creditWords.defaultX < -0.001 || creditWords.defaultX > 0.001) {
            result += " default-x=\"" + std::to_string(creditWords.defaultX) + "\"";
        }
        if (creditWords.defaultY < -0.001 || creditWords.defaultY > 0.001) {
            result += " default-y=\"" + std::to_string(creditWords.defaultY) + "\"";
        }
        if (creditWords.fontSize < -0.001 || creditWords.fontSize > 0.001) {
            result += " font-size=\"" + std::to_string(creditWords.fontSize) + "\"";
        }
        if (creditWords.halign.size() > 0) {
            result += " halign=\"" + creditWords.halign + "\"";
        }
        if (creditWords.justify.size() > 0) {
             result += " justify=\"" + creditWords.justify + "\"";
        }
        if (creditWords.valign.size() > 0) {
            result += " valign=\"" + creditWords.valign + "\"";
        }
        result += " \"" + creditWords.text + "\"";
    }
    return result;
}

CreditWords::CreditWords()
    : Element(ElementType::CREDITWORDS)
{
    // nothing
}

Defaults::Defaults()
    : Element(ElementType::DEFAULTS)
{
    // nothing
}

std::string Defaults::toString() const
{
    std::string result;
    result += "\n defaults";
    if (scalingRead) {
        result += scaling.toString();
    }
    if (pageLayoutRead) {
        result += pageLayout.toString();
    }
    return result;
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

Lyric::Lyric()
      : Element(ElementType::LYRIC)
{
    // nothing
}

std::string Lyric::toString() const
{
    std::string result;
    result += "\n    lyric number=\"" + number + "\"";
    result += "\n     text \"" + text + "\"";
    return result;
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
        if (pitch.alter) {
            result += "\n     alter \"" + std::to_string(pitch.alter) + "\"";
        }
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
        result += timeModification.toString();
    }
    result += "\n    staff \"" + std::to_string(staff) + "\"";
    for (const auto& lyric : lyrics) {
        result += lyric.toString();
    }
    return result;
}

PageLayout::PageLayout()
    : Element(ElementType::PAGELAYOUT)
{
    // nothing
}

std::string PageLayout::toString() const
    {
    std::string result;
    result += "\n  page-layout\"";
    if (pageSizeRead) {
        result += "\n   page-height \"" + std::to_string(pageHeight) + "\"";
        result += "\n   page-width \"" + std::to_string(pageWidth) + "\"";
    }
    if (evenMarginsRead) {
        result += "\n   even left-margin \"" + std::to_string(evenLeftMargin) + "\"";
        result += "\n   even right-margin \"" + std::to_string(evenRightMargin) + "\"";
        result += "\n   even top-margin \"" + std::to_string(evenTopMargin) + "\"";
        result += "\n   even bottom-margin \"" + std::to_string(evenBottomMargin) + "\"";
    }
    if (oddMarginsRead) {
        result += "\n   odd left-margin \"" + std::to_string(oddLeftMargin) + "\"";
        result += "\n   odd right-margin \"" + std::to_string(oddRightMargin) + "\"";
        result += "\n   odd top-margin \"" + std::to_string(oddTopMargin) + "\"";
        result += "\n   odd bottom-margin \"" + std::to_string(oddBottomMargin) + "\"";
    }
    result += "\n   two-sided \"" + std::string { twoSided ? "true" : "false" } + "\"";
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

Scaling::Scaling()
    : Element(ElementType::SCALING)
{
    // nothing
}

std::string Scaling::toString() const
{
    std::string result;
    result += "\n  scaling";
    result += "\n   millimeters \"" + std::to_string(millimeters) + "\"";
    result += "\n   tenths \"" + std::to_string(tenths) + "\"";
    return result;
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
    result += work.toString();
    if (!movementNumber.empty()) {
        result += "\n movement-number \"" + movementNumber + "\"";
    }
    if (!movementNumber.empty()) {
        result += "\n movement-title \"" + movementTitle + "\"";
    }
    if (defaultsRead) {
        result += defaults.toString();
    }
    for (const auto& credit : credits) {
        result += credit.toString();
    }
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

bool TimeModification::isValid() const
      {
      return !(actualNotes == 0 || normalNotes == 0 || (actualNotes == 1 && normalNotes == 1));
      }

std::string TimeModification::toString() const
{
    std::string result;
    result += "\n    time-modification";
    result += "\n     actual-notes \"" + std::to_string(actualNotes) + "\"";
    result += "\n     normal-notes \"" + std::to_string(normalNotes) + "\"";
    return result;
}

std::string Work::toString() const
{
    if (workNumber.empty() && workTitle.empty()) {
        return "";
    }
    std::string result;
    result += "\n work";
    if (!workNumber.empty()) {
        result += "\n  work-number \"" + workNumber + "\"";
    }
    if (!workTitle.empty()) {
        result += "\n  work-title \"" + workTitle + "\"";
    }
    return result;
}

} // namespace MusicXML
