#include "spacechart.h"
#include "ui.h"
#include "../sizereport.h"

using namespace Theme;

static const int barTop = 4;
static const int barHeight = 14;
static const int legendTop = barTop + barHeight + 16;
static const int rowHeight = 24;
static const int percentWidth = 52;

//By sizePartNames index: the eight categorical slots of the chart palette,
//stepped for a dark surface, with the engine and Other in neutral greys.
static const QColor partColors[numSizeParts] =
{
    QColor(0x8A8D96), QColor(0x3987E5), QColor(0xD95926), QColor(0x199E70), QColor(0xC98500),
    QColor(0xD55181), QColor(0x008300), QColor(0x9085E9), QColor(0xE66767), QColor(0x5F626B)
};

static QString Size(qint64 bytes)
{
    if(bytes < 100 * 1024)
        return QString("%1 KB").arg((bytes + 1023) / 1024);

    return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
}

bool ParseSpaceLine(const QString& message, SpaceReport& r)
{
    static const QRegularExpression headerRe("^Space used: (ROM|WAD) (\\d+) bytes");
    static const QRegularExpression partRe("^  (.+): (\\d+) bytes \\(");

    QRegularExpressionMatch m = headerRe.match(message);

    if(m.hasMatch())
    {
        r.rom = m.captured(1) == "ROM";
        r.total = m.captured(2).toLongLong();
        r.parts = QVector<qint64>(numSizeParts, 0);
        return true;
    }

    m = partRe.match(message);

    if(!m.hasMatch() || r.parts.isEmpty())
        return false;

    for(int i = 0; i < numSizeParts; i++)
    {
        if(m.captured(1) == sizePartNames[i])
        {
            r.parts[i] = m.captured(2).toLongLong();
            return true;
        }
    }

    return false;
}


SpaceChart::SpaceChart(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void SpaceChart::Set(const QVector<qint64>& p, qint64 l)
{
    parts = p;
    limit = l;
    total = 0;

    for(qint64 b : parts)
        total += b;

    updateGeometry();
    update();
}

QSize SpaceChart::sizeHint() const
{
    int rows = 0;

    for(qint64 b : parts)
        rows += b > 0;

    return QSize(320, legendTop + rows * rowHeight);
}

QVector<QRectF> SpaceChart::Segments() const
{
    QVector<QRectF> segs(parts.size());

    const double scale = qMax(total, limit);

    if(scale <= 0)
        return segs;

    double x = 0;

    for(int i = 0; i < parts.size(); i++)
    {
        if(!parts[i])
            continue;

        //A 2px gap between segments; the smallest still show.
        const double w = width() * parts[i] / scale;
        segs[i] = QRectF(x, barTop, qMax(2.0, w - 2), barHeight);
        x += qMax(4.0, w);
    }

    return segs;
}

void SpaceChart::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    //Disabled: an old measurement, shown while the next one runs.
    if(!isEnabled())
        p.setOpacity(0.45);

    const QRectF bar(0, barTop, width(), barHeight);

    //What's left of the cart.
    p.setPen(Qt::NoPen);
    p.setBrush(BarGround);
    p.drawRoundedRect(bar, 4, 4);

    QPainterPath clip;
    clip.addRoundedRect(bar, 4, 4);
    p.save();
    p.setClipPath(clip);

    const QVector<QRectF> segs = Segments();

    for(int i = 0; i < segs.size(); i++)
    {
        if(parts[i])
            p.fillRect(segs[i], partColors[i]);
    }

    p.restore();

    //Over the limit: mark where the cart ends.
    if(limit && total > limit)
    {
        const double x = width() * double(limit) / total;
        p.fillRect(QRectF(x - 1, 0, 2, barTop * 2 + barHeight), Text);
    }

    //Legend: identity by swatch and name, values in text colours.
    int y = legendTop;

    const QFont nameFont = Font(13);
    const QFont valueFont = Font(12, QFont::Normal, true);
    const QFont percentFont = Font(12);

    for(int i = 0; i < parts.size(); i++)
    {
        if(!parts[i])
            continue;

        p.setPen(Qt::NoPen);
        p.setBrush(partColors[i]);
        p.drawRoundedRect(QRectF(0, y + (rowHeight - 10) / 2.0, 10, 10), 2, 2);

        const QRect row(18, y, width() - 18, rowHeight);

        p.setFont(nameFont);
        p.setPen(TextSoft);
        p.drawText(row, Qt::AlignLeft | Qt::AlignVCenter, sizePartNames[i]);

        p.setFont(valueFont);
        p.setPen(Text);
        p.drawText(row.adjusted(0, 0, -percentWidth, 0), Qt::AlignRight | Qt::AlignVCenter, Size(parts[i]));

        p.setFont(percentFont);
        p.setPen(TextFaint);
        p.drawText(row, Qt::AlignRight | Qt::AlignVCenter, QString::number(100.0 * parts[i] / total, 'f', 1) + "%");

        y += rowHeight;
    }
}

bool SpaceChart::event(QEvent* e)
{
    if(e->type() != QEvent::ToolTip || !total)
        return QWidget::event(e);

    const QPoint pos = static_cast<QHelpEvent*>(e)->pos();
    int part = -1;

    if(pos.y() < legendTop)
    {
        const QVector<QRectF> segs = Segments();

        for(int i = 0; i < segs.size(); i++)
        {
            //Hit targets bigger than the mark: the gap counts too.
            if(parts[i] && pos.x() >= segs[i].left() - 1 && pos.x() <= segs[i].right() + 2)
                part = i;
        }
    }
    else
    {
        int row = (pos.y() - legendTop) / rowHeight;

        for(int i = 0; i < parts.size() && part < 0; i++)
        {
            if(parts[i] && row-- == 0)
                part = i;
        }
    }

    if(part < 0)
    {
        QToolTip::hideText();
        e->ignore();
        return true;
    }

    QToolTip::showText(static_cast<QHelpEvent*>(e)->globalPos(),
                       QString("%1: %2 (%3%), %L4 bytes").arg(sizePartNames[part], Size(parts[part]))
                           .arg(100.0 * parts[part] / total, 0, 'f', 1).arg(parts[part]),
                       this);

    return true;
}
