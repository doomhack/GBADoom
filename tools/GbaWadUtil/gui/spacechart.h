#ifndef GUI_SPACECHART_H
#define GUI_SPACECHART_H

#include <QtWidgets>

//The "Space used" report GbaWadUtil prints (sizereport.h), read back from its messages.
struct SpaceReport
{
    bool rom = false;
    qint64 total = 0;

    //Bytes by sizePartNames index.
    QVector<qint64> parts;
};

//Reads one message of the report into r. Returns false for other messages.
bool ParseSpaceLine(const QString& message, SpaceReport& r);

//The parts as one stacked bar against the 32 MB a cart holds (or the WAD's own
//size), with a legend of each part's size. Hovering a segment names it.
class SpaceChart : public QWidget
{
public:
    explicit SpaceChart(QWidget* parent = nullptr);

    //parts by sizePartNames index. With a limit, the bar's length is the limit
    //(or the total, if that's bigger, with a mark at the limit).
    void Set(const QVector<qint64>& parts, qint64 limit);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return QSize(200, sizeHint().height()); }

protected:
    void paintEvent(QPaintEvent*) override;
    bool event(QEvent* e) override;

private:
    QVector<QRectF> Segments() const;

    QVector<qint64> parts;
    qint64 total = 0;
    qint64 limit = 0;
};

#endif // GUI_SPACECHART_H
