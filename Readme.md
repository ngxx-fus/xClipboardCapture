XClipboardCapture (With Rofi)
========================
# About 


# Directory Tree

A overview of the sourcecode:

```
fus@ngxxfus xClipboardCapture git:(master)
> tree -a
.
├── .git
├── .gitignore
├── .gitmodules
├── CBC_Setup.h                                   <--------------------------- General configuration (Path/...)
├── CBC_SysFile.c
├── CBC_SysFile.h                                 <--------------------------- Utils for file/dir manager
├── ClipboardCapture.c
├── ClipboardCapture.h                            <--------------------------- Utils for intercommunication with X-Server, receive and provide data
├── Doc
│   ├── Doxygen                                   <--------------------------- For generate document
│   └── doxygen-awesome-css
├── Makefile                                      <--------------------------- Makefile for Compile/Run/Install (*)
├── xClipBoardCapture.c                           <--------------------------- Application
├── xClipBoardCapture                             <--------------------------- Binary Application (Run with no dependancy)
└── xUniversal                                    <--------------------------- Log, Other utils Lib
    ├── .git
    ├── Build                                     <--------------------------- Output after run 2nd makefile, do not edit!
    │   ├── include
    │   │   ├── xUniversal.h
    │   │   ├── xUniversalCondition.h
    │   │   ├── xUniversalLog.h
    │   │   ├── xUniversalLoop.h
    │   │   └── xUniversalReturn.h
    │   └── lib
    │       └── libxuniversal.so
    ├── Makefile                                  <--------------------------- 2nd makefile for xUniversal lib 
    ├── TestLib.c
    ├── readme.md
    ├── xUniversal.c
    ├── xUniversal.h
    ├── xUniversalCondition.h
    ├── xUniversalLog.h
    ├── xUniversalLoop.h
    └── xUniversalReturn.h
```

The database directory (application data directory)
```
fus@ngxxfus .fus
> tree .XCBC_Data
.XCBC_Data                                        <--------------------------- Application data
├── DBs                                           <--------------------------- All clipboard items
│   ├── 20260215_220833.txt
│   ├── 20260215_220847.txt
│   ├── 20260215_220939.txt
│   ├── 20260215_221014.txt
│   ├── 20260215_221100.txt
│   ├── 20260215_221142.txt
│   ├── 20260215_221152.txt
│   ├── 20260215_221335.txt
│   ├── 20260215_221537.txt
│   ├── 20260215_221729.txt
│   ├── 20260215_222108.txt
│   ├── 20260215_222134.txt
│   ├── 20260215_222416.txt
│   ├── 20260215_222520.txt
│   ├── 20260215_222524.txt
│   ├── 20260215_222525.txt
│   ├── 20260215_222529.txt
│   ├── 20260215_222540.txt
│   ├── 20260215_222543.txt
│   ├── 20260215_222553.txt
│   ├── 20260215_222620.txt
│   ├── 20260215_222621.txt
│   ├── 20260215_222844.txt
│   ├── 20260215_222856.txt
│   ├── 20260215_223011.txt
│   └── 20260215_223031.txt
└── XCBRofiMenu.txt                                <--------------------------- Temporary file for RoFi menu
```

# Installation

## Prerequisites

```ZSH
sudo xbps-install -S xcb-util-devel xcb-util-xfixes-devel zlib-devel rofi
```

## Clone the source code

Go to somewhere you want to install the run git clone:`git clone --recursive https://github.com/ngxx-fus/xClipboardCapture`

Log:
```
fus@ngxxfus Documents
> git clone --recursive https://github.com/ngxx-fus/xClipboardCapture
Cloning into 'xClipboardCapture'...
remote: Enumerating objects: 107, done.
remote: Counting objects: 100% (107/107), done.
remote: Compressing objects: 100% (63/63), done.
remote: Total 107 (delta 59), reused 89 (delta 41), pack-reused 0 (from 0)
Receiving objects: 100% (107/107), 462.88 KiB | 2.86 MiB/s, done.
Resolving deltas: 100% (59/59), done.
Submodule 'Doc/doxygen-awesome-css' (https://github.com/jothepro/doxygen-awesome-css.git) registered for path 'Doc/doxygen-awesome-css'
Submodule 'xUniversal' (https://github.com/ngxx-fus/xUniversal.git) registered for path 'xUniversal'
Cloning into '/home/fus/Documents/xClipboardCapture/Doc/doxygen-awesome-css'...
remote: Enumerating objects: 2996, done.
remote: Counting objects: 100% (485/485), done.
remote: Compressing objects: 100% (129/129), done.
remote: Total 2996 (delta 407), reused 356 (delta 356), pack-reused 2511 (from 2)
Receiving objects: 100% (2996/2996), 8.48 MiB | 17.37 MiB/s, done.
Resolving deltas: 100% (2279/2279), done.
Cloning into '/home/fus/Documents/xClipboardCapture/xUniversal'...
remote: Enumerating objects: 36, done.
remote: Counting objects: 100% (36/36), done.
remote: Compressing objects: 100% (26/26), done.
remote: Total 36 (delta 13), reused 27 (delta 9), pack-reused 0 (from 0)
Receiving objects: 100% (36/36), 17.96 KiB | 593.00 KiB/s, done.
Resolving deltas: 100% (13/13), done.
Submodule path 'Doc/doxygen-awesome-css': checked out '1f3620084ff75734ed192101acf40e9dff01d848'
Submodule path 'xUniversal': checked out '2a0ba5216623f7fb7d1115cf750d0a2e6c538041'
```

