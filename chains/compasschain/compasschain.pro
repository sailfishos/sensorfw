TARGET       = compasschain

HEADERS += compasschain.h   \
           compasschainplugin.h \
           compassfilter.h \
           orientationfilter.h

SOURCES += compasschain.cpp   \
           compasschainplugin.cpp \
           compassfilter.cpp \
           orientationfilter.cpp

INCLUDEPATH += ../../filters/coordinatealignfilter \
               ../../filters/downsamplefilter \
               ../../filters/avgaccfilter \
               ../../chains/magcalibrationchain \
               ../../filters/magcoordinatealignfilter \
               ../../filters/declinationfilter

# CompassChain calls into every one of the siblings above, but the plugin only
# ever put them on INCLUDEPATH and never linked any of them. The .so builds
# regardless, carrying undefined symbols with no DT_NEEDED that could resolve
# them, and sensorfwd rejects it at load - one symbol at a time, so fixing them
# individually just moves the error:
#   undefined symbol: _ZN19MagCalibrationChain16resetCalibrationEv
#   undefined symbol: _ZN16DownsampleFilter10setTimeoutEi
#   undefined symbol: _ZN17DeclinationFilter21declinationCorrectionEv
# compasssensor and rotationsensor are invalidated along with compasschain, and
# rotationsensor is what QtSensors clients (Messwerk) ask for - which is why
# rotation never reported while the UI, reading orientation/accelerometer, still
# rotated. sensortestapp also aborts with the stock testapp.conf.
#
# Link the lot, so this cannot regress one symbol at a time again.
LIBS += -L../magcalibrationchain            -lmagcalibrationchain-qt$${QT_MAJOR_VERSION}
LIBS += -L../../filters/downsamplefilter    -ldownsamplefilter-qt$${QT_MAJOR_VERSION}
LIBS += -L../../filters/declinationfilter   -ldeclinationfilter-qt$${QT_MAJOR_VERSION}
LIBS += -L../../filters/coordinatealignfilter -lcoordinatealignfilter-qt$${QT_MAJOR_VERSION}
LIBS += -L../../filters/magcoordinatealignfilter -lmagcoordinatealignfilter-qt$${QT_MAJOR_VERSION}
LIBS += -L../../filters/avgaccfilter        -lavgaccfilter-qt$${QT_MAJOR_VERSION}

# The plugins are installed into PLUGINPATH, which is not on the dynamic linker's
# search path, and have no soname, so the DT_NEEDED entries above would not be
# found when sensorfwd dlopen()s compasschain (the Loader loads a plugin before
# its Dependencies()). Look for them next to the plugin itself.
QMAKE_LFLAGS += "-Wl,-rpath,\'\$$ORIGIN\'"

include( ../chain-config.pri )
