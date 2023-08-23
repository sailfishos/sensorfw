QT += network
TARGET = sensorclient

TEMPLATE = lib

include( ../common-config.pri )

SOURCES += sensormanagerinterface.cpp \
    sensormanager_i.cpp \
    abstractsensor_i.cpp \
    socketreader.cpp \
    compasssensor_i.cpp \
    orientationsensor_i.cpp \
    accelerometersensor_i.cpp \
    alssensor_i.cpp \
    tapsensor_i.cpp \
    proximitysensor_i.cpp \
    rotationsensor_i.cpp \
    magnetometersensor_i.cpp \
    gyroscopesensor_i.cpp \
    lidsensor_i.cpp \
    humiditysensor_i.cpp \
    pressuresensor_i.cpp \
    temperaturesensor_i.cpp \
    stepcountersensor_i.cpp

HEADERS += sensormanagerinterface.h \
    sensormanager_i.h \
    abstractsensor_i.h \
    socketreader.h \
    compasssensor_i.h \
    orientationsensor_i.h \
    accelerometersensor_i.h \
    alssensor_i.h \
    tapsensor_i.h \
    proximitysensor_i.h \
    rotationsensor_i.h \
    magnetometersensor_i.h \
    gyroscopesensor_i.h \
    lidsensor_i.h \
    humiditysensor_i.h \
    pressuresensor_i.h \
    temperaturesensor_i.h \
    stepcountersensor_i.h

SENSORFW_INCLUDEPATHS = .. \
    ../include \
    ../datatypes \
    ../core

DEPENDPATH += $$SENSORFW_INCLUDEPATHS
INCLUDEPATH += $$SENSORFW_INCLUDEPATHS

QMAKE_LIBDIR_FLAGS += -L../datatypes -lsensordatatypes-qt$${QT_MAJOR_VERSION}

include(../common-install.pri)
publicheaders.files = $$HEADERS
target.path = $$SHAREDLIBPATH
INSTALLS += target
