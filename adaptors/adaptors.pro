TEMPLATE = subdirs

include( ../common-config.pri )

HYBRIS_SUBDIRS = hybrisaccelerometer \
                 hybrisalsadaptor \
                 hybrisgyroscopeadaptor \
                 hybrismagnetometeradaptor \
                 hybrispressureadaptor \
                 hybrisproximityadaptor \
                 hybrisorientationadaptor \
                 hybrisrotationadaptor \
                 hybrisgeorotationadaptor \
                 hybrisstepcounteradaptor

# split like this as Sailfish only installs hybris plugins
contains(CONFIG,hybris) {
    SUBDIRS += $$HYBRIS_SUBDIRS

} else {

SUBDIRS = alsadaptor \
          alsadaptor-evdev \
          alsadaptor-sysfs \
          alsadaptor-ascii \
          tapadaptor \
          accelerometeradaptor \
          magnetometeradaptor \
          magnetometeradaptor-ascii \
          magnetometeradaptor-evdev \
          touchadaptor \
          kbslideradaptor \
          proximityadaptor \
          proximityadaptor-evdev \
          proximityadaptor-ascii \
          gyroscopeadaptor \
          gyroscopeadaptor-evdev

SUBDIRS += lidsensoradaptor-evdev
SUBDIRS += iioadaptor
SUBDIRS += humidityadaptor
SUBDIRS += pressureadaptor
SUBDIRS += temperatureadaptor

config_hybris {
    SUBDIRS += $$HYBRIS_SUBDIRS
}

contains(CONFIG,legacy) {

SUBDIRS += mrstaccelerometer
SUBDIRS += magnetometeradaptor-ncdk
SUDBIRS += oemtabletmagnetometeradaptor
SUBDIRS += pegatronaccelerometeradaptor
SUBDIRS += oemtabletalsadaptor-ascii
SUBDIRS += oaktrailaccelerometer
SUBDIRS += oemtabletaccelerometer
SUDBIRS += oemtabletgyroscopeadaptor
SUBDIRS += steaccelerometeradaptor
SUBDIRS += mpu6050accelerometer

}

}


