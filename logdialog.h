#ifndef LOGDIALOG_H
#define LOGDIALOG_H

#include <QDialog>

namespace Ui {
class LogDialog;
}

class LoggerHandler;
class LogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LogDialog(QWidget *parent = nullptr);
    ~LogDialog();
    void on_recvLog(const QString&);
private:
    Ui::LogDialog *ui;
    LoggerHandler* logHandler;
};

#endif // LOGDIALOG_H
