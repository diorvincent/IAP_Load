#-------------------------------------------------
#
# Project created by QtCreator 2014-08-30T14:32:51
# add AES-128 Encrypt for .bin fireware download
#
#-------------------------------------------------

QT       += core gui serialport printsupport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Iap_Load
TEMPLATE = app


SOURCES += main.cpp\
        aes_128_decrypt.cpp \
        canbase.cpp \
        chuangxincan.cpp \
        csvexporter.cpp \
        databuffer.cpp \
        plotwidget.cpp \
        proxyplot.cpp \
        qcustomplot.cpp \
        widget.cpp \
        zlgcan_s.cpp

HEADERS  += widget.h \
    aes_128_decrypt.h \
    canbase.h \
    canframe.h \
    chuangxincan.h \
    config.h \
    csvexporter.h \
    databuffer.h \
    plotwidget.h \
    proxyplot.h \
    qcustomplot.h \
    typedef.h \
    zlgcan.h \
    zlgcan_s.h

FORMS += widget.ui    # 确保路径与实际UI文件位置一致

RESOURCES += \
    src/name.qrc \
    src/src.qrc

win32: LIBS += -L$$PWD/./ -lzlgcan

INCLUDEPATH += $$PWD/.
DEPENDPATH += $$PWD/.


win32: LIBS += -L$$PWD/./ -lControlCANFD

INCLUDEPATH += $$PWD/.
DEPENDPATH += $$PWD/.

win32:!win32-g++: PRE_TARGETDEPS += $$PWD/./ControlCANFD.lib
