QT += network
QT -= gui

include(../common-install.pri)

TEMPLATE = app
TARGET = sensorsockethandler-test

CONFIG += testcase

SOURCES += sockethandlertest.cpp

INCLUDEPATH += ../../include \
    ../../core \
    ../../datatypes \
    ../..

QMAKE_LIBDIR_FLAGS += -L../../builddir/datatypes -L../../datatypes/
QMAKE_LIBDIR_FLAGS += -L../../builddir/core -L../../core/

include(../../common.pri)
