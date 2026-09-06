#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QDialog>

#include "settings.h"

namespace Ui {
class SettingsWindow;
}

class SettingsWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsWindow(Settings *settings, QWidget *parent = nullptr);

    ~SettingsWindow();

private:
    Ui::SettingsWindow *ui;
    Settings *settings;

private slots:
    /**
     * @brief updateSettingsGui Updates the state of the control settings in
     * the GUI to match the internal settings
     */
    void updateSettingsGui();

    /**
     * @brief coloredCheckUpdated Slot which triggers when the user changes
     * the state of the checkbox regarding tracking online player text color.
     * @param checked If set to true, will enable the player text color
     * tracking, if set to false, disable tracking.
     */
    void coloredCheckUpdated(bool checked);

    /**
     * @brief multiplayerUpdated Slot which triggers when the user changes
     * the state of the checkbox regarding if local multiplayer matches should
     * be tracked.
     * @param checked If set to true, will enable player tracking when
     * the game is a local match (not online).
     */
    void multiplayerUpdated(bool checked);

    /**
     * @brief tavernHeroUpdated Slot which triggers when the user changes
     * the state of the checkbox regarding if hero display should update
     * when opening the tavern screen
     * @param checked If set to true, will enable tracking best hero
     * according to the tavern screen.
     */
    void tavernHeroUpdated(bool checked);

};

#endif // SETTINGSWINDOW_H
