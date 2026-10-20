# Keep platform libraries out of flat Runtime packages without dropping the
# application's shared third-party dependencies.
cmake_minimum_required(VERSION 3.24)
include_guard(GLOBAL)

if (CMAKE_SYSTEM_NAME)
    set(NUMTILO_RUNTIME_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}")
else()
    set(NUMTILO_RUNTIME_SYSTEM_NAME "${CMAKE_HOST_SYSTEM_NAME}")
endif()

set(NUMTILO_RUNTIME_PRE_EXCLUDE_REGEXES "")
set(NUMTILO_RUNTIME_POST_EXCLUDE_REGEXES "")
set(NUMTILO_RUNTIME_POST_INCLUDE_REGEXES "")
set(NUMTILO_RUNTIME_FORBIDDEN_NAMES "")

if (NUMTILO_RUNTIME_SYSTEM_NAME STREQUAL "Windows")
    # CMake lowercases DLL names for PRE matching. These are Windows components,
    # not the redistributable MSVC runtime or third-party graphics/audio DLLs.
    set(NUMTILO_RUNTIME_FORBIDDEN_NAMES
        "^(advapi32|avrt|bcrypt|combase|crypt32|dwmapi|gdi32|imm32|kernel32|kernelbase|msvcp_win|ntdll|ole32|oleaut32|opengl32|rpcrt4|secur32|setupapi|shell32|shlwapi|ucrtbase|user32|version|winmm|winspool|ws2_32)[.]dll$"
        "^(api-ms-|ext-ms-).*"
        "^hvsifiletrust[.]dll$"
    )
    set(NUMTILO_RUNTIME_PRE_EXCLUDE_REGEXES ${NUMTILO_RUNTIME_FORBIDDEN_NAMES})
    # Windows APIs can return backslashes while CMake appends a forward slash.
    set(NUMTILO_RUNTIME_POST_EXCLUDE_REGEXES
        ".*[/\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\].*[.][Dd][Ll][Ll]$"
        ".*[/\\][Ss][Yy][Ss][Ww][Oo][Ww]64[/\\].*[.][Dd][Ll][Ll]$"
    )
    # These redistributable runtimes may resolve from System32 on build hosts.
    # Include them explicitly so the installed application remains relocatable.
    set(NUMTILO_RUNTIME_POST_INCLUDE_REGEXES
        ".*[/\\]([Mm][Ss][Vv][Cc][Pp]|[Vv][Cc][Rr][Uu][Nn][Tt][Ii][Mm][Ee]|[Cc][Oo][Nn][Cc][Rr][Tt]|[Vv][Cc][Oo][Mm][Pp])[0-9]+(_[A-Za-z0-9_]+)?[.][Dd][Ll][Ll]$"
        ".*[/\\][Ss][Ff][Mm][Ll]-([Gg][Rr][Aa][Pp][Hh][Ii][Cc][Ss]|[Ww][Ii][Nn][Dd][Oo][Ww]|[Ss][Yy][Ss][Tt][Ee][Mm]|[Aa][Uu][Dd][Ii][Oo])(-[Dd])?(-[0-9]+)?[.][Dd][Ll][Ll]$"
        ".*[/\\][Oo][Pp][Ee][Nn][Aa][Ll]32[.][Dd][Ll][Ll]$"
    )
elseif (NUMTILO_RUNTIME_SYSTEM_NAME STREQUAL "Linux")
    # glibc and its ELF loader belong to the target OS. Do not exclude /usr/lib
    # wholesale: it also supplies OpenAL, codecs, graphics and C++ runtimes.
    set(NUMTILO_RUNTIME_FORBIDDEN_NAMES
        "^lib(c|m|mvec|dl|pthread|rt|util|resolv|anl|BrokenLocale|nss_(compat|dns|files|hesiod))[.]so([.][0-9]+)*$"
        "^ld-linux(-[A-Za-z0-9_-]+)?[.]so([.][0-9]+)*$"
        "^ld64[.]so([.][0-9]+)*$"
        "^(lib(c|m|mvec|dl|pthread|rt|util|resolv|anl|BrokenLocale|nss_(compat|dns|files|hesiod))|ld)-[0-9]+([.][0-9]+)*[.]so$"
    )
    set(NUMTILO_RUNTIME_PRE_EXCLUDE_REGEXES ${NUMTILO_RUNTIME_FORBIDDEN_NAMES})
elseif (NUMTILO_RUNTIME_SYSTEM_NAME STREQUAL "Darwin")
    set(NUMTILO_RUNTIME_POST_EXCLUDE_REGEXES ".*/usr/lib/.*" ".*/System/Library/.*")
endif()