The directory tree:
```
fus@ngxxfus xClipboardCapture git:(master)
> tree -a
.
├── .git
├── .gitignore
├── .gitmodules
├── CBC_Setup.h
├── CBC_SysFile.c
├── CBC_SysFile.h
├── ClipboardCapture.c
├── ClipboardCapture.h
├── Doc
│   ├── Doxygen
├── Makefile
├── xClipBoardCapture.c
└── xUniversal
    ├── .git
    ├── Build
    │   ├── include
    │   │   ├── xUniversal.h
    │   │   ├── xUniversalCondition.h
    │   │   ├── xUniversalLog.h
    │   │   ├── xUniversalLoop.h
    │   │   └── xUniversalReturn.h
    │   └── lib
    │       └── libxuniversal.so
    ├── Makefile
    ├── TestLib.c
    ├── readme.md
    ├── xUniversal.c
    ├── xUniversal.h
    ├── xUniversalCondition.h
    ├── xUniversalLog.h
    ├── xUniversalLoop.h
    └── xUniversalReturn.h

65 directories, 149 files
```

## Build the application

### Config install path

Open Makefile (in current directory) then change the install path:
```
INSTALL_PATH_DIR=$(HOME)/.fus/
```
### Run `make`

To make BinaryApplication, you need to run: `make clean all`

LOG:
```
fus@ngxxfus xClipboardCapture git:(master)
> make clean all
>>> Cleaning up ClipboardCapture...
rm -f xClipBoardCapture CBC_SysFile.o ClipboardCapture.o xClipBoardCapture.o
>>> Cleaning up xUniversal Submodule...
make[1]: Entering directory '/home/fus/Documents/xClipboardCapture/xUniversal'
rm -vrf xUniversal.o libxuniversal.so TestLib
rm -vrf ./Build
removed './Build/include/xUniversal.h'
removed './Build/include/xUniversalLoop.h'
removed './Build/include/xUniversalCondition.h'
removed './Build/include/xUniversalReturn.h'
removed './Build/include/xUniversalLog.h'
removed directory './Build/include'
removed './Build/lib/libxuniversal.so'
removed directory './Build/lib'
removed directory './Build'
make[1]: Leaving directory '/home/fus/Documents/xClipboardCapture/xUniversal'
>>> Checking xUniversal Submodule...
make[1]: Entering directory '/home/fus/Documents/xClipboardCapture/xUniversal'
gcc -Wall -Wextra -fPIC -O2 -c xUniversal.c -o xUniversal.o
gcc -shared -o libxuniversal.so xUniversal.o
Installing to ./Build...
mkdir -p ./Build/include ./Build/lib
cp *.h ./Build/include/
cp libxuniversal.so ./Build/lib/
Cleaning up local build files...
rm -f libxuniversal.so xUniversal.o
Done! Library is ready at ./Build.
make[1]: Leaving directory '/home/fus/Documents/xClipboardCapture/xUniversal'
>>> Compiling CBC_SysFile.c...
gcc -std=gnu11 -O2 -Wall -Wextra -I. -I./xUniversal/Build/include -c CBC_SysFile.c -o CBC_SysFile.o
CBC_SysFile.c: In function 'EnsureDB':
CBC_SysFile.c:722:17: warning: unused variable 'Status' [-Wunused-variable]
  722 |     struct stat Status;
      |                 ^~~~~~
CBC_SysFile.c: At top level:
CBC_SysFile.c:77:12: warning: 'Convert2LinearIndex' defined but not used [-Wunused-function]
   77 | static int Convert2LinearIndex(int AllocatedIndex) {
      |            ^~~~~~~~~~~~~~~~~~~
>>> Compiling ClipboardCapture.c...
gcc -std=gnu11 -O2 -Wall -Wextra -I. -I./xUniversal/Build/include -c ClipboardCapture.c -o ClipboardCapture.o
ClipboardCapture.c: In function 'SignalRuntime':
ClipboardCapture.c:423:27: warning: unused parameter 'Param' [-Wunused-parameter]
  423 | RetType SignalRuntime(int Param) {
      |                       ~~~~^~~~~
ClipboardCapture.c: In function 'XClipboardRuntime':
ClipboardCapture.c:865:96: warning: comparison of integer expressions of different signedness: '__off_t' {aka 'long int'} and 'long unsigned int' [-Wsign-compare]
  865 |                 if (stat(FullPath, &FileStat) == 0 && FileStat.st_size > 0 && FileStat.st_size <= sizeof(RawData)) {
      |                                                                                                ^~
ClipboardCapture.c:817:31: warning: unused parameter 'Param' [-Wunused-parameter]
  817 | RetType XClipboardRuntime(int Param) {
      |                           ~~~~^~~~~
ClipboardCapture.c: In function 'ClipboardCaptureInitialize':
ClipboardCapture.c:975:52: warning: cast between incompatible function types from 'RetType (*)(int)' {aka 'int (*)(int)'} to 'void * (*)(void *)' [-Wcast-function-type]
  975 |     if (pthread_create(&SignalRuntimeThread, NULL, (void *(*)(void *))SignalRuntime, NULL) != 0) {
      |                                                    ^
ClipboardCapture.c:981:56: warning: cast between incompatible function types from 'RetType (*)(int)' {aka 'int (*)(int)'} to 'void * (*)(void *)' [-Wcast-function-type]
  981 |     if (pthread_create(&XClipboardRuntimeThread, NULL, (void *(*)(void *))XClipboardRuntime, NULL) != 0) {
      |                                                        ^
>>> Compiling xClipBoardCapture.c...
gcc -std=gnu11 -O2 -Wall -Wextra -I. -I./xUniversal/Build/include -c xClipBoardCapture.c -o xClipBoardCapture.o
xClipBoardCapture.c: In function 'main':
xClipBoardCapture.c:10:14: warning: unused parameter 'argc' [-Wunused-parameter]
   10 | int main(int argc, char *argv[]) {
      |          ~~~~^~~~
xClipBoardCapture.c:10:26: warning: unused parameter 'argv' [-Wunused-parameter]
   10 | int main(int argc, char *argv[]) {
      |                    ~~~~~~^~~~~~
>>> Linking xClipBoardCapture...
gcc -std=gnu11 -O2 -Wall -Wextra -I. -I./xUniversal/Build/include -o xClipBoardCapture CBC_SysFile.o ClipboardCapture.o xClipBoardCapture.o -L./xUniversal/Build/lib -Wl,-rpath,./xUniversal/Build/lib -lxcb -lxcb-xfixes -lz -lxuniversal -lpthread
>>> Build successful!
```

