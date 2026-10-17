#ifndef OPTIONS_H
#define OPTIONS_H

#include <QtCore>

//GbaWadUtil's command line, shared by the CLI and the window (which runs
//builds as a GbaWadUtil child process with these arguments).
struct WadUtilOptions
{
    QString inFile;
    QString outFile;
    QString cFile;
    QString romIn;
    QString romOut;
    QString gusCfg;
    int oversample = 1;
    QStringList pwads;
    QStringList dehs;
    bool gui = false;

    //-size: process the WAD and report the space it takes, writing nothing.
    bool sizeOnly = false;
};

//args without the program name. Each argument the CLI can't use goes in
//ignored, as "-verbose (unknown)".
WadUtilOptions ParseArguments(const QStringList& args, QStringList* ignored = nullptr);

//The arguments that ParseArguments turns back into options.
QStringList BuildArguments(const WadUtilOptions& options);

//GbaWadUtil's own files, in data/ next to the exe: gbadoom.wad, which is
//merged into every WAD, and the GUS patches.
QString DataDir();
QString GbaWadPath();
QString GusPatchDir();

//A -gus config as given or, if there's no such file, the one of that name
//in GusPatchDir(), so -gus timidity.cfg and the old dgguspat\timidity.cfg work.
QString FindGusConfig(const QString& cfg);

//Size of the IWAD header that gbadoom.ld puts at the end of the ROM
//(doom_iwad_header_t in the engine's doom_iwad.h).
static const int romIwadHeaderSize = 32;

//Offset of the IWAD header in a GBADoom ROM, or -1 if it has none.
int FindRomIwadHeader(const QByteArray& rom);

#endif // OPTIONS_H
