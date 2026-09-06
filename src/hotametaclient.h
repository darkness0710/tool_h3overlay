#ifndef HOTAMETACLIENT_H
#define HOTAMETACLIENT_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QJsonObject>

/**
 * @brief The hotaMetaStruct holds everything the overlay shows that comes from
 * hotameta.com rather than from the game memory. A field stays empty until its
 * request has answered, and the display shows a placeholder for it meanwhile.
 */
struct hotaMetaStruct
{
    QString localRating;
    QString opponentRating;
    QString todayWins;
    QString todayLosses;
    QString todayRatingChange;
    QString h2hWins;
    QString h2hLosses;
    /** The agreed gold trade for the running match. One side is positive and
     *  the other negative, they always add up to zero. */
    QString localTrade;
    QString opponentTrade;
};

/**
 * @brief The HotaMetaClient fetches the online statistics for the two players
 * in the current match from hotameta.com.
 *
 * Requests are only sent when the pair of players changes, and today's record
 * is refreshed when a match finishes. Nothing is fetched from the poll loop,
 * which runs ten times a second.
 */
class HotaMetaClient : public QObject
{
    Q_OBJECT

public:
    explicit HotaMetaClient(QObject *parent = nullptr);

    /**
     * @brief setPlayers Tells the client who is playing. Fetches everything for
     * the pair, but only if this is a pair it has not already fetched, so it is
     * safe to call on every update from the game.
     * @param localName The name of the player running this overlay.
     * @param opponentName The name of the other player.
     */
    void setPlayers(const QString &localName, const QString &opponentName);

    /**
     * @brief refresh Re-reads everything for the current pair of players.
     * Called when a match has just finished, which moves every value here: both
     * ratings, the head to head score, today's record, and, for a rematch
     * against the same opponent, the trade.
     */
    void refresh();

    /**
     * @brief clear Forgets the current players and their statistics, so the
     * next match starts from a clean state.
     */
    void clear();

    const hotaMetaStruct & data() const;

signals:
    /**
     * @brief dataUpdated Emitted whenever a request has added something, so
     * the display can be redrawn.
     */
    void dataUpdated();

private:
    /**
     * @brief get Sends one GET request and hands the parsed object to the
     * handler. A failed request is logged and dropped, leaving the fields it
     * would have filled empty.
     * @param path The path part of the url, already percent encoded.
     * @param handler Called with the response object on success.
     */
    void get(const QString &path, const std::function<void(const QJsonObject &)> &handler);

    void fetchRating(const QString &name, bool isLocal);
    void fetchToday(const QString &name);
    void fetchHeadToHead(const QString &local, const QString &opponent);

    /**
     * @brief fetchTrade Reads the agreed gold trade for the running match from
     * the live lobby. The lobby only publishes the picks a little while after
     * the match starts, so this retries a bounded number of times instead of
     * giving up on the first empty answer.
     * @param name The local player, used to look the running match up.
     * @param attemptsLeft How many more times to retry before giving up.
     */
    void fetchTrade(const QString &name, int attemptsLeft);

    QNetworkAccessManager network;
    hotaMetaStruct fetched;
    QString local;
    QString opponent;
};

#endif // HOTAMETACLIENT_H
