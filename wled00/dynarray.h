/* dynarray.h

Macros for generating a "dynamic array", a static array of objects declared in different translation units

*/

#pragma once

// Declare the beginning and ending elements of a dynamic array of 'type'.
// This must be used in only one translation unit in your program for any given array.
#define DECLARE_DYNARRAY(type, array_name) \
  static type const DYNARRAY_BEGIN(array_name)[0] __attribute__((__section__(DYNARRAY_SECTION "." #array_name ".0"), unused)) = {}; \
  static type const DYNARRAY_END(array_name)[0] __attribute__((__section__(DYNARRAY_SECTION "." #array_name ".99999"), unused)) = {};

// Declare an object that is a member of a dynamic array.  "member name" must be unique; "array_section" is an integer for ordering items.
// It is legal to define multiple items with the same section name; the order of those items will be up to the linker.
#define DYNARRAY_MEMBER(type, array_name, member_name, array_section) type const member_name __attribute__((__section__(DYNARRAY_SECTION "." #array_name "." #array_section), used))

#define DYNARRAY_BEGIN(array_name) array_name##_begin
#define DYNARRAY_END(array_name) array_name##_end
#define DYNARRAY_LENGTH(array_name) (&DYNARRAY_END(array_name)[0] - &DYNARRAY_BEGIN(array_name)[0])

#ifdef ESP8266
// ESP8266 linker script cannot be extended with a unique section for dynamic arrays.
// We instead pack them in the ".dtors" section, as it's sorted and uploaded to the flash
// (but will never be used in the embedded system)
#define DYNARRAY_SECTION ".dtors"

#else /* ESP8266 */

// Use a unique named section; the linker script must be extended to ensure it's correctly placed.
#define DYNARRAY_SECTION ".dynarray"

#endif
