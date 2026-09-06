#ifndef MAINDISPLAY_H
#define MAINDISPLAY_H

#include <QWidget>
#include <QList>

#include "gamestructs.h"

namespace Ui {
class MainDisplay;
}

class MainDisplay : public QWidget
{
    Q_OBJECT

public:
    explicit MainDisplay(QWidget *parent = nullptr);

    /**
     * @brief setTitle Sets the title, this is never set automatically so
     * it has its own function call.
     * @param title The text to be shown as the title.
     */
    void setTitle(const QString &title);

    /**
     * @brief setHeadToHeadVisible Shows or hides the line under the title.
     * Hiding it shrinks the overlay back to the single row, so the space is
     * given back rather than left blank.
     * @param visible True to show the line.
     */
    void setHeadToHeadVisible(bool visible);
    ~MainDisplay();

public slots:
    /**
     * @brief update A slot to send updated information to be displayed
     * @param displayData The data that will be displayed
     */
    void update(const std::array<displayPlayerInfoStruct, 2> &displayData);

signals:
    void textUpdated();

private:
    Ui::MainDisplay *ui;

    /**
     * @brief updateHeadToHead Fills the centred line below the main row with
     * the two player names, their leaderboard ranks and their head to head
     * record. Missing values are shown as placeholders.
     * @param displayData The same data the main row is built from.
     */
    void updateHeadToHead(const std::array<displayPlayerInfoStruct, 2> &displayData);

    /**
     * @brief updateTurnOrderIcons Puts the attack icon on the side which moves
     * first and the shield on the other. Both keep the shield while the attack
     * artwork is not present in the resources.
     * @param displayData The same data the main row is built from.
     */
    void updateTurnOrderIcons(const std::array<displayPlayerInfoStruct, 2> &displayData);

    /**
     * @brief applyFactionSkin Tints the overlay frame to the local player's
     * town, so the bar carries the colours of the faction being played.
     * @param displayData The same data the main row is built from.
     */
    void applyFactionSkin(const std::array<displayPlayerInfoStruct, 2> &displayData);

    /** The frame colour currently applied, so the style sheet is only rebuilt
     *  when the town actually changes. */
    QString frameAccent;

    /**
     * @brief mousePressEvent Registers when LMB has been pressed to be used
     * for dragging the window.
     * @param event The mouse event.
     */
    void mousePressEvent(QMouseEvent *event);

    /**
     * @brief mouseReleaseEvent Registers when LMB has been released to be used
     * for stop dragging the window.
     * @param event The mouse event.
     */
    void mouseReleaseEvent(QMouseEvent *event);

    /**
     * @brief mouseMoveEvent Moves the window if the LMB is hold while on top
     * of the window
     * @param event The mouse event
     */
    void mouseMoveEvent(QMouseEvent *event);


    /** * * * * * * * * * *
     *  Member variables  *
     ** * * * * * * * * * */

    QPointF dragingOldWindowPos;
    bool holdingLeft;
};

#endif // MAINDISPLAY_H
