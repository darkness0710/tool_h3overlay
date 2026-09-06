#include "mainwindow.h"

#include <QStatusBar>
#include "ui_mainwindow.h"

#include <iostream>
#include <algorithm>

#include <QPalette>
#include <QString>
#include <QFile>

static AboutWindow *globalAboutWindow = {nullptr};
static bool debugActivated = {false};

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(context)

    switch (type) {
    case QtDebugMsg:
        if (debugActivated)
        {
            if (globalAboutWindow != nullptr)
                globalAboutWindow->addToLog("Debug: " + msg);
            std::cout << "Debug: " << msg.toStdString() << std::endl;
        }
        break;
    case QtInfoMsg:
        if (globalAboutWindow != nullptr)
            globalAboutWindow->addToLog("Info: " + msg);
        std::cout << "Info: " << msg.toStdString() << std::endl;
        break;
    case QtWarningMsg:
        if (globalAboutWindow != nullptr)
            globalAboutWindow->addToLog("Warning: " + msg);
        std::cerr << "Warning: " << msg.toStdString() << std::endl;
        break;
    case QtCriticalMsg:
        if (globalAboutWindow != nullptr)
            globalAboutWindow->addToLog("Critical: " + msg);
        std::cerr << "Critical: " << msg.toStdString() << std::endl;
        break;
    case QtFatalMsg:
        if (globalAboutWindow != nullptr)
            globalAboutWindow->addToLog("Fatal: " + msg);
        std::cerr << "Fatal: " << msg.toStdString() << std::endl;
        break;
    }
    std::cout << std::flush;
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      storedSettings(new Settings(this)),
      scanner(new MemoryScanner(storedSettings, this)),
      about(new AboutWindow(this)),
      settingsWindow(new SettingsWindow(storedSettings, this)),
      stats(new StatsWindow(this)),
      mainDisplay(new MainDisplay()),
      matches(new StatisticsDatabase(this)),
      hotaMeta(new HotaMetaClient(this))
{
    ui->setupUi(this);

    qInstallMessageHandler(myMessageOutput);
    globalAboutWindow = this->about;

    removeManifest();

    this->stats->setModel(this->matches->getModel());
    this->scanner->init();

    setupTextColors();
    populateComboboxes();

    // Set before the signals are connected: restoring the stored choice must
    // not look like the user just toggled it.
    this->ui->todayLineBox->setChecked(this->storedSettings->getDisplayTodayLine());

    connectSignals();

}


MainWindow::~MainWindow()
{
    globalAboutWindow = nullptr;
    // mainDisplay is not set as child so we have to delete it manually
    delete mainDisplay;
    delete ui;
}

void MainWindow::removeManifest()
{
    QFile manifest(QCoreApplication::applicationDirPath() + "/h3overlay.exe.manifest");
    if (manifest.exists())
    {
        manifest.remove();
    }
}

void MainWindow::init()
{
    mainDisplay->show();
    // The scanner and the settings both emit their first update from their
    // constructors, before connectSignals() has run, so those are lost. Without
    // this the display keeps whatever the .ui file happened to specify until
    // the game is started and the first real update arrives, which loses the
    // stored show/hide choice for the line under the title.
    sendUpdate();
}


void MainWindow::setInitFontColor(QWidget * widget)
{
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Text, "#E0CE75");
    palette.setColor(QPalette::PlaceholderText, "#897D48");
    widget->setPalette(palette);
}

void MainWindow::setupTextColors()
{
    this->autoData.player[0].nameColor = "#E0CE75";
    this->autoData.player[1].nameColor = "#E0CE75";

    setInitFontColor(this->ui->redNameEdit);
    setInitFontColor(this->ui->redRatingEdit);
    setInitFontColor(this->ui->redTradeEdit);
    setInitFontColor(this->ui->redWinEdit);

    setInitFontColor(this->ui->blueNameEdit);
    setInitFontColor(this->ui->blueRatingEdit);
    setInitFontColor(this->ui->blueTradeEdit);
    setInitFontColor(this->ui->blueWinEdit);

    setInitFontColor(this->ui->titleEdit);
}

