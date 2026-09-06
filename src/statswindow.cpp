#include "statswindow.h"
#include "ui_statswindow.h"

#include <QFileDialog>

StatsWindow::StatsWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::StatsWindow)
{
    ui->setupUi(this);
    connect(ui->exportButton, &QPushButton::clicked, this, &StatsWindow::exportPressed);
}


void StatsWindow::setModel(QSqlRelationalTableModel *model)
{
    this->ui->tableView->setModel(model);
}


StatsWindow::~StatsWindow()
{
    delete ui;
}


void StatsWindow::exportPressed()
{
    QString filename = QFileDialog::getSaveFileName(this,
                                                    "Export data as CVS",
                                                    "",
                                                    "*.csv");
    if (filename.isEmpty())
    {
        return;
    }
    QString allData = this->ui->tableView->getAlldata();
    QFile outFile(filename);
    if(outFile.open(QIODevice::WriteOnly))
    {
        if(outFile.write(allData.toLatin1()) != allData.toLatin1().size())
        {
            this->ui->statusLabel->setText("Error! Could write all data to " + filename);
            qCritical() << this->ui->statusLabel->text();
        }
        else
        {
            this->ui->statusLabel->setText("Data exported succesfully to " + filename);
            qInfo() << this->ui->statusLabel->text();
        }
    }
    else
    {
        this->ui->statusLabel->setText("Error! Could not open file " + filename);
        qCritical() << this->ui->statusLabel->text();
    }
}
