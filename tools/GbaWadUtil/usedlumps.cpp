#include "wadprocessor.h"
#include "doomtypes.h"

#include <algorithm>
#include <cstddef>
#include <vector>

//GbaWadUtil keeps only the lumps the engine reads: anything else (a PWAD's
//DEHACKED, intermission animations, PC speaker sounds, textures, flats and
//sprites no map or thing uses, ...) is dropped. The tables here mirror the
//engine's own, so keep them in step with it: a lump the engine looks for
//that isn't kept stops the game with "W_GetNumForName: ... not found".

//sprnames in info.c.
static const char* const spriteNames[] =
{
    "TROO","SHTG","PUNG","PISG","PISF","SHTF","SHT2","CHGG","CHGF","MISG",
    "MISF","SAWG","PLSG","PLSF","BFGG","BFGF","BLUD","PUFF","BAL1","BAL2",
    "PLSS","PLSE","MISL","BFS1","BFE1","BFE2","TFOG","IFOG","PLAY","POSS",
    "SPOS","VILE","FIRE","FATB","FBXP","SKEL","MANF","FATT","CPOS","SARG",
    "HEAD","BAL7","BOSS","BOS2","SKUL","SPID","BSPI","APLS","APBX","CYBR",
    "PAIN","SSWV","KEEN","BBRN","BOSF","ARM1","ARM2","BAR1","BEXP","FCAN",
    "BON1","BON2","BKEY","RKEY","YKEY","BSKU","RSKU","YSKU","STIM","MEDI",
    "SOUL","PINV","PSTR","PINS","MEGA","SUIT","PMAP","PVIS","CLIP","AMMO",
    "ROCK","BROK","CELL","CELP","SHEL","SBOX","BPAK","BFUG","MGUN","CSAW",
    "LAUN","PLAS","SHOT","SGN2","COLU","SMT2","GOR1","POL2","POL5","POL4",
    "POL3","POL1","POL6","GOR2","GOR3","GOR4","GOR5","SMIT","COL1","COL2",
    "COL3","COL4","CAND","CBRA","COL6","TRE1","TRE2","ELEC","CEYE","FSKU",
    "COL5","TBLU","TGRN","TRED","SMBT","SMGT","SMRT","HDB1","HDB2","HDB3",
    "HDB4","HDB5","HDB6","POB1","POB2","BRS1","TLMP","TLP2","TNT1"
};

//S_sfx in sounds.c, read as DSxxxxxx.
static const char* const soundNames[] =
{
    "PISTOL","SHOTGN","SGCOCK","DSHTGN","DBOPN","DBCLS","DBLOAD","PLASMA","BFG","SAWUP",
    "SAWIDL","SAWFUL","SAWHIT","RLAUNC","RXPLOD","FIRSHT","FIRXPL","PSTART","PSTOP","DOROPN",
    "DORCLS","STNMOV","SWTCHN","SWTCHX","PLPAIN","DMPAIN","POPAIN","VIPAIN","MNPAIN","PEPAIN",
    "SLOP","ITEMUP","WPNUP","OOF","TELEPT","POSIT1","POSIT2","POSIT3","BGSIT1","BGSIT2",
    "SGTSIT","CACSIT","BRSSIT","CYBSIT","SPISIT","BSPSIT","KNTSIT","VILSIT","MANSIT","PESIT",
    "SKLATK","SGTATK","SKEPCH","VILATK","CLAW","SKESWG","PLDETH","PDIEHI","PODTH1","PODTH2",
    "PODTH3","BGDTH1","BGDTH2","SGTDTH","CACDTH","SKLDTH","BRSDTH","CYBDTH","SPIDTH","BSPDTH",
    "VILDTH","KNTDTH","PEDTH","SKEDTH","POSACT","BGACT","DMACT","BSPACT","BSPWLK","VILACT",
    "NOWAY","BAREXP","PUNCH","HOOF","METAL","CHGUN","TINK","BDOPN","BDCLS","ITMBK",
    "FLAME","FLAMST","GETPOW","BOSPIT","BOSCUB","BOSSIT","BOSPN","BOSDTH","MANATK","MANDTH",
    "SSSIT","SSDTH","KEENPN","KEENDT","SKEACT","SKESIT","SKEATK","RADIO"
};

