TARGET = hybrischopchopadaptor

HEADERS += hybrischopchopadaptor.h \
           hybrischopchopadaptorplugin.h

SOURCES += hybrischopchopadaptor.cpp \
           hybrischopchopadaptorplugin.cpp
LIBS += -L../../core -lhybrissensorfw-qt$${QT_MAJOR_VERSION}

include( ../adaptor-config.pri )
config_hybris {
    PKGCONFIG += android-headers
}
