#include "mainwindow.h"
#include "buildpage.h"
#include "ui.h"
#include "spacechart.h"
#include "../dehacked.h"

#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace Theme;

namespace
{

const double romLimitMB = 32.0;

enum class WadKind { None, Missing, Iwad, Pwad, NotWad };

WadKind CheckWad(const QString& path)
{
    if(path.isEmpty())
        return WadKind::None;

    QFile f(path);

    if(!f.open(QIODevice::ReadOnly))
        return WadKind::Missing;

    const QByteArray id = f.read(4);

    if(id == "IWAD")
        return WadKind::Iwad;

    if(id == "PWAD")
        return WadKind::Pwad;

    return WadKind::NotWad;
}

enum class RomKind { None, Missing, NoHeader, Empty, HasWad };

struct RomCheck
{
    RomKind kind = RomKind::None;

    //The engine, up to the end of the IWAD header.
    qint64 engineBytes = 0;
};

RomCheck CheckRom(const QString& path)
{
    RomCheck r;

    if(path.isEmpty())
        return r;

    const QFileInfo fi(path);

    if(!fi.isFile())
    {
        r.kind = RomKind::Missing;
        return r;
    }

    //Refresh runs on every key press, so don't read the ROM each time.
    struct Cached { QDateTime modified; qint64 size; RomCheck check; };
    static QHash<QString, Cached> cache;

    auto it = cache.constFind(path);

    if(it != cache.constEnd() && it->modified == fi.lastModified() && it->size == fi.size())
        return it->check;

    QFile f(path);

    if(!f.open(QIODevice::ReadOnly))
    {
        r.kind = RomKind::Missing;
        return r;
    }

    const QByteArray rom = f.readAll();
    const int header = FindRomIwadHeader(rom);

    if(header < 0)
    {
        r.kind = RomKind::NoHeader;
        r.engineBytes = rom.size();
    }
    else
    {
        r.engineBytes = header + romIwadHeaderSize;
        r.kind = rom.size() > r.engineBytes ? RomKind::HasWad : RomKind::Empty;
    }

    cache.insert(path, {fi.lastModified(), fi.size(), r});

    return r;
}

QStringList* scanLog = nullptr;

void ScanHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    if(scanLog)
        scanLog->append((type == QtInfoMsg || type == QtDebugMsg ? "i" : "w") + msg);
}

//Applies a patch to scratch tables, to say before the build what it changes and skips.
DehScan ScanPatch(const QByteArray& data, const QString& source)
{
    DehScan scan;

    QStringList log;
    scanLog = &log;
    const QtMessageHandler old = qInstallMessageHandler(ScanHandler);

    std::unique_ptr<DehTables> tables(new DehTables);
    const bool ok = ApplyDehacked(data, source, *tables);

    qInstallMessageHandler(old);
    scanLog = nullptr;

    if(!ok)
    {
        scan.note = "Binary patch, not supported";
        scan.unusable = true;
        return scan;
    }

    static const QRegularExpression skippedRe("Skipped \\(not supported\\): (.+) x\\d+\\.$");

    //ApplyDehacked's summary, "DeHackEd x.deh: 3 things, 12 frames, 1 renames.", by pill.
    static const QHash<QString, int> counted =
    {
        {"things", 0}, {"frames", 1}, {"code pointers", 2}, {"weapons", 3},
        {"ammo", 4}, {"misc values", 5}, {"texts", 6}, {"renames", 6}
    };

    const QString summary = "iDeHackEd " + source + ": ";

    QStringList skipped;
    int warnings = 0;

    for(const QString& l : log)
    {
        const QRegularExpressionMatch m = skippedRe.match(l);

        if(l.startsWith(summary))
        {
            for(const QString& item : l.mid(summary.size()).chopped(1).split(", "))
            {
                const int pill = counted.value(item.section(' ', 1), -1);

                if(pill >= 0)
                    scan.changes |= 1 << pill;
            }
        }
        else if(m.hasMatch())
        {
            QString what = m.captured(1);

            if(what.startsWith("BEX"))
                what = "BEX";

            if(!skipped.contains(what))
                skipped.append(what);
        }
        else if(l.startsWith('w'))
            warnings++;
    }

    QStringList notes;

    if(!skipped.isEmpty())
        notes.append(JoinAnd(skipped) + (skipped.size() == 1 ? " block" : " blocks") + " will be skipped");

    if(warnings)
        notes.append(QString("%1 %2").arg(warnings).arg(warnings == 1 ? "warning" : "warnings"));

    scan.note = notes.join(", ");
    return scan;
}

DehScan ScanDehacked(const QString& path)
{
    QFile f(path);

    if(!f.open(QIODevice::ReadOnly))
    {
        DehScan scan;
        scan.note = "Can't read this file";
        scan.unusable = true;
        return scan;
    }

    return ScanPatch(f.readAll(), QFileInfo(path).fileName());
}

//A WAD's DEHACKED lumps as one patch, and its maps. Reads only the directory and
//those lumps, and only again if the file changes, as Refresh runs on every key press.
DehScan ScanWad(const QString& path)
{
    const QFileInfo fi(path);

    struct Cached { QDateTime modified; qint64 size; DehScan scan; };
    static QHash<QString, Cached> cache;

    auto it = cache.constFind(path);

    if(it != cache.constEnd() && it->modified == fi.lastModified() && it->size == fi.size())
        return it->scan;

    DehScan scan;
    QFile f(path);
    QStringList notes;

    if(f.open(QIODevice::ReadOnly))
    {
        const QByteArray header = f.read(12);
        const quint32 numLumps = header.size() == 12 ? qFromLittleEndian<quint32>(header.constData() + 4) : 0;
        const quint32 dirOfs = header.size() == 12 ? qFromLittleEndian<quint32>(header.constData() + 8) : 0;

        if((header.startsWith("IWAD") || header.startsWith("PWAD")) && f.seek(dirOfs))
        {
            const QByteArray dir = f.read(qint64(numLumps) * 16);
            QStringList names;

            for(quint32 i = 0; i < numLumps && (i + 1) * 16 <= quint32(dir.size()); i++)
            {
                const char* e = dir.constData() + i * 16;
                names.append(QString::fromLatin1(e + 8, int(strnlen(e + 8, 8))));

                if(names.last().toUpper() != "DEHACKED" || !f.seek(qFromLittleEndian<quint32>(e)))
                    continue;

                DehScan lump = ScanPatch(f.read(qFromLittleEndian<quint32>(e + 4)), QString("DEHACKED lump %1").arg(i));

                //In a WAD, a binary patch is skipped rather than stopping the build.
                if(lump.unusable)
                    lump.note = "a binary DEHACKED lump will be skipped";

                scan.changes |= lump.changes;
                scan.lumps++;

                if(!lump.note.isEmpty() && !notes.contains(lump.note))
                    notes.append(lump.note);
            }

            scan.mapGame = MapGameOf(names, &scan.maps);
        }
    }

    scan.note = notes.join(", ");
    cache.insert(path, {fi.lastModified(), fi.size(), scan});

    return scan;
}

QString Quote(const QString& s)
{
    return s.contains(' ') ? '"' + s + '"' : s;
}

QString KnownIwad(const QString& path)
{
    static const QHash<QString, QString> known =
    {
        {"doom1.wad", "Doom (shareware)"},
        {"doom.wad", "Doom"},
        {"doomu.wad", "The Ultimate Doom"},
        {"doom2.wad", "Doom II"},
        {"doom2f.wad", "Doom II (French)"},
        {"tnt.wad", "Final Doom: TNT"},
        {"plutonia.wad", "Final Doom: Plutonia"},
        {"freedoom1.wad", "Freedoom: Phase 1"},
        {"freedoom2.wad", "Freedoom: Phase 2"},
    };

    return known.value(QFileInfo(path).fileName().toLower());
}

bool SamePath(const QString& a, const QString& b)
{
    return !a.isEmpty() && !QString::compare(QDir::cleanPath(a), QDir::cleanPath(b), Qt::CaseInsensitive);
}

QFrame* CardFrame(QVBoxLayout** layout, int spacing)
{
    QFrame* card = new QFrame;
    card->setObjectName("card");

    QVBoxLayout* v = new QVBoxLayout(card);
    v->setContentsMargins(20, 20, 20, 20);
    v->setSpacing(spacing);

    *layout = v;
    return card;
}

