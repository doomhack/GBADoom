#include "wadprocessor.h"
#include "doomtypes.h"
#include "mus2mid.h"
#include "gusbank.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

WadProcessor::WadProcessor(WadFile& wad, QObject *parent)
    : QObject(parent), wadFile(wad)
{

}

bool WadProcessor::ProcessWad()
{
    //Figure out if our IWAD is Doom or Doom2. (E1M1 or MAP01)

    Lump mapLump;

    RemoveOverriddenMaps();
    RemoveUnusedLumps();
    ProcessTextureColumns();
    ProcessSounds();
    ProcessMusic();

    int lumpNum = wadFile.GetLumpByName("MAP01", mapLump);

    if(lumpNum != -1)
    {
        return ProcessD2Levels();
    }
    else
    {
        int lumpNum = wadFile.GetLumpByName("E1M1", mapLump);

        //Can't find any maps.
        if(lumpNum == -1)
            return false;
    }

    return ProcessD1Levels();
}

bool WadProcessor::ProcessD2Levels()
{
    for(int m = 1; m <= 32; m++)
    {
        Lump l;

        QString mapName = QString("MAP%1").arg(m, 2, 10, QChar('0'));

        int lumpNum = wadFile.GetLumpByName(mapName, l);

        if(lumpNum != -1)
        {
            ProcessLevel(lumpNum);
        }
    }

    return true;
}

bool WadProcessor::ProcessD1Levels()
{
    for(int e = 1; e <= 4; e++)
    {
        for(int m = 1; m <= 9; m++)
        {
            Lump l;

            QString mapName = QString("E%1M%2").arg(e).arg(m);

            int lumpNum = wadFile.GetLumpByName(mapName, l);

            if(lumpNum != -1)
            {
                ProcessLevel(lumpNum);
            }
        }
    }

    return true;
}

bool WadProcessor::ProcessLevel(quint32 lumpNum)
{
    ProcessVertexes(lumpNum);
    ProcessLines(lumpNum);
    ProcessSegs(lumpNum);
    ProcessSides(lumpNum);
    ProcessBlockmap(lumpNum);
    ProcessReject(lumpNum);
    ProcessSectorLines(lumpNum);

    //Lines and segs now hold their own vertex coordinates, so the
    //engine doesn't need VERTEXES. Its map lump enum has no
    //ML_VERTEXES, so the lumps after it move down one slot.
    wadFile.RemoveLump(lumpNum+ML_VERTEXES);

    return true;
}

bool WadProcessor::ProcessVertexes(quint32 lumpNum)
{
    quint32 vtxLumpNum = lumpNum+ML_VERTEXES;

    Lump vxl;

    if(!wadFile.GetLumpByNum(vtxLumpNum, vxl))
        return false;

    if(vxl.length == 0)
        return false;

    quint32 vtxCount = vxl.length / sizeof(mapvertex_t);

    vertex_t* newVtx = new vertex_t[vtxCount];
    const mapvertex_t* oldVtx = reinterpret_cast<const mapvertex_t*>(vxl.data.constData());

    for(quint32 i = 0; i < vtxCount; i++)
    {
        newVtx[i].x = (oldVtx[i].x << 16);
        newVtx[i].y = (oldVtx[i].y << 16);
    }

    Lump newVxl;
    newVxl.name = vxl.name;
    newVxl.length = vtxCount * sizeof(vertex_t);
    newVxl.data = QByteArray(reinterpret_cast<const char*>(newVtx), newVxl.length);

    delete[] newVtx;

    wadFile.ReplaceLump(vtxLumpNum, newVxl);

    return true;
}

bool WadProcessor::ProcessLines(quint32 lumpNum)
{
    quint32 lineLumpNum = lumpNum+ML_LINEDEFS;

    Lump lines;

    if(!wadFile.GetLumpByNum(lineLumpNum, lines))
        return false;

    if(lines.length == 0)
        return false;

    quint32 lineCount = lines.length / sizeof(maplinedef_t);

    line_t* newLines = new line_t[lineCount];

    const maplinedef_t* oldLines = reinterpret_cast<const maplinedef_t*>(lines.data.constData());

    //We need vertexes for this...

    quint32 vtxLumpNum = lumpNum+ML_VERTEXES;

    Lump vxl;

    if(!wadFile.GetLumpByNum(vtxLumpNum, vxl))
        return false;

    if(vxl.length == 0)
        return false;

    const vertex_t* vtx = reinterpret_cast<const vertex_t*>(vxl.data.constData());

    Lump mapLump;
    wadFile.GetLumpByNum(lumpNum, mapLump);

    //lineno and sidenums are 16 bit; 0xffff is NO_INDEX.
    if(lineCount >= NO_INDEX)
    {
        qCritical("%s: %u lines, the engine supports at most %u.", qPrintable(mapLump.name), lineCount, NO_INDEX - 1u);
        exit(1);
    }

    for(unsigned int i = 0; i < lineCount; i++)
    {
        //vtx is in fixed point (ProcessVertexes), lines keep map units.
        const int x1 = vtx[oldLines[i].v1].x >> FRACBITS;
        const int y1 = vtx[oldLines[i].v1].y >> FRACBITS;
        const int x2 = vtx[oldLines[i].v2].x >> FRACBITS;
        const int y2 = vtx[oldLines[i].v2].y >> FRACBITS;

        //dx and dy are shorts in the engine.
        if(x2 - x1 != qint16(x2 - x1) || y2 - y1 != qint16(y2 - y1))
        {
            qCritical("%s: line %u is too long (dx %d, dy %d). Lines can be at most 32767 units in x and y.", qPrintable(mapLump.name), i, x2 - x1, y2 - y1);
            exit(1);
        }

        newLines[i].v1.x = x1;
        newLines[i].v1.y = y1;

        newLines[i].v2.x = x2;
        newLines[i].v2.y = y2;

        newLines[i].special = oldLines[i].special;
        newLines[i].flags = oldLines[i].flags;
        newLines[i].tag = oldLines[i].tag;

        newLines[i].dx = x2 - x1;
        newLines[i].dy = y2 - y1;

        newLines[i].sidenum[0] = oldLines[i].sidenum[0];
        newLines[i].sidenum[1] = oldLines[i].sidenum[1];

        newLines[i].bbox[BOXLEFT] = (x1 < x2 ? x1 : x2);
        newLines[i].bbox[BOXRIGHT] = (x1 < x2 ? x2 : x1);

        newLines[i].bbox[BOXTOP] = (y1 < y2 ? y2 : y1);
        newLines[i].bbox[BOXBOTTOM] = (y1 < y2 ? y1 : y2);

        newLines[i].lineno = i;
    }

    Lump newLine;
    newLine.name = lines.name;
    newLine.length = lineCount * sizeof(line_t);
    newLine.data = QByteArray(reinterpret_cast<const char*>(newLines), newLine.length);

    delete[] newLines;

    wadFile.ReplaceLump(lineLumpNum, newLine);

    return true;
}

bool WadProcessor::ProcessSegs(quint32 lumpNum)
{
    quint32 segsLumpNum = lumpNum+ML_SEGS;

    Lump segs;

    if(!wadFile.GetLumpByNum(segsLumpNum, segs))
        return false;

    if(segs.length == 0)
        return false;

    quint32 segCount = segs.length / sizeof(mapseg_t);

    seg_t* newSegs = new seg_t[segCount];

    const mapseg_t* oldSegs = reinterpret_cast<const mapseg_t*>(segs.data.constData());

    //We need vertexes for this...

    quint32 vtxLumpNum = lumpNum+ML_VERTEXES;

    Lump vxl;

    if(!wadFile.GetLumpByNum(vtxLumpNum, vxl))
        return false;

    if(vxl.length == 0)
        return false;

    const vertex_t* vtx = reinterpret_cast<const vertex_t*>(vxl.data.constData());

    //And LineDefs. Must process lines first.

    quint32 linesLumpNum = lumpNum+ML_LINEDEFS;

    Lump lxl;

    if(!wadFile.GetLumpByNum(linesLumpNum, lxl))
        return false;

    if(lxl.length == 0)
        return false;

    const line_t* lines = reinterpret_cast<const line_t*>(lxl.data.constData());

    //And sides too...

    quint32 sidesLumpNum = lumpNum+ML_SIDEDEFS;

    Lump sxl;

    if(!wadFile.GetLumpByNum(sidesLumpNum, sxl))
        return false;

    if(sxl.length == 0)
        return false;

    const mapsidedef_t* sides = reinterpret_cast<const mapsidedef_t*>(sxl.data.constData());


    //****************************

    for(unsigned int i = 0; i < segCount; i++)
    {
        //vtx is in fixed point (ProcessVertexes), segs keep map units.
        newSegs[i].v1.x = vtx[oldSegs[i].v1].x >> FRACBITS;
        newSegs[i].v1.y = vtx[oldSegs[i].v1].y >> FRACBITS;

        newSegs[i].v2.x = vtx[oldSegs[i].v2].x >> FRACBITS;
        newSegs[i].v2.y = vtx[oldSegs[i].v2].y >> FRACBITS;

        newSegs[i].angle = oldSegs[i].angle;
        newSegs[i].offset = oldSegs[i].offset;

        newSegs[i].linenum = oldSegs[i].linedef;

        const line_t* ldef = &lines[newSegs[i].linenum];

        int side = oldSegs[i].side;

        newSegs[i].sidenum = ldef->sidenum[side];

        if(newSegs[i].sidenum != NO_INDEX)
        {
            newSegs[i].frontsectornum = sides[newSegs[i].sidenum].sector;
        }
        else
        {
            newSegs[i].frontsectornum = NO_INDEX;
        }

        newSegs[i].backsectornum = NO_INDEX;

        if(ldef->flags & ML_TWOSIDED)
        {
            if(ldef->sidenum[side^1] != NO_INDEX)
            {
                newSegs[i].backsectornum = sides[ldef->sidenum[side^1]].sector;
            }
        }
    }

    Lump newSeg;
    newSeg.name = segs.name;
    newSeg.length = segCount * sizeof(seg_t);
    newSeg.data = QByteArray(reinterpret_cast<const char*>(newSegs), newSeg.length);

    delete[] newSegs;

    wadFile.ReplaceLump(segsLumpNum, newSeg);

    return true;
}