//musicNames in i_audio.c, read as D_xxxxxx.
static const char* const musicNames[] =
{
    "E1M1","E1M2","E1M3","E1M4","E1M5","E1M6","E1M7","E1M8","E1M9",
    "E2M1","E2M2","E2M3","E2M4","E2M5","E2M6","E2M7","E2M8","E2M9",
    "E3M1","E3M2","E3M3","E3M4","E3M5","E3M6","E3M7","E3M8","E3M9",
    "INTER","INTRO","BUNNY","VICTOR","INTROA","RUNNIN","STALKS","COUNTD",
    "BETWEE","DOOM","THE_DA","SHAWN","DDTBLU","IN_CIT","DEAD","STLKS2",
    "THEDA2","DOOM2","DDTBL2","RUNNI2","DEAD2","STLKS3","ROMERO","SHAWN2",
    "MESSAG","COUNT2","DDTBL3","AMPIE","THEDA3","ADRIAN","MESSG2","ROMER2",
    "TENSE","SHAWN3","OPENIN","EVIL","ULTIMA","READ_M","DM2TTL","DM2INT"
};

//Lumps read by these names.
static const char* const lumpNames[] =
{
    //r_data.c, v_video.c, s_mix.c (GUSBANK is added after ProcessWad).
    "TEXTURE1","TEXTURE2","TEXCOLS","COLPOOL","COLRUNS","COLORMAP","GBAPAL","GUSBANK",

    //d_main.c's demo loop.
    "DEMO1","DEMO2","DEMO3",

    //Full-screen pictures (ProcessPages). Not HELP1 or Doom 2's HELP, the
    //read this screens: the menu has no read this page.
    "TITLEPIC","INTERPIC","CREDIT","HELP2","VICTORY2","ENDPIC","BOSSBACK","BUNNYPIC",

    //m_menu.c.
    "M_DOOM","M_NGAME","M_OPTION","M_LOADG","M_SAVEG","M_EPISOD","M_EPI1","M_EPI2","M_EPI3","M_EPI4",
    "M_NEWG","M_SKILL","M_JKILL","M_ROUGH","M_HURT","M_ULTRA","M_NMARE","M_LSLEFT","M_LSCNTR","M_LSRGHT",
    "M_OPTTTL","M_ENDGAM","M_MESSG","M_MSGOFF","M_MSGON","M_ARUN","M_DETAIL","M_GDLOW","M_GDHIGH","M_GAMMA",
    "M_SVOL","M_SFXVOL","M_MUSVOL","M_THERML","M_THERMM","M_THERMR","M_THERMO","M_SKULL1","M_SKULL2",

    //st_stuff.c.
    "STFGOD0","STFDEAD0",

    //wi_stuff.c.
    "WIURH0","WIURH1","WISPLAT","WIPCNT","WICOLON","WIMINUS","WIF","WIENTER","WISCRT2",
    "WIOSTK","WIOSTI","WITIME","WIPAR","WISUCKS","WIMSTT",
};

//The animated textures and flats in p_spec.c's animdefs: first and last.
static const char* const animTextures[][2] =
{
    {"BLODGR1","BLODGR4"},{"SLADRIP1","SLADRIP3"},{"BLODRIP1","BLODRIP4"},{"FIREWALA","FIREWALL"},
    {"GSTFONT1","GSTFONT3"},{"FIRELAV3","FIRELAVA"},{"FIREMAG1","FIREMAG3"},{"FIREBLU1","FIREBLU2"},
    {"ROCKRED1","ROCKRED3"},{"BFALL1","BFALL4"},{"SFALL1","SFALL4"},{"WFALL1","WFALL4"},
    {"DBRAIN1","DBRAIN4"}
};

static const char* const animFlats[][2] =
{
    {"NUKAGE1","NUKAGE3"},{"FWATER1","FWATER4"},{"SWATER1","SWATER4"},{"LAVA1","LAVA4"},
    {"BLOOD1","BLOOD3"},{"RROCK05","RROCK08"},{"SLIME01","SLIME04"},{"SLIME05","SLIME08"},
    {"SLIME09","SLIME12"}
};

//Flats read by name: the sky (g_game.c) and the finale text backgrounds (f_finale.c).
static const char* const flatNames[] =
{
    "F_SKY1","FLOOR4_8","SFLR6_1","MFLR8_4","MFLR8_3","SLIME16",
    "RROCK14","RROCK07","RROCK17","RROCK13","RROCK19"
};

