#include "wadfile.h"
#include "doomtypes.h"

#define ROUND_UP4(x) ((x+3) & -4)

WadFile::WadFile(QString filePath, QObject *parent)
    : QObject(parent)
{
    wadPath = filePath;
}

bool WadFile::LoadWadFile()
{
    QFile f(wadPath);

    if(!f.open(QIODevice::ReadOnly))
        return false;

    QByteArray fd = f.readAll();

    const char* wadData = fd.constData();



    f.close();

    const wadinfo_t* header = reinterpret_cast<const wadinfo_t*>(wadData);

    QString id = QString(QLatin1String(header->identification, 4));

    if(QString::compare(id, "IWAD") && QString::compare(id, "PWAD"))
        return false;

    const filelump_t* fileinfo = reinterpret_cast<const filelump_t*>(&wadData[header->infotableofs]);

    for(int i = 0; i < header->numlumps; i++)
    {
        Lump l;
        l.name = QString(QLatin1String(fileinfo[i].name, 8));
        l.name = QString(QLatin1String(l.name.toLatin1().constData()));

        l.length = fileinfo[i].size;
        l.data = QByteArray(&wadData[fileinfo[i].filepos], fileinfo[i].size);

        this->lumps.append(l);
    }

    return true;
}

bool WadFile::SaveWadFile(QString filePath)
{
    QFile f(filePath);

    if(!f.open(QIODevice::Truncate | QIODevice::ReadWrite))
        return false;

    bool ret = SaveWadFile(&f);
    f.close();

    return ret;
}

bool WadFile::SaveWadFile(QIODevice* device)
{
    if(!device->isOpen() || !device->isWritable())
        return false;

    wadinfo_t header;

    header.numlumps = lumps.count();

    header.identification[0] = 'I';
    header.identification[1] = 'W';
    header.identification[2] = 'A';
    header.identification[3] = 'D';

    header.infotableofs = sizeof(wadinfo_t);

    device->write(reinterpret_cast<const char*>(&header), sizeof(header));

    quint32 fileOffset = sizeof(wadinfo_t) + (sizeof(filelump_t)*lumps.count());

    fileOffset = ROUND_UP4(fileOffset);


    //Write the file info blocks.
    for(int i = 0; i < lumps.count(); i++)
    {
        Lump l = lumps.at(i);

        filelump_t fl;

        memset(fl.name, 0, 8);
        strncpy(fl.name, l.name.toLatin1().toUpper().constData(), 8);

        fl.size = l.length;

        if(l.length > 0)
            fl.filepos = fileOffset;
        else
            fl.filepos = 0;

        device->write(reinterpret_cast<const char*>(&fl), sizeof(fl));

        fileOffset += l.length;
        fileOffset = ROUND_UP4(fileOffset);
    }

    //Write the lump data out.
    for(int i = 0; i < lumps.count(); i++)
    {
        Lump l = lumps.at(i);

        if(l.length == 0)
            continue;

        quint32 pos = device->pos();

        pos = ROUND_UP4(pos);

        device->seek(pos);

        device->write(l.data, l.length);
    }

    return true;
}

qint32 WadFile::GetLumpByName(QString name, Lump& lump)
{
    for(int i = lumps.length()-1; i >= 0; i--)
    {
        if(!lumps.at(i).name.compare(name, Qt::CaseInsensitive))
        {
            lump = lumps.at(i);
            return i;
        }
    }

    return -1;
}

bool WadFile::GetLumpByNum(quint32 lumpnum, Lump& lump)
{
    if(lumpnum >= lumps.count())
        return false;

    lump = lumps.at(lumpnum);

    return true;
}

bool WadFile::ReplaceLump(quint32 lumpnum, Lump newLump)
{
    if(lumpnum >= lumps.count())
        return false;

    lumps.replace(lumpnum, newLump);

    return true;
}

bool WadFile::InsertLump(quint32 lumpnum, Lump newLump)
{
    lumps.insert(lumpnum, newLump);

    return true;
}

bool WadFile::RemoveLump(quint32 lumpnum)
{
    if(lumpnum >= lumps.count())
        return false;

    lumps.removeAt(lumpnum);

    return true;
}

quint32 WadFile::LumpCount()
{
    return lumps.count();
}

bool WadFile::MergeWadFile(WadFile& wadFile)
{
    for(quint32 i = 0; i < wadFile.LumpCount(); i++)
    {
        Lump l;

        wadFile.GetLumpByNum(i, l);

        InsertLump(0xffff, l);
    }

    return true;
}
