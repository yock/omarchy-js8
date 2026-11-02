QT += core gui testlib
CONFIG += c++20 testcase
TARGET = js8_tests
TEMPLATE = app

ROOT = $$PWD/..
MODEM = $$ROOT/../gfsk8-modem

INCLUDEPATH += $$ROOT/src $$MODEM/include $$MODEM/src
DEFINES += QT_DEPRECATED_WARNINGS

HEADERS += \
    $$ROOT/src/settings.h \
    $$ROOT/src/spectrum.h \
    $$ROOT/src/theme.h \
    $$ROOT/src/translator.h

SOURCES += \
    js8_tests.cpp \
    $$ROOT/src/settings.cpp \
    $$ROOT/src/spectrum.cpp \
    $$ROOT/src/theme.cpp \
    $$ROOT/src/translator.cpp

LIBS += $$ROOT/build/deps/gfsk8modem/libgfsk8modem.a -lpthread
