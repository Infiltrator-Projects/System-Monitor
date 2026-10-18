# SPDX-License-Identifier: GPL-3.0-or-later
# Windows-only verified font materialisation from InfiltratorOS's immutable
# 23-face inventory. The ordinary Makefile font stamp remains separate.
get_filename_component(LSM_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(NOT DEFINED LSM_FONT_OUTPUT_DIR)
    message(FATAL_ERROR "LSM_FONT_OUTPUT_DIR is required")
endif()
set(LSM_EXTENDED_FONT_COMMIT "66c17fe9ea3ea9678b4028ffea1862a37d488f79")
set(LSM_EXTENDED_FONT_BASE
    "https://raw.githubusercontent.com/Infiltrator-Projects/InfiltratorOS/${LSM_EXTENDED_FONT_COMMIT}/assets/fonts")
file(MAKE_DIRECTORY "${LSM_FONT_OUTPUT_DIR}")
set(LSM_FONT_MANIFEST "${LSM_FONT_OUTPUT_DIR}/SHA256SUMS")
file(DOWNLOAD "${LSM_EXTENDED_FONT_BASE}/SHA256SUMS"
    "${LSM_FONT_MANIFEST}" TLS_VERIFY ON STATUS LSM_DOWNLOAD_STATUS)
list(GET LSM_DOWNLOAD_STATUS 0 LSM_DOWNLOAD_CODE)
if(NOT LSM_DOWNLOAD_CODE EQUAL 0)
    message(FATAL_ERROR "Unable to retrieve pinned extended font manifest")
endif()
file(STRINGS "${LSM_FONT_MANIFEST}" LSM_MANIFEST_LINES)
list(LENGTH LSM_MANIFEST_LINES LSM_MANIFEST_COUNT)
if(NOT LSM_MANIFEST_COUNT EQUAL 23)
    message(FATAL_ERROR "Incomplete 23-face InfiltratorOS inventory")
endif()
set(LSM_WINDOWS_FONT_ROLES
    "ui_regular|mb_corpo_s_text_regular.ttf"
    "ui_bold|mb_corpo_s_text_bold.ttf"
    "brand_regular|mb_corpo_a_title_cond_regular.ttf"
    "caption_regular|mb_corpo_s_text_cond_regular.ttf"
    "heading_bold|mb_corpo_s_title_bold.ttf"
    "metric_regular|mb_corpo_a_screen_1_cond_regular.ttf")
foreach(LSM_ROLE IN LISTS LSM_WINDOWS_FONT_ROLES)
    string(REPLACE "|" ";" LSM_ROLE_PARTS "${LSM_ROLE}")
    list(GET LSM_ROLE_PARTS 0 LSM_ROLE_ALIAS)
    list(GET LSM_ROLE_PARTS 1 LSM_ROLE_FILENAME)
    set(LSM_ROLE_SHA "")
    foreach(LSM_MANIFEST_LINE IN LISTS LSM_MANIFEST_LINES)
        string(REPLACE "  " ";" LSM_MANIFEST_PARTS "${LSM_MANIFEST_LINE}")
        list(LENGTH LSM_MANIFEST_PARTS LSM_PART_COUNT)
        if(LSM_PART_COUNT EQUAL 2)
            list(GET LSM_MANIFEST_PARTS 1 LSM_ENTRY_FILENAME)
            if(LSM_ENTRY_FILENAME STREQUAL LSM_ROLE_FILENAME)
                list(GET LSM_MANIFEST_PARTS 0 LSM_ROLE_SHA)
            endif()
        endif()
    endforeach()
    string(LENGTH "${LSM_ROLE_SHA}" LSM_ROLE_HASH_LENGTH)
    if(NOT LSM_ROLE_HASH_LENGTH EQUAL 64)
        message(FATAL_ERROR "No verified checksum for ${LSM_ROLE_FILENAME}")
    endif()
    set(LSM_ROLE_OUTPUT "${LSM_FONT_OUTPUT_DIR}/${LSM_ROLE_ALIAS}.ttf")
    file(DOWNLOAD
        "${LSM_EXTENDED_FONT_BASE}/${LSM_ROLE_FILENAME}"
        "${LSM_ROLE_OUTPUT}" TLS_VERIFY ON
        EXPECTED_HASH "SHA256=${LSM_ROLE_SHA}" STATUS LSM_DOWNLOAD_STATUS)
    list(GET LSM_DOWNLOAD_STATUS 0 LSM_DOWNLOAD_CODE)
    if(NOT LSM_DOWNLOAD_CODE EQUAL 0)
        message(FATAL_ERROR "Could not verify ${LSM_ROLE_FILENAME}")
    endif()
endforeach()
message(STATUS "Verified six Windows fonts against the pinned 23-face inventory")