//Rebuilds BLOCKMAP: header (orgx, orgy, width, height), one word offset
//per cell, then the line lists, each ending in 0xffff. The leading 0
//of each list (never used by the engine) is dropped, each distinct
//list is stored once, and a list that is the tail of a longer one
//points into it. Each cell keeps its lines in the original order.
bool WadProcessor::ProcessBlockmap(quint32 lumpNum)
{
    quint32 bmLumpNum = lumpNum+ML_BLOCKMAP;

    Lump bml;

    if(!wadFile.GetLumpByNum(bmLumpNum, bml))
        return false;

    if(bml.length < 8)
        return false;

    const quint16* oldBm = reinterpret_cast<const quint16*>(bml.data.constData());
    const quint32 oldWords = bml.length / 2;
    const quint32 cellCount = oldBm[2] * oldBm[3];

    if(4 + cellCount > oldWords)
        return false;

    std::vector<std::vector<quint16>> cells(cellCount);

    for(quint32 c = 0; c < cellCount; c++)
    {
        quint32 pos = oldBm[4+c];

        //Skip the leading 0.
        if(pos < oldWords && oldBm[pos] != 0xffff)
            pos++;

        while(pos < oldWords && oldBm[pos] != 0xffff)
            cells[c].push_back(oldBm[pos++]);
    }

    //Longest lists first, so shorter ones can find themselves as a tail.
    std::vector<std::vector<quint16>> lists(cells);
    std::sort(lists.begin(), lists.end(), [](const std::vector<quint16>& a, const std::vector<quint16>& b)
    {
        return a.size() != b.size() ? a.size() > b.size() : a < b;
    });

    std::vector<quint16> newBm(oldBm, oldBm + 4);
    newBm.resize(4 + cellCount);

    //Every stored list and each of its tails -> word offset.
    std::map<std::vector<quint16>, quint32> placed;

    for(const std::vector<quint16>& l : lists)
    {
        if(placed.count(l))
            continue;

        const quint32 start = newBm.size();

        newBm.insert(newBm.end(), l.begin(), l.end());
        newBm.push_back(0xffff);

        for(size_t i = 0; i <= l.size(); i++)
            placed.emplace(std::vector<quint16>(l.begin() + i, l.end()), start + i);
    }

    if(newBm.size() > 0x10000)
    {
        qWarning("%s: BLOCKMAP needs %u words, offsets only reach 65536. Left unchanged.", qPrintable(bml.name), quint32(newBm.size()));
        return false;
    }

    for(quint32 c = 0; c < cellCount; c++)
        newBm[4+c] = placed[cells[c]];

    Lump newBml;
    newBml.name = bml.name;
    newBml.length = newBm.size() * sizeof(quint16);
    newBml.data = QByteArray(reinterpret_cast<const char*>(newBm.data()), newBml.length);

    wadFile.ReplaceLump(bmLumpNum, newBml);

    return true;
}

//An all-zero REJECT never rejects anything, so it is emptied (kept as a
//zero-length lump so BLOCKMAP stays at its ML_* offset) and the engine
//skips the check.
bool WadProcessor::ProcessReject(quint32 lumpNum)
{
    quint32 rejectLumpNum = lumpNum+ML_REJECT;

    Lump rl;

    if(!wadFile.GetLumpByNum(rejectLumpNum, rl))
        return false;

    if(rl.length == 0 || rl.data.count('\0') != rl.data.size())
        return true;

    Lump emptyRl;
    emptyRl.name = rl.name;
    emptyRl.length = 0;

    wadFile.ReplaceLump(rejectLumpNum, emptyRl);

    return true;
}

