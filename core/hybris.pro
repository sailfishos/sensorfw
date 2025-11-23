QT += network

TEMPLATE = lib
TARGET = hybrissensorfw

include( ../common-config.pri )

SENSORFW_INCLUDEPATHS = .. \
                        ../include \
                        ../filters \
                        ../datatypes

DEPENDPATH += $$SENSORFW_INCLUDEPATHS
INCLUDEPATH += $$SENSORFW_INCLUDEPATHS

SOURCES += hybrisadaptor.cpp \
           hybrisbackend.cpp \
           hybrisbackend_binder.cpp \
           hybrisbackend_binder_aidl.cpp \
           hybrisbackend_binder_hidl.cpp \
           hybrisbackend_hal.cpp
HEADERS += hybrisadaptor.h \
           hybrisbackend.h \
           hybrisbackend_binder.h \
           hybrisbackend_binder_aidl.h \
           hybrisbackend_binder_hidl.h \
           hybrisbackend_hal.h
LIBS += -L$$[QT_INSTALL_LIBS] -L../datatypes -lsensordatatypes-qt$${QT_MAJOR_VERSION} -L. -lsensorfw-qt$${QT_MAJOR_VERSION}

CONFIG += link_pkgconfig

!contains(CONFIG,binder) {
    LIBS += -lhybris-common
    PKGCONFIG += android-headers libhardware
}

include(../common-install.pri)
target.path = $$SHAREDLIBPATH
INSTALLS += target
