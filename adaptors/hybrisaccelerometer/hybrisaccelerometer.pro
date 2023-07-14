TARGET       = hybrisaccelerometeradaptor

HEADERS += hybrisaccelerometeradaptor.h \
           hybrisaccelerometeradaptorplugin.h

SOURCES += hybrisaccelerometeradaptor.cpp \
           hybrisaccelerometeradaptorplugin.cpp
LIBS+= -L../../core -lhybrissensorfw-qt$${QT_MAJOR_VERSION}

include( ../adaptor-config.pri )
config_hybris {
    PKGCONFIG += android-headers
}