//Builds SECLINES, inserted after BLOCKMAP: each sector's lines, in the
//order the engine's P_GroupLines used to make them (a line goes to its
//front sector, and to its back sector if that is different).
//Format: u32 start[sectors+1], then u16 line numbers; sector i has
//lines[start[i] .. start[i+1]-1]. Needs processed lines and sides.
bool WadProcessor::ProcessSectorLines(quint32 lumpNum)
{
    Lump mapLump, lines, sides, sectors, segs, ssectors;

    if(!wadFile.GetLumpByNum(lumpNum, mapLump) || !wadFile.GetLumpByNum(lumpNum+ML_LINEDEFS, lines) ||
       !wadFile.GetLumpByNum(lumpNum+ML_SIDEDEFS, sides) || !wadFile.GetLumpByNum(lumpNum+ML_SECTORS, sectors) ||
       !wadFile.GetLumpByNum(lumpNum+ML_SEGS, segs) || !wadFile.GetLumpByNum(lumpNum+ML_SSECTORS, ssectors))
        return false;

    const line_t* lineData = reinterpret_cast<const line_t*>(lines.data.constData());
    const sidedef_t* sideData = reinterpret_cast<const sidedef_t*>(sides.data.constData());
    const seg_t* segData = reinterpret_cast<const seg_t*>(segs.data.constData());
    const quint16* ssData = reinterpret_cast<const quint16*>(ssectors.data.constData()); //numsegs, firstseg

    const quint32 lineCount = lines.length / sizeof(line_t);
    const quint32 sideCount = sides.length / sizeof(sidedef_t);
    const quint32 sectorCount = sectors.length / sizeof(mapsector_t);
    const quint32 segCount = segs.length / sizeof(seg_t);
    const quint32 ssCount = ssectors.length / (2 * sizeof(quint16));

    //The engine takes a subsector's sector from its first seg.
    for(quint32 i = 0; i < ssCount; i++)
    {
        const quint16 firstSeg = ssData[i*2 + 1];

        if(firstSeg >= segCount || segData[firstSeg].frontsectornum >= sectorCount)
            qWarning("%s: subsector %u is a part of no sector.", qPrintable(mapLump.name), i);
    }

    std::vector<std::vector<quint16>> secLines(sectorCount);

    for(quint32 i = 0; i < lineCount; i++)
    {
        const quint16 s0 = lineData[i].sidenum[0];
        const quint16 s1 = lineData[i].sidenum[1];

        if(s0 >= sideCount)
        {
            qWarning("%s: line %u has no front side.", qPrintable(mapLump.name), i);
            continue;
        }

        const quint16 front = sideData[s0].sector;

        secLines[front].push_back(i);

        if(s1 < sideCount && quint16(sideData[s1].sector) != front)
            secLines[quint16(sideData[s1].sector)].push_back(i);
    }

    std::vector<quint32> start;
    std::vector<quint16> lineNums;

    for(const std::vector<quint16>& l : secLines)
    {
        start.push_back(lineNums.size());
        lineNums.insert(lineNums.end(), l.begin(), l.end());
    }

    start.push_back(lineNums.size());

    Lump secLinesLump;
    secLinesLump.name = "SECLINES";
    secLinesLump.data = QByteArray(reinterpret_cast<const char*>(start.data()), start.size() * sizeof(quint32));
    secLinesLump.data.append(reinterpret_cast<const char*>(lineNums.data()), lineNums.size() * sizeof(quint16));
    secLinesLump.length = secLinesLump.data.size();

    wadFile.InsertLump(lumpNum+ML_SECLINES, secLinesLump);

    return true;
}

