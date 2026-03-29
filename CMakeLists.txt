cmake_minimum_required(VERSION 3.16.0)
set(EXECUTABLE_COMPONENT_NAME "wled00")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# ── Matter / GCC 14 compatibility fixups ──────────────────────────────────────
# The CHIP SDK's TypeTraits.h has a C++23 branch with invalid code:
#   using to_underlying = std::to_underlying;  (type alias to function template)
# Downgrade from gnu++2b to gnu++20 so the correct #else polyfill is used.
# Guard: only apply when the Matter usermod is present.
if(EXISTS "${CMAKE_SOURCE_DIR}/usermods/matter_over_wifi")
  idf_build_replace_option_from_property(CXX_COMPILE_OPTIONS
    "-std=gnu++2b" "-std=gnu++20")
endif()

project(WLED)
