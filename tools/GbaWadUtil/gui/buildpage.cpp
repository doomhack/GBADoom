#include "buildpage.h"
#include "ui.h"
#include "spacechart.h"
#include "../options.h"

using namespace Theme;

//One build stage: a status dot, its name and a line on how it went.
class StageCard : public QFrame
{
public:
    enum State { Pending, Running, Ok, Warn, Fail, Off };

    explicit StageCard(const QString& name)
    {
        setObjectName("stage");

        QHBoxLayout* h = new QHBoxLayout(this);
        h->setContentsMargins(14, 11, 14, 11);
        h->setSpacing(10);

        dot = new QLabel;
        dot->setFixedSize(22, 22);
        h->addWidget(dot);

        QVBoxLayout* v = new QVBoxLayout;
        v->setSpacing(0);
        QLabel* title = Label(name, "body");
        title->setStyleSheet("font-weight: 600");
        v->addWidget(title);
        note = Label(QString(), "dim");
        v->addWidget(note);
        h->addLayout(v, 1);
    }

    void Set(State state, const QString& text)
    {
        static const struct { const char* icon; QColor ground; QColor fore; qreal stroke; } looks[] =
        {
            {"dot", QColor(0x2A2C32), TextFaint, 4},
            {"dot", QColor(0x27254A), AccentText, 6},
            {"check", OkGround, Ok, 3},
            {"bang", WarnGround, Warn, 2.5},
            {"close", ErrorGround, Error, 2.5},
            {"dash", QColor(0x2A2C32), TextFaint, 2.5},
        };

        const auto& l = looks[state];

        QPixmap pm(QSize(22, 22) * 2);
        pm.fill(Qt::transparent);
        pm.setDevicePixelRatio(2);

        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(l.ground);
        p.drawEllipse(QRectF(0, 0, 22, 22));
        p.drawPixmap(QPointF(4.5, 4.5), Pixmap(l.icon, l.fore, 13, l.stroke, 2));
        p.end();

        dot->setPixmap(pm);
        note->setText(text);
        note->setStyleSheet(state == Warn ? QString("color: %1").arg(Theme::Warn.name()) :
                            state == Fail ? QString("color: %1").arg(Theme::Error.name()) : QString());

        SetStyleProperty(this, "state", state == Warn ? "warn" : (state == Fail ? "fail" : ""));
    }

private:
    QLabel* dot;
    QLabel* note;
};

//The stage cards in as many columns as fit.
class StageGrid : public QWidget
{
public:
    explicit StageGrid(const QList<QWidget*>& cards) : cards(cards)
    {
        grid = new QGridLayout(this);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(8);
        Place(int(cards.size()));
    }

protected:
    void resizeEvent(QResizeEvent* e) override
    {
        const int n = int(cards.size());
        const int cols = qBound(1, e->size().width() / 190, n);

        //An odd last row looks unfinished: 5 cards go 5, 3+2 or 1.
        Place(cols >= n ? n : (cols >= 3 ? 3 : (cols >= 2 ? 2 : 1)));
        QWidget::resizeEvent(e);
    }

private:
    void Place(int cols)
    {
        if(cols == columns)
            return;

        columns = cols;

        for(QWidget* c : cards)
            grid->removeWidget(c);

        for(int i = 0; i < 8; i++)
            grid->setColumnStretch(i, i < cols ? 1 : 0);

        for(int i = 0; i < cards.size(); i++)
            grid->addWidget(cards[i], i / cols, i % cols);
    }

    QList<QWidget*> cards;
    QGridLayout* grid;
    int columns = 0;
};

static QString OutputName(BuildOutput::Kind k)
{
    return k == BuildOutput::Rom ? "GBA ROM" : (k == BuildOutput::Wad ? "WAD file" : "C source");
}

static QString Megabytes(qint64 bytes)
{
    return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
}

static QPixmap Badge(const QString& icon, const QColor& ground, const QColor& fore, int size)
{
    QPixmap pm(QSize(size, size) * 2);
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(2);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(ground);
    p.drawEllipse(QRectF(0, 0, size, size));

    const int inner = size / 2;
    p.drawPixmap(QPointF((size - inner) / 2.0, (size - inner) / 2.0), Pixmap(icon, fore, inner, 2.5, 2));

    return pm;
}

