TARGET       = hybrisalsadaptor

HEADERS += hybrisalsadaptor.h \
           hybrisalsadaptorplugin.h

SOURCES += hybrisalsadaptor.cpp \
           hybrisalsadaptorplugin.cpp
LIBS+= -L../../core -lhybrissensorfw-qt$${QT_MAJOR_VERSION}

include( ../adaptor-config.pri )
config_hybris {
    PKGCONFIG += android-headers
}
