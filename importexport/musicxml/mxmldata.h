#ifndef MXMLDATA_H
#define MXMLDATA_H

#include <optional>
#include <string>
#include <vector>

enum class ElementType {
    INVALID = 0,
    ATTRIBUTES,
    BACKUP,
    CLEF,
    ELEMENT,
    FORWARD,
    KEY,
    MEASURE,
    NOTE,
    PART,
    PARTLIST,
    PITCH,
    SCOREPART,
    SCOREPARTWISE,
    TIME,
    TIMEMODIFICATION,
};

struct Note;
struct ScorePart;

struct Element {
    Element(const ElementType& p_elementType);
    virtual ~Element() {};
    ElementType elementType { ElementType::INVALID };
    virtual std::string toString() const { return ""; }
};

struct Clef : public Element {
    Clef();
    std::optional<int> line;
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

struct Attributes : public Element {
    Attributes();
    std::vector<Clef> clefs;
    std::optional<unsigned int> divisions;
    std::vector<Key> keys;
    std::vector<Time> times;
    std::string toString() const;
};

struct Backup : public Element {
    Backup();
    std::string toString() const;
    std::optional<unsigned int> duration;
};

struct Forward : public Element {
    Forward();
    std::string toString() const;
    std::optional<unsigned int> duration;
};

struct Measure : public Element {
    Measure();
    std::vector<std::unique_ptr<Element>> elements;
    std::string number;
    std::string toString() const;
};

struct Pitch : public Element {
    Pitch();
    unsigned int octave { 4 };
    char step { 'C' };  // TODO make type safe
};

struct TimeModification : public Element {
    TimeModification();
    unsigned int actualNotes { 0 };
    unsigned int normalNotes { 0 };
    std::string toString() const;
};

struct Note : public Element {
    Note();
    bool chord { false };
    unsigned int dots { 0 };
    std::optional<unsigned int> duration;
    bool grace { false };
    bool measureRest { false };
    Pitch pitch;            // TODO: make optional ?
    bool rest { false };    // TODO: support display-step and display-octave
    std::optional<TimeModification> timeModification;
    std::string toString() const;
    std::string type;
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

struct ScorePart : public Element {
    ScorePart();
    std::string id;
    std::string partName;
    std::string toString() const;
};

struct ScorePartwise : public Element {
    ScorePartwise();
    bool isFound { false };
    PartList partList;
    std::vector<Part> parts;
    std::string version { "1.0" };
    std::string toString() const;
};

struct MxmlData {
    ScorePartwise scorePartwise;
};

#endif // MXMLDATA_H
