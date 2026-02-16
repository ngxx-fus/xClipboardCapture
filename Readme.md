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
.XCBC_Data                                          <--------------------------- Application data
├── DBs                                             <--------------------------- All clipboard items
│   ├── 20260216_064911_312_7.txt
│   ├── ...
│   ├── 20260216_105059_406_18.txt
│   ├── 20260216_114622_925_19.png
│   └── 20260216_114631_607_20.png
└── XCBRofiMenu.txt                                 <--------------------------- Temporary file for RoFi menu

```

# Installation

## Prerequisites

### Void Linux

```ZSH
sudo xbps-install -S base-devel libxcb-devel zlib-devel rofi
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

Open Makefile (in current directory) then change the install path to somewhere you want to install:
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

After run `make clean all`, you will get a BinaryApplication named `xClipBoardCapture`


### Run a test before install

Now, you can run the application by `./xClipBoardCapture &` and can trigger the app by `kill -SIGUSR1 $(pidof xClipBoardCapture)`, if a Ro-Fi window will be shown, you can move to next step.

### Install

The installation just a thing that we copy the binary app to somewhere and start it every startup! You also use `make install` to install the binary application or manually copy.

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
# System overview



# DEMO

![DemoGIF](Imgs/Demo.gif)

## System flow (Generated by AI)

```
Overall System Architecture & Flow
──────────────────────────────────────

This is a clipboard history / clipboard manager for X11 that uses:
• XCB (low-level X11 protocol binding)
• XFixes extension (to detect clipboard ownership changes)
• INCR protocol support (for large data transfers > ~256 KB)
• 128 MB RAM buffer + disk fallback for receiving large clipboard items
• Three threads + semaphore + signals for coordination
• Optional Rofi UI integration

Three main threads:
1. Receiver Thread    → listens to X server events (main clipboard capture logic)
2. Provider Thread    → waits to push selected history item back to system clipboard
3. Signal Thread      → handles SIGUSR1, SIGUSR2, SIGINT/SIGTERM

Lifecycle:
ClipboardCaptureInitialize()
    ↓
    • Creates 128 MB receive buffer
    • Initializes semaphore (SemProviderWakeup)
    • Spawns 3 threads
    • Registers atexit(ClipboardCaptureFinalize)

ClipboardCaptureFinalize() (called at exit)
    • Sets RequestExit = true
    • Kills signal thread with pthread_kill(SIGINT)
    • Posts semaphore → wakes provider
    • Sends dummy X11 ClientMessage → wakes receiver
    • Joins all threads
    • Frees memory & destroys semaphore

──────────────────────────────────────
Receiver Thread (XClipboardRuntime_Receiver)
──────────────────────────────────────

Main loop: xcb_wait_for_event() → dispatch based on event type

Key event handlers:

1. XFixes Selection Notify (new clipboard owner)
   → HandleXFixesNotify()
       • Checks TransactionLock + timeout (5 seconds)
       • Sets TransactionLock = 1
       • Deletes old property
       • Requests TARGETS via xcb_convert_selection(..., AtomTarget, ...)

2. SelectionNotify (response to our convert_selection request)
   → HandleSelectionNotify()
       if target == TARGETS
           → HandleSelectionNotify_Negotiate()
               • Picks best format: image/png > image/jpeg > image/bmp > UTF8_STRING
               • Requests that format via another convert_selection()

       if target == chosen format (png/jpeg/bmp/utf8)
           → HandleSelectionNotify_ReceiveAndSave()
               • Creates unique filename + opens file
               if reply type == INCR
                   • Enters incremental mode (IsReceivingIncr = 1)
                   • Waits for PropertyNotify events
               else (single-shot / small data)
                   • PushToCache() the initial chunk
                   • Drains remaining data with multiple get_property calls
                   • FinalizeTransactionAndUnlock() → writes to disk + adds to history

3. PropertyNotify (INCR chunks arriving)
   → HandlePropertyNotify()  [receiver part]
       if IsReceivingIncr && NEW_VALUE
           • Reads chunk (up to 256 KB)
           • Drains remaining hidden data (loop get_property)
           • PushToCache()
           • Deletes property → signals sender to send next chunk
           • If chunk length == 0 → end of transfer → FinalizeTransactionAndUnlock()

4. SelectionRequest (another app wants our clipboard data)
   → HandlePropertyNotify()  [provider part]
       if PROPERTY_DELETE on our outgoing property
           • Sends next 64 KB chunk via change_property()
           • If no more data → sends 0-byte chunk → ends transaction

──────────────────────────────────────
Provider Thread (XClipboardRuntime_Provider)
──────────────────────────────────────

Blocks on: sem_wait(&SemProviderWakeup)

Wakes up when:
• SIGUSR2 received → sem_post() inside SignalEventHandler
• User selects item in Rofi → sem_post() inside ShowRofiMenu

Main action when ReqTestInject == true:
• Gets selected item from XCBList
• Determines target atom (png/jpeg/bmp/utf8)
• Reads file content into static 8 MB buffer
• Calls SetClipboardData() → copies data to ActiveData + claims CLIPBOARD ownership

──────────────────────────────────────
Signal Thread (SignalRuntime)
──────────────────────────────────────

Main loop: pause() → woken by any signal

Handles:
• SIGINT / SIGTERM → RequestExit = true
• SIGUSR1         → toggles TogglePopUpStatus (REQ_SHOW / REQ_HIDE)
                     (currently just logs — probably meant to show/hide Rofi)
• SIGUSR2         → sets ReqTestInject = true + sem_post(&SemProviderWakeup)

──────────────────────────────────────
Key Functions & Call Points Summary
──────────────────────────────────────

• ClipboardCaptureInitialize()
    Entry point → sets up semaphore, buffer, spawns threads

• ClipboardCaptureFinalize()
    Cleanup → wakes all threads, joins, frees resources

• SetClipboardData()
    Called from Provider thread when injecting history item
    → allocates ActiveData, memcpy, claims selection ownership

• HandleXFixesNotify()
    Called when someone else copies something
    → starts TARGETS negotiation

• HandleSelectionNotify()
    Called when we receive answer to convert_selection
    → either negotiates format or receives actual data

• HandlePropertyNotify()
    Dual role:
    • Receiver: receives INCR chunks → PushToCache()
    • Provider: detects PROPERTY_DELETE → sends next chunk

• HandleSelectionRequest()
    Called when another app wants our clipboard content
    • Supports TARGETS, TIMESTAMP, and actual data (single-shot or INCR)

• PushToCache()
    Helper → writes to 128 MB buffer, flushes to disk when full

• FinalizeTransactionAndUnlock()
    Helper → flushes remaining buffer, closes file, pushes to XCBList, resets lock

• ShowRofiMenu()  (if ROFI_SUPPORT)
    Called when user triggers UI (usually via SIGUSR1 or external script)
    → generates rofi input file → runs rofi → parses selection → sets ReqTestInject + sem_post

──────────────────────────────────────

Typical capture flow (when user copies something):
App A → copies image/text → X server → XFixesNotify → Request TARGETS → negotiate format → Request data → INCR or single-shot → save to file → add to history list

Typical paste-from-history flow:
User selects item (Rofi or hotkey) → SIGUSR2 or direct call → Provider wakes → read file → SetClipboardData() → claim CLIPBOARD → user pastes with Ctrl+V

```