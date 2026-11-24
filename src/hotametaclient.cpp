#include "hotametaclient.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QTimer>
#include <QUrl>
#include <QDebug>


static const QString API_BASE = "https://hotameta.com";

// The API asks clients to identify themselves so that named clients can be
// warned before a breaking change. A generic agent is treated as a scraper and
// answered with 403.
static const QByteArray USER_AGENT = "h3overlay/1.1.8.3 (Heroes3 HotA stream overlay)";

// The service allows five requests per second. A whole match needs four, so
// the only reason to ever back off is a shared address being over the limit.
constexpr int DEFAULT_RETRY_SECONDS = 2;
constexpr int MAX_RETRY_SECONDS = 60;

// The lobby publishes the picks, and with them the trade, only once the match
// has actually started, which the API documents as usually under a minute.
// Give it a couple of minutes and then stop asking.
constexpr int TRADE_ATTEMPTS = 8;
constexpr int TRADE_RETRY_SECONDS = 15;

// The API takes one to two and a half minutes to show a finished game, going
// by games watched finishing on its match feed. After a game, poll at this
// interval until both ratings have moved, for at most this long.
constexpr int RESULT_POLL_SECONDS = 30;
constexpr int RESULT_POLL_LIMIT_SECONDS = 300;

// When a match starts the opponent may have finished another game moments
// before, which the API is not showing yet. Re-read the ratings at these
// points to catch that.
constexpr int MATCH_START_POLL_SECONDS[] = { 60, 150, 300 };

// How long a rating worked out from the popup may hold out against the API.
// Past this the API is trusted even if it still reports the old number, so a
// misread popup can never pin a wrong rating on screen for good.
constexpr qint64 PENDING_RATING_LIMIT_MS = 10 * 60 * 1000;


HotaMetaClient::HotaMetaClient(QObject *parent):
    QObject(parent)
{
}

const hotaMetaStruct & HotaMetaClient::data() const
{
    return this->fetched;
}

void HotaMetaClient::clear()
{
    this->fetched = hotaMetaStruct();
    ++this->generation;
    // The opponent's pending result belongs to that opponent only. The local
    // one is kept: setPlayers carries it over to the next pair.
    this->pendingOpponent.active = false;
    this->local.clear();
    this->opponent.clear();
}

bool HotaMetaClient::belongsToLocal(const QString &name) const
{
    // The lobby treats names case insensitively and the API answers in lower
    // case, so neither side of this can be trusted to match exactly.
    return name.compare(this->local, Qt::CaseInsensitive) == 0;
}

bool HotaMetaClient::setPlayers(const QString &localName, const QString &opponentName)
{
    if(localName.isEmpty() || opponentName.isEmpty())
    {
        return false;
    }
    // Called from the update that runs ten times a second, so anything already
    // fetched must not be fetched again.
    if(localName == this->local && opponentName == this->opponent)
    {
        return false;
    }

    clear();
    this->local = localName;
    this->opponent = opponentName;
    // A new opponent straight after a game can arrive before the API has the
    // result of that game, and clear() just dropped the number that has it.
    if(this->pendingLocal.active &&
            this->pendingLocal.player.compare(localName, Qt::CaseInsensitive) == 0)
    {
        this->fetched.localRating = QString::number(this->pendingLocal.expected);
    }
    refresh();
    return true;
}

void HotaMetaClient::refresh()
{
    if(this->local.isEmpty() || this->opponent.isEmpty())
    {
        return;
    }
    // Everything here moves when a match finishes: both ratings change, the
    // head to head score gains a game, and today's record gains a game. A
    // rematch against the same opponent also gets a new trade. Refreshing only
    // part of it would leave the overlay showing a mix of before and after.
    fetchRating(this->local, true);
    fetchRating(this->opponent, false);
    fetchToday(this->local);
    fetchHeadToHead(this->local, this->opponent);
    fetchTrade(this->local, TRADE_ATTEMPTS);
}

