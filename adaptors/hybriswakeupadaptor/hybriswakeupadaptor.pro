TARGET = hybriswakeupadaptor

HEADERS += hybriswakeupadaptor.h \
           hybriswakeupadaptorplugin.h

SOURCES += hybriswakeupadaptor.cpp \
           hybriswakeupadaptorplugin.cpp
LIBS += -L../../core -lhybrissensorfw-qt$${QT_MAJOR_VERSION}

include( ../adaptor-config.pri )
config_hybris {
    PKGCONFIG += android-headers
}
