add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../components/impo_arcs_mini ${CMAKE_BINARY_DIR}/impo_arcs_mini)
listenai_library_named(board_yunbao)
listenai_library_sources(board_yunbao.c yunbao_button.c yunbao_display.c yunbao_screen_ui.c yunbao_wake.c)
listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
