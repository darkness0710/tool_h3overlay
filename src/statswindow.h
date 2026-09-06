#ifndef STATSWINDOW_H
#define STATSWINDOW_H

#include <QDialog>
#include <QTableView>
#include <QSqlRelationalTableModel>

namespace Ui {
class StatsWindow;
}

class StatsWindow : public QDialog
{
    Q_OBJECT

public:
    explicit StatsWindow(QWidget *parent = nullptr);

    /**
     * @brief setModel Binds the sql model to the table view
     * @param model The model to bind to the table view
     */
    void setModel(QSqlRelationalTableModel * model);
    ~StatsWindow();


private slots:
    /**
     * @brief exportPressed Slot to handle the button press of exporting
     * the database.
     */
    void exportPressed();
private:
    Ui::StatsWindow *ui;

};

#endif // STATSWINDOW_H
