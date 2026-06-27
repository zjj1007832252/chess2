cmake_minimum_required(VERSION 3.16)
project(chess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Locate Qt 6 (from environment or system PATH)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Network Concurrent)

qt_standard_project_setup()

qt_add_executable(chess
    main.cpp
    board.cpp
    ai.cpp
    boardview.cpp
    networkmanager.cpp
    gamemodedialog.cpp
    mainwindow.cpp
)

set_target_properties(chess PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

target_include_directories(chess PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(chess PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
)

target_compile_definitions(chess PRIVATE
    UNICODE
    _UNICODE
)

# Deploy Qt runtime after build
find_program(WINDEPLOYQT6_EXECUTABLE windeployqt6 HINTS "${QT6_INSTALL_PREFIX}/bin")
if(WINDEPLOYQT6_EXECUTABLE)
    add_custom_command(TARGET chess POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E echo "Running windeployqt6..."
        COMMAND "${WINDEPLOYQT6_EXECUTABLE}"
                --dir "${CMAKE_CURRENT_BINARY_DIR}/deploy"
                "$<TARGET_FILE:chess>"
        COMMENT "Deploying Qt runtime with windeployqt6"
    )
else()
    message(WARNING "windeployqt6 not found, skipping deployment step")
endif()
