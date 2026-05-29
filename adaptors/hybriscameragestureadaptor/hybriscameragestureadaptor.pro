TARGET = hybriscameragestureadaptor

HEADERS += hybriscameragestureadaptor.h \
           hybriscameragestureadaptorplugin.h

SOURCES += hybriscameragestureadaptor.cpp \
           hybriscameragestureadaptorplugin.cpp
LIBS += -L../../core -lhybrissensorfw-qt$${QT_MAJOR_VERSION}

include( ../adaptor-config.pri )
config_hybris {
    PKGCONFIG += android-headers
}
