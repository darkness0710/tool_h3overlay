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
static const QByteArray USER_AGENT = "h3overlay/1.1.4.0 (Heroes3 HotA stream overlay)";

// The service allows five requests per second. A whole match needs four, so
// the only reason to ever back off is a shared address being over the limit.
constexpr int DEFAULT_RETRY_SECONDS = 2;
constexpr int MAX_RETRY_SECONDS = 60;

// The lobby publishes the picks, and with them the trade, only once the match
// has actually started, which the API documents as usually under a minute.
// Give it a couple of minutes and then stop asking.
constexpr int TRADE_ATTEMPTS = 8;
constexpr int TRADE_RETRY_SECONDS = 15;


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
    this->local.clear();
    this->opponent.clear();
}

void HotaMetaClient::setPlayers(const QString &localName, const QString &opponentName)
{
    if(localName.isEmpty() || opponentName.isEmpty())
    {
        return;
    }
    // Called from the update that runs ten times a second, so anything already
    // fetched must not be fetched again.
    if(localName == this->local && opponentName == this->opponent)
    {
        return;
    }

    clear();
    this->local = localName;
    this->opponent = opponentName;
    refresh();
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
        // This endpoint reports what the HotA server itself has, which is what
        // the game client shows, so it stays in step with the in game number.
        if(object.contains("rating") == false)
        {
            return;
        }
        const QString rating = QString::number(object.value("rating").toInt());
        if(isLocal)
        {
            this->fetched.localRating = rating;
        }
        else
        {
            this->fetched.opponentRating = rating;
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
