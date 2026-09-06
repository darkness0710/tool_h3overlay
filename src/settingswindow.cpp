#include "settingswindow.h"
#include "ui_settingswindow.h"

SettingsWindow::SettingsWindow(Settings *settings, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsWindow),
    settings(settings)
{
    ui->setupUi(this);
    connect(settings,
            &Settings::settingsUpdated,
            this,
            &SettingsWindow::updateSettingsGui);

    connect(this->ui->multiplayerCheckbox,
            &QCheckBox::toggled,
            this,
            &SettingsWindow::multiplayerUpdated);

    connect(this->ui->playerColorBox,
            &QCheckBox::toggled,
            this,
            &SettingsWindow::coloredCheckUpdated);

    connect(this->ui->tavernHeroesBox,
            &QCheckBox::toggled,
            this,
            &SettingsWindow::tavernHeroUpdated);

    updateSettingsGui();
}

SettingsWindow::~SettingsWindow()
{
    delete ui;
}

void SettingsWindow::updateSettingsGui()
{
    this->ui->multiplayerCheckbox->setChecked(this->settings->getDisplayLocalMatch());
    this->ui->playerColorBox->setChecked(this->settings->getDisplayNameColor());
    this->ui->tavernHeroesBox->setChecked(this->settings->getDisplayTavernHeroes());
}

void SettingsWindow::coloredCheckUpdated(bool checked)
{
    settings->enablePlayerNameColor(checked);
}

void SettingsWindow::multiplayerUpdated(bool checked)
{
    settings->enableLocalMultiplayer(checked);
}

void SettingsWindow::tavernHeroUpdated(bool checked)
{
    settings->enableBestTavernHero(checked);
}

