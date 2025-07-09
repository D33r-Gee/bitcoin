# Copyright (c) 2025 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Packaging output directory
set(ANDROID_PACKAGE_DIR ${CMAKE_BINARY_DIR}/android_package)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
  PATTERN "CMakeLists.txt" EXCLUDE
  PATTERN "template_deploy.json" EXCLUDE
)

# Do NOT copy the generic Qt Java bindings sources (QtActivity, QtService, etc.)
# as they are already provided in the Qt6Android*.jar files bundled with Qt.
# Copy only our custom application subclass sources.
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/org DESTINATION ${ANDROID_PACKAGE_DIR}/src)

file(COPY ${ANDROID_SHARED_CXX_LIB} DESTINATION libs/${CMAKE_ANDROID_ARCH_ABI})

# Build a shared library instead of an executable so that
# androiddeployqt can package it and Java can load it via
# System.loadLibrary().  The output name must be unique per ABI.
add_library(bitcoin-qt-android SHARED
  ../main.cpp
  ../../init/bitcoin-qt.cpp
)
if(DEFINED ANDROID_ABI)
  string(REPLACE "-" "_" _android_abi_sanitized "${ANDROID_ABI}")
  set_target_properties(bitcoin-qt-android PROPERTIES OUTPUT_NAME "bitcoin-qt_${_android_abi_sanitized}")
endif()
# Allow duplicate JNI_OnLoad coming from QtCore and the platform plugin.
target_link_options(bitcoin-qt-android PRIVATE -Wl,--allow-multiple-definition)

# Set the same compile definitions and include directories as the main bitcoinqt library
target_compile_definitions(bitcoin-qt-android
  PRIVATE
    QT_NO_KEYWORDS
    QT_USE_QSTRINGBUILDER
)

target_include_directories(bitcoin-qt-android
  PRIVATE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
)

# Link the same libraries as the main bitcoin-qt executable
target_link_libraries(bitcoin-qt-android
  core_interface
  bitcoinqt
  bitcoin_node
)

# Import Qt plugins for Android
import_plugins(bitcoin-qt-android)

# Provide a custom Android package source directory so that a user-supplied
# AndroidManifest.xml (and any extra Java/Kotlin sources or resources) found
# under <project>/android will be merged into the package produced by
# androiddeployqt. This lets us tweak the manifest without touching Qt's
# own template. In addition, pass command-line arguments to the Qt loader
# via the dedicated manifest <meta-data> entry.
set_target_properties(bitcoin-qt-android PROPERTIES
  QT_ANDROID_PACKAGE_SOURCE_DIR "${PROJECT_SOURCE_DIR}/android"
  # Qt's deployment JSON generator (qt6_android_generate_deployment_settings)
  # doesn't currently pick up QT_ANDROID_PACKAGE_SOURCE_DIR. Instead it looks
  # for a private property called _qt_android_native_package_source_dir. Set
  # it explicitly so the generated armv7a_deploy.json contains the
  # "android-package-source-directory" entry and thus makes
  # androiddeployqt copy our custom template from <project>/android verbatim.
  _qt_android_native_package_source_dir "${PROJECT_SOURCE_DIR}/android"
  QT_ANDROID_APPLICATION_ARGUMENTS "-signet"
)

# cmake_path(RELATIVE_PATH GRADLEW_EXECUTABLE BASE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# === Generate armv7a_deploy.json ==========================================
# The template contains placeholder paths pointing to /home/user/... . Replace
# those with the actual locations discovered from the tool-chain and build
# variables, then write the processed JSON to the build directory.

set(TEMPLATE_DEPLOY_JSON ${CMAKE_CURRENT_SOURCE_DIR}/template_deploy.json)
set(DEPLOY_JSON_PATH     ${CMAKE_BINARY_DIR}/${CMAKE_ANDROID_ARCH_ABI}_deploy.json)

if(NOT EXISTS "${TEMPLATE_DEPLOY_JSON}")
  message(FATAL_ERROR "Expected template deploy file not found at ${TEMPLATE_DEPLOY_JSON}")
endif()

# Derive relevant paths from variables populated by the depends tool-chain.
if(DEFINED ANDROID_SDK_ROOT)
  set(_sdk_path "${ANDROID_SDK_ROOT}")
elseif(DEFINED ENV{ANDROID_SDK})
  set(_sdk_path "$ENV{ANDROID_SDK}")
else()
  message(FATAL_ERROR "ANDROID_SDK_ROOT / $ENV{ANDROID_SDK} is not defined; cannot generate deploy json")
endif()

if(DEFINED ANDROID_NDK_ROOT)
  set(_ndk_path "${ANDROID_NDK_ROOT}")
elseif(DEFINED ENV{ANDROID_NDK})
  set(_ndk_path "$ENV{ANDROID_NDK}")
else()
  message(FATAL_ERROR "ANDROID_NDK_ROOT / $ENV{ANDROID_NDK} is not defined; cannot generate deploy json")
endif()

# Qt install locations. Prefer the variables populated by the depends tool-chain
# (EXTERNAL_QT_ANDROID_SDK / EXTERNAL_QT_HOST_QT). If those are not available
# – e.g. when using the upstream Qt tool-chain file – fall back to CMake cache
# variables or environment variables.

if(DEFINED EXTERNAL_QT_ANDROID_SDK)
  set(_qt_android_sdk "${EXTERNAL_QT_ANDROID_SDK}")
elseif(DEFINED QT_ANDROID_SDK)
  set(_qt_android_sdk "${QT_ANDROID_SDK}")