NOTE: There are some warns, but it can be skipped =)))) Don't worry about that!

After run `make`, you will get a BinaryApplication named `xClipBoardCapture`

### Run a test before install

Now, you can run the application by `./xClipBoardCapture &` and can trigger the app by `kill -SIGUSR1 $(pidof xClipBoardCapture)`, if a Ro-Fi window will be shown, you can move to next step.

### Install

The installation just a thing that we copy the binary app to somewhere and start it every startup!


# Configuration
## Basic configuration

A basic configuration means a config for application data path, max history size...

### CBC_Setup.h
```

/**
 * @brief Maximum number of items retained in the clipboard history ring buffer.
 */
#define MAX_HISTORY_ITEMS       1000 

/**
 * @brief Root directory for all temporary runtime files.
 */
#define PATH_DIR_ROOT           "/home/fus/.fus/.XCBC_Data"

/**
 * @brief Sub-directory storing the raw clipboard data chunks/files.
 */
#define PATH_DIR_DB             PATH_DIR_ROOT "/DBs"

/**
 * @brief Path to the file storing the serialized clipboard history list.
 */
#define PATH_ITEM               PATH_DIR_ROOT "/ClipboardItem"

/**
 * @brief Path to the temporary text file used to feed the Rofi menu.
 */
#define PATH_FILE_ROFI_MENU     PATH_DIR_ROOT "/XCBRofiMenu.txt"

/**
 * @brief Toggle switch to enable (1) or disable (0) Rofi UI integration.
 */
#define ROFI_SUPPORT            1

/**
 * @brief Maximum character length for text previews in the Rofi menu.
 */
#define PREVIEW_TXT_LEN         80

```

## Advanced configuration

Advanced configuration means that config for log, debug, and for developing new feature!

xUniversal/xUniversalLog.h:
```
#ifndef XLOG_EN
    #define XLOG_EN                     1
#endif
#ifndef XLOG_LEVEL
    #define XLOG_LEVEL                  0 /*MIN:Lv0 to MAX:Lv2*/
#endif
```


# DEMO

![DemoGIF](Imgs/Demo.gif)
