CONFIG += link_pkgconfig

TARGET = cameragesturesensor

HEADERS += cameragesturesensor.h \
           cameragesturesensor_a.h \
           cameragestureplugin.h

SOURCES += cameragesturesensor.cpp \
           cameragesturesensor_a.cpp \
           cameragestureplugin.cpp

include( ../sensor-config.pri )

contextprovider {
    DEFINES += PROVIDE_CONTEXT_INFO
    PKGCONFIG += contextprovider-1.0
}