void HotaMetaClient::refreshResults()
{
    if(this->local.isEmpty() || this->opponent.isEmpty())
    {
        return;
    }
    fetchRating(this->local, true);
    fetchRating(this->opponent, false);
    fetchToday(this->local);
    fetchHeadToHead(this->local, this->opponent);
}

void HotaMetaClient::refreshAfterMatch(const QString &localRatingDelta,
                                       const QString &localGameRating)
{
    bool deltaParsed = false;
    const int delta = localRatingDelta.toInt(&deltaParsed);

    // The rating from before the game: the one the API gave for this match,
    // or, if that request never answered, the one read from the game itself.
    bool ratingParsed = false;
    int before = this->fetched.localRating.toInt(&ratingParsed);
    if(ratingParsed == false)
    {
        const int fromGame = localGameRating.toInt(&ratingParsed);
        // The game reports 0 when it has no profile to read from.
        ratingParsed = ratingParsed && fromGame > 0;
        before = fromGame;
    }

    // A change of zero leaves nothing to work out, and could not be told
    // apart from an API that has not caught up anyway.
    if(deltaParsed && ratingParsed && delta != 0 && this->local.isEmpty() == false)
    {
        this->pendingLocal.active = true;
        this->pendingLocal.player = this->local;
        this->pendingLocal.before = before;
        this->pendingLocal.expected = before + delta;
        this->pendingLocal.since.start();
        this->fetched.localRating = QString::number(this->pendingLocal.expected);
        qInfo() << "Local rating from the popup:" << before << "+" << delta
                << "=" << this->pendingLocal.expected;
        emit dataUpdated();
    }
    else
    {
        this->pendingLocal.active = false;
        qInfo() << "Local rating left to the API. Popup change:" << localRatingDelta
                << "API rating:" << this->fetched.localRating
                << "game rating:" << localGameRating;
    }

    // The opponent's change is not on the popup, so all that can be done is
    // to notice when the API moves away from the rating before the game.
    bool opponentParsed = false;
    const int opponentBefore = this->fetched.opponentRating.toInt(&opponentParsed);
    this->pendingOpponent.active = opponentParsed && this->opponent.isEmpty() == false;
    this->pendingOpponent.player = this->opponent;
    this->pendingOpponent.before = opponentBefore;

    refreshResults();
    // With nothing to compare against there is no telling when the API has
    // caught up, so poll the whole window rather than not at all.
    pollResults(RESULT_POLL_SECONDS, RESULT_POLL_LIMIT_SECONDS, resultsSettled() == false);
}

void HotaMetaClient::matchStarted(bool alreadyFetched)
{
    // A rematch keeps the same pair, so setPlayers fetched nothing: re-read
    // everything, the trade included, which is new for every match. This is
    // also what catches a finished game whose popup was never seen.
    if(alreadyFetched == false)
    {
        refresh();
    }

    if(resultsSettled() == false)
    {
        // Still waiting for the API to show the last game.
        pollResults(RESULT_POLL_SECONDS, RESULT_POLL_LIMIT_SECONDS, true);
        return;
    }

    // The opponent may have come straight from another game.
    const unsigned int series = ++this->pollSeries;
    const unsigned int scheduledFor = this->generation;
    for(const int seconds: MATCH_START_POLL_SECONDS)
    {
        QTimer::singleShot(seconds * 1000, this, [this, series, scheduledFor]()
        {
            if(series != this->pollSeries || scheduledFor != this->generation)
            {
                return;
            }
            fetchRating(this->local, true);
            fetchRating(this->opponent, false);
        });
    }
}

bool HotaMetaClient::resultsSettled() const
{
    return this->pendingLocal.active == false && this->pendingOpponent.active == false;
}

