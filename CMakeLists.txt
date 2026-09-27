cmake_minimum_required(VERSION 3.21)
project(omarchy-fb-qt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Concurrent
    DBus
    Multimedia
)

qt_add_executable(omarchy-fb-qt
    src/main.cpp
    src/systemtheme.cpp
    src/systemtheme.h
    src/themeloader.cpp
    src/themeloader.h
    src/appconfig.cpp
    src/appconfig.h
    src/i18n.cpp
    src/i18n.h
    src/filesystemcontroller.cpp
    src/filesystemcontroller.h
    src/fileoperations.cpp
    src/fileoperations.h
    src/dirsizecalculator.cpp
    src/dirsizecalculator.h
    src/previewprovider.cpp
    src/previewprovider.h
    src/resources.qrc
)

target_include_directories(omarchy-fb-qt PRIVATE src)
target_compile_definitions(omarchy-fb-qt PRIVATE
    OMARCHY_FB_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(omarchy-fb-qt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Concurrent
    Qt6::DBus
    Qt6::Multimedia
)

install(TARGETS omarchy-fb-qt RUNTIME DESTINATION bin)
install(FILES packaging/omarchy-fb-qt.desktop
        DESTINATION share/applications)
install(FILES packaging/omarchy-fb-qt.svg
        DESTINATION share/icons/hicolor/scalable/apps)
install(FILES LICENSE
        DESTINATION share/licenses/omarchy-fb-qt)
install(DIRECTORY languages/
        DESTINATION share/omarchy-fb-qt/languages
        FILES_MATCHING PATTERN "*.json")
