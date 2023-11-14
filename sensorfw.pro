# RPM build: Hybris plugin has separate spec file that does:
#   qmake CONFIG+=hybris
# And pro-file behavioral differences are handled via:
#   contains(CONFIG,hybris)  { ... }
#
# Debian builds: debian/rules triggers build time hybris check:
#   qmake CONFIG+=autohybris
# And pro-file behavioral differences are handled via:
#   config_hybris { ... }
#
# LuneOS builds: Uses build time hybris check and special handling of hybris in core/hybris.pro:
#   qmake CONFIG+=autohybris CONFIG+=luneos

contains(CONFIG,autohybris)|contains(CONFIG,luneos) {
    load(configure)
    qtCompileTest(hybris)
}

TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = datatypes \
          adaptors \
          core \
          filters \
          sensors \
          sensord \
          qt-api \
          chains \
          tests \
          examples

contains(CONFIG,configs) {
    contains(CONFIG,hybris) {
        SENSORDHYBRISCONFIGFILE.files = config/sensord-hybris.conf
        SENSORDHYBRISCONFIGFILE.path = /etc/sensorfw
        INSTALLS += SENSORDHYBRISCONFIGFILE
    }

    contains(CONFIG,legacy) {

        SENSORFWCONFIGFILES.files = config/sensord-rx_51.conf \
               config/sensord-oaktrail.conf \
               config/sensord-exopc.conf \
               config/sensord-aava.conf \
               config/sensord-rm_696.conf \
               config/sensord-arm_grouper_0000.conf \
               config/sensord-mrst_cdk.conf \
               config/sensord-ncdk.conf \
               config/sensord.conf \
               config/sensord-rm_680.conf \
               config/sensord-icdk.conf \
               config/sensord-u8500.conf \

        SENSORFWCONFIGFILES.path = /etc/sensorfw
        INSTALLS += SENSORFWCONFIGFILES
    }

    SENSORCONFIG_SETUP.files = config/sensord-daemon-conf-setup
    SENSORCONFIG_SETUP.path = /usr/bin

    INSTALLS += SENSORCONFIG_SETUP
}

contains(CONFIG,hybris) {

    SUBDIRS = core/hybris.pro \
               adaptors
} else {
    config_hybris {
    # Reorder so that adaptors are built after hybris.
    SUBDIRS -= adaptors
    SUBDIRS += core/hybris.pro \
               adaptors
    }
    publicheaders.files += include/*.h

    INSTALLS += PKGCONFIGFILES QTCONFIGFILES
    PKGCONFIGFILES.path = $$[QT_INSTALL_LIBS]/pkgconfig
    QTCONFIGFILES.files = sensord.prf

    qt-api.depends = datatypes
    sensord.depends = datatypes adaptors sensors chains

    include( doc/doc.pri )
    include( common-install.pri )
    include( common-config.pri )

    PKGCONFIGFILES.files = sensord-qt$${QT_MAJOR_VERSION}.pc
    PKGCONFIGFILES.commands = 'sed -i "s/Version:.*/Version: $$PC_VERSION/" $$_PRO_FILE_PWD_/sensord-qt$${QT_MAJOR_VERSION}.pc'
    QTCONFIGFILES.path = $$[QT_INSTALL_ARCHDATA]/mkspecs/features

}



# How to make this work in all cases?
#PKGCONFIGFILES.commands = sed -i \"s/Version:.*$$/Version: `head -n1 debian/changelog | cut -f 2 -d\' \' | tr -d \'()\'`/\" sensord-qt$${QT_MAJOR_VERSION}.pc


!contains(CONFIG,hybris) {
# config file installation not handled here
    DBUSCONFIGFILES.files = sensorfw.conf
    DBUSCONFIGFILES.path = /etc/dbus-1/system.d
    INSTALLS += DBUSCONFIGFILES

    SENSORDCONFIGFILES.files  = config/10-sensord-default.conf
    SENSORDCONFIGFILES.files += config/20-sensors-default.conf
    SENSORDCONFIGFILES.path = /etc/sensorfw/sensord.conf.d
    INSTALLS += SENSORDCONFIGFILES
}

contains(CONFIG,systemdunit) {
    # Install service files through packaging to take into account
    # units file location unless called with CONFIG+=systemdunit
    SENSORSYSTEMD.files = rpm/sensorfwd.service
    SENSORSYSTEMD.path = /lib/systemd/system
    INSTALLS += SENSORSYSTEMD
}

OTHER_FILES += rpm/sensorfw-qt$${QT_MAJOR_VERSION}.spec \
               rpm/sensorfw-qt$${QT_MAJOR_VERSION}-binder.spec \
               rpm/sensorfw-qt$${QT_MAJOR_VERSION}-hybris.inc \
               rpm/sensorfw-qt$${QT_MAJOR_VERSION}-hybris.spec

OTHER_FILES += config/*
