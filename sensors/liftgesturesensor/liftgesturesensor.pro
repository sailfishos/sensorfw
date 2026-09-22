CONFIG += link_pkgconfig

TARGET = liftgesturesensor

HEADERS += liftgesturesensor.h \
           liftgesturesensor_a.h \
           liftgestureplugin.h

SOURCES += liftgesturesensor.cpp \
           liftgesturesensor_a.cpp \
           liftgestureplugin.cpp

include( ../sensor-config.pri )

contextprovider {
    DEFINES += PROVIDE_CONTEXT_INFO
    PKGCONFIG += contextprovider-1.0
}