QWidget* SectionHeader(const QString& num, const QString& title, QWidget* right = nullptr)
{
    QWidget* w = new QWidget;
    QHBoxLayout* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(12);

    if(!num.isEmpty())
        h->addWidget(Label(num, "num"));

    h->addWidget(Label(title, "title"));
    h->addStretch(1);

    if(right)
        h->addWidget(right);

    return w;
}

QFrame* ListRow(const char* name, QHBoxLayout** layout, bool tight)
{
    QFrame* row = new QFrame;
    row->setObjectName(name);

    QHBoxLayout* h = new QHBoxLayout(row);
    h->setContentsMargins(12, tight ? 5 : 10, tight ? 6 : 12, tight ? 5 : 10);
    h->setSpacing(12);

    *layout = h;
    return row;
}

QLabel* OrderLabel(const QString& text)
{
    QLabel* l = Label(text, "orderNum");
    l->setFixedWidth(22);
    l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return l;
}

QLabel* NameLabel(const QString& path, const char* role = "mono")
{
    QLabel* l = Label(QFileInfo(path).fileName(), role);
    l->setToolTip(QDir::toNativeSeparators(path));
    l->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    l->setMinimumWidth(40);
    return l;
}

QLabel* IconLabel(const QString& icon, const QColor& color, int size, const QString& tip = QString())
{
    QLabel* l = new QLabel;
    l->setPixmap(Pixmap(icon, color, size, 1.75, 2.0));

    if(!tip.isEmpty())
    {
        l->setToolTip(tip);
        l->setAccessibleName(tip);
    }

    return l;
}

QPushButton* IconButton(const QString& icon, const QString& tip)
{
    QPushButton* b = Button(QString(), "icon");
    b->setIcon(Icon(icon, TextDim, 16));
    b->setIconSize(QSize(16, 16));
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    return b;
}

} // namespace


MainWindow::MainWindow(const QStringList& args)
{
    exeDir = QCoreApplication::applicationDirPath();
    lastDir = exeDir;

    measureTimer = new QTimer(this);
    measureTimer->setSingleShot(true);
    measureTimer->setInterval(700);
    connect(measureTimer, &QTimer::timeout, this, [this]() { StartMeasurement(); });

    setWindowTitle("GbaWadUtil");
    setAcceptDrops(true);

    QIcon icon;

    for(int size : {16, 24, 32, 48, 64, 256})
        icon.addPixmap(Logo(size, 1.0));

    setWindowIcon(icon);

    cfgFiles = {GusPatchDir() + "/timidity.cfg", GusPatchDir() + "/timid_d1.cfg", GusPatchDir() + "/timid_d2.cfg"};
    s.gusCfg = cfgFiles[0];

    //Where build.bat leaves it, next to this folder.
    const QString engine = QDir::cleanPath(exeDir + "/../GBADoom.gba");

    if(QFileInfo::exists(engine))
        s.engine = engine;

    stack = new QStackedWidget;
    setCentralWidget(stack);

    settingsPage = new QWidget;
    settingsPage->setObjectName("page");

    QVBoxLayout* page = new QVBoxLayout(settingsPage);
    page->setContentsMargins(0, 0, 0, 0);
    page->setSpacing(0);
    page->addWidget(BuildHeader());

    QScrollArea* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget* content = new QWidget;
    content->setObjectName("scrollContent");

    QHBoxLayout* centre = new QHBoxLayout(content);
    centre->setContentsMargins(24, 24, 24, 24);

    QWidget* inner = new QWidget;
    inner->setMaximumWidth(1232);
    centre->addWidget(inner);

    QVBoxLayout* v = new QVBoxLayout(inner);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(20);

    banner = new QFrame;
    banner->setObjectName("banner");
    QVBoxLayout* bv = new QVBoxLayout(banner);
    bv->setContentsMargins(14, 12, 14, 12);
    bv->setSpacing(6);
    bv->addWidget(Label("Filled in from the command line.", "body"));
    bannerIgnored = Label(QString(), "body");
    bannerIgnored->setStyleSheet(QString("color: %1").arg(Warn.name()));
    bannerIgnored->setWordWrap(true);
    bv->addWidget(bannerIgnored);
    banner->hide();
    v->addWidget(banner);

    QWidget* left = new QWidget;
    QVBoxLayout* lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(20);
    lv->addWidget(BuildSources());
    lv->addWidget(BuildDehacked());
    lv->addWidget(BuildMusic());
    lv->addStretch(1);

    QWidget* right = new QWidget;
    QVBoxLayout* rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(20);
    rv->addWidget(BuildOutputs());
    rv->addWidget(BuildSpace());
    rv->addWidget(BuildCommand());
    rv->addStretch(1);

    v->addWidget(new Columns(left, right, 1000, 400));

    scroll->setWidget(content);
    page->addWidget(scroll, 1);

    buildPage = new BuildPage;
    buildPage->onBack = [this]() { stack->setCurrentWidget(settingsPage); };
    buildPage->onBuildAgain = [this]() { StartBuild(); };

    stack->addWidget(settingsPage);
    stack->addWidget(buildPage);

    FillFromArguments(args);
    RebuildPwadRows();
    RebuildDehRows();
    Refresh();

    //Not the first button, which would show its focus ring as if tabbed to.
    scroll->setFocus();

    setMinimumSize(560, 480);

    const QRect avail = screen()->availableGeometry();
    resize(QSize(1240, 960).boundedTo(avail.size() * 0.92));
}

QString MainWindow::Abs(const QString& path) const
{
    QString p = path.trimmed();

    //Pasted from Explorer's "Copy as path".
    if(p.size() >= 2 && p.startsWith('"') && p.endsWith('"'))
        p = p.mid(1, p.size() - 2);

    if(p.isEmpty())
        return QString();

    return QDir::cleanPath(QDir(exeDir).absoluteFilePath(QDir::fromNativeSeparators(p)));
}

QString MainWindow::Shown(const QString& path) const
{
    if(path.isEmpty())
        return QString();

    //Inside GbaWadUtil's folder or next to it, as the .bat scripts there name them.
    const QString rel = QDir(exeDir).relativeFilePath(path);

    if(QDir::isAbsolutePath(rel) || rel.startsWith("../.."))
        return QDir::toNativeSeparators(path);

    return QDir::toNativeSeparators(rel);
}

QString MainWindow::StartDir() const
{
    return lastDir;
}

QWidget* MainWindow::BuildHeader()
{
    QFrame* header = new QFrame;
    header->setObjectName("header");

    QHBoxLayout* h = new QHBoxLayout(header);
    h->setContentsMargins(24, 14, 24, 14);
    h->setSpacing(12);

    QLabel* logo = new QLabel;
    logo->setPixmap(Logo(34, 2.0));
    h->addWidget(logo);

    QVBoxLayout* names = new QVBoxLayout;
    names->setSpacing(0);
    names->addWidget(Label("GbaWadUtil", "appName"));
    names->addWidget(Label("Build a GBADoom ROM or WAD", "dim"));
    h->addLayout(names);
    h->addStretch(1);

    QPushButton* open = Button("Open preset…", "header");
    QPushButton* save = Button("Save preset", "header");
    connect(open, &QPushButton::clicked, this, [this]() { OpenPreset(); });
    connect(save, &QPushButton::clicked, this, [this]() { SavePreset(); });
    h->addWidget(open);
    h->addWidget(save);

    return header;
}

QLineEdit* MainWindow::PathEdit(QString* field, bool* edited, const QString& placeholder)
{
    QLineEdit* e = new QLineEdit;
    e->setPlaceholderText(placeholder);
    e->setClearButtonEnabled(false);

    connect(e, &QLineEdit::textEdited, this, [this, field, edited](const QString& text)
    {
        *field = Abs(text);

        //Cleared, a suggested name can take over again.
        if(edited)
            *edited = !field->isEmpty();

        Refresh();
    });

    connect(e, &QLineEdit::editingFinished, this, [this, e, field]()
    {
        e->setText(Shown(*field));
    });

    pathEdits.append({e, field});

    return e;
}

QPushButton* MainWindow::BrowseButton(const QString& tip)
{
    QPushButton* b = Button(QString(), "square");
    b->setIcon(Icon("folder", Text, 16));
    b->setIconSize(QSize(16, 16));
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    return b;
}

static QHBoxLayout* EditRow(QLineEdit* edit, QPushButton* button)
{
    QHBoxLayout* h = new QHBoxLayout;
    h->setSpacing(8);
    h->addWidget(edit, 1);
    h->addWidget(button);
    return h;
}

