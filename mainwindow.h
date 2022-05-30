#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "networksetting.h"
#include "logdialog.h"
#include"desktopview.h"
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_NewConnectionAction_triggered(bool checked);

    void on_OpenLogWindow_triggered(bool checked);

    void on_BuildConn();

//    void on_ResoChanged(quint16 w, quint16 h);
    void on_DesktopSizeChanged(quint16 w, quint16 h);
private:
    Ui::MainWindow *ui;
    NetworkSetting* nsWidget;
    LogDialog* logDialog;
public:
    DesktopView* desktopView;

//protected:
//    virtual void resizeEvent(QResizeEvent* evt) override
//    {
//        desktopView->resize(800, 600);
//        QMainWindow::resizeEvent(evt);
//    }
};
#endif // MAINWINDOW_H
