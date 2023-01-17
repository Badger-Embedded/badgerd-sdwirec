# Copyright (c) 2016 Samsung Electronics Co., Ltd All Rights Reserved
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#
# @file        src/CMakeLists.txt
# @author      Adam Malinowski <a.malinowsk2@partner.samsung.com>
#

FIND_PACKAGE(PkgConfig)

PKG_CHECK_MODULES(SDMUX_DEP
    REQUIRED
    libftdi1>=1.4
    popt
    )

SET(SDMUXCTRL_PATH
    ${PROJECT_SOURCE_DIR}/src
    )

SET(SDMUXCTRL_SOURCES
    ${SDMUXCTRL_PATH}/main.cpp
    )

INCLUDE_DIRECTORIES(
    ${SDMUXCTRL_PATH}
    ${FTD2XX_PATH}
    )

ADD_EXECUTABLE(${TARGET_SDMUXCTRL} ${SDMUXCTRL_SOURCES})

TARGET_LINK_LIBRARIES(${TARGET_SDMUXCTRL}
    ${SDMUX_DEP_LIBRARIES}
    )

INSTALL(TARGETS ${TARGET_SDMUXCTRL} DESTINATION ${BIN_INSTALL_DIR})
