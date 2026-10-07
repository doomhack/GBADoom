#ifndef MUS2MID_H
#define MUS2MID_H

#include <QtCore>

//Convert a Doom MUS lump to a type 0 MIDI file.
//Returns an empty array if the lump isn't valid MUS.
QByteArray ConvertMusToMidi(const QByteArray& mus);

#endif // MUS2MID_H