elseif(DEFINED ENV{QT_ANDROID_SDK})
  set(_qt_android_sdk "$ENV{QT_ANDROID_SDK}")
else()
  message(FATAL_ERROR "QT_ANDROID_SDK / EXTERNAL_QT_ANDROID_SDK is not defined; cannot generate deploy json")
endif()

if(DEFINED EXTERNAL_QT_HOST_QT)
  set(_qt_host_qt "${EXTERNAL_QT_HOST_QT}")
elseif(DEFINED QT_HOST_QT)
  set(_qt_host_qt "${QT_HOST_QT}")
elseif(DEFINED ENV{QT_HOST_QT})
  set(_qt_host_qt "$ENV{QT_HOST_QT}")
else()
  message(FATAL_ERROR "QT_HOST_QT / EXTERNAL_QT_HOST_QT is not defined; cannot generate deploy json")
endif()

# Current package source dir inside build tree.
file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}" _package_src_dir)

file(READ "${TEMPLATE_DEPLOY_JSON}" _json_in)
# Perform straightforward string substitutions.

string(REPLACE "/home/user/Android/Sdk"        "${_sdk_path}"           _json_tmp "${_json_in}")
string(REPLACE "/home/user/Qt/6.7.3/gcc_64"    "${_qt_host_qt}"         _json_tmp "${_json_tmp}")
string(REPLACE "/home/user/bitcoin/build/src/qt/android" "${_package_src_dir}" _json_tmp "${_json_tmp}")
string(REPLACE "libplugins_platforms_qtforandroid_abi.so" "libplugins_platforms_qtforandroid_${CMAKE_ANDROID_ARCH_ABI}.so" _json_tmp "${_json_tmp}")
# NDK path replacement (it may include version sub-dir; just drop root match)
string(REPLACE "/home/user/Android/Sdk/ndk/26.1.10909125" "${_ndk_path}" _json_tmp "${_json_tmp}")

# Replace ABI-specific Qt paths
string(REPLACE "/home/user/Qt/6.7.3/android_armv7" "${_qt_android_sdk}" _json_tmp "${_json_tmp}")
string(REPLACE "/home/user/Qt/6.7.3/android_arm64" "${_qt_android_sdk}" _json_tmp "${_json_tmp}")
string(REPLACE "/home/user/Qt/6.7.3/android_x86_64" "${_qt_android_sdk}" _json_tmp "${_json_tmp}")
string(REPLACE "/home/user/Qt/6.7.3/android_abi" "${_qt_android_sdk}" _json_out "${_json_tmp}")

# Filter JSON to only include the current ABI
# Create a Python script to filter the JSON
set(FILTER_JSON_SCRIPT ${CMAKE_BINARY_DIR}/filter_json.py)
file(WRITE ${FILTER_JSON_SCRIPT}
"import json
import sys

# Read the JSON content from stdin
json_content = sys.stdin.read()
data = json.loads(json_content)

# Get the current ABI from command line argument
current_abi = sys.argv[1]

# Filter out all ABIs except the current one
for key in ['qt', 'qtDataDirectory', 'qtLibExecsDirectory', 'qtLibsDirectory', 'qtPluginsDirectory']:
    if key in data:
        # Keep only the current ABI
        filtered_dict = {}
        if current_abi in data[key]:
            filtered_dict[current_abi] = data[key][current_abi]
        data[key] = filtered_dict

# Filter architectures
if 'architectures' in data:
    filtered_arch = {}
    if current_abi in data['architectures']:
        filtered_arch[current_abi] = data['architectures'][current_abi]
    data['architectures'] = filtered_arch

# Output the filtered JSON
print(json.dumps(data, indent=2))
")

# Filter the JSON to only include the current ABI
execute_process(
    COMMAND ${CMAKE_COMMAND} -E echo "${_json_out}"
    COMMAND python3 ${FILTER_JSON_SCRIPT} ${CMAKE_ANDROID_ARCH_ABI}
    OUTPUT_VARIABLE _filtered_json
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

file(WRITE "${DEPLOY_JSON_PATH}" "${_filtered_json}")

# The deployment settings JSON we just ensured exists.
set(ANDROID_DEPLOY_JSON "${DEPLOY_JSON_PATH}")

# Path to androiddeployqt taken from the external host Qt installation that was
# passed into the depends tool-chain via the QT_HOST_QT variable. The variable
# EXTERNAL_QT_HOST_QT is defined in the generated tool-chain file.
set(ANDROIDDEPLOYQT_EXECUTABLE "${_qt_host_qt}/bin/androiddeployqt")

if(NOT EXISTS "${ANDROIDDEPLOYQT_EXECUTABLE}")
  message(FATAL_ERROR "androiddeployqt was not found at ${ANDROIDDEPLOYQT_EXECUTABLE}. Make sure that QT_HOST_QT points to a valid host Qt installation.")
endif()

add_custom_target(apk_package
  COMMAND ${CMAKE_COMMAND} -E make_directory ${ANDROID_PACKAGE_DIR}/libs/${CMAKE_ANDROID_ARCH_ABI}
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:bitcoin-qt-android> ${ANDROID_PACKAGE_DIR}/libs/${CMAKE_ANDROID_ARCH_ABI}/libbitcoin-qt_${CMAKE_ANDROID_ARCH_ABI}.so
  COMMAND ${ANDROIDDEPLOYQT_EXECUTABLE}
          --input ${ANDROID_DEPLOY_JSON}
          --output ${ANDROID_PACKAGE_DIR}
          --gradle --verbose
  VERBATIM
)