bool WadProcessor::ProcessSides(quint32 lumpNum)
{
    quint32 sidesLumpNum = lumpNum+ML_SIDEDEFS;

    Lump sides;

    if(!wadFile.GetLumpByNum(sidesLumpNum, sides))
        return false;

    if(sides.length == 0)
        return false;

    //Lines and segs are already processed. Their sidenums get remapped.
    quint32 linesLumpNum = lumpNum+ML_LINEDEFS;
    quint32 segsLumpNum = lumpNum+ML_SEGS;
    quint32 sectorsLumpNum = lumpNum+ML_SECTORS;

    Lump mapLump, lines, segs, sectors;

    if(!wadFile.GetLumpByNum(lumpNum, mapLump) || !wadFile.GetLumpByNum(linesLumpNum, lines) || !wadFile.GetLumpByNum(segsLumpNum, segs) || !wadFile.GetLumpByNum(sectorsLumpNum, sectors))
        return false;

    quint32 sideCount = sides.length / sizeof(mapsidedef_t);
    quint32 lineCount = lines.length / sizeof(line_t);
    quint32 segCount = segs.length / sizeof(seg_t);
    quint32 sectorCount = sectors.length / sizeof(mapsector_t);

    std::vector<sidedef_t> convSides(sideCount);

    const mapsidedef_t* oldSides = reinterpret_cast<const mapsidedef_t*>(sides.data.constData());

    for(unsigned int i = 0; i < sideCount; i++)
    {
        convSides[i].textureoffset = oldSides[i].textureoffset;
        convSides[i].rowoffset = oldSides[i].rowoffset;

        convSides[i].toptexture = GetTextureNumForName(oldSides[i].toptexture);
        convSides[i].bottomtexture = GetTextureNumForName(oldSides[i].bottomtexture);
        convSides[i].midtexture = GetTextureNumForName(oldSides[i].midtexture);

        convSides[i].sector = oldSides[i].sector;

        if(quint16(convSides[i].sector) >= sectorCount)
        {
            qWarning("%s: sidedef %u has out-of-range sector %u. Using sector 0.", qPrintable(mapLump.name), i, quint16(convSides[i].sector));
            convSides[i].sector = 0;
        }
    }

    //The engine reads sidedefs from ROM. Side 0 of a line with a special
    //can be changed by the game (switch textures, scrolling), so each of
    //those keeps its own entry, numbered from 0; the engine copies them
    //to RAM. The rest are read only, so identical ones are merged.
    std::vector<quint16> remap(sideCount, NO_INDEX);
    std::vector<sidedef_t> newSides;

    line_t* newLines = reinterpret_cast<line_t*>(lines.data.data());
    seg_t* newSegs = reinterpret_cast<seg_t*>(segs.data.data());

    for(quint32 i = 0; i < lineCount; i++)
    {
        const quint16 s = newLines[i].sidenum[0];

        if(newLines[i].special && s < sideCount && remap[s] == NO_INDEX)
        {
            remap[s] = newSides.size();
            newSides.push_back(convSides[s]);
        }
    }

    std::map<QByteArray, quint16> merged;

    for(quint32 i = 0; i < sideCount; i++)
    {
        if(remap[i] != NO_INDEX)
            continue;

        const QByteArray key(reinterpret_cast<const char*>(&convSides[i]), sizeof(sidedef_t));

        auto it = merged.find(key);

        if(it != merged.end())
        {
            remap[i] = it->second;
        }
        else
        {
            remap[i] = newSides.size();
            merged.emplace(key, remap[i]);
            newSides.push_back(convSides[i]);
        }
    }

    for(quint32 i = 0; i < lineCount; i++)
    {
        for(int s = 0; s < 2; s++)
        {
            if(newLines[i].sidenum[s] < sideCount)
                newLines[i].sidenum[s] = remap[newLines[i].sidenum[s]];
        }
    }

    for(quint32 i = 0; i < segCount; i++)
    {
        if(newSegs[i].sidenum < sideCount)
            newSegs[i].sidenum = remap[newSegs[i].sidenum];
    }

    Lump newSide;
    newSide.name = sides.name;
    newSide.length = newSides.size() * sizeof(sidedef_t);
    newSide.data = QByteArray(reinterpret_cast<const char*>(newSides.data()), newSide.length);

    wadFile.ReplaceLump(sidesLumpNum, newSide);
    wadFile.ReplaceLump(linesLumpNum, lines);
    wadFile.ReplaceLump(segsLumpNum, segs);

    return true;
}

int WadProcessor::GetTextureNumForName(const char* tex_name)
{
    const int  *maptex1, *maptex2;
    int  numtextures1, numtextures2 = 0;
    const int *directory1, *directory2;


    //Convert name to uppercase for comparison.
    char tex_name_upper[9];

    strncpy(tex_name_upper, tex_name, 8);
    tex_name_upper[8] = 0; //Ensure null terminated.

    for (int i = 0; i < 8; i++)
    {
        tex_name_upper[i] = toupper(tex_name_upper[i]);
    }

    Lump tex1lump;
    wadFile.GetLumpByName("TEXTURE1", tex1lump);

    maptex1 = (const int*)tex1lump.data.constData();
    numtextures1 = *maptex1;
    directory1 = maptex1+1;

    Lump tex2lump;
    if (wadFile.GetLumpByName("TEXTURE2", tex2lump) != -1)
    {
        maptex2 = (const int*)tex2lump.data.constData();
        directory2 = maptex2+1;
        numtextures2 = *maptex2;
    }
    else
    {
        maptex2 = NULL;
        directory2 = NULL;
    }

    const int *directory = directory1;
    const int *maptex = maptex1;

    int numtextures = (numtextures1 + numtextures2);

    for (int i=0 ; i<numtextures; i++, directory++)
    {
        if (i == numtextures1)
        {
            // Start looking in second texture file.
            maptex = maptex2;
            directory = directory2;
        }

        int offset = *directory;

        const maptexture_t* mtexture = (const maptexture_t *) ( (const quint8*)maptex + offset);

        if(!strncmp(tex_name_upper, mtexture->name, 8))
        {
            return i;
        }
    }

    return 0;
}

