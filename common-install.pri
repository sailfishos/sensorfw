
#
# Common installation specifications
#
#

# Remove gui dependency from everything
QT -= gui

# Path for headers - remember to add files if they should be installed
publicheaders.path = /usr/include/sensord-qt$${QT_MAJOR_VERSION}
PLUGINPATH = $$[QT_INSTALL_LIBS]/sensord-qt$${QT_MAJOR_VERSION}

# Path for shared libraries
SHAREDLIBPATH = $$[QT_INSTALL_LIBS]

INSTALLS += publicheaders
