#include "dehacked.h"
#include "wadprocessor.h"
#include "../../include/sounds.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstddef>
#include <iterator>

//Vanilla DeHackEd patches, as Chocolate Doom reads them (deh_*.c): a
//section starts with a line like "Thing 1 (Player)" and holds "Key = Value"
//lines up to a blank line. Text sections are "Text <from> <to>" followed by
//that many characters, newlines included, of the old string then the new.

DehTables::DehTables()
{
    memcpy(states, defaultStates, sizeof(states));
    memcpy(mobjinfo, defaultMobjinfo, sizeof(mobjinfo));
    memcpy(weaponinfo, defaultWeaponinfo, sizeof(weaponinfo));
    misc = defaultMiscinfo;

    static const char* const gbaText[NUMTEXTS] =
    {
#define DEH_TEXT_GBA(id, s) s,
        DEH_TEXTS(DEH_TEXT_GBA)
#undef DEH_TEXT_GBA
    };

    for(int i = 0; i < NUMTEXTS; i++)
    {
        text[i] = gbaText[i];
        textPatched[i] = false;
    }
}

namespace
{

enum FieldType
{
    Number,
    FrameNum,
    SoundNum,
    ThingBits,
    AmmoType,
    Ignored     //A vanilla field GBADoom has no use for.
};

//A field of mobjinfo_t, weaponinfo_t or miscinfo_t, all ints.
struct Field
{
    const char* name;
    int offset;
    FieldType type;
};

#define THING(name, field, type) {name, int(offsetof(mobjinfo_t, field)), type}

const Field thingFields[] =
{
    THING("ID #",               doomednum,      Number),
    THING("Initial frame",      spawnstate,     FrameNum),
    THING("Hit points",         spawnhealth,    Number),
    THING("First moving frame", seestate,       FrameNum),
    THING("Alert sound",        seesound,       SoundNum),
    THING("Reaction time",      reactiontime,   Number),
    THING("Attack sound",       attacksound,    SoundNum),
    THING("Injury frame",       painstate,      FrameNum),
    THING("Pain chance",        painchance,     Number),
    THING("Pain sound",         painsound,      SoundNum),
    THING("Close attack frame", meleestate,     FrameNum),
    THING("Far attack frame",   missilestate,   FrameNum),
    THING("Death frame",        deathstate,     FrameNum),
    THING("Exploding frame",    xdeathstate,    FrameNum),
    THING("Death sound",        deathsound,     SoundNum),
    THING("Speed",              speed,          Number),
    THING("Width",              radius,         Number),
    THING("Height",             height,         Number),
    THING("Mass",               mass,           Number),
    THING("Missile damage",     damage,         Number),
    THING("Action sound",       activesound,    SoundNum),
    THING("Bits",               flags,          ThingBits),
    THING("Respawn frame",      raisestate,     FrameNum),
};

#define WEAPON(name, field, type) {name, int(offsetof(weaponinfo_t, field)), type}

const Field weaponFields[] =
{
    WEAPON("Ammo type",         ammo,           AmmoType),
    WEAPON("Deselect frame",    downstate,      FrameNum),
    WEAPON("Select frame",      upstate,        FrameNum),
    WEAPON("Bobbing frame",     readystate,     FrameNum),
    WEAPON("Shooting frame",    atkstate,       FrameNum),
    WEAPON("Firing frame",      flashstate,     FrameNum),
};

#define MISC(name, field) {name, int(offsetof(miscinfo_t, field)), Number}

const Field miscFields[] =
{
    MISC("Initial Health",      initial_health),
    MISC("Initial Bullets",     initial_bullets),
    MISC("Max Health",          maxhealth),
    MISC("Max Armor",           max_armor),
    MISC("Green Armor Class",   green_armor_class),
    MISC("Blue Armor Class",    blue_armor_class),
    MISC("Max Soulsphere",      max_soul),
    MISC("Soulsphere Health",   soul_health),
    MISC("Megasphere Health",   mega_health),
    MISC("God Mode Health",     god_health),
    MISC("IDFA Armor",          idfa_armor),
    MISC("IDFA Armor Class",    idfa_armor_class),
    MISC("BFG Cells/Shot",      bfgcells),

    //The GBA's Ammo & Keys cheat uses the IDFA values, and monsters
    //always infight.
    {"IDKFA Armor",             0, Ignored},
    {"IDKFA Armor Class",       0, Ignored},
    {"Monsters Infight",        0, Ignored},
};

//Vanilla's mobj flags. Above them, the engine keeps its own (MF_POOLED...).
const unsigned int vanillaThingBits = 0x0FFFFFFF;

//Bits can also be flag names joined by +, | or commas, as BEX allows and
//some patches for vanilla use.
const struct { const char* name; unsigned int bits; } thingBitNames[] =
{
    {"SPECIAL", MF_SPECIAL}, {"SOLID", MF_SOLID}, {"SHOOTABLE", MF_SHOOTABLE},
    {"NOSECTOR", MF_NOSECTOR}, {"NOBLOCKMAP", MF_NOBLOCKMAP}, {"AMBUSH", MF_AMBUSH},
    {"JUSTHIT", MF_JUSTHIT}, {"JUSTATTACKED", MF_JUSTATTACKED}, {"SPAWNCEILING", MF_SPAWNCEILING},
    {"NOGRAVITY", MF_NOGRAVITY}, {"DROPOFF", MF_DROPOFF}, {"PICKUP", MF_PICKUP},
    {"NOCLIP", MF_NOCLIP}, {"SLIDE", MF_SLIDE}, {"FLOAT", MF_FLOAT},
    {"TELEPORT", MF_TELEPORT}, {"MISSILE", MF_MISSILE}, {"DROPPED", MF_DROPPED},
    {"SHADOW", MF_SHADOW}, {"NOBLOOD", MF_NOBLOOD}, {"CORPSE", MF_CORPSE},
    {"INFLOAT", MF_INFLOAT}, {"COUNTKILL", MF_COUNTKILL}, {"COUNTITEM", MF_COUNTITEM},
    {"SKULLFLY", MF_SKULLFLY}, {"NOTDMATCH", MF_NOTDMATCH}, {"TRANSLATION", MF_TRANSLATION1},
    {"TRANSLATION1", MF_TRANSLATION1}, {"TRANSLATION2", MF_TRANSLATION2},
};

//Text blocks name a string by its vanilla text. Some patches leave out a
//string's trailing newline (DeHackEd found it anyway), so they're compared
//without.
QByteArray TextKey(QByteArray text)
{
    while(text.endsWith('\n'))
        text.chop(1);

    return text;
}

//Each deh_text.h string, by TextKey of its vanilla text.
const QHash<QByteArray, QList<int>>& VanillaTexts()
{
    static QHash<QByteArray, QList<int>> texts;

    if(texts.isEmpty())
    {
        static const char* const gbaText[NUMTEXTS] =
        {
#define DEH_TEXT_GBA(id, s) s,
            DEH_TEXTS(DEH_TEXT_GBA)
#undef DEH_TEXT_GBA
        };

        static const struct { int id; const char* text; } vanilla[] =
        {
#define DEH_TEXT_VANILLA(id, s) {TXT_##id, s},
            DEH_VANILLA_TEXTS(DEH_TEXT_VANILLA)
#undef DEH_TEXT_VANILLA
        };

        const char* text[NUMTEXTS];

        for(int i = 0; i < NUMTEXTS; i++)
            text[i] = gbaText[i];

        for(const auto& v : vanilla)
            text[v.id] = v.text;

        for(int i = 0; i < NUMTEXTS; i++)
            texts[TextKey(text[i])].append(i);
    }

    return texts;
}

const QStringList sectionNames = {"thing", "frame", "pointer", "sound", "ammo", "weapon",
                                  "sprite", "text", "misc", "cheat"};

class Parser
{
public:
    Parser(const QByteArray& data, const QString& source, DehTables& tables)
        : data(data), source(source), t(tables) {}

