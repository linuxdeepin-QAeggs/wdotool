cmake_minimum_required(VERSION 3.0)

project(wdotoold VERSION 0.1 LANGUAGES CXX)

set(BIN_NAME wdotoold)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_FLAGS "-g -Wall")


find_package(Qt5 COMPONENTS Core Gui Widgets DBus Concurrent Network REQUIRED)


# 执行系统命令检查进程
execute_process(
    COMMAND ps -e
    COMMAND grep Xwayland
    RESULT_VARIABLE PROCESS_FOUND
    OUTPUT_VARIABLE PROCESS_OUTPUT
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(PROCESS_FOUND EQUAL 0)
    set(USE_DEEPIN_KF5_WAYLAND TRUE)
    message(STATUS "USE_DEEPIN_KF5_WAYLAND ${USE_DEEPIN_KF5_WAYLAND}")
else()
    set(USE_DEEPIN_KF5_WAYLAND FALSE)
    message(STATUS "USE_DEEPIN_KF5_WAYLAND ${USE_DEEPIN_KF5_WAYLAND}")
endif()

if(USE_DEEPIN_KF5_WAYLAND)
  add_definitions(-DUSE_DEEPIN_KF5_WAYLAND)
endif()

if (USE_DEEPIN_KF5_WAYLAND)
    message(STATUS "USE_DEEPIN_KF5_WAYLAND find_package")
    find_package(KF5Wayland REQUIRED)
else()
    find_package(X11 REQUIRED)
    message(STATUS "USE_DEEPIN_X11 find_package")
endif()

set(Qt_LIBS
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::DBus
    Qt5::Concurrent
    Qt5::Network
)
set(SOURCE_FILES
        cli/main.cpp
        cli/datastruct.h
        cli/input.h
        cli/input.cpp
        cli/tcpserver.h
        cli/tcpserver.cpp
        cli/config.h
        cli/config.cpp
        )
add_executable(${BIN_NAME}
    ${SOURCE_FILES}
)

set_target_properties(${BIN_NAME}
        PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS ON
)
target_include_directories(${BIN_NAME}
        PRIVATE
        ${SOURCE_FILES}
)

target_link_libraries(${BIN_NAME}
        ${Qt_LIBS}
)

if (USE_DEEPIN_KF5_WAYLAND)
    target_link_libraries(${BIN_NAME} KF5::WaylandClient)
    message(STATUS "USE_DEEPIN_KF5_WAYLAND target_link_libraries")
else()
    target_link_libraries(${BIN_NAME} X11)
    message(STATUS "USE_DEEPIN_X11 target_link_libraries")
endif()

INSTALL(TARGETS ${BIN_NAME} DESTINATION bin)
