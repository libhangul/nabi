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

add_library(Ximd STATIC
    FrameMgr.c
    FrameMgr.h
    IMConn.c
    IMMethod.c
    IMValues.c
    IMdkit.h
    Xi18n.h
    Xi18nX.h
    XimFunc.h
    XimProto.h
    i18nAttr.c
    i18nClbk.c
    i18nIMProto.c
    i18nIc.c
    i18nMethod.c
    i18nPtHdr.c
    i18nUtil.c
    i18nX.c
)

target_include_directories(Ximd
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_BINARY_DIR}
)

target_compile_definitions(Ximd PRIVATE HAVE_CONFIG_H)

target_link_libraries(Ximd PRIVATE X11::X11)
