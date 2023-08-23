TARGET       = hybrisstepcounteradaptor

HEADERS += hybrisstepcounteradaptor.h \
           hybrisstepcounteradaptorplugin.h

SOURCES += hybrisstepcounteradaptor.cpp \
           hybrisstepcounteradaptorplugin.cpp
LIBS+= -L../../core -lhybrissensorfw-qt$${QT_MAJOR_VERSION}

include( ../adaptor-config.pri )
config_hybris {
    PKGCONFIG += android-headers
}
