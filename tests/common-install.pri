QT += testlib

include(../common-config.pri)

QMAKE_LIBDIR_FLAGS += -L../../datatypes -L../../../datatypes

QMAKE_LIBDIR_FLAGS += -lsensordatatypes-qt$${QT_MAJOR_VERSION}

target.path = /usr/bin
INSTALLS += target
