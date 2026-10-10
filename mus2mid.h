#ifndef MUS2MID_H
#define MUS2MID_H

#include <QtCore>

//Convert a Doom MUS lump to a type 0 MIDI file.
//Returns an empty array if the lump isn't valid MUS.
QByteArray ConvertMusToMidi(const QByteArray& mus);

//Merge the tracks of a type 1 MIDI file into one (type 0) track. The player
//only plays the first track. Type 0 files are returned unchanged.
//Returns an empty array if the lump isn't valid MIDI.
QByteArray ConvertMidiToType0(const QByteArray& midi);

#endif // MUS2MID_H