static void ShowInFolder(const QString& path)
{
#ifdef Q_OS_WIN
    QProcess p;
    p.setProgram("explorer.exe");
    p.setNativeArguments("/select,\"" + QDir::toNativeSeparators(path) + "\"");
    p.startDetached();
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}


BuildPage::BuildPage(QWidget* parent) : QWidget(parent)
{
    setObjectName("page");

    QVBoxLayout* page = new QVBoxLayout(this);
    page->setContentsMargins(0, 0, 0, 0);
    page->setSpacing(0);

    //Header
    QFrame* header = new QFrame;
    header->setObjectName("header");
    QHBoxLayout* hh = new QHBoxLayout(header);
    hh->setContentsMargins(24, 14, 24, 14);
    hh->setSpacing(12);

    QLabel* logo = new QLabel;
    logo->setPixmap(Logo(34, 2.0));
    hh->addWidget(logo);

    QVBoxLayout* names = new QVBoxLayout;
    names->setSpacing(0);
    names->addWidget(Label("GbaWadUtil", "appName"));
    names->addWidget(Label("Build a GBADoom ROM, WAD or C source", "dim"));
    hh->addLayout(names);
    hh->addStretch(1);

    backButton = Button("Back to settings", "header");
    backButton->setIcon(Icon("back", Text, 16));
    connect(backButton, &QPushButton::clicked, this, [this]() { if(onBack) onBack(); });
    hh->addWidget(backButton);
    page->addWidget(header);

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

    //Stages
    const char* const stageNames[NumStages] = {"Merge WADs", "DeHackEd", "Convert", "Music", "Write outputs"};
    QList<QWidget*> cards;

    for(int i = 0; i < NumStages; i++)
    {
        stages[i] = new StageCard(stageNames[i]);
        cards.append(stages[i]);
    }

    v->addWidget(new StageGrid(cards));

    //Log
    QFrame* logCard = new QFrame;
    logCard->setObjectName("card");
    QVBoxLayout* lv = new QVBoxLayout(logCard);
    lv->setContentsMargins(1, 1, 1, 1);
    lv->setSpacing(0);

    QWidget* logTop = new QWidget;
    QHBoxLayout* lt = new QHBoxLayout(logTop);
    lt->setContentsMargins(15, 12, 15, 12);
    lt->addWidget(Label("Build log", "title"));
    lt->addStretch(1);

    QFrame* segment = new QFrame;
    segment->setObjectName("segment");
    QHBoxLayout* sh = new QHBoxLayout(segment);
    sh->setContentsMargins(3, 3, 3, 3);
    sh->setSpacing(3);
    allButton = Button("All", "tab");
    warnButton = Button("Warnings", "tab");
    allButton->setCheckable(true);
    warnButton->setCheckable(true);
    allButton->setChecked(true);
    QButtonGroup* filter = new QButtonGroup(this);
    filter->addButton(allButton);
    filter->addButton(warnButton);
    connect(allButton, &QPushButton::clicked, this, [this]() { warnOnly = false; RenderLog(); });
    connect(warnButton, &QPushButton::clicked, this, [this]() { warnOnly = true; RenderLog(); });
    sh->addWidget(allButton);
    sh->addWidget(warnButton);
    lt->addWidget(segment);
    lv->addWidget(logTop);

    QFrame* rule = new QFrame;
    rule->setObjectName("topRule");
    rule->setFixedHeight(1);
    lv->addWidget(rule);

    log = new QPlainTextEdit;
    log->setObjectName("log");
    log->setReadOnly(true);
    log->setLineWrapMode(QPlainTextEdit::NoWrap);
    log->setMinimumHeight(340);
    log->document()->setDocumentMargin(12);
    lv->addWidget(log, 1);

    //Result
    QFrame* done = new QFrame;
    done->setObjectName("card");
    QVBoxLayout* dv = new QVBoxLayout(done);
    dv->setContentsMargins(20, 20, 20, 20);
    dv->setSpacing(18);

    QHBoxLayout* head = new QHBoxLayout;
    head->setSpacing(12);
    doneIcon = new QLabel;
    doneIcon->setFixedSize(36, 36);
    head->addWidget(doneIcon);
    QVBoxLayout* titles = new QVBoxLayout;
    titles->setSpacing(0);
    doneTitle = Label(QString(), "bigTitle");
    doneSubtitle = Label(QString(), "dim");
    doneSubtitle->setWordWrap(true);
    titles->addWidget(doneTitle);
    titles->addWidget(doneSubtitle);
    head->addLayout(titles, 1);
    dv->addLayout(head);

    outputsBox = new QWidget;
    QVBoxLayout* ov = new QVBoxLayout(outputsBox);
    ov->setContentsMargins(0, 0, 0, 0);
    ov->setSpacing(10);
    dv->addWidget(outputsBox);

    spaceBox = new QWidget;
    QVBoxLayout* sv = new QVBoxLayout(spaceBox);
    sv->setContentsMargins(0, 0, 0, 0);
    sv->setSpacing(10);
    QLabel* space = Label("Space used", "dim");
    space->setStyleSheet("font-size: 13px; font-weight: 500");
    sv->addWidget(space);
    spaceChart = new SpaceChart;
    sv->addWidget(spaceChart);
    dv->addWidget(spaceBox);

    lookBox = new QWidget;
    QVBoxLayout* kv = new QVBoxLayout(lookBox);
    kv->setContentsMargins(0, 0, 0, 0);
    kv->setSpacing(10);
    QLabel* look = Label("Needs a look", "dim");
    look->setStyleSheet("font-size: 13px; font-weight: 500");
    kv->addWidget(look);
    lookList = new QVBoxLayout;
    lookList->setSpacing(10);
    kv->addLayout(lookList);
    dv->addWidget(lookBox);

    QFrame* bottomRule = new QFrame;
    bottomRule->setObjectName("topRule");
    bottomRule->setFixedHeight(1);
    dv->addWidget(bottomRule);

    QHBoxLayout* actions = new QHBoxLayout;
    actions->setSpacing(8);
    changeButton = Button("Change settings");
    againButton = Button("Build again");
    stopButton = Button("Stop");
    saveLogButton = Button("Save log…", "link");
    connect(changeButton, &QPushButton::clicked, this, [this]() { if(onBack) onBack(); });
    connect(againButton, &QPushButton::clicked, this, [this]() { if(onBuildAgain) onBuildAgain(); });
    connect(stopButton, &QPushButton::clicked, this, [this]() { Stop(); });
    connect(saveLogButton, &QPushButton::clicked, this, [this]() { SaveLog(); });
    actions->addWidget(changeButton);
    actions->addWidget(againButton);
    actions->addWidget(stopButton);
    actions->addWidget(saveLogButton);
    actions->addStretch(1);
    dv->addLayout(actions);

    QWidget* right = new QWidget;
    QVBoxLayout* rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->addWidget(done);
    rv->addStretch(1);

    v->addWidget(new Columns(logCard, right, 1000, 400), 1);

    scroll->setWidget(content);
    page->addWidget(scroll, 1);

    ticker = new QTimer(this);
    ticker->setInterval(250);
    connect(ticker, &QTimer::timeout, this, [this]() { UpdatePanel(); });
}

