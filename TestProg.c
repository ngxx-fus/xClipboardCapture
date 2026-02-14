#define XLOG_LEVEL 1
#include <xUniversal.h>
#include "ClipboardCapture.h"



int main(int argc, char *argv[]) {
    xEntry("TestProg/main");

    ClipboardCaptureInitialize();

    xExit("TestProg/main");
    return 0;
}

