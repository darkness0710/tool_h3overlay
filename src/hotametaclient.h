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
    /** Thumbs up the lobby shows next to a player. Comes back on the same
     *  request as the rating, so it costs nothing extra to have. */
    QString localLikes;
    QString opponentLikes;
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
     * @brief refreshAfterMatch Called when a match has just finished.
     *
     * The API only reflects a finished game within about two minutes of it
     * ending, so asking once at the WIN/LOSE popup got the rating from before
     * the game, which then stayed on screen for a whole rematch. Two things
     * cover that gap:
     *
     * - The local player's new rating is worked out at once from the change
     *   the popup shows, and kept until the API reports something other than
     *   the rating from before the game. The API wins from then on, and a
     *   difference between the two is logged.
     * - The ratings, today's record and the head to head score are re-read a
     *   few more times over the next minutes. The opponent's rating relies on
     *   this alone, since the popup only shows the local player's change.
     *
     * @param localRatingDelta The change the popup shows, e.g. "-18". Empty or
     * unparsable leaves the local rating to the API.
     */
    void refreshAfterMatch(const QString &localRatingDelta);

    /**
     * @brief clear Forgets the current players and their statistics, so the
     * next match starts from a clean state.
     */
    void clear();

    const hotaMetaStruct & data() const;

    /**
     * @brief belongsToLocal Whether a player name is the one this client
     * fetched as the local player.
     *
     * Needed because the isLocal flag the game reports cannot separate the two
     * sides of a hot seat match, where both players are local. Names can, and
     * they are what the requests were keyed on in the first place.
     * @param name The player name to test.
     * @return true if it is the local player of the current pair.
     */
    bool belongsToLocal(const QString &name) const;

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

    /** Re-reads what a finished game changes, but not the trade, which
     *  belongs to the next match and has its own retries. */
    void refreshResults();

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
    /** Bumped whenever the pair of players is forgotten, so follow-up
     *  refreshes scheduled for an earlier pair can tell they are stale. */
    unsigned int generation = 0;

    /** A local rating worked out from the WIN/LOSE popup, held until the API
     *  catches up with the game it was worked out from. */
    struct PendingRating
    {
        bool active = false;
        QString player;
        int before = 0;
        int expected = 0;
    } pendingLocal;
};

#endif // HOTAMETACLIENT_H
