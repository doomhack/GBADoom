#ifndef GUI_BUILDPAGE_H
#define GUI_BUILDPAGE_H

#include <QtWidgets>
#include <functional>

class StageCard;
class SpaceChart;

struct BuildOutput
{
    enum Kind { Rom, Wad };

    Kind kind;
    QString path;
};

//Runs a build as a GbaWadUtil child process and shows its stages, its log
//and what it wrote.
class BuildPage : public QWidget
{
public:
    explicit BuildPage(QWidget* parent = nullptr);

    //oversample 0: no music.
    void Start(const QString& program, const QStringList& args, const QString& workDir, const QList<BuildOutput>& outputs, int oversample);
    void Stop();
    bool Running() const;

    std::function<void()> onBack;
    std::function<void()> onBuildAgain;

private:
    enum Stage { Merge, Dehacked, Convert, Music, Write, NumStages };
    enum class Level { Ok, Info, Warn, Error, Done, Fail };

    struct Line
    {
        Level level;
        QString text;
        int stage;
    };

    void ReadOutput(bool flush);
    void AddLine(Level level, const QString& text, int stage);
    void Finished(int code, QProcess::ExitStatus status);
    int StageOf(const QString& text) const;
    void UpdateStages();
    void UpdatePanel();
    void ShowOutputs();
    void ShowWarnings();
    void RenderLog();
    void AppendLog(const Line& line);
    void SaveLog();

    QProcess* process = nullptr;
    QByteArray pending;
    Level pendingLevel = Level::Info;
    QElapsedTimer timer;
    QTimer* ticker;

    QList<Line> lines;
    QList<BuildOutput> outputs;
    QStringList dehNames;
    int oversample = 0;
    int warnings = 0;
    int stage = Merge;
    bool running = false;
    bool succeeded = false;
    bool stopped = false;
    QString failure;
    bool warnOnly = false;
    bool hasSpace = false;

    StageCard* stages[NumStages];

    QPushButton* backButton;
    QPushButton* allButton;
    QPushButton* warnButton;
    QPlainTextEdit* log;

    QLabel* doneIcon;
    QLabel* doneTitle;
    QLabel* doneSubtitle;
    QWidget* outputsBox;
    QWidget* spaceBox;
    SpaceChart* spaceChart;
    QWidget* lookBox;
    QVBoxLayout* lookList;

    QPushButton* changeButton;
    QPushButton* againButton;
    QPushButton* stopButton;
    QPushButton* saveLogButton;
};

#endif // GUI_BUILDPAGE_H