bool BuildPage::Running() const
{
    return running;
}

void BuildPage::Start(const QString& program, const QStringList& args, const QString& workDir, const QList<BuildOutput>& outs, int over)
{
    if(running)
        return;

    outputs = outs;
    oversample = over;
    lines.clear();
    pending.clear();
    pendingLevel = Level::Info;
    warnings = 0;
    stage = Merge;
    succeeded = false;
    stopped = false;
    failure.clear();
    hasSpace = false;

    dehNames.clear();

    for(const QString& d : ParseArguments(args).dehs)
        dehNames.append(QFileInfo(d).fileName());

    //The finished build replaces the files, so don't show the last build's.
    ClearLayout(outputsBox->layout());
    ClearLayout(lookList);

    delete process;
    process = new QProcess(this);
    process->setProgram(program);
    process->setArguments(args);
    process->setWorkingDirectory(workDir);
    process->setProcessChannelMode(QProcess::MergedChannels);

    //Say each message's type, and log to the pipe even without a console.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("QT_MESSAGE_PATTERN", "%{type}|%{message}");
    env.insert("QT_FORCE_STDERR_LOGGING", "1");
    process->setProcessEnvironment(env);

    connect(process, &QProcess::readyRead, this, [this]() { ReadOutput(false); });
    connect(process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) { Finished(code, status); });
    connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e)
    {
        if(e == QProcess::FailedToStart && running)
        {
            failure = "GbaWadUtil couldn't be started: " + process->errorString();
            Finished(-1, QProcess::CrashExit);
        }
    });

    QStringList quoted;

    for(const QString& a : args)
        quoted.append(a.contains(' ') ? '"' + a + '"' : a);

    RenderLog();
    AddLine(Level::Info, "GbaWadUtil " + quoted.join(' '), Merge);

    running = true;
    timer.start();
    ticker->start();
    process->start();

    UpdateStages();
    UpdatePanel();
}

