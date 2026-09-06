#include "statstableview.h"

#include <QApplication>
#include <QClipboard>
#include <QRect>

#include <QHeaderView>
#include <QModelIndex>
#include <QItemSelection>

StatsTableView::StatsTableView()
{

}


StatsTableView::StatsTableView(QWidget *widget)
{

}

QString StatsTableView::getAlldata()
{
    selectAll();
    QString allData = getSelectedData();
    clearSelection();
    return allData;
}

QString StatsTableView::getSelectedData()
{

    QModelIndexList cells = selectedIndexes();
    std::sort(cells.begin(), cells.end()); // Necessary, otherwise they are in column order
    QString text;
    int currentRow = 0; // To determine when to insert newlines
    foreach (const QModelIndex& cell, cells) {
        if (text.length() == 0) {
            // First item
        } else if (cell.row() != currentRow) {
            // New row
            text += '\n';
        } else {
            // Next cell
            text += ',';
        }
        currentRow = cell.row();
        if(cell.data().toString().contains(","))
        {
            text += "\"" + cell.data().toString() + "\"";
        }
        else
        {
            text += cell.data().toString();
        }
    }
    return text;
}


void StatsTableView::keyPressEvent(QKeyEvent* event) {

    if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier))
    {
        QApplication::clipboard()->setText(getSelectedData());
    }
    if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier))
    {
        selectAll();
    }
}
