# About

A lightweight, modular C Shared Library designed to streamline low-level development. It provides essential utilities for thread-safe logging, condition checking, and standardized code flow control.

# Working-Tree

```
    fus@ngxxfus xUniversal 
    > tree
    .
    ├── Build <------------------------------------ Output when run Makefile 
    │   ├── include
    │   │   ├── xUniversal.h
    │   │   ├── xUniversalCondition.h
    │   │   ├── xUniversalLog.h
    │   │   ├── xUniversalLoop.h
    │   │   └── xUniversalReturn.h
    │   └── lib
    │       └── libxuniversal.so    <------------------- Shared lib 
    ├── Makefile
    ├── TestLib
    ├── TestLib.c  <---------------------------------- Test program (written in C)  
    ├── readme.md
    ├── xUniversal.c
    ├── xUniversal.h
    ├── xUniversalCondition.h
    ├── xUniversalLog.h
    ├── xUniversalLoop.h
    └── xUniversalReturn.h
```

# Installation (Void Linux / Unix)

## 1. Build 

Make a `build` directory, in root-dir, run make file

```
make install 
```

## 2. System Integration

To use this library globally (like a pro), create symlinks to your system directories.

**NOTE**: The location of the lib is `~/.fus/xUniversal/`, you need to change the path corresponding to your system.

### Link the Library:

```
sudo ln -sf $(pwd)/Build/lib/libxuniversal.so /usr/local/lib/libxuniversal.so
sudo ldconfig
```

### 

```
sudo ln -s $(pwd)/Build/include/*.h /usr/include/
```


# Demo 

Test code:
```
#include <xUniversal.h>

int main() {
    xEntry("main");

    xCoreLog("TEST", "Hello from shared library! (Via XCoreLog)");
    xLog("This is a log Lv0!!!");
    
    xExit("main");
    return 0;
}
```

Compile and run:
```
gcc TestLib.c -o TestLib  -lxuniversal && ./TestLib
```

Result:
```
[21:17:05.935968] [==>] main
[21:17:05.936013] [TEST] Hello from shared library! (Via XCoreLog)
[21:17:05.936016] [LOG] This is a log Lv0!!!
[21:17:05.936018] [<==] main
```