void BuildPage::Stop()
{
    if(!running || !process)
        return;

    stopped = true;
    process->kill();
    process->waitForFinished(2000);
}

void BuildPage::ReadOutput(bool flush)
{
    pending += process->readAll();

    for(;;)
    {
        int nl = pending.indexOf('\n');

        if(nl < 0)
        {
            if(!flush || pending.isEmpty())
                break;

            nl = int(pending.size());
        }

        QString text = QString::fromLocal8Bit(pending.left(nl));
        pending.remove(0, nl + 1);

        if(text.endsWith('\r'))
            text.chop(1);

        //"type|message" from QT_MESSAGE_PATTERN. Lines without it continue the last message.
        static const QHash<QString, Level> types =
        {
            {"debug", Level::Ok}, {"info", Level::Ok}, {"warning", Level::Warn}, {"critical", Level::Error}, {"fatal", Level::Error}
        };

        const int bar = text.indexOf('|');
        const auto type = types.constFind(bar > 0 ? text.left(bar) : QString());

        if(type != types.constEnd())
        {
            pendingLevel = *type;
            text = text.mid(bar + 1);
        }

        Level level = pendingLevel;

        if(text.trimmed().isEmpty())
            continue;

        //DeHackEd's notes on what it applied, under its summary line.
        if(level == Level::Ok && text.startsWith("  "))
            level = text.contains("Skipped (not supported)") ? Level::Warn : Level::Info;

        AddLine(level, text, StageOf(text));
    }
}

int BuildPage::StageOf(const QString& t) const
{
    if(t.startsWith("Loaded ") || t.startsWith("Merged ") || t.startsWith("Can't read WAD") || t.startsWith("Ignored ") ||
       t.contains("gbadoom.wad"))
        return Merge;

    if(t.contains("DeHackEd") || t.startsWith("  Skipped (not supported)") || t.contains("Text strings GBADoom") || t.startsWith("DEHACKED lump"))
        return Dehacked;

    for(const QString& d : dehNames)
    {
        if(t.startsWith(d + " line ") || t.startsWith(d + ":"))
            return Dehacked;
    }

    if(t.startsWith("GUSBANK") || t.startsWith("Instrument ") || t.contains(": not an instrument") || (t.startsWith("Can't open ") && t.endsWith(".cfg")))
        return Music;

    //The size report and its "  Maps: 123 bytes (4.5%)." lines.
    static const QRegularExpression spacePartRe("^  [A-Z][a-z ]+: \\d+ bytes \\(");

    if(t.startsWith("Wrote ") || t.startsWith("Replaced the WAD") || t.startsWith("Space used:") || spacePartRe.match(t).hasMatch() || t.contains("IWAD header not found") || t.startsWith("Can't open ROM") ||
       t.startsWith("Can't write") || t.startsWith("ROM is ") || t.startsWith("-rom and -romout"))
        return Write;

    //Everything ProcessWad does after DeHackEd: maps, textures, pictures, sounds, music lumps.
    return Convert;
}

void BuildPage::AddLine(Level level, const QString& text, int lineStage)
{
    //The command line itself belongs to no stage's progress.
    if(!lines.isEmpty())
        stage = qMax(stage, lineStage);

    if(level == Level::Warn)
        warnings++;

    lines.append({level, text, lineStage});
    AppendLog(lines.last());

    warnButton->setText(QString("Warnings · %1").arg(warnings));

    UpdateStages();
}

