find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Multimedia Widgets Concurrent)
qt_standard_project_setup()
qt_add_executable(FramoryxDesktop WIN32 main.cpp controller.cpp audio_monitor.cpp platform.cpp lifecycle.cpp process_catalog.cpp game_safety.cpp telemetry.cpp)
target_compile_features(FramoryxDesktop PRIVATE cxx_std_20)
target_compile_definitions(FramoryxDesktop PRIVATE NOMINMAX)
target_include_directories(FramoryxDesktop PRIVATE ../source)
target_link_libraries(FramoryxDesktop PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::Multimedia Qt6::Widgets Qt6::Concurrent)
set_target_properties(FramoryxDesktop PROPERTIES OUTPUT_NAME Framoryx)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(FramoryxDesktop URI FramoryxUI VERSION 1.0
    RESOURCE_PREFIX / NO_RESOURCE_TARGET_PATH
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/FramoryxUI
    QML_FILES
    qml/Main.qml qml/Theme.qml qml/FButton.qml qml/FCard.qml qml/FCombo.qml qml/FToggle.qml
    qml/FSlider.qml qml/Metric.qml qml/FrameGraph.qml qml/OverlayPreview.qml qml/Backdrop.qml
    qml/OverlayReadouts.qml qml/ExternalOverlay.qml
    qml/Overview.qml qml/OverlayStudio.qml qml/AudioPage.qml qml/GamesPage.qml qml/RulesPage.qml qml/SettingsPage.qml qml/ChangesPage.qml
    )
qt_add_resources(FramoryxDesktop framoryx_local_types PREFIX / FILES qml/qmldir)
qt_add_resources(FramoryxDesktop framoryx_art PREFIX /art BASE ../assets FILES ../assets/icon.png ../assets/artwork.png ../assets/Framoryx.ico)
if(MSVC)
    target_compile_options(FramoryxDesktop PRIVATE /utf-8)
endif()
if(WIN32)
    target_link_libraries(FramoryxDesktop PRIVATE dwmapi advapi32 dxgi pdh wbemuuid ole32 oleaut32)
    target_sources(FramoryxDesktop PRIVATE resources/desktop.rc)
    target_include_directories(FramoryxDesktop PRIVATE ../assets)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XCB_PRESENT REQUIRED IMPORTED_TARGET xcb xcb-present)
    target_link_libraries(FramoryxDesktop PRIVATE PkgConfig::XCB_PRESENT)
    target_sources(FramoryxDesktop PRIVATE passive_linux.cpp noise_cache.cpp)
    qt_add_executable(FramoryxPresentScene present_scene.cpp)
    target_link_libraries(FramoryxPresentScene PRIVATE Qt6::Core PkgConfig::XCB_PRESENT)
    target_compile_features(FramoryxPresentScene PRIVATE cxx_std_20)
    install(TARGETS FramoryxPresentScene RUNTIME DESTINATION bin)
    target_sources(FramoryxDesktop PRIVATE ../source/vendor/miniz/miniz.c ../source/vendor/miniz/miniz_zip.c ../source/vendor/miniz/miniz_tinfl.c)
    target_compile_definitions(FramoryxDesktop PRIVATE MINIZ_NO_DEFLATE_APIS)
endif()
qt_add_executable(FramoryxScene scene.cpp)
if(MSVC)
    target_compile_options(FramoryxScene PRIVATE /utf-8)
endif()
target_link_libraries(FramoryxScene PRIVATE Qt6::Gui)
target_compile_features(FramoryxScene PRIVATE cxx_std_20)
install(TARGETS FramoryxDesktop FramoryxScene RUNTIME DESTINATION bin)
if(UNIX AND NOT APPLE)
    install(FILES resources/io.framoryx.Framoryx.desktop DESTINATION share/applications)
    install(FILES ../assets/icon.png DESTINATION share/icons/hicolor/256x256/apps RENAME io.framoryx.Framoryx.png)
endif()
add_test(NAME desktop-model COMMAND FramoryxDesktop --self-test --test-root ${CMAKE_CURRENT_BINARY_DIR}/model-test)
set_tests_properties(desktop-model PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 30)

qt_add_resources(FramoryxDesktop framoryx_changes PREFIX /releases BASE ../docs/releases FILES
    ../docs/releases/v0.3.0.md ../docs/releases/v0.2.3.md ../docs/releases/v0.2.2.md
    ../docs/releases/v0.2.1.md ../docs/releases/v0.2.0.md)

option(FRAMORYX_BUILD_DIAGNOSTICS "Build the isolated UI input driver" OFF)
if(FRAMORYX_BUILD_DIAGNOSTICS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_library(FramoryxUiChecks SHARED tests/input_driver.cpp)
    target_link_libraries(FramoryxUiChecks PRIVATE Qt6::Gui Qt6::Test)
endif()