void MainWindow::populateComboboxes()
{
    for (QString town: townMap)
    {
        QIcon icon(":/images/towns/" + town);
        ui->redTownBox->addItem(icon, town);
        ui->blueTownBox->addItem(icon, town);
    }

    std::array heroes = heroMap;
    std::sort(heroes.begin(),
              heroes.end(),
              [](char const *lhs, char const *rhs){return strcmp(lhs,rhs) < 0;});

    for (QString hero: heroes)
    {
        QIcon icon(":/images/heroes/Hero_" + hero);
        ui->redHeroBox->addItem(icon, hero);
        ui->blueHeroBox->addItem(icon, hero);
    }
}

void MainWindow::showLayoutWarning(const bool suspect, const QString what)
{
    if(suspect)
    {
        // No timeout: this does not fix itself, the build has to be updated.
        statusBar()->showMessage(tr("Game memory does not look like %1. "
                                    "Heroes 3 was probably updated and this "
                                    "version of the overlay needs new offsets.")
                                 .arg(what));
    }
    else
    {
        statusBar()->clearMessage();
    }
}

void MainWindow::connectSignals()
{
    connect(this->scanner, &MemoryScanner::playerUpdated, this, &MainWindow::getUpdate);
    connect(this->storedSettings, &Settings::settingsUpdated, this, &MainWindow::sendUpdate);
    connect(this->hotaMeta, &HotaMetaClient::dataUpdated, this, &MainWindow::sendUpdate);
    connect(this->about, &AboutWindow::activateDebugMessages, this, &MainWindow::updateDebugStatus);
    connect(this->scanner, &MemoryScanner::memoryLayoutSuspect, this, &MainWindow::showLayoutWarning);

    connect(this->ui->todayLineBox, &QCheckBox::toggled, this, [this](bool checked)
    {
        this->storedSettings->enableTodayLine(checked);
    });

    connect(this->ui->swapButton, &QPushButton::clicked, this, &MainWindow::swapFields);
    connect(this->ui->actionSettings, &QAction::triggered, this->settingsWindow, &SettingsWindow::show);
    connect(this->ui->actionAbout, &QAction::triggered, this->about, &AboutWindow::show);
    connect(this->ui->actionStats, &QAction::triggered, this->stats, &StatsWindow::show);
    connect(this->ui->actionOpen_display, &QAction::triggered, this->mainDisplay, &MainDisplay::show);
    connect(this->ui->actionExit, &QAction::triggered, this, &MainWindow::exitApplication);


    auto triggerUpdate = [=, this](QString arg){Q_UNUSED(arg); emit sendUpdate();};
    connect(this->ui->titleEdit, &QLineEdit::textChanged,         triggerUpdate);

    connect(this->ui->redNameEdit, &QLineEdit::textChanged,       triggerUpdate);
    connect(this->ui->redRatingEdit, &QLineEdit::textChanged,     triggerUpdate);
    connect(this->ui->redTradeEdit, &QLineEdit::textChanged,      triggerUpdate);
    connect(this->ui->redWinEdit, &QLineEdit::textChanged,        triggerUpdate);
    connect(this->ui->redHeroBox, &QComboBox::currentTextChanged, triggerUpdate);
    connect(this->ui->redTownBox, &QComboBox::currentTextChanged, triggerUpdate);

    connect(this->ui->blueNameEdit, &QLineEdit::textChanged,       triggerUpdate);
    connect(this->ui->blueRatingEdit, &QLineEdit::textChanged,     triggerUpdate);
    connect(this->ui->blueTradeEdit, &QLineEdit::textChanged,      triggerUpdate);
    connect(this->ui->blueWinEdit, &QLineEdit::textChanged,        triggerUpdate);
    connect(this->ui->blueHeroBox, &QComboBox::currentTextChanged, triggerUpdate);
    connect(this->ui->blueTownBox, &QComboBox::currentTextChanged, triggerUpdate);
}

void MainWindow::exitApplication()
{
    QApplication::exit(0);
}

void swapEdit(QLineEdit * edit1, QLineEdit * edit2)
{
    if (edit1 == nullptr || edit2 == nullptr)
    {
        return;
    }
    edit1->blockSignals(true);
    edit2->blockSignals(true);
    QString tmp = edit1->text();
    edit1->setText(edit2->text());
    edit2->setText(tmp);
    edit1->blockSignals(false);
    edit2->blockSignals(false);
}

