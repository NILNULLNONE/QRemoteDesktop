QT       += core gui network opengl

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    logdialog.cpp \
    lz4/lz4.c \
    lz4helper.cpp \
    main.cpp \
    mainwindow.cpp \
    networksetting.cpp \
    win32helper.cpp

HEADERS += \
    copytest.h \
    desktopview.h \
    logdialog.h \
    logger.h \
    lz4/lz4.h \
    mainwindow.h \
    networksetting.h \
    rddatahandler.h

FORMS += \
    logdialog.ui \
    mainwindow.ui \
    networksetting.ui

win32:LIBS += -lGdi32 -lUser32
# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
