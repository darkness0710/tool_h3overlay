#include "aboutwindow.h"
#include "ui_aboutwindow.h"

#include <QApplication>

AboutWindow::AboutWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutWindow)
{
    ui->setupUi(this);
    ui->qtVersionLabel->setText(QString::number(QT_VERSION_MAJOR) + "." +
                                QString::number(QT_VERSION_MINOR) + "." +
                                QString::number(QT_VERSION_PATCH));

    ui->gccVersionLabel->setText(QString::number(__GNUC__) + "." +
                                 QString::number(__GNUC_MINOR__) + "." +
                                 QString::number(__GNUC_PATCHLEVEL__));

    ui->appVersionLabel->setText(QApplication::applicationVersion());

    connect(ui->debugCheckBox, &QCheckBox::clicked, this, &AboutWindow::debugClicked);
}

AboutWindow::~AboutWindow()
{
    delete ui;
}

void AboutWindow::addToLog(const QString &text)
{
    ui->textEdit->append(text);
}

void AboutWindow::debugClicked(const bool checked)
{
    emit activateDebugMessages(checked);
}