void swapBox(QComboBox * box1, QComboBox * box2)
{
    if (box1 == nullptr || box2 == nullptr)
    {
        return;
    }
    box1->blockSignals(true);
    box2->blockSignals(true);
    int tmp = box1->currentIndex();
    box1->setCurrentIndex(box2->currentIndex());
    box2->setCurrentIndex(tmp);
    box1->blockSignals(false);
    box2->blockSignals(false);
}

void MainWindow::swapFields()
{
    swapEdit(this->ui->redNameEdit, this->ui->blueNameEdit);
    swapEdit(this->ui->redRatingEdit, this->ui->blueRatingEdit);
    swapEdit(this->ui->redTradeEdit, this->ui->blueTradeEdit);
    swapEdit(this->ui->redWinEdit, this->ui->blueWinEdit);

    swapBox(this->ui->redTownBox, this->ui->blueTownBox);
    swapBox(this->ui->redHeroBox, this->ui->blueHeroBox);
    sendUpdate();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    this->mainDisplay->close();
    event->accept();
}

void MainWindow::updateDebugStatus(const bool activated)
{
    debugActivated = activated;
}

void MainWindow::getUpdate(const displayInfoStruct &automaticData)
{
    this->autoData = automaticData;
    const bool matchJustFinished = this->autoData.newMatchToRegister;
    if(matchJustFinished)
    {
        matches->addMatchResult(this->autoData);
        this->autoData.newMatchToRegister = false;
    }

    // The client itself ignores a pair it has already fetched, so this is
    // cheap even though this slot runs ten times a second.
    const size_t localIndex = this->autoData.player[Right].isLocalPlayer ? Right : Left;
    const size_t remoteIndex = (localIndex == Left) ? Right : Left;
    this->hotaMeta->setPlayers(this->autoData.player[localIndex].name,
                               this->autoData.player[remoteIndex].name);

    // A finished match moves every online value: both ratings, the head to head
    // score and today's record. Re-read them all rather than leave the overlay
    // showing the numbers from before the game.
    if(matchJustFinished)
    {
        this->hotaMeta->refresh();
    }

    sendUpdate();
}

QString setText(QLineEdit * lineEdit, QString autoData)
{
    if (lineEdit == nullptr)
    {
        return QString();
    }

    if(lineEdit->text().isEmpty() == false)
    {
        return lineEdit->text();
    }

    if(autoData.isEmpty() == false)
    {
        lineEdit->setPlaceholderText(autoData);
        return autoData;
    }

    lineEdit->setPlaceholderText("Auto");
    return QString();
}


QString setText(QComboBox * comboBox, QString autoData)
{
    if (comboBox == nullptr)
    {
        return QString();
    }

    // Index 0 == "auto"
    if(comboBox->currentIndex() == 0)
    {
        return autoData;
    }
    return comboBox->currentText();
}