QWidget* MainWindow::BuildSources()
{
    QVBoxLayout* v;
    QFrame* card = CardFrame(&v, 18);
    v->addWidget(SectionHeader("01", "Source WADs"));

    QVBoxLayout* field = new QVBoxLayout;
    field->setSpacing(6);

    QHBoxLayout* top = new QHBoxLayout;
    QLabel* label = Label(QString("Base IWAD <span style=\"color: %1\">(required)</span>").arg(TextFaint.name()), "field");
    top->addWidget(label);
    top->addStretch(1);
    iwadTag = new QLabel;
    top->addWidget(iwadTag);
    field->addLayout(top);

    iwadEdit = PathEdit(&s.iwad, nullptr, "doom1.wad, doomu.wad or doom2.wad");
    label->setBuddy(iwadEdit);

    QPushButton* browse = Button("Browse…");
    browse->setIcon(Icon("folder", Text, 16));
    connect(browse, &QPushButton::clicked, this, [this]() { BrowseIwad(); });
    field->addLayout(EditRow(iwadEdit, browse));
    v->addLayout(field);

    QVBoxLayout* order = new QVBoxLayout;
    order->setSpacing(8);

    QHBoxLayout* orderTop = new QHBoxLayout;
    orderTop->addWidget(Label("Load order", "field"));
    orderTop->addStretch(1);
    orderTop->addWidget(Label("Later files override lumps from earlier ones", "faint"));
    order->addLayout(orderTop);

    QFrame* list = new QFrame;
    list->setObjectName("list");
    QVBoxLayout* lv = new QVBoxLayout(list);
    lv->setContentsMargins(1, 1, 1, 1);
    lv->setSpacing(0);

    QHBoxLayout* h;
    QFrame* iwadRow = ListRow("rowFirst", &h, false);
    h->addWidget(OrderLabel("1"));
    iwadRowName = Label(QString(), "mono");
    iwadRowName->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    h->addWidget(iwadRowName, 1);
    h->addWidget(Label("IWAD", "badgeAccent"), 0, Qt::AlignVCenter);
    lv->addWidget(iwadRow);

    pwadRows = new QWidget;
    QVBoxLayout* pv = new QVBoxLayout(pwadRows);
    pv->setContentsMargins(0, 0, 0, 0);
    pv->setSpacing(0);
    lv->addWidget(pwadRows);

    QFrame* addRow = ListRow("row", &h, true);
    h->addSpacing(22);
    QPushButton* add = Button("Add PWADs…", "link");
    add->setIcon(Icon("plus", AccentText, 16));
    connect(add, &QPushButton::clicked, this, [this]()
    {
        const QStringList files = QFileDialog::getOpenFileNames(this, "Add PWADs", StartDir(), "WAD files (*.wad);;All files (*)");

        if(!files.isEmpty())
        {
            lastDir = QFileInfo(files.first()).absolutePath();
            AddPwads(files);
        }
    });
    h->addWidget(add);
    h->addWidget(Label("or drop .wad files here", "faint"));
    h->addStretch(1);
    lv->addWidget(addRow);

    QFrame* gbaRow = ListRow("rowFixed", &h, false);
    gbaWadOrder = OrderLabel(QString());
    h->addWidget(gbaWadOrder);
    h->addWidget(Label("gbadoom.wad", "monoDim"));
    QLabel* gbaNote = Label("GBADoom menu graphics · always merged last", "faint");
    gbaNote->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    h->addWidget(gbaNote, 1);
    h->addWidget(IconLabel("lock", TextFaint, 16, "Always merged"));
    lv->addWidget(gbaRow);

    order->addWidget(list);
    v->addLayout(order);

    return card;
}

void MainWindow::RebuildPwadRows()
{
    ClearLayout(pwadRows->layout());
    pwadWarnings.clear();

    for(int i = 0; i < s.pwads.size(); i++)
    {
        const QString& path = s.pwads.at(i);
        const QString name = QFileInfo(path).fileName();

        QHBoxLayout* h;
        QFrame* row = ListRow("row", &h, true);
        h->addWidget(OrderLabel(QString::number(i + 2)));
        h->addWidget(NameLabel(path), 1);

        //Doom 1 maps on a Doom 2 IWAD or the other way round: filled in by Refresh,
        //as it depends on the IWAD too.
        QWidget* warn = new QWidget;
        QHBoxLayout* wh = new QHBoxLayout(warn);
        wh->setContentsMargins(0, 0, 0, 0);
        wh->setSpacing(6);
        wh->addWidget(IconLabel("warn", Warn, 14));
        QLabel* note = Label(QString(), "dim");
        note->setStyleSheet(QString("color: %1").arg(Warn.name()));
        note->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        wh->addWidget(note, 1);
        warn->hide();
        h->addWidget(warn, 2);
        pwadWarnings.append(note);

        h->addWidget(Label("PWAD", "badge"), 0, Qt::AlignVCenter);

        QPushButton* up = IconButton("up", "Move " + name + " earlier");
        up->setEnabled(i > 0);
        connect(up, &QPushButton::clicked, this, [this, i]()
        {
            s.pwads.swapItemsAt(i - 1, i);
            RebuildPwadRows();
            Refresh();
        });

        QPushButton* remove = IconButton("close", "Remove " + name);
        connect(remove, &QPushButton::clicked, this, [this, i]()
        {
            s.pwads.removeAt(i);
            RebuildPwadRows();
            Refresh();
        });

        QHBoxLayout* buttons = new QHBoxLayout;
        buttons->setSpacing(2);
        buttons->addWidget(up);
        buttons->addWidget(remove);
        h->addLayout(buttons);

        pwadRows->layout()->addWidget(row);
    }
}

QWidget* MainWindow::BuildDehacked()
{
    QVBoxLayout* v;
    QFrame* card = CardFrame(&v, 16);
    v->addWidget(SectionHeader("02", "DeHackEd patches", Label("Optional", "faint")));

    QFrame* list = new QFrame;
    list->setObjectName("list");
    QVBoxLayout* lv = new QVBoxLayout(list);
    lv->setContentsMargins(1, 1, 1, 1);
    lv->setSpacing(0);

    QHBoxLayout* h;
    QFrame* autoRow = ListRow("rowAuto", &h, false);
    h->addWidget(OrderLabel("1"));
    dehAutoText = Label(QString(), "soft");
    dehAutoText->setStyleSheet(QString("color: %1").arg(TextDim.name()));
    dehAutoText->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    h->addWidget(dehAutoText, 1);
    h->addWidget(IconLabel("lock", TextFaint, 16, "Automatic"));
    lv->addWidget(autoRow);

    dehRows = new QWidget;
    QVBoxLayout* dv = new QVBoxLayout(dehRows);
    dv->setContentsMargins(0, 0, 0, 0);
    dv->setSpacing(0);
    lv->addWidget(dehRows);

    QFrame* addRow = ListRow("row", &h, true);
    h->addSpacing(22);
    QPushButton* add = Button("Add .deh files…", "link");
    add->setIcon(Icon("plus", AccentText, 16));
    connect(add, &QPushButton::clicked, this, [this]()
    {
        const QStringList files = QFileDialog::getOpenFileNames(this, "Add DeHackEd patches", StartDir(), "DeHackEd patches (*.deh *.bex);;All files (*)");

        if(!files.isEmpty())
        {
            lastDir = QFileInfo(files.first()).absolutePath();
            AddDehs(files);
        }
    });
    h->addWidget(add);
    h->addStretch(1);
    lv->addWidget(addRow);

    v->addWidget(list);

    QHBoxLayout* chips = new QHBoxLayout;
    chips->setSpacing(6);
    chips->addWidget(Label("Applied", "faint"));
    chips->addSpacing(4);

    //Lit by Refresh for the blocks the patches change.
    for(const char* c : dehBlocks)
    {
        dehChips.append(Label(c, "chip"));
        chips->addWidget(dehChips.last());
    }

    chips->addStretch(1);

    QVBoxLayout* applied = new QVBoxLayout;
    applied->setSpacing(6);
    applied->addLayout(chips);
    applied->addWidget(Label("Skipped: BEX, Sound, Sprite and Cheat blocks, binary patches", "faint"));
    v->addLayout(applied);

    return card;
}