void BuildPage::Finished(int code, QProcess::ExitStatus status)
{
    if(!running)
        return;

    ReadOutput(true);

    running = false;
    ticker->stop();
    succeeded = !stopped && status == QProcess::NormalExit && code == 0;

    const double secs = timer.elapsed() / 1000.0;

    if(succeeded)
    {
        AddLine(Level::Done, warnings ? QString("Finished with %1 %2 in %3 s").arg(warnings).arg(warnings == 1 ? "warning" : "warnings").arg(secs, 0, 'f', 1)
                                      : QString("Finished in %1 s").arg(secs, 0, 'f', 1), Write);
    }
    else
    {
        if(failure.isEmpty())
        {
            if(stopped)
                failure = "Stopped.";
            else if(status == QProcess::CrashExit)
                failure = "GbaWadUtil crashed.";
            else
            {
                //The first error is the cause; later ones ("Processing x failed") follow from it.
                for(const Line& l : lines)
                {
                    if(l.level == Level::Error && failure.isEmpty())
                        failure = l.text;
                }

                if(failure.isEmpty())
                    failure = QString("GbaWadUtil stopped with exit code %1.").arg(code);
            }
        }

        AddLine(Level::Fail, stopped ? "Stopped" : "Failed: " + failure, stage);
    }

    UpdateStages();
    UpdatePanel();
    ShowOutputs();
    ShowWarnings();
}

void BuildPage::UpdateStages()
{
    int stageWarnings[NumStages] = {};
    int wads = 0;
    int patches = 0;
    int skipped = 0;
    QString gus;

    for(int i = 1; i < lines.size(); i++)
    {
        const Line& l = lines[i];

        if(l.level == Level::Warn)
            stageWarnings[l.stage]++;

        if(l.text.startsWith("Loaded ") || l.text.startsWith("Merged "))
            wads++;

        if(l.text.startsWith("DeHackEd ") && l.level == Level::Ok)
            patches++;

        if(l.text.startsWith("  Skipped (not supported)"))
        {
            static const QRegularExpression countRe(" x(\\d+)\\.$");
            const QRegularExpressionMatch m = countRe.match(l.text);
            skipped += m.hasMatch() ? m.captured(1).toInt() : 1;
        }

        if(l.text.startsWith("GUSBANK"))
        {
            static const QRegularExpression overRe("oversampled (\\d+)x");
            const QRegularExpressionMatch m = overRe.match(l.text);
            gus = "GUSBANK at " + (m.hasMatch() ? m.captured(1) : QString::number(oversample)) + "×";
        }
    }

    const bool finished = !running && !lines.isEmpty();
    const bool failed = finished && !succeeded;

    for(int i = 0; i < NumStages; i++)
    {
        QString note;

        switch(i)
        {
            case Merge: note = wads ? QString("%1 %2").arg(wads).arg(wads == 1 ? "file" : "files") : "Waiting"; break;
            case Dehacked: note = skipped ? QString("%1 %2 skipped").arg(skipped).arg(skipped == 1 ? "block" : "blocks") :
                                  (patches ? QString("%1 %2").arg(patches).arg(patches == 1 ? "patch" : "patches") : "No patches"); break;
            case Convert: note = "Maps, textures, sounds"; break;
            case Music: note = gus.isEmpty() ? "Instruments" : gus; break;

            case Write:
            {
                QStringList kinds;

                for(const BuildOutput& o : outputs)
                    kinds.append(o.kind == BuildOutput::Rom ? "ROM" : (o.kind == BuildOutput::Wad ? "WAD" : "C file"));

                note = kinds.join(", ");
                break;
            }
        }

        StageCard::State state;

        if(i == Music && !oversample)
        {
            state = StageCard::Off;
            note = "Off";
        }
        else if(failed && i == stage)
        {
            state = StageCard::Fail;
            note = stopped ? "Stopped" : "Failed";
        }
        else if(i < stage || (finished && succeeded))
            state = stageWarnings[i] ? StageCard::Warn : StageCard::Ok;
        else if(i == stage && running)
            state = StageCard::Running;
        else
        {
            state = StageCard::Pending;

            if(failed)
                note = "Not run";
        }

        if(state == StageCard::Warn && i != Dehacked)
            note = QString("%1 %2").arg(stageWarnings[i]).arg(stageWarnings[i] == 1 ? "warning" : "warnings");

        stages[i]->Set(state, note);
    }
}

