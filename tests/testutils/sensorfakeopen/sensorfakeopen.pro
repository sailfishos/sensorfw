TEMPLATE = lib
include(../../../common-install.pri)
TARGET = sensorfakeopen
DEPENDPATH += .
INCLUDEPATH += .
QT -= gui
LIBS += -ldl

# Input
HEADERS += sensorfakeopen.h
SOURCES += sensorfakeopen.cpp

target.path = $$SHAREDLIBPATH
INSTALLS += target
