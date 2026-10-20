cmake_minimum_required(VERSION 3.24)

set(NUMTILO_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
if (NOT EXISTS "${NUMTILO_VERSION_FILE}")
    message(FATAL_ERROR "VERSION file not found: ${NUMTILO_VERSION_FILE}")
endif()

file(READ "${NUMTILO_VERSION_FILE}" NUMTILO_VERSION_RAW)
string(STRIP "${NUMTILO_VERSION_RAW}" NUMTILO_PROJECT_VERSION)

if (NOT NUMTILO_PROJECT_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must follow SemVer format X.Y.Z, got: ${NUMTILO_PROJECT_VERSION}")
endif()

# The vcpkg toolchain installs manifest dependencies during the first project()
# call, so its feature selection must already reflect the requested build.
option(NUMTILO_BUILD_APP "Build the SFML desktop application and runtime package" ON)
option(NUMTILO_BUILD_UI_ACCEPTANCE "Build graphical UI acceptance (requires a display)" OFF)
option(BUILD_TESTING "Build the testing tree." ON)

foreach(NUMTILO_REQUESTED_FEATURE IN LISTS VCPKG_MANIFEST_FEATURES)
    if (NOT NUMTILO_REQUESTED_FEATURE STREQUAL "desktop" AND
        NOT NUMTILO_REQUESTED_FEATURE STREQUAL "tests")
        message(FATAL_ERROR
            "Unknown VCPKG_MANIFEST_FEATURES entry '${NUMTILO_REQUESTED_FEATURE}'. "
            "Use NUMTILO_BUILD_APP and BUILD_TESTING to select project dependencies."
        )
    endif()
endforeach()
unset(NUMTILO_REQUESTED_FEATURE)

foreach(NUMTILO_INSTALL_OPTION IN LISTS VCPKG_INSTALL_OPTIONS)
    # vcpkg treats option names case-insensitively. Inspect a normalized copy
    # while preserving the original argument and diagnostic.
    string(TOLOWER "${NUMTILO_INSTALL_OPTION}" NUMTILO_INSTALL_OPTION_LOWER)
    # vcpkg expands @response files into arguments, which could hide feature
    # switches from this check. Keep feature selection in the project options.
    if (NUMTILO_INSTALL_OPTION_LOWER MATCHES "^--(x-)?(feature|no-default-features)(=|$)" OR
        NUMTILO_INSTALL_OPTION_LOWER MATCHES "^@")
        message(FATAL_ERROR
            "VCPKG_INSTALL_OPTIONS entry '${NUMTILO_INSTALL_OPTION}' is a feature override "
            "or response file that can bypass managed manifest feature selection. "
            "Use NUMTILO_BUILD_APP and BUILD_TESTING instead."
        )
    endif()
endforeach()
unset(NUMTILO_INSTALL_OPTION)
unset(NUMTILO_INSTALL_OPTION_LOWER)

set(NUMTILO_MANIFEST_FEATURES "")
if (NUMTILO_BUILD_APP)
    list(APPEND NUMTILO_MANIFEST_FEATURES desktop)
endif()
if (BUILD_TESTING)
    list(APPEND NUMTILO_MANIFEST_FEATURES tests)
endif()
# These two cache entries are derived, not independent user build options.
# FORCE prevents a previous ON configuration from retaining unwanted features.
set(VCPKG_MANIFEST_FEATURES "${NUMTILO_MANIFEST_FEATURES}" CACHE STRING
    "Manifest features derived from NUMTILO_BUILD_APP and BUILD_TESTING" FORCE)
set(VCPKG_MANIFEST_NO_DEFAULT_FEATURES ON CACHE BOOL
    "Select project manifest features explicitly from the build options" FORCE)
# A parent or initial-cache script may also define normal variables. Under
# CMP0126 those would otherwise shadow the recomputed cache entries.
set(VCPKG_MANIFEST_FEATURES "${NUMTILO_MANIFEST_FEATURES}")
set(VCPKG_MANIFEST_NO_DEFAULT_FEATURES ON)
unset(NUMTILO_MANIFEST_FEATURES)

project(numtilo VERSION ${NUMTILO_PROJECT_VERSION} LANGUAGES CXX)

if (NUMTILO_BUILD_UI_ACCEPTANCE AND (NOT NUMTILO_BUILD_APP OR NOT BUILD_TESTING))
    message(FATAL_ERROR "UI acceptance requires NUMTILO_BUILD_APP=ON and BUILD_TESTING=ON")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep BOM-free source literals and their execution encoding independent of the
# Windows host code page, including when project warnings are disabled.
if (MSVC)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
endif()

option(NUMTILO_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(NUMTILO_ENABLE_SANITIZERS "Enable AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(NUMTILO_ENABLE_COVERAGE "Enable coverage instrumentation (GCC/Clang)" OFF)
if (APPLE)
    option(NUMTILO_MACOS_BUNDLE "Build the macOS application bundle" ON)
else()
    option(NUMTILO_MACOS_BUNDLE "Build the macOS application bundle" OFF)
endif()

include(CTest)
# Regenerate the test index even when BUILD_TESTING is switched OFF in an existing
# build tree. Test targets and registrations remain conditional below.
enable_testing()

set(NUMTILO_USING_VCPKG OFF)
if (DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    set(NUMTILO_USING_VCPKG ON)
endif()

function(enable_project_warnings target_name)
    if (NOT NUMTILO_ENABLE_WARNINGS)
        return()
    endif()

    if (MSVC)
        target_compile_options(${target_name} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:/W4>"
            "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:/permissive->"
        )
    else()
        target_compile_options(${target_name} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wall>"
            "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wextra>"
            "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wpedantic>"
        )
    endif()
endfunction()

function(enable_project_sanitizers target_name)
    if (NOT NUMTILO_ENABLE_SANITIZERS)
        return()
    endif()

    if (MSVC)
        message(WARNING "Sanitizers are not enabled for MSVC in this project.")
        return()
    endif()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            ${target_name}
            PRIVATE
                "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-fsanitize=address,undefined>"
                "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-fno-omit-frame-pointer>"
        )
        target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

function(enable_project_coverage target_name)
    if (NOT NUMTILO_ENABLE_COVERAGE)
        return()
    endif()

    if (MSVC)
        message(WARNING "Coverage instrumentation is not enabled for MSVC in this project.")
        return()
    endif()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target_name} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-O0>"
            "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-g>"
            "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:--coverage>"
        )
        target_link_options(${target_name} PRIVATE --coverage)
    endif()
endfunction()

if (NUMTILO_BUILD_APP)
    find_package(SFML 2.5 COMPONENTS graphics window system audio CONFIG QUIET)
    if (NOT SFML_FOUND)
        find_package(SFML 2.5 COMPONENTS graphics window system audio REQUIRED)
    endif()

    if (NUMTILO_USING_VCPKG)
        find_package(fmt CONFIG REQUIRED)
    else()
        find_package(fmt CONFIG QUIET)
    endif()
    find_package(OpenAL QUIET)
endif()

find_package(nlohmann_json CONFIG QUIET)
if (NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(game_core STATIC
    src/core/Board.cpp
    src/core/Game.cpp
    src/core/ScoreManager.cpp
    src/core/ScoreStorage.cpp
    src/core/FileStorage.cpp
    src/core/detail/ProfileValidation.cpp
)

target_include_directories(game_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(game_core PUBLIC nlohmann_json::nlohmann_json)

enable_project_warnings(game_core)
enable_project_sanitizers(game_core)
enable_project_coverage(game_core)

add_library(app_paths STATIC src/app/AppPaths.cpp)
target_include_directories(app_paths PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
enable_project_warnings(app_paths)
enable_project_sanitizers(app_paths)
enable_project_coverage(app_paths)

add_library(asset_resolver STATIC src/app/AssetResolver.cpp)
target_include_directories(asset_resolver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(asset_resolver PUBLIC app_paths)
enable_project_warnings(asset_resolver)
enable_project_sanitizers(asset_resolver)
enable_project_coverage(asset_resolver)

add_library(asset_locations STATIC src/app/AssetLocations.cpp)
target_include_directories(asset_locations PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(asset_locations PUBLIC app_paths)
if (APPLE)
    enable_language(OBJCXX)
    set_target_properties(asset_locations PROPERTIES OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON
                                                    OBJCXX_EXTENSIONS OFF)
    target_sources(asset_locations PRIVATE src/app/platform/AssetLocationsMac.mm)
    find_library(ASSET_LOCATIONS_FOUNDATION_FRAMEWORK Foundation REQUIRED)
    target_link_libraries(asset_locations PRIVATE ${ASSET_LOCATIONS_FOUNDATION_FRAMEWORK})
elseif (WIN32)
    target_sources(asset_locations PRIVATE src/app/platform/AssetLocationsWindows.cpp)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(asset_locations PRIVATE src/app/platform/AssetLocationsLinux.cpp)
endif()
enable_project_warnings(asset_locations)
enable_project_sanitizers(asset_locations)
enable_project_coverage(asset_locations)

add_library(user_data_paths STATIC src/app/UserDataPaths.cpp)
target_include_directories(user_data_paths PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(user_data_paths PUBLIC app_paths)
if (APPLE)
    enable_language(OBJCXX)
    set_target_properties(user_data_paths PROPERTIES OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON
                                                   OBJCXX_EXTENSIONS OFF)
    target_sources(user_data_paths PRIVATE src/app/platform/UserDataMac.mm)
    find_library(USER_DATA_FOUNDATION_FRAMEWORK Foundation REQUIRED)
    target_link_libraries(user_data_paths PRIVATE ${USER_DATA_FOUNDATION_FRAMEWORK})
elseif (WIN32)
    target_sources(user_data_paths PRIVATE src/app/platform/UserDataWindows.cpp)
    target_link_libraries(user_data_paths PRIVATE shell32 ole32 uuid)
else()
    target_sources(user_data_paths PRIVATE src/app/platform/UserDataLinux.cpp)
endif()
enable_project_warnings(user_data_paths)
enable_project_sanitizers(user_data_paths)
enable_project_coverage(user_data_paths)

add_library(app_settings STATIC src/app/SettingsStore.cpp src/app/detail/SettingsValidation.cpp)
target_include_directories(app_settings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(app_settings PUBLIC game_core)
enable_project_warnings(app_settings)
enable_project_sanitizers(app_settings)
enable_project_coverage(app_settings)

add_library(profile_migration STATIC src/app/ProfileMigration.cpp)
target_include_directories(profile_migration PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(profile_migration PUBLIC app_paths app_settings)
enable_project_warnings(profile_migration)
enable_project_sanitizers(profile_migration)
enable_project_coverage(profile_migration)

add_library(runtime_check STATIC src/app/RuntimeCheck.cpp)
target_include_directories(runtime_check PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(runtime_check PUBLIC asset_locations asset_resolver user_data_paths
                                           nlohmann_json::nlohmann_json)
enable_project_warnings(runtime_check)
enable_project_sanitizers(runtime_check)
enable_project_coverage(runtime_check)

# Gameplay/session coordination is compiled once for both desktop and unit consumers.
add_library(app_logic STATIC src/app/GameSession.cpp src/app/ScoreCoordinator.cpp)
target_include_directories(app_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(app_logic PUBLIC game_core)
enable_project_warnings(app_logic)
enable_project_sanitizers(app_logic)
enable_project_coverage(app_logic)

include(GNUInstallDirs)

install(
    TARGETS game_core
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development EXCLUDE_FROM_ALL
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development EXCLUDE_FROM_ALL
)

if (NUMTILO_BUILD_APP)
    set(NUMTILO_UI_SOURCES
        src/app/ui/UiPrimitives.cpp
        src/app/ui/SplashScene.cpp
        src/app/ui/HighScoresScene.cpp
        src/app/ui/PlayingScene.cpp
        src/app/ui/GameOverScene.cpp
        src/app/ui/WinScene.cpp
    )
    add_library(app_accessibility STATIC src/app/ui/Accessibility.cpp)
    target_include_directories(app_accessibility PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if (APPLE)
        target_sources(app_accessibility PRIVATE src/app/platform/AccessibilityMac.mm)
        set_target_properties(app_accessibility PROPERTIES OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON)
        set_source_files_properties(src/app/platform/AccessibilityMac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
        find_library(ACCESSIBILITY_APPKIT_FRAMEWORK AppKit REQUIRED)
        target_link_libraries(app_accessibility PRIVATE ${ACCESSIBILITY_APPKIT_FRAMEWORK})
    else()
        target_sources(app_accessibility PRIVATE src/app/platform/AccessibilityStub.cpp)
    endif()
    enable_project_warnings(app_accessibility)
    enable_project_sanitizers(app_accessibility)
    enable_project_coverage(app_accessibility)
    add_executable(numtilo src/app/main.cpp src/app/SoundManager.cpp src/app/App.cpp
                            ${NUMTILO_UI_SOURCES})
    if (WIN32)
        enable_language(RC)
        # Keep the RC text and referenced filename ASCII even when the source
        # checkout path contains spaces or Unicode. COPYONLY preserves the ICO.
        configure_file(packaging/windows/Numtilo.ico
                       "${CMAKE_CURRENT_BINARY_DIR}/Numtilo.ico" COPYONLY)
        # Changing the ICO also changes the RC source for generators that do
        # not implement OBJECT_DEPENDS, including Visual Studio.
        file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/Numtilo.ico"
             NUMTILO_WINDOWS_ICON_SHA256)
        configure_file(packaging/windows/Numtilo.rc.in
                       "${CMAKE_CURRENT_BINARY_DIR}/Numtilo.rc" @ONLY)
        set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/Numtilo.rc" PROPERTIES
            INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}"
            OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/Numtilo.ico"
        )
        target_sources(numtilo PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Numtilo.rc")
    endif()
    target_link_libraries(numtilo PRIVATE app_accessibility)

    target_link_libraries(numtilo PRIVATE app_logic profile_migration runtime_check)

    if (WIN32 OR (APPLE AND NUMTILO_MACOS_BUNDLE))
        set_target_properties(numtilo PROPERTIES OUTPUT_NAME "Numtilo")
    else()
        set_target_properties(numtilo PROPERTIES OUTPUT_NAME "numtilo")
    endif()

    if (APPLE AND NUMTILO_MACOS_BUNDLE)
        set(NUMTILO_MACOS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Numtilo.icns")
        set_source_files_properties("${NUMTILO_MACOS_ICON}" PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources
        )
        target_sources(numtilo PRIVATE "${NUMTILO_MACOS_ICON}")
        set_target_properties(numtilo PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in"
            MACOSX_BUNDLE_ICON_FILE "Numtilo.icns"
            MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.enesgoker.numtilo"
            MACOSX_BUNDLE_BUNDLE_NAME "Numtilo"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
            INSTALL_RPATH "@executable_path/../Frameworks"
        )
    elseif (UNIX AND NOT APPLE)
        set_target_properties(numtilo PROPERTIES INSTALL_RPATH "$ORIGIN")
    endif()

    if (TARGET SFML::Graphics)
        target_link_libraries(numtilo PRIVATE SFML::Graphics SFML::Window SFML::System SFML::Audio)
    elseif (TARGET sfml-graphics)
        target_link_libraries(numtilo PRIVATE sfml-graphics sfml-window sfml-system sfml-audio)
    else()
        target_link_libraries(numtilo PRIVATE ${SFML_LIBRARIES})
    endif()

    if (TARGET OpenAL::OpenAL)
        target_link_libraries(numtilo PRIVATE OpenAL::OpenAL)
    elseif (OPENAL_LIBRARY)
        target_link_libraries(numtilo PRIVATE ${OPENAL_LIBRARY})
    endif()

    if (APPLE)
        find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox)
        find_library(COREAUDIO_FRAMEWORK CoreAudio)
        find_library(AUDIOUNIT_FRAMEWORK AudioUnit)
        find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
        find_library(FOUNDATION_FRAMEWORK Foundation)

        foreach(_fw IN ITEMS AUDIOTOOLBOX_FRAMEWORK COREAUDIO_FRAMEWORK AUDIOUNIT_FRAMEWORK
                            COREFOUNDATION_FRAMEWORK FOUNDATION_FRAMEWORK)
            if (${_fw})
                target_link_libraries(numtilo PRIVATE ${${_fw}})
            endif()
        endforeach()
    endif()

    if (TARGET fmt::fmt)
        target_link_libraries(numtilo PRIVATE fmt::fmt)
    elseif (TARGET fmt::fmt-header-only)
        target_link_libraries(numtilo PRIVATE fmt::fmt-header-only)
    endif()

    enable_project_warnings(numtilo)
    enable_project_sanitizers(numtilo)
    enable_project_coverage(numtilo)

    # Refresh assets even when no C++ file changed, including removal of stale files.
    if (APPLE AND NUMTILO_MACOS_BUNDLE)
        set(NUMTILO_BUILD_ASSET_DIR "$<TARGET_BUNDLE_CONTENT_DIR:numtilo>/Resources/assets")
        set(NUMTILO_INSTALL_ASSET_PARENT "$<TARGET_BUNDLE_DIR_NAME:numtilo>/Contents/Resources")
    else()
        set(NUMTILO_BUILD_ASSET_DIR "$<TARGET_FILE_DIR:numtilo>/assets")
        set(NUMTILO_INSTALL_ASSET_PARENT ".")
    endif()
    add_custom_target(numtilo_assets ALL
        COMMAND ${CMAKE_COMMAND}
                "-DNUMTILO_ASSET_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/assets"
                "-DNUMTILO_ASSET_DESTINATION=${NUMTILO_BUILD_ASSET_DIR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SyncAssets.cmake"
        COMMENT "Synchronizing runtime assets"
        VERBATIM
    )
    add_dependencies(numtilo numtilo_assets)

    if (APPLE AND NUMTILO_MACOS_BUNDLE)
        install(TARGETS numtilo BUNDLE DESTINATION . COMPONENT Runtime)
    else()
        install(TARGETS numtilo
            RUNTIME_DEPENDENCY_SET numtilo_runtime_deps
            RUNTIME DESTINATION . COMPONENT Runtime
        )
        install(RUNTIME_DEPENDENCY_SET numtilo_runtime_deps
            PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
                "$<$<PLATFORM_ID:Windows>:^hvsifiletrust\\.dll$>"
            POST_EXCLUDE_REGEXES ".*/[Ss][Yy][Ss][Tt][Ee][Mm]32/.*\\.dll" ".*/usr/lib/.*" ".*/System/Library/.*"
            RUNTIME DESTINATION . COMPONENT Runtime
            LIBRARY DESTINATION . COMPONENT Runtime
        )
    endif()

    configure_file(cmake/CleanInstalledAssets.cmake.in
                   "${CMAKE_CURRENT_BINARY_DIR}/CleanInstalledAssets.cmake.in" @ONLY)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/CleanInstalledAssets-$<CONFIG>.cmake"
                  INPUT "${CMAKE_CURRENT_BINARY_DIR}/CleanInstalledAssets.cmake.in")
    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/CleanInstalledAssets-$<CONFIG>.cmake" COMPONENT Runtime)
    install(DIRECTORY assets DESTINATION "${NUMTILO_INSTALL_ASSET_PARENT}" COMPONENT Runtime
            PATTERN ".DS_Store" EXCLUDE PATTERN "._*" EXCLUDE)
    if (APPLE AND NUMTILO_MACOS_BUNDLE)
        install(FILES packaging/macos/README.md packaging/macos/README.tr.md
                DESTINATION . COMPONENT Runtime)
    else()
        install(FILES README.md README.tr.md DESTINATION . COMPONENT Runtime)
    endif()
    install(FILES LICENSE THIRD_PARTY_NOTICES.md CHANGELOG.md VERSION
            DESTINATION . COMPONENT Runtime)

    if (APPLE AND NUMTILO_MACOS_BUNDLE)
        # BundleUtilities must run on the installed copy, not at configure/build time.
        # Resolve @rpath prerequisites from the original imported library directories.
        set(NUMTILO_BUNDLE_LIBRARY_DIRS "$<TARGET_FILE_DIR:numtilo>")
        foreach(_dependency IN ITEMS SFML::Graphics SFML::Window SFML::System SFML::Audio
                                     sfml-graphics sfml-window sfml-system sfml-audio
                                     OpenAL::OpenAL fmt::fmt)
            if (TARGET ${_dependency})
                get_target_property(_dependency_type ${_dependency} TYPE)
                if (NOT _dependency_type STREQUAL "INTERFACE_LIBRARY")
                    list(APPEND NUMTILO_BUNDLE_LIBRARY_DIRS "$<TARGET_FILE_DIR:${_dependency}>")
                endif()
            endif()
        endforeach()
        configure_file(cmake/FixupMacBundle.cmake.in
                       "${CMAKE_CURRENT_BINARY_DIR}/FixupMacBundle.cmake.in" @ONLY)
        file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/FixupMacBundle-$<CONFIG>.cmake"
                      INPUT "${CMAKE_CURRENT_BINARY_DIR}/FixupMacBundle.cmake.in")
        install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/FixupMacBundle-$<CONFIG>.cmake" COMPONENT Runtime)
    endif()
endif()

if (BUILD_TESTING)
    find_package(Catch2 3 CONFIG QUIET)
    if (NOT Catch2_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.7.1
        )
        FetchContent_MakeAvailable(Catch2)
    endif()

    add_executable(core_unit_tests
        tests/app_paths_tests.cpp
        tests/asset_paths_tests.cpp
        tests/asset_resolver_tests.cpp
        tests/runtime_check_tests.cpp
        tests/user_data_paths_tests.cpp
        tests/settings_store_tests.cpp
        tests/profile_migration_tests.cpp
        tests/profile_validation_tests.cpp
        tests/core_unit_tests.cpp
        tests/board_move_tests.cpp
        tests/board_move_properties.cpp
        tests/board_state_tests.cpp
        tests/game_spawn_tests.cpp
        tests/win_state_tests.cpp
        tests/score_memory_tests.cpp
        tests/score_schema_tests.cpp
        tests/score_storage_tests.cpp
        tests/score_coordinator_tests.cpp
        tests/score_acceptance_tests.cpp
        tests/game_session_tests.cpp
        tests/gameplay_outcome_tests.cpp
    )

    target_link_libraries(core_unit_tests PRIVATE app_logic profile_migration runtime_check
                                                Catch2::Catch2WithMain)
    enable_project_warnings(core_unit_tests)
    enable_project_sanitizers(core_unit_tests)
    enable_project_coverage(core_unit_tests)

    add_test(
        NAME core_unit_tests
        COMMAND core_unit_tests
    )
    set_tests_properties(core_unit_tests PROPERTIES LABELS "unit")

    if (NUMTILO_BUILD_APP)
        # Queue/lifecycle checks use a test-local platform, without a native window.
        add_executable(accessibility_unit_tests
            tests/accessibility_tests.cpp src/app/ui/Accessibility.cpp)
        target_include_directories(accessibility_unit_tests PRIVATE src)
        target_link_libraries(accessibility_unit_tests PRIVATE Catch2::Catch2WithMain)
        enable_project_warnings(accessibility_unit_tests)
        enable_project_sanitizers(accessibility_unit_tests)
        enable_project_coverage(accessibility_unit_tests)
        add_test(NAME accessibility_unit_tests COMMAND accessibility_unit_tests)
        set_tests_properties(accessibility_unit_tests PROPERTIES LABELS "unit" TIMEOUT 30)

        # This worker exercises native profile storage using only harness-owned
        # temporary directories. It is not installed or included in Runtime packages.
        add_executable(platform_acceptance_worker tests/platform_acceptance_worker.cpp)
        target_link_libraries(platform_acceptance_worker PRIVATE profile_migration user_data_paths
                                                                asset_locations
                                                                nlohmann_json::nlohmann_json)
        if (WIN32)
            target_link_libraries(platform_acceptance_worker PRIVATE shell32)
        endif()
        enable_project_warnings(platform_acceptance_worker)
        enable_project_sanitizers(platform_acceptance_worker)
        enable_project_coverage(platform_acceptance_worker)

        find_package(Python3 COMPONENTS Interpreter QUIET)
        if (Python3_Interpreter_FOUND AND NOT CMAKE_CROSSCOMPILING)
            add_test(
                NAME platform_acceptance
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_platform.py"
                        --executable "$<TARGET_FILE:numtilo>"
                        --worker "$<TARGET_FILE:platform_acceptance_worker>"
                        --output "${CMAKE_CURRENT_BINARY_DIR}/platform-acceptance-ci"
                        --mode build
            )
            set_tests_properties(platform_acceptance PROPERTIES LABELS "platform" TIMEOUT 180)
            if (NUMTILO_BUILD_UI_ACCEPTANCE)
                add_executable(ui_acceptance tests/ui_acceptance.cpp ${NUMTILO_UI_SOURCES})
                target_link_libraries(ui_acceptance PRIVATE app_logic app_accessibility)
                if (TARGET SFML::Graphics)
                    target_link_libraries(ui_acceptance PRIVATE SFML::Graphics SFML::Window SFML::System)
                elseif (TARGET sfml-graphics)
                    target_link_libraries(ui_acceptance PRIVATE sfml-graphics sfml-window sfml-system)
                else()
                    target_link_libraries(ui_acceptance PRIVATE ${SFML_LIBRARIES})
                endif()
                enable_project_warnings(ui_acceptance)
                enable_project_sanitizers(ui_acceptance)
                enable_project_coverage(ui_acceptance)
                add_test(NAME ui_acceptance
                    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_ui.py"
                            --worker "$<TARGET_FILE:ui_acceptance>"
                            --font "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Inter-Variable.ttf"
                            --output "${CMAKE_CURRENT_BINARY_DIR}/ui-acceptance")
                set_tests_properties(ui_acceptance PROPERTIES LABELS "ui" TIMEOUT 180 RUN_SERIAL TRUE)
            endif()
        elseif (CMAKE_CROSSCOMPILING)
            if (NUMTILO_BUILD_UI_ACCEPTANCE)
                message(FATAL_ERROR "UI acceptance requires a native build")
            endif()
            message(STATUS "Native platform acceptance disabled for cross-compilation")
        else()
            if (NUMTILO_BUILD_UI_ACCEPTANCE)
                message(FATAL_ERROR "UI acceptance requires Python3")
            endif()
            message(STATUS "Python3 interpreter unavailable: native platform acceptance is not registered")
        endif()
    endif()
endif()

if (NUMTILO_BUILD_APP)
    set(CPACK_PACKAGE_NAME "Numtilo")
    set(CPACK_PACKAGE_VENDOR "Numtilo Project Contributors")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Numtilo, a 2048 desktop game in C++ with SFML")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    set(CPACK_GENERATOR "ZIP")
    # Source distribution has a separate ZIP-only policy; binary settings stay
    # independent and never acquire source selection or verification hooks.
    set(CPACK_SOURCE_GENERATOR "ZIP")
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" NUMTILO_SYSTEM_NAME)
    set(
        CPACK_PACKAGE_FILE_NAME
        "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${NUMTILO_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
    )
    set(CPACK_SOURCE_IGNORE_FILES "/build/;/\\.git/;\\.DS_Store")

    include(CPack)
    configure_file(cmake/StageSourcePackage.cmake.in
                   "${CMAKE_CURRENT_BINARY_DIR}/StageSourcePackage.cmake" @ONLY)
    configure_file(cmake/VerifySourcePackage.cmake.in
                   "${CMAKE_CURRENT_BINARY_DIR}/VerifySourcePackage.cmake" @ONLY)
    # Override only the source config after CPack writes both configurations.
    # Keep these raw-copy inputs defined but empty: CPack 3.24 checks for a
    # defined install method before its staging engine skips empty methods.
    file(APPEND "${CPACK_SOURCE_OUTPUT_CONFIG_FILE}"
        "\n# Source packages must use the shared tracked-file selector.\n"
        "set(CPACK_INSTALLED_DIRECTORIES \"\")\n"
        "set(CPACK_INSTALL_CMAKE_PROJECTS \"\")\n"
        "set(CPACK_INSTALL_COMMANDS \"\")\n"
        "set(CPACK_INSTALL_SCRIPTS [==[${CMAKE_CURRENT_BINARY_DIR}/StageSourcePackage.cmake]==])\n"
        "set(CPACK_POST_BUILD_SCRIPTS [==[${CMAKE_CURRENT_BINARY_DIR}/VerifySourcePackage.cmake]==])\n"
    )
else()
    # CMake uses existing CPack configuration files to generate package targets.
    # Remove only our configure-time scripts when an existing desktop build is
    # switched to headless; previously built binaries/assets/packages are retained.
    file(GLOB NUMTILO_STALE_DESKTOP_SCRIPTS LIST_DIRECTORIES FALSE
        "${CMAKE_CURRENT_BINARY_DIR}/CleanInstalledAssets-*.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/FixupMacBundle-*.cmake"
    )
    file(REMOVE
        "${CMAKE_CURRENT_BINARY_DIR}/CPackConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/CPackSourceConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/CleanInstalledAssets.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/FixupMacBundle.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/StageSourcePackage.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/VerifySourcePackage.cmake"
        ${NUMTILO_STALE_DESKTOP_SCRIPTS}
    )
    unset(NUMTILO_STALE_DESKTOP_SCRIPTS)
endif()
