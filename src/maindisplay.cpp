#include "maindisplay.h"
#include "ui_maindisplay.h"

#include <QMouseEvent>
#include <QFile>
#include <QStringList>
#include <QDebug>


MainDisplay::MainDisplay(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainDisplay),
    holdingLeft(false)
{
    ui->setupUi(this);
    QWidget::setWindowFlags( Qt::Window|
                             Qt::FramelessWindowHint|
                             Qt::WindowSystemMenuHint|
                             Qt::CustomizeWindowHint);

    this->ui->redFlag->setMaxFontSize(25);
    this->ui->blueFlag->setMaxFontSize(25);

    connect(this, &MainDisplay::textUpdated, this->ui->title,          &ResizeLabel::fitLabelToContainer);
    connect(this, &MainDisplay::textUpdated, this->ui->redMoney,       &ResizeLabel::fitLabelToContainer);
    connect(this, &MainDisplay::textUpdated, this->ui->redPlayerName,  &ResizeLabel::fitLabelToContainer);
    connect(this, &MainDisplay::textUpdated, this->ui->redRating,      &ResizeLabel::fitLabelToContainer);
    connect(this, &MainDisplay::textUpdated, this->ui->redFlag,        &ResizeLabel::fitLabelToContainer);
    connect(this, &MainDisplay::textUpdated, this->ui->blueMoney,      &ResizeLabel::fitLabelToContainer);
    connect(this, &MainDisplay::textUpdated, this->ui->bluePlayerName, &ResizeLabel::fitLabelToContainer);
    connect(this, &MainDisplay::textUpdated, this->ui->blueRating,     &ResizeLabel::fitLabelToContainer);
    connect(this, &MainDisplay::textUpdated, this->ui->blueFlag,       &ResizeLabel::fitLabelToContainer);

}

void MainDisplay::setTitle(const QString &title)
{
    this->ui->title->setText(title);
}

void MainDisplay::setHeadToHeadVisible(bool visible)
{
    // Called from the cyclic update, so do nothing unless this is a change:
    // adjustSize() resizes the window and must not run ten times a second.
    if(visible == this->ui->h2hLine->isVisible())
    {
        return;
    }
    this->ui->h2hLine->setVisible(visible);
    adjustSize();
}

// Shown next to the player name. The player who moves first gets the attack
// icon, the other one keeps the shield.
static const QString SHIELD_ICON = ":/images/main/player_name_icon.png";
static const QString ATTACK_ICON = ":/images/main/attack_icon.png";

// The same green and red the game itself uses for text, so the overlay does
// not introduce a second palette.
static const QString RATING_GAIN_COLOR = "#83E16F";
static const QString RATING_LOSS_COLOR = "#F15D5A";

/**
 * @brief colouredRatingChange Renders today's rating change, green when the
 * player is up for the day and red when down. Zero is left in the normal text
 * colour, since neither reading applies.
 * @param change The rating change as text, as delivered by the API.
 * @return An HTML fragment, or a plain placeholder if there is no value yet.
 */
static QString colouredRatingChange(const QString &change)
{
    bool isNumber = false;
    const int value = change.toInt(&isNumber);
    if(isNumber == false)
    {
        return "?";
    }

    const QString text = (value > 0 ? "+" : "") + QString::number(value);
    if(value == 0)
    {
        return text;
    }
    return QString("<span style=\"color:%1\">%2</span>")
            .arg(value > 0 ? RATING_GAIN_COLOR : RATING_LOSS_COLOR, text);
}

// Used until a town is known, and for anything not found in townMap.
static const QString DEFAULT_ACCENT = "#E0CE75";

void MainDisplay::applyFactionSkin(const std::array<displayPlayerInfoStruct, 2> &displayData)
{
    // The local player's town sets the colour, not the opponent's: this is the
    // streamer's overlay, so it takes their side.
    const size_t localIndex = displayData[Right].isLocalPlayer ? Right : Left;
    const QString &town = displayData[localIndex].town;

    QString accent = DEFAULT_ACCENT;
    for(size_t i = 0; i < townMap.size(); i++)
    {
        if(town == townMap[i])
        {
            accent = townAccentMap[i];
            break;
        }
    }

    // Setting the sheet repaints the whole bar, and this runs ten times a
    // second, so only touch it when the colour actually changes.
    if(accent == this->frameAccent)
    {
        return;
    }
    this->frameAccent = accent;
    this->ui->widget_4->setStyleSheet(
        QString("#widget_4{border: 1px solid %1;}").arg(accent));
}

void MainDisplay::updateTurnOrderIcons(const std::array<displayPlayerInfoStruct, 2> &displayData)
{
    // Turn order in the game follows the player colour: the lower colour moves
    // first, which is the attacking side.
    bool leftAttacks = displayData[Left].playerNumer < displayData[Right].playerNumer;
    bool rightAttacks = displayData[Right].playerNumer < displayData[Left].playerNumer;

    // Equal colours mean there is no match loaded yet, so neither side attacks.
    // Without the artwork present, both sides keep the shield they had.
    if(QFile::exists(ATTACK_ICON) == false)
    {
        leftAttacks = false;
        rightAttacks = false;
    }

    const QString style("border-image:  url(%1) 0 0 0 0 stretch stretch;");
    this->ui->redPlayerNameIcon->setStyleSheet(
        style.arg(leftAttacks ? ATTACK_ICON : SHIELD_ICON));
    this->ui->bluePlayerNameIcon->setStyleSheet(
        style.arg(rightAttacks ? ATTACK_ICON : SHIELD_ICON));
}

