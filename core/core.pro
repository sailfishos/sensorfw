QT += network

TEMPLATE = lib
TARGET = sensorfw
include( ../common-config.pri )

CONFIG += link_pkgconfig
VERSION = 0.9.0

SENSORFW_INCLUDEPATHS = .. \
                        ../include \
                        ../filters \
                        ../datatypes

DEPENDPATH += $$SENSORFW_INCLUDEPATHS
INCLUDEPATH += $$SENSORFW_INCLUDEPATHS

DEFINES += PLUGIN_DIRECTORY=\"\\\"$$[QT_INSTALL_LIBS]/sensord-qt$${QT_MAJOR_VERSION}\\\"\"

QMAKE_LIBDIR_FLAGS += -L../datatypes\
                      -lsensordatatypes-qt$${QT_MAJOR_VERSION}

SOURCES += sensormanager.cpp \
    sensormanager_a.cpp \
    pusher.cpp \
    ringbuffer.cpp \
    producer.cpp \
    source.cpp \
    consumer.cpp \
    bin.cpp \
    filter.cpp \
    deviceadaptor.cpp \
    loader.cpp \
    plugin.cpp \
    abstractsensor_a.cpp \
    abstractsensor.cpp \
    parameterparser.cpp \
    abstractchain.cpp \
    sysfsadaptor.cpp \
    sockethandler.cpp \
    inputdevadaptor.cpp \
    config.cpp \
    nodebase.cpp

HEADERS += sensormanager.h \
    sensormanager_a.h \
    dataemitter.h \
    pusher.h \
    ringbuffer.h \
    producer.h \
    callback.h \
    source.h \
    consumer.h \
    sink.h \
    bin.h \
    filter.h \
    deviceadaptor.h \
    deviceadaptorringbuffer.h \
    bufferreader.h \
    loader.h \
    plugin.h \
    abstractsensor_a.h \
    abstractsensor.h \
    logging.h \
    parameterparser.h \
    abstractchain.h \
    sysfsadaptor.h \
    sockethandler.h \
    inputdevadaptor.h \
    config.h \
    nodebase.h

mce {
    SOURCES += mcewatcher.cpp
    HEADERS += mcewatcher.h
    DEFINES += SENSORFW_MCE_WATCHER
}

contains(CONFIG,ssusysinfo) {
    PKGCONFIG += ssu-sysinfo
    QMAKE_CXXFLAGS += -DUSE_SSUSYSINFO
}

lunaservice {
    SOURCES += lsclient.cpp
    HEADERS += lsclient.h
    DEFINES += SENSORFW_LUNA_SERVICE_CLIENT
    PKGCONFIG += Qt$${QT_MAJOR_VERSION}Gui json-c
    PKGCONFIG += luna-service2 LunaSysMgrCommon LunaSysMgrIpcMessages
}

contains(CONFIG,hybris) {
} else {
    publicheaders.path  = $${publicheaders.path}/core
    publicheaders.files = $$HEADERS

    include(../common-install.pri)
    target.path = $$SHAREDLIBPATH
    INSTALLS += target
}