void MainWindow::RebuildDehRows()
{
    ClearLayout(dehRows->layout());

    for(int i = 0; i < s.dehs.size(); i++)
    {
        const QString& path = s.dehs.at(i);
        const QString note = dehScans.value(path).note;

        QHBoxLayout* h;
        QFrame* row = ListRow("row", &h, true);
        h->addWidget(OrderLabel(QString::number(i + 2)));

        QLabel* name = NameLabel(path);
        name->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        h->addWidget(name);

        if(!note.isEmpty())
        {
            QHBoxLayout* warn = new QHBoxLayout;
            warn->setSpacing(6);
            warn->addWidget(IconLabel("warn", Warn, 14));
            QLabel* n = Label(note, "dim");
            n->setStyleSheet(QString("color: %1").arg(Warn.name()));
            n->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            n->setToolTip(note);
            warn->addWidget(n, 1);
            h->addLayout(warn, 1);
        }
        else
            h->addStretch(1);

        QPushButton* remove = IconButton("close", "Remove " + QFileInfo(path).fileName());
        connect(remove, &QPushButton::clicked, this, [this, i]()
        {
            s.dehs.removeAt(i);
            RebuildDehRows();
            Refresh();
        });
        h->addWidget(remove);

        dehRows->layout()->addWidget(row);
    }
}

QWidget* MainWindow::BuildMusic()
{
    QVBoxLayout* v;
    QFrame* card = CardFrame(&v, 18);

    musicSwitch = new ToggleSwitch("Include music");
    connect(musicSwitch, &ToggleSwitch::toggled, this, [this](bool on)
    {
        s.music = on;
        Refresh();
    });
    v->addWidget(SectionHeader("03", "Music", musicSwitch));

    musicOff = Label("The ROM will have no music. Turn this on to pack a GUSBANK from the GUS patches in "
                     "<span style=\"font-family: '" + MonoFamily() + "'\">data/dgguspat/</span>.", "soft");
    musicOff->setStyleSheet(QString("color: %1").arg(TextDim.name()));
    musicOff->setWordWrap(true);
    v->addWidget(musicOff);

    musicOn = new QWidget;
    QVBoxLayout* mv = new QVBoxLayout(musicOn);
    mv->setContentsMargins(0, 0, 0, 0);
    mv->setSpacing(18);

    QVBoxLayout* mapping = new QVBoxLayout;
    mapping->setSpacing(8);

    QHBoxLayout* top = new QHBoxLayout;
    top->addWidget(Label("Instrument mapping", "field"));
    top->addStretch(1);
    QPushButton* other = Button("Use another .cfg…", "link");
    connect(other, &QPushButton::clicked, this, [this]()
    {
        const QString file = QFileDialog::getOpenFileName(this, "Choose a timidity .cfg", GusPatchDir(), "Timidity configs (*.cfg);;All files (*)");

        if(!file.isEmpty())
        {
            SetGusCfg(file);
            Refresh();
        }
    });
    top->addWidget(other);
    mapping->addLayout(top);

    QHBoxLayout* cards = new QHBoxLayout;
    cards->setSpacing(8);

    const char* const names[] = {"Modern", "Retro I", "Retro II"};
    const char* const notes[] = {"Recommended", "Older sound", "Older sound"};

    for(int i = 0; i < 4; i++)
    {
        ChoiceCard* c = new ChoiceCard;

        if(i < 3)
            c->Set(names[i], notes[i], i == 0 ? AccentText : TextFaint, cfgFiles[i]);

        connect(c, &ChoiceCard::clicked, this, [this, i]()
        {
            s.gusCfg = i < 3 ? cfgFiles[i] : customCfg;
            Refresh();
        });

        cfgCards.append(c);
        cards->addWidget(c);
    }

    mapping->addLayout(cards);
    mv->addLayout(mapping);

    QVBoxLayout* over = new QVBoxLayout;
    over->setSpacing(8);
    over->addWidget(Label("Sample oversampling", "field"));

    QHBoxLayout* overRow = new QHBoxLayout;
    overRow->setSpacing(16);

    QFrame* segment = new QFrame;
    segment->setObjectName("segment");
    QHBoxLayout* sh = new QHBoxLayout(segment);
    sh->setContentsMargins(3, 3, 3, 3);
    sh->setSpacing(3);

    overGroup = new QButtonGroup(this);

    for(int n : {1, 2, 4})
    {
        QPushButton* b = Button(QString("%1×").arg(n), "pill");
        b->setCheckable(true);
        b->setAccessibleName(QString("Oversample %1 times").arg(n));
        overGroup->addButton(b, n);
        sh->addWidget(b);
    }

    connect(overGroup, &QButtonGroup::idClicked, this, [this](int id)
    {
        s.oversample = id;
        Refresh();
    });

    overRow->addWidget(segment);
    overText = Label(QString(), "soft");
    overText->setWordWrap(true);
    overRow->addWidget(overText, 1);
    over->addLayout(overRow);

    QLabel* overNote = Label("No extra CPU cost on the GBA. Only instruments used by the WAD's D_* tracks are packed.", "faint");
    overNote->setWordWrap(true);
    over->addWidget(overNote);
    mv->addLayout(over);

    v->addWidget(musicOn);

    return card;
}

QWidget* MainWindow::OutputCard(CheckCard* header, QWidget* body, QFrame** frame)
{
    QFrame* f = new QFrame;
    f->setObjectName("outCard");

    QVBoxLayout* v = new QVBoxLayout(f);
    v->setContentsMargins(1, 1, 1, 1);
    v->setSpacing(0);
    v->addWidget(header);
    v->addWidget(body);

    *frame = f;
    return f;
}

static QWidget* Body(QVBoxLayout** layout)
{
    QWidget* body = new QWidget;
    QVBoxLayout* v = new QVBoxLayout(body);
    v->setContentsMargins(13, 0, 13, 13);
    v->setSpacing(12);
    *layout = v;
    return body;
}

static QVBoxLayout* FieldBlock(const QString& text, QLineEdit* edit, QPushButton* button)
{
    QVBoxLayout* f = new QVBoxLayout;
    f->setSpacing(6);
    QLabel* l = Label(text, "field");
    l->setBuddy(edit);
    f->addWidget(l);
    f->addLayout(EditRow(edit, button));
    return f;
}

QWidget* MainWindow::BuildOutputs()
{
    QVBoxLayout* v;
    QFrame* card = CardFrame(&v, 12);
    v->addWidget(SectionHeader("04", "Outputs"));
    v->addSpacing(4);

    //GBA ROM
    romCheck = new CheckCard("GBA ROM", "Attach the WAD to an engine-only ROM", "cart");
    connect(romCheck, &CheckCard::toggled, this, [this](bool on) { s.rom = on; Refresh(); });

    QVBoxLayout* rb;
    romBody = Body(&rb);

    engineEdit = PathEdit(&s.engine, nullptr, "GBADoom.gba from build.bat");
    QPushButton* engineBrowse = BrowseButton("Browse for engine ROM");
    connect(engineBrowse, &QPushButton::clicked, this, [this]() { BrowseEngine(); });
    QVBoxLayout* engineField = FieldBlock("Engine ROM", engineEdit, engineBrowse);
    engineStatus = new StatusLine;
    engineField->addWidget(engineStatus);
    rb->addLayout(engineField);

    QLineEdit* romOutEdit = PathEdit(&s.romOut, &romOutEdited, "Choose a file name");
    QPushButton* romOutBrowse = BrowseButton("Choose where to save the ROM");
    connect(romOutBrowse, &QPushButton::clicked, this, [this]() { BrowseSave(&s.romOut, &romOutEdited, "Save ROM as", "GBA ROMs (*.gba)"); });
    rb->addLayout(FieldBlock("Save ROM as", romOutEdit, romOutBrowse));

    QVBoxLayout* size = new QVBoxLayout;
    size->setSpacing(6);
    QHBoxLayout* sizeTop = new QHBoxLayout;
    romSizeLabel = Label("Estimated ROM size", "dim");
    sizeTop->addWidget(romSizeLabel);
    sizeTop->addStretch(1);
    romSize = Label(QString(), "dim");
    sizeTop->addWidget(romSize);
    size->addLayout(sizeTop);
    romBar = new SizeBar;
    size->addWidget(romBar);
    overLimit = new StatusLine(nullptr, 13);
    size->addWidget(overLimit);

    overLimitButtons = new QWidget;
    QHBoxLayout* ob = new QHBoxLayout(overLimitButtons);
    ob->setContentsMargins(0, 2, 0, 0);
    ob->setSpacing(8);
    useOneX = Button("Use 1× samples", "small");
    connect(useOneX, &QPushButton::clicked, this, [this]() { s.oversample = 1; Refresh(); });
    QPushButton* wadOnly = Button("Build WAD only", "small");
    connect(wadOnly, &QPushButton::clicked, this, [this]() { s.rom = false; s.wad = true; Refresh(); });
    ob->addWidget(useOneX);
    ob->addWidget(wadOnly);
    ob->addStretch(1);
    size->addWidget(overLimitButtons);
    rb->addLayout(size);

    v->addWidget(OutputCard(romCheck, romBody, &romFrame));

    //WAD
    wadCheck = new CheckCard("WAD file", "For the Qt desktop build", "file");
    connect(wadCheck, &CheckCard::toggled, this, [this](bool on) { s.wad = on; Refresh(); });

    QVBoxLayout* wb;
    wadBody = Body(&wb);
    QLineEdit* wadOutEdit = PathEdit(&s.wadOut, &wadOutEdited, "Choose a file name");
    QPushButton* wadOutBrowse = BrowseButton("Choose where to save the WAD");
    connect(wadOutBrowse, &QPushButton::clicked, this, [this]() { BrowseSave(&s.wadOut, &wadOutEdited, "Save WAD as", "WAD files (*.wad)"); });
    wb->addLayout(FieldBlock("Save WAD as", wadOutEdit, wadOutBrowse));

    v->addWidget(OutputCard(wadCheck, wadBody, &wadFrame));

    //Here rather than below the other cards, so it's on screen without scrolling.
    v->addSpacing(8);
    v->addWidget(BuildFooter());

    return card;
}