void MainDisplay::updateHeadToHead(const std::array<displayPlayerInfoStruct, 2> &displayData)
{
    // Without any player there is no match to describe, so keep the line
    // empty rather than showing a row of placeholders.
    if(displayData[Left].name.isEmpty() && displayData[Right].name.isEmpty())
    {
        this->ui->h2hLine->setText("");
        return;
    }

    // Both halves are the local player's point of view, so the names are not
    // repeated here: they are already in the row directly above.
    const size_t localIndex = displayData[Right].isLocalPlayer ? Right : Left;
    const displayPlayerInfoStruct &local = displayData[localIndex];

    const QChar enDash(0x2013);
    QStringList segments;

    // Having played nothing yet today does not earn a segment. Spelling out
    // that there is no number to show would only take up room, and it is the
    // normal state at the start of every session.
    if(local.todayWins.isEmpty() == false && local.todayLosses.isEmpty() == false &&
            (local.todayWins != "0" || local.todayLosses != "0"))
    {
        segments << QString("[Today] %1W%2%3L (%4)")
                        .arg(local.todayWins,
                             enDash,
                             local.todayLosses,
                             colouredRatingChange(local.todayRatingChange));
    }

    // Never having met, unlike the above, is a fact about this pairing rather
    // than missing data, so it is worth saying out loud.
    if(local.h2hWins.isEmpty() == false && local.h2hLosses.isEmpty() == false)
    {
        if(local.h2hWins == "0" && local.h2hLosses == "0")
        {
            segments << "first meeting";
        }
        else
        {
            segments << QString("H2H %1%2%3").arg(local.h2hWins, enDash, local.h2hLosses);
        }
    }

    // Rich text collapses runs of plain spaces, so the air around the
    // separator has to be non breaking.
    this->ui->h2hLine->setTextFormat(Qt::RichText);
    this->ui->h2hLine->setText(segments.join("&nbsp;&nbsp;&#183;&nbsp;&nbsp;"));
}

MainDisplay::~MainDisplay()
{
    delete ui;
}

void MainDisplay::update( const std::array<displayPlayerInfoStruct, 2> &displayData)
{

    this->ui->redTown->setStyleSheet("QFrame{ border-image:  url(:/images/towns/" +
                                     displayData[Left].town + ".png) 0 0 0 0 stretch stretch;}");
    this->ui->blueTown->setStyleSheet("QFrame{border-image:  url(:/images/towns/" +
                                      displayData[Right].town + ".png) 0 0 0 0 stretch stretch;}");

    this->ui->redHero->setStyleSheet("QFrame{border-image:  url(:/images/heroes/Hero_" +
                                     displayData[Left].hero + ".png) 0 0 0 0 stretch stretch;}");
    this->ui->blueHero->setStyleSheet("QFrame{border-image:  url(:/images/heroes/Hero_" +
                                      displayData[Right].hero + ".png) 0 0 0 0 stretch stretch;}");

    this->ui->redFlag->setStyleSheet("border-image:  url(:/images/main/" +
                                     displayData[Left].playerColor + "_flag.png) 0 0 0 0 stretch stretch;  color:#F9E687");
    this->ui->blueFlag->setStyleSheet("border-image:  url(:/images/main/" +
                                      displayData[Right].playerColor + "_flag.png) 0 0 0 0 stretch stretch;  color:#F9E687");


    this->ui->redMoney->setText(displayData[Left].money);
    this->ui->blueMoney->setText(displayData[Right].money);

    this->ui->redPlayerName->setText(displayData[Left].name);
    this->ui->bluePlayerName->setText(displayData[Right].name);
    this->ui->redRating->setText(displayData[Left].rating);
    this->ui->blueRating->setText(displayData[Right].rating);

    this->ui->redFlag->setText(displayData[Left].wins);
    this->ui->blueFlag->setText(displayData[Right].wins);

    QPalette palette;
    palette.setColor(QPalette::WindowText, QColor(displayData[Left].nameColor));
    this->ui->redPlayerName->setPalette(palette);
    palette.setColor(QPalette::WindowText, QColor(displayData[Right].nameColor));
    this->ui->bluePlayerName->setPalette(palette);

    applyFactionSkin(displayData);
    updateTurnOrderIcons(displayData);
    updateHeadToHead(displayData);
    emit textUpdated();
}

void MainDisplay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton )
    {
        this->dragingOldWindowPos = event->globalPosition();
        this->holdingLeft = true;
    }
}

void MainDisplay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton )
    {
        this->holdingLeft = false;
    }
}

void MainDisplay::mouseMoveEvent(QMouseEvent *event)
{
    if (this->holdingLeft)
    {
        const QPointF delta = event->globalPosition() - this->dragingOldWindowPos;
        move(x()+delta.x(), y()+delta.y());
        this->dragingOldWindowPos = event->globalPosition();
    }
}

