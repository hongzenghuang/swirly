# The Restful Matching-Engine.
# Copyright (C) 2013, 2018 Swirly Cloud Limited.
#
# This program is free software; you can redistribute it and/or modify it under the terms of the
# GNU General Public License as published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with this program; if
# not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.

if(PYTHONINTERP_FOUND)

  add_custom_target(swift
    COMMAND "${ENV_EXECUTABLE}"
            "SWIRLY_PROGRAM=\"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/swirlyd\""
            "SWIRLY_SCHEMA=\"${PROJECT_SOURCE_DIR}/etc/sql/schema.sqlite\""
            "SWIRLY_REFDATA=\"${PROJECT_SOURCE_DIR}/etc/sql/test.sql\""
            "${PYTHON_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -v
    DEPENDS swirlyd)
endif()
