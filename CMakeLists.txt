cmake_minimum_required(VERSION 3.21)

project(qsing-box VERSION 2.0.1-subscription LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set Qt6 installation path
set(CMAKE_PREFIX_PATH "C:/Qt/6.9.1/mingw_64")

find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools Network)
if(NOT Qt6_FOUND)
    message(FATAL_ERROR "Qt6 not found. Please install Qt6 or set Qt6_DIR to point to your Qt6 installation.")
endif()
qt_standard_project_setup()

add_subdirectory(src)

set(qsing-box_sources
    src/about_dialog.cpp
    src/about_dialog.ui
    src/main.cpp
    src/main_window.cpp
    src/main_window.ui
    src/settings_dialog.cpp
    src/settings_dialog.ui
    src/tray_icon.cpp
)

qt_add_executable(qsing-box
    ${qsing-box_sources}
    resources/app.rc
)

target_include_directories(qsing-box PRIVATE
    include
)

target_link_libraries(qsing-box PRIVATE
    Qt6::Widgets
    Qt6::Network
    config
    proxy
    settings
    utils
)

set_target_properties(qsing-box PROPERTIES
    WIN32_EXECUTABLE ON
)

# Deploy Qt6 DLLs and plugins on Windows
if(WIN32)
    # Copy essential Qt6 DLLs to output directory
    add_custom_command(TARGET qsing-box POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_PREFIX_PATH}/bin/Qt6Core.dll"
        "${CMAKE_PREFIX_PATH}/bin/Qt6Gui.dll"
        "${CMAKE_PREFIX_PATH}/bin/Qt6Widgets.dll"
        "${CMAKE_PREFIX_PATH}/bin/Qt6Network.dll"
        "${CMAKE_PREFIX_PATH}/bin/libgcc_s_seh-1.dll"
        "${CMAKE_PREFIX_PATH}/bin/libstdc++-6.dll"
        "${CMAKE_PREFIX_PATH}/bin/libwinpthread-1.dll"
        $<TARGET_FILE_DIR:qsing-box>
        COMMENT "Copying Qt6 DLLs and MinGW runtime"
    )

    # Copy network TLS plugin if it exists
    add_custom_command(TARGET qsing-box POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:qsing-box>/networkinformation
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_PREFIX_PATH}/plugins/networkinformation/qnetworklistmanager.dll"
        $<TARGET_FILE_DIR:qsing-box>/networkinformation/
        COMMENT "Copying Qt6 network plugins"
        COMMAND_EXPAND_LISTS
    )

    # Create platforms directory and copy platform plugins
    add_custom_command(TARGET qsing-box POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:qsing-box>/platforms
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_PREFIX_PATH}/plugins/platforms/qwindows.dll"
        $<TARGET_FILE_DIR:qsing-box>/platforms/
        COMMENT "Copying Qt6 platform plugins"
    )

    # Copy styles plugins if they exist
    add_custom_command(TARGET qsing-box POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:qsing-box>/styles
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_PREFIX_PATH}/plugins/styles/qmodernwindowsstyle.dll"
        $<TARGET_FILE_DIR:qsing-box>/styles/
        COMMENT "Copying Qt6 style plugins"
    )
endif()

set(qsing-box_resources
    "resources/images/app.ico"
    "resources/images/app_enable_proxy.ico"
    "resources/images/status_enabled.png"
    "resources/images/status_disabled.png"
    "resources/images/about.png"
    "resources/images/add-new.png"
    "resources/images/delete.png"
    "resources/images/edit.png"
    "resources/images/import.png"
    "resources/images/save.png"
    "resources/images/setting.png"
    "resources/images/start.png"
    "resources/images/stop.png"
    "resources/images/switch.png"
    "resources/images/qt_logo.png"
    "resources/texts/gpl-3.0.txt"
    "resources/xml/task.xml"
)

qt_add_resources(qsing-box res
    PREFIX "/"
    BASE "resources"
    FILES
        ${qsing-box_resources}
)

qt_add_translations(qsing-box
    TS_FILES
        languages/qsing-box_zh_CN.ts
        languages/qsing-box_en_US.ts
    LUPDATE_OPTIONS
        -no-obsolete
)
# ---- TLS backend plugin ----------------------------------------------------
set(QT_TLS_PLUGIN_DIR "${CMAKE_PREFIX_PATH}/plugins/tls")

add_custom_command(TARGET qsing-box POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:qsing-box>/tls
        # copy OpenSSL backend; add Schannel or cert-only if you want fallbacks
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_TLS_PLUGIN_DIR}/qopensslbackend.dll"
        $<TARGET_FILE_DIR:qsing-box>/tls/
        COMMENT "Copying Qt TLS backend plugin (OpenSSL)"
)

set(OPENSSL_DLL_PATH "C:/Program Files/OpenSSL-Win64/bin")
add_custom_command(TARGET qsing-box POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENSSL_DLL_PATH}/libssl-3-x64.dll"
        "${OPENSSL_DLL_PATH}/libcrypto-3-x64.dll"
        $<TARGET_FILE_DIR:qsing-box>
        COMMENT "Copying OpenSSL 3 runtime"
)

string(TIMESTAMP BUILD_TIME "%Y.%m.%d %H:%M")
add_definitions(-DPROJECT_VERSION="${PROJECT_VERSION}")
add_definitions(-DBUILD_TIME="${BUILD_TIME}")
add_definitions(-DCOMPILER_ID="${CMAKE_CXX_COMPILER_ID}")
add_definitions(-DCOMPILER_VERSION="${CMAKE_CXX_COMPILER_VERSION}")
