# SDK 0.1.6 returns DVP frame buffers through the camera_bus device by mistake.
# Backport the upstream 0.1.8 fix into the build tree, preserving ARCS_BASE.
function(impo_yunbao_camera_driver_compat)
if(NOT CONFIG_LISA_CAMERA_DEVICE OR NOT CONFIG_LISA_CAMERA_BUS_DVP)
    return()
endif()

if(TARGET lisa_camera_drv)
    # SDK 0.1.8 already releases through the owning camera device.
    return()
endif()
if(NOT TARGET lisa_camera_arcs)
    message(FATAL_ERROR "Yunbao camera compatibility: expected lisa_camera_arcs or lisa_camera_drv target")
endif()

get_target_property(_yunbao_camera_sources lisa_camera_arcs SOURCES)
get_target_property(_yunbao_camera_source_dir lisa_camera_arcs SOURCE_DIR)
set(_yunbao_camera_dvp_source "")
foreach(_source IN LISTS _yunbao_camera_sources)
    get_filename_component(_absolute "${_source}" ABSOLUTE BASE_DIR "${_yunbao_camera_source_dir}")
    if(_absolute MATCHES "/bus/lisa_camera_bus_dvp\\.c$")
        if(_yunbao_camera_dvp_source)
            message(FATAL_ERROR "Yunbao camera compatibility: multiple DVP bus sources")
        endif()
        set(_yunbao_camera_dvp_source "${_source}")
        set(_yunbao_camera_dvp_path "${_absolute}")
    endif()
endforeach()
if(NOT _yunbao_camera_dvp_source OR NOT EXISTS "${_yunbao_camera_dvp_path}")
    message(FATAL_ERROR "Yunbao camera compatibility: exact DVP bus source was not found")
endif()

file(READ "${_yunbao_camera_dvp_path}" _yunbao_camera_dvp)
string(REGEX MATCHALL "lisa_camera_release_fb\\(dev, priv->(ping_fb|pong_fb|current_fb)\\)"
    _yunbao_camera_bad_releases "${_yunbao_camera_dvp}")
list(LENGTH _yunbao_camera_bad_releases _yunbao_camera_release_count)
if(NOT _yunbao_camera_release_count EQUAL 4)
    message(FATAL_ERROR "Yunbao camera compatibility: expected four SDK 0.1.6 DVP release calls")
endif()
set(_yunbao_camera_release_helper [=[static void camera_bus_dvp_release_fb(camera_bus_dvp_priv_t *priv, lisa_camera_fb_t *fb)
{
    if ((priv == NULL) || (fb == NULL) || (priv->user_data == NULL)) {
        return;
    }
    lisa_camera_release_fb((lisa_device_t *)priv->user_data, fb);
}

]=])
set(_yunbao_camera_callback "static void dvp_event_callback(lisa_dvp_event_t event, void *user_data)")
string(FIND "${_yunbao_camera_dvp}" "${_yunbao_camera_callback}" _yunbao_camera_callback_pos)
if(_yunbao_camera_callback_pos LESS 0)
    message(FATAL_ERROR "Yunbao camera compatibility: DVP callback marker was not found")
endif()
string(REPLACE "${_yunbao_camera_callback}"
    "${_yunbao_camera_release_helper}${_yunbao_camera_callback}"
    _yunbao_camera_dvp "${_yunbao_camera_dvp}")
foreach(_fb ping_fb pong_fb current_fb)
    string(REPLACE "lisa_camera_release_fb(dev, priv->${_fb});"
        "camera_bus_dvp_release_fb(priv, priv->${_fb});"
        _yunbao_camera_dvp "${_yunbao_camera_dvp}")
endforeach()
set(_yunbao_camera_compat_dir "${CMAKE_BINARY_DIR}/yunbao_camera_compat")
file(MAKE_DIRECTORY "${_yunbao_camera_compat_dir}")
set(_yunbao_camera_compat_path "${_yunbao_camera_compat_dir}/lisa_camera_bus_dvp.c")
file(WRITE "${_yunbao_camera_compat_path}" "${_yunbao_camera_dvp}")
list(REMOVE_ITEM _yunbao_camera_sources "${_yunbao_camera_dvp_source}")
list(APPEND _yunbao_camera_sources "${_yunbao_camera_compat_path}")
set_property(TARGET lisa_camera_arcs PROPERTY SOURCES "${_yunbao_camera_sources}")
# The generated source no longer resides beside this internal SDK header.
get_filename_component(_yunbao_camera_bus_dir "${_yunbao_camera_dvp_path}" DIRECTORY)
target_include_directories(lisa_camera_arcs PRIVATE "${_yunbao_camera_bus_dir}")
message(STATUS "Yunbao camera: SDK 0.1.6 DVP frame-owner compatibility fix enabled")
endfunction()

# Gadget configuration can precede the SDK's drivers directory. Resolve its
# camera target at the end of the top-level configuration, once it exists.
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL impo_yunbao_camera_driver_compat)
