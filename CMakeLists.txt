cmake_minimum_required(VERSION 3.20)
project(alliulingo VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Network)

find_package(PkgConfig REQUIRED)
pkg_check_modules(TESSERACT REQUIRED tesseract)

add_executable(alliulingo ${GUI_TYPE}
    src/main.cpp
    src/core/hotkey_filter.h
    src/core/hotkey_filter.cpp
    src/core/config_manager.h
    src/core/config_manager.cpp
    src/core/prompt_manager.h
    src/core/prompt_manager.cpp
    src/core/localization_manager.h
    src/core/localization_manager.cpp
    src/network/llm_client.h
    src/network/llm_client.cpp
    src/network/fallback_translator.h
    src/network/fallback_translator.cpp
    src/ui/selection_overlay.h
    src/ui/selection_overlay.cpp
    src/ui/result_popup.h
    src/ui/result_popup.cpp
    src/ui/help_window.h
    src/ui/help_window.cpp
    src/ui/prompt_info_window.h
    src/ui/prompt_info_window.cpp
    src/ui/manual_translate_window.h
    src/ui/manual_translate_window.cpp
    src/ocr/ocr_engine.h
    src/ocr/ocr_engine.cpp
    src/res/resources.qrc
    src/res/app_info.rc
)

target_link_libraries(alliulingo
    PRIVATE
        Qt6::Widgets
        Qt6::Gui
        Qt6::Network
        ${TESSERACT_LIBRARIES}
        user32
        dwmapi
)

target_include_directories(alliulingo
    PRIVATE
        src
        ${TESSERACT_INCLUDE_DIRS}
)

target_compile_options(alliulingo
    PRIVATE
        ${TESSERACT_CFLAGS_OTHER}
)