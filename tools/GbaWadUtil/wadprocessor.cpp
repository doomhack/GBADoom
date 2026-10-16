#include "wadprocessor.h"
#include "doomtypes.h"
#include "mus2mid.h"
#include "gusbank.h"
#include "../../include/doom_iwad.h"

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
    const QStringList maps = MapNames();

    //Can't find any maps.
    if(maps.isEmpty())
        return false;

    RemoveOverriddenMaps();
    RemoveUnusedTextures();
    ProcessTextureColumns();
    ProcessSounds();
    ProcessMusic();

    //Before ProcessPalette, which removes PLAYPAL.
    if(!ProcessPages())
        return false;

    if(!ProcessPalette())
        return false;

    for(const QString& map : maps)
    {
        Lump l;
        ProcessLevel(wadFile.GetLumpByName(map, l));
    }

    return KeepUsedLumps();
}

bool WadProcessor::ProcessLevel(quint32 lumpNum)
{
    ProcessVertexes(lumpNum);
    ProcessLines(lumpNum);
    ProcessSegs(lumpNum);
    ProcessSides(lumpNum);
    ProcessBlockmap(lumpNum);
    ProcessReject(lumpNum);
    ProcessSectors(lumpNum);
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

//The engine finds sector flats by name and lump names are uppercase, but
//some maps have lowercase flat names (TNT's MAP11 has ceil3_4 and flat10).
bool WadProcessor::ProcessSectors(quint32 lumpNum)
{
    Lump sectors;

    if(!wadFile.GetLumpByNum(lumpNum+ML_SECTORS, sectors))
        return false;

    mapsector_t* sector = reinterpret_cast<mapsector_t*>(sectors.data.data());

    for(quint32 i = 0; i < sectors.length / sizeof(mapsector_t); i++)
    {
        for(int c = 0; c < 8 && sector[i].floorpic[c]; c++)
            sector[i].floorpic[c] = toupper(sector[i].floorpic[c]);

        for(int c = 0; c < 8 && sector[i].ceilingpic[c]; c++)
            sector[i].ceilingpic[c] = toupper(sector[i].ceilingpic[c]);
    }

    wadFile.ReplaceLump(lumpNum+ML_SECTORS, sectors);

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

//Doom's gamma correction (gammatable in id's v_video.c), one row per
//level, applied to each 8 bit channel of PLAYPAL.
static const quint8 gammatable[GBAPAL_GAMMA_LEVELS][256] =
{
    {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
     17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
     33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
     49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,
     65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,
     81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,
     97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,
     113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
     128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
     144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
     160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
     176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
     192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
     208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
     224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
     240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255},
    {2,4,5,7,8,10,11,12,14,15,16,18,19,20,21,23,
     24,25,26,27,29,30,31,32,33,34,36,37,38,39,40,41,
     42,44,45,46,47,48,49,50,51,52,54,55,56,57,58,59,
     60,61,62,63,64,65,66,67,69,70,71,72,73,74,75,76,
     77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,
     93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,
     109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,
     125,126,127,128,129,129,130,131,132,133,134,135,136,137,138,139,
     140,141,142,143,144,145,146,147,148,148,149,150,151,152,153,154,
     155,156,157,158,159,160,161,162,163,163,164,165,166,167,168,169,
     170,171,172,173,174,175,175,176,177,178,179,180,181,182,183,184,
     185,186,186,187,188,189,190,191,192,193,194,195,196,196,197,198,
     199,200,201,202,203,204,205,205,206,207,208,209,210,211,212,213,
     214,214,215,216,217,218,219,220,221,222,222,223,224,225,226,227,
     228,229,230,230,231,232,233,234,235,236,237,237,238,239,240,241,
     242,243,244,245,245,246,247,248,249,250,251,252,252,253,254,255},
    {4,7,9,11,13,15,17,19,21,22,24,26,27,29,30,32,
     33,35,36,38,39,40,42,43,45,46,47,48,50,51,52,54,
     55,56,57,59,60,61,62,63,65,66,67,68,69,70,72,73,
     74,75,76,77,78,79,80,82,83,84,85,86,87,88,89,90,
     91,92,93,94,95,96,97,98,100,101,102,103,104,105,106,107,
     108,109,110,111,112,113,114,114,115,116,117,118,119,120,121,122,
     123,124,125,126,127,128,129,130,131,132,133,133,134,135,136,137,
     138,139,140,141,142,143,144,144,145,146,147,148,149,150,151,152,
     153,153,154,155,156,157,158,159,160,160,161,162,163,164,165,166,
     166,167,168,169,170,171,172,172,173,174,175,176,177,178,178,179,
     180,181,182,183,183,184,185,186,187,188,188,189,190,191,192,193,
     193,194,195,196,197,197,198,199,200,201,201,202,203,204,205,206,
     206,207,208,209,210,210,211,212,213,213,214,215,216,217,217,218,
     219,220,221,221,222,223,224,224,225,226,227,228,228,229,230,231,
     231,232,233,234,235,235,236,237,238,238,239,240,241,241,242,243,
     244,244,245,246,247,247,248,249,250,251,251,252,253,254,254,255},
    {8,12,16,19,22,24,27,29,31,34,36,38,40,41,43,45,
     47,49,50,52,53,55,57,58,60,61,63,64,65,67,68,70,
     71,72,74,75,76,77,79,80,81,82,84,85,86,87,88,90,
     91,92,93,94,95,96,98,99,100,101,102,103,104,105,106,107,
     108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,
     124,125,126,127,128,129,130,131,132,133,134,135,135,136,137,138,
     139,140,141,142,143,143,144,145,146,147,148,149,150,150,151,152,
     153,154,155,155,156,157,158,159,160,160,161,162,163,164,165,165,
     166,167,168,169,169,170,171,172,173,173,174,175,176,176,177,178,
     179,180,180,181,182,183,183,184,185,186,186,187,188,189,189,190,
     191,192,192,193,194,195,195,196,197,197,198,199,200,200,201,202,
     202,203,204,205,205,206,207,207,208,209,210,210,211,212,212,213,
     214,214,215,216,216,217,218,219,219,220,221,221,222,223,223,224,
     225,225,226,227,227,228,229,229,230,231,231,232,233,233,234,235,
     235,236,237,237,238,238,239,240,240,241,242,242,243,244,244,245,
     246,246,247,247,248,249,249,250,251,251,252,253,253,254,254,255},
    {16,23,28,32,36,39,42,45,48,50,53,55,57,60,62,64,
     66,68,69,71,73,75,76,78,80,81,83,84,86,87,89,90,
     92,93,94,96,97,98,100,101,102,103,105,106,107,108,109,110,
     112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,128,
     128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
     143,144,145,146,147,148,149,150,150,151,152,153,154,155,155,156,
     157,158,159,159,160,161,162,163,163,164,165,166,166,167,168,169,
     169,170,171,172,172,173,174,175,175,176,177,177,178,179,180,180,
     181,182,182,183,184,184,185,186,187,187,188,189,189,190,191,191,
     192,193,193,194,195,195,196,196,197,198,198,199,200,200,201,202,
     202,203,203,204,205,205,206,207,207,208,208,209,210,210,211,211,
     212,213,213,214,214,215,216,216,217,217,218,219,219,220,220,221,
     221,222,223,223,224,224,225,225,226,227,227,228,228,229,229,230,
     230,231,232,232,233,233,234,234,235,235,236,236,237,237,238,239,
     239,240,240,241,241,242,242,243,243,244,244,245,245,246,246,247,
     247,248,248,249,249,250,250,251,251,252,252,253,254,254,255,255},
};

//The 5 bit GBA level that shows closest to an 8 bit channel. The GBA's
//5 bits are shown as 8 by repeating the top bits, (c << 3) | (c >> 2),
//so 31 is 255 and plain rounding (or truncating) to 5 bits is a little
//off, most of all on dark colours. Each channel is converted on its own,
//so greys stay grey. (Mixing levels across channels to keep brightness
//closer was tried, and gave visible green and purple tints.)
static quint16 To5Bit(int v)
{
    int best = 0;
    int bestErr = 256;

    for(int c = 0; c < 32; c++)
    {
        const int err = std::abs(((c << 3) | (c >> 2)) - v);

        if(err < bestErr)
        {
            best = c;
            bestErr = err;
        }
    }

    return quint16(best);
}

//PLAYPAL (14 palettes of 256 RGB triplets) becomes GBAPAL: every palette
//as GBA RGB15 for each of Doom's gamma levels, so the game copies them
//straight to palette RAM. The PLAYPAL used is the last one, so a PWAD's
//own palette wins. Every PLAYPAL lump is removed.
bool WadProcessor::ProcessPalette()
{
    Lump pal;

    const qint32 palNum = wadFile.GetLumpByName("PLAYPAL", pal);

    if(palNum == -1 || pal.length < GBAPAL_PALETTES * GBAPAL_COLOURS * 3)
    {
        qCritical("PLAYPAL missing or too short.");
        return false;
    }

    const quint8* src = reinterpret_cast<const quint8*>(pal.data.constData());

    QByteArray out(GBAPAL_GAMMA_LEVELS * GBAPAL_PALETTES * GBAPAL_COLOURS * 2, 0);
    char* dst = out.data();

    for(int level = 0; level < GBAPAL_GAMMA_LEVELS; level++)
    {
        for(int i = 0; i < GBAPAL_PALETTES * GBAPAL_COLOURS; i++)
        {
            const quint16 r = To5Bit(gammatable[level][src[i*3+0]]);
            const quint16 g = To5Bit(gammatable[level][src[i*3+1]]);
            const quint16 b = To5Bit(gammatable[level][src[i*3+2]]);

            qToLittleEndian<quint16>(r | (g << 5) | (b << 10), dst);
            dst += 2;
        }
    }

    for(quint32 i = 0; i < wadFile.LumpCount(); i++)
    {
        Lump l;

        wadFile.GetLumpByNum(i, l);

        if(l.name.startsWith("PLAYPAL"))
        {
            wadFile.RemoveLump(i);
            i--;
        }
    }

    Lump newLump;
    newLump.name = "GBAPAL";
    newLump.data = out;
    newLump.length = out.size();

    wadFile.InsertLump(wadFile.LumpCount(), newLump);

    return true;
}

//Draw a patch lump onto a canvas of palette indices, at x,y less its
//offsets, as V_DrawPatch would. False if it isn't a valid patch.
static bool DrawPatch(const QByteArray& lump, int x, int y, std::vector<quint8>& canvas, int canvasWidth, int canvasHeight)
{
    const quint8* patch = reinterpret_cast<const quint8*>(lump.constData());
    const quint32 size = quint32(lump.size());

    if(size < 8)
        return false;

    const int width = qFromLittleEndian<qint16>(patch);
    const int height = qFromLittleEndian<qint16>(patch + 2);

    if(width <= 0 || height <= 0 || size < 8 + quint32(width) * 4)
        return false;

    x -= qFromLittleEndian<qint16>(patch + 4);
    y -= qFromLittleEndian<qint16>(patch + 6);

    for(int c = 0; c < width; c++)
    {
        quint32 post = qFromLittleEndian<quint32>(patch + 8 + c*4);

        while(true)
        {
            if(post >= size)
                return false;

            if(patch[post] == 0xff)
                break;

            const int top = patch[post];
            const int length = (post + 1 < size) ? patch[post + 1] : 0;

            if(post + 4 + length > size)
                return false;

            for(int i = 0; i < length; i++)
            {
                const int px = x + c;
                const int py = y + top + i;

                if(px >= 0 && px < canvasWidth && py >= 0 && py < canvasHeight)
                    canvas[py * canvasWidth + px] = patch[post + 3 + i];
            }

            post += length + 4;
        }
    }

    return true;
}

//For each destination pixel along one axis, the source pixels it covers
//and how much of each.
static std::vector<std::vector<std::pair<int, double>>> BoxWeights(int srcSize, int dstSize)
{
    std::vector<std::vector<std::pair<int, double>>> weights(dstSize);

    const double scale = double(srcSize) / dstSize;

    for(int d = 0; d < dstSize; d++)
    {
        const double a = d * scale;
        const double b = (d + 1) * scale;

        for(int s = int(a); s < b && s < srcSize; s++)
        {
            const double w = qMin(b, s + 1.0) - qMax(a, double(s));

            if(w > 1e-9)
                weights[d].push_back({s, w / scale});
        }
    }

    return weights;
}

//Box filter: each destination pixel is the average, in linear light, of
//the source area it covers, matched to the nearest palette colour. Areas
//of one colour keep it exactly.
static QByteArray DownscalePage(const std::vector<quint8>& src, int srcWidth, int srcHeight,
                                int dstWidth, int dstHeight, const quint8* playpal)
{
    double toLinear[256];

    for(int i = 0; i < 256; i++)
        toLinear[i] = std::pow(i / 255.0, 2.2);

    const auto xWeights = BoxWeights(srcWidth, dstWidth);
    const auto yWeights = BoxWeights(srcHeight, dstHeight);

    QByteArray out(dstWidth * dstHeight, 0);

    for(int dy = 0; dy < dstHeight; dy++)
    {
        for(int dx = 0; dx < dstWidth; dx++)
        {
            double rgb[3] = {0, 0, 0};

            const int first = src[yWeights[dy][0].first * srcWidth + xWeights[dx][0].first];
            bool oneColour = true;

            for(const auto& wy : yWeights[dy])
            {
                for(const auto& wx : xWeights[dx])
                {
                    const int p = src[wy.first * srcWidth + wx.first];
                    const double w = wy.second * wx.second;

                    oneColour = oneColour && (p == first);

                    for(int c = 0; c < 3; c++)
                        rgb[c] += toLinear[playpal[p*3 + c]] * w;
                }
            }

            int best = first;

            if(!oneColour)
            {
                for(int c = 0; c < 3; c++)
                    rgb[c] = 255.0 * std::pow(rgb[c], 1.0 / 2.2);

                //"Redmean" weighted distance, closer to how different colours look than plain RGB.
                double bestErr = 1e30;

                for(int i = 0; i < GBAPAL_COLOURS; i++)
                {
                    const double r = playpal[i*3 + 0], g = playpal[i*3 + 1], b = playpal[i*3 + 2];
                    const double rmean = (r + rgb[0]) / 2;
                    const double dr = r - rgb[0], dg = g - rgb[1], db = b - rgb[2];

                    const double err = (2 + rmean / 256) * dr*dr + 4 * dg*dg + (2 + (255 - rmean) / 256) * db*db;

                    if(err < bestErr)
                    {
                        best = i;
                        bestErr = err;
                    }
                }
            }

            out[dy * dstWidth + dx] = char(best);
        }
    }

    return out;
}

//Draw the last lump called name onto the canvas at x,0.
static bool DrawPageLump(WadFile& wadFile, const char* name, int x, std::vector<quint8>& canvas, int canvasWidth)
{
    Lump l;

    if(wadFile.GetLumpByName(name, l) == -1)
        return false;

    if(!DrawPatch(l.data, x, 0, canvas, canvasWidth, 200))
    {
        qWarning("%s is not a valid patch, so it isn't converted. The game will stop when it shows it.", name);
        return false;
    }

    return true;
}

static void RemoveLumpsNamed(WadFile& wadFile, const char* name)
{
    Lump l;

    for(qint32 i; (i = wadFile.GetLumpByName(name, l)) != -1; )
        wadFile.RemoveLump(i);
}

//Replace every lump called name with one new one.
static void ReplacePageLump(WadFile& wadFile, const char* name, const QByteArray& data)
{
    RemoveLumpsNamed(wadFile, name);

    Lump newLump;
    newLump.name = name;
    newLump.data = data;
    newLump.length = data.size();

    wadFile.InsertLump(wadFile.LumpCount(), newLump);
}

//Full-screen pictures, all drawn at 0,0 on Doom's 320x200 screen. They
//become raw pixels at the GBA's resolution (RAWPAGE_WIDTH in doom_iwad.h),
//so the game copies them to the screen instead of scaling a patch every
//frame. The E3 ending scrolls from PFUB2 to PFUB1, so those two are drawn
//side by side and downscaled together as BUNNYPIC, with no seam.
bool WadProcessor::ProcessPages()
{
    static const char* const pageNames[] = {"TITLEPIC", "INTERPIC", "WIMAP0", "WIMAP1", "WIMAP2",
                                            "CREDIT", "HELP2", "VICTORY2", "ENDPIC", "BOSSBACK"};

    Lump pal;

    if(wadFile.GetLumpByName("PLAYPAL", pal) == -1 || pal.length < GBAPAL_COLOURS * 3)
    {
        qCritical("PLAYPAL missing or too short.");
        return false;
    }

    const quint8* playpal = reinterpret_cast<const quint8*>(pal.data.constData());

    for(const char* name : pageNames)
    {
        std::vector<quint8> canvas(320 * 200, 0);

        if(DrawPageLump(wadFile, name, 0, canvas, 320))
            ReplacePageLump(wadFile, name, DownscalePage(canvas, 320, 200, RAWPAGE_WIDTH, RAWPAGE_HEIGHT, playpal));
    }

    //F_BunnyScroll draws PFUB1 at 320-scrolled, then PFUB2 at -scrolled.
    std::vector<quint8> strip(640 * 200, 0);

    const bool pfub1 = DrawPageLump(wadFile, "PFUB1", 320, strip, 640);
    const bool pfub2 = DrawPageLump(wadFile, "PFUB2", 0, strip, 640);

    if(pfub1 || pfub2)
    {
        ReplacePageLump(wadFile, "BUNNYPIC", DownscalePage(strip, 640, 200, BUNNYPIC_WIDTH, RAWPAGE_HEIGHT, playpal));
        RemoveLumpsNamed(wadFile, "PFUB1");
        RemoveLumpsNamed(wadFile, "PFUB2");
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
std::vector<double> SincResample(const std::vector<double>& in, double rate, double outRate)
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

    std::vector<double> out(outCount, 0.0);

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

        out[n] = sum;
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

        const std::vector<double> out = SincResample(samples, rate, GUSBANK_RATE);

        QByteArray resampled(qsizetype(out.size()), 0);

        for(size_t j = 0; j < out.size(); j++)
            resampled[j] = char(qBound(-128L, std::lround(out[j]), 127L));

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
