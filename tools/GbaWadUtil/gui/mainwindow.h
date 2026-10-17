#ifndef GUI_MAINWINDOW_H
#define GUI_MAINWINDOW_H

#include <QtWidgets>
#include "../options.h"
#include "spacechart.h"

class BuildPage;
class ChoiceCard;
class CheckCard;
class SizeBar;
class StatusLine;
class ToggleSwitch;

//The DeHackEd blocks GbaWadUtil applies, as the window's pills show them.
static const char* const dehBlocks[] = {"Thing", "Frame", "Pointer", "Weapon", "Ammo", "Misc", "Text"};
static const int numDehBlocks = 7;

//What a patch (or a WAD's DEHACKED lumps) does, read before the build.
struct DehScan
{
    //What will be skipped, or why it can't be read.
    QString note;

    //A bit per dehBlocks entry it changes.
    int changes = 0;

    //A WAD's DEHACKED lumps.
    int lumps = 0;

    //Binary or unreadable: as a -deh file, the build stops on it.
    bool unusable = false;
};

//What the window builds, with absolute paths.
struct GuiSettings
{
    QString iwad;
    QStringList pwads;
    QStringList dehs;
    bool music = true;
    QString gusCfg;
    int oversample = 4;
    bool rom = true;
    bool wad = false;
    bool cfile = false;
    QString engine;
    QString romOut;
    QString wadOut;
    QString cOut;
};

//The build settings page and, while and after a build runs, its result page.
//Builds run GbaWadUtil itself as a child process, with the arguments shown
//under Command line, from GbaWadUtil's folder.
class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(const QStringList& args);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    void closeEvent(QCloseEvent* e) override;

private:
    QWidget* BuildHeader();
    QWidget* BuildSources();
    QWidget* BuildDehacked();
    QWidget* BuildMusic();
    QWidget* BuildOutputs();
    QWidget* BuildSpace();
    QWidget* BuildCommand();
    QWidget* BuildFooter();

    QLineEdit* PathEdit(QString* field, bool* edited, const QString& placeholder = QString());
    QPushButton* BrowseButton(const QString& tip);
    QWidget* OutputCard(CheckCard* header, QWidget* body, QFrame** frame);

    void FillFromArguments(const QStringList& args);
    void RebuildPwadRows();
    void RebuildDehRows();
    void Refresh();
    void SuggestNames();
    QString InputProblem() const;
    QString Blocker() const;

    //The space the WAD takes, from GbaWadUtil -size in the background.
    void UpdateMeasurement();
    void StartMeasurement();
    void StopMeasurement();
    WadUtilOptions Options() const;

    void AddFiles(const QStringList& files);
    void AddPwads(const QStringList& files);
    void AddDehs(const QStringList& files);
    void SetGusCfg(const QString& file);
    void BrowseIwad();
    void BrowseEngine();
    void BrowseSave(QString* field, bool* edited, const QString& title, const QString& filter);
    void OpenPreset();
    void SavePreset();
    void StartBuild();

    //Paths as typed and shown are relative to GbaWadUtil's folder, where builds run.
    QString Abs(const QString& path) const;
    QString Shown(const QString& path) const;
    QString StartDir() const;

    QString exeDir;
    QString lastDir;
    GuiSettings s;
    bool romOutEdited = false;
    bool wadOutEdited = false;
    bool cOutEdited = false;
    bool refreshing = false;

    //DeHackEd patches by path, read when they are added.
    QHash<QString, DehScan> dehScans;
    QLabel* dehAutoText;
    QList<QLabel*> dehChips;
    QString customCfg;

    QStackedWidget* stack;
    QWidget* settingsPage;
    BuildPage* buildPage;

    QList<QPair<QLineEdit*, QString*>> pathEdits;

    QFrame* banner;
    QLabel* bannerIgnored;

    QLineEdit* iwadEdit;
    QLabel* iwadTag;
    QLabel* iwadRowName;
    QWidget* pwadRows;
    QLabel* gbaWadOrder;

    QWidget* dehRows;

    ToggleSwitch* musicSwitch;
    QLabel* musicOff;
    QWidget* musicOn;
    QList<ChoiceCard*> cfgCards;
    QStringList cfgFiles;
    QButtonGroup* overGroup;
    QLabel* overText;

    QFrame* romFrame;
    QFrame* wadFrame;
    QFrame* cFrame;
    CheckCard* romCheck;
    CheckCard* wadCheck;
    CheckCard* cCheck;
    QWidget* romBody;
    QWidget* wadBody;
    QWidget* cBody;
    QLineEdit* engineEdit;
    StatusLine* engineStatus;
    QLabel* romSizeLabel;
    QLabel* romSize;
    SizeBar* romBar;
    StatusLine* overLimit;
    QWidget* overLimitButtons;
    QPushButton* useOneX;

    struct Measurement
    {
        bool ok = false;
        SpaceReport report;
        QString error;
    };

    //By measureKey: the -size arguments and the inputs' sizes and times.
    QHash<QString, Measurement> measurements;
    QString measureKey;
    QStringList measureArgs;
    QProcess* measureProcess = nullptr;
    QTimer* measureTimer;
    SpaceReport lastSpace;

    QLabel* spaceState;
    SpaceChart* spaceChart;
    QLabel* spaceNote;
    StatusLine* spaceError;

    QTextEdit* cmdView;
    QPushButton* copyButton;
    QString cmdFlat;

    QPushButton* buildButton;
    StatusLine* blockerLine;
};

#endif // GUI_MAINWINDOW_H
