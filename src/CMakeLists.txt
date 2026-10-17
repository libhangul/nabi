# Nabi - X Input Method server for hangul
# Copyright (C) 2026 Choe Hwanjin
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA

add_executable(nabi
    nabi.h
    gettext.h
    xim_protocol.h
    default-icons.h
    debug.h
    debug.c
    server.h
    server.c
    ic.h
    ic.c
    fontset.h
    fontset.c
    eggtrayicon.h
    eggtrayicon.c
    session.h
    session.c
    candidate.h
    candidate.c
    keycapturedialog.h
    keycapturedialog.c
    conf.h
    conf.c
    handler.c
    ui.c
    preference.h
    preference.c
    handlebox.h
    handlebox.c
    sctc.h
    util.h
    util.c
    ustring.h
    ustring.c
    keyboard-layout.h
    keyboard-layout.c
    main.c
)

target_include_directories(nabi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)

target_compile_definitions(nabi PRIVATE
    HAVE_CONFIG_H
    LOCALEDIR="${LOCALEDIR}"
    NABI_DATA_DIR="${NABI_DATA_DIR}"
    NABI_THEMES_DIR="${NABI_THEMES_DIR}"
)

target_link_libraries(nabi PRIVATE
    Ximd
    PkgConfig::GTK
    PkgConfig::LIBHANGUL
    X11::X11
)

if(HAVE_LIBSM)
    target_link_libraries(nabi PRIVATE X11::SM X11::ICE)
endif()

if(Intl_FOUND)
    target_link_libraries(nabi PRIVATE ${Intl_LIBRARIES})
endif()

install(TARGETS nabi
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
