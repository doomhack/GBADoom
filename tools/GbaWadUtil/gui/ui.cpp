#include "ui.h"

#include <QSvgRenderer>

namespace Theme
{

QString MonoFamily()
{
    static QString family;

    if(family.isEmpty())
    {
        const QStringList installed = QFontDatabase::families();
        family = "Courier New";

        for(const char* f : {"IBM Plex Mono", "Cascadia Mono", "Consolas"})
        {
            if(installed.contains(f))
            {
                family = f;
                break;
            }
        }
    }

    return family;
}

static QString SansFamily()
{
    static QString family;

    if(family.isEmpty())
    {
        const QStringList installed = QFontDatabase::families();
        family = QApplication::font().family();

        for(const char* f : {"IBM Plex Sans", "Segoe UI"})
        {
            if(installed.contains(f))
            {
                family = f;
                break;
            }
        }
    }

    return family;
}

QFont Font(int px, int weight, bool mono)
{
    QFont f(mono ? MonoFamily() : SansFamily());
    f.setPixelSize(px);
    f.setWeight(QFont::Weight(weight));
    return f;
}

QString StyleSheet()
{
    QString s = R"(
* { font-family: "%SANS%"; font-size: 14px; color: #ECEAE4; }
QMainWindow, #page { background: #141518; }
QScrollArea, #scrollContent { background: #141518; border: 0; }
QLabel { background: transparent; }
QToolTip { background: #24262B; color: #ECEAE4; border: 1px solid #3A3D45; padding: 4px 6px; }

#header { background: #18191C; border: 0; border-bottom: 1px solid #2A2C32; }
#card { background: #1C1D21; border: 1px solid #2A2C32; border-radius: 10px; }
#outCard { background: #1C1D21; border: 1px solid #2E3036; border-radius: 8px; }
#outCard[on="true"] { background: #1F1F2B; border-color: #47428A; }
#banner { background: #1F1F2B; border: 1px solid #47428A; border-radius: 8px; }
#list { background: #1C1D21; border: 1px solid #2E3036; border-radius: 8px; }
#row { background: transparent; border: 0; border-top: 1px solid #2A2C32; }
#rowFirst { background: #202227; border: 0; border-top-left-radius: 7px; border-top-right-radius: 7px; }
#rowFixed { background: #18191C; border: 0; border-top: 1px dashed #34363D; border-bottom-left-radius: 7px; border-bottom-right-radius: 7px; }
#rowAuto { background: #18191C; border: 0; border-top-left-radius: 7px; border-top-right-radius: 7px; }
#cmd { background: #111214; border: 1px solid #2A2C32; border-radius: 8px; }
#segment { background: #23252A; border: 1px solid #34363D; border-radius: 8px; }
#topRule { border: 0; border-top: 1px solid #2A2C32; background: transparent; }
#stage { background: #1C1D21; border: 1px solid #2A2C32; border-radius: 8px; }
#stage[state="warn"] { border-color: #4A3B22; }
#stage[state="fail"] { border-color: #5A2E28; }

QLabel[role="title"] { font-size: 15px; font-weight: 600; }
QLabel[role="bigTitle"] { font-size: 17px; font-weight: 600; }
QLabel[role="appName"] { font-family: "%MONO%"; font-size: 15px; font-weight: 600; }
QLabel[role="num"] { font-family: "%MONO%"; font-size: 12px; color: #8F88FF; }
QLabel[role="field"] { font-size: 13px; font-weight: 500; color: #A3A6AE; }
QLabel[role="dim"] { font-size: 12px; color: #A3A6AE; }
QLabel[role="faint"] { font-size: 12px; color: #868993; }
QLabel[role="soft"] { font-size: 13px; color: #CFCDC7; }
QLabel[role="body"] { font-size: 13px; }
QLabel[role="mono"] { font-family: "%MONO%"; font-size: 13px; }
QLabel[role="monoDim"] { font-family: "%MONO%"; font-size: 13px; color: #A3A6AE; }
QLabel[role="orderNum"] { font-family: "%MONO%"; font-size: 12px; color: #868993; }
QLabel[role="badge"] { font-size: 11px; font-weight: 600; color: #A3A6AE; border: 1px solid #3A3D45; border-radius: 4px; padding: 1px 6px; }
QLabel[role="badgeAccent"] { font-size: 11px; font-weight: 600; color: #A9A3FF; border: 1px solid #47428A; border-radius: 4px; padding: 1px 6px; }
QLabel[role="chip"] { font-family: "%MONO%"; font-size: 12px; color: #CFCDC7; background: #24262B; border-radius: 4px; padding: 2px 7px; }
QLabel[role="chipOn"] { font-family: "%MONO%"; font-size: 12px; color: #FFFFFF; background: #4F46C8; border-radius: 4px; padding: 2px 7px; }
QLabel[role="chipWarn"] { font-family: "%MONO%"; font-size: 12px; color: #F2D7AA; background: #3D3020; border-radius: 4px; padding: 2px 7px; }

QPushButton { background: #2A2C32; border: 1px solid #3A3D45; border-radius: 6px; padding: 0 14px; min-height: 38px; font-size: 13px; font-weight: 500; }
QPushButton:hover { background: #33353C; }
QPushButton:pressed { background: #25272C; }
QPushButton:focus { border-color: #8F88FF; }
QPushButton:disabled { color: #868993; background: #26272C; border-color: #34363D; }
QPushButton[kind="header"] { background: #24262B; min-height: 34px; }
QPushButton[kind="header"]:hover { background: #2E3036; }
QPushButton[kind="small"] { background: #24262B; min-height: 30px; padding: 0 12px; }
QPushButton[kind="small"]:hover { background: #2E3036; }
QPushButton[kind="square"] { min-width: 36px; max-width: 36px; min-height: 36px; max-height: 36px; padding: 0; }
QPushButton[kind="link"] { background: transparent; border: 1px solid transparent; color: #A9A3FF; padding: 0 8px; min-height: 30px; }
QPushButton[kind="link"]:hover { color: #C8C4FF; background: #23252A; }
QPushButton[kind="link"]:focus { border-color: #8F88FF; }
QPushButton[kind="icon"] { background: transparent; border: 1px solid transparent; padding: 0; min-width: 30px; max-width: 30px; min-height: 30px; max-height: 30px; }
QPushButton[kind="icon"]:hover { background: #2A2C32; }
QPushButton[kind="icon"]:focus { border-color: #8F88FF; }
QPushButton[kind="icon"]:disabled { background: transparent; border-color: transparent; }
QPushButton[kind="primary"] { background: #4F46C8; border: 1px solid #4F46C8; color: #FFFFFF; font-size: 15px; font-weight: 600; min-height: 50px; border-radius: 8px; }
QPushButton[kind="primary"]:hover { background: #5A51D8; border-color: #5A51D8; }
QPushButton[kind="primary"]:focus { border-color: #C8C4FF; }
QPushButton[kind="primary"]:disabled { background: #26272C; border-color: #34363D; color: #868993; }
QPushButton[kind="primarySmall"] { background: #4F46C8; border: 1px solid #4F46C8; color: #FFFFFF; font-weight: 600; min-height: 34px; }
QPushButton[kind="primarySmall"]:hover { background: #5A51D8; }
QPushButton[kind="primarySmall"]:focus { border-color: #C8C4FF; }
QPushButton[kind="pill"] { background: transparent; border: 1px solid transparent; border-radius: 6px; min-width: 52px; min-height: 32px; padding: 0 12px; font-family: "%MONO%"; color: #CFCDC7; }
QPushButton[kind="pill"]:hover { background: #2E3036; }
QPushButton[kind="pill"]:checked { background: #4F46C8; color: #FFFFFF; font-weight: 600; }
QPushButton[kind="pill"]:focus { border-color: #8F88FF; }
QPushButton[kind="tab"] { background: transparent; border: 1px solid transparent; border-radius: 6px; min-height: 28px; padding: 0 12px; color: #A3A6AE; }
QPushButton[kind="tab"]:hover { color: #ECEAE4; }
QPushButton[kind="tab"]:checked { background: #3A3D45; color: #ECEAE4; font-weight: 600; }
QPushButton[kind="tab"]:focus { border-color: #8F88FF; }

QLineEdit { background: #23252A; border: 1px solid #34363D; border-radius: 6px; padding: 0 10px; min-height: 36px; font-family: "%MONO%"; font-size: 13px; selection-background-color: #4F46C8; }
QLineEdit:focus { border-color: #8F88FF; }
QLineEdit[error="true"] { border-color: #C85A4C; }

QPlainTextEdit#log { background: #111214; border: 0; border-bottom-left-radius: 9px; border-bottom-right-radius: 9px; font-family: "%MONO%"; font-size: 12px; selection-background-color: #4F46C8; }

QScrollBar:vertical { background: transparent; width: 12px; margin: 0; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 0; }
QScrollBar::handle:vertical { background: #34363D; border-radius: 4px; min-height: 30px; margin: 2px; }
QScrollBar::handle:horizontal { background: #34363D; border-radius: 4px; min-width: 30px; margin: 2px; }
QScrollBar::handle:hover { background: #45484F; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QMessageBox { background: #1C1D21; }
QMessageBox QPushButton { min-width: 80px; }
)";

    return s.replace("%SANS%", SansFamily()).replace("%MONO%", MonoFamily());
}

static QString IconSvg(const QString& name)
{
    static const QHash<QString, QString> icons =
    {
        {"folder", R"(<path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>)"},
        {"plus", R"(<path d="M12 5v14M5 12h14"/>)"},
        {"close", R"(<path d="M6 6l12 12M18 6L6 18"/>)"},
        {"up", R"(<path d="M12 19V5M6 11l6-6 6 6"/>)"},
        {"lock", R"(<rect x="5" y="11" width="14" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 8 0v4"/>)"},
        {"check", R"(<path d="M5 12l5 5 9-10"/>)"},
        {"warn", R"(<path d="M12 3l9 16H3z"/><path d="M12 10v4M12 17h.01"/>)"},
        {"error", R"(<circle cx="12" cy="12" r="9"/><path d="M12 8v5M12 16h.01"/>)"},
        {"info", R"(<circle cx="12" cy="12" r="9"/><path d="M12 11v5M12 8h.01"/>)"},
        {"bang", R"(<path d="M12 6v7M12 17.5h.01"/>)"},
        {"cart", R"(<path d="M6 3h12v18H6z"/><path d="M9 7h6v5H9z"/><path d="M9 16h2M13 16h2"/>)"},
        {"file", R"(<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5"/>)"},
        {"code", R"(<path d="M8 8l-4 4 4 4M16 8l4 4-4 4"/>)"},
        {"copy", R"(<rect x="9" y="9" width="11" height="11" rx="2"/><path d="M5 15V5a2 2 0 0 1 2-2h10"/>)"},
        {"back", R"(<path d="M19 12H5M11 6l-6 6 6 6"/>)"},
        {"dash", R"(<path d="M7 12h10"/>)"},
        {"dot", R"(<path d="M12 12h.01"/>)"},
    };

    return icons.value(name);
}

QPixmap Pixmap(const QString& name, const QColor& color, int size, qreal stroke, qreal dpr)
{
    QString svg;

    //The play triangle is filled, the rest are strokes.
    if(name == "play")
        svg = QString(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="%1"><path d="M7 4l13 8-13 8z"/></svg>)").arg(color.name());
    else
        svg = QString(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" stroke-width="%2" stroke-linecap="round" stroke-linejoin="round">%3</svg>)")
                .arg(color.name()).arg(stroke).arg(IconSvg(name));

    QPixmap pm(QSize(size, size) * dpr);
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(dpr);

    QSvgRenderer renderer(svg.toUtf8());
    QPainter p(&pm);
    renderer.render(&p, QRectF(0, 0, size, size));

    return pm;
}

QIcon Icon(const QString& name, const QColor& color, int size, qreal stroke)
{
    QIcon icon;
    icon.addPixmap(Pixmap(name, color, size, stroke, 1.0));
    icon.addPixmap(Pixmap(name, color, size, stroke, 2.0));

    //Fusion greys disabled icons out from these; dim them to the design's colour instead.
    icon.addPixmap(Pixmap(name, QColor(0x4A4D55), size, stroke, 1.0), QIcon::Disabled);
    icon.addPixmap(Pixmap(name, QColor(0x4A4D55), size, stroke, 2.0), QIcon::Disabled);

    return icon;
}

QPixmap Logo(int size, qreal dpr)
{
    QPixmap pm(QSize(size, size) * dpr);
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(dpr);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Accent);
    p.drawRoundedRect(QRectF(0, 0, size, size), size * 0.235, size * 0.235);

    const int inner = qRound(size * 0.53);
    p.drawPixmap(QPointF((size - inner) / 2.0, (size - inner) / 2.0), Pixmap("cart", Qt::white, inner, 1.75, dpr));

    return pm;
}

QLabel* Label(const QString& text, const char* role, QWidget* parent)
{
    QLabel* l = new QLabel(text, parent);

    if(role)
        l->setProperty("role", role);

    return l;
}

QPushButton* Button(const QString& text, const char* kind, QWidget* parent)
{
    QPushButton* b = new QPushButton(text, parent);

    if(kind)
        b->setProperty("kind", kind);

    b->setFocusPolicy(Qt::TabFocus);
    b->setCursor(Qt::PointingHandCursor);

    return b;
}

void SetStyleProperty(QWidget* w, const char* name, const QVariant& value)
{
    if(w->property(name) == value)
        return;

    w->setProperty(name, value);
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

void ClearLayout(QLayout* layout)
{
    while(QLayoutItem* item = layout->takeAt(0))
    {
        //Later, as the button being clicked may be one of them.
        if(QWidget* w = item->widget())
        {
            w->hide();
            w->deleteLater();
        }

        delete item;
    }
}

QString JoinAnd(const QStringList& items)
{
    if(items.size() < 2)
        return items.join("");

    return items.mid(0, items.size() - 1).join(", ") + " and " + items.last();
}

} // namespace Theme


static void PaintFocusRing(QPainter& p, const QRectF& r, qreal radius)
{
    p.setPen(QPen(Theme::AccentBright, 2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), radius, radius);
}


ToggleSwitch::ToggleSwitch(const QString& text, QWidget* parent) : QAbstractButton(parent)
{
    setText(text);
    setCheckable(true);
    setFocusPolicy(Qt::TabFocus);
    setCursor(Qt::PointingHandCursor);
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(52 + QFontMetrics(Theme::Font(13, QFont::Medium)).horizontalAdvance(text()), 32);
}

void ToggleSwitch::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF track(2, (height() - 22) / 2.0, 38, 22);

    p.setPen(Qt::NoPen);
    p.setBrush(isChecked() ? Theme::Accent : QColor(0x3A3D45));
    p.drawRoundedRect(track, 11, 11);

    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(track.x() + (isChecked() ? 19 : 3), track.y() + 3, 16, 16));

    if(hasFocus())
        PaintFocusRing(p, track.adjusted(-2, -2, 2, 2), 13);

    p.setPen(Theme::Text);
    p.setFont(Theme::Font(13, QFont::Medium));
    p.drawText(QRect(50, 0, width() - 50, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
}


CheckCard::CheckCard(const QString& title, const QString& subtitle, const QString& icon, QWidget* parent)
    : QAbstractButton(parent), subtitle(subtitle), iconName(icon)
{
    setText(title);
    setCheckable(true);
    setFocusPolicy(Qt::TabFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
}

QSize CheckCard::sizeHint() const
{
    return QSize(300, 60);
}

void CheckCard::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal dpr = devicePixelRatioF();
    const QRectF box(14.75, (height() - 20) / 2.0 + 0.75, 18.5, 18.5);

    p.setPen(QPen(isChecked() ? QColor(0x6B63E8) : (underMouse() ? Theme::TextFaint : QColor(0x5A5D66)), 1.5));
    p.setBrush(isChecked() ? Theme::Accent : Qt::transparent);
    p.drawRoundedRect(box, 4.5, 4.5);

    if(isChecked())
        p.drawPixmap(QPointF(box.center().x() - 7, box.center().y() - 7), Theme::Pixmap("check", Qt::white, 14, 3, dpr));

    if(hasFocus())
        PaintFocusRing(p, rect(), 8);

    const int textX = 46;
    const int textW = width() - textX - 46;

    p.setPen(Theme::Text);
    p.setFont(Theme::Font(14, QFont::DemiBold));
    p.drawText(QRect(textX, 0, textW, height() / 2 + 1), Qt::AlignLeft | Qt::AlignBottom, text());

    p.setPen(Theme::TextDim);
    p.setFont(Theme::Font(12));
    p.drawText(QRect(textX, height() / 2 + 2, textW, height() / 2), Qt::AlignLeft | Qt::AlignTop, subtitle);

    p.drawPixmap(QPointF(width() - 14 - 18, (height() - 18) / 2.0), Theme::Pixmap(iconName, Theme::TextDim, 18, 1.75, dpr));
}


ChoiceCard::ChoiceCard(QWidget* parent) : QAbstractButton(parent)
{
    setCheckable(true);
    setAutoExclusive(false);
    setFocusPolicy(Qt::TabFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ChoiceCard::Set(const QString& n, const QString& nt, const QColor& nc, const QString& f)
{
    name = n;
    note = nt;
    noteColor = nc;
    file = f;
    setToolTip(f);
    setAccessibleName(n + ", " + f);
    update();
}

QSize ChoiceCard::sizeHint() const
{
    return QSize(160, 58);
}

void ChoiceCard::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    p.setPen(isChecked() ? Theme::AccentBright : (underMouse() ? QColor(0x45484F) : Theme::FieldBorder));
    p.setBrush(isChecked() ? QColor(0x27254A) : Theme::Field);
    p.drawRoundedRect(r, 8, 8);

    if(hasFocus())
        PaintFocusRing(p, rect(), 8);

    const QRect line1(12, 9, width() - 24, 20);
    const QRect line2(12, 31, width() - 24, 18);

    p.setFont(Theme::Font(11));
    const int noteW = p.fontMetrics().horizontalAdvance(note);
    p.setPen(noteColor);
    p.drawText(line1, Qt::AlignRight | Qt::AlignVCenter, note);

    p.setFont(Theme::Font(14, QFont::DemiBold));
    p.setPen(Theme::Text);
    p.drawText(line1.adjusted(0, 0, -noteW - 8, 0), Qt::AlignLeft | Qt::AlignVCenter,
               p.fontMetrics().elidedText(name, Qt::ElideRight, line1.width() - noteW - 8));

    p.setFont(Theme::Font(12, QFont::Normal, true));
    p.setPen(Theme::TextDim);
    p.drawText(line2, Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(QFileInfo(file).fileName(), Qt::ElideMiddle, line2.width()));
}


SizeBar::SizeBar(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(6);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void SizeBar::Set(double f, const QColor& c)
{
    fraction = qBound(0.0, f, 1.0);
    color = c;
    update();
}

void SizeBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::BarGround);
    p.drawRoundedRect(rect(), 3, 3);

    if(fraction > 0)
    {
        p.setBrush(color);
        p.drawRoundedRect(QRectF(0, 0, qMax(6.0, width() * fraction), height()), 3, 3);
    }
}


StatusLine::StatusLine(QWidget* parent, int px) : QWidget(parent), textPx(px)
{
    QHBoxLayout* h = new QHBoxLayout(this);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);

    iconLabel = new QLabel(this);
    iconLabel->setFixedSize(14, 18);
    iconLabel->setAlignment(Qt::AlignCenter);

    textLabel = new QLabel(this);
    textLabel->setWordWrap(true);
    textLabel->setTextFormat(Qt::PlainText);

    h->addWidget(iconLabel, 0, Qt::AlignTop);
    h->addWidget(textLabel, 1);

    hide();
}

void StatusLine::Set(const QString& icon, const QColor& color, const QString& text)
{
    setVisible(!text.isEmpty());

    if(text.isEmpty())
        return;

    iconLabel->setPixmap(Theme::Pixmap(icon, color, 14, 2, devicePixelRatioF()));
    iconLabel->setVisible(!icon.isEmpty());
    textLabel->setText(text);
    textLabel->setStyleSheet(QString("color: %1; font-size: %2px;").arg(color.name()).arg(textPx));
}


Spinner::Spinner(int s, QWidget* parent) : QWidget(parent), size(s)
{
    setFixedSize(size, size);
    setAccessibleName("Working");

    timer.setInterval(16);
    connect(&timer, &QTimer::timeout, this, [this]()
    {
        angle = (angle + 8) % 360;
        update();
    });
}

void Spinner::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal pen = qMax(2.0, size / 7.0);
    const QRectF r = QRectF(rect()).adjusted(pen / 2, pen / 2, -pen / 2, -pen / 2);

    //A faint full ring, with a quarter of it in the accent going round.
    p.setPen(QPen(Theme::BarGround, pen));
    p.drawEllipse(r);

    p.setPen(QPen(Theme::AccentText, pen, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(r, (90 - angle) * 16, -90 * 16);
}


Columns::Columns(QWidget* l, QWidget* r, int stack, int rw, QWidget* parent)
    : QWidget(parent), right(r), stackBelow(stack), rightWidth(rw)
{
    box = new QBoxLayout(QBoxLayout::LeftToRight, this);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(24);
    box->addWidget(l, 1);
    box->addWidget(r, 0);
    right->setFixedWidth(rightWidth);
}

void Columns::resizeEvent(QResizeEvent* e)
{
    const bool stacked = e->size().width() < stackBelow;

    if(stacked != (box->direction() == QBoxLayout::TopToBottom))
    {
        box->setDirection(stacked ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        box->setSpacing(stacked ? 20 : 24);

        if(stacked)
        {
            right->setMinimumWidth(0);
            right->setMaximumWidth(QWIDGETSIZE_MAX);
        }
        else
            right->setFixedWidth(rightWidth);
    }

    QWidget::resizeEvent(e);
}
