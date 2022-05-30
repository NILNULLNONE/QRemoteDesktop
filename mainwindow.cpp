#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QStyle>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    nsWidget = new NetworkSetting();
    logDialog = new LogDialog();
    desktopView = new DesktopView();
    ui->vlayout->addWidget(nsWidget);
    ui->vlayout->addWidget(logDialog);
//    ui->hlayout->addWidget(desktopView);
    nsWidget->RegisterRDDataHandler(desktopView);
//    desktopView->show();

    connect(nsWidget, &NetworkSetting::sig_buildConn,
             this, &MainWindow::on_BuildConn);
    connect(nsWidget, &NetworkSetting::sig_UpdateSize,
            this, &MainWindow::on_DesktopSizeChanged);
//    connect(nsWidget, &NetworkSetting::sig_resoChanged,
//            this, &MainWindow::on_ResoChanged);
    connect(desktopView, &DesktopView::sig_close,
            nsWidget, &NetworkSetting::TriggerDisconnect);
//    nsWidget->Init();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_BuildConn()
{
    if(desktopView->isVisible())return;
    desktopView->show();
}

void MainWindow::on_DesktopSizeChanged(quint16 w, quint16 h)
{
    h = h + QApplication::style()->pixelMetric(QStyle::PM_TitleBarHeight);
    if(desktopView->width() == w || desktopView->height() == h)
        return;
//    qDebug()<<"resize desktop view "<<w<<" "<<h;
    desktopView->resize(w, h);
}

//void MainWindow::on_ResoChanged(quint16 w, quint16 h)
//{
//    h = h + QApplication::style()->pixelMetric(QStyle::PM_TitleBarHeight);
//    qDebug()<<"resize desktop view "<<w<<" "<<h;
////   desktopView->setMinimumSize(w, h);
////   desktopView->setMaximumSize(w, h);
//    desktopView->resize(w, h);
//}



void MainWindow::on_NewConnectionAction_triggered(bool checked)
{
//    if(nsWidget->isVisible())return;
//    nsWidget->setWindowFlags(Qt::WindowStaysOnTopHint);
//    nsWidget->show();
}


void MainWindow::on_OpenLogWindow_triggered(bool checked)
{
//    if(logDialog->isVisible())return;
//    logDialog->setWindowFlags(Qt::WindowStaysOnTopHint);
//    logDialog->show();
}