//Composite every texture into 128 byte columns (the renderer wraps
//texture rows at 128), dedupe them and write:
//  TEXCOLS: u32 numtextures, u32 tableofs[numtextures], then for each
//           texture u16 colid[width], u16 runid[width]. Offsets are
//           from the lump start.
//  COLPOOL: the unique columns, 128 bytes each.
//  COLRUNS: the unique run lists for masked drawing: (topdelta, length)
//           byte pairs of the rows patches cover, ended by (0xff, 0xff).
//           A runid is the list's offset / 2.
//Every texture gets a table, in TEXTURE1/TEXTURE2 order, so switches and
//animations (which just swap texture numbers) need nothing special.
//After this the wall patches and PNAMES are not needed and are removed.
bool WadProcessor::ProcessTextureColumns()
{
    static const int colHeight = 128;

    Lump pnamesLump;
    if(wadFile.GetLumpByName("PNAMES", pnamesLump) == -1)
        return false;

    const char* pnamesData = pnamesLump.data.constData();
    const quint32 pnameCount = *((const quint32*)pnamesData);

    QVector<QByteArray> patches(pnameCount);

    for(quint32 i = 0; i < pnameCount; i++)
    {
        char n[9] = {0};
        strncpy(n, &pnamesData[4 + i*8], 8);

        Lump patchLump;
        if(wadFile.GetLumpByName(QString(QLatin1String(n)), patchLump) != -1)
            patches[i] = patchLump.data;
    }

    QList<QByteArray> texLumps;

    Lump tex1lump;
    if(wadFile.GetLumpByName("TEXTURE1", tex1lump) == -1)
        return false;

    texLumps.append(tex1lump.data);

    Lump tex2lump;
    if(wadFile.GetLumpByName("TEXTURE2", tex2lump) != -1)
        texLumps.append(tex2lump.data);

    QByteArray pool;
    QHash<QByteArray, quint16> poolIds;

    QByteArray runs;
    QHash<QByteArray, quint16> runIds;

    QVector<quint32> tableOfs;
    QByteArray tables;

    quint32 totalColumns = 0;

    for(const QByteArray& texLump : texLumps)
    {
        const char* maptex = texLump.constData();
        const qint32 numTextures = *((const qint32*)maptex);
        const qint32* directory = ((const qint32*)maptex) + 1;

        for(qint32 t = 0; t < numTextures; t++)
        {
            const maptexture_t* mtexture = (const maptexture_t*)(maptex + directory[t]);

            const int width = mtexture->width;
            const int height = qMin<int>(mtexture->height, colHeight);

            if(mtexture->height > colHeight)
                qWarning("Texture %.8s is %d high. Rows past %d are dropped.", mtexture->name, mtexture->height, colHeight);

            tableOfs.append(tables.size());

            QByteArray texRunIds;

            for(int x = 0; x < width; x++)
            {
                QByteArray col(colHeight, 0);
                quint8* dest = (quint8*)col.data();

                bool covered[colHeight] = {false};

                //Same compositing as R_DrawColumnInCache in GBADoom.
                for(int p = 0; p < mtexture->patchcount; p++)
                {
                    const mappatch_t* mpatch = &mtexture->patches[p];

                    if(mpatch->patch < 0 || mpatch->patch >= (int)pnameCount || patches[mpatch->patch].isEmpty())
                        continue;

                    const quint8* patch = (const quint8*)patches[mpatch->patch].constData();
                    const int patchWidth = *((const qint16*)patch);

                    const int px = x - mpatch->originx;

                    if(px < 0 || px >= patchWidth)
                        continue;

                    const quint8* post = patch + ((const qint32*)(patch + 8))[px];

                    while(post[0] != 0xff)
                    {
                        const quint8* source = post + 3;
                        int count = post[1];
                        int position = mpatch->originy + post[0];

                        if(position < 0)
                        {
                            source -= position;
                            count += position;
                            position = 0;
                        }

                        if(position + count > height)
                            count = height - position;

                        if(count > 0)
                        {
                            memcpy(dest + position, source, count);

                            for(int y = position; y < position + count; y++)
                                covered[y] = true;
                        }

                        post += post[1] + 4;
                    }
                }

                //Run list of the covered rows, for masked drawing.
                QByteArray colRuns;

                for(int y = 0; y < height; )
                {
                    if(!covered[y])
                    {
                        y++;
                        continue;
                    }

                    int start = y;

                    while(y < height && covered[y])
                        y++;

                    colRuns.append(char(start));
                    colRuns.append(char(y - start));
                }

                colRuns.append(char(0xff));
                colRuns.append(char(0xff));

                quint16 runId;

                auto rit = runIds.constFind(colRuns);

                if(rit != runIds.constEnd())
                {
                    runId = rit.value();
                }
                else
                {
                    if((runs.size() / 2) > 0xffff)
                    {
                        qCritical("Too many unique texture run lists.");
                        return false;
                    }

                    runId = runs.size() / 2;
                    runIds.insert(colRuns, runId);
                    runs.append(colRuns);
                }

                texRunIds.append((const char*)&runId, sizeof(runId));

                //Fill holes with the nearest covered pixel (above on a tie),
                //like the pad bytes around patch posts, so the edge rows the
                //column drawer can overrun into look right.
                for(int y = 0; y < height; y++)
                {
                    if(covered[y])
                        continue;

                    for(int d = 1; d < height; d++)
                    {
                        if(y - d >= 0 && covered[y - d])
                        {
                            dest[y] = dest[y - d];
                            break;
                        }

                        if(y + d < height && covered[y + d])
                        {
                            dest[y] = dest[y + d];
                            break;
                        }
                    }
                }

                //Repeat short textures so they tile.
                for(int y = height; height > 0 && y < colHeight; y++)
                    dest[y] = dest[y % height];

                quint16 id;

                auto it = poolIds.constFind(col);

                if(it != poolIds.constEnd())
                {
                    id = it.value();
                }
                else
                {
                    if(poolIds.size() > 0xffff)
                    {
                        qCritical("Too many unique texture columns.");
                        return false;
                    }

                    id = poolIds.size();
                    poolIds.insert(col, id);
                    pool.append(col);
                }

                tables.append((const char*)&id, sizeof(id));
            }

            tables.append(texRunIds);

            totalColumns += width;
        }
    }

    const quint32 numTextures = tableOfs.size();
    const quint32 headerSize = 4 + (numTextures * 4);

    QByteArray texCols;
    texCols.append((const char*)&numTextures, 4);

    for(quint32 i = 0; i < numTextures; i++)
    {
        quint32 ofs = headerSize + tableOfs[i];
        texCols.append((const char*)&ofs, 4);
    }

    texCols.append(tables);

    Lump texColsLump;
    texColsLump.name = "TEXCOLS";
    texColsLump.length = texCols.size();
    texColsLump.data = texCols;

    Lump poolLump;
    poolLump.name = "COLPOOL";
    poolLump.length = pool.size();
    poolLump.data = pool;

    Lump runsLump;
    runsLump.name = "COLRUNS";
    runsLump.length = runs.size();
    runsLump.data = runs;

    wadFile.InsertLump(wadFile.LumpCount(), texColsLump);
    wadFile.InsertLump(wadFile.LumpCount(), poolLump);
    wadFile.InsertLump(wadFile.LumpCount(), runsLump);

    qInfo("Texture columns: %u textures, %u columns, %d unique, COLPOOL %d bytes, TEXCOLS %d bytes, COLRUNS %d bytes.",
          numTextures, totalColumns, int(poolIds.size()), int(pool.size()), int(texCols.size()), int(runs.size()));

    return RemovePatches();
}