QWidget* MainWindow::BuildSpace()
{
    QVBoxLayout* v;
    QFrame* card = CardFrame(&v, 14);

    QWidget* state = new QWidget;
    QHBoxLayout* sh = new QHBoxLayout(state);
    sh->setContentsMargins(0, 0, 0, 0);
    sh->setSpacing(8);
    spaceSpinner = new Spinner(14);
    spaceSpinner->hide();
    sh->addWidget(spaceSpinner);
    spaceState = Label(QString(), "dim");
    sh->addWidget(spaceState);
    v->addWidget(SectionHeader(QString(), "Space used", state));

    spaceChart = new SpaceChart;
    v->addWidget(spaceChart);

    spaceNote = Label(QString(), "faint");
    spaceNote->setWordWrap(true);
    v->addWidget(spaceNote);

    spaceError = new StatusLine(nullptr, 13);
    v->addWidget(spaceError);

    return card;
}

void MainWindow::UpdateMeasurement()
{
    QStringList args;
    QString key;

    if(InputProblem().isEmpty())
    {
        //The WAD alone: the engine's size is known without converting anything.
        WadUtilOptions o = Options();
        o.romIn.clear();
        o.romOut.clear();
        o.outFile.clear();
        o.sizeOnly = true;
        args = BuildArguments(o);

        //Measured again if an input changes on disk.
        key = args.join('\n');

        //A named list: ranging over the temporary << returns would use it after it's gone.
        const QStringList inputs = QStringList() << s.iwad << s.pwads << s.dehs << (s.music ? s.gusCfg : QString());

        for(const QString& f : inputs)
        {
            const QFileInfo fi(f);
            key += QString("\n%1 %2").arg(fi.size()).arg(fi.lastModified().toMSecsSinceEpoch());
        }
    }

    if(key == measureKey)
        return;

    StopMeasurement();
    measureKey = key;
    measureArgs = args;

    //A pause first, so typing a path doesn't start a run per key.
    if(!key.isEmpty() && !measurements.contains(key))
        measureTimer->start();
}

void MainWindow::StartMeasurement()
{
    StopMeasurement();

    QProcess* p = new QProcess(this);
    p->setProgram(QCoreApplication::applicationFilePath());
    p->setArguments(measureArgs);
    p->setWorkingDirectory(exeDir);
    p->setProcessChannelMode(QProcess::MergedChannels);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("QT_MESSAGE_PATTERN", "%{type}|%{message}");
    env.insert("QT_FORCE_STDERR_LOGGING", "1");
    p->setProcessEnvironment(env);

    const QString key = measureKey;

    connect(p, &QProcess::finished, this, [this, p, key](int code, QProcess::ExitStatus status)
    {
        Measurement m;

        for(QString line : QString::fromLocal8Bit(p->readAll()).split('\n'))
        {
            if(line.endsWith('\r'))
                line.chop(1);

            const QString type = line.section('|', 0, 0);
            const QString message = line.section('|', 1);

            if((type == "critical" || type == "fatal") && m.error.isEmpty())
                m.error = message;
            else if(type == "info")
                ParseSpaceLine(message, m.report);
        }

        m.ok = status == QProcess::NormalExit && code == 0 && m.report.total > 0;

        if(!m.ok && m.error.isEmpty())
            m.error = status == QProcess::NormalExit ? QString("GbaWadUtil stopped with exit code %1.").arg(code) : "GbaWadUtil crashed.";

        measurements.insert(key, m);

        if(measureProcess == p)
            measureProcess = nullptr;

        p->deleteLater();
        Refresh();
    });

    measureProcess = p;
    p->start();
}

void MainWindow::StopMeasurement()
{
    measureTimer->stop();

    if(!measureProcess)
        return;

    disconnect(measureProcess, nullptr, this, nullptr);
    measureProcess->kill();
    measureProcess->waitForFinished(1000);
    measureProcess->deleteLater();
    measureProcess = nullptr;
}

QWidget* MainWindow::BuildCommand()
{
    QVBoxLayout* v;
    QFrame* card = CardFrame(&v, 12);

    copyButton = Button("Copy", "small");
    copyButton->setIcon(Icon("copy", Text, 14));
    connect(copyButton, &QPushButton::clicked, this, [this]()
    {
        QGuiApplication::clipboard()->setText(cmdFlat);
        copyButton->setText("Copied");
        QTimer::singleShot(2000, copyButton, [this]() { copyButton->setText("Copy"); });
    });
    v->addWidget(SectionHeader(QString(), "Command line", copyButton));

    cmdView = new QTextEdit;
    cmdView->setObjectName("cmd");
    cmdView->setReadOnly(true);
    cmdView->setLineWrapMode(QTextEdit::NoWrap);
    cmdView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    cmdView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    cmdView->document()->setDocumentMargin(12);
    cmdView->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    v->addWidget(cmdView);

    QLabel* note = Label("The exact arguments the build runs with, from GbaWadUtil's folder. Paste it into a script or CI job.", "faint");
    note->setWordWrap(true);
    v->addWidget(note);

    return card;
}

QWidget* MainWindow::BuildFooter()
{
    QWidget* w = new QWidget;
    QVBoxLayout* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    buildButton = Button("Build", "primary");
    buildButton->setFocusPolicy(Qt::StrongFocus);
    connect(buildButton, &QPushButton::clicked, this, [this]() { StartBuild(); });
    v->addWidget(buildButton);

    blockerLine = new StatusLine(nullptr, 13);
    v->addWidget(blockerLine);

    return w;
}

void MainWindow::FillFromArguments(const QStringList& args)
{
    if(args.isEmpty())
        return;

    QStringList ignored;
    const WadUtilOptions o = ParseArguments(args, &ignored);

    //Relative to where the command ran, not to GbaWadUtil's folder.
    auto abs = [](const QString& p) { return p.isEmpty() ? p : QDir::cleanPath(QDir::current().absoluteFilePath(p)); };

    s.iwad = abs(o.inFile);

    for(const QString& p : o.pwads)
        s.pwads.append(abs(p));

    for(const QString& p : o.dehs)
    {
        s.dehs.append(abs(p));
        dehScans.insert(s.dehs.last(), ScanDehacked(s.dehs.last()));
    }

    s.music = !o.gusCfg.isEmpty();

    if(s.music)
        SetGusCfg(abs(FindGusConfig(o.gusCfg)));

    s.oversample = o.oversample <= 1 ? 1 : (o.oversample <= 2 ? 2 : 4);

    if(s.oversample != o.oversample)
        ignored.append(QString("-oversample %1 (the window offers 1, 2 or 4×, using %2×)").arg(o.oversample).arg(s.oversample));

    s.rom = !o.romIn.isEmpty() || !o.romOut.isEmpty();

    if(!o.romIn.isEmpty())
        s.engine = abs(o.romIn);

    s.romOut = abs(o.romOut);
    romOutEdited = !s.romOut.isEmpty();

    s.wad = !o.outFile.isEmpty();
    s.wadOut = abs(o.outFile);
    wadOutEdited = s.wad;

    //Just -gui: nothing was filled in.
    if(args.size() == 1 && ignored.isEmpty())
        return;

    banner->show();
    bannerIgnored->setVisible(!ignored.isEmpty());
    bannerIgnored->setText("Ignored: " + ignored.join(", "));
}

