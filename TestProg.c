#define XLOG_LEVEL 1
#include <xUniversal.h>
#include "ClipboardCapture.h"



int main(int argc, char *argv[]) {
    xEntry("TestProg/main");
    /* 1. Setup Logging (Nếu thư viện xUniversal cần init) */
    // xUniversal_Init(); 

    /* 2. Prepare Directories (Tạo thư mục DBs, ClipboardItem...) */
    if (EnsureDB() != OKE) {
        xError("System Check Failed! Exiting...");
        return -1;
    }

    /* 3. Start the Application Loop */
    xLog1(">>> Application Started: %s", PROG_NAME);

    /* Because the program is trapped above, CoreRuntime will NEVER be executed! */
    XClipboardRuntime(0); 
    /* The program will enter SignalRuntime and get TRAPPED inside its while loop. */
    SignalRuntime(0); 

    


    xExit("TestProg/main");
    return 0;
}