void BuildPage::UpdatePanel()
{
    const double secs = timer.elapsed() / 1000.0;

    if(running)
    {
        static const char* const stageNames[NumStages] = {"Merging WADs", "Applying DeHackEd", "Converting", "Packing instruments", "Writing outputs"};

        doneIcon->setPixmap(Badge("dot", QColor(0x27254A), AccentText, 36));
        doneTitle->setText("Building…");
        doneSubtitle->setText(QString("%1 · %2 s").arg(stageNames[stage]).arg(int(secs)));
        doneSubtitle->setStyleSheet(QString());
    }
    else if(succeeded)
    {
        doneIcon->setPixmap(Badge("check", OkGround, Ok, 36));
        doneTitle->setText("Build finished");
        doneSubtitle->setText(warnings ? QString("With %1 %2").arg(warnings).arg(warnings == 1 ? "warning" : "warnings") : "No warnings");
        doneSubtitle->setStyleSheet(warnings ? QString("color: %1").arg(Warn.name()) : QString());
    }
    else
    {
        doneIcon->setPixmap(Badge("close", ErrorGround, Error, 36));
        doneTitle->setText(stopped ? "Build stopped" : "Build failed");
        doneSubtitle->setText(failure);
        doneSubtitle->setStyleSheet(QString("color: %1").arg(Error.name()));
    }

    backButton->setEnabled(!running);
    changeButton->setEnabled(!running);
    againButton->setVisible(!running);
    stopButton->setVisible(running);
    saveLogButton->setEnabled(!running);
    outputsBox->setVisible(succeeded);
    spaceBox->setVisible(succeeded && hasSpace);
}

void BuildPage::ShowOutputs()
{
    QLayout* ov = outputsBox->layout();
    ClearLayout(ov);

    //The build's own size report.
    SpaceReport space;

    for(const Line& l : lines)
        ParseSpaceLine(l.text, space);

    hasSpace = succeeded && !space.parts.isEmpty();
    spaceBox->setVisible(hasSpace);

    if(hasSpace)
        spaceChart->Set(space.parts, space.rom ? 32 * 1024 * 1024 : 0);

    if(!succeeded)
        return;

    for(int i = 0; i < outputs.size(); i++)
    {
        const BuildOutput& o = outputs[i];
        const QFileInfo fi(o.path);

        QFrame* card = new QFrame;
        card->setObjectName("outCard");
        card->setProperty("on", o.kind == BuildOutput::Rom);

        QVBoxLayout* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 14, 14, 14);
        v->setSpacing(12);

        QHBoxLayout* top = new QHBoxLayout;
        top->setSpacing(12);
        QLabel* icon = new QLabel;
        icon->setPixmap(Pixmap(o.kind == BuildOutput::Rom ? "cart" : (o.kind == BuildOutput::Wad ? "file" : "code"), AccentText, 20, 1.75, 2));
        top->addWidget(icon);

        QVBoxLayout* names = new QVBoxLayout;
        names->setSpacing(0);
        QLabel* name = Label(fi.fileName(), "mono");
        name->setToolTip(QDir::toNativeSeparators(o.path));
        name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        names->addWidget(name);

        QString size = Megabytes(fi.size());

        if(o.kind == BuildOutput::Rom)
            size += " of 32 MB";

        names->addWidget(Label(OutputName(o.kind) + " · " + size, "dim"));
        top->addLayout(names, 1);
        v->addLayout(top);

        if(o.kind == BuildOutput::Rom)
        {
            SizeBar* bar = new SizeBar;
            const double f = fi.size() / (32.0 * 1024 * 1024);
            bar->Set(f, f > 1 ? ErrorBar : (f > 0.875 ? Warn : Bar));
            v->addWidget(bar);
        }

        QHBoxLayout* buttons = new QHBoxLayout;
        buttons->setSpacing(8);

        QPushButton* show = Button("Show in folder", o.kind == BuildOutput::Rom ? "primarySmall" : "small");
        show->setIcon(Icon("folder", o.kind == BuildOutput::Rom ? QColor(Qt::white) : Text, 15));
        connect(show, &QPushButton::clicked, this, [o]() { ShowInFolder(o.path); });
        buttons->addWidget(show);

        if(o.kind == BuildOutput::Rom)
        {
            QPushButton* run = Button("Open in emulator", "small");
            run->setToolTip("Opens the ROM with the program Windows uses for .gba files");
            connect(run, &QPushButton::clicked, this, [this, o]()
            {
                if(!QDesktopServices::openUrl(QUrl::fromLocalFile(o.path)))
                    QMessageBox::information(this, "Open in emulator", "No program is set to open .gba files. Pick your emulator for them with Open with in Explorer, or open the ROM from the emulator.");
            });
            buttons->addWidget(run);
        }

        buttons->addStretch(1);
        v->addLayout(buttons);

        ov->addWidget(card);
    }
}

