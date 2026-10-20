# Opt-in evidence provider for scripts/verify_headless.py. Load this file through
# CMAKE_PROJECT_TOP_LEVEL_INCLUDES; it does not replace any allowed dependency.
if (NOT DEFINED NUMTILO_BUILD_APP OR NUMTILO_BUILD_APP)
    message(FATAL_ERROR "Headless dependency audit requires NUMTILO_BUILD_APP=OFF")
endif()
if (NOT DEFINED HEADLESS_AUDIT_DIR OR NOT IS_ABSOLUTE "${HEADLESS_AUDIT_DIR}")
    message(FATAL_ERROR "Headless dependency audit requires an absolute HEADLESS_AUDIT_DIR")
endif()

function(_numtilo_hda_json_string input output)
    set(_numtilo_hda_escaped "${input}")
    string(REPLACE [[\]] [[\\]] _numtilo_hda_escaped "${_numtilo_hda_escaped}")
    string(REPLACE [["]] [[\"]] _numtilo_hda_escaped "${_numtilo_hda_escaped}")
    # JSON requires every U+0001..U+001F character to be escaped. CMake strings
    # cannot contain NUL, so there is no U+0000 value to preserve here.
    set(_numtilo_hda_hex "0123456789abcdef")
    foreach(_numtilo_hda_code RANGE 1 31)
        string(ASCII ${_numtilo_hda_code} _numtilo_hda_control)
        math(EXPR _numtilo_hda_high "${_numtilo_hda_code} / 16")
        math(EXPR _numtilo_hda_low "${_numtilo_hda_code} % 16")
        string(SUBSTRING "${_numtilo_hda_hex}" ${_numtilo_hda_high} 1 _numtilo_hda_high_hex)
        string(SUBSTRING "${_numtilo_hda_hex}" ${_numtilo_hda_low} 1 _numtilo_hda_low_hex)
        string(REPLACE "${_numtilo_hda_control}"
            "\\u00${_numtilo_hda_high_hex}${_numtilo_hda_low_hex}"
            _numtilo_hda_escaped "${_numtilo_hda_escaped}")
    endforeach()
    set(${output} "\"${_numtilo_hda_escaped}\"" PARENT_SCOPE)
endfunction()

function(_numtilo_hda_json_array output)
    set(_numtilo_hda_result "[")
    set(_numtilo_hda_separator "")
    foreach(_numtilo_hda_item IN LISTS ARGN)
        _numtilo_hda_json_string("${_numtilo_hda_item}" _numtilo_hda_json_item)
        string(APPEND _numtilo_hda_result "${_numtilo_hda_separator}${_numtilo_hda_json_item}")
        set(_numtilo_hda_separator ",")
    endforeach()
    string(APPEND _numtilo_hda_result "]")
    set(${output} "${_numtilo_hda_result}" PARENT_SCOPE)
endfunction()

function(_numtilo_hda_platform_json output)
    if (CMAKE_CROSSCOMPILING)
        set(_numtilo_hda_cross_json true)
    else()
        set(_numtilo_hda_cross_json false)
    endif()
    set(_numtilo_hda_platform "\"cross_compiling\":${_numtilo_hda_cross_json}")
    foreach(_numtilo_hda_platform_field system_name host_system_name system_processor host_system_processor)
        string(TOUPPER "${_numtilo_hda_platform_field}" _numtilo_hda_platform_variable)
        _numtilo_hda_json_string("${CMAKE_${_numtilo_hda_platform_variable}}"
            _numtilo_hda_platform_value)
        string(APPEND _numtilo_hda_platform
            ",\"${_numtilo_hda_platform_field}\":${_numtilo_hda_platform_value}")
    endforeach()
    set(${output} "${_numtilo_hda_platform}" PARENT_SCOPE)
endfunction()

file(MAKE_DIRECTORY "${HEADLESS_AUDIT_DIR}")
# A failed reconfigure must not leave a previous successful property snapshot.
file(REMOVE "${HEADLESS_AUDIT_DIR}/target-properties.json")
_numtilo_hda_json_string("${CMAKE_SOURCE_DIR}" _numtilo_hda_source_json)
_numtilo_hda_json_string("${CMAKE_BINARY_DIR}" _numtilo_hda_build_json)
_numtilo_hda_json_string("${CMAKE_CURRENT_LIST_FILE}" _numtilo_hda_guard_json)
_numtilo_hda_platform_json(_numtilo_hda_platform_fields)
if (BUILD_TESTING)
    set(_numtilo_hda_testing_json true)
else()
    set(_numtilo_hda_testing_json false)
endif()
file(WRITE "${HEADLESS_AUDIT_DIR}/dependency-events.jsonl"
    "{\"schema_version\":1,\"event\":\"loaded\",\"source_dir\":${_numtilo_hda_source_json},"
    "\"build_dir\":${_numtilo_hda_build_json},\"guard_path\":${_numtilo_hda_guard_json},"
    "\"build_testing\":${_numtilo_hda_testing_json},${_numtilo_hda_platform_fields}}\n")

macro(_numtilo_headless_dependency_provider method package_name)
    string(TOLOWER "${package_name}" _numtilo_hda_package_lower)
    set(_numtilo_hda_rejected false)
    if (_numtilo_hda_package_lower MATCHES "^(sfml|fmt|openal|python|python3)$" OR
        (NOT BUILD_TESTING AND _numtilo_hda_package_lower STREQUAL "catch2"))
        set(_numtilo_hda_rejected true)
    endif()
    _numtilo_hda_json_string("${method}" _numtilo_hda_method_json)
    _numtilo_hda_json_string("${package_name}" _numtilo_hda_package_json)
    _numtilo_hda_json_string("${ARGN}" _numtilo_hda_arguments_json)
    if (BUILD_TESTING)
        set(_numtilo_hda_testing_json true)
    else()
        set(_numtilo_hda_testing_json false)
    endif()
    file(APPEND "${HEADLESS_AUDIT_DIR}/dependency-events.jsonl"
        "{\"schema_version\":1,\"event\":\"request\",\"method\":${_numtilo_hda_method_json},"
        "\"name\":${_numtilo_hda_package_json},\"arguments\":${_numtilo_hda_arguments_json},"
        "\"build_testing\":${_numtilo_hda_testing_json},\"rejected\":${_numtilo_hda_rejected}}\n")
    if (_numtilo_hda_rejected)
        message(FATAL_ERROR "Headless dependency audit rejected ${method} request for ${package_name}")
    endif()
    # Deliberately do not set <package>_FOUND or call FetchContent_SetPopulated:
    # allowed requests must use the real package discovery/fallback path.
endmacro()

cmake_language(SET_DEPENDENCY_PROVIDER _numtilo_headless_dependency_provider
    SUPPORTED_METHODS FIND_PACKAGE FETCHCONTENT_MAKEAVAILABLE_SERIAL)

function(_numtilo_hda_directory_targets directory built_output imported_output)
    get_property(_numtilo_hda_built DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(_numtilo_hda_imported DIRECTORY "${directory}" PROPERTY IMPORTED_TARGETS)
    get_property(_numtilo_hda_subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(_numtilo_hda_subdirectory IN LISTS _numtilo_hda_subdirectories)
        _numtilo_hda_directory_targets("${_numtilo_hda_subdirectory}"
            _numtilo_hda_child_built _numtilo_hda_child_imported)
        list(APPEND _numtilo_hda_built ${_numtilo_hda_child_built})
        list(APPEND _numtilo_hda_imported ${_numtilo_hda_child_imported})
    endforeach()
    list(REMOVE_DUPLICATES _numtilo_hda_built)
    list(REMOVE_DUPLICATES _numtilo_hda_imported)
    list(SORT _numtilo_hda_built)
    list(SORT _numtilo_hda_imported)
    set(${built_output} "${_numtilo_hda_built}" PARENT_SCOPE)
    set(${imported_output} "${_numtilo_hda_imported}" PARENT_SCOPE)
endfunction()

function(_numtilo_hda_link_targets raw_links output)
    # Scan raw generator expressions without evaluating LINK_ONLY or other
    # context-sensitive expressions. Colon suffixes expose a target hidden
    # behind a genex operator while retaining valid namespaced target names.
    string(REGEX MATCHALL "[A-Za-z0-9_.:+-]+" _numtilo_hda_tokens "${raw_links}")
    set(_numtilo_hda_targets "")
    foreach(_numtilo_hda_token IN LISTS _numtilo_hda_tokens)
        set(_numtilo_hda_candidate "${_numtilo_hda_token}")
        while (NOT _numtilo_hda_candidate STREQUAL "")
            if (TARGET "${_numtilo_hda_candidate}")
                list(APPEND _numtilo_hda_targets "${_numtilo_hda_candidate}")
                break()
            endif()
            string(FIND "${_numtilo_hda_candidate}" ":" _numtilo_hda_colon)
            if (_numtilo_hda_colon EQUAL -1)
                break()
            endif()
            math(EXPR _numtilo_hda_suffix_start "${_numtilo_hda_colon} + 1")
            string(SUBSTRING "${_numtilo_hda_candidate}" ${_numtilo_hda_suffix_start} -1
                _numtilo_hda_candidate)
        endwhile()
    endforeach()
    list(REMOVE_DUPLICATES _numtilo_hda_targets)
    set(${output} "${_numtilo_hda_targets}" PARENT_SCOPE)
endfunction()

function(_numtilo_hda_snapshot)
    _numtilo_hda_directory_targets("${CMAKE_SOURCE_DIR}"
        _numtilo_hda_built _numtilo_hda_imported)
    set(_numtilo_hda_requested game_core app_logic app_paths asset_resolver asset_locations
        user_data_paths app_settings profile_migration runtime_check)
    if (TARGET core_unit_tests)
        list(APPEND _numtilo_hda_requested core_unit_tests)
    endif()
    set(_numtilo_hda_pending "${_numtilo_hda_requested}")
    set(_numtilo_hda_unresolved_imported "")
    foreach(_numtilo_hda_declared_imported IN LISTS _numtilo_hda_imported)
        if (TARGET "${_numtilo_hda_declared_imported}")
            list(APPEND _numtilo_hda_pending "${_numtilo_hda_declared_imported}")
        else()
            # Non-GLOBAL imports declared in a child directory are not visible
            # here. Report this scope limit instead of inventing their properties.
            list(APPEND _numtilo_hda_unresolved_imported "${_numtilo_hda_declared_imported}")
        endif()
    endforeach()
    set(_numtilo_hda_seen "")
    set(_numtilo_hda_missing "")
    set(_numtilo_hda_targets_json "{")
    set(_numtilo_hda_target_separator "")
    while (_numtilo_hda_pending)
        list(POP_FRONT _numtilo_hda_pending _numtilo_hda_target)
        if (_numtilo_hda_target IN_LIST _numtilo_hda_seen)
            continue()
        endif()
        list(APPEND _numtilo_hda_seen "${_numtilo_hda_target}")
        if (NOT TARGET "${_numtilo_hda_target}")
            list(APPEND _numtilo_hda_missing "${_numtilo_hda_target}")
            continue()
        endif()

        get_target_property(_numtilo_hda_type "${_numtilo_hda_target}" TYPE)
        get_target_property(_numtilo_hda_is_imported "${_numtilo_hda_target}" IMPORTED)
        get_target_property(_numtilo_hda_alias "${_numtilo_hda_target}" ALIASED_TARGET)
        get_target_property(_numtilo_hda_configs "${_numtilo_hda_target}" IMPORTED_CONFIGURATIONS)
        if (_numtilo_hda_alias STREQUAL "_numtilo_hda_alias-NOTFOUND")
            set(_numtilo_hda_alias_json null)
        else()
            _numtilo_hda_json_string("${_numtilo_hda_alias}" _numtilo_hda_alias_json)
            list(APPEND _numtilo_hda_pending "${_numtilo_hda_alias}")
        endif()
        if (_numtilo_hda_configs STREQUAL "_numtilo_hda_configs-NOTFOUND")
            set(_numtilo_hda_configs "")
        endif()
        list(APPEND _numtilo_hda_configs DEBUG RELEASE RELWITHDEBINFO MINSIZEREL
            ${CMAKE_CONFIGURATION_TYPES} ${CMAKE_BUILD_TYPE})
        list(REMOVE_DUPLICATES _numtilo_hda_configs)

        set(_numtilo_hda_properties TYPE IMPORTED ALIASED_TARGET LINK_LIBRARIES
            INTERFACE_LINK_LIBRARIES INCLUDE_DIRECTORIES INTERFACE_INCLUDE_DIRECTORIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES LINK_OPTIONS INTERFACE_LINK_OPTIONS
            IMPORTED_CONFIGURATIONS IMPORTED_LOCATION IMPORTED_IMPLIB IMPORTED_SONAME
            IMPORTED_NO_SONAME IMPORTED_LINK_INTERFACE_LIBRARIES IMPORTED_LINK_DEPENDENT_LIBRARIES)
        foreach(_numtilo_hda_config IN LISTS _numtilo_hda_configs)
            string(TOUPPER "${_numtilo_hda_config}" _numtilo_hda_config_upper)
            foreach(_numtilo_hda_config_property IMPORTED_LOCATION IMPORTED_IMPLIB IMPORTED_SONAME
                IMPORTED_NO_SONAME IMPORTED_LINK_INTERFACE_LIBRARIES IMPORTED_LINK_DEPENDENT_LIBRARIES
                MAP_IMPORTED_CONFIG)
                list(APPEND _numtilo_hda_properties
                    "${_numtilo_hda_config_property}_${_numtilo_hda_config_upper}")
            endforeach()
        endforeach()
        list(REMOVE_DUPLICATES _numtilo_hda_properties)
        set(_numtilo_hda_properties_json "{")
        set(_numtilo_hda_property_separator "")
        foreach(_numtilo_hda_property IN LISTS _numtilo_hda_properties)
            get_property(_numtilo_hda_property_is_set TARGET "${_numtilo_hda_target}"
                PROPERTY "${_numtilo_hda_property}" SET)
            if (_numtilo_hda_property_is_set)
                get_target_property(_numtilo_hda_property_value "${_numtilo_hda_target}"
                    "${_numtilo_hda_property}")
                _numtilo_hda_json_string("${_numtilo_hda_property_value}"
                    _numtilo_hda_property_json)
                if (_numtilo_hda_property MATCHES "^(LINK_LIBRARIES|INTERFACE_LINK_LIBRARIES|IMPORTED_LINK_INTERFACE_LIBRARIES|IMPORTED_LINK_DEPENDENT_LIBRARIES)(_|$)")
                    _numtilo_hda_link_targets("${_numtilo_hda_property_value}"
                        _numtilo_hda_reachable)
                    list(APPEND _numtilo_hda_pending ${_numtilo_hda_reachable})
                endif()
            else()
                set(_numtilo_hda_property_json null)
            endif()
            _numtilo_hda_json_string("${_numtilo_hda_property}" _numtilo_hda_property_key)
            string(APPEND _numtilo_hda_properties_json
                "${_numtilo_hda_property_separator}${_numtilo_hda_property_key}:${_numtilo_hda_property_json}")
            set(_numtilo_hda_property_separator ",")
        endforeach()
        string(APPEND _numtilo_hda_properties_json "}")
        if (_numtilo_hda_is_imported)
            set(_numtilo_hda_imported_json true)
        else()
            set(_numtilo_hda_imported_json false)
        endif()
        _numtilo_hda_json_string("${_numtilo_hda_target}" _numtilo_hda_target_key)
        _numtilo_hda_json_string("${_numtilo_hda_type}" _numtilo_hda_type_json)
        string(APPEND _numtilo_hda_targets_json
            "${_numtilo_hda_target_separator}${_numtilo_hda_target_key}:{"
            "\"name\":${_numtilo_hda_target_key},\"type\":${_numtilo_hda_type_json},"
            "\"imported\":${_numtilo_hda_imported_json},\"alias_of\":${_numtilo_hda_alias_json},"
            "\"properties\":${_numtilo_hda_properties_json}}")
        set(_numtilo_hda_target_separator ",")
    endwhile()
    string(APPEND _numtilo_hda_targets_json "}")
    _numtilo_hda_json_array(_numtilo_hda_requested_json ${_numtilo_hda_requested})
    _numtilo_hda_json_array(_numtilo_hda_missing_json ${_numtilo_hda_missing})
    _numtilo_hda_json_array(_numtilo_hda_built_json ${_numtilo_hda_built})
    _numtilo_hda_json_array(_numtilo_hda_imported_targets_json ${_numtilo_hda_imported})
    _numtilo_hda_json_array(_numtilo_hda_unresolved_imported_json ${_numtilo_hda_unresolved_imported})
    _numtilo_hda_json_string("${CMAKE_SOURCE_DIR}" _numtilo_hda_source_json)
    _numtilo_hda_json_string("${CMAKE_BINARY_DIR}" _numtilo_hda_build_json)
    _numtilo_hda_platform_json(_numtilo_hda_platform_fields)
    if (BUILD_TESTING)
        set(_numtilo_hda_testing_json true)
    else()
        set(_numtilo_hda_testing_json false)
    endif()
    file(WRITE "${HEADLESS_AUDIT_DIR}/target-properties.json"
        "{\"schema_version\":1,\"source_dir\":${_numtilo_hda_source_json},"
        "\"build_dir\":${_numtilo_hda_build_json},\"build_testing\":${_numtilo_hda_testing_json},"
        "${_numtilo_hda_platform_fields},"
        "\"requested_targets\":${_numtilo_hda_requested_json},\"missing_targets\":${_numtilo_hda_missing_json},"
        "\"buildsystem_targets\":${_numtilo_hda_built_json},"
        "\"imported_targets\":${_numtilo_hda_imported_targets_json},"
        "\"unresolved_imported_targets\":${_numtilo_hda_unresolved_imported_json},"
        "\"targets\":${_numtilo_hda_targets_json}}\n")
endfunction()

cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _numtilo_hda_snapshot)
