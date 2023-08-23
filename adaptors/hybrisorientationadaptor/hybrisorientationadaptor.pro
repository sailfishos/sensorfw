TARGET       = hybrisorientationadaptor

HEADERS += hybrisorientationadaptor.h \
           hybrisorientationadaptorplugin.h

SOURCES += hybrisorientationadaptor.cpp \
           hybrisorientationadaptorplugin.cpp

LIBS+= -L../../core -lhybrissensorfw-qt$${QT_MAJOR_VERSION}

include( ../adaptor-config.pri )
config_hybris {
    PKGCONFIG += android-headers
}