void BuildPage::ShowWarnings()
{
    ClearLayout(lookList);

    QStringList errors;
    QStringList items;
    QString source;
    QStringList skipped;
    int skippedCount = 0;

    //Skipped blocks are listed under their patch's summary line: one item per patch.
    auto addSkipped = [&]()
    {
        if(skippedCount)
            items.append(QString("%1: %2 %3 skipped. Only vanilla DeHackEd blocks are applied.")
                         .arg(source, JoinAnd(skipped), skippedCount == 1 ? "block is" : "blocks are"));

        skipped.clear();
        skippedCount = 0;
    };

    static const QRegularExpression skippedRe("Skipped \\(not supported\\): (.+) x(\\d+)\\.$");

    for(const Line& l : lines)
    {
        const QRegularExpressionMatch m = skippedRe.match(l.text);

        if(m.hasMatch())
        {
            const int n = m.captured(2).toInt();
            skipped.append(n == 1 ? m.captured(1) : QString("%1 ×%2").arg(m.captured(1)).arg(n));
            skippedCount += n;
            continue;
        }

        addSkipped();

        if(l.text.startsWith("DeHackEd "))
            source = l.text.mid(9).section(": ", 0, 0);

        QStringList& list = l.level == Level::Error ? errors : items;
        const QString text = l.text.trimmed();

        if((l.level == Level::Warn || l.level == Level::Error) && !list.contains(text))
            list.append(text);
    }

    addSkipped();

    //Errors first: the build stopped on them.
    const int errorCount = int(errors.size());
    items = errors + items;

    const int shown = qMin(int(items.size()), qMax(6, errorCount));

    for(int i = 0; i < shown; i++)
    {
        StatusLine* s = new StatusLine(nullptr, 13);
        s->Set(i < errorCount ? "error" : "warn", i < errorCount ? Error : TextSoft, items[i]);
        lookList->addWidget(s);
    }

    if(items.size() > shown)
        lookList->addWidget(Label(QString("And %1 more in the log.").arg(items.size() - shown), "faint"));

    lookBox->setVisible(!items.isEmpty() && !running);
}

void BuildPage::RenderLog()
{
    log->clear();

    for(const Line& l : lines)
        AppendLog(l);
}

void BuildPage::AppendLog(const Line& l)
{
    if(warnOnly && l.level != Level::Warn && l.level != Level::Error)
        return;

    static const struct { const char* tag; QColor tagColor; QColor textColor; QColor ground; } looks[] =
    {
        {"ok", Ok, TextSoft, Qt::transparent},
        {"info", AccentText, TextSoft, Qt::transparent},
        {"warn", Warn, WarnText, WarnRow},
        {"error", Error, QColor(0xF5C2BA), QColor(0x2A1714)},
        {"done", Text, Text, Qt::transparent},
        {"fail", Error, Error, Qt::transparent},
    };

    const auto& look = looks[int(l.level)];

    QTextCursor c(log->document());
    c.movePosition(QTextCursor::End);

    if(!log->document()->isEmpty())
        c.insertBlock();

    QTextBlockFormat block;
    block.setBackground(look.ground);
    block.setLineHeight(150, QTextBlockFormat::ProportionalHeight);
    c.setBlockFormat(block);

    QTextCharFormat tag;
    tag.setForeground(look.tagColor);
    tag.setFontWeight(QFont::DemiBold);
    c.insertText(QString(look.tag).leftJustified(6, ' '), tag);

    QTextCharFormat text;
    text.setForeground(look.textColor);
    c.insertText(l.text, text);

    log->verticalScrollBar()->setValue(log->verticalScrollBar()->maximum());
}

void BuildPage::SaveLog()
{
    const QString file = QFileDialog::getSaveFileName(this, "Save log", QDir::homePath() + "/GbaWadUtil-log.txt", "Text files (*.txt)");

    if(file.isEmpty())
        return;

    static const char* const tags[] = {"ok", "info", "warn", "error", "done", "fail"};

    QString text;

    for(const Line& l : lines)
        text += QString(tags[int(l.level)]).leftJustified(6, ' ') + l.text + "\n";

    QFile f(file);

    if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text) || f.write(text.toUtf8()) < 0)
        QMessageBox::warning(this, "Save log", "Can't write " + QFileInfo(file).fileName() + ".");
}
