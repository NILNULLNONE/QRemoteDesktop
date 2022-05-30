#include "logdialog.h"
#include "ui_logdialog.h"
#include "logger.h"
#include<QDebug>

LogDialog::LogDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LogDialog)
{
    ui->setupUi(this);
//    logHandler = new LogVisualizer(ui->textBrowser);
    logHandler = new LoggerHandler();
    Logger::Inst().RegisterHandler(logHandler);
    connect(logHandler, &LoggerHandler::siglog,
            this, &LogDialog::on_recvLog);
}

void LogDialog::on_recvLog(const QString& log)
{
    ui->textBrowser->insertPlainText(log);
}

LogDialog::~LogDialog()
{
    delete ui;
}
