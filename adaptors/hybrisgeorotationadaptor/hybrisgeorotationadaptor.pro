TARGET       = hybrisgeorotationadaptor

HEADERS += hybrisgeorotationadaptor.h \
           hybrisgeorotationadaptorplugin.h

SOURCES += hybrisgeorotationadaptor.cpp \
           hybrisgeorotationadaptorplugin.cpp

LIBS+= -L../../core -lhybrissensorfw-qt$${QT_MAJOR_VERSION}

include( ../adaptor-config.pri )
config_hybris {
    PKGCONFIG += android-headers
}
