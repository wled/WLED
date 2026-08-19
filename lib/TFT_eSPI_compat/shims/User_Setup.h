#pragma once
// TFT_eSPI_compat: dependency shim.
//
// At compile time this file is NOT used — TFT_eSPI.h's __has_include(<tft_setup.h>)
// fires first, so USER_SETUP_LOADED is defined before User_Setup_Select.h ever
// reaches the fallback #include <User_Setup.h>.
//
// The only purpose of this file is to give SCons's C/C++ header scanner a plain
// #include directive that it can follow to discover tft_setup.h as a build
// dependency, ensuring the Command that generates tft_setup.h runs before any
// TFT_eSPI source file is compiled.
#include <tft_setup.h>
