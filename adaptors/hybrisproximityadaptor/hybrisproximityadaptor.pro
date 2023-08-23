TARGET       = hybrisproximityadaptor

HEADERS += hybrisproximityadaptor.h \
           hybrisproximityadaptorplugin.h

SOURCES += hybrisproximityadaptor.cpp \
           hybrisproximityadaptorplugin.cpp

LIBS+= -L../../core -lhybrissensorfw-qt$${QT_MAJOR_VERSION}

include( ../adaptor-config.pri )
config_hybris {
    PKGCONFIG += android-headers
}