void HotaMetaClient::pollResults(int intervalSeconds, int limitSeconds, bool stopWhenSettled)
{
    // Starting a new series cancels whatever was scheduled before it.
    const unsigned int series = ++this->pollSeries;
    const unsigned int scheduledFor = this->generation;
    for(int seconds = intervalSeconds; seconds <= limitSeconds; seconds += intervalSeconds)
    {
        const bool last = seconds + intervalSeconds > limitSeconds;
        QTimer::singleShot(seconds * 1000, this,
                           [this, series, scheduledFor, last, limitSeconds, stopWhenSettled]()
        {
            // A new pair of players has been fetched since, or a newer series
            // has taken over.
            if(series != this->pollSeries || scheduledFor != this->generation)
            {
                return;
            }
            if(stopWhenSettled && resultsSettled())
            {
                return;
            }
            refreshResults();
            if(last && this->pendingOpponent.active)
            {
                qWarning() << "The API still shows the opponent's rating from"
                           << "before the game after" << limitSeconds << "s.";
                this->pendingOpponent.active = false;
            }
        });
    }
}

void HotaMetaClient::get(const QString &path,
                         const std::function<void(const QJsonObject &)> &handler)
{
    QNetworkRequest request(QUrl(API_BASE + path));
    request.setRawHeader("User-Agent", USER_AGENT);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = this->network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, path, handler]()
    {
        reply->deleteLater();

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(status == 429)
        {
            // Over the rate limit. The header says for how long, retry once.
            int wait = reply->rawHeader("Retry-After").toInt();
            if(wait <= 0 || wait > MAX_RETRY_SECONDS)
            {
                wait = DEFAULT_RETRY_SECONDS;
            }
            qInfo() << "hotameta rate limited, retrying" << path << "in" << wait << "s";
            QTimer::singleShot(wait * 1000, this, [this, path, handler]()
            {
                get(path, handler);
            });
            return;
        }

        if(reply->error() != QNetworkReply::NoError)
        {
            qWarning() << "hotameta request failed:" << path << reply->errorString();
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
        if(document.isObject() == false)
        {
            qWarning() << "hotameta returned no object for" << path;
            return;
        }
        handler(document.object());
        emit dataUpdated();
    });
}

void HotaMetaClient::fetchRating(const QString &name, bool isLocal)
{
    const QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(name));
    get("/api/player/" + encoded + "/live-rating",
        [this, isLocal, name](const QJsonObject &object)
    {
        // Since API 1.2.0 this is the stored rating, which only reflects a
        // finished game within about two minutes; see refreshAfterMatch.
        if(object.contains("rating") == false)
        {
            return;
        }
        // The players may have changed while this was in flight.
        const QString &current = isLocal ? this->local : this->opponent;
        if(name.compare(current, Qt::CaseInsensitive) != 0)
        {
            return;
        }
        const QString rating = QString::number(object.value("rating").toInt());
        // The same reply carries the thumbs up count the lobby shows beside
        // the rating, so it is taken here rather than asked for separately.
        const QString likes = object.contains("likes")
                ? QString::number(object.value("likes").toInt())
                : QString();
        if(isLocal && this->pendingLocal.active &&
                name.compare(this->pendingLocal.player, Qt::CaseInsensitive) == 0)
        {
            const int reported = rating.toInt();
            if(reported == this->pendingLocal.before &&
                    this->pendingLocal.since.elapsed() < PENDING_RATING_LIMIT_MS)
            {
                // Still the rating from before the game: keep the one worked
                // out from the popup until the API has the result.
                return;
            }
            if(reported == this->pendingLocal.before)
            {
                qWarning() << "The API still reports" << reported
                           << "long after the game, dropping the popup rating"
                           << this->pendingLocal.expected;
            }
            else if(reported != this->pendingLocal.expected)
            {
                qWarning() << "The API rating" << reported
                           << "differs from the one worked out from the popup"
                           << this->pendingLocal.expected << "- using the API.";
            }
            this->pendingLocal.active = false;
        }
        if(isLocal == false && this->pendingOpponent.active &&
                name.compare(this->pendingOpponent.player, Qt::CaseInsensitive) == 0 &&
                rating.toInt() != this->pendingOpponent.before)
        {
            this->pendingOpponent.active = false;
        }
        if(isLocal)
        {
            this->fetched.localRating = rating;
            this->fetched.localLikes = likes;
        }
        else
        {
            this->fetched.opponentRating = rating;
            this->fetched.opponentLikes = likes;
        }
    });
}

