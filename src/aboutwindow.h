#ifndef ABOUTWINDOW_H
#define ABOUTWINDOW_H

#include <QDialog>

namespace Ui {
class AboutWindow;
}

class AboutWindow : public QDialog
{
    Q_OBJECT

public:
    explicit AboutWindow(QWidget *parent = nullptr);
    ~AboutWindow();
    /**
     * @brief addToLog Write text to the log in the about window.
     * @param text The text to show in the log.
     */
    void addToLog(const QString &text);

private slots:
    /**
     * @brief debugClicked Slot to be triggered when clicking the debug
     * checkbox.
     * @param checked the new state of the checkbox.
     */
    void debugClicked(const bool checked);

signals:
    /**
     * @brief activateDebugMessages Signal which matches the state of the
     * debug check box.
     * @param activate True if debug messages should be activated.
     */
    void activateDebugMessages(const bool activate);

private:
    Ui::AboutWindow *ui;
};

#endif // ABOUTWINDOW_H
