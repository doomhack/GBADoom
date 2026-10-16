#ifndef DEHACKED_H
#define DEHACKED_H

#include <QtCore>
#include "infotables.h"
#include "../../include/deh_text.h"

//The engine's dehackable tables, the defaults (infotables.cpp) with any
//DeHackEd patches applied. ProcessDehacked writes them to the WAD.
class DehTables
{
public:
    DehTables();

    state_t states[NUMSTATES];
    mobjinfo_t mobjinfo[NUMMOBJTYPES];
    weaponinfo_t weaponinfo[NUMWEAPONS];
    miscinfo_t misc;

    //deh_text.h's strings, Latin-1.
    QByteArray text[NUMTEXTS];
    bool textPatched[NUMTEXTS];

    //Text blocks that rename a sprite, sound or music, by engine name
    //(upper case): the name of the lumps the engine should get instead.
    QMap<QString, QString> spriteRenames;
    QMap<QString, QString> soundRenames;
    QMap<QString, QString> musicRenames;
};

//Applies one vanilla DeHackEd patch (patch format 6, from DeHackEd 2.3 to
//3.1) to tables: its Thing, Frame, Pointer, Weapon, Ammo, Misc and Text
//sections. Anything else is skipped with a warning naming source and line.
//Returns false if the patch couldn't be read at all.
bool ApplyDehacked(const QByteArray& patch, const QString& source, DehTables& tables);

#endif // DEHACKED_H
