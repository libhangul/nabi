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

cmake_minimum_required(VERSION 3.13)

project(nabi
    VERSION 1.0.2
    DESCRIPTION "Korean X Input Method"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckSymbolExists)

# Options
option(ENABLE_DEBUG "Include debug code" OFF)
option(ENABLE_NLS "Enable Native Language Support" ON)
set(DEFAULT_KEYBOARD "2" CACHE STRING "Default hangul keyboard (2, 39, 3f)")
set(DEFAULT_THEME "Jini" CACHE STRING "Default icon theme")

# Validate DEFAULT_KEYBOARD
if(NOT DEFAULT_KEYBOARD MATCHES "^(2|39|3f)$")
    message(WARNING "Invalid DEFAULT_KEYBOARD '${DEFAULT_KEYBOARD}', falling back to '2'")
    set(DEFAULT_KEYBOARD "2")
endif()

# Validate DEFAULT_THEME
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/themes/${DEFAULT_THEME}")
    message(WARNING "There is no such theme directory: themes/${DEFAULT_THEME}, using default 'Jini'")
    set(DEFAULT_THEME "Jini")
endif()

if(ENABLE_DEBUG)
    set(NABI_DEBUG 1)
    add_compile_options(-Wall -g)
endif()

# Package metadata for config.h
set(PACKAGE "${PROJECT_NAME}")
set(PACKAGE_NAME "${PROJECT_NAME}")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PACKAGE_TARNAME "${PROJECT_NAME}")
set(PACKAGE_URL "https://github.com/libhangul/nabi")
set(PACKAGE_BUGREPORT "https://github.com/libhangul/nabi/")
set(VERSION "${PROJECT_VERSION}")
set(GETTEXT_PACKAGE "${PROJECT_NAME}")

# Installation directories
set(NABI_DATA_DIR "${CMAKE_INSTALL_FULL_DATADIR}/nabi")
set(NABI_THEMES_DIR "${NABI_DATA_DIR}/themes")
set(LOCALEDIR "${CMAKE_INSTALL_FULL_LOCALEDIR}")

# Dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED IMPORTED_TARGET gtk+-2.0>=2.4.0)
pkg_check_modules(LIBHANGUL REQUIRED IMPORTED_TARGET libhangul>=0.1.0)
find_package(X11 REQUIRED)

if(X11_SM_FOUND)
    set(HAVE_LIBSM 1)
endif()

# NLS / Gettext
if(ENABLE_NLS)
    find_package(Gettext)
    find_package(Intl)
    check_symbol_exists(gettext "libintl.h" HAVE_LIBINTL_GETTEXT)
    check_function_exists(gettext HAVE_GETTEXT)
    check_function_exists(dcgettext HAVE_DCGETTEXT)
    if(HAVE_LIBINTL_GETTEXT OR HAVE_GETTEXT)
        set(HAVE_GETTEXT 1)
    endif()
    if(NOT HAVE_DCGETTEXT AND Intl_FOUND)
        set(CMAKE_REQUIRED_LIBRARIES ${Intl_LIBRARIES})
        check_function_exists(dcgettext HAVE_DCGETTEXT)
        unset(CMAKE_REQUIRED_LIBRARIES)
    endif()
endif()

# Header checks
check_include_file("dirent.h" HAVE_DIRENT_H)
check_include_file("inttypes.h" HAVE_INTTYPES_H)
check_include_file("langinfo.h" HAVE_LANGINFO_H)
check_include_file("libintl.h" HAVE_LIBINTL_H)
check_include_file("locale.h" HAVE_LOCALE_H)
check_include_file("ndir.h" HAVE_NDIR_H)
check_include_file("stddef.h" HAVE_STDDEF_H)
check_include_file("stdint.h" HAVE_STDINT_H)
check_include_file("stdio.h" HAVE_STDIO_H)
check_include_file("stdlib.h" HAVE_STDLIB_H)
check_include_file("strings.h" HAVE_STRINGS_H)
check_include_file("string.h" HAVE_STRING_H)
check_include_file("sys/dir.h" HAVE_SYS_DIR_H)
check_include_file("sys/ndir.h" HAVE_SYS_NDIR_H)
check_include_file("sys/param.h" HAVE_SYS_PARAM_H)
check_include_file("sys/stat.h" HAVE_SYS_STAT_H)
check_include_file("sys/types.h" HAVE_SYS_TYPES_H)
check_include_file("unistd.h" HAVE_UNISTD_H)

# Function checks
check_function_exists("gethostname" HAVE_GETHOSTNAME)
check_function_exists("localtime_r" HAVE_LOCALTIME_R)
check_function_exists("memmove" HAVE_MEMMOVE)
check_function_exists("memset" HAVE_MEMSET)
check_function_exists("mkdir" HAVE_MKDIR)
check_function_exists("putenv" HAVE_PUTENV)
check_function_exists("setlocale" HAVE_SETLOCALE)
check_function_exists("strchr" HAVE_STRCHR)
check_function_exists("strdup" HAVE_STRDUP)
check_function_exists("strftime" HAVE_STRFTIME)
check_function_exists("strtol" HAVE_STRTOL)
check_function_exists("vprintf" HAVE_VPRINTF)

set(HAVE_MALLOC 1)
set(HAVE_REALLOC 1)
set(STDC_HEADERS 1)

# Generate config.h
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)

# Install logos
install(
    FILES nabi.png nabi.svg nabi-about.png
    DESTINATION "${CMAKE_INSTALL_DATADIR}/nabi"
)

# Subdirectories
add_subdirectory(IMdkit)
add_subdirectory(src)
add_subdirectory(tables)
add_subdirectory(themes)

if(ENABLE_NLS AND GETTEXT_FOUND)
    add_subdirectory(po)
endif()

set(ARCHIVE_PREFIX "${PROJECT_NAME}-${PROJECT_VERSION}")
set(SOURCE_PACKAGE "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-${PROJECT_VERSION}.tar.xz")
add_custom_target(dist
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}

    COMMAND git archive --prefix="${ARCHIVE_PREFIX}/" -o "${SOURCE_PACKAGE}" HEAD
)

add_custom_target(distcheck
    DEPENDS dist
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}

    COMMAND ${CMAKE_COMMAND} -E remove_directory "_distcheck"
    COMMAND ${CMAKE_COMMAND} -E make_directory "_distcheck"
    COMMAND ${CMAKE_COMMAND} -E chdir "_distcheck" ${CMAKE_COMMAND} -E tar xf "${SOURCE_PACKAGE}"
    COMMAND ${CMAKE_COMMAND} -B "_distcheck/build" -S "_distcheck/${ARCHIVE_PREFIX}"
    COMMAND ${CMAKE_COMMAND} --build "_distcheck/build"
)
