cmake_minimum_required(VERSION 3.20)
project(blackarch-launcher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Gui Qml Quick QuickControls2 Svg
)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(blackarch-launcher
    src/main.cpp
    src/backend.h
    src/backend.cpp
    src/desktopparser.h
    src/desktopparser.cpp
    src/toolmodel.h
    src/toolmodel.cpp
)

# Bundle QML and assets into the binary via Qt's resource system.
# This makes the program a single self-contained file at runtime.
qt_add_qml_module(blackarch-launcher
    URI BlackArch
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/BackgroundCarousel.qml
        qml/BackgroundManager.qml
        qml/FallbackBackground.qml
        qml/GlassPane.qml
        qml/TitleBar.qml
        qml/ToolTree.qml
        qml/DetailsPane.qml
        qml/CornerSprite.qml
)

target_link_libraries(blackarch-launcher PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Svg
)

# Let the binary know where its installed assets live, so --install can copy
# them to the user's home directory on first run.
set(ASSETS_INSTALL_DIR "${CMAKE_INSTALL_FULL_DATADIR}/blackarch-launcher")
target_compile_definitions(blackarch-launcher PRIVATE
    ASSETS_INSTALL_DIR="${ASSETS_INSTALL_DIR}"
)

# ── Install ──────────────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS blackarch-launcher
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES packaging/blackarch-launcher.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
install(FILES packaging/blackarch-launcher.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

# Assets — installed under /usr/share/blackarch-launcher/ so --install can
# discover and copy them to the user's XDG cache / data dirs.
install(DIRECTORY assets/
    DESTINATION ${ASSETS_INSTALL_DIR}
    FILES_MATCHING
    PATTERN "*.png"
    PATTERN "*.svg"
    PATTERN "*.ico"
    PATTERN "README.md" EXCLUDE
)