function(numtilo_verify_runtime_dependencies executable)
    # macOS bundles retain their separate BundleUtilities verification path.
    if (NOT NUMTILO_RUNTIME_SYSTEM_NAME MATCHES "^(Windows|Linux)$")
        message(FATAL_ERROR "The flat Runtime dependency check requires Windows or Linux")
    endif()
    if (NOT EXISTS "${executable}" OR IS_DIRECTORY "${executable}")
        message(FATAL_ERROR "Runtime executable does not exist: ${executable}")
    endif()

    file(REAL_PATH "${executable}" _numtilo_executable)
    get_filename_component(_numtilo_root "${_numtilo_executable}" DIRECTORY)
    file(GLOB_RECURSE _numtilo_members LIST_DIRECTORIES FALSE "${_numtilo_root}/*")
    foreach(_numtilo_member IN LISTS _numtilo_members)
        get_filename_component(_numtilo_name "${_numtilo_member}" NAME)
        if (NUMTILO_RUNTIME_SYSTEM_NAME STREQUAL "Windows")
            string(TOLOWER "${_numtilo_name}" _numtilo_name)
        endif()
        foreach(_numtilo_forbidden IN LISTS NUMTILO_RUNTIME_FORBIDDEN_NAMES)
            if (_numtilo_name MATCHES "${_numtilo_forbidden}")
                message(FATAL_ERROR "OS runtime library must not be packaged: ${_numtilo_member}")
            endif()
        endforeach()
    endforeach()

    file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES "${_numtilo_executable}"
        DIRECTORIES "${_numtilo_root}"
        RESOLVED_DEPENDENCIES_VAR _numtilo_resolved
        UNRESOLVED_DEPENDENCIES_VAR _numtilo_unresolved
        CONFLICTING_DEPENDENCIES_PREFIX _numtilo_conflicts
        PRE_EXCLUDE_REGEXES ${NUMTILO_RUNTIME_PRE_EXCLUDE_REGEXES}
        POST_EXCLUDE_REGEXES ${NUMTILO_RUNTIME_POST_EXCLUDE_REGEXES}
        POST_INCLUDE_REGEXES ${NUMTILO_RUNTIME_POST_INCLUDE_REGEXES}
    )
    if (_numtilo_unresolved)
        message(FATAL_ERROR "Unresolved Runtime dependencies: ${_numtilo_unresolved}")
    endif()
    if (_numtilo_conflicts_FILENAMES)
        foreach(_numtilo_name IN LISTS _numtilo_conflicts_FILENAMES)
            message(STATUS "Conflicting Runtime library ${_numtilo_name}: ${_numtilo_conflicts_${_numtilo_name}}")
        endforeach()
        message(FATAL_ERROR "Conflicting Runtime dependencies: ${_numtilo_conflicts_FILENAMES}")
    endif()

    # Linux RUNPATH is not inherited by transitive dependencies, so the scanner
    # can report an OS search path even when the same library is packaged. Check
    # every resolved non-OS library's companion in this flat Runtime package.
    set(_numtilo_companions "")
    foreach(_numtilo_dependency IN LISTS _numtilo_resolved)
        get_filename_component(_numtilo_name "${_numtilo_dependency}" NAME)
        set(_numtilo_companion "${_numtilo_root}/${_numtilo_name}")
        if (NOT EXISTS "${_numtilo_companion}" OR IS_DIRECTORY "${_numtilo_companion}")
            message(FATAL_ERROR "Required Runtime library is missing from the package: ${_numtilo_name}")
        endif()
        list(APPEND _numtilo_companions "${_numtilo_companion}")
    endforeach()
    # CMake 3.29's Windows REAL_PATH only expands the absolute lexical path.
    # Python resolves native file links and parent junctions on every host,
    # including when a portable fixture selects a different target platform.
    if (NOT Python3_EXECUTABLE)
        find_package(Python3 3.8 COMPONENTS Interpreter REQUIRED)
    endif()
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -B -c [=[
from pathlib import Path
import sys

try:
    root = Path(sys.argv[1]).resolve(strict=True)
    for name in sys.argv[2:]:
        actual = Path(name).resolve(strict=True)
        if not actual.is_file():
            raise ValueError("Required runtime library is not a regular file: " + name)
        actual.relative_to(root)
except (OSError, RuntimeError, ValueError) as error:
    print(str(error), file=sys.stderr)
    sys.exit(1)
]=] "${_numtilo_root}" ${_numtilo_companions}
        RESULT_VARIABLE _numtilo_containment_result
        ERROR_VARIABLE _numtilo_containment_error
        TIMEOUT 15
    )
    if (NOT _numtilo_containment_result STREQUAL "0")
        message(FATAL_ERROR "Runtime library points outside the package or cannot be resolved safely: ${_numtilo_containment_error}")
    endif()
    list(LENGTH _numtilo_resolved _numtilo_count)
    message(STATUS "Verified ${_numtilo_count} packaged Runtime dependencies (${NUMTILO_RUNTIME_SYSTEM_NAME})")
endfunction()

if (CMAKE_SCRIPT_MODE_FILE AND CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
    if (NOT NUMTILO_RUNTIME_EXECUTABLE)
        message(FATAL_ERROR "NUMTILO_RUNTIME_EXECUTABLE is required")
    endif()
    numtilo_verify_runtime_dependencies("${NUMTILO_RUNTIME_EXECUTABLE}")
endif()
