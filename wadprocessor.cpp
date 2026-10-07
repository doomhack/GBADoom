#include "wadprocessor.h"
#include "doomtypes.h"
#include "mus2mid.h"
#include "gusbank.h"

#include <cmath>
#include <vector>

WadProcessor::WadProcessor(WadFile& wad, QObject *parent)
    : QObject(parent), wadFile(wad)
{

}

bool WadProcessor::ProcessWad()
{
    //Figure out if our IWAD is Doom or Doom2. (E1M1 or MAP01)

    Lump mapLump;

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

    for(unsigned int i = 0; i < lineCount; i++)
    {
        newLines[i].v1.x = vtx[oldLines[i].v1].x;
        newLines[i].v1.y = vtx[oldLines[i].v1].y;

        newLines[i].v2.x = vtx[oldLines[i].v2].x;
        newLines[i].v2.y = vtx[oldLines[i].v2].y;

        newLines[i].special = oldLines[i].special;
        newLines[i].flags = oldLines[i].flags;
        newLines[i].tag = oldLines[i].tag;

        newLines[i].dx = newLines[i].v2.x - newLines[i].v1.x;
        newLines[i].dy = newLines[i].v2.y - newLines[i].v1.y;

        newLines[i].slopetype =
                !newLines[i].dx ? ST_VERTICAL : !newLines[i].dy ? ST_HORIZONTAL :
                FixedDiv(newLines[i].dy, newLines[i].dx) > 0 ? ST_POSITIVE : ST_NEGATIVE;

        newLines[i].sidenum[0] = oldLines[i].sidenum[0];
        newLines[i].sidenum[1] = oldLines[i].sidenum[1];

        newLines[i].bbox[BOXLEFT] = (newLines[i].v1.x < newLines[i].v2.x ? newLines[i].v1.x : newLines[i].v2.x);
        newLines[i].bbox[BOXRIGHT] = (newLines[i].v1.x < newLines[i].v2.x ? newLines[i].v2.x : newLines[i].v1.x);

        newLines[i].bbox[BOXTOP] = (newLines[i].v1.y < newLines[i].v2.y ? newLines[i].v2.y : newLines[i].v1.y);
        newLines[i].bbox[BOXBOTTOM] = (newLines[i].v1.y < newLines[i].v2.y ? newLines[i].v1.y : newLines[i].v2.y);

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
        newSegs[i].v1.x = vtx[oldSegs[i].v1].x;
        newSegs[i].v1.y = vtx[oldSegs[i].v1].y;

        newSegs[i].v2.x = vtx[oldSegs[i].v2].x;
        newSegs[i].v2.y = vtx[oldSegs[i].v2].y;

        newSegs[i].angle = oldSegs[i].angle << 16;
        newSegs[i].offset = oldSegs[i].offset << 16;

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

bool WadProcessor::ProcessSides(quint32 lumpNum)
{
    quint32 sidesLumpNum = lumpNum+ML_SIDEDEFS;

    Lump sides;

    if(!wadFile.GetLumpByNum(sidesLumpNum, sides))
        return false;

    if(sides.length == 0)
        return false;

    quint32 sideCount = sides.length / sizeof(mapsidedef_t);

    sidedef_t* newSides = new sidedef_t[sideCount];

    const mapsidedef_t* oldSides = reinterpret_cast<const mapsidedef_t*>(sides.data.constData());

    for(unsigned int i = 0; i < sideCount; i++)
    {
        newSides[i].textureoffset = oldSides[i].textureoffset;
        newSides[i].rowoffset = oldSides[i].rowoffset;

        newSides[i].toptexture = GetTextureNumForName(oldSides[i].toptexture);
        newSides[i].bottomtexture = GetTextureNumForName(oldSides[i].bottomtexture);
        newSides[i].midtexture = GetTextureNumForName(oldSides[i].midtexture);

        newSides[i].sector = oldSides[i].sector;
    }

    Lump newSide;
    newSide.name = sides.name;
    newSide.length = sideCount * sizeof(sidedef_t);
    newSide.data = QByteArray(reinterpret_cast<const char*>(newSides), newSide.length);

    delete[] newSides;

    wadFile.ReplaceLump(sidesLumpNum, newSide);

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

//MUS music lumps become type 0 MIDI for libtimidity.
bool WadProcessor::ProcessMusic()
{
    for(quint32 i = 0; i < wadFile.LumpCount(); i++)
    {
        Lump l;

        wadFile.GetLumpByNum(i, l);

        if(!l.name.startsWith("D_") || l.data.startsWith("MThd"))
            continue;

        Lump newLump;
        newLump.name = l.name;
        newLump.data = ConvertMusToMidi(l.data);
        newLump.length = newLump.data.size();

        if(!newLump.length)
        {
            qWarning("%s is not a MUS lump. Removed.", qPrintable(l.name));
            wadFile.RemoveLump(i);
            i--;
            continue;
        }

        wadFile.ReplaceLump(i, newLump);
    }

    return true;
}
