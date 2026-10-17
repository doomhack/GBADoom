#include "sizereport.h"
#include "wadfile.h"

enum
{
    PartEngine, PartMaps, PartWallTextures, PartFlats, PartSprites, PartGraphics,
    PartSoundEffects, PartMusic, PartInstruments, PartOther
};

static_assert(numSizeParts == PartOther + 1, "sizePartNames and the Part enum differ");

//Map lumps after the marker, as they are after ProcessLevel.
static const QStringList mapLumpNames = {"THINGS", "LINEDEFS", "SIDEDEFS", "SEGS", "SSECTORS",
                                         "NODES", "SECTORS", "REJECT", "BLOCKMAP", "SECLINES"};

static const QStringList wallTextureNames = {"TEXCOLS", "COLPOOL", "COLRUNS", "TEXTURE1", "TEXTURE2"};

//Palettes, light maps, the DeHackEd tables and demos: small, so in Other.
static const QStringList otherNames = {"GBAPAL", "COLORMAP", "GBASTATE", "GBAMOBJ", "GBAWEAP", "GBAMISC", "GBATEXT"};

static int PartOf(const QString& name, int ns, bool inMap)
{
    static const QRegularExpression mapRe("^(E\\dM\\d|MAP\\d\\d)$");

    if(ns)
        return ns;

    if(mapRe.match(name).hasMatch() || (inMap && mapLumpNames.contains(name)))
        return PartMaps;

    if(wallTextureNames.contains(name))
        return PartWallTextures;

    if(name == "GUSBANK")
        return PartInstruments;

    if(name.startsWith("DS"))
        return PartSoundEffects;

    if(name.startsWith("D_"))
        return PartMusic;

    if(otherNames.contains(name) || name.startsWith("DEMO"))
        return PartOther;

    //Menus, the status bar, fonts, intermission and full-screen pictures.
    return PartGraphics;
}

QVector<qint64> SizeBreakdown(WadFile& wad, qint64 wadBytes, qint64 engineBytes)
{
    QVector<qint64> parts(numSizeParts, 0);
    QSet<QByteArray> seen;

    int ns = 0;
    bool inMap = false;

    for(quint32 i = 0; i < wad.LumpCount(); i++)
    {
        Lump l;
        wad.GetLumpByNum(i, l);

        const QString name = l.name.toUpper();

        //KeepUsedLumps merges every flat and sprite namespace into these.
        if(name == "F_START" || name == "S_START")
        {
            ns = name == "F_START" ? PartFlats : PartSprites;
            continue;
        }

        if(name == "F_END" || name == "S_END")
        {
            ns = 0;
            continue;
        }

        const int part = PartOf(name, ns, inMap);
        inMap = part == PartMaps;

        if(!l.length)
            continue;

        const QByteArray data = l.data.left(l.length);

        if(seen.contains(data))
            continue;

        seen.insert(data);
        parts[part] += (qint64(l.length) + 3) & ~qint64(3);
    }

    qint64 sum = 0;

    for(qint64 p : parts)
        sum += p;

    //The header and directory, less the last lump's padding, which isn't written.
    parts[PartOther] += wadBytes - sum;
    parts[PartEngine] = engineBytes;

    return parts;
}

void PrintSizes(WadFile& wad, qint64 wadBytes, qint64 engineBytes)
{
    const QVector<qint64> parts = SizeBreakdown(wad, wadBytes, engineBytes);
    const qint64 total = wadBytes + engineBytes;

    if(engineBytes)
        qInfo("Space used: ROM %lld bytes (%.1f MB of 32 MB).", total, total / (1024.0 * 1024.0));
    else
        qInfo("Space used: WAD %lld bytes (%.1f MB).", total, total / (1024.0 * 1024.0));

    for(int i = 0; i < numSizeParts; i++)
    {
        if(parts[i])
            qInfo("  %s: %lld bytes (%.1f%%).", sizePartNames[i], parts[i], 100.0 * parts[i] / total);
    }
}
