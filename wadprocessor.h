#ifndef WADPROCESSOR_H
#define WADPROCESSOR_H

#include <QtCore>
#include <vector>
#include "wadfile.h"

class WadProcessor : public QObject
{
    Q_OBJECT
public:
    explicit WadProcessor(WadFile& wad, QObject *parent = nullptr);

    bool ProcessWad();

private:

    bool ProcessD2Levels();
    bool ProcessD1Levels();

    bool ProcessLevel(quint32 lumpNum);

    bool ProcessVertexes(quint32 lumpNum);
    bool ProcessLines(quint32 lumpNum);
    bool ProcessSegs(quint32 lumpNum);
    bool ProcessSides(quint32 lumpNum);
    bool ProcessBlockmap(quint32 lumpNum);
    bool ProcessReject(quint32 lumpNum);
    bool ProcessSectorLines(quint32 lumpNum);

    bool RemovePatches();
    bool ProcessTextureColumns();
    bool RemoveOverriddenMaps();
    bool RemoveUnusedLumps();
    bool ProcessSounds();
    bool ProcessMusic();

    int GetTextureNumForName(const char* tex_name);

    WadFile& wadFile;

};

//Kaiser windowed sinc resampler from rate to outRate (any ratio). Lowpasses
//at the lower of the two Nyquist frequencies.
std::vector<double> SincResample(const std::vector<double>& in, double rate, double outRate);

#endif // WADPROCESSOR_H
