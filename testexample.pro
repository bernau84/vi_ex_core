TARGET = noqt
CONFIG   += console
TEMPLATE = app
CONFIG -= qt

SOURCES += main.cpp \
    vi_ex_io.cpp \
    vi_ex_hid.cpp \
    vi_ex_cell.cpp

HEADERS += \
    vi_ex_par.h \
    vi_ex_mio.h \
    vi_ex_io.h \
    vi_ex_hid.h \
    vi_ex_def.h \
    vi_ex_cell.h \
    vi_ex_ter.h

OTHER_FILES += \
    vi_ex_def_settings_types.inc \
    vi_ex_def_settings_head.inc \
    vi_ex_def_dgram_head.inc







