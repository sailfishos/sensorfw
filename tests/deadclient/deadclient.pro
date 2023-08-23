include(../common-install.pri)

TEMPLATE = app
TARGET = sensord-deadclient

QT += network dbus
QT -= gui
CONFIG += debug
DEFINES += ORIENTATION

INCLUDEPATH += ../../ \
               ../../include \
               ../../filters \
               ../../qt-api

QMAKE_LIBDIR_FLAGS += -L../../qt-api  \
                      -L../../datatypes

QMAKE_LIBDIR_FLAGS += -lsensordatatypes-qt$${QT_MAJOR_VERSION} -lsensorclient-qt$${QT_MAJOR_VERSION}

# Input
HEADERS += datareceiver.h testwindow.h
SOURCES += deadclient.cpp

deadclienttest.files = sensord-deadclienttest.py
deadclienttest.path = /usr/share/sensorfw-tests

INSTALLS += deadclienttest
