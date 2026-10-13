#include <QCoreApplication>

#include "wadfile.h"
#include "wadprocessor.h"
#include "gusbank.h"

void SaveBytesAsCFile(QByteArray& bytes, QString file);
bool SaveBytesToRom(const QByteArray& bytes, QString romIn, QString romOut);

//Must match doom_iwad_header_t / DOOM_IWAD_VERSION in GBADoom's doom_iwad.h.
static const char romIwadMagic[16] = {'G','B','A','D','O','O','M','-','I','W','A','D','-','H','D','R'};
static const quint32 romIwadVersion = 12;
static const int romIwadHeaderSize = 32;
static const int romMaxSize = 32 * 1024 * 1024;


int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QStringList args = QCoreApplication::arguments();

    QString inFile;
    QString outFile;
    QString cFile;
    QString romIn;
    QString romOut;
    QString gusCfg;
    int oversample = 1;
    QStringList pwads;

    for(int i = 0; i < args.count()-1; i++)
    {
        if(args.at(i) == "-in")
            inFile = args.at(++i);
        else if(args.at(i) == "-out")
            outFile = args.at(++i);
        else if(args.at(i) == "-cfile")
            cFile = args.at(++i);
        else if(args.at(i) == "-rom")
            romIn = args.at(++i);
        else if(args.at(i) == "-romout")
            romOut = args.at(++i);
        else if(args.at(i) == "-gus")
            gusCfg = args.at(++i);
        else if(args.at(i) == "-oversample")
            oversample = args.at(++i).toInt();
        else if(args.at(i) == "-pwad")
        {
            i++;

            while( (i < args.count()) && !args.at(i).startsWith("-"))
            {
                pwads.append(args.at(i));
                i++;
            }

            //Let the for loop's i++ land on the next option.
            i--;
        }
    }

    //Also insert the GBADoom wad file. (Extra menu options etc)
    pwads.append(QCoreApplication::applicationDirPath().append("/gbadoom.wad"));

    WadFile wf(inFile);
    wf.LoadWadFile();

    WadProcessor wp(wf);

    for(int i = 0; i < pwads.length(); i++)
    {
        WadFile pf(pwads.at(i));
        pf.LoadWadFile();

        wf.MergeWadFile(pf);
    }


    wp.ProcessWad();

    //GUS patches for the music. Without it the music is silent.
    if(gusCfg.length())
    {
        Lump bankLump;

        if(!BuildGusBank(wf, gusCfg, bankLump, oversample))
            return 1;

        wf.InsertLump(wf.LumpCount(), bankLump);
    }

    if(outFile.length())
        wf.SaveWadFile(outFile);

    if(cFile.length())
    {
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);

        wf.SaveWadFile(&buffer);

        SaveBytesAsCFile(byteArray, cFile);
    }

    if(romIn.length() || romOut.length())
    {
        if(!romIn.length() || !romOut.length())
        {
            qCritical("-rom and -romout must both be given.");
            return 1;
        }

        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);

        wf.SaveWadFile(&buffer);

        if(!SaveBytesToRom(byteArray, romIn, romOut))
            return 1;
    }

    return 0;
}

//Find the IWAD header that gbadoom.ld puts at the end of the ROM,
//drop anything after it (a previously attached IWAD) and append this one.
bool SaveBytesToRom(const QByteArray& bytes, QString romIn, QString romOut)
{
    QFile in(romIn);

    if(!in.open(QIODevice::ReadOnly))
    {
        qCritical("Can't open ROM %s", qPrintable(romIn));
        return false;
    }

    QByteArray rom = in.readAll();
    in.close();

    int headerOffset = -1;

    //Header is 32 byte aligned.
    for(int i = 0; i + romIwadHeaderSize <= rom.size(); i += romIwadHeaderSize)
    {
        if(memcmp(rom.constData() + i, romIwadMagic, sizeof(romIwadMagic)) == 0)
        {
            headerOffset = i;
            break;
        }
    }

    if(headerOffset < 0)
    {
        qCritical("IWAD header not found in %s. Not a GBADoom ROM, or too old a version.", qPrintable(romIn));
        return false;
    }

    rom.truncate(headerOffset + romIwadHeaderSize);

    char* header = rom.data() + headerOffset;

    qToLittleEndian<quint32>(romIwadVersion, header + 16);
    qToLittleEndian<quint32>(bytes.size(), header + 20);
    memset(header + 24, 0, romIwadHeaderSize - 24);

    rom.append(bytes);

    if(rom.size() > romMaxSize)
    {
        qCritical("ROM is %d bytes, more than the 32MB a GBA cart can hold.", int(rom.size()));
        return false;
    }

    QFile out(romOut);

    if(!out.open(QIODevice::Truncate | QIODevice::WriteOnly) || out.write(rom) != rom.size())
    {
        qCritical("Can't write ROM %s", qPrintable(romOut));
        return false;
    }

    out.close();

    return true;
}

void SaveBytesAsCFile(QByteArray& bytes, QString file)
{
    QFile f(file);

    if(!f.open(QIODevice::Truncate | QIODevice::ReadWrite))
        return;

    QString decl = QString("const unsigned char doom_iwad[%1UL] = {\n").arg(bytes.size());

    f.write(decl.toLatin1());

    for(int i = 0; i < bytes.size(); i++)
    {
        QString element = QString("0x%1,").arg((quint8)bytes.at(i),2, 16, QChar('0'));

        if(( (i+1) % 40) == 0)
            element += "\n";

        f.write(element.toLatin1());
    }

    QString close = QString("\n};");
    f.write(close.toLatin1());

    f.close();
}
