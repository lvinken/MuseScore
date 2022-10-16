#ifndef MXMLDATA_H
#define MXMLDATA_H

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace MusicXML {

enum class ElementType {
    INVALID = 0,
    ATTRIBUTES,
    BACKUP,
    BARLINE,
    CLEF,
    CREDIT,
    CREDITWORDS,
    DEFAULTS,
    ELEMENT,
    FORWARD,
    KEY,
    LYRIC,
    MEASURE,
    NOTE,
    PAGELAYOUT,
    PART,
    PARTLIST,
    PITCH,
    SCALING,
    SCOREPART,
    SCOREPARTWISE,
    SOUND,
    TIME,
    TIMEMODIFICATION,
};

struct Note;
struct ScorePart;

struct Element {
    Element(const ElementType& p_ElementType);
    virtual ~Element() {};
    ElementType elementType { ElementType::INVALID };
    virtual std::string toString() const { return ""; }
};

struct CreditWords : public Element {
    CreditWords();
    float defaultX { 0.0 }; // TODO make optional
    float defaultY { 0.0 }; // TODO make optional
    float fontSize { 0.0 }; // TODO make optional
    std::string justify;    // TODO make type safe
    std::string halign;     // TODO make type safe
    std::string valign;     // TODO make type safe
    std::string text;
};

struct Credit : public Element {
    Credit();
    std::vector<std::string> creditTypes;
    std::vector<CreditWords> creditWordses;
    unsigned int page { 0 };
    std::string toString() const;
};

struct Clef : public Element {
    Clef();
    int line { 0 };
    std::string sign;   // TODO make type safe
};

struct Key : public Element {
    Key();
    int fifths { 0 };   // TODO exactly one must be present
};

struct Time : public Element {
    Time();
    // TODO both beats and beat-type must be present one or more times
    std::string beats;
    std::string beatType;
};

struct Transpose {
    int chromatic { 0 };      // TODO support microtones ?
    int diatonic { 0 };       // optional, missing means 0
    int octaveChange { 0 };   // optional, missing means 0
    std::string toString() const;
};

struct Attributes : public Element {
    Attributes();
    std::map<unsigned int, Clef> clefs;
    unsigned int divisions { 0 }; // TODO make optional
    std::vector<Key> keys;
    unsigned int staves { 1 }; // TODO make optional
    std::vector<Time> times;
    Transpose transpose; // TODO make optional
    bool transposeRead { false };
    std::string toString() const;
};

struct Backup : public Element {
    Backup();
    std::string toString() const;
    unsigned int duration { 0 };
};

struct Barline : public Element {
    Barline();
    std::string location;
    std::string barStyle;
    std::string endingNumber;
    std::string endingType;
    std::string endingText;
    std::string repeatDirection;
    int repeatTimes { 0 }; // TODO make optional nonNegativeInteger
    std::string toString() const;
};

struct PageLayout : public Element {
    PageLayout();
    float pageHeight { 0.0 };
    float pageWidth { 0.0 };
    bool pageSizeRead { false };
    float evenLeftMargin { 0.0 };
    float evenRightMargin { 0.0 };
    float evenTopMargin { 0.0 };
    float evenBottomMargin { 0.0 };
    bool evenMarginsRead { false };
    float oddLeftMargin { 0.0 };
    float oddRightMargin { 0.0 };
    float oddTopMargin { 0.0 };
    float oddBottomMargin { 0.0 };
    bool oddMarginsRead { false };
    bool twoSided { false };
    std::string toString() const;
};

struct Scaling : public Element {
    Scaling();
    float millimeters { 0.0 };
    float tenths { 0.0 };
    std::string toString() const;
};

struct Font {
    std::string fontFamily;
    std::string fontSize;
    std::string toString() const;
};

struct StaffLayout {
    float staffDistance { 0.0 };  // TODO make optional
    bool staffDistanceRead { false };
    std::string toString() const;
};

struct SystemLayout {
    float systemDistance { 0.0 }; // TODO make optional
    bool systemDistanceRead { false };
    std::string toString() const;
};

struct Defaults : public Element {
    Defaults();
    Font lyricFont;      // TODO make optional
    bool lyricFontRead { false };
    Scaling scaling;       // TODO make optional
    bool scalingRead { false };
    PageLayout pageLayout; // TODO make optional
    bool pageLayoutRead { false };
    StaffLayout staffLayout;      // TODO make optional ?
    SystemLayout systemLayout;    // TODO make optional ?
    Font wordFont;      // TODO make optional
    bool wordFontRead { false };
    std::string toString() const;
};

struct Forward : public Element {
    Forward();
    std::string toString() const;
    unsigned int duration { 0 };
};

struct Creator {
    std::string text;
    std::string type;
    std::string toString() const;
};

struct Supports {
    std::string attribute;
    std::string element;
    std::string type;
    std::string value;
    std::string toString() const;
};

struct Encoding {
    std::string encodingDate;
    std::string software;
    std::vector<Supports> supportses;
    std::string toString() const;
};

struct Rights {
    std::string text;
    std::string type;
    std::string toString() const;
};

// TODO: make optional ?
struct Identification {
    std::vector<Creator> creators;
    Encoding encoding;
    std::vector<Rights> rightses;
    std::string source;
    std::string toString() const;
};

// Parent elements: <direction>, <measure> (partwise), <part> (timewise)
struct Sound : public Element {
    Sound();
    std::string capo;
    std::string coda;
    std::string dacapo;
    std::string dalsegno;
    std::string dynamics;
    std::string fine;
    std::string segno;
    float tempo { 0.0 };  // TODO: make optional
    std::string toString() const;
};

struct Measure : public Element {
    Measure();
    std::vector<std::unique_ptr<Element>> elements;
    std::string number;
    std::string toString() const;
};

struct MidiDevice {
    std::string id;
    int port { 0 }; // TODO: make optional
    bool portRead { false };
    std::string toString() const;
};

struct MidiInstrument {
    std::string id;
    int midiChannel { 0 };  // TODO: make optional
    bool midiChannelRead { false };
    int midiProgram { 0 };  // TODO: make optional
    bool midiProgramRead { false };
    int midiUnpitched { 0 };  // TODO: make optional
    bool midiUnpitchedRead { false };
    float pan { 0.0 };  // TODO: make optional
    bool panRead { false };
    float volume { 0.0 };  // TODO: make optional
    bool volumeRead { false };
    std::string toString() const;
};

struct Lyric : public Element {
    Lyric();
    std::string number;
    std::string text;
    std::string toString() const;
};

struct Pitch : public Element {
    Pitch();
    int alter { 0 }; // TODO support microtones
    unsigned int octave { 4 };
    char step { 'C' };  // TODO make type safe
};

struct TimeModification : public Element {
    TimeModification();
    unsigned int actualNotes { 1 };
    bool isValid() const;
    unsigned int normalNotes { 1 };
    std::string toString() const;
};

struct Note : public Element {
    Note();
    std::string beam; // TODO: make type-safe, support multiple beams
    bool chord { false };
    bool cue { false };
    unsigned int dots { 0 };
    unsigned int duration { 0 };
    bool grace { false };
    std::string instrument; // TODO: support multiple instrument
    std::vector<Lyric> lyrics;
    bool measureRest { false };
    std::string noteheadColor; // TODO: make type-safe
    std::string noteheadFilled; // TODO: this is an optional yes-no, when no longer a string, make optional
    std::string noteheadParentheses; // TODO: this is an optional yes-no, when no longer a string, make optional
    std::string noteheadText;
    Pitch pitch;            // TODO: make optional ?
    bool rest { false };    // TODO: support display-step and display-octave
    unsigned int staff { 1 };
    std::string stem; // TODO: make type-safe
    TimeModification timeModification;
    std::string toString() const;
    std::string type;
    std::string typeSize;
    std::string voice;
};

struct Part : public Element {
    Part();
    std::string id;
    std::vector<Measure> measures;
    std::string toString() const;
};

struct PartList : public Element {
    PartList();
    std::vector<ScorePart> scoreParts;
    std::string toString() const;
};

struct ScoreInstrument {
    std::string id;
    std::string instrumentName;
    std::string instrumentSound;
    std::string virtualLibrary;
    std::string virtualName;
    std::string toString() const;
};

struct ScorePart : public Element {
    ScorePart();
    std::string id;
    MidiDevice midiDevice; // TODO: there may be zero or more than one ...
    bool midiDeviceRead { false };
    std::vector<MidiInstrument> midiInstruments;
    std::string partAbbreviation;
    bool partAbbreviationPrintObject { true };
    std::string partName;
    std::vector<ScoreInstrument> scoreInstruments;
    std::string toString() const;
};

struct Work {
    std::string workNumber;
    std::string workTitle;
    std::string toString() const;
};

struct ScorePartwise : public Element {
    ScorePartwise();
    std::vector<Credit> credits;
    Defaults defaults;
    bool defaultsRead { false };
    Identification identification;
    bool isFound { false };
    std::string movementNumber;
    std::string movementTitle;
    PartList partList;
    std::vector<Part> parts;
    std::string version { "1.0" };
    Work work;
    std::string toString() const;
};

struct MxmlData {
    ScorePartwise scorePartwise;
};

} // namespace MusicXML

#endif // MXMLDATA_H
