#include "statisticsdatabase.h"

#include <QSqlQuery>
#include <QSqlError>

#include <QString>
#include <QStandardPaths>
#include <QDateTime>
#include <QList>
#include <QDir>
#include <QDebug>

StatisticsDatabase::StatisticsDatabase(QObject *parent)
    : QObject{parent},
    statistics(QSqlDatabase::addDatabase("QSQLITE"))
{
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir appDir(dirPath);
    if(!appDir.exists())
    {
        appDir.mkdir(dirPath);
    }
    statistics.setDatabaseName(dirPath + "/statistics.sql");

    if (!statistics.open()) {
        qWarning() << "Unable to establish a database connection.\n" << statistics.lastError().text();
    }
    else
    {
        QSqlQuery query;
        query.exec("create table matches "
                   "(id int primary key, "
                   "localname varchar(20),"
                   "remotename varchar(20),"
                   "localtown varchar(20),"
                   "remotetown varchar(20),"
                   "localhero varchar(20),"
                   "remotehero varchar(20),"
                   "localcolor varchar(20),"
                   "remotecolor varchar(20),"
                   "localgold int,"
                   "remotegold int,"
                   "localrating int,"
                   "remoterating int,"
                   "wincolor varchar(20),"
                   "ratingchange int,"
                   "date varchar(30));");

        model.setTable("matches");
        model.setQuery("SELECT * FROM matches");
        model.setHeaderData(0, Qt::Horizontal, tr("ID"));
        model.setHeaderData(1, Qt::Horizontal, tr("Player name"));
        model.setHeaderData(2, Qt::Horizontal, tr("Opponent name"));
        model.setHeaderData(3, Qt::Horizontal, tr("Player Town"));
        model.setHeaderData(4, Qt::Horizontal, tr("Opponent Town"));
        model.setHeaderData(5, Qt::Horizontal, tr("Player Hero"));
        model.setHeaderData(6, Qt::Horizontal, tr("Opponent Hero"));
        model.setHeaderData(7, Qt::Horizontal, tr("Player Color"));
        model.setHeaderData(8, Qt::Horizontal, tr("Opponent Color"));
        model.setHeaderData(9, Qt::Horizontal, tr("Player Gold"));
        model.setHeaderData(10, Qt::Horizontal, tr("Opponent Gold"));
        model.setHeaderData(11, Qt::Horizontal, tr("Player Rating"));
        model.setHeaderData(12, Qt::Horizontal, tr("Opponent Rating"));
        model.setHeaderData(13, Qt::Horizontal, tr("Winner"));
        model.setHeaderData(14, Qt::Horizontal, tr("Rating changed"));
        model.setHeaderData(15, Qt::Horizontal, tr("Time"));
    }
}


QSqlRelationalTableModel *StatisticsDatabase::getModel()
{
    return &this->model;
}


bool StatisticsDatabase::addMatchResult(displayInfoStruct matchInfo)
{
    displayPlayerInfoStruct * local = &matchInfo.player[0];
    displayPlayerInfoStruct * remote = &matchInfo.player[1];

    if(matchInfo.player[1].isLocalPlayer)
    {
        local = &matchInfo.player[1];
        remote = &matchInfo.player[0];
    }

    QSqlQuery query;
    uint64_t newID = 0;
    query.exec("SELECT * FROM matches ORDER BY ID DESC LIMIT 1;");
    while (query.next()) {
        newID = query.value(0).toInt() + 1;
    }

    QDateTime date = QDateTime::currentDateTime();
    QString formattedTime = date.toString("yyyy-MM-dd hh:mm:ss");

    bool success = query.exec("insert into matches values(" +
                              QString::number(newID) + ",\"" +
                              local->name.toLower() + "\",\"" +
                              remote->name.toLower() + "\",\"" +
                              local->town + "\",\"" +
                              remote->town + "\",\"" +
                              local->hero + "\",\"" +
                              remote->hero + "\",\"" +
                              local->playerColor + "\",\"" +
                              remote->playerColor + "\",\"" +
                              local->money + "\",\"" +
                              remote->money + "\",\"" +
                              local->rating + "\",\"" +
                              remote->rating + "\",\"" +
                              matchInfo.winner + "\",\"" +
                              matchInfo.ratingDelta + "\",\"" +
                              formattedTime + "\");");

    model.select();

    return success;
}
