
# Development Notes on Plugins

The igniting spark for this new WLED feature came from [this issue](https://github.com/wled/WLED/issues/5290) by @blazoncek , and some comments from @softhack007 and @willmmiles in the discussion there. Kudos to them for the great ideas!

These notes describe why some parts of this plugin design are as they are. And which problems during development led to those design decisions.


## Why having a dedicated API for a usermod; why not using the usermod class directly by other usermods?

(1) To break dependencies. Imagine, your usermod wants to call functions on another usermod, that for examples controls a display via I2C. The class definition (yet not the implementation) of that other usermod would have to be inside its headerfile, which your usermod must include. <br>
Now, since that usermod uses I2C, it will depend on any kind of I2C library, and have an 'I2C-driver' as member variable. As a consequence, your usermod will implicitly pull in all these dependencies of the other usermod. This is _working only if the other usermod is also enabled_, so its dependencies are getting installed automatically by PlatformIO, and the include paths are set appropriately by the build system. <br>
The problem arises, when your usermod wants to access the other usermod _optionally_ - i.e. only when it has been compiled into WLED. If it is missing, you want to detect that inside your usermod at runtime, and maybe just skip the part with the display interaction. You'll end up with a compiler error: your usermod includes the other's headerfile, which in turn includes the I2C driver's headerfile. Unfortunately, that I2C driver is not installed (and won't be), because no one else needed it yet. So, the compiler quits because of a missing headerfile - Game Over. <br>

(2) An alternative solution would be to use `#define`s, which are set by the other usermod. Ideas about how to do this have already been pointed out in the issue's thread by @softhack007 . However, that approach does _not_ work out of the box, because of some magic performed by the Arduino framework; see below for more details on that problem.

(3) See the failing build as a feature and not as a problem. Just include the other's headerfile in your usermod, and use it like any other class. Therefore, make its object accessible through its headerfile. (How to do that has also been shown in the issue's thread by @willmmiles ) <br>
Now, when the build fails, this is a reminder for you that you forgot to enable the other usermod as well. But as a consequence, the 'optional' part of detecting the other usermod is then gone.

In the end, it will be the developer's choice which option to choose; there is not only one right way. This PR shows an additional clean and interface-based way - at the expense of virtual functions. However, when weighing the cost of the vtable against the feature gain and simplicity, I am happily willing to pay that price.


## Why a dedicated folder for custom usermod APIs - shouldn't they live in the same folder as the usermod itself?

Short answer: Yes, absolutely! Long answer: Unfortunately that doesn't work in practice.
The problem here is some magic that the underlying Arduino framework (I assume) is performing under the hood: <br>
Whenever your sourcecode includes a headerfile from the directory of another usermod, that usermod's cpp files will be compiled automatically. Regardless if that usermod is enabled or not! This always ends with a failed build in the latter case, because e.g. include paths are not set appropriately. At least that was my experience during development. If anyone knows how to prevent that from happening: solutions are really welcome!


## Why does a custom usermod API need a cpp file, when it is just an interface?

This is necessary because of the way _weak symbols_ are handled by the linker. At least how far I understand them as of now, after some contradictory adventures with AI. Nevertheless, this idea was with weak linking was brought up by @softhack007 in the issue's thread - and was the initial spark for me to start tinkering on that topic; just out of curiosity. <br>
My experience during development was that weak symbols are **not** NULL when they are missing. It seems as if they are undefined (i.e. random), which leads to a crash. Apparently, we need an additional _weak implementation_ of such a _weak symbol_, which is initialized with NULL (that's happening inside this cpp file). When the (optional) usermod is missing, that _weak implementation_ is picked by the linker, and can be checked for NULL by the user. <br>
Now, when the actual usermod is enabled, it contains a _strong implementation_ of that _weak symbol_ - which the linker will pick instead of the weak one from this cpp file. Thus the user obtained a valid pointer to the usermod object (not NULL), and can now directly interact with it. <br>
If anyone knows a solution how the linker will default missing weak symbols to NULL, please share! Then we can get rid of this cpp file and the corresponding macro completely!


## The PluginManager's info section in the UI is a bit bloated and messy...

Yes, indeed... The current implementation is intended as demonstration and for debugging.
For release code, it may also be removed completely since it isn't essentially needed.
My idea would be to make its 'bloatiness' configurable via `#define`s, like `PLUGINMGR_DISABLE_UI`. Any suggestions about what would be of interest are highly appreciated!