//Remove PNAMES and everything in the patch namespace (P_START..P_END,
//including P1_START etc. inside it). Only textures use them, and those
//now come from COLPOOL.
bool WadProcessor::RemovePatches()
{
    static const QRegularExpression startMarker("^P[P0-9]?_START$");
    static const QRegularExpression endMarker("^P[P0-9]?_END$");

    int depth = 0;
    quint32 removed = 0;
    quint32 removedBytes = 0;

    for(quint32 i = 0; i < wadFile.LumpCount(); i++)
    {
        Lump l;

        wadFile.GetLumpByNum(i, l);

        const QString name = l.name.toUpper();

        bool remove = (depth > 0) || (name == "PNAMES");

        if(startMarker.match(name).hasMatch())
        {
            depth++;
            remove = true;
        }
        else if(endMarker.match(name).hasMatch())
        {
            depth = qMax(depth - 1, 0);
            remove = true;
        }

        if(remove)
        {
            removed++;
            removedBytes += l.length;

            wadFile.RemoveLump(i);
            i--;
        }
    }

    qInfo("Removed %u patch lumps, %u bytes.", removed, removedBytes);

    return true;
}

//A PWAD map (e.g. Sigil's E3M1) replaces the IWAD's: lumps are found by
//name from the end, so the earlier copy is never loaded. Remove it: the
//marker and the map lumps after it.
bool WadProcessor::RemoveOverriddenMaps()
{
    static const QStringList mapLumpNames = {"THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS",
                                             "SSECTORS", "NODES", "SECTORS", "REJECT", "BLOCKMAP"};

    quint32 removedLumps = 0, removedBytes = 0;

    for(quint32 i = 0; i + 1 < wadFile.LumpCount(); i++)
    {
        Lump marker, next;

        wadFile.GetLumpByNum(i, marker);
        wadFile.GetLumpByNum(i+1, next);

        if(next.name.compare("THINGS", Qt::CaseInsensitive))
            continue;

        //Is there a later map with this name?
        Lump later;

        if(wadFile.GetLumpByName(marker.name, later) <= qint32(i))
            continue;

        quint32 end = i + 1;

        for(Lump l; wadFile.GetLumpByNum(end, l) && mapLumpNames.contains(l.name, Qt::CaseInsensitive); end++)
            removedBytes += l.length;

        removedBytes += marker.length;
        removedLumps += end - i;

        while(end-- > i)
            wadFile.RemoveLump(i);

        i--;
    }

    if(removedLumps)
        qInfo("Removed %u lumps of overridden maps, %u bytes.", removedLumps, removedBytes);

    return true;
}

bool WadProcessor::RemoveUnusedLumps()
{
    for(quint32 i = 0; i < wadFile.LumpCount(); i++)
    {
        Lump l;

        wadFile.GetLumpByNum(i, l);

        if(     l.name.startsWith("DP") ||
                l.name.startsWith("GENMIDI") ||
                l.name.startsWith("DMXGUS"))
        {
            wadFile.RemoveLump(i);
            i--;
        }
    }

    return true;
}

