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

find_package(Gettext REQUIRED)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/LINGUAS" LANGUAGES REGEX "^[^#]")

set(GMO_FILES)

foreach(LANG ${LANGUAGES})
    string(STRIP "${LANG}" LANG)
    if(LANG)
        set(PO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${LANG}.po")
        set(GMO_FILE "${CMAKE_CURRENT_BINARY_DIR}/${LANG}.gmo")

        add_custom_command(
            OUTPUT "${GMO_FILE}"
            COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -c -o "${GMO_FILE}" "${PO_FILE}"
            DEPENDS "${PO_FILE}"
            COMMENT "Generating translation ${LANG}.gmo"
        )
        list(APPEND GMO_FILES "${GMO_FILE}")

        install(
            FILES "${GMO_FILE}"
            DESTINATION "${CMAKE_INSTALL_LOCALEDIR}/${LANG}/LC_MESSAGES"
            RENAME "nabi.mo"
        )
    endif()
endforeach()

add_custom_target(translations ALL DEPENDS ${GMO_FILES})
