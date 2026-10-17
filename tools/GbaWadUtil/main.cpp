#include <QCoreApplication>

#include "options.h"
#include "wadfile.h"
#include "wadprocessor.h"
#include "gusbank.h"
#include "sizereport.h"
#include "gui/gui.h"
#include "../../include/doom_iwad.h"

bool SaveBytesAsCFile(const QByteArray& bytes, QString file);
bool SaveBytesToRom(const QByteArray& bytes, QString romIn, QString romOut);
bool ReadEngineRom(const QString& romIn, QByteArray& rom, int& headerOffset);

//Layout of doom_iwad_header_t in the engine's doom_iwad.h.
static const quint32 romIwadVersion = DOOM_IWAD_VERSION;
static_assert(sizeof(doom_iwad_header_t) == romIwadHeaderSize, "doom_iwad_header_t size changed");
static const int romMaxSize = 32 * 1024 * 1024;


int main(int argc, char *argv[])
{
    //No arguments, or -gui: the window. -gui fills it in from the other arguments.
    bool gui = argc < 2;

    for(int i = 1; i < argc; i++)
    {
        if(!strcmp(argv[i], "-gui"))
            gui = true;
    }

    if(gui)
        return RunGui(argc, argv);

    QCoreApplication a(argc, argv);

    QStringList ignored;
    const WadUtilOptions o = ParseArguments(QCoreApplication::arguments().mid(1), &ignored);

    for(const QString& s : ignored)
        qWarning("Ignored %s.", qPrintable(s));

    WadFile wf(o.inFile);

    if(!wf.LoadWadFile())
    {
        qCritical("Can't read WAD %s", qPrintable(o.inFile));
        return 1;
    }

    qInfo("Loaded %s.", qPrintable(QFileInfo(o.inFile).fileName()));

    WadProcessor wp(wf);
    wp.SetDehackedFiles(o.dehs);

    for(const QString& pwad : o.pwads)
    {
        WadFile pf(pwad);

        if(!pf.LoadWadFile())
        {
            qCritical("Can't read WAD %s", qPrintable(pwad));
            return 1;
        }

        wf.MergeWadFile(pf);
        qInfo("Merged %s.", qPrintable(QFileInfo(pwad).fileName()));
    }

    //Also insert the GBADoom wad file. (Extra menu options etc)
    //The engine can't start without its lumps, so it's not optional.
    WadFile gf(GbaWadPath());

    if(!gf.LoadWadFile())
    {
        qCritical("Can't read %s. GbaWadUtil needs it in data/ next to GbaWadUtil.exe.", qPrintable(QDir::toNativeSeparators(GbaWadPath())));
        return 1;
    }

    wf.MergeWadFile(gf);
    qInfo("Merged gbadoom.wad.");

    const QString gusCfg = FindGusConfig(o.gusCfg);

    if(!wp.ProcessWad())
    {
        qCritical("Processing %s failed.", qPrintable(o.inFile));
        return 1;
    }

    //GUS patches for the music. Without it the music is silent.
    if(gusCfg.length())
    {
        Lump bankLump;

        if(!BuildGusBank(wf, gusCfg, bankLump, o.oversample))
            return 1;

        wf.InsertLump(wf.LumpCount(), bankLump);
    }

    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);

    wf.SaveWadFile(&buffer);

    //The engine, up to the end of its IWAD header.
    qint64 engineBytes = 0;

    if(o.romIn.length())
    {
        QByteArray rom;
        int headerOffset;

        if(!ReadEngineRom(o.romIn, rom, headerOffset))
            return 1;

        engineBytes = headerOffset + romIwadHeaderSize;
    }

    PrintSizes(wf, byteArray.size(), engineBytes);

    if(o.sizeOnly)
    {
        if(engineBytes && engineBytes + byteArray.size() > romMaxSize)
            qWarning("The ROM would be %lld bytes, more than the 32MB a GBA cart can hold.", engineBytes + byteArray.size());

        return 0;
    }

    if(o.outFile.length())
    {
        QFile f(o.outFile);

        if(!f.open(QIODevice::Truncate | QIODevice::WriteOnly) || f.write(byteArray) != byteArray.size())
        {
            qCritical("Can't write WAD %s", qPrintable(o.outFile));
            return 1;
        }

        qInfo("Wrote %s, %d bytes.", qPrintable(o.outFile), int(byteArray.size()));
    }

    if(o.cFile.length())
    {
        if(!SaveBytesAsCFile(byteArray, o.cFile))
        {
            qCritical("Can't write C file %s", qPrintable(o.cFile));
            return 1;
        }

        qInfo("Wrote %s.", qPrintable(o.cFile));
    }

    if(o.romIn.length() || o.romOut.length())
    {
        if(!o.romIn.length() || !o.romOut.length())
        {
            qCritical("-rom and -romout must both be given.");
            return 1;
        }

        if(!SaveBytesToRom(byteArray, o.romIn, o.romOut))
            return 1;
    }

    return 0;
}

//Reads an engine ROM and finds the IWAD header that gbadoom.ld puts at its end.
bool ReadEngineRom(const QString& romIn, QByteArray& rom, int& headerOffset)
{
    QFile in(romIn);

    if(!in.open(QIODevice::ReadOnly))
    {
        qCritical("Can't open ROM %s", qPrintable(romIn));
        return false;
    }

    rom = in.readAll();
    headerOffset = FindRomIwadHeader(rom);

    if(headerOffset < 0)
    {
        qCritical("IWAD header not found in %s. Not a GBADoom ROM, or too old a version.", qPrintable(romIn));
        return false;
    }

    return true;
}

//Drop anything after the ROM's IWAD header (a previously attached IWAD) and append this one.
bool SaveBytesToRom(const QByteArray& bytes, QString romIn, QString romOut)
{
    QByteArray rom;
    int headerOffset;

    if(!ReadEngineRom(romIn, rom, headerOffset))
        return false;

    if(rom.size() > headerOffset + romIwadHeaderSize)
        qInfo("Replaced the WAD already in %s.", qPrintable(QFileInfo(romIn).fileName()));

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

    qInfo("Wrote %s, %d bytes.", qPrintable(romOut), int(rom.size()));

    return true;
}

bool SaveBytesAsCFile(const QByteArray& bytes, QString file)
{
    QFile f(file);

    if(!f.open(QIODevice::Truncate | QIODevice::ReadWrite))
        return false;

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

    return f.error() == QFileDevice::NoError;
}
