#-------------------------------------------------
#
# Project created by QtCreator 2016-11-16T16:46:32
#
#-------------------------------------------------

QT       += core gui network xml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ConsoleClient
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    connectionpane.cpp \
    connectiondata.cpp \
    addconndialog.cpp \
    groupdata.cpp \
    addgroupdialog.cpp \
    preferencesdialog.cpp \
    splashdialog.cpp

HEADERS  += mainwindow.h \
    connectionpane.h \
    connectiondata.h \
    addconndialog.h \
    groupdata.h \
    addgroupdialog.h \
    preferencesdialog.h \
    splashdialog.h

FORMS    += mainwindow.ui \
    connectionpane.ui \
    addconndialog.ui \
    addgroupdialog.ui \
    preferencesdialog.ui \
    splashdialog.ui

RESOURCES += \
    Resources.qrc

ICON = ConsoleClient.icns

RC_ICONS = ConsoleClient.ico