    bool Run();

private:
    QByteArray data;
    const QString source;
    DehTables& t;

    int pos = 0;
    int lineNum = 0;
    QByteArray pending;
    bool hasPending = false;

    int warnings = 0;
    int things = 0, frames = 0, pointers = 0, weapons = 0, ammo = 0, misc = 0;
    int texts = 0, renames = 0;
    QStringList unusedTexts;
    QMap<QString, int> skipped;

    bool ReadLine(QByteArray& line);
    bool IsSectionStart(const QByteArray& line);
    bool ReadField(QByteArray& key, QByteArray& value);
    void Warn(const QString& msg);
    void Skip(const QString& what);

    bool ParseNumber(const QByteArray& value, qint64& n);
    void SetField(char* base, const Field* fields, int count, const QByteArray& key, const QByteArray& value);

    void ThingSection(int n);
    void FrameSection(int n);
    void PointerSection(int n);
    void WeaponSection(int n);
    void AmmoSection(int n);
    void MiscSection();
    void TextSection(int fromLen, int toLen);
    void ApplyText(const QByteArray& from, const QByteArray& to);
};

//The next line, without its end of line. False at the end of the patch.
bool Parser::ReadLine(QByteArray& line)
{
    if(hasPending)
    {
        line = pending;
        hasPending = false;
        return true;
    }

    if(pos >= data.size())
        return false;

    int end = data.indexOf('\n', pos);

    if(end < 0)
        end = data.size();

    line = data.mid(pos, end - pos);
    pos = end + 1;
    lineNum++;

    if(line.endsWith('\r'))
        line.chop(1);

    return true;
}

bool Parser::IsSectionStart(const QByteArray& line)
{
    const QByteArray l = line.trimmed();

    if(l.startsWith('['))
        return true;

    const int space = l.indexOf(' ');

    return space > 0 && !l.contains('=') && sectionNames.contains(QString::fromLatin1(l.left(space)).toLower());
}

//The next "Key = Value" of a section. False at the blank line (or new
//section) that ends it.
bool Parser::ReadField(QByteArray& key, QByteArray& value)
{
    QByteArray line;

    while(ReadLine(line))
    {
        const QByteArray l = line.trimmed();

        if(l.isEmpty())
            return false;

        if(l.startsWith('#'))
            continue;

        //Some patches leave out the blank line between sections.
        if(IsSectionStart(l))
        {
            pending = line;
            hasPending = true;
            return false;
        }

        const int eq = l.indexOf('=');

        if(eq < 0)
        {
            Warn(QString("not a \"Key = Value\" line: %1").arg(QString::fromLatin1(l)));
            continue;
        }

        key = l.left(eq).trimmed();
        value = l.mid(eq + 1).trimmed();
        return true;
    }

    return false;
}

void Parser::Warn(const QString& msg)
{
    warnings++;

    if(warnings <= 20)
        qWarning("%s line %d: %s", qPrintable(source), lineNum, qPrintable(msg));
}

//A section GBADoom doesn't support: counted for the summary, then skipped.
void Parser::Skip(const QString& what)
{
    skipped[what]++;

    QByteArray key, value;

    while(ReadField(key, value))
    {
    }
}

bool Parser::ParseNumber(const QByteArray& value, qint64& n)
{
    bool ok;

    n = value.toLongLong(&ok);

    if(!ok)
        Warn(QString("\"%1\" isn't a number").arg(QString::fromLatin1(value)));

    return ok;
}

void Parser::SetField(char* base, const Field* fields, int count, const QByteArray& key, const QByteArray& value)
{
    for(int i = 0; i < count; i++)
    {
        const Field& f = fields[i];

        if(key.compare(f.name, Qt::CaseInsensitive))
            continue;

        if(f.type == Ignored)
            return;

        qint64 n;
        bool isNumber;

        value.toLongLong(&isNumber);

        if(f.type == ThingBits && !isNumber)
        {
            n = 0;

            for(const QString& name : QString::fromLatin1(value).split(QRegularExpression("[+|,\\s]+"), Qt::SkipEmptyParts))
            {
                auto it = std::find_if(std::begin(thingBitNames), std::end(thingBitNames),
                                       [&](const auto& b) { return !name.compare(b.name, Qt::CaseInsensitive); });

                if(it == std::end(thingBitNames))
                    Warn(QString("Bits: %1 isn't one of vanilla's flags, ignored").arg(name));
                else
                    n |= it->bits;
            }
        }
        else if(!ParseNumber(value, n))
        {
            return;
        }

        switch(f.type)
        {
            case FrameNum:
                if(n < 0 || n >= NUMSTATES)
                    return Warn(QString("%1 = %2: there are only %3 frames").arg(f.name).arg(n).arg(NUMSTATES));
                break;

            case SoundNum:
                if(n < 0 || n >= NUMSFX)
                    return Warn(QString("%1 = %2: there are only %3 sounds").arg(f.name).arg(n).arg(NUMSFX));
                break;

            case AmmoType:
                if((n < 0 || n >= NUMAMMO) && n != am_noammo)
                    return Warn(QString("%1 = %2 isn't an ammo type").arg(f.name).arg(n));
                break;

            case ThingBits:
                if(quint32(n) & ~vanillaThingBits)
                    Warn(QString("Bits 0x%1 aren't vanilla's, ignored").arg(quint32(n) & ~vanillaThingBits, 0, 16));

                n = quint32(n) & vanillaThingBits;
                break;

            default:
                break;
        }

        *reinterpret_cast<int*>(base + f.offset) = int(n);
        return;
    }

    Warn(QString("unknown field \"%1\"").arg(QString::fromLatin1(key)));
}

void Parser::ThingSection(int n)
{
    //Thing numbers start at 1.
    if(n < 1 || n > NUMMOBJTYPES)
    {
        Warn(QString("there's no Thing %1").arg(n));
        return Skip("bad Thing");
    }

    QByteArray key, value;

    while(ReadField(key, value))
        SetField(reinterpret_cast<char*>(&t.mobjinfo[n - 1]), thingFields, std::size(thingFields), key, value);

    things++;
}

void Parser::FrameSection(int n)
{
    if(n < 0 || n >= NUMSTATES)
    {
        Warn(QString("there's no Frame %1").arg(n));
        return Skip("bad Frame");
    }

    state_t& st = t.states[n];
    QByteArray key, value;

    while(ReadField(key, value))
    {
        qint64 v;

        if(!key.compare("Unknown 1", Qt::CaseInsensitive) || !key.compare("Unknown 2", Qt::CaseInsensitive))
            continue;

        if(!ParseNumber(value, v))
            continue;

        if(!key.compare("Sprite number", Qt::CaseInsensitive))
        {
            if(v < 0 || v >= NUMSPRITES)
                Warn(QString("Sprite number = %1: there are only %2 sprites").arg(v).arg(NUMSPRITES));
            else
                st.sprite = quint16(v);
        }
        else if(!key.compare("Sprite subnumber", Qt::CaseInsensitive))
        {
            if(v < 0 || v > 0xffff)
                Warn(QString("Sprite subnumber = %1 is out of range").arg(v));
            else
                st.frame = quint16(v);
        }
        else if(!key.compare("Duration", Qt::CaseInsensitive))
        {
            if(v < -1 || v > SHRT_MAX)
                Warn(QString("Duration = %1 is out of range, clamped").arg(v));

            st.tics = qint16(qBound<qint64>(-1, v, qint64(SHRT_MAX)));
        }
        else if(!key.compare("Next frame", Qt::CaseInsensitive))
        {
            if(v < 0 || v >= NUMSTATES)
                Warn(QString("Next frame = %1: there are only %2 frames").arg(v).arg(NUMSTATES));
            else
                st.nextstate = quint16(v);
        }
        else if(!key.compare("Codep Frame", Qt::CaseInsensitive))
        {
            if(v < 0 || v >= NUMSTATES)
                Warn(QString("Codep Frame = %1: there are only %2 frames").arg(v).arg(NUMSTATES));
            else
                st.action = defaultStates[v].action;
        }
        else
        {
            Warn(QString("unknown field \"%1\"").arg(QString::fromLatin1(key)));
        }
    }

    frames++;
}

//"Pointer N (Frame F)": gives frame F the code pointer frame "Codep Frame"
//had in the original game.
void Parser::PointerSection(int frame)
{
    if(frame < 0 || frame >= NUMSTATES)
    {
        Warn(QString("there's no Frame %1").arg(frame));
        return Skip("bad Pointer");
    }

    QByteArray key, value;

    while(ReadField(key, value))
    {
        qint64 v;

        if(key.compare("Codep Frame", Qt::CaseInsensitive))
        {
            Warn(QString("unknown field \"%1\"").arg(QString::fromLatin1(key)));
            continue;
        }

        if(!ParseNumber(value, v))
            continue;

        if(v < 0 || v >= NUMSTATES)
            Warn(QString("Codep Frame = %1: there are only %2 frames").arg(v).arg(NUMSTATES));
        else
            t.states[frame].action = defaultStates[v].action;
    }

    pointers++;
}

void Parser::WeaponSection(int n)
{
    if(n < 0 || n >= NUMWEAPONS)
    {
        Warn(QString("there's no Weapon %1").arg(n));
        return Skip("bad Weapon");
    }

    QByteArray key, value;

    while(ReadField(key, value))
        SetField(reinterpret_cast<char*>(&t.weaponinfo[n]), weaponFields, std::size(weaponFields), key, value);

    weapons++;
}

void Parser::AmmoSection(int n)
{
    if(n < 0 || n >= NUMAMMO)
    {
        Warn(QString("there's no Ammo %1").arg(n));
        return Skip("bad Ammo");
    }

    QByteArray key, value;

    while(ReadField(key, value))
    {
        qint64 v;

        if(!ParseNumber(value, v))
            continue;

        if(!key.compare("Max ammo", Qt::CaseInsensitive))
            t.misc.maxammo[n] = int(v);
        else if(!key.compare("Per ammo", Qt::CaseInsensitive))
            t.misc.clipammo[n] = int(v);
        else
            Warn(QString("unknown field \"%1\"").arg(QString::fromLatin1(key)));
    }

    ammo++;
}

void Parser::MiscSection()
{
    QByteArray key, value;

    while(ReadField(key, value))
    {
        SetField(reinterpret_cast<char*>(&t.misc), miscFields, std::size(miscFields), key, value);
        misc++;
    }
}

//The old and new strings follow the header line, newlines included, with
//any \r left out (as Chocolate Doom's DEH_GetChar).
void Parser::TextSection(int fromLen, int toLen)
{
    if(fromLen < 0 || toLen < 0)
        return Warn("bad Text lengths");

    QByteArray s;

    while(s.size() < fromLen + toLen && pos < data.size())
    {
        const char c = data[pos++];

        if(c == '\n')
            lineNum++;

        if(c != '\r')
            s.append(c);
    }

    //A new string cut short by the end of the patch is used as it is.
    if(s.size() < fromLen)
        return Warn("the patch ends in a Text block");

    ApplyText(s.left(fromLen), s.mid(fromLen));
}

void Parser::ApplyText(const QByteArray& from, const QByteArray& to)
{
    auto it = VanillaTexts().constFind(TextKey(from));

    if(it != VanillaTexts().constEnd())
    {
        for(int id : it.value())
        {
            //Flat and texture names are lump names, of which vanilla only
            //looks at the first 8 characters.
            t.text[id] = id >= TXT_FLOOR4_8 ? to.left(8).toUpper() : to;
            t.textPatched[id] = true;
        }

        texts++;
        return;
    }

    //The engine looks up sprites, sounds and music by these names, so
    //ProcessDehacked gives it the new name's lumps under the old one.
    //Vanilla looks at 4 characters of a sprite name and 8 of a lump name
    //(DSxxxxxx, D_xxxxxx).
    const QString name = QString::fromLatin1(from).toUpper();

    struct
    {
        QStringList names;
        QMap<QString, QString>* renames;
        int length;
    } const kinds[] =
    {
        {EngineSpriteNames(), &t.spriteRenames, 4},
        {EngineSoundNames(), &t.soundRenames, 6},
        {EngineMusicNames(), &t.musicRenames, 6},
    };

    for(const auto& k : kinds)
    {
        if(!k.names.contains(name))
            continue;

        const QString newName = QString::fromLatin1(to).left(k.length).toUpper();

        if(newName.isEmpty() || (k.renames == &t.spriteRenames && newName.size() != 4))
            return Warn(QString("can't rename %1 to \"%2\"").arg(name, newName));

        (*k.renames)[name] = newName;
        renames++;
        return;
    }

    //Menus, cheat messages and so on: GBADoom has its own.
    QString shown = QString::fromLatin1(from).simplified();

    if(shown.size() > 30)
        shown = shown.left(27) + "...";

    unusedTexts.append("\"" + shown + "\"");
}

bool Parser::Run()
{
    //Binary patches are from DeHackEd 2.x and before.
    if(data.left(1024).contains('\0'))
    {
        qWarning("%s: a binary DeHackEd patch, not supported. Convert it to text with DeHackEd 3.", qPrintable(source));
        return false;
    }

    //DOS text files can end with a Ctrl-Z.
    const int eof = data.indexOf('\x1a');

    if(eof >= 0)
        data.truncate(eof);

    QByteArray line;
    bool first = true;

    while(ReadLine(line))
    {
        const QByteArray l = line.trimmed();

        if(l.isEmpty() || l.startsWith('#'))
            continue;

        //"Patch File for DeHackEd v3.0"
        if(first && l.startsWith("Patch File"))
        {
            first = false;
            continue;
        }

        first = false;

        if(l.startsWith('['))
        {
            //BEX sections run to the next section, blank lines and all.
            skipped["BEX " + QString::fromLatin1(l)]++;

            while(ReadLine(line))
            {
                if(IsSectionStart(line))
                {
                    pending = line;
                    hasPending = true;
                    break;
                }
            }

            continue;
        }

        const int eq = l.indexOf('=');

        if(eq >= 0 && !IsSectionStart(l))
        {
            const QByteArray key = l.left(eq).trimmed();
            const int value = l.mid(eq + 1).trimmed().toInt();

            //Doom 1.666 (16, 17) and 1.9 (19 to 21) number things and frames the same.
            if(!key.compare("Doom version", Qt::CaseInsensitive))
            {
                if(value < 16 || value > 21)
                    Warn(QString("written for Doom version %1, numbers may not match Doom 1.9's").arg(value));
            }
            else if(!key.compare("Patch format", Qt::CaseInsensitive))
            {
                if(value != 5 && value != 6)
                    Warn(QString("patch format %1, only 6 is supported").arg(value));
            }
            else if(!key.compare("Engine config", Qt::CaseInsensitive) || !key.compare("IWAD", Qt::CaseInsensitive) ||
                    !key.compare("Data WAD", Qt::CaseInsensitive))
            {
                //WhackEd's notes on what the patch was made for.
            }
            else
            {
                Warn(QString("unknown setting \"%1\"").arg(QString::fromLatin1(key)));
            }

            continue;
        }

        const QList<QByteArray> words = l.simplified().split(' ');
        const QByteArray type = words[0].toLower();
        const int n = words.size() > 1 ? words[1].toInt() : -1;

        if(type == "thing")
        {
            ThingSection(n);
        }
        else if(type == "frame")
        {
            FrameSection(n);
        }
        else if(type == "pointer")
        {
            //"Pointer 12 (Frame 34)": the frame is the one that counts.
            static const QRegularExpression frameRe("\\(\\s*frame\\s+(-?\\d+)\\s*\\)", QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch m = frameRe.match(QString::fromLatin1(l));

            if(m.hasMatch())
                PointerSection(m.captured(1).toInt());
            else
            {
                Warn(QString("can't find the frame in \"%1\"").arg(QString::fromLatin1(l)));
                Skip("bad Pointer");
            }
        }
        else if(type == "weapon")
        {
            WeaponSection(n);
        }
        else if(type == "ammo")
        {
            AmmoSection(n);
        }
        else if(type == "misc")
        {
            MiscSection();
        }
        else if(type == "text")
        {
            TextSection(n, words.size() > 2 ? words[2].toInt() : -1);
        }
        else if(type == "sound" || type == "sprite" || type == "cheat")
        {
            Skip(QString::fromLatin1(words[0]));
        }
        else
        {
            Warn(QString("unknown section \"%1\"").arg(QString::fromLatin1(l)));
        }
    }

    if(warnings > 20)
        qWarning("%s: %d more warnings.", qPrintable(source), warnings - 20);

    QStringList done;

    auto count = [&](int n, const char* what)
    {
        if(n)
            done.append(QString("%1 %2").arg(n).arg(what));
    };

    count(things, "things");
    count(frames, "frames");
    count(pointers, "code pointers");
    count(weapons, "weapons");
    count(ammo, "ammo");
    count(misc, "misc values");
    count(texts, "texts");
    count(renames, "renames");

    qInfo("DeHackEd %s: %s.", qPrintable(source), done.isEmpty() ? "no changes" : qPrintable(done.join(", ")));

    for(auto it = skipped.constBegin(); it != skipped.constEnd(); ++it)
        qInfo("  Skipped (not supported): %s x%d.", qPrintable(it.key()), it.value());

    if(!unusedTexts.isEmpty())
        qInfo("  %d Text strings GBADoom doesn't show: %s", int(unusedTexts.size()),
              qPrintable(unusedTexts.mid(0, 4).join(", ") + (unusedTexts.size() > 4 ? ", ..." : "")));

    return true;
}

} // namespace

bool ApplyDehacked(const QByteArray& patch, const QString& source, DehTables& tables)
{
    Parser p(patch, source, tables);

    return p.Run();
}

//Text blocks that rename sounds and music: the engine still asks for
//prefix + the old name, so that gets the new name's lump.
static void RenameLumps(WadFile& wad, const QString& prefix, const QMap<QString, QString>& renames)
{
    QList<Lump> renamed;

    for(auto it = renames.constBegin(); it != renames.constEnd(); ++it)
    {
        Lump l;

        if(wad.GetLumpByName(prefix + it.value(), l) == -1)
        {
            qWarning("DeHackEd: %s%s is renamed to %s%s, which isn't in the WAD.",
                     qPrintable(prefix), qPrintable(it.key()), qPrintable(prefix), qPrintable(it.value()));
            continue;
        }

        l.name = prefix + it.key();
        renamed.append(l);
    }

    for(const Lump& l : renamed)
    {
        Lump old;
        const qint32 num = wad.GetLumpByName(l.name, old);

        if(num != -1)
            wad.ReplaceLump(num, l);
        else
            wad.InsertLump(wad.LumpCount(), l);
    }
}

//Text blocks that rename sprites: the old name's frames are replaced by
//copies of the new name's, renamed. KeepUsedLumps merges the namespace
//they are added in with the others.
static void RenameSprites(WadFile& wad, const QMap<QString, QString>& renames)
{
    if(renames.isEmpty())
        return;

    static const QRegularExpression spriteMarker("^S[S0-9]?_(START|END)$");

    QMap<QString, Lump> frames;     //The last of each name.
    QList<quint32> remove;

    int depth = 0;

    for(quint32 i = 0; i < wad.LumpCount(); i++)
    {
        Lump l;
        wad.GetLumpByNum(i, l);

        const QString name = l.name.toUpper();

        if(spriteMarker.match(name).hasMatch())
        {
            if(name.endsWith("_START"))
                depth++;
            else if(depth > 0)
                depth--;

            continue;
        }

        if(!depth)
            continue;

        frames[name] = l;

        if(renames.contains(name.left(4)))
            remove.append(i);
    }

    for(int i = remove.size() - 1; i >= 0; i--)
        wad.RemoveLump(remove[i]);

    Lump marker;
    marker.length = 0;
    marker.name = "SS_START";
    wad.InsertLump(wad.LumpCount(), marker);

    for(auto it = renames.constBegin(); it != renames.constEnd(); ++it)
    {
        int count = 0;

        for(auto f = frames.constBegin(); f != frames.constEnd(); ++f)
        {
            if(!f.key().startsWith(it.value()))
                continue;

            Lump l = f.value();
            l.name = it.key() + f.key().mid(4);
            wad.InsertLump(wad.LumpCount(), l);
            count++;
        }

        if(!count)
            qWarning("DeHackEd: sprite %s is renamed to %s, which has no frames in the WAD.",
                     qPrintable(it.key()), qPrintable(it.value()));
    }

    marker.name = "SS_END";
    wad.InsertLump(wad.LumpCount(), marker);
}

//F_TextWrite (f_finale.c) draws finale text from x 10, y 10, 11 lines apart,
//and cuts it off at the edges of the 240x160 screen. Vanilla's text is
//wrapped for 320 pixels, so each paragraph with a line that doesn't fit is
//rewrapped to fit. GBADoom's own text already fits.
static const int finaleLeft = 10;
static const int finaleWidth = 240 - finaleLeft;
static const int finaleMaxLines = (160 - 10 - 8) / 11 + 1;

static QByteArray RewrapFinaleText(const QByteArray& text, const int (&charWidth)[256], int& lines)
{
    auto width = [&](const QByteArray& s)
    {
        int w = 0;

        for(char c : s)
            w += charWidth[quint8(toupper(quint8(c)))];

        return w;
    };

    const QList<QByteArray> in = text.split('\n');
    QList<QByteArray> out;

    for(int i = 0; i < in.size(); )
    {
        //A paragraph: lines up to a blank one.
        int end = i;
        bool fits = true;

        while(end < in.size() && !in[end].trimmed().isEmpty())
            fits &= width(in[end++]) <= finaleWidth;

        if(end == i)
        {
            out.append(in[i++]);
            continue;
        }

        if(fits)
        {
            out.append(in.mid(i, end - i));
        }
        else
        {
            QByteArray line;

            for(int j = i; j < end; j++)
            {
                for(const QByteArray& word : in[j].simplified().split(' '))
                {
                    const QByteArray next = line.isEmpty() ? word : line + ' ' + word;

                    if(!line.isEmpty() && width(next) > finaleWidth)
                    {
                        out.append(line);
                        line = word;
                    }
                    else
                    {
                        line = next;
                    }
                }
            }

            out.append(line);
        }

        i = end;
    }

    //Trailing blank lines aren't drawn.
    lines = int(out.size());

    while(lines > 0 && out[lines - 1].trimmed().isEmpty())
        lines--;

    return out.join('\n');
}

//Applies the IWAD's and PWADs' DEHACKED lumps, then the -deh patches, and
//writes the tables the engine reads (P_InitInfoTables in info.c).
bool WadProcessor::ProcessDehacked()
{
    for(quint32 i = 0; i < wadFile.LumpCount(); i++)
    {
        Lump l;
        wadFile.GetLumpByNum(i, l);

        if(!l.name.compare("DEHACKED", Qt::CaseInsensitive))
            ApplyDehacked(l.data.left(l.length), QString("DEHACKED lump %1").arg(i), deh);
    }

    for(const QString& file : dehFiles)
    {
        QFile f(file);

        if(!f.open(QIODevice::ReadOnly))
        {
            qCritical("Can't open DeHackEd patch %s", qPrintable(file));
            return false;
        }

        if(!ApplyDehacked(f.readAll(), QFileInfo(file).fileName(), deh))
            return false;
    }

    RenameLumps(wadFile, "DS", deh.soundRenames);
    RenameLumps(wadFile, "D_", deh.musicRenames);
    RenameSprites(wadFile, deh.spriteRenames);

    //The width of each character in the WAD's font. Others are a 4 pixel space.
    int charWidth[256];

    for(int c = 0; c < 256; c++)
        charWidth[c] = 4;

    for(int c = '!'; c <= '_'; c++)
    {
        Lump l;

        if(wadFile.GetLumpByName(QString::asprintf("STCFN%03d", c), l) != -1 && l.length >= 2)
            charWidth[c] = qFromLittleEndian<qint16>(l.data.constData());
    }

    for(int t = TXT_E1TEXT; t <= TXT_T6TEXT; t++)
    {
        int lines;

        deh.text[t] = RewrapFinaleText(deh.text[t], charWidth, lines);

        if(deh.textPatched[t] && lines > finaleMaxLines)
            qWarning("DeHackEd: a finale text is %d lines, only %d fit on the screen: \"%s...\"",
                     lines, finaleMaxLines, qPrintable(QString::fromLatin1(deh.text[t].left(30)).simplified()));
    }

    //An offset to each string, then the strings.
    QByteArray text(NUMTEXTS * sizeof(quint32), 0);

    for(int t = 0; t < NUMTEXTS; t++)
    {
        qToLittleEndian<quint32>(quint32(text.size()), text.data() + t * sizeof(quint32));
        text.append(deh.text[t]);
        text.append('\0');
    }

    auto add = [&](const char* name, const QByteArray& data)
    {
        Lump l;
        l.name = name;
        l.data = data;
        l.length = data.size();

        wadFile.InsertLump(wadFile.LumpCount(), l);
    };

    add("GBASTATE", QByteArray(reinterpret_cast<const char*>(deh.states), sizeof(deh.states)));
    add("GBAMOBJ", QByteArray(reinterpret_cast<const char*>(deh.mobjinfo), sizeof(deh.mobjinfo)));
    add("GBAWEAP", QByteArray(reinterpret_cast<const char*>(deh.weaponinfo), sizeof(deh.weaponinfo)));
    add("GBAMISC", QByteArray(reinterpret_cast<const char*>(&deh.misc), sizeof(deh.misc)));
    add("GBATEXT", text);

    return true;
}
