#define XLOG_EN     0
#define XLOG_LEVEL  0
#include <xUniversal.h>
#include "ClipboardCapture.h"

/// @brief Main entry point. Initializes systems and runs the UI event loop.
/// @param argc Argument count.
/// @param argv Argument vector.
/// @return 0 on success, -1 on initialization failure.
int main(int argc, char *argv[]) {
    /// 1. Initialize background threads (Signal + X11)
    if (ClipboardCaptureInitialize() != OKE) {
        return -1;
    }

    xLog1("[Main] Systems initialized. Main thread is entering UI loop...");

    /// 2. Main UI Event Loop
    while (RequestExit != eACTIVATE) {
        
        /// Check if the Signal Thread requested the popup menu (via SIGUSR1)
        if (TogglePopUpStatus == eREQ_SHOW) {
            ShowRofiMenu();
            
            /// Reset the popup status to hidden after the menu closes
            TogglePopUpStatus = eHIDEN;
        }
        
        /// Sleep 10ms to prevent 100% CPU usage while idling
        usleep(10000); 
    }

    xLog1("[Main] Exit signal detected. Cleaning up...");

    /// 3. Cleanup is handled automatically by atexit(ClipboardCaptureFinalize)
    return 0;
}





































/// int main(int argc, char *argv[]) {
///     sClipboardItem TempItem;
///     char NameBuffer[NAME_MAX + 1]; /// +1 for safety null-terminator
///     
///     /// 1. Initialize directory environment
///     printf(">>> 1. Initializing DB Directory...\n");
///     if (EnsureDir(PATH_DIR_DB) != OKE) {
///         printf("Failed to ensure DB directories!\n");
///         return -1;
///     }
/// 
///     /// 2. Initial sync: Load data from disk to RAM
///     printf("\n>>> 2. Initial Syncing DB...\n");
///     XCBList_Scan(0); /// Dùng hàm Scan(0) vì nó đã gộp cả Sort và Purge
///     
///     int CurrentSize = XCBList_GetItemSize();
///     printf("Found %d existing items in DB.\n", CurrentSize);
/// 
///     /// 3. Iterate and print current history list
///     printf("\n>>> 3. Dumping current DB content:\n");
///     for (int i = 0; i < CurrentSize; i++) {
///         /// Clear buffer before reading binary data
///         memset(NameBuffer, 0, sizeof(NameBuffer));
///         
///         /// Retrieve metadata from the list FIRST
///         if (XCBList_GetItem(i, &TempItem) == OKE) {
///             /// Read file content as binary
///             XCBList_ReadAsBinary(i, (void *)(NameBuffer), NAME_MAX);
///             
///             /// Note: Accessing fields directly due to anonymous struct
///             printf("[%d] %-25s | TS: %ld | Data: %s\n", 
///                    i, 
///                    TempItem.Filename, 
///                    TempItem.Timestamp, 
///                    NameBuffer);
///         }
///     }
/// 
///     /// 4. Simulate a new capture (PushItem)
///     printf("\n>>> 4. Testing PushItem with a dummy file...\n");
///     char DummyFile[] = "20260214_160000.txt";
///     
///     /// Create a dummy file on disk so we can test the path extraction
///     char TouchCmd[PATH_MAX];
///     snprintf(TouchCmd, sizeof(TouchCmd), "touch %s/%s", PATH_DIR_DB, DummyFile);
///     system(TouchCmd); 
/// 
///     /// Test PushItem (nó sẽ tự gọi GetFileNameFromPath để bóc tách)
///     if (XCBList_PushItem(DummyFile) == OKE) {
///         printf("Push success! List size now: %d\n", XCBList_GetItemSize());
///     }
/// 
///     /// 5. Verify the latest item in the list
///     printf("\n>>> 5. Fetching Latest Item...\n");
///     if (XCBList_GetLatestItem(&TempItem) == OKE) {
///         printf("Latest Item is now: %s\n", TempItem.Filename);
///     }
/// 
///     /// 6. Test Error Handling & Manual Rescan
///     printf("\n>>> 6. Testing Manual Resync (Deleting a file physically)...\n");
///     snprintf(TouchCmd, sizeof(TouchCmd), "rm %s/%s", PATH_DIR_DB, DummyFile);
///     system(TouchCmd); 
/// 
///     /// ReadAsBinary will fail, and we will manually trigger the scan
///     uint8_t Buffer[1024];
///     printf("Attempting to read the deleted file (Index 0)...\n");
///     
///     if (XCBList_ReadAsBinary(0, Buffer, sizeof(Buffer)) != OKE) {
///         printf("--> Read failed as expected (File lost)!\n");
///         printf("--> Manually re-syncing DB...\n");
///         
///         XCBList_Scan(0); /// Dọn dẹp lại RAM cho khớp với đĩa
///         
///         printf("--> New List Size: %d\n", XCBList_GetItemSize());
///     }
/// 
///     printf("\n>>> Test Completed Successfully! <<<\n");
///     return 0;
/// }


