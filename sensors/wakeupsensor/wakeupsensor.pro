CONFIG += link_pkgconfig

TARGET = wakeupsensor

HEADERS += wakeupsensor.h \
           wakeupsensor_a.h \
           wakeupplugin.h

SOURCES += wakeupsensor.cpp \
           wakeupsensor_a.cpp \
           wakeupplugin.cpp

include( ../sensor-config.pri )

contextprovider {
    DEFINES += PROVIDE_CONTEXT_INFO
    PKGCONFIG += contextprovider-1.0
}
