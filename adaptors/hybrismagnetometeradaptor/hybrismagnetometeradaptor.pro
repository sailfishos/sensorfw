TARGET       = hybrismagnetometeradaptor

HEADERS += hybrismagnetometeradaptor.h \
           hybrismagnetometeradaptorplugin.h

SOURCES += hybrismagnetometeradaptor.cpp \
           hybrismagnetometeradaptorplugin.cpp

LIBS+= -L../../core -lhybrissensorfw-qt$${QT_MAJOR_VERSION}

include( ../adaptor-config.pri )
config_hybris {
    PKGCONFIG += android-headers
}
