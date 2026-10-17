#ifndef SIZEREPORT_H
#define SIZEREPORT_H

#include <QtCore>

class WadFile;

//What the space in a processed WAD (and the ROM) goes on, in this order.
//The window colours them by their index here, so add new ones at the end.
static const char* const sizePartNames[] =
{
    "Engine", "Maps", "Wall textures", "Flats", "Sprites", "Graphics",
    "Sound effects", "Music", "Instruments", "Other"
};

static const int numSizeParts = sizeof(sizePartNames) / sizeof(sizePartNames[0]);

//Bytes per part, by sizePartNames index. They add up to engineBytes + wadBytes:
//data shared by several lumps counts once, under the first, as SaveWadFile
//writes it, and Other takes the header, directory, padding and small tables.
QVector<qint64> SizeBreakdown(WadFile& wad, qint64 wadBytes, qint64 engineBytes);

//"Space used: ROM 20237632 bytes (19.3 MB of 32 MB)." then a line per part,
//"  Maps: 1234567 bytes (6.1%)." The window parses these lines.
void PrintSizes(WadFile& wad, qint64 wadBytes, qint64 engineBytes);

#endif // SIZEREPORT_H
