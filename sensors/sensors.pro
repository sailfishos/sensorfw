TEMPLATE = subdirs

include( ../common-config.pri )
include( ../common-install.pri )

SUBDIRS  = accelerometersensor \
           orientationsensor \
           tapsensor \
           alssensor \
           proximitysensor \
           compasssensor \
           rotationsensor \
           magnetometersensor \
           gyroscopesensor \
           lidsensor \
           humiditysensor \
           pressuresensor \
           wakeupsensor \
           temperaturesensor \
           stepcountersensor

contextprovider:SUBDIRS += contextplugin