void HotaMetaClient::fetchToday(const QString &name)
{
    const QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(name));
    get("/api/today/" + encoded, [this](const QJsonObject &object)
    {
        if(object.contains("wins") == false || object.contains("losses") == false)
        {
            return;
        }
        this->fetched.todayWins = QString::number(object.value("wins").toInt());
        this->fetched.todayLosses = QString::number(object.value("losses").toInt());
        this->fetched.todayRatingChange =
                QString::number(object.value("rating_change").toInt());
        // Deliberately ignoring "current_rating" here, this endpoint reports it
        // as zero when no game has been played today.
    });
}

void HotaMetaClient::fetchTrade(const QString &name, int attemptsLeft)
{
    const QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(name));
    get("/api/lobby/player/" + encoded, [this, name, attemptsLeft](const QJsonObject &object)
    {
        // The players may have changed while this was in flight, in which case
        // this answer belongs to a match that is no longer on screen.
        if(name.compare(this->local, Qt::CaseInsensitive) != 0)
        {
            return;
        }

        const QJsonObject picks = object.value("picks").toObject();
        const QJsonObject first = picks.value("p1").toObject();
        const QJsonObject second = picks.value("p2").toObject();
        if(first.contains("trade") == false || second.contains("trade") == false)
        {
            // Not started yet, or the picks are not published yet.
            if(attemptsLeft > 1)
            {
                QTimer::singleShot(TRADE_RETRY_SECONDS * 1000, this, [this, name, attemptsLeft]()
                {
                    fetchTrade(name, attemptsLeft - 1);
                });
            }
            return;
        }

        // Whichever of the two entries is the local player decides the sign.
        // The names live in the nested "game" object, not next to the picks.
        const QJsonObject game = object.value("game").toObject();
        const QString firstName = game.contains("p1_name")
                                      ? game.value("p1_name").toString()
                                      : object.value("p1_name").toString();
        if(firstName.isEmpty())
        {
            qWarning() << "hotameta lobby answer has no player names, cannot place the trade";
            return;
        }
        const bool localIsFirst = firstName.compare(name, Qt::CaseInsensitive) == 0;
        const QJsonObject &mine = localIsFirst ? first : second;
        const QJsonObject &theirs = localIsFirst ? second : first;
        // A trade is a transfer, so the sign is the point of it and a positive
        // amount is written with its plus.
        const auto signed_ = [](int value)
        {
            return (value > 0 ? "+" : "") + QString::number(value);
        };
        this->fetched.localTrade = signed_(mine.value("trade").toInt());
        this->fetched.opponentTrade = signed_(theirs.value("trade").toInt());
    });
}

void HotaMetaClient::fetchHeadToHead(const QString &localName, const QString &opponentName)
{
    const QString first = QString::fromUtf8(QUrl::toPercentEncoding(localName));
    const QString second = QString::fromUtf8(QUrl::toPercentEncoding(opponentName));
    // The local player is asked for first, so "p1" is the local player and the
    // record needs no flipping.
    get("/api/h2h/" + first + "/" + second, [this](const QJsonObject &object)
    {
        if(object.contains("p1_wins") == false || object.contains("p2_wins") == false)
        {
            return;
        }
        this->fetched.h2hWins = QString::number(object.value("p1_wins").toInt());
        this->fetched.h2hLosses = QString::number(object.value("p2_wins").toInt());
    });
}
