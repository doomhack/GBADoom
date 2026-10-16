#ifndef WADPROCESSOR_H
#define WADPROCESSOR_H

#include <QtCore>
#include <vector>
#include "wadfile.h"
#include "dehacked.h"

class WadProcessor : public QObject
{
    Q_OBJECT
public:
    explicit WadProcessor(WadFile& wad, QObject *parent = nullptr);

    //DeHackEd patches to apply after the WAD's own DEHACKED lumps.
    void SetDehackedFiles(const QStringList& files) { dehFiles = files; }

    bool ProcessWad();

private:

    QStringList MapNames();

    bool ProcessLevel(quint32 lumpNum);

    bool ProcessVertexes(quint32 lumpNum);
    bool ProcessLines(quint32 lumpNum);
    bool ProcessSegs(quint32 lumpNum);
    bool ProcessSides(quint32 lumpNum);
    bool ProcessBlockmap(quint32 lumpNum);
    bool ProcessReject(quint32 lumpNum);
    bool ProcessSectors(quint32 lumpNum);
    bool ProcessSectorLines(quint32 lumpNum);

    bool ProcessDehacked();
    bool RemovePatches();
    bool ProcessTextureColumns();
    bool RemoveOverriddenMaps();
    bool RemoveUnusedTextures();
    bool KeepUsedLumps();
    bool ProcessSounds();
    bool ProcessMusic();
    bool ProcessPalette();
    bool ProcessPages();

    int GetTextureNumForName(const char* tex_name);

    WadFile& wadFile;

    QStringList dehFiles;

    //The tables with the patches applied (ProcessDehacked). Later steps
    //keep the flats and textures their lump names name.
    DehTables deh;

};

//Kaiser windowed sinc resampler from rate to outRate (any ratio). Lowpasses
//at the lower of the two Nyquist frequencies.
std::vector<double> SincResample(const std::vector<double>& in, double rate, double outRate);

//The names the engine reads sprites (sprnames), sounds (DSxxx) and music
//(D_xxx) by, upper case (usedlumps.cpp).
const QStringList& EngineSpriteNames();
const QStringList& EngineSoundNames();
const QStringList& EngineMusicNames();

#endif // WADPROCESSOR_H
