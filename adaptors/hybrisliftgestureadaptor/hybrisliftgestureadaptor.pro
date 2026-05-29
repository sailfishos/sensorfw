TARGET = hybrisliftgestureadaptor

HEADERS += hybrisliftgestureadaptor.h \
           hybrisliftgestureadaptorplugin.h

SOURCES += hybrisliftgestureadaptor.cpp \
           hybrisliftgestureadaptorplugin.cpp
LIBS += -L../../core -lhybrissensorfw-qt$${QT_MAJOR_VERSION}

include( ../adaptor-config.pri )
config_hybris {
    PKGCONFIG += android-headers
}
