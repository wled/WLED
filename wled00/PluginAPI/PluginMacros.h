/**
 * (c) 2026 Joachim Dick
 * Licensed under the EUPL v. 1.2 or later
 */

#pragma once

//--------------------------------------------------------------------------------------------------

/** Place this macro in the headerfile of the custom plugin API.
 * Argument \a API_type is the class name of the plugin's interface:
 * @code
 * // in PluginAPI/custom/MyStuff/MyStuff.h
 * #include "PluginAPI/PluginMacros.h"
 * class MyStuff
 * {
 * public:
 *   virtual void doSomething() = 0;
 * };
 * DECLARE_PLUGIN_API(MyStuff);
 * @endcode
 */
#define DECLARE_PLUGIN_API(API_type) extern API_type *PLUGINAPI_##API_type __attribute__((weak))

/** Place this macro in the sourcefile of the custom plugin API.
 * @note Every API needs its own sourcefile, which must always be compiled; regardless whether the
 * corresponding usermod is enabled or not. It usually contains just the few lines that are shown
 * below. Be aware that the header and the cpp file of the API \e must be located in a different
 * directory than the actual usermod implementation!
 * @code
 * // in PluginAPI/custom/MyStuff/MyStuff.cpp
 * #include "MyStuff.h"
 * CPPFILE_PLUGIN_API(MyStuff);
 * @endcode
 */
#define CPPFILE_PLUGIN_API(API_type) API_type *PLUGINAPI_##API_type __attribute__((weak)) = nullptr

/** Place this macro in the sourcefile of the corresponding usermod implementation.
 * @note The usermod itself doesn't necessarily need to have its own headerfile; everything may be
 * contained inside its cpp file.
 * @code
 * // in usermods/UM_MyStuff/UM_MyStuff.cpp
 * #include "PluginAPI/custom/MyStuff/MyStuff.h"
 * class UM_MyStuff : public MyStuff
 * {
 *   void doSomething() override { ... }
 * };
 * static UM_MyStuff um_MyStuff;
 * REGISTER_USERMOD(um_MyStuff);
 * DEFINE_PLUGIN_API(MyStuff, um_MyStuff);
 * @endcode
 */
#define DEFINE_PLUGIN_API(API_type, instance) API_type *PLUGINAPI_##API_type = &instance

/** Use this macro at all places where you want to interact through the plugin API.
 * @code
 * // in usermods/UM_AnotherUsermod/UM_AnotherUsermod.cpp
 * #include "PluginAPI/custom/MyStuff/MyStuff.h"
 * // ...
 * MyStuff* myStuff = GET_PLUGIN_API(MyStuff);
 * if (myStuff)
 *   myStuff->doSomething();
 * // else: WLED was compiled without the corresponding usermod
 * @endcode
 */
#define GET_PLUGIN_API(API_typee) PLUGINAPI_##API_typee

//--------------------------------------------------------------------------------------------------