void MainWindow::SetGusCfg(const QString& file)
{
    s.gusCfg = file;

    for(const QString& c : cfgFiles)
    {
        if(SamePath(c, file))
        {
            s.gusCfg = c;
            return;
        }
    }

    customCfg = file;
}

void MainWindow::SuggestNames()
{
    if(s.iwad.isEmpty())
    {
        if(!romOutEdited)
            s.romOut.clear();

        if(!wadOutEdited)
            s.wadOut.clear();

        return;
    }

    QStringList stems = {QFileInfo(s.iwad).completeBaseName().toLower()};

    for(const QString& p : s.pwads)
        stems.append(QFileInfo(p).completeBaseName().toLower());

    const QString stem = stems.join('-');

    const QStringList inputs = QStringList() << s.iwad << s.pwads << s.dehs << s.engine << s.gusCfg;

    //doom1.wad from doom1.wad would overwrite it.
    auto unused = [&](const QString& path)
    {
        for(const QString& in : inputs)
        {
            if(SamePath(in, path))
            {
                const QFileInfo fi(path);
                return fi.absolutePath() + "/" + fi.completeBaseName() + "-gba." + fi.suffix();
            }
        }

        return path;
    };

    const QString romDir = s.engine.isEmpty() ? QDir::cleanPath(exeDir + "/..") : QFileInfo(s.engine).absolutePath();
    const QString wadDir = QFileInfo(s.iwad).absolutePath();

    if(!romOutEdited)
        s.romOut = unused(romDir + "/" + stem + ".gba");

    if(!wadOutEdited)
        s.wadOut = unused(wadDir + "/" + stem + ".wad");
}

QString MainWindow::InputProblem() const
{
    auto name = [](const QString& p) { return QFileInfo(p).fileName(); };

    const WadKind gbaWad = CheckWad(GbaWadPath());

    if(gbaWad == WadKind::Missing || gbaWad == WadKind::NotWad)
        return "Can't find gbadoom.wad. It must be in data\\ next to GbaWadUtil.exe.";

    if(s.iwad.isEmpty())
        return "Choose a base IWAD to build from.";

    QStringList wads = QStringList() << s.iwad << s.pwads;

    for(const QString& w : wads)
    {
        const WadKind k = CheckWad(w);

        if(k == WadKind::Missing)
            return "Can't find " + name(w) + ".";

        if(k == WadKind::NotWad)
            return name(w) + " is not a WAD file.";
    }

    for(const QString& d : s.dehs)
    {
        if(!QFileInfo(d).isFile())
            return "Can't find " + name(d) + ".";

        if(dehScans.value(d).unusable)
            return name(d) + " can't be applied: " + dehScans.value(d).note.toLower() + ". Convert it to text with DeHackEd 3, or use a text version of it.";
    }

    if(s.music && !QFileInfo(s.gusCfg).isFile())
        return "Can't find the instrument mapping " + name(s.gusCfg) + ".";

    return QString();
}

QString MainWindow::Blocker() const
{
    auto name = [](const QString& p) { return QFileInfo(p).fileName(); };

    const QString input = InputProblem();

    if(!input.isEmpty())
        return input;

    if(!s.rom && !s.wad)
        return "Pick at least one output.";

    if(s.rom)
    {
        if(s.engine.isEmpty() || s.romOut.isEmpty())
            return "A ROM needs both an engine ROM and a place to save it.";

        const RomKind k = CheckRom(s.engine).kind;

        if(k == RomKind::Missing)
            return "Can't find the engine ROM " + name(s.engine) + ".";

        if(k == RomKind::NoHeader)
            return name(s.engine) + " is not an engine-only GBADoom ROM.";
    }

    if(s.wad && s.wadOut.isEmpty())
        return "Choose where to save the WAD.";

    const QStringList inputs = QStringList() << s.iwad << s.pwads << s.dehs << s.gusCfg;

    for(const QString& out : {s.rom ? s.romOut : QString(), s.wad ? s.wadOut : QString()})
    {
        for(const QString& in : inputs)
        {
            if(SamePath(in, out))
                return name(out) + " is an input. Choose another name to save to.";
        }
    }

    return QString();
}

WadUtilOptions MainWindow::Options() const
{
    WadUtilOptions o;

    o.inFile = Shown(s.iwad);

    for(const QString& p : s.pwads)
        o.pwads.append(Shown(p));

    for(const QString& p : s.dehs)
        o.dehs.append(Shown(p));

    if(s.music)
    {
        o.gusCfg = Shown(s.gusCfg);
        o.oversample = s.oversample;
    }

    if(s.rom)
    {
        o.romIn = Shown(s.engine);
        o.romOut = Shown(s.romOut);
    }

    if(s.wad)
        o.outFile = Shown(s.wadOut);

    return o;
}

