
#ifndef __CBC_SYSFILE_H__
#define __CBC_SYSFILE_H__

#include "CBC_Setup.h"

/**************************************************************************************************
 * CLIPBOARD ITEM DEFINITION SECTION *************************************************************
 **************************************************************************************************/ 

/// @brief Union to hold clipboard item metadata with raw access capability.
typedef union {
    uint8_t RawData[NAME_MAX + 1 + sizeof(time_t)];
    struct {
        char Filename[NAME_MAX + 1]; 
        time_t Timestamp;            
    };
} sClipboardItem;

/**************************************************************************************************
 * PUBLIC FUNCTION PROTOTYPES *********************************************************************
 **************************************************************************************************/ 

/// @brief Scans the DB directory to count and populate the list. 
int XCBList_Scan(void);

/// @brief Synchronizes the list from disk and sorts it (Newest first).
RetType XCBList_ScanAndSort(void);

/// @brief Adds a new file to the list. Triggers rescan if file is missing.
RetType XCBList_PushItem(char FileName[]);

/// @brief Removes the newest item from list and disk.
RetType XCBList_PopItem(sClipboardItem *Output);

/// @brief Gets the latest item without removing it.
RetType XCBList_GetLatestItem(sClipboardItem *Output);

/// @brief Removes item at index 'n' from list and disk.
RetType XCBList_RemoveItem(int n, sClipboardItem *Output);

/// @brief Gets item metadata at index 'n'.
RetType XCBList_GetItem(int n, sClipboardItem *Output);

/// @brief Returns current size of the list.
int XCBList_GetItemSize(void);

/// @brief Reads file content into a binary buffer.
RetType XCBList_ReadAsBinary(int n, void* Output, int MaxOutputSize);

/**************************************************************************************************
 * SYSTEMCALL HELPER SECTION PROTOTYPES ***********************************************************
 **************************************************************************************************/ 

/** * @brief Handle and log errors occurred during directory creation.
 * @param FunctionName Name of the caller function for tracing.
 * @param Path The directory path that failed to be created.
 * @param ErrorCode The errno value captured after mkdir() failure.
 */
void PrintError_Mkdir(const char FunctionName[], const char Path[], int ErrorCode);

/** * @brief Check for directory existence and create it if missing.
 * @param path The string path of the directory to ensure.
 * @return OKE on success or if exists, otherwise returns error code from mkdir.
 */
RetType EnsureDir(char path[]);

/** * @brief Recursively removes a directory and all its contents.
 * @param path The directory path to be removed.
 * @return OKE on success, ERR on failure.
 */
RetType RemoveDir(const char path[]);

/** * @brief Initialize the database directory structure.
 * @return OKE if all directories are ready, error code otherwise.
 */
RetType EnsureDB(void);

/**
 * @brief Simple helper to save text data to file (Demostration).
 * @param data The data buffer.
 * @param len Length of data.
 */
void SaveClipboardToFile(const char* data, int len);

#endif /*__CBC_SYSFILE_H__*/
