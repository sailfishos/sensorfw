TEMPLATE = app
TARGET = sensormanagerpipe-test
QT += testlib network dbus
QT -= gui
CONFIG += c++17 debug
INCLUDEPATH += ../.. ../../include ../../core ../../filters ../../datatypes
SOURCES += sensormanagerpipetest.cpp
LIBS += -L$$SENSORFW_CORE -lsensorfw-qt6 -L$$SENSORFW_DT -lsensordatatypes-qt6