void MainWindow::sendUpdate()
{
    std::array<displayPlayerInfoStruct, 2> dataToSend;

    displayPlayerInfoStruct *leftInput = &this->autoData.player[0];
    displayPlayerInfoStruct *rightInput = &this->autoData.player[1];

    if(leftInput->playerNumer > rightInput->playerNumer)
    {
        leftInput = &this->autoData.player[1];
        rightInput = &this->autoData.player[0];
    }

    if(leftInput->playerColor.isEmpty())
    {
        leftInput->playerColor = "Red";
    }
    if(rightInput->playerColor.isEmpty())
    {
        rightInput->playerColor = "Blue";
    }


    if(leftInput->hero.isEmpty() == false)
    {
        this->ui->redHeroBox->setItemIcon(0, QIcon(":/images/heroes/Hero_" + leftInput->hero));
    }
    if(rightInput->hero.isEmpty() == false)
    {
        this->ui->blueHeroBox->setItemIcon(0, QIcon(":/images/heroes/Hero_" + rightInput->hero));
    }
    if(leftInput->town.isEmpty() == false)
    {
        this->ui->redTownBox->setItemIcon(0, QIcon(":/images/towns/" + leftInput->town));
    }
    if(rightInput->town.isEmpty() == false)
    {
        this->ui->blueTownBox->setItemIcon(0, QIcon(":/images/towns/" + rightInput->town));
    }

    QString title =    setText(this->ui->titleEdit,autoData.mapName);
    this->mainDisplay->setTitle(title);

    // Cheap to call every time, the display ignores it unless it is a change.
    this->mainDisplay->setHeadToHeadVisible(this->storedSettings->getDisplayTodayLine());


    dataToSend[Left] = *leftInput;
    dataToSend[Right] = *rightInput;

    // The online statistics. The rating from the API is preferred over the one
    // read from game memory: it is the same number, but it is also available
    // for the opponent without having to hover them in the game lobby first.
    // An empty value means the request has not answered, so keep the fallback.
    const hotaMetaStruct &meta = this->hotaMeta->data();
    QString leftRating = leftInput->rating;
    QString rightRating = rightInput->rating;
    const QString &leftMetaRating =
            leftInput->isLocalPlayer ? meta.localRating : meta.opponentRating;
    const QString &rightMetaRating =
            rightInput->isLocalPlayer ? meta.localRating : meta.opponentRating;
    if(leftMetaRating.isEmpty() == false)
    {
        leftRating = leftMetaRating;
    }
    if(rightMetaRating.isEmpty() == false)
    {
        rightRating = rightMetaRating;
    }

    // Same for the gold trade. Reading it from the lobby chat only worked if
    // the overlay happened to be running when the trade was announced, the
    // API reports it for the running match either way.
    QString leftMoney = leftInput->money;
    QString rightMoney = rightInput->money;
    const QString &leftMetaTrade =
            leftInput->isLocalPlayer ? meta.localTrade : meta.opponentTrade;
    const QString &rightMetaTrade =
            rightInput->isLocalPlayer ? meta.localTrade : meta.opponentTrade;
    if(leftMetaTrade.isEmpty() == false)
    {
        leftMoney = leftMetaTrade;
    }
    if(rightMetaTrade.isEmpty() == false)
    {
        rightMoney = rightMetaTrade;
    }

    // Head to head is stored on the local player, whose point of view it is,
    // and so is today's record.
    const size_t localSide = dataToSend[Right].isLocalPlayer ? Right : Left;
    dataToSend[localSide].h2hWins = meta.h2hWins;
    dataToSend[localSide].h2hLosses = meta.h2hLosses;
    dataToSend[localSide].todayWins = meta.todayWins;
    dataToSend[localSide].todayLosses = meta.todayLosses;
    dataToSend[localSide].todayRatingChange = meta.todayRatingChange;

    dataToSend[Left].name =    setText(this->ui->redNameEdit,    leftInput->name);
    dataToSend[Left].rating =  setText(this->ui->redRatingEdit,  leftRating);
    dataToSend[Left].money =   setText(this->ui->redTradeEdit,   leftMoney);
    dataToSend[Left].wins =    setText(this->ui->redWinEdit,     leftInput->wins);
    dataToSend[Left].town =    setText(this->ui->redTownBox,     leftInput->town);
    dataToSend[Left].hero =    setText(this->ui->redHeroBox,     leftInput->hero);

    dataToSend[Right].name =   setText(this->ui->blueNameEdit,   rightInput->name);
    dataToSend[Right].rating = setText(this->ui->blueRatingEdit, rightRating);
    dataToSend[Right].money =  setText(this->ui->blueTradeEdit,  rightMoney);
    dataToSend[Right].wins =   setText(this->ui->blueWinEdit,    rightInput->wins);
    dataToSend[Right].town =   setText(this->ui->blueTownBox,    rightInput->town);
    dataToSend[Right].hero =   setText(this->ui->blueHeroBox,    rightInput->hero);



    mainDisplay->update(dataToSend);
}

void MainWindow::on_redClearButton_clicked()
{
    ui->redNameEdit->setText("");
    ui->redRatingEdit->setText("");
    ui->redTownBox->setCurrentIndex(0);
    ui->redHeroBox->setCurrentIndex(0);
    ui->redTradeEdit->setText("");
    ui->redWinEdit->setText("");
    sendUpdate();
}


void MainWindow::on_blueClearButton_clicked()
{
    ui->blueNameEdit->setText("");
    ui->blueRatingEdit->setText("");
    ui->blueTownBox->setCurrentIndex(0);
    ui->blueHeroBox->setCurrentIndex(0);
    ui->blueTradeEdit->setText("");
    ui->blueWinEdit->setText("");
    sendUpdate();
}

