/****************************************************************************
**
** mxmldata.cpp -- implementation of TBD
**
****************************************************************************/

#include "mxmldata.h"

namespace MusicXML {

std::string Accidental::toString() const
{
    std::string result;
    result += "\n    accidental";
    if (cautionary) {
        result += " cautionary=\"yes\"";
    }
    if (editorial) {
        result += " editorial=\"yes\"";
    }
    if (parentheses) {
        result += " parentheses=\"yes\"";
    }
    result += " \"" + text + "\"";
    return result;
}

Attributes::Attributes()
    : Element(ElementType::ATTRIBUTES)
{
    // nothing
}

std::string Attributes::toString() const
{
    std::string result;
    result += "\n   attributes";
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
    if (transposeRead) {
        result += transpose.toString();
    }
    return result;
}

Backup::Backup()
    : Element(ElementType::BACKUP)
{
    // nothing
}

Barline::Barline()
    : Element(ElementType::BARLINE)
{
    // nothing
}

std::string Barline::toString() const
{
    std::string result;
    result += "\n   barline";
    if (!location.empty()) {
        result += " location=\"" + location + "\"";
    }
    if (!barStyle.empty()) {
        result += "\n    bar-style \"" + barStyle + "\"";
    }
    if (!endingNumber.empty() || !endingText.empty() || !endingType.empty()) {
        result += "\n    ending";
        if (!endingNumber.empty()) {
            result += " number=\"" + endingNumber + "\"";
        }
        if (!endingType.empty()) {
            result += " type=\"" + endingType + "\"";
        }
        if (!endingText.empty()) {
            result += " \"" + endingText + "\"";
        }
    }
    if (!repeatDirection.empty() || repeatTimes > 0) {
        result += "\n    repeat";
        if (!repeatDirection.empty()) {
            result += " direction=\"" + repeatDirection + "\"";
        }
        if (repeatTimes > 0) {
            result += " times=\"" + std::to_string(repeatTimes) + "\"";
        }
    }
    return result;
}

std::string Creator::toString() const
{
    std::string result;
    result += "\n  creator";
    if (!type.empty()) {
        result += " type=\"" + type + "\"";
    }
    result += " \"" + text + "\"";
    return result;
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
    if (systemLayout.systemDistanceRead) {
        result += systemLayout.toString();
    }
    if (staffLayout.staffDistanceRead) {
        result += staffLayout.toString();
    }
    if (wordFontRead) {
        result += "\n  word-font" + wordFont.toString();
    }
    if (lyricFontRead) {
        result += "\n  lyric-font" + lyricFont.toString();
    }
    return result;
}

std::string Backup::toString() const
{
    std::string result;
    result += "\n   backup";
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

std::string Encoding::toString() const
{
    std::string result;
    result += "\n  encoding";
    if (!encodingDate.empty()) {
        result += "\n   encoding-date \"" + encodingDate + "\"";
    }
    if (!software.empty()) {
        result += "\n   software \"" + software + "\"";
    }
    for (const auto& supports : supportses) {
        result += supports.toString();
    }
    return result;
}

std::string Font::toString() const
{
    std::string result;
    if (!fontFamily.empty()) {
        result += " font-family=\"" + fontFamily + "\"";
    }
    if (!fontSize.empty()) {
        result += " font-size=\"" + fontSize + "\"";
    }
    return result;
}

Forward::Forward()
    : Element(ElementType::FORWARD)
{
    // nothing
}

std::string Forward::toString() const
{
    std::string result;
    result += "\n   forward";
    if (duration) {
        result += "\n    duration \"" + std::to_string(duration) + "\"";
    }
    return result;
}

std::string Identification::toString() const
{
    std::string result;
    result += "\n identification";
    for (const auto& creator : creators) {
         result += creator.toString();
    }
    for (const auto& rights : rightses) {
         result += rights.toString();
    }
    result += encoding.toString();
    if (!source.empty()) {
        result += "\n  " + source;
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
        result += element->toString();
    }
    return result;
}

std::string MidiDevice::toString() const
{
    std::string result;
    result += "\n   midi-device id=\"" + id + "\"";
    if (portRead) {
        result += " port=\"" + std::to_string(port + 1) + "\"";
    }
    return result;
}

std::string MidiInstrument::toString() const
{
    std::string result;
    result += "\n   midi-instrument id=\"" + id + "\"";
    if (midiChannelRead) {
        result += "\n    midi-channel \"" + std::to_string(midiChannel + 1) + "\"";
    }
    if (midiProgramRead) {
        result += "\n    midi-program \"" + std::to_string(midiProgram + 1) + "\"";
    }
    if (midiUnpitchedRead) {
        result += "\n    midi-unpitched \"" + std::to_string(midiUnpitched + 1) + "\"";
    }
    if (volumeRead) {
        result += "\n    volume \"" + std::to_string(static_cast<int>(volume + 0.5)) + "\"";
    }
    if (panRead) {
        result += "\n    pan \"" + std::to_string(static_cast<int>(pan + 0.5)) + "\"";
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
    result += "\n   note";
    if (grace) {
        result += "\n    grace";
    }
    if (cue) {
        result += "\n    cue";
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
    if (!instrument.empty()) {
        result += "\n    instrument \"" + instrument + "\"";
    }
    if (!voice.empty()) {
        result += "\n    voice \"" + voice + "\"";
    }
    if (!type.empty()) {
        result += "\n    type";
        if (!typeSize.empty()) {
            result += " size=\"" + typeSize + "\"";
        }
        result += " \"" + type + "\"";
    }
    for (unsigned int i = 0; i < dots; ++i) {
        result += "\n    dot";
    }
    if (accidentalRead) {
        result += accidental.toString();
    }
    if (timeModification.isValid()) {
        result += timeModification.toString();
    }
    if (!stem.empty()) {
        result += "\n    stem \"" + stem + "\"";
    }
    if (!noteheadColor.empty() || !noteheadFilled.empty() || !noteheadParentheses.empty() || !noteheadText.empty()) {
        result += "\n    notehead";
        if (!noteheadColor.empty()) {
            result += " color=\"" + noteheadColor + "\"";
        }
        if (!noteheadFilled.empty()) {
            result += " filled=\"" + noteheadFilled + "\"";
        }
        if (!noteheadParentheses.empty()) {
            result += " parentheses=\"" + noteheadParentheses + "\"";
        }
        if (!noteheadText.empty()) {
            result += " \"" + noteheadText + "\"";
        }
    }
    result += "\n    staff \"" + std::to_string(staff) + "\"";
    if (!beam.empty()) {
        result += "\n    beam number=\"1\" \"" + beam + "\"";
    }
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

std::string Supports::toString() const
{
    std::string result;
    result += "\n   supports";
    if (!attribute.empty()) {
        result += " attribute=\"" + attribute + "\"";
    }
    if (!element.empty()) {
        result += " element=\"" + element + "\"";
    }
    if (!type.empty()) {
        result += " type=\"" + type + "\"";
    }
    if (!value.empty()) {
        result += " value=\"" + value + "\"";
    }
    return result;
}

std::string Rights::toString() const
{
    std::string result;
    result += "\n  rights";
    if (!type.empty()) {
        result += " type=\"" + type + "\"";
    }
    result += " \"" + text + "\"";
    return result;
}

Sound::Sound()
    : Element(ElementType::SOUND)
    {
    // nothing
    }

std::string Sound::toString() const
{
    std::string result;
    result += "\n   sound";
    if (!capo.empty()) {
        result += " capo=\"" + capo + "\"";
    }
    if (!coda.empty()) {
        result += " coda=\"" + coda + "\"";
    }
    if (!dacapo.empty()) {
        result += " dacapo=\"" + dacapo + "\"";
    }
    if (!dalsegno.empty()) {
        result += " dalsegno=\"" + dalsegno + "\"";
    }
    if (!dynamics.empty()) {
        result += " dynamics=\"" + dynamics + "\"";
    }
    if (!fine.empty()) {
        result += " fine=\"" + fine + "\"";
    }
    if (!segno.empty()) {
        result += " segno=\"" + segno + "\"";
    }
    if (tempo > 0.001) {
        result += " tempo=\"" + std::to_string(tempo) + "\"";
    }
    return result;
}

std::string ScoreInstrument::toString() const
{
    std::string result;
    result += "\n   score-instrument id=\"" + id + "\"";
    if (!instrumentName.empty()) {
        result += "\n    instrument-name \"" + instrumentName + "\"";
    }
    if (!instrumentSound.empty()) {
        result += "\n    instrument-sound \"" + instrumentSound + "\"";
    }
    if (!virtualLibrary.empty() || !virtualName.empty()) {
        result += "\n    virtual-instrument";
        if (!virtualLibrary.empty()) {
            result += "\n     virtual-library \"" + virtualLibrary + "\"";
        }
        if (!virtualName.empty()) {
            result += "\n     virtual-name \"" + virtualName + "\"";
        }
    }
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
    if (!partAbbreviation.empty()) {
        result += "\n   part-abbreviation";
        if (!partAbbreviationPrintObject) {
            result += " print-object=\"no\"";
        }
        result += " \"" + partAbbreviation + "\"";
    }
    for (const auto& scoreInstrument : scoreInstruments) {
        result += scoreInstrument.toString();
    }
    if (midiDeviceRead) {
        result += midiDevice.toString();
    }
    for (const auto& midiInstrument : midiInstruments) {
        result += midiInstrument.toString();
    }
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
    result += identification.toString();
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

std::string StaffLayout::toString() const
{
    std::string result;
    if (staffDistanceRead) {
        result += "\n  staff-layout\"";
        result += "\n   staff-distance \"" + std::to_string(staffDistance) + "\"";
    }
    return result;
}

std::string SystemLayout::toString() const
{
    std::string result;
    if (systemDistanceRead) {
        result += "\n  system-layout\"";
        result += "\n   system-distance \"" + std::to_string(systemDistance) + "\"";
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

std::string Transpose::toString() const
{
    std::string result;
    result += "\n    transpose";
    result += "\n     diatonic \"" + std::to_string(diatonic) + "\"";
    result += "\n     chromatic \"" + std::to_string(chromatic) + "\"";
    result += "\n     octave-change \"" + std::to_string(octaveChange) + "\"";
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
