#include <xUniversal.h>

int main() {
    xEntry("main");

    xCoreLog("TEST", "Hello from shared library! (Via XCoreLog)");
    xLog("This is a log Lv0!!!");
    
    xExit("main");
    return 0;
}
