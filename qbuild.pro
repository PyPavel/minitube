TEMPLATE=app
TARGET=minitube

CONFIG+=qtopia
DEFINES+=QTOPIA

# I18n info
STRING_LANGUAGE=en_US
LANGUAGES=en_US

VERSION = 1.9
DEFINES += APP_VERSION="$$VERSION"

APP_NAME = Minitube
DEFINES += APP_NAME="$$APP_NAME"

APP_UNIX_NAME = minitube
DEFINES += APP_UNIX_NAME="$$APP_UNIX_NAME"

DEFINES += QT_USE_FAST_CONCATENATION
DEFINES += QT_USE_FAST_OPERATOR_PLUS

# TODO Saner string behaviour
# DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII QT_STRICT_ITERATORS
TARGET = minitube
QT += network \
    xml \
    phonon

HEADERS += src/MainWindow.h \
    src/SearchView.h \
    src/MediaView.h \
    src/AboutView.h \
    src/youtubesearch.h \
    src/video.h \
    src/youtubestreamreader.h \
    src/View.h \
    src/searchlineedit.h \
    src/urllineedit.h \
    src/spacer.h \
    src/constants.h \
    src/iconloader/qticonloader.h \
    src/ListModel.h \
    src/playlist/PrettyItemDelegate.h \
    src/networkaccess.h \
    src/videomimedata.h \
    src/global.h \
    src/updatechecker.h \
    src/playlistwidget.h \
    src/searchparams.h \
    src/minisplitter.h \
    src/loadingwidget.h \
    src/videoareawidget.h \
    src/autocomplete.h \
    src/videodefinition.h \
    src/fontutils.h \
    src/globalshortcuts.h \
    src/globalshortcutbackend.h \
    src/downloadmanager.h \
    src/downloaditem.h \
    src/downloadview.h \
    src/downloadmodel.h \
    src/downloadlistview.h \
    src/downloadsettings.h \
    src/youtubesuggest.h \
    src/suggester.h \
    src/channelsuggest.h \
    src/temporary.h \
    src/segmentedcontrol.h \
    src/playlistview.h \
    src/refinesearchwidget.h \
    src/refinesearchbutton.h \
    src/sidebarwidget.h
SOURCES += src/main.cpp \
    src/MainWindow.cpp \
    src/SearchView.cpp \
    src/MediaView.cpp \
    src/AboutView.cpp \
    src/youtubesearch.cpp \
    src/youtubestreamreader.cpp \
    src/searchlineedit.cpp \
    src/urllineedit.cpp \
    src/spacer.cpp \
    src/video.cpp \
    src/iconloader/qticonloader.cpp \
    src/ListModel.cpp \
    src/playlist/PrettyItemDelegate.cpp \
    src/videomimedata.cpp \
    src/updatechecker.cpp \
    src/networkaccess.cpp \
    src/playlistwidget.cpp \
    src/searchparams.cpp \
    src/minisplitter.cpp \
    src/loadingwidget.cpp \
    src/videoareawidget.cpp \
    src/autocomplete.cpp \
    src/videodefinition.cpp \
    src/constants.cpp \
    src/fontutils.cpp \
    src/globalshortcuts.cpp \
    src/globalshortcutbackend.cpp \
    src/downloadmanager.cpp \
    src/downloaditem.cpp \
    src/downloadview.cpp \
    src/downloadmodel.cpp \
    src/downloadlistview.cpp \
    src/downloadsettings.cpp \
    src/youtubesuggest.cpp \
    src/channelsuggest.cpp \
    src/temporary.cpp \
    src/segmentedcontrol.cpp \
    src/playlistview.cpp \
    src/refinesearchwidget.cpp \
    src/refinesearchbutton.cpp \
    src/sidebarwidget.cpp
RESOURCES += resources.qrc
DESTDIR = build/target/
OBJECTS_DIR = build/obj/
MOC_DIR = build/moc/
RCC_DIR = build/rcc/

# Tell Qt Linguist that we use UTF-8 strings in our sources
CODECFORTR = UTF-8
CODECFORSRC = UTF-8
include(locale/locale.pri)

# Package info
pkg [
    name=qgcide-eng-dictionary
    desc="An English explanatory dictionary with GCIDE database"
    license=GPLv2
    version=1.0
    maintainer="Radek Polak <psonek2@seznam.cz>"
]

# Install rules
target [
    hint=sxe
    domain=untrusted
]

desktop [
    hint=desktop
    files=minitube.desktop
    path=/apps/Applications
]

pics [
    hint=pics
    files=images/*
    path=/pics/qgcide
]