QT += testlib \
      dbus \
      network
QT -= gui

include(../common-install.pri)

CONFIG += debug
TEMPLATE = app
TARGET = sensorapi-test
HEADERS += clientapitest.h
SOURCES += clientapitest.cpp

#CONFIG += link_pkgconfig
#PKGCONFIG += mlite5

SENSORFW_INCLUDEPATHS = ../.. \
                        ../../include \
                        ../../filters \
			../../qt-api

DEPENDPATH += $$SENSORFW_INCLUDEPATHS
INCLUDEPATH += $$SENSORFW_INCLUDEPATHS

QMAKE_LIBDIR_FLAGS += -L../../qt-api \
                      -L../../datatypes

QMAKE_LIBDIR_FLAGS += -lsensordatatypes-qt$${QT_MAJOR_VERSION} -lsensorclient-qt$${QT_MAJOR_VERSION}