//Map lumps after the marker, as they are after ProcessLevel.
static const QStringList mapLumpNames = {"THINGS", "LINEDEFS", "SIDEDEFS", "SEGS", "SSECTORS",
                                         "NODES", "SECTORS", "REJECT", "BLOCKMAP", "SECLINES"};

static QString LumpName8(const char* name)
{
    return QString::fromLatin1(name, int(strnlen(name, 8))).toUpper();
}

static QSet<QString> UsedLumpNames()
{
    QSet<QString> names;

    for(const char* n : lumpNames)
        names.insert(n);

    for(const char* n : soundNames)
        names.insert(QString("DS") + n);

    for(const char* n : musicNames)
        names.insert(QString("D_") + n);

    //hu_stuff.c: the font, '!' to '_'.
    for(int c = '!'; c <= '_'; c++)
        names.insert(QString::asprintf("STCFN%03d", c));

    //st_stuff.c.
    for(int i = 0; i < 10; i++)
    {
        names.insert(QString::asprintf("STGANUM%d", i));
        names.insert(QString::asprintf("STYSNUM%d", i));
    }

    for(int i = 0; i < 6; i++)
    {
        names.insert(QString::asprintf("STKEYS%d", i));
        names.insert(QString::asprintf("STGNUM%d", i + 2));
    }

    for(int i = 0; i < 5; i++)
    {
        for(int j = 0; j < 3; j++)
            names.insert(QString::asprintf("STFST%d%d", i, j));

        names.insert(QString::asprintf("STFTR%d0", i));
        names.insert(QString::asprintf("STFTL%d0", i));
        names.insert(QString::asprintf("STFOUCH%d", i));
        names.insert(QString::asprintf("STFEVL%d", i));
        names.insert(QString::asprintf("STFKILL%d", i));
    }

    //wi_stuff.c.
    for(int i = 0; i < 10; i++)
        names.insert(QString::asprintf("WINUM%d", i));

    for(int e = 0; e < 3; e++)
        names.insert(QString::asprintf("WIMAP%d", e));

    for(int e = 0; e < 4; e++)
        for(int m = 0; m < 9; m++)
            names.insert(QString::asprintf("WILV%d%d", e, m));

    for(int m = 0; m < 32; m++)
        names.insert(QString::asprintf("CWILV%02d", m));

    //f_finale.c: the E3 ending's "THE END".
    for(int s = 0; s < 7; s++)
        names.insert(QString::asprintf("END%d", s));

    return names;
}

//The maps the engine can load, by marker: Doom 2's MAP01-MAP32 if there
//is a MAP01, else E1M1-E4M9.
QStringList WadProcessor::MapNames()
{
    QStringList maps;
    Lump l;

    auto add = [&](const QString& name)
    {
        if(wadFile.GetLumpByName(name, l) != -1)
            maps.append(name);
    };

    if(wadFile.GetLumpByName("MAP01", l) != -1)
    {
        for(int m = 1; m <= 32; m++)
            add(QString::asprintf("MAP%02d", m));
    }
    else
    {
        for(int e = 1; e <= 4; e++)
            for(int m = 1; m <= 9; m++)
                add(QString::asprintf("E%dM%d", e, m));
    }

    return maps;
}

