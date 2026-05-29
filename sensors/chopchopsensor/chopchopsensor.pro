CONFIG += link_pkgconfig

TARGET = chopchopsensor

HEADERS += chopchopsensor.h \
           chopchopsensor_a.h \
           chopchopplugin.h

SOURCES += chopchopsensor.cpp \
           chopchopsensor_a.cpp \
           chopchopplugin.cpp

include( ../sensor-config.pri )

contextprovider {
    DEFINES += PROVIDE_CONTEXT_INFO
    PKGCONFIG += contextprovider-1.0
}
