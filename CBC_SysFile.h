#ifndef __CBC_SYSFILE_H__
#define __CBC_SYSFILE_H__

#include "CBC_Setup.h"

/**************************************************************************************************
 * CLIPBOARD ITEM DEFINITION SECTION **************************************************************
 **************************************************************************************************/ 

/**
 * @brief Supported clipboard file formats.
 */
enum XCBFileType {
    eFMT_NONE = 0, 
    eFMT_TXT, 
    eFMT_IMG_JGP, 
    eFMT_IMG_PNG
};

/**
 * @brief Union to hold clipboard item metadata with raw access capability.
 */
typedef union {
    uint8_t RawData[NAME_MAX + 4 + sizeof(time_t) + sizeof(enum XCBFileType)];
    struct {
        char                Filename[NAME_MAX + 4]; 
        time_t              Timestamp;
        enum XCBFileType    FileType;
    };
} sClipboardItem;

/**************************************************************************************************
 * SYSTEM / UTILS PROTOTYPES **********************************************************************
 **************************************************************************************************/ 

/**
 * @brief Generates a unique .txt filename based on the current time.
 * @param buffer Output buffer for the filename.
 * @param size Size of the buffer.
 */
void GetTimeBasedFilenameTxt(char *buffer, size_t size);

/**
 * @brief Generates a time-based filename with an optional extension.
 * @param buffer Output buffer for the filename.
 * @param size Size of the buffer.
 * @param ext Extension string (e.g., "png", "txt"). If NULL, no extension is added.
 */
void GetTimeBasedFilename(char *buffer, size_t size, const char *ext);

/**
 * @brief Extracts the file name from a full path (e.g., "A/B/C.txt" -> "C.txt").
 * @param Path The full path string.
 * @param OutputFileName Buffer to store the extracted file name.
 * @param MaxFileNameSize Maximum size of the output buffer.
 * @return OKE on success, ERR if the path ends with '/'.
 */
RetType GetFileNameFromPath(char *Path, char *OutputFileName, int MaxFileNameSize);

/**************************************************************************************************
 * PUBLIC LIST PROTOTYPES *************************************************************************
 **************************************************************************************************/ 

/**
 * @brief Scans DB directory. Keeps only MAX_HISTORY_ITEMS, deletes the rest.
 * @param WithNoLock Set to 1 to bypass mutex locking, 0 for thread-safe scan.
 * @return The number of items loaded into the RAM list.
 */
int XCBList_Scan(int WithNoLock);

/**
 * @brief Pushes a name/path to the list without checking the disk. Extracts filename only.
 * @param Path The file path to push.
 * @return OKE on success, ERR on invalid path.
 */
RetType XCBList_PushItem(char Path[]);

/**
 * @brief Pushes a name/path to the list only if it physically exists in PATH_DIR_DB.
 * @param Path The file path to push.
 * @return OKE on success, ERR if the file does not exist.
 */
RetType XCBList_PushItemWithExistCheck(char Path[]);

/**
 * @brief Removes the oldest item from RAM and deletes its physical file.
 * @param Output Pointer to store the popped item. Pass NULL to discard data.
 * @return OKE on success, ERR if the list is empty.
 */
RetType XCBList_PopItem(sClipboardItem *Output);

/**
 * @brief Gets the item at logical index 'n' (0 = newest). 
 * @param n The logical index.
 * @param Output Pointer to store the data. Pass NULL to verify existence only.
 * @return OKE on success, ERR if index is out of bounds.
 */
RetType XCBList_GetItem(int n, sClipboardItem *Output);

/**
 * @brief Gets the most recent item (index 0). 
 * @param Output Pointer to store the data. Pass NULL to verify existence only.
 * @return OKE on success, ERR if the list is empty.
 */
RetType XCBList_GetLatestItem(sClipboardItem *Output);

/**
 * @brief Reads the binary content of the file at logical index 'n'.
 * @param n The logical index of the item.
 * @param Output Buffer to store the binary data. Pass NULL to verify disk presence.
 * @param MaxOutputSize The maximum capacity of the output buffer.
 * @return OKE on success, ERR or ERR_OVERFLOW on failure.
 */
RetType XCBList_ReadAsBinary(int n, void* Output, int MaxOutputSize);

/**
 * @brief Returns the total number of items currently in the RAM list.
 * @return The number of items.
 */
int XCBList_GetItemSize(void);

/**
 * @brief Sets the currently selected logical index.
 * @param LinearIndex The UI index to select (0 to XCBListSize - 1).
 * @return OKE on success, ERR if the index is out of bounds.
 */
int XCBList_SetSelectedNum(int LinearIndex);

/**
 * @brief Retrieves the logical UI index of the currently selected clipboard item.
 * @return The currently selected index, or -1 if no item is selected.
 */
int XCBList_GetSelectedNum(void);

/**
 * @brief Gets the data of the currently selected clipboard item.
 * @param Output Pointer to store the data. Pass NULL to verify existence only.
 * @return OKE on success, ERR if no valid item is selected.
 */
int XCBList_GetSelectedItem(sClipboardItem *Output);

/**************************************************************************************************
 * SYSTEMCALL HELPER PROTOTYPES *******************************************************************
 **************************************************************************************************/ 

/**
 * @brief Handles and logs errors that occur during directory creation.
 * @param FunctionName Name of the calling function for tracing.
 * @param Path The directory path that failed to be created.
 * @param ErrorCode The errno value captured after mkdir() failure.
 */
void PrintError_Mkdir(const char FunctionName[], const char Path[], int ErrorCode);

/**
 * @brief Checks for directory existence and creates it if missing.
 * @param path The path of the directory to ensure.
 * @return OKE on success or if it exists, ERR on failure.
 */
RetType EnsureDir(char path[]);

/**
 * @brief Recursively removes a directory and all of its contents.
 * @param path The directory path to be removed.
 * @return OKE on success, ERR on failure.
 */
RetType RemoveDir(const char path[]);

/**
 * @brief Initializes the core database directory structure.
 * @return OKE if all directories are ready, ERR otherwise.
 */
RetType EnsureDB(void);

/**
 * @brief Simple helper to save raw text data to a demonstration file.
 * @param data The text data buffer.
 * @param len Length of the data in bytes.
 */
void SaveClipboardToFile(const char* data, int len);

#endif /*__CBC_SYSFILE_H__*/

/**************************************************************************************************
 * EOF ********************************************************************************************
 **************************************************************************************************/
