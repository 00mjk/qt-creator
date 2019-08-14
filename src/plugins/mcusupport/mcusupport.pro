include(../../qtcreatorplugin.pri)

DEFINES += MCUSUPPORT_LIBRARY

QT += gui network

HEADERS += \
    mcusupport_global.h \
    mcusupportconstants.h \
    mcusupportdevice.h \
    mcusupportoptionspage.h \
    mcusupportplugin.h \
    mcusupportrunconfiguration.h

SOURCES += \
    mcusupportdevice.cpp \
    mcusupportoptionspage.cpp \
    mcusupportplugin.cpp \
    mcusupportrunconfiguration.cpp

RESOURCES += \
    mcusupport.qrc
