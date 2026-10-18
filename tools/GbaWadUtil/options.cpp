#include "options.h"

static const char romIwadMagic[16] = {'G','B','A','D','O','O','M','-','I','W','A','D','-','H','D','R'};

WadUtilOptions ParseArguments(const QStringList& args, QStringList* ignored)
{
    WadUtilOptions o;

    auto ignore = [&](const QString& what)
    {
        if(ignored)
            ignored->append(what);
    };

    QString* const values[] = {&o.inFile, &o.outFile, &o.romIn, &o.romOut, &o.gusCfg};
    const char* const names[] = {"-in", "-out", "-rom", "-romout", "-gus"};

    for(int i = 0; i < args.count(); i++)
    {
        const QString& a = args.at(i);
        const bool hasValue = i + 1 < args.count();

        int named = -1;

        for(int n = 0; n < 5; n++)
        {
            if(a == names[n])
                named = n;
        }

        if(named >= 0 || a == "-oversample")
        {
            //A value can start with -, but there has to be one.
            if(!hasValue)
            {
                ignore(a + " (no value)");
                continue;
            }

            const QString v = args.at(++i);

            if(named >= 0)
            {
                *values[named] = v;
                continue;
            }

            bool ok;
            o.oversample = v.toInt(&ok);

            if(!ok)
                ignore(QString("-oversample %1 (not a number, using 1x)").arg(v));

            o.oversample = qMax(o.oversample, 1);
        }
        else if(a == "-pwad" || a == "-deh")
        {
            QStringList& list = (a == "-pwad") ? o.pwads : o.dehs;

            while(i + 1 < args.count() && !args.at(i + 1).startsWith("-"))
                list.append(args.at(++i));
        }
        else if(a == "-gui")
        {
            o.gui = true;
        }
        else if(a == "-size")
        {
            o.sizeOnly = true;
        }
        else if(a.startsWith("-"))
        {
            ignore(a + " (unknown)");
        }
        else
        {
            ignore(a + " (not after an option)");
        }
    }

    return o;
}

QStringList BuildArguments(const WadUtilOptions& o)
{
    QStringList a;

    if(o.inFile.length())
        a << "-in" << o.inFile;

    if(o.pwads.length())
        a << "-pwad" << o.pwads;

    if(o.dehs.length())
        a << "-deh" << o.dehs;

    if(o.gusCfg.length())
    {
        a << "-gus" << o.gusCfg;

        if(o.oversample != 1)
            a << "-oversample" << QString::number(o.oversample);
    }

    if(o.romIn.length())
        a << "-rom" << o.romIn;

    if(o.romOut.length())
        a << "-romout" << o.romOut;

    if(o.outFile.length())
        a << "-out" << o.outFile;

    if(o.sizeOnly)
        a << "-size";

    return a;
}

QString DataDir()
{
    return QCoreApplication::applicationDirPath() + "/data";
}

QString GbaWadPath()
{
    return DataDir() + "/gbadoom.wad";
}

QString GusPatchDir()
{
    return DataDir() + "/dgguspat";
}

QString FindGusConfig(const QString& cfg)
{
    if(cfg.isEmpty() || QFileInfo::exists(cfg))
        return cfg;

    const QString bundled = GusPatchDir() + "/" + QFileInfo(cfg).fileName();

    return QFileInfo::exists(bundled) ? bundled : cfg;
}

//The header is 32 byte aligned.
int FindRomIwadHeader(const QByteArray& rom)
{
    for(int i = 0; i + romIwadHeaderSize <= rom.size(); i += romIwadHeaderSize)
    {
        if(memcmp(rom.constData() + i, romIwadMagic, sizeof(romIwadMagic)) == 0)
            return i;
    }

    return -1;
}