void MainWindow::Refresh()
{
    if(refreshing)
        return;

    refreshing = true;

    SuggestNames();

    for(const auto& pe : pathEdits)
    {
        const QString shown = Shown(*pe.second);

        if(!pe.first->hasFocus() && pe.first->text() != shown)
            pe.first->setText(shown);
    }

    //Base IWAD
    QString tag;
    QColor tagColor = Ok;
    bool bad = false;

    switch(CheckWad(s.iwad))
    {
        case WadKind::None: tag = "No IWAD selected"; tagColor = Error; bad = true; break;
        case WadKind::Missing: tag = "File not found"; tagColor = Error; bad = true; break;
        case WadKind::NotWad: tag = "Not a WAD file"; tagColor = Error; bad = true; break;
        case WadKind::Pwad: tag = "A PWAD, not an IWAD"; tagColor = Warn; break;

        case WadKind::Iwad:
            tag = KnownIwad(s.iwad);

            if(tag.isEmpty())
            {
                tag = "IWAD, not a known name";
                tagColor = Warn;
            }
            else
                tag = "Detected: " + tag;

            break;
    }

    iwadTag->setText(tag);
    iwadTag->setStyleSheet(QString("color: %1; font-size: 12px").arg(tagColor.name()));
    SetStyleProperty(iwadEdit, "error", bad);

    iwadRowName->setText(s.iwad.isEmpty() ? "—" : QFileInfo(s.iwad).fileName());
    iwadRowName->setToolTip(QDir::toNativeSeparators(s.iwad));
    gbaWadOrder->setText(QString::number(s.pwads.size() + 2));

    //PWADs whose maps are for the other game than the IWAD's.
    const MapGame iwadGame = s.iwad.isEmpty() ? MapGame::None : ScanWad(s.iwad).mapGame;

    for(int i = 0; i < pwadWarnings.size() && i < s.pwads.size(); i++)
    {
        const DehScan p = ScanWad(s.pwads[i]);
        const QString warning = MapGameWarning(QFileInfo(s.pwads[i]).fileName(), p.mapGame, p.maps, QFileInfo(s.iwad).fileName(), iwadGame);

        pwadWarnings[i]->setText(p.mapGame == MapGame::Doom1 ? "Doom 1 maps, left out on a Doom 2 IWAD" : "Doom 2 maps, need a Doom 2 IWAD");
        pwadWarnings[i]->setToolTip(warning);
        pwadWarnings[i]->parentWidget()->setVisible(!warning.isEmpty());
    }

    //DeHackEd: light the blocks that the WADs' DEHACKED lumps and the patch files change.
    QStringList changedBy[numDehBlocks];
    QStringList withLumps;
    QStringList lumpNotes;

    auto changes = [&](const DehScan& d, const QString& name)
    {
        for(int b = 0; b < numDehBlocks; b++)
        {
            if(d.changes & (1 << b))
                changedBy[b].append(name);
        }
    };

    const QStringList wads = QStringList() << s.iwad << s.pwads;

    for(const QString& w : wads)
    {
        const DehScan d = w.isEmpty() ? DehScan() : ScanWad(w);
        const QString name = QFileInfo(w).fileName();

        if(!d.lumps)
            continue;

        withLumps.append(name);
        changes(d, name);

        if(!d.note.isEmpty())
            lumpNotes.append(name + ": " + d.note);
    }

    for(const QString& p : s.dehs)
        changes(dehScans.value(p), QFileInfo(p).fileName());

    QString autoText = QString("DEHACKED lumps inside the WADs&nbsp;&nbsp;<span style=\"color: %1; font-size: 12px\">%2</span>")
                           .arg(TextFaint.name(), withLumps.isEmpty() ? "applied first, in load order" : ("in " + JoinAnd(withLumps) + ", applied first").toHtmlEscaped());

    if(!lumpNotes.isEmpty())
        autoText += QString("&nbsp;&nbsp;<span style=\"color: %1; font-size: 12px\">%2</span>").arg(Warn.name(), lumpNotes.join("; ").toHtmlEscaped());

    dehAutoText->setText(autoText);
    dehAutoText->setToolTip(lumpNotes.join("\n"));

    for(int b = 0; b < numDehBlocks; b++)
    {
        SetStyleProperty(dehChips[b], "role", changedBy[b].isEmpty() ? "chip" : "chipOn");
        dehChips[b]->setToolTip(changedBy[b].isEmpty() ? QString("No patch here has %1 blocks").arg(dehBlocks[b])
                                                       : QString("%1 blocks in %2").arg(dehBlocks[b], JoinAnd(changedBy[b])));
    }

    //Music
    {
        const QSignalBlocker b(musicSwitch);
        musicSwitch->setChecked(s.music);
    }

    musicOff->setVisible(!s.music);
    musicOn->setVisible(s.music);

    cfgCards[3]->setVisible(!customCfg.isEmpty());

    if(!customCfg.isEmpty())
        cfgCards[3]->Set(QFileInfo(customCfg).completeBaseName(), "Your own", TextFaint, customCfg);

    for(int i = 0; i < 4; i++)
        cfgCards[i]->setChecked(SamePath(i < 3 ? cfgFiles[i] : customCfg, s.gusCfg));

    overGroup->button(s.oversample)->setChecked(true);
    overText->setText(s.oversample == 1 ? "Native rate. Smallest ROM." :
                      s.oversample == 2 ? "Half the resampling error, twice the sample space." :
                                          "A quarter of the resampling error, four times the sample space.");

    //Outputs
    const struct { CheckCard* check; QWidget* body; QFrame* frame; bool on; } outs[] =
    {
        {romCheck, romBody, romFrame, s.rom}, {wadCheck, wadBody, wadFrame, s.wad}
    };

    for(const auto& o : outs)
    {
        const QSignalBlocker b(o.check);
        o.check->setChecked(o.on);
        o.body->setVisible(o.on);
        SetStyleProperty(o.frame, "on", o.on);
    }

    const RomCheck rom = CheckRom(s.engine);

    switch(rom.kind)
    {
        case RomKind::None: engineStatus->Set(QString(), Text, QString()); break;
        case RomKind::Missing: engineStatus->Set("error", Error, "File not found."); break;
        case RomKind::NoHeader: engineStatus->Set("error", Error, "No GBADOOM-IWAD-HDR header in this file. Pick an engine-only GBADoom ROM."); break;
        case RomKind::Empty: engineStatus->Set("check", Ok, "IWAD header found · no WAD attached yet"); break;
        case RomKind::HasWad: engineStatus->Set("info", AccentText, "Header found. The WAD already attached will be replaced."); break;
    }

    SetStyleProperty(engineEdit, "error", rom.kind == RomKind::Missing || rom.kind == RomKind::NoHeader);

    //Space: measured by a background -size run, a rough guess until it reports.
    UpdateMeasurement();

    const bool canMeasure = !measureKey.isEmpty();
    const auto measured = measurements.constFind(measureKey);
    const bool known = canMeasure && measured != measurements.constEnd();
    const bool exact = known && measured->ok;

    qint64 wadBytes = 0;

    if(exact)
        wadBytes = measured->report.total;
    else
    {
        //Processed WADs come out about the size they went in; the GUS bank is measured
        //from the stock WADs with timidity.cfg.
        wadBytes = QFileInfo(s.iwad).size() + QFileInfo(GbaWadPath()).size();

        for(const QString& w : s.pwads)
            wadBytes += QFileInfo(w).size();

        if(s.music)
            wadBytes += qint64((0.2 + 1.2 * s.oversample) * 1024 * 1024);
    }

    const double mb = (rom.engineBytes + wadBytes) / (1024.0 * 1024.0);
    const bool sized = canMeasure && (!known || exact);

    const QColor barColor = mb > romLimitMB ? ErrorBar : (mb > romLimitMB - 4 ? Warn : Bar);

    if(!sized)
        romSize->setText(QString("<span style=\"font-family: '%1'; color: %2\">— / 32 MB</span>").arg(MonoFamily(), TextFaint.name()));
    else
        romSize->setText(QString("<span style=\"color: %2\">%5</span><span style=\"font-family: '%1'; color: %2\">%3 MB</span> <span style=\"font-family: '%1'; color: %4\">/ 32 MB</span>")
                     .arg(MonoFamily(), mb > romLimitMB ? Error.name() : Text.name()).arg(mb, 0, 'f', 1).arg(TextFaint.name(), exact ? "" : "≈ "));

    romSizeLabel->setText(exact ? "ROM size" : "Estimated ROM size");
    romSize->setToolTip(exact ? "Measured by converting the WADs without writing anything."
                              : "A rough guess from the files' sizes, until the WADs have been measured.");
    romBar->Set(sized ? mb / romLimitMB : 0, barColor);

    const bool over = mb > romLimitMB && sized;
    overLimit->Set(QString(), Error, over ? QString("%1 MB over the cartridge limit.").arg(mb - romLimitMB, 0, 'f', 1) : QString());
    overLimitButtons->setVisible(over);
    useOneX->setVisible(s.music && s.oversample > 1);

    //Space used: the last measurement stays up while the next one runs.
    if(exact)
        lastSpace = measured->report;

    const bool measuring = canMeasure && !known;
    const bool failed = known && !measured->ok;
    const bool chart = canMeasure && !failed && !lastSpace.parts.isEmpty();

    if(chart)
    {
        QVector<qint64> parts = lastSpace.parts;
        parts[0] = s.rom ? rom.engineBytes : 0;
        spaceChart->Set(parts, s.rom ? qint64(romLimitMB * 1024 * 1024) : 0);
    }

    spaceChart->setVisible(chart);
    spaceChart->setEnabled(!measuring);
    spaceSpinner->setVisible(measuring);
    spaceState->setText(measuring ? "Measuring…" : (exact ? QString("%1 MB%2").arg(mb, 0, 'f', 1).arg(s.rom ? " of 32 MB" : "") : QString()));
    spaceNote->setVisible(!chart && !failed);
    spaceNote->setText(!canMeasure ? (s.iwad.isEmpty() ? "Choose a base IWAD to see what the space goes on." : "Can't measure it: " + InputProblem()) :
                                     "Converting the WADs to measure them. Big megawads take a while.");
    spaceError->Set("error", Error, failed ? "This can't be built: " + measured->error : QString());

    //Command line
    QStringList args = BuildArguments(Options());

    if(s.iwad.isEmpty())
        args = QStringList({"-in", "<iwad>"}) + args;

    QStringList quoted;

    for(const QString& a : args)
        quoted.append(Quote(a));

    cmdFlat = "GbaWadUtil " + quoted.join(' ');

    QString html = QString("<div style=\"font-family: '%1'; font-size: 12px; line-height: 160%\"><span style=\"color: %2\">GbaWadUtil</span>")
                       .arg(MonoFamily(), Text.name());
    //One option per line. Options that take one value can take one starting with -.
    static const QStringList singles = {"-in", "-gus", "-oversample", "-rom", "-romout", "-out"};
    bool isValue = false;

    for(const QString& a : args)
    {
        if(!isValue && a.startsWith('-'))
        {
            html += QString("<br>&nbsp;&nbsp;&nbsp;&nbsp;<span style=\"color: %1\">%2</span>").arg(AccentText.name(), a);
            isValue = singles.contains(a);
        }
        else
        {
            html += QString(" <span style=\"color: %1\">%2</span>").arg(TextSoft.name(), Quote(a).toHtmlEscaped());
            isValue = false;
        }
    }

    html += "</div>";
    cmdView->setHtml(html);
    cmdView->setFixedHeight(int(cmdView->document()->size().height()) + 2 + 12);

    //Build
    const QString blocker = Blocker();
    const int count = int(s.rom) + int(s.wad);

    buildButton->setEnabled(blocker.isEmpty());
    buildButton->setText(blocker.isEmpty() ? (count == 1 ? "Build 1 output" : QString("Build %1 outputs").arg(count)) : "Build");
    buildButton->setIcon(blocker.isEmpty() ? Icon("play", Qt::white, 16) : QIcon());
    blockerLine->Set("warn", Error, blocker);

    refreshing = false;
}