//Keep only the textures a map uses, along with the sky textures, the other
//half of each switch and the whole of each animation they are in. Texture 0
//stays first, since sidedefs use it for no texture. Must run before
//ProcessTextureColumns and before ProcessSides turns names into numbers.
bool WadProcessor::RemoveUnusedTextures()
{
    struct Texture
    {
        QString name;
        QByteArray entry;
        int lump;           //0 for TEXTURE1, 1 for TEXTURE2.
    };

    std::vector<Texture> textures;

    const char* const texLumpNames[2] = {"TEXTURE1", "TEXTURE2"};
    qint32 texLumpNums[2];

    for(int t = 0; t < 2; t++)
    {
        Lump l;

        texLumpNums[t] = wadFile.GetLumpByName(texLumpNames[t], l);

        if(texLumpNums[t] == -1)
            continue;

        const char* data = l.data.constData();
        const qint32 count = *reinterpret_cast<const qint32*>(data);
        const qint32* directory = reinterpret_cast<const qint32*>(data) + 1;

        for(qint32 i = 0; i < count; i++)
        {
            const maptexture_t* mtexture = reinterpret_cast<const maptexture_t*>(data + directory[i]);
            const int size = int(offsetof(maptexture_t, patches) + mtexture->patchcount * sizeof(mappatch_t));

            textures.push_back({LumpName8(mtexture->name), QByteArray(data + directory[i], size), t});
        }
    }

    if(textures.empty())
        return false;

    std::vector<bool> used(textures.size(), false);
    QHash<QString, int> numForName;

    //The first of a name, as GetTextureNumForName finds.
    for(int i = int(textures.size()) - 1; i >= 0; i--)
        numForName[textures[i].name] = i;

    auto use = [&](const QString& name)
    {
        auto it = numForName.constFind(name);

        if(it != numForName.constEnd())
            used[it.value()] = true;
    };

    used[0] = true;

    for(const QString& map : MapNames())
    {
        Lump sides;
        wadFile.GetLumpByNum(wadFile.GetLumpByName(map, sides) + ML_SIDEDEFS, sides);

        const mapsidedef_t* side = reinterpret_cast<const mapsidedef_t*>(sides.data.constData());

        for(quint32 i = 0; i < sides.length / sizeof(mapsidedef_t); i++)
        {
            use(LumpName8(side[i].toptexture));
            use(LumpName8(side[i].bottomtexture));
            use(LumpName8(side[i].midtexture));
        }
    }

    //g_game.c picks one by game and episode.
    for(int i = 1; i <= 4; i++)
        use(QString("SKY%1").arg(i));

    //p_switch.c's switches are all SW1xxx and SW2xxx pairs.
    for(size_t i = 0; i < textures.size(); i++)
    {
        const QString& name = textures[i].name;

        if(used[i] && (name.startsWith("SW1") || name.startsWith("SW2")))
            use(QString(name).replace(2, 1, name[2] == '1' ? '2' : '1'));
    }

    //Animations step through texture numbers from first to last.
    for(const auto& anim : animTextures)
    {
        const int first = numForName.value(anim[0], -1);
        const int last = numForName.value(anim[1], -1);

        if(first == -1 || last == -1)
            continue;

        const int lo = qMin(first, last), hi = qMax(first, last);

        if(std::find(used.begin() + lo, used.begin() + hi + 1, true) != used.begin() + hi + 1)
            std::fill(used.begin() + lo, used.begin() + hi + 1, true);
    }

    quint32 removed = 0;

    for(int t = 0; t < 2; t++)
    {
        if(texLumpNums[t] == -1)
            continue;

        std::vector<qint32> offsets;
        QByteArray entries;

        for(size_t i = 0; i < textures.size(); i++)
        {
            if(textures[i].lump != t)
                continue;

            if(!used[i])
            {
                removed++;
                continue;
            }

            offsets.push_back(entries.size());
            entries.append(textures[i].entry);
        }

        //Count, directory, then the textures.
        const qint32 count = qint32(offsets.size());
        QByteArray out(reinterpret_cast<const char*>(&count), 4);

        for(qint32 ofs : offsets)
        {
            ofs += 4 + count * 4;
            out.append(reinterpret_cast<const char*>(&ofs), 4);
        }

        out.append(entries);

        Lump l;
        l.name = texLumpNames[t];
        l.data = out;
        l.length = out.size();

        wadFile.ReplaceLump(texLumpNums[t], l);
    }

    qInfo("Removed %u unused textures, %u left.", removed, quint32(textures.size()) - removed);

    return true;
}

