#ifndef HOTAMETACLIENT_H
#define HOTAMETACLIENT_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QElapsedTimer>

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
     * @return true if this was a new pair and everything was fetched for it.
     */
    bool setPlayers(const QString &localName, const QString &opponentName);

    /**
     * @brief refresh Re-reads everything for the current pair of players.
     * Called when a match has just finished, which moves every value here: both
     * ratings, the head to head score, today's record, and, for a rematch
     * against the same opponent, the trade.
     */
    void refresh();

    /**
     * @brief refreshAfterMatch Called when the WIN/LOSE popup of a ranked game
     * has been seen.
     *
     * The API takes one to two and a half minutes to show a finished game.
     * Until it does:
     *
     * - The local player's new rating is worked out from the change on the
     *   popup and shown at once. It is held until the API reports something
     *   other than the rating from before the game (or for ten minutes at
     *   most); the API wins from then on and a difference is logged.
     * - Both ratings, today's record and the head to head score are re-read
     *   every 30 seconds for up to five minutes, stopping once both ratings
     *   have moved. The opponent's rating relies on this alone, since the
     *   popup only shows the local player's change.
     *
     * @param localRatingDelta The change on the popup, e.g. "-18".
     * @param localGameRating The local rating read from the game, used as the
     * rating before the game if the API never answered for this match.
     */
    void refreshAfterMatch(const QString &localRatingDelta, const QString &localGameRating);

    /**
     * @brief matchStarted Called when a new match begins.
     *
     * Re-reads everything for a rematch, which setPlayers ignores because the
     * pair is unchanged. This is what puts the right numbers up even when the
     * popup of the last game was never seen. If the last game's result is
     * still pending the polling goes on; otherwise the ratings are re-read a
     * few times in case the opponent came straight from another game.
     * @param alreadyFetched true if setPlayers just fetched this pair.
     */
    void matchStarted(bool alreadyFetched);

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

    /** Re-reads the results every intervalSeconds for limitSeconds, stopping
     *  early once both ratings have moved if stopWhenSettled is set. Replaces
     *  any earlier series. */
    void pollResults(int intervalSeconds, int limitSeconds, bool stopWhenSettled);

    /** Nothing is waiting for the API to catch up with a finished game. */
    bool resultsSettled() const;

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
        QElapsedTimer since;
    } pendingLocal;

    /** The opponent's rating from before a finished game, so polling can stop
     *  once the API has moved away from it. */
    struct PendingOpponent
    {
        bool active = false;
        QString player;
        int before = 0;
    } pendingOpponent;

    /** Bumped by every new polling series, so the timers of an older series
     *  can tell they have been replaced. */
    unsigned int pollSeries = 0;
};

#endif // HOTAMETACLIENT_H
