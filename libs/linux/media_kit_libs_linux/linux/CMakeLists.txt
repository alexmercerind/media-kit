# This file is a part of media_kit (https://github.com/media-kit/media-kit).
#
# Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
# All rights reserved.
# Use of this source code is governed by MIT license that can be found in the LICENSE file.

cmake_minimum_required(VERSION 3.13)

# This option is read by the other packages which are part of package:media_kit.
option(MEDIA_KIT_LIBS_AVAILABLE "package:media_kit libraries are available." ON)

# ------------------------------------------------------------------------------

function(download_and_verify_sha256 url sha256 locationForArchive)
  # Check if the archive exists.
  if(EXISTS "${locationForArchive}")
    file(SHA256 "${locationForArchive}" ARCHIVE_SHA256)

    # If SHA-256 doesn't match, delete the archive to download again.
    if(NOT sha256 STREQUAL ARCHIVE_SHA256)
      file(REMOVE "${locationForArchive}")
      message(STATUS "SHA-256 mismatch. File deleted.")
    endif()
  endif()

  # Download the archive if it doesn't exist.
  if(NOT EXISTS "${locationForArchive}")
    message(STATUS "Downloading archive from ${url}...")
    file(DOWNLOAD "${url}" "${locationForArchive}")
    message(STATUS "Downloaded archive to ${locationForArchive}.")

    # Verify SHA-256 of the newly downloaded file.
    file(SHA256 "${locationForArchive}" ARCHIVE_SHA256)

    if(sha256 STREQUAL ARCHIVE_SHA256)
      message(STATUS "${locationForArchive} Verification successful.")
    else()
      message(FATAL_ERROR "${locationForArchive} Integrity check failed, please try to rebuild project again.")
    endif()
  endif()
endfunction()

set(LIBMPV_VERSION "v0.0.5")

if(${FLUTTER_TARGET_PLATFORM} STREQUAL "linux-arm64")
  set(LIBMPV "libmpv_linux_aarch64_audio.zip")
  set(LIBMPV_SHA256 "88061168ec3cab52147f5bfbc077bd57a56e2a91a40563246f388a3f71588608")
elseif(${FLUTTER_TARGET_PLATFORM} STREQUAL "linux-x64")
  set(LIBMPV "libmpv_linux_x86_64_audio.zip")
  set(LIBMPV_SHA256 "a9eb8e7c19f37a9fce477a232e6ecbcb0888ecb4658003ceb334f3f17ae97d00")
else()
  message(FATAL_ERROR "Unsupported FLUTTER_TARGET_PLATFORM for prebuilt libmpv: ${FLUTTER_TARGET_PLATFORM}")
endif()

# Download URL for the libmpv archive.
set(LIBMPV_URL "https://github.com/alexmercerind/libmpv-build/releases/download/${LIBMPV_VERSION}/${LIBMPV}")

# Download location of the libmpv archive.
set(LIBMPV_ARCHIVE "${CMAKE_BINARY_DIR}/${LIBMPV_VERSION}_${LIBMPV}")
set(LIBMPV_SRC "${CMAKE_BINARY_DIR}/libmpv")

download_and_verify_sha256(
  ${LIBMPV_URL}
  ${LIBMPV_SHA256}
  ${LIBMPV_ARCHIVE}
)

# Extract the libmpv archive.
if(NOT EXISTS "${LIBMPV_SRC}/lib/libmpv.so")
  message(STATUS "Extracting ${LIBMPV}...")
  file(MAKE_DIRECTORY "${LIBMPV_SRC}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xf "${LIBMPV_ARCHIVE}"
    WORKING_DIRECTORY "${LIBMPV_SRC}"
    RESULT_VARIABLE LIBMPV_EXTRACT_RESULT
  )
  if(NOT LIBMPV_EXTRACT_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to extract ${LIBMPV_ARCHIVE}.")
  endif()
endif()

# ------------------------------------------------------------------------------
set(PLUGIN_NAME "media_kit_libs_linux_plugin")

add_library(
  ${PLUGIN_NAME} SHARED
  "media_kit_libs_linux_plugin.cc"
)

apply_standard_settings(${PLUGIN_NAME})
set_target_properties(${PLUGIN_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)

target_include_directories(
  ${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(${PLUGIN_NAME} PRIVATE flutter)

set(
  media_kit_libs_linux_bundled_libraries
  "${LIBMPV_SRC}/lib/libmpv.so.2.6.0"
  PARENT_SCOPE
)
