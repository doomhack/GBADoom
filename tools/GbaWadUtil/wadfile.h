#ifndef WADFILE_H
#define WADFILE_H

#include <QtCore>
#include <QObject>

class Lump
{
public:
    QString name;
    quint32 length;
    QByteArray data;
};

class WadFile : public QObject
{
    Q_OBJECT
public:
    explicit WadFile(QString filePath, QObject *parent = nullptr);

    bool LoadWadFile();

    bool SaveWadFile(QString filePath);
    bool SaveWadFile(QIODevice* device);

    qint32 GetLumpByName(QString name, Lump& lump);
    bool GetLumpByNum(quint32 lumpnum, Lump& lump);

    bool ReplaceLump(quint32 lumpnum, Lump newLump);
    bool InsertLump(quint32 lumpnum, Lump newLump);
    bool RemoveLump(quint32 lumpnum);

    quint32 LumpCount();

    bool MergeWadFile(WadFile& wadFile);

private:
    QString wadPath;

    QList<Lump> lumps;
};

#endif // WADFILE_H
