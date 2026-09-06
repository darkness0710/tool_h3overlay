#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCloseEvent>

#include "memoryscanner.h"
#include "maindisplay.h"
#include "aboutwindow.h"
#include "settingswindow.h"
#include "statswindow.h"
#include "statisticsdatabase.h"
#include "settings.h"
#include "hotametaclient.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    void init();
    ~MainWindow();

private:

    void closeEvent(QCloseEvent *event);

    /**
     * @brief removeManifest The manifest file made the executable request
     * admin privilage. This is no longer needed but the file must be removed
     * to stop asking for it.
     */
    void removeManifest();

    /**
     * @brief setupTextColors Sets the text and placeholder text colors
     * if the input fields.
     */
    void setupTextColors();

    /**
     * @brief populateComboboxes Populates the hero and town comboboxes with
     * respective items
     */
    void populateComboboxes();

    /**
     * @brief connectSignals Connects Qt signals.
     */
    void connectSignals();


    /**
     * @brief sendUpdate Uses the automatically gathered data to be sent to the
     * main display. If a GUI input field is not empty, it will override the
     * value sent to the main display matching that field
     */
    void sendUpdate();

    /**
     * @brief setFontColor Sets the font/placeholder color to the input fields
     * @param widget The widget to change the font color for
     */
    void setInitFontColor(QWidget *widget);

    /** * * * * * * * * * *
     *  Member variables  *
     ** * * * * * * * * * */

    Ui::MainWindow *ui;
    Settings *storedSettings;
    MemoryScanner *scanner;
    displayInfoStruct autoData;

    AboutWindow *about;
    SettingsWindow *settingsWindow;
    StatsWindow *stats;
    MainDisplay *mainDisplay;
    StatisticsDatabase *matches;
    HotaMetaClient *hotaMeta;

private slots:
    /**
     * @brief getUpdate Slot to receive display data gathered from the game.
     * @param automaticData The data gathered from the game.
     */
    void getUpdate(const displayInfoStruct &automaticData);

    /**
     * @brief on_redClearButton_clicked Clears all the input fileds in the
     * red area of the GUI.
     */
    void on_redClearButton_clicked();
    /**
     * @brief on_blueClearButton_clicked Clears all the input fileds in the
     * blue area of the GUI.
     */
    void on_blueClearButton_clicked();

    /**
     * @brief updateDebugStatus Slot to set a global variable which
     * enable/disables the debug prints
     * @param activated If true, enables debug messages.
     */
    void updateDebugStatus(const bool activated);

    /**
     * @brief showLayoutWarning Shows a warning in the status bar when the data
     * read from the game no longer matches the structs this build expects,
     * which means the game was updated and this build needs new offsets.
     * @param suspect true if the game memory no longer looks like we expect.
     * @param what The name of the data which did not look valid.
     */
    void showLayoutWarning(const bool suspect, const QString what);

    /**
     * @brief exitApplication Exits the application.
     */
    void exitApplication();

    /**
     * @brief swapFields Swaps all user input fields between the left side and
     * the right side of the main window.
     */
    void swapFields();
};
#endif // MAINWINDOW_H
