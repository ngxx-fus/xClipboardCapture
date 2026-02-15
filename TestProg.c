#define XLOG_LEVEL 1
#include <xUniversal.h>
#include "ClipboardCapture.h"

#if (ROFI_SUPPORT==1)

    #define PREVIEW_TXT_LEN 80

/// @brief Reads file content and writes a formatted Rofi menu item directly to the temp file.
/// @param OutFile The temporary file stream (/tmp/cbc_rofi.txt).
    /// @param Index The logical index of the clipboard item.
    /// @param Item Pointer to the clipboard item data.
    /// @return OKE on success.
    RetType WriteRofiMenuItem(FILE *OutFile, int Index, sClipboardItem *Item) {
        char FullPath[PATH_MAX];
        snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, Item->Filename);

        if (Item->FileType == eFMT_IMG_PNG || Item->FileType == eFMT_IMG_JGP) {
            /// For images: No preview text, just print "[Image]" and pass the path as Thumbnail
            fprintf(OutFile, "%d: [Image] %s%cicon\x1f%s\n", 
                    Index, Item->Filename, '\0', FullPath);
        } 
        else {
            /// For text: Read file content to generate a preview string
            FILE *fp = fopen(FullPath, "rb");
            char Preview[PREVIEW_TXT_LEN + 1];
            memset(Preview, 0, sizeof(Preview));
            
            if (fp) {
                /// Read up to PREVIEW_TXT_LEN characters
                size_t ReadBytes = fread(Preview, 1, PREVIEW_TXT_LEN, fp);
                fclose(fp);
                
                /// Safely null-terminate the string
                Preview[ReadBytes] = '\0';
                
                /// [CRITICAL STEP]: Filter out newlines (\n, \r) and tabs (\t)
                /// Without this, Rofi will break lines and the index parsing logic will fail!
                for (size_t i = 0; i < ReadBytes; i++) {
                    if (Preview[i] == '\n' || Preview[i] == '\r' || Preview[i] == '\t') {
                        Preview[i] = ' '; 
                    }
                }
                
                /// If the text is longer than the allowed limit, append "[...]"
                if (ReadBytes == PREVIEW_TXT_LEN) {
                    Preview[PREVIEW_TXT_LEN - 5] = '[';
                    Preview[PREVIEW_TXT_LEN - 4] = '.';
                    Preview[PREVIEW_TXT_LEN - 3] = '.';
                    Preview[PREVIEW_TXT_LEN - 2] = '.';
                    Preview[PREVIEW_TXT_LEN - 1] = ']';
                }
                
                fprintf(OutFile, "%d: %s%cicon\x1ftext-x-generic\n", 
                        Index, Preview, '\0');
            } 
            else {
                /// Fallback in case the file is missing or deleted
                fprintf(OutFile, "%d: [Empty/Missing File]%cicon\x1ftext-x-generic\n", 
                        Index, '\0');
            }
        }
        return OKE;
    }


    /// @brief Calls the Rofi dmenu interface to let the user select a clipboard item.
    void ShowRofiMenu(void) {
        xEntry1("ShowRofiMenu");
        
        /// 1. Create a temporary file to hold the menu items
        FILE *tmp = fopen("/tmp/cbc_rofi.txt", "w");
        if (!tmp) {
            xError("[UI] Failed to create temp file for Rofi.");
            return;
        }

        /// 2. Dump the current RAM list into the text file with PREVIEW and ICON tags
        int size = XCBList_GetItemSize();
        for (int i = 0; i < size; i++) {
            sClipboardItem item;
            if (XCBList_GetItem(i, &item) == OKE) {
                /// Gọi hàm Helper để nó tự đọc và ghi vào file tmp
                WriteRofiMenuItem(tmp, i, &item);
            }
        }
        fclose(tmp);

        /// 3. Execute Rofi and capture its output
        FILE *rofi = popen("rofi -dmenu -i -show-icons -p 'X11 Clipboard' < /tmp/cbc_rofi.txt", "r");
        if (!rofi) {
            xError("[UI] Failed to execute Rofi.");
            return;
        }

        char result[256];
        
        /// 4. fgets will block and wait until the user selects an item or presses ESC
        if (fgets(result, sizeof(result), rofi) != NULL) {
            
            /// 5. Parse the selected index from the returned string
            int selected_index = -1;
            if (sscanf(result, "%d:", &selected_index) == 1) {
                xLog1("[UI] User selected index: %d", selected_index);
                
                /// 6. Set the logical index and trigger the X11 injection
                if (XCBList_SetSelectedNum(selected_index) == OKE) {
                    ReqTestInject = eACTIVATE;
                }
            }
        } else {
            xLog1("[UI] User cancelled Rofi (pressed ESC).");
        }

        pclose(rofi);
        xExit1("ShowRofiMenu");
    }

#endif /*ROFI_SUPPORT*/

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


