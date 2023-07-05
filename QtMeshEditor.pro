QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    HarmonicMap.cpp \

HEADERS += \
    mainwindow.h \
    HarmonicMap.h \
    HarmonicMapMesh.h \

FORMS += \
    mainwindow.ui

INCLUDEPATH += \
    "C:/Program Files/opencv/build/include" \
    3rdparty/MeshLib/core \
    3rdparty/eigen \
    3rdparty/freeglut/include

LIBS += -lopengl32 \
        -lglu32

CONFIG(debug, debug|release) {
    LIBS *= -L$$PWD/3rdparty/freeglut/lib/x64 \
            -L$$PWD/3rdparty/freeglut/bin/x64 \
            -lfreeglut
}

CONFIG(debug, debug|release) {
        DESTDIR = $$PWD/build/debug
} else {
        DESTDIR = $$PWD/build/release
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
