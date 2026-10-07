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
    ProcessPNames();

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

bool WadProcessor::ProcessPNames()
{
    Lump pnamesLump;
    qint32 lumpNum = wadFile.GetLumpByName("PNAMES", pnamesLump);

    if(lumpNum == -1)
        return false;

    const char* pnamesData = (const char*)pnamesLump.data.constData();

    quint32 count = *((quint32*)pnamesData);

    pnamesData += 4; //Fist 4 bytes are count.

    QStringList pnamesUpper;

    for(quint32 i = 0; i < count; i++)
    {
        char n[9] = {0};
        strncpy(n, &pnamesData[i*8], 8);


        QLatin1String nl(n);
        QString newName(nl);

       pnamesUpper.push_back(newName.toUpper());
    }

    char* newPnames = new char[(count * 8) + 4];
    memset(newPnames, 0, (count * 8) + 4);

    *((quint32*)newPnames) = count; //Write count of pnames.

    char* newPnames2 = &newPnames[4]; //Start of name list.

    for(quint32 i = 0; i < count; i++)
    {
        QByteArray pl = pnamesUpper[i].toLatin1();

        strncpy(&newPnames2[i*8], pl.constData(), 8);
    }

    Lump newLump;
    newLump.name = "PNAMES";
    newLump.length = (count * 8) + 4;
    newLump.data = QByteArray(reinterpret_cast<const char*>(newPnames), newLump.length);

    delete[] newPnames;

    wadFile.ReplaceLump(lumpNum, newLump);

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