static double BesselI0(double x)
{
    double sum = 1.0, term = 1.0;

    for(int k = 1; k < 64; k++)
    {
        term *= (x / (2 * k)) * (x / (2 * k));
        sum += term;

        if(term < 1e-12 * sum)
            break;
    }

    return sum;
}

//Kaiser windowed sinc resampler. Lowpasses at the lower of the two Nyquist
//frequencies, so 22050Hz sounds don't alias when they're brought down.
static QByteArray ResampleSound(const std::vector<double>& in, double rate, double outRate)
{
    const double step = rate / outRate;
    const double cutoff = 0.5 * qMin(1.0, outRate / rate) * 0.95; //Cycles per input sample.
    const double halfWidth = 16.0 / (2.0 * cutoff);                //16 zero crossings each side.
    const double beta = 8.6;
    const double i0beta = BesselI0(beta);

    //Kernel tabulated by distance from the output position, then linearly interpolated.
    const int tableSteps = 256;
    std::vector<double> kernel(int(halfWidth * tableSteps) + 2, 0.0);

    for(size_t i = 0; i < kernel.size(); i++)
    {
        const double x = double(i) / tableSteps;

        if(x >= halfWidth)
            break;

        const double r = x / halfWidth;
        const double window = BesselI0(beta * std::sqrt(1.0 - r * r)) / i0beta;
        const double sinc = (x == 0.0) ? 2.0 * cutoff : std::sin(2.0 * M_PI * cutoff * x) / (M_PI * x);

        kernel[i] = sinc * window;
    }

    const qint64 inCount = qint64(in.size());
    const qint64 outCount = qint64(std::ceil(inCount / step));

    QByteArray out(outCount, 0);

    for(qint64 n = 0; n < outCount; n++)
    {
        const double t = n * step;

        const qint64 first = qMax<qint64>(0, qint64(std::ceil(t - halfWidth)));
        const qint64 last = qMin<qint64>(inCount - 1, qint64(std::floor(t + halfWidth)));

        double sum = 0.0;

        for(qint64 k = first; k <= last; k++)
        {
            const double pos = std::fabs(t - k) * tableSteps;
            const size_t i = size_t(pos);

            if(i + 1 >= kernel.size())
                continue;

            const double frac = pos - i;

            sum += in[k] * (kernel[i] + (kernel[i + 1] - kernel[i]) * frac);
        }

        out[n] = char(qBound(-128L, std::lround(sum), 127L));
    }

    return out;
}

//DMX sound lumps: drop the 16 bytes of padding either side and resample to signed
//8 bit at the GBA mixer's rate, so it can mix them without interpolating. The 8 byte
//header (format, rate, sample count) stays, with the new rate and count.
bool WadProcessor::ProcessSounds()
{
    for(quint32 i = 0; i < wadFile.LumpCount(); i++)
    {
        Lump l;

        wadFile.GetLumpByNum(i, l);

        if(!l.name.startsWith("DS") || l.length < 8)
            continue;

        const quint8* p = reinterpret_cast<const quint8*>(l.data.constData());

        if(qFromLittleEndian<quint16>(p) != 3)
            continue;

        const quint16 rate = qFromLittleEndian<quint16>(p + 2);
        quint32 count = qFromLittleEndian<quint32>(p + 4);

        count = qMin<quint32>(count, l.length - 8);

        quint32 first = 0;

        if(count > 32)
        {
            first = 16;
            count -= 32;
        }

        if(!rate || !count)
            continue;

        std::vector<double> samples(count);

        for(quint32 j = 0; j < count; j++)
            samples[j] = double(p[8 + first + j]) - 128.0;

        const QByteArray resampled = ResampleSound(samples, rate, GUSBANK_RATE);

        char header[8];
        qToLittleEndian<quint16>(3, header);
        qToLittleEndian<quint16>(GUSBANK_RATE, header + 2);
        qToLittleEndian<quint32>(resampled.size(), header + 4);

        Lump newLump;
        newLump.name = l.name;
        newLump.data = QByteArray(header, sizeof(header)) + resampled;
        newLump.length = newLump.data.size();

        wadFile.ReplaceLump(i, newLump);
    }

    return true;
}

//MUS (and type 1 MIDI) music lumps become type 0 MIDI for libtimidity.
bool WadProcessor::ProcessMusic()
{
    for(quint32 i = 0; i < wadFile.LumpCount(); i++)
    {
        Lump l;

        wadFile.GetLumpByNum(i, l);

        if(!l.name.startsWith("D_"))
            continue;

        const bool isMidi = l.data.startsWith("MThd");

        Lump newLump;
        newLump.name = l.name;
        newLump.data = isMidi ? ConvertMidiToType0(l.data) : ConvertMusToMidi(l.data);
        newLump.length = newLump.data.size();

        if(!newLump.length)
        {
            qWarning("%s is not a MUS or MIDI lump. Removed.", qPrintable(l.name));
            wadFile.RemoveLump(i);
            i--;
            continue;
        }

        wadFile.ReplaceLump(i, newLump);
    }

    return true;
}