//Rebuild the WAD with only the lumps the engine reads: the last of each
//named lump, the loadable maps, and one F_START..F_END and one
//S_START..S_END holding the used flats and sprites from every flat and
//sprite namespace (FF_START, SS_START, ...), the engine only looks in the
//last of each. A flat or sprite in more than one keeps its first place
//(animations go by flat order) and its last data. Runs after everything
//else, so it sees the lumps as the engine will.
bool WadProcessor::KeepUsedLumps()
{
    static const QRegularExpression flatMarker("^F[F0-9]?_(START|END)$");
    static const QRegularExpression spriteMarker("^S[S0-9]?_(START|END)$");

    const QSet<QString> names = UsedLumpNames();

    QSet<QString> spriteSet;

    for(const char* n : spriteNames)
        spriteSet.insert(n);

    const QStringList maps = MapNames();

    struct Namespace
    {
        std::vector<Lump> lumps;
        QHash<QString, int> index;

        void Add(const Lump& l, const QString& name)
        {
            auto it = index.constFind(name);

            if(it != index.constEnd())
            {
                lumps[it.value()] = l;
            }
            else
            {
                index.insert(name, int(lumps.size()));
                lumps.push_back(l);
            }
        }
    };

    Namespace flats, sprites;

    //Flats read by name, and below, the ones the maps' sectors use.
    QSet<QString> usedFlats;

    for(const char* n : flatNames)
        usedFlats.insert(n);

    std::vector<Lump> kept;

    quint32 removed = 0, removedBytes = 0;
    QStringList removedNames;

    Namespace* ns = nullptr;
    int depth = 0;

    for(quint32 i = 0; i < wadFile.LumpCount(); i++)
    {
        Lump l;
        wadFile.GetLumpByNum(i, l);

        const QString name = l.name.toUpper();

        //Markers are dropped and put back around the merged namespaces.
        const bool isFlatMarker = flatMarker.match(name).hasMatch();
        const bool isSpriteMarker = spriteMarker.match(name).hasMatch();

        if(isFlatMarker || isSpriteMarker)
        {
            if(name.endsWith("_START"))
            {
                ns = isFlatMarker ? &flats : &sprites;
                depth++;
            }
            else if(depth > 0 && --depth == 0)
            {
                ns = nullptr;
            }

            continue;
        }

        if(ns)
        {
            ns->Add(l, name);
            continue;
        }

        if(maps.contains(name))
        {
            kept.push_back(l);

            for(Lump m; wadFile.GetLumpByNum(i + 1, m) && mapLumpNames.contains(m.name, Qt::CaseInsensitive); i++)
            {
                kept.push_back(m);

                if(m.name.compare("SECTORS", Qt::CaseInsensitive))
                    continue;

                const mapsector_t* sector = reinterpret_cast<const mapsector_t*>(m.data.constData());

                for(quint32 s = 0; s < m.length / sizeof(mapsector_t); s++)
                {
                    usedFlats.insert(LumpName8(sector[s].floorpic));
                    usedFlats.insert(LumpName8(sector[s].ceilingpic));
                }
            }

            continue;
        }

        //Only the last of a name can be found.
        Lump last;

        if(names.contains(name) && wadFile.GetLumpByName(name, last) == qint32(i))
        {
            kept.push_back(l);
            continue;
        }

        removed++;
        removedBytes += l.length;
        removedNames.append(name);
    }

    //Plus whole animations.
    std::vector<bool> flatUsed(flats.lumps.size(), false);

    for(size_t i = 0; i < flats.lumps.size(); i++)
        flatUsed[i] = usedFlats.contains(flats.lumps[i].name.toUpper());

    for(const auto& anim : animFlats)
    {
        const int first = flats.index.value(anim[0], -1);
        const int last = flats.index.value(anim[1], -1);

        if(first == -1 || last == -1)
            continue;

        const int lo = qMin(first, last), hi = qMax(first, last);

        if(std::find(flatUsed.begin() + lo, flatUsed.begin() + hi + 1, true) != flatUsed.begin() + hi + 1)
            std::fill(flatUsed.begin() + lo, flatUsed.begin() + hi + 1, true);
    }

    auto addNamespace = [&](const char* start, const char* end, const std::vector<Lump>& lumps, auto isUsed)
    {
        Lump marker;
        marker.length = 0;

        marker.name = start;
        kept.push_back(marker);

        for(size_t i = 0; i < lumps.size(); i++)
        {
            if(isUsed(i))
            {
                kept.push_back(lumps[i]);
            }
            else
            {
                removed++;
                removedBytes += lumps[i].length;
                removedNames.append(lumps[i].name.toUpper());
            }
        }

        marker.name = end;
        kept.push_back(marker);
    };

    addNamespace("F_START", "F_END", flats.lumps, [&](size_t i) { return flatUsed[i]; });
    addNamespace("S_START", "S_END", sprites.lumps, [&](size_t i) { return spriteSet.contains(sprites.lumps[i].name.left(4).toUpper()); });

    while(wadFile.LumpCount())
        wadFile.RemoveLump(wadFile.LumpCount() - 1);

    for(const Lump& l : kept)
        wadFile.InsertLump(wadFile.LumpCount(), l);

    removedNames.removeDuplicates();

    qInfo("Removed %u unused lumps, %u bytes: %s", removed, removedBytes, qPrintable(removedNames.join(' ')));

    return true;
}
