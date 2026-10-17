#ifndef MAPGAME_H
#define MAPGAME_H

#include <QtCore>

//Which game a WAD's maps are for, by their markers: ExMy for Doom 1, MAPxx for Doom 2.
enum class MapGame { None, Doom1, Doom2, Both };

//lumpNames in WAD order. A marker counts when THINGS follows it. maps gets
//the markers, in order.
MapGame MapGameOf(const QStringList& lumpNames, QStringList* maps = nullptr);

//The warning for a PWAD whose maps are for the other game than the IWAD's,
//or an empty string. Shared by the command line and the window.
QString MapGameWarning(const QString& pwadName, MapGame pwadGame, const QStringList& pwadMaps,
                       const QString& iwadName, MapGame iwadGame);

#endif // MAPGAME_H
