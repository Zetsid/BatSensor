TEMPLATE = app
TARGET = audiooutput

QT += multimedia widgets quick
CONFIG += c++11
HEADERS       = audiooutput.h

SOURCES       = audiooutput.cpp \
                main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/multimedia/audiooutput
INSTALLS += target
