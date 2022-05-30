#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
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
    desktopView->setMinimumSize(800, 600);
    ui->hlayout->addWidget(desktopView);

    nsWidget->RegisterRDDataHandler(desktopView);
//    desktopView->show();
}

MainWindow::~MainWindow()
{
    delete ui;
}


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

