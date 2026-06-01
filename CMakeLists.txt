cmake_minimum_required(VERSION 3.16)

project(LvglFontViewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    lvglfontparser.cpp
    lvglfontparser.h
    fontgridwidget.cpp
    fontgridwidget.h
    fontpreviewwidget.cpp
    fontpreviewwidget.h
    fonteditorwidget.cpp
    fonteditorwidget.h
    colorselectorwidget.cpp
    colorselectorwidget.h
    bitreader.cpp
    bitreader.h
    rledecompressor.cpp
    rledecompressor.h
    rlecompressor.cpp
    rlecompressor.h
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
