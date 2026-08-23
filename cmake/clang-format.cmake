cmake_minimum_required(VERSION 3.22)

set(_valid_modes CHECK FORMAT)

if(NOT DEFINED CLANG_FORMAT_MODE)
    set(CLANG_FORMAT_MODE CHECK)
endif()

string(TOUPPER "${CLANG_FORMAT_MODE}" CLANG_FORMAT_MODE)
if(NOT CLANG_FORMAT_MODE IN_LIST _valid_modes)
    message(FATAL_ERROR
        "Invalid CLANG_FORMAT_MODE='${CLANG_FORMAT_MODE}'. Expected CHECK or FORMAT."
    )
endif()

if(DEFINED CLANG_FORMAT_EXECUTABLE AND NOT CLANG_FORMAT_EXECUTABLE STREQUAL "")
    if(IS_ABSOLUTE "${CLANG_FORMAT_EXECUTABLE}")
        if(NOT EXISTS "${CLANG_FORMAT_EXECUTABLE}")
            message(FATAL_ERROR
                "CLANG_FORMAT_EXECUTABLE does not exist: ${CLANG_FORMAT_EXECUTABLE}"
            )
        endif()
        set(_clang_format "${CLANG_FORMAT_EXECUTABLE}")
    else()
        find_program(_clang_format NAMES "${CLANG_FORMAT_EXECUTABLE}")
    endif()
else()
    find_program(_clang_format NAMES clang-format-18 clang-format)
endif()

if(NOT _clang_format)
    message(FATAL_ERROR
        "clang-format was not found. Install clang-format 18 or set "
        "-DCLANG_FORMAT_EXECUTABLE=<path-or-command>."
    )
endif()

execute_process(
    COMMAND "${_clang_format}" --version
    RESULT_VARIABLE _version_result
    OUTPUT_VARIABLE _version_output
    ERROR_VARIABLE _version_error
)
if(NOT _version_result EQUAL 0)
    message(FATAL_ERROR
        "Failed to run ${_clang_format}: ${_version_error}"
    )
endif()
string(STRIP "${_version_output}" _version_output)
message(STATUS "Using ${_version_output}")

get_filename_component(_project_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_source_roots
    "${_project_root}/firmware/MiniAudioPlayerST/App"
    "${_project_root}/firmware/MiniAudioPlayerST/BSP"
)
set(_excluded_filenames
    font_cn.c
    font_cn.h
    font_en.c
    font_en.h
    font_file_cn.c
    font_file_cn.h
)

set(_candidate_files)
foreach(_source_root IN LISTS _source_roots)
    if(NOT IS_DIRECTORY "${_source_root}")
        message(FATAL_ERROR "Source directory does not exist: ${_source_root}")
    endif()

    file(GLOB_RECURSE _root_files
        LIST_DIRECTORIES false
        "${_source_root}/*.c"
        "${_source_root}/*.h"
    )
    list(APPEND _candidate_files ${_root_files})
endforeach()

set(_source_files)
foreach(_source_file IN LISTS _candidate_files)
    get_filename_component(_filename "${_source_file}" NAME)
    if(NOT _filename IN_LIST _excluded_filenames)
        list(APPEND _source_files "${_source_file}")
    endif()
endforeach()
list(SORT _source_files)

if(NOT _source_files)
    message(FATAL_ERROR "No source files were found for clang-format.")
endif()

if(CLANG_FORMAT_MODE STREQUAL "FORMAT")
    set(_clang_format_arguments --style=file -i)
else()
    set(_clang_format_arguments --style=file --dry-run --Werror)
endif()

set(_failed_files)
foreach(_source_file IN LISTS _source_files)
    execute_process(
        COMMAND "${_clang_format}" ${_clang_format_arguments} "${_source_file}"
        WORKING_DIRECTORY "${_project_root}"
        RESULT_VARIABLE _format_result
        OUTPUT_VARIABLE _format_output
        ERROR_VARIABLE _format_error
    )

    if(NOT _format_result EQUAL 0)
        if(NOT _format_output STREQUAL "")
            string(STRIP "${_format_output}" _format_output)
            message("${_format_output}")
        endif()
        if(NOT _format_error STREQUAL "")
            string(STRIP "${_format_error}" _format_error)
            message("${_format_error}")
        endif()

        file(RELATIVE_PATH _relative_file "${_project_root}" "${_source_file}")
        list(APPEND _failed_files "${_relative_file}")
    endif()
endforeach()

list(LENGTH _source_files _source_file_count)
if(_failed_files)
    list(JOIN _failed_files "\n  " _failed_file_list)
    message(FATAL_ERROR
        "clang-format ${CLANG_FORMAT_MODE} failed for:\n"
        "  ${_failed_file_list}\n"
        "Format locally with:\n"
        "  cmake -DCLANG_FORMAT_MODE=FORMAT -P cmake/clang-format.cmake"
    )
endif()

message(STATUS
    "clang-format ${CLANG_FORMAT_MODE} passed for ${_source_file_count} file(s)."
)