void MainWindow::AddPwads(const QStringList& files)
{
    for(const QString& f : files)
    {
        const QString p = QDir::cleanPath(f);

        if(!s.pwads.contains(p, Qt::CaseInsensitive) && !SamePath(p, s.iwad))
            s.pwads.append(p);
    }

    RebuildPwadRows();
    Refresh();
}

void MainWindow::AddDehs(const QStringList& files)
{
    for(const QString& f : files)
    {
        const QString p = QDir::cleanPath(f);

        if(!s.dehs.contains(p, Qt::CaseInsensitive))
            s.dehs.append(p);

        dehScans.insert(p, ScanDehacked(p));
    }

    RebuildDehRows();
    Refresh();
}

void MainWindow::AddFiles(const QStringList& files)
{
    QStringList wads;
    QStringList dehs;

    for(const QString& f : files)
    {
        const QString ext = QFileInfo(f).suffix().toLower();

        if(ext == "deh" || ext == "bex")
            dehs.append(f);
        else if(ext == "gba")
            s.engine = QDir::cleanPath(f);
        else if(ext == "cfg")
        {
            s.music = true;
            SetGusCfg(QDir::cleanPath(f));
        }
        else if(CheckWad(f) == WadKind::Iwad && s.iwad.isEmpty())
            s.iwad = QDir::cleanPath(f);
        else if(CheckWad(f) == WadKind::Iwad || CheckWad(f) == WadKind::Pwad)
            wads.append(f);
    }

    AddPwads(wads);
    AddDehs(dehs);
}

void MainWindow::BrowseIwad()
{
    const QString file = QFileDialog::getOpenFileName(this, "Choose the base IWAD", s.iwad.isEmpty() ? StartDir() : s.iwad, "WAD files (*.wad);;All files (*)");

    if(file.isEmpty())
        return;

    lastDir = QFileInfo(file).absolutePath();
    s.iwad = QDir::cleanPath(file);
    s.pwads.removeAll(s.iwad);
    RebuildPwadRows();
    Refresh();
}

void MainWindow::BrowseEngine()
{
    const QString file = QFileDialog::getOpenFileName(this, "Choose the engine ROM", s.engine.isEmpty() ? StartDir() : s.engine, "GBA ROMs (*.gba);;All files (*)");

    if(file.isEmpty())
        return;

    s.engine = QDir::cleanPath(file);
    Refresh();
}

void MainWindow::BrowseSave(QString* field, bool* edited, const QString& title, const QString& filter)
{
    const QString file = QFileDialog::getSaveFileName(this, title, field->isEmpty() ? StartDir() : *field, filter);

    if(file.isEmpty())
        return;

    *field = QDir::cleanPath(file);
    *edited = true;
    Refresh();
}

void MainWindow::OpenPreset()
{
    const QString file = QFileDialog::getOpenFileName(this, "Open preset", StartDir(), "GbaWadUtil presets (*.json);;All files (*)");

    if(file.isEmpty())
        return;

    QFile f(file);
    QJsonParseError err;
    const QJsonDocument doc = f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll(), &err) : QJsonDocument();

    if(!doc.isObject())
    {
        QMessageBox::warning(this, "Open preset", "Can't read " + QFileInfo(file).fileName() + " as a GbaWadUtil preset.");
        return;
    }

    lastDir = QFileInfo(file).absolutePath();

    const QJsonObject o = doc.object();

    //Relative paths in a preset are relative to the preset.
    const QDir dir = QFileInfo(file).absoluteDir();
    auto path = [&](const char* key) { const QString p = o.value(key).toString(); return p.isEmpty() ? p : QDir::cleanPath(dir.absoluteFilePath(p)); };
    auto paths = [&](const char* key)
    {
        QStringList l;

        for(const QJsonValue& v : o.value(key).toArray())
            l.append(QDir::cleanPath(dir.absoluteFilePath(v.toString())));

        return l;
    };

    s = GuiSettings();
    s.iwad = path("iwad");
    s.pwads = paths("pwads");
    s.dehs = paths("dehs");
    s.music = o.value("music").toBool(true);
    s.gusCfg = cfgFiles[0];

    if(!o.value("gus").toString().isEmpty())
        SetGusCfg(FindGusConfig(path("gus")));

    const int over = o.value("oversample").toInt(4);
    s.oversample = over <= 1 ? 1 : (over <= 2 ? 2 : 4);
    s.rom = o.value("rom").toBool(true);
    s.engine = path("engine");
    s.romOut = path("romOut");
    s.wad = o.value("wad").toBool(false);
    s.wadOut = path("wadOut");

    romOutEdited = !s.romOut.isEmpty();
    wadOutEdited = !s.wadOut.isEmpty();

    for(const QString& d : s.dehs)
        dehScans.insert(d, ScanDehacked(d));

    banner->hide();
    RebuildPwadRows();
    RebuildDehRows();
    Refresh();
}

void MainWindow::SavePreset()
{
    const QString name = s.iwad.isEmpty() ? "preset" : QFileInfo(s.romOut.isEmpty() ? s.iwad : s.romOut).completeBaseName();
    const QString file = QFileDialog::getSaveFileName(this, "Save preset", StartDir() + "/" + name + ".json", "GbaWadUtil presets (*.json)");

    if(file.isEmpty())
        return;

    lastDir = QFileInfo(file).absolutePath();

    QJsonObject o;
    o["iwad"] = s.iwad;
    o["pwads"] = QJsonArray::fromStringList(s.pwads);
    o["dehs"] = QJsonArray::fromStringList(s.dehs);
    o["music"] = s.music;
    o["gus"] = s.gusCfg;
    o["oversample"] = s.oversample;
    o["rom"] = s.rom;
    o["engine"] = s.engine;
    o["romOut"] = s.romOut;
    o["wad"] = s.wad;
    o["wadOut"] = s.wadOut;

    QFile f(file);

    if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(QJsonDocument(o).toJson()) < 0)
        QMessageBox::warning(this, "Save preset", "Can't write " + QFileInfo(file).fileName() + ".");
}

void MainWindow::StartBuild()
{
    if(!Blocker().isEmpty() || buildPage->Running())
        return;

    QList<BuildOutput> outputs;

    if(s.rom)
        outputs.append({BuildOutput::Rom, s.romOut});

    if(s.wad)
        outputs.append({BuildOutput::Wad, s.wadOut});

    buildPage->Start(QCoreApplication::applicationFilePath(), BuildArguments(Options()), exeDir, outputs, s.music ? s.oversample : 0);
    stack->setCurrentWidget(buildPage);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e)
{
    if(stack->currentWidget() != settingsPage || !e->mimeData()->hasUrls())
        return;

    for(const QUrl& u : e->mimeData()->urls())
    {
        if(u.isLocalFile())
        {
            e->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent* e)
{
    QStringList files;

    for(const QUrl& u : e->mimeData()->urls())
    {
        if(u.isLocalFile())
            files.append(u.toLocalFile());
    }

    AddFiles(files);
    e->acceptProposedAction();
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    StopMeasurement();
    buildPage->Stop();
    e->accept();
}


int RunGui(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    //Started from Explorer, the console window is GbaWadUtil's own, so close it.
    //From a command prompt it is the prompt's, so leave it.
    DWORD processes[2];

    if(GetConsoleProcessList(processes, 2) == 1)
        FreeConsole();
#endif

    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));

    //For the parts the style sheet doesn't reach: menus, message boxes, selections.
    QPalette pal;
    pal.setColor(QPalette::Window, Card);
    pal.setColor(QPalette::WindowText, Text);
    pal.setColor(QPalette::Base, Field);
    pal.setColor(QPalette::AlternateBase, Card);
    pal.setColor(QPalette::Text, Text);
    pal.setColor(QPalette::Button, QColor(0x2A2C32));
    pal.setColor(QPalette::ButtonText, Text);
    pal.setColor(QPalette::Highlight, Accent);
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::ToolTipBase, QColor(0x24262B));
    pal.setColor(QPalette::ToolTipText, Text);
    pal.setColor(QPalette::PlaceholderText, TextFaint);
    pal.setColor(QPalette::Disabled, QPalette::Text, TextFaint);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, TextFaint);
    app.setPalette(pal);
    app.setStyleSheet(StyleSheet());

    MainWindow w(app.arguments().mid(1));
    w.show();

    return app.exec();
}
