CONFIG += console c++11
SOURCES += main.cpp \
    audiooutput.cpp
TEMPLATE = app
TARGET = audiooutput

QT += multimedia widgets

HEADERS       = audiooutput.h \
    audiooutput.h

SOURCES       = audiooutput.cpp \
                main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/multimedia/audiooutput
INSTALLS += target
