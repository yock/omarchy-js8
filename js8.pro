QT += core gui qml quick

CONFIG += c++20 release
TARGET = js8
TEMPLATE = app

ROOT = $$PWD
MODEM = $$ROOT/../gfsk8-modem
RAYLIB = $$ROOT/build/deps/raylib-src/src

INCLUDEPATH += $$ROOT/src $$MODEM/include $$MODEM/src $$RAYLIB
DEPENDPATH += $$ROOT/src
DEFINES += GRAPHICS_API_OPENGL_33

HEADERS += \
    src/audio.h \
    src/backend.h \
    src/decoderworker.h \
    src/messagemodel.h \
    src/settings.h \
    src/spectrum.h \
    src/strokefont.h \
    src/tacticaldisplay.h \
    src/theme.h \
    src/translator.h \

SOURCES += \
    src/main.cpp \
    src/audio.cpp \
    src/backend.cpp \
    src/decoderworker.cpp \
    src/messagemodel.cpp \
    src/settings.cpp \
    src/spectrum.cpp \
    src/strokefont.cpp \
    src/tacticaldisplay.cpp \
    src/theme.cpp \
    src/translator.cpp \

RESOURCES += src/resources.qrc

# gfsk8-modem and raylib are built statically into build/deps by bin/build.
# raylib is only used for its rlgl renderer, but the static archive also pulls
# in the X11 libraries that its desktop platform normally needs.
LIBS += \
    $$ROOT/build/deps/gfsk8modem/libgfsk8modem.a \
    $$RAYLIB/libraylib.a \
    -lportaudio -lGL -lX11 -lXrandr -lXinerama -lXcursor -lXi \
    -lm -ldl -lrt -lpthread

QMAKE_CXXFLAGS += -Wall -Wextra
