#include "mainwindow.h"
//#include "win32helper.h"
#include <QApplication>
#include"copytest.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qRegisterMetaType<QAbstractSocket::SocketState>("QAbstractSocket::SocketState");
    qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");
//    qRegisterMetaType<TestCopy>("TestCopy");

    MainWindow w;
    w.show();

//    TestObj o;
//    o.Test();
    return a.exec();
}
