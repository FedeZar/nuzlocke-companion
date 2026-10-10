cmake_minimum_required(VERSION 3.20)
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum supported macOS")
project(NuzlockeCompanion VERSION 0.4.0 LANGUAGES CXX)
if(NOT APPLE)
    message(FATAL_ERROR "This release requires macOS (AppKit and Carbon overlay integration).")
endif()
enable_language(OBJCXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Network)
add_executable(NuzlockeCompanion MACOSX_BUNDLE
    src/main.cpp src/Gdb.cpp src/Nuzlocke.cpp src/ScreenLayout.cpp src/Mac.mm
    assets/AppIcon.icns)
set_source_files_properties(assets/AppIcon.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
target_link_libraries(NuzlockeCompanion PRIVATE Qt6::Widgets Qt6::Network
    "-framework AppKit" "-framework CoreGraphics" "-framework Carbon")
target_compile_options(NuzlockeCompanion PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
set_target_properties(NuzlockeCompanion PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Nuzlocke Companion"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.fedezar.nuzlocke-companion"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "10"
    MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")
# These resources are generated locally. They must never be committed.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/nuzlocke/database.json")
    add_custom_target(companion_resources ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/nuzlocke"
        "$<TARGET_BUNDLE_DIR:NuzlockeCompanion>/Contents/Resources/nuzlocke")
    add_dependencies(NuzlockeCompanion companion_resources)
endif()
