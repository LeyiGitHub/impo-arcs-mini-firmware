# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Shared setup for the standalone application and an enclosing internal project.
# Include before project(); source lists and dependencies stay in the components.
get_filename_component(GADGET_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Keep each build's generated configuration beside its build artifacts.
# Explicit -DSDKCONFIG paths, including the flash helper's, still take priority.
if(NOT SDKCONFIG)
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig" CACHE FILEPATH "Project configuration")
endif()

if(NOT CMAKE_SOURCE_DIR STREQUAL GADGET_SOURCE_DIR)
    list(APPEND EXTRA_COMPONENT_DIRS
        "${GADGET_SOURCE_DIR}/main"
        "${GADGET_SOURCE_DIR}/components"
    )
endif()

# A device folder (devices/<id>/, devices/README.md) is one board or gadget:
# device.json, its settings (sdkconfig), its code and its components. IMPO_DEVICE
# (-DIMPO_DEVICE=<id> or the environment) builds one. A device with a "base"
# builds on that device: a StackChan is a CoreS3 in a robot body, so its build
# has the CoreS3's folder too, and its settings load after the CoreS3's.
# Only the folders of the device being built are components of the build, so
# one device's code and dependencies never reach another's.
if(NOT IMPO_DEVICE AND DEFINED ENV{IMPO_DEVICE})
    set(IMPO_DEVICE "$ENV{IMPO_DEVICE}")
endif()
if(IMPO_DEVICE)
    set(IMPO_DEVICE "${IMPO_DEVICE}" CACHE STRING "The device folder to build (devices/<id>)")
    set(impo_chain)
    set(impo_id "${IMPO_DEVICE}")
    while(impo_id)
        set(impo_dir "${GADGET_SOURCE_DIR}/devices/${impo_id}")
        if(NOT EXISTS "${impo_dir}/device.json")
            message(FATAL_ERROR "No device ${impo_id}: ${impo_dir}/device.json does not exist")
        endif()
        if(impo_id IN_LIST impo_chain)
            message(FATAL_ERROR "Device ${impo_id} is its own base")
        endif()
        list(PREPEND impo_chain "${impo_id}")
        file(READ "${impo_dir}/device.json" impo_json)
        string(JSON impo_target ERROR_VARIABLE impo_err GET "${impo_json}" target)
        string(JSON impo_profile ERROR_VARIABLE impo_err GET "${impo_json}" profile)
        string(JSON impo_id ERROR_VARIABLE impo_err GET "${impo_json}" base)
        if(impo_err)
            set(impo_id "")
        endif()
    endwhile()
    # The root device names the chip and the shared settings it starts from.
    list(GET impo_chain 0 impo_root)
    file(READ "${GADGET_SOURCE_DIR}/devices/${impo_root}/device.json" impo_json)
    string(JSON IMPO_DEVICE_TARGET GET "${impo_json}" target)
    string(JSON impo_profile GET "${impo_json}" profile)
    set(impo_defaults "${GADGET_SOURCE_DIR}/sdkconfig.defaults" "${GADGET_SOURCE_DIR}/devices/sdkconfig.${impo_profile}")
    foreach(impo_id IN LISTS impo_chain)
        list(APPEND EXTRA_COMPONENT_DIRS "${GADGET_SOURCE_DIR}/devices/${impo_id}")
        list(APPEND impo_defaults "${GADGET_SOURCE_DIR}/devices/${impo_id}/sdkconfig")
    endforeach()
    # IMPO_EXTRA_DEFAULTS: more settings files on top, such as devices/sdkconfig.impo-bench.
    if(DEFINED ENV{IMPO_EXTRA_DEFAULTS})
        list(APPEND impo_defaults $ENV{IMPO_EXTRA_DEFAULTS})
    endif()
    set(SDKCONFIG_DEFAULTS "${impo_defaults}")
    if(NOT IDF_TARGET)
        set(IDF_TARGET "${IMPO_DEVICE_TARGET}")
    elseif(NOT IDF_TARGET STREQUAL IMPO_DEVICE_TARGET)
        message(FATAL_ERROR "Device ${IMPO_DEVICE} is an ${IMPO_DEVICE_TARGET}, not an ${IDF_TARGET}")
    endif()
    message(STATUS "Device: ${impo_chain} (${IMPO_DEVICE_TARGET})")
endif()

# ESP-IDF reads version.txt from the active project root unless PROJECT_VER is set.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
