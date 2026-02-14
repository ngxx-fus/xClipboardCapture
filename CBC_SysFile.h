
#ifndef __CBC_SYSFILE_H__
#define __CBC_SYSFILE_H__

#include "CBC_Setup.h"

/**************************************************************************************************
 * CLIPBOARD ITEM DEFINITION SECTION *************************************************************
 **************************************************************************************************/ 

enum XCBFileType {eFMT_NONE, eFMT_TXT, eFMT_IMG_JGP, eFMT_IMG_PNG};

/// @brief Union to hold clipboard item metadata with raw access capability.
typedef union {
    uint8_t RawData[NAME_MAX + 1 + sizeof(time_t) + sizeof(enum XCBFileType)];
    struct {
        char                Filename[NAME_MAX + 1]; 
        time_t              Timestamp;
        enum XCBFileType    FileType;
    };
} sClipboardItem;

/**************************************************************************************************
 * PUBLIC FUNCTION PROTOTYPES *********************************************************************
 **************************************************************************************************/ 

/// @brief Extracts "C.txt" from "A/B/C.txt". Returns ERR if ends with '/'.
RetType GetFileNameFromPath(char *Path, char *OutputFileName, int MaxFileNameSize);

/// @brief Scans DB directory. Keeps only MAX_HISTORY_ITEMS, deletes the rest.
int XCBList_Scan(int WithNoLock);

/// @brief Pushes a name/path to list (No disk check). Extracts filename only.
RetType XCBList_PushItem(char Path[]);

/// @brief Pushes a name/path only if it actually exists in PATH_DIR_DB.
RetType XCBList_PushItemWithExistCheck(char Path[]);

/// @brief Removes oldest item from RAM and deletes its physical file.
RetType XCBList_PopItem(sClipboardItem *Output);

/// @brief Gets item at index 'n' (0 = newest). Pass NULL to Output to only verify existence.
RetType XCBList_GetItem(int n, sClipboardItem *Output);

/// @brief Gets the most recent item. Pass NULL to Output to only verify existence.
RetType XCBList_GetLatestItem(sClipboardItem *Output);

/// @brief Reads file binary content. Pass NULL to Output to verify disk presence.
RetType XCBList_ReadAsBinary(int n, void* Output, int MaxOutputSize);

/// @brief Returns the total items currently in RAM list.
int XCBList_GetItemSize(void);

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
