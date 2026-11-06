TEMPLATE = subdirs

include( ../common-config.pri )
include( ../common-install.pri )

SUBDIRS  = accelerometerchain \
           orientationchain \
           magcalibrationchain \
           compasschain

# compasschain links -lmagcalibrationchain-qt$${QT_MAJOR_VERSION}, so it cannot
# be built at the same time as the chain it links. This directory is not
# CONFIG+=ordered (only the top level is, and that only serialises chains
# against filters, sensors, ... - not the subdirs within it), so without an
# explicit dependency make is free to start both at once and compasschain's
# link step loses the race:
#   ld: cannot find -lmagcalibrationchain-qt6
compasschain.depends = magcalibrationchain
