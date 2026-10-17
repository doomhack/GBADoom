#include "mapgame.h"

MapGame MapGameOf(const QStringList& lumpNames, QStringList* maps)
{
    static const QRegularExpression doom1Re("^E\\dM\\d$", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression doom2Re("^MAP\\d\\d$", QRegularExpression::CaseInsensitiveOption);

    bool doom1 = false;
    bool doom2 = false;

    for(int i = 0; i + 1 < lumpNames.size(); i++)
    {
        if(lumpNames[i + 1].compare("THINGS", Qt::CaseInsensitive))
            continue;

        const bool d1 = doom1Re.match(lumpNames[i]).hasMatch();
        const bool d2 = doom2Re.match(lumpNames[i]).hasMatch();

        doom1 |= d1;
        doom2 |= d2;

        if(maps && (d1 || d2))
            maps->append(lumpNames[i].toUpper());
    }

    return doom1 && doom2 ? MapGame::Both : (doom1 ? MapGame::Doom1 : (doom2 ? MapGame::Doom2 : MapGame::None));
}

QString MapGameWarning(const QString& pwadName, MapGame pwadGame, const QStringList& pwadMaps,
                       const QString& iwadName, MapGame iwadGame)
{
    //A PWAD with both kinds, or with no maps, is left alone.
    if(pwadGame == MapGame::None || pwadGame == MapGame::Both || iwadGame == MapGame::None || iwadGame == MapGame::Both || pwadGame == iwadGame)
        return QString();

    QString maps = pwadMaps.mid(0, 3).join(", ");

    if(pwadMaps.size() > 3)
        maps += ", ...";

    //MapNames goes by MAP01: Doom 1 maps over Doom 2's are dropped, and Doom 2 maps
    //over Doom 1's replace them all, without Doom 2's monsters, textures and flats.
    if(pwadGame == MapGame::Doom1)
        return QString("%1 has Doom 1 maps (%2), but %3 is a Doom 2 IWAD, so they are left out. Build it on a Doom 1 IWAD.")
                   .arg(pwadName, maps, iwadName);

    return QString("%1 has Doom 2 maps (%2), but %3 is a Doom 1 IWAD, which lacks Doom 2's monsters, textures and flats. Build it on a Doom 2 IWAD.")
               .arg(pwadName, maps, iwadName);
}
