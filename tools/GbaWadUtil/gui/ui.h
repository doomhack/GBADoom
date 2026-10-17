#ifndef GUI_UI_H
#define GUI_UI_H

#include <QtWidgets>

//The window's look: colours, fonts, icons and the few widgets that a
//style sheet can't draw.
namespace Theme
{
    inline const QColor Ground = QColor(0x141518);
    inline const QColor HeaderGround = QColor(0x18191C);
    inline const QColor Card = QColor(0x1C1D21);
    inline const QColor CardBorder = QColor(0x2A2C32);
    inline const QColor Field = QColor(0x23252A);
    inline const QColor FieldBorder = QColor(0x34363D);
    inline const QColor Text = QColor(0xECEAE4);
    inline const QColor TextSoft = QColor(0xCFCDC7);
    inline const QColor TextDim = QColor(0xA3A6AE);
    inline const QColor TextFaint = QColor(0x868993);
    inline const QColor Accent = QColor(0x4F46C8);
    inline const QColor AccentBright = QColor(0x8F88FF);
    inline const QColor AccentText = QColor(0xA9A3FF);
    inline const QColor AccentBorder = QColor(0x47428A);
    inline const QColor AccentCard = QColor(0x1F1F2B);
    inline const QColor Bar = QColor(0x7C74F2);
    inline const QColor BarGround = QColor(0x2A2C32);
    inline const QColor Ok = QColor(0x5DC39E);
    inline const QColor OkGround = QColor(0x1E3B33);
    inline const QColor Warn = QColor(0xE8A33D);
    inline const QColor WarnGround = QColor(0x3D3020);
    inline const QColor WarnText = QColor(0xF2D7AA);
    inline const QColor WarnRow = QColor(0x221C12);
    inline const QColor Error = QColor(0xEE8A7C);
    inline const QColor ErrorGround = QColor(0x3D2320);
    inline const QColor ErrorBar = QColor(0xE0604F);
    inline const QColor LogGround = QColor(0x111214);

    QString MonoFamily();

    QFont Font(int px, int weight = QFont::Normal, bool mono = false);

    QString StyleSheet();

    //Stroke icons from the design, by name: folder, plus, close, up, lock,
    //check, warn, error, info, bang, cart, file, code, copy, play, back.
    QIcon Icon(const QString& name, const QColor& color, int size = 16, qreal stroke = 1.75);
    QPixmap Pixmap(const QString& name, const QColor& color, int size, qreal stroke, qreal dpr);

    //The cartridge on an accent square, for the header and the window.
    QPixmap Logo(int size, qreal dpr);

    //A label with a style sheet role ("dim", "faint", "mono", ...).
    QLabel* Label(const QString& text, const char* role = nullptr, QWidget* parent = nullptr);

    //A button with a style sheet kind ("link", "icon", "primary", ...),
    //focused by Tab only so the focus ring means the keyboard.
    QPushButton* Button(const QString& text, const char* kind = nullptr, QWidget* parent = nullptr);

    //Re-applies the style sheet after a property it selects on changed.
    void SetStyleProperty(QWidget* w, const char* name, const QVariant& value);

    //Removes and deletes a layout's items.
    void ClearLayout(QLayout* layout);

    //"a, b and c"
    QString JoinAnd(const QStringList& items);
}

//"Include music": a pill switch with its label.
class ToggleSwitch : public QAbstractButton
{
public:
    explicit ToggleSwitch(const QString& text, QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;
};

//The header of an output card: check box, title, subtitle and icon.
class CheckCard : public QAbstractButton
{
public:
    CheckCard(const QString& title, const QString& subtitle, const QString& icon, QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override { update(); }
    void leaveEvent(QEvent*) override { update(); }

private:
    QString subtitle;
    QString iconName;
};

//One instrument mapping: name, a note on the right and the file below.
class ChoiceCard : public QAbstractButton
{
public:
    ChoiceCard(QWidget* parent = nullptr);
    void Set(const QString& name, const QString& note, const QColor& noteColor, const QString& file);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override { update(); }
    void leaveEvent(QEvent*) override { update(); }

private:
    QString name;
    QString note;
    QColor noteColor;
    QString file;
};

//A thin bar filled to a fraction, for ROM size against 32 MB.
class SizeBar : public QWidget
{
public:
    explicit SizeBar(QWidget* parent = nullptr);
    void Set(double fraction, const QColor& color);
    QSize sizeHint() const override { return QSize(100, 6); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    double fraction = 0;
    QColor color = Theme::Bar;
};

//An icon and a line of text, hidden while the text is empty.
class StatusLine : public QWidget
{
public:
    explicit StatusLine(QWidget* parent = nullptr, int textPx = 12);
    void Set(const QString& icon, const QColor& color, const QString& text);

private:
    QLabel* iconLabel;
    QLabel* textLabel;
    int textPx;
};

//Two columns side by side, stacked when the window is narrow. The right
//column keeps the width the design gives it.
class Columns : public QWidget
{
public:
    Columns(QWidget* left, QWidget* right, int stackBelow, int rightWidth, QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* e) override;

private:
    QBoxLayout* box;
    QWidget* right;
    int stackBelow;
    int rightWidth;
};

#endif // GUI_UI_H
