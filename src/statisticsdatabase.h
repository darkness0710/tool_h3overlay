#ifndef STATISTICSDATABASE_H
#define STATISTICSDATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlRelationalTableModel>
#include <QTableView>

#include "gamestructs.h"
class StatisticsDatabase : public QObject
{
    Q_OBJECT

private:
    QSqlDatabase statistics;
    QSqlRelationalTableModel model;
public:
    explicit StatisticsDatabase(QObject *parent = nullptr);

    /**
     * @brief getModel Gets the internal stored sql model
     * @return The internal sql model
     */
    QSqlRelationalTableModel * getModel();

    /**
     * @brief addMatchResult Adds a match result to the database
     * @param matchInfo The data to be stored in the database.
     * @return true if the data could be put into the database,
     * returns false otherwise
     */
    bool addMatchResult(displayInfoStruct matchInfo);
signals:

};

#endif // STATISTICSDATABASE_H
