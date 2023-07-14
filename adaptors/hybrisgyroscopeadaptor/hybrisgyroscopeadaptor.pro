TARGET       = hybrisgyroscopeadaptor

HEADERS += hybrisgyroscopeadaptor.h \
           hybrisgyroscopeadaptorplugin.h

SOURCES += hybrisgyroscopeadaptor.cpp \
           hybrisgyroscopeadaptorplugin.cpp

LIBS+= -L../../core -lhybrissensorfw-qt$${QT_MAJOR_VERSION}

include( ../adaptor-config.pri )
config_hybris {
    PKGCONFIG += android-headers
}
