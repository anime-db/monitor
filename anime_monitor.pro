#-------------------------------------------------
#
# Project created by QtCreator 2014-04-03T09:30:36
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = anime_monitor
TEMPLATE = app

RC_FILE = myapp.rc

SOURCES += main.cpp\
    TrayIcon.cpp \
    MSettings.cpp

HEADERS  += \
    TrayIcon.h \
    MSettings.h

RESOURCES += \
    icons.qrc
