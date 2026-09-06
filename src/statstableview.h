#ifndef STATSTABLEVIEW_H
#define STATSTABLEVIEW_H

#include <QTableView>
#include <QKeyEvent>

class StatsTableView : public QTableView
{
public:
    StatsTableView();
    StatsTableView(QWidget *widget);

    /**
     * @brief getAlldata Fetches all available data from the database.
     * @return A QString containing all available data from the database
     * in CSV-format.
     */
    QString getAlldata();
protected:
    void keyPressEvent(QKeyEvent *event);

    /**
     * @brief getSelectedData Fetches the current selection
     * @return a QString containing the selected entries in CSV-format
     */
    QString getSelectedData();
};

#endif // STATSTABLEVIEW_H
