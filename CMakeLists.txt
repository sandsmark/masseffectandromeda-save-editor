cmake_minimum_required(VERSION 3.5)

project(masseffectandromeda-save-editor LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)

add_executable(masseffectandromeda-save-editor
    main.cpp
    MainWindow.cpp
    SaveFile.cpp
    SaveFile.h
    MainWindow.h

    bits/bits.cpp
    bits/bits-stream.cpp
    )

target_link_libraries(masseffectandromeda-save-editor PRIVATE Qt5::Widgets)
