#include "CBC_SysFile.h"
#include "CBC_Setup.h"
#include <xUniversal.h>
#include <xUniversalReturn.h>

/**************************************************************************************************
 * INTERNAL DATA SECTION **************************************************************************
 **************************************************************************************************/ 

/**
 * @brief The physical array acting as a ring buffer for clipboard items.
 */
static sClipboardItem   XCBList[MAX_HISTORY_ITEMS];

/**
 * @brief The current number of items stored in the buffer.
 */
static int              XCBListSize = 0;

/**
 * @brief The physical array index pointing to the newest (latest) item.
 */
static int              HeadIndex = -1;

/**
 * @brief Mutex to ensure thread-safe access to the clipboard list.
 */
static pthread_mutex_t  ListMutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief The currently selected logical index (used by UI injection).
 */
static int              XCBList_SelectedItem = -1;

/**************************************************************************************************
 * LOCKING HELPERS ********************************************************************************
 **************************************************************************************************/ 

/**
 * @brief Locks the list mutex for thread-safe operations.
 */
static void LockList(void) { 
    pthread_mutex_lock(&ListMutex); 
}

/**
 * @brief Unlocks the list mutex.
 */
static void UnlockList(void) { 
    pthread_mutex_unlock(&ListMutex); 
}

/**************************************************************************************************
 * INTERNAL HELPERS: INDEX CONVERSION & FILE TYPE *************************************************
 **************************************************************************************************/ 

/**
 * @brief Converts a logical UI index (0 = Newest) to the physical array index.
 * @param LinearIndex The logical index from the UI perspective.
 * @return The physical array index, or -1 if the input index is out of bounds.
 */
static int Convert2AllocatedIndex(int LinearIndex) {
    /// Prevent out-of-bounds access if the UI requests a non-existent item
    if (LinearIndex < 0 || LinearIndex >= XCBListSize) return -1;
    
    /// Ring Buffer Math: 
    /// We subtract the logical index (0 is newest) from the HeadIndex (newest physical location).
    /// Adding MAX_HISTORY_ITEMS ensures the value is strictly positive before applying the modulo.
    return (HeadIndex - LinearIndex + MAX_HISTORY_ITEMS) % MAX_HISTORY_ITEMS;
}

/**
 * @brief Converts a physical array index to the logical UI index.
 * @param AllocatedIndex The physical index inside the ring buffer.
 * @return The logical UI index (0 = Newest), or -1 if the list is empty.
 */
static int Convert2LinearIndex(int AllocatedIndex) {
    /// If the list is empty, there is no valid linear index
    if (XCBListSize == 0) return -1;
    
    /// Reverse Ring Buffer Math:
    /// Calculate how far the given allocated index is from the current HeadIndex.
    return (HeadIndex - AllocatedIndex + MAX_HISTORY_ITEMS) % MAX_HISTORY_ITEMS;
}

/**
 * @brief Parses a file extension to determine its FileType enum.
 * @param Filename The name of the file to parse.
 * @return The corresponding XCBFileType enumeration.
 */
static enum XCBFileType GetFileTypeFromName(const char* Filename) {
    /// Find the last occurrence of the dot ('.') in the filename string
    char *ext = strrchr(Filename, '.');
    
    /// If no extension is found, return the default NONE format
    if (!ext) return eFMT_NONE;
    
    /// Use case-insensitive string comparison (strcasecmp) to match extensions
    if (strcasecmp(ext, ".txt") == 0) return eFMT_TXT;
    if (strcasecmp(ext, ".png") == 0) return eFMT_IMG_PNG;
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) return eFMT_IMG_JGP;
    
    /// Fallback for unknown extensions
    return eFMT_NONE;
}

/**
 * @brief Sort comparator (Ascending): Oldest first. 
 * @param a Pointer to the first sClipboardItem.
 * @param b Pointer to the second sClipboardItem.
 * @return 1 if a > b, -1 if a < b, 0 if equal.
 * @note Used ONLY during initial Scan to setup the buffer chronologically.
 */
static int CompareItemsAsc(const void *a, const void *b) {
    sClipboardItem *itemA = (sClipboardItem *)a;
    sClipboardItem *itemB = (sClipboardItem *)b;
    
    /// Compare Unix timestamps. Smaller timestamp means older file.
    if (itemA->Timestamp > itemB->Timestamp) return 1;
    if (itemA->Timestamp < itemB->Timestamp) return -1;
    return 0;
}

/**
 * @brief Internal PopOldest for Circle Buffer. Physically removes the oldest item from disk.
 * @param Output Optional pointer to store the popped item data.
 * @return OKE on success, ERR if the list is empty or file deletion fails.
 * @note This function assumes the caller has already locked the ListMutex.
 */
static RetType Internal_PopOldest(sClipboardItem *Output) {
    /// Cannot pop from an empty buffer
    if (XCBListSize <= 0) return ERR;
    
    /// Find the physical array index of the oldest item.
    /// Since index 0 is the newest, (XCBListSize - 1) is always the oldest logical index.
    int OldestAllocIdx = Convert2AllocatedIndex(XCBListSize - 1);
    if (OldestAllocIdx < 0) return ERR;
    
    /// Safely construct the absolute file path targeting the DB directory
    char FullPath[PATH_MAX];
    snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, XCBList[OldestAllocIdx].Filename);

    /// If the caller wants to read the deleted item's metadata, copy it out
    if (Output != NULL) {
        memcpy(Output, &XCBList[OldestAllocIdx], sizeof(sClipboardItem));
    }

    /// Physically delete the file from the hard drive to free up space
    RemoveDir(FullPath);

    /// Shrink the logical tracking size
    XCBListSize--;
    return OKE;
}

/**************************************************************************************************
 * SYSTEM / UTILS IMPLEMENTATION ******************************************************************
 **************************************************************************************************/ 

/**
 * @brief Extracts the raw filename from a full directory path.
 * @param Path The full path string.
 * @param OutputFileName Buffer to store the extracted filename.
 * @param MaxFileNameSize The maximum capacity of the output buffer.
 * @return OKE on success, ERR on invalid input.
 */
RetType GetFileNameFromPath(char *Path, char *OutputFileName, int MaxFileNameSize) {
    /// Guard against null pointers
    if (!Path || !OutputFileName) return ERR;
    
    /// Find the last slash character to isolate the filename
    char *LastSlash = strrchr(Path, '/');
    
    /// If a slash is found, jump 1 character forward. Otherwise, the whole string is the name.
    char *CleanName = (LastSlash) ? (LastSlash + 1) : Path;
    
    /// Prevent processing empty names (e.g., path ended with a slash "/")
    if (*CleanName == '\0') return ERR;

    /// Copy safely, ensuring null-termination
    strncpy(OutputFileName, CleanName, MaxFileNameSize - 1);
    OutputFileName[MaxFileNameSize - 1] = '\0';
    return OKE;
}

/**
 * @brief Helper to generate a unique text filename based on current time.
 * @param buffer Output buffer for the filename.
 * @param size Size of the buffer.
 */
void GetTimeBasedFilenameTxt(char *buffer, size_t size) {
    time_t rawtime;
    struct tm *timeinfo;
    
    /// Fetch current system time
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    /// Format the time into a standardized YYYYMMDD_HHMMSS.txt string
    strftime(buffer, size, "%Y%m%d_%H%M%S.txt", timeinfo);
}

/**
 * @brief Helper to generate a filename based on current time with an optional extension.
 * @param buffer Output buffer for the filename.
 * @param size Size of the buffer.
 * @param ext Extension string (e.g., "png", "txt"). If NULL, no extension is added.
 */
void GetTimeBasedFilename(char *buffer, size_t size, const char *ext) {
    time_t rawtime;
    struct tm *timeinfo;
    char TimeStr[64];

    /// Fetch current system time
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    /// 1. Generate the base timestamp string: YYYYMMDD_HHMMSS
    strftime(TimeStr, sizeof(TimeStr), "%Y%m%d_%H%M%S", timeinfo);

    /// 2. Concatenate the extension if provided
    if (ext == NULL || ext[0] == '\0') {
        /// If no extension is specified, just copy the raw timestamp
        snprintf(buffer, size, "%s", TimeStr);
    } else {
        /// Format with a dot separator
        snprintf(buffer, size, "%s.%s", TimeStr, ext);
    }
}

/**************************************************************************************************
 * PUBLIC LIST IMPLEMENTATION *********************************************************************
 **************************************************************************************************/ 

/**
 * @brief Scans the database directory and rebuilds the ring buffer.
 * @param WithNoLock If 1, bypasses mutex locking (useful during initialization).
 * @return The number of items scanned, or ERR on failure.
 */
int XCBList_Scan(int WithNoLock) {
    if (!WithNoLock) LockList();

    /// Open the database directory to read existing clipboard files
    DIR *DirStream = opendir(PATH_DIR_DB);
    struct dirent *Entry;
    struct stat FileStat;
    char FullPath[PATH_MAX];
    
    /// Completely reset the ring buffer state before scanning
    XCBListSize = 0;
    HeadIndex = -1; 

    if (DirStream == NULL) {
        if (!WithNoLock) UnlockList();
        return ERR;
    }

    /// Loop through all items inside the DB directory
    while ((Entry = readdir(DirStream)) != NULL) {
        /// Ignore hidden files or current/parent directory links (".", "..")
        if (Entry->d_name[0] == '.') continue;
        
        /// Construct the absolute path to access the file's metadata
        snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, Entry->d_name);
        
        /// If we haven't reached the memory limit, load the file into the buffer
        if (XCBListSize < MAX_HISTORY_ITEMS) {
            if (stat(FullPath, &FileStat) == 0) {
                /// [LEGACY] strncpy(XCBList[XCBListSize].Filename, Entry->d_name, NAME_MAX);
                /// [LEGACY] XCBList[XCBListSize].Filename[NAME_MAX] = '\0';
                snprintf(XCBList[XCBListSize].Filename, NAME_MAX+1, "%s", Entry->d_name);
                
                /// Save the modification time so we can sort chronologically later
                XCBList[XCBListSize].Timestamp = FileStat.st_mtime;
                XCBList[XCBListSize].FileType = GetFileTypeFromName(Entry->d_name);
                XCBListSize++;
            }
        } else {
            /// If the DB has more files than allowed, purge the excess files from disk
            RemoveDir(FullPath); 
        }
    }
    closedir(DirStream);

    /// If we found valid files, we must re-establish the chronological Ring Buffer order
    if (XCBListSize > 0) {
        /// Sort the loaded items from oldest to newest based on their timestamp
        qsort(XCBList, XCBListSize, sizeof(sClipboardItem), CompareItemsAsc);
        
        /// Set HeadIndex to point to the last element (the newest item in the sorted array)
        HeadIndex = XCBListSize - 1; 
    }

    if (!WithNoLock) UnlockList();
    return XCBListSize;
}

/**
 * @brief Pushes a new item into the ring buffer. Evicts the oldest if full.
 * @param Path The path or filename to be added.
 * @return OKE on success, ERR on invalid path.
 */
RetType XCBList_PushItem(char Path[]) {
    char CleanName[256];

    xEntry1("XCBList_PushItem(%s)", Path);

    /// Extract just the filename to avoid saving absolute paths in the DB
    if (GetFileNameFromPath(Path, CleanName, sizeof(CleanName)) != OKE) return ERR;

    LockList();
    
    /// If the buffer has reached maximum capacity, pop the oldest item to make space
    if (XCBListSize >= MAX_HISTORY_ITEMS) {
        Internal_PopOldest(NULL); 
    }

    /// Advance the HeadIndex circularly. If it reaches the end, it wraps back to 0.
    HeadIndex = (HeadIndex + 1) % MAX_HISTORY_ITEMS;

    /// Store the new item's metadata at the newly allocated HeadIndex
    /// [LEGACY]strncpy(XCBList[HeadIndex].Filename, CleanName, NAME_MAX);
    snprintf(XCBList[HeadIndex].Filename, NAME_MAX + 1, "%s", CleanName);
    XCBList[HeadIndex].Filename[NAME_MAX] = '\0';
    XCBList[HeadIndex].Timestamp = time(NULL);
    XCBList[HeadIndex].FileType = GetFileTypeFromName(CleanName);

    XCBListSize++;
    UnlockList();
    return OKE;
}

/**
 * @brief Checks if a file exists on disk before pushing it to the list.
 * @param Path The filename to be verified and added.
 * @return OKE if the file exists and was pushed, ERR otherwise.
 */
RetType XCBList_PushItemWithExistCheck(char Path[]) {
    char CleanName[256];
    if (GetFileNameFromPath(Path, CleanName, sizeof(CleanName)) != OKE) return ERR;

    struct stat FileStat;
    char FullPath[PATH_MAX];
    snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, CleanName);

    /// Verify physical file presence on the disk
    if (stat(FullPath, &FileStat) != 0) return ERR;

    /// Delegate the insertion logic to the standard Push function
    XCBList_PushItem(CleanName);

    /// Override the generated timestamp with the actual file modification time
    LockList();
    XCBList[HeadIndex].Timestamp = FileStat.st_mtime;
    UnlockList();
    return OKE;
}

/**
 * @brief Pops the oldest item from the list and deletes its file.
 * @param Output Optional pointer to receive the popped item's metadata.
 * @return OKE on success, ERR if the list is empty.
 */
RetType XCBList_PopItem(sClipboardItem *Output) {
    LockList();
    /// Delegate the heavy lifting to the internal thread-unsafe function
    RetType Ret = Internal_PopOldest(Output);
    UnlockList();
    return Ret;
}

/**
 * @brief Retrieves metadata of an item at a specific logical index.
 * @param n The logical index (0 = Newest).
 * @param Output Pointer to store the retrieved metadata.
 * @return OKE on success, ERR if the index is invalid.
 */
RetType XCBList_GetItem(int n, sClipboardItem *Output) {
    LockList();
    
    /// Map the UI's logical view to the ring buffer's physical memory index
    int AllocIdx = Convert2AllocatedIndex(n);
    if (AllocIdx < 0) {
        UnlockList();
        return ERR;
    }
    
    /// Copy the metadata out to the caller's buffer
    if (Output != NULL) {
        memcpy(Output, &XCBList[AllocIdx], sizeof(sClipboardItem));
    }
    UnlockList();
    return OKE;
}

/**
 * @brief Retrieves metadata of the newest item (Index 0).
 * @param Output Pointer to store the retrieved metadata.
 * @return OKE on success, ERR if the list is empty.
 */
RetType XCBList_GetLatestItem(sClipboardItem *Output) {
    /// Pass logical index 0 to target the most recently pushed item
    return XCBList_GetItem(0, Output);
}

/**
 * @brief Gets the current number of items in the history.
 * @return The size of the list.
 */
int XCBList_GetItemSize(void) {
    int Size;
    LockList();
    Size = XCBListSize;
    UnlockList();
    return Size;
}

/**
 * @brief Reads the binary content of a file corresponding to a logical index.
 * @param n The logical index of the item.
 * @param Output Buffer to hold the read binary data.
 * @param MaxOutputSize Maximum allowed size to prevent buffer overflow.
 * @return OKE on success, ERR_OVERFLOW if index is invalid, ERR on file errors.
 */
RetType XCBList_ReadAsBinary(int n, void* Output, int MaxOutputSize) {
    xEntry1("XCBList_ReadAsBinary(%d, %p, %d)", n, Output, MaxOutputSize);
    LockList();
    
    /// Find exactly where this item's metadata sits in the array
    int AllocIdx = Convert2AllocatedIndex(n);
    if (AllocIdx < 0) { 
        UnlockList(); 
        return ERR_OVERFLOW; 
    }
    
    /// Reconstruct the absolute path to the physical file
    char FullPath[PATH_MAX];
    snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, XCBList[AllocIdx].Filename);
    UnlockList();

    xLog1("[XCBList_ReadAsBinary] FullPath=%s", FullPath);

    /// Open file in Read Binary mode
    FILE *f = fopen(FullPath, "rb");
    if (!f) return ERR;
    
    /// If the caller passed NULL, they just wanted to verify if the file is readable
    if (Output == NULL) { 
        fclose(f); 
        return OKE; 
    }

    /// Jump to the end of the file to determine its byte size
    fseek(f, 0, SEEK_END);
    long FileSize = ftell(f);
    /// Rewind the cursor back to the start for reading
    rewind(f);

    /// Prevent memory corruption by rejecting oversized files
    if (FileSize > (long)MaxOutputSize) { 
        fclose(f); 
        return ERR; 
    }
    
    /// Read the entire file content into the user-provided buffer in one go
    size_t ReadSize = fread(Output, 1, FileSize, f);
    fclose(f);

    xLog1("[XCBList_ReadAsBinary] ReadSize=%ld", ReadSize);
    
    /// Ensure we read exactly the number of bytes we expected
    return (ReadSize == (size_t)FileSize) ? OKE : ERR;
}

/**
 * @brief Sets the logical index of the currently selected item.
 * @param LinearIndex The logical index to select.
 * @return OKE on success, ERR if the index is out of bounds.
 */
int XCBList_SetSelectedNum(int LinearIndex) {
    LockList();
    
    /// Validate bounds to ensure UI cannot select a non-existent item
    if (LinearIndex < 0 || LinearIndex >= XCBListSize) {
        xError("[XCBList_SetSelectedNum] Invalid index = %d", LinearIndex);
        UnlockList();
        return ERR;
    }
    
    XCBList_SelectedItem = LinearIndex;
    UnlockList();
    
    return OKE;
}

/**
 * @brief Gets the logical index of the currently selected item.
 * @return The selected index, or 0 if out of bounds.
 */
int XCBList_GetSelectedNum(void) {
    int SelectedNum;
    
    LockList();
    SelectedNum = XCBList_SelectedItem;
    UnlockList();
    
    /// Fallback mechanism: Default to the newest item (0) if the selection is corrupted
    if(SelectedNum < 0 || SelectedNum >= XCBListSize) {
        SelectedNum = 0;
    }

    return SelectedNum;
}

/**
 * @brief Retrieves the metadata of the currently selected item.
 * @param Output Pointer to store the retrieved metadata.
 * @return OKE on success, ERR if no valid selection exists.
 */
int XCBList_GetSelectedItem(sClipboardItem *Output) {
    LockList();
    
    /// 1. Check if a valid item is currently selected
    if (XCBList_SelectedItem < 0 || XCBList_SelectedItem >= XCBListSize) {
        UnlockList();
        return ERR;
    }
    
    /// 2. Convert the logical UI index to the physical ring buffer index
    int AllocIdx = Convert2AllocatedIndex(XCBList_SelectedItem);
    if (AllocIdx < 0) {
        UnlockList();
        return ERR;
    }
    
    /// 3. Copy metadata if Output pointer is provided
    if (Output != NULL) {
        memcpy(Output, &XCBList[AllocIdx], sizeof(sClipboardItem));
    }
    
    UnlockList();
    return OKE;
}

/**************************************************************************************************
 * SYSTEMCALL HELPER SECTION **********************************************************************
 **************************************************************************************************/ 

/**
 * @brief Handle and log errors occurred during directory creation.
 * @param FunctionName Name of the caller function for tracing.
 * @param Path The directory path that failed to be created.
 * @param ErrorCode The errno value captured after mkdir() failure.
 */
void PrintError_Mkdir(const char FunctionName[], const char Path[], int ErrorCode) {
    /// Interpret the POSIX error code to provide meaningful logs
    switch (ErrorCode) {
        case EEXIST:
            xWarn2("[%s] Path already exists: %s", FunctionName, Path);
            break;
        case EACCES:
        case EPERM:
            xError("[%s] Permission denied to create: %s", FunctionName, Path);
            break;
        case ENOSPC:
            xError("[%s] No space left on device to create: %s", FunctionName, Path);
            break;
        case EROFS:
            xError("[%s] Read-only file system: %s", FunctionName, Path);
            break;
        default:
            xError("[%s] Failed to create %s: %s (errno: %d)", 
                   FunctionName, Path, strerror(ErrorCode), ErrorCode);
            break;
    }
}

/**
 * @brief Check for directory existence and create it if missing.
 * @param path The string path of the directory to ensure.
 * @return OKE on success or if it exists, ERR on failure.
 */
RetType EnsureDir(char path[]){
    xEntry2("EnsureDir");
    RetType RetVal;
    struct stat Stat;

    /// Attempt to read file/folder metadata to check existence
    RetVal = stat(path, &Stat);
    if( RetVal != 0){
        /// If stat fails, the folder likely doesn't exist. Attempt to create it with 0755 permissions.
        RetVal = mkdir(path, 0755);
        if (RetVal == 0) {
            xLog2("[EnsureDir] %s created successfully!", path);
        } else {
            PrintError_Mkdir("EnsureDir", path, errno);
        }
    }
    xExit2("EnsureDir");
    return (RetVal == 0) ? OKE : ERR; 
}

/**
 * @brief Recursively removes a directory and all of its contents.
 * @param path The directory path to be removed.
 * @return OKE on success, ERR on failure.
 */
RetType RemoveDir(const char path[]) {
    xEntry2("RemoveDir");
    
    DIR *dir_stream = NULL;
    struct dirent *entry;
    char sub_path[PATH_MAX]; 
    RetType status = OKE;

    /// Attempt to remove the directory directly. This works instantly if it's already empty.
    if (rmdir(path) == 0) {
        xLog2("[RemoveDir] Empty directory removed: %s", path);
        xExit2("RemoveDir: OKE");
        return OKE;
    }

    /// If rmdir fails, it means the directory contains files. We must open and traverse it.
    dir_stream = opendir(path);
    if (dir_stream == NULL) {
        xError("[RemoveDir] Failed to open %s: %s", path, strerror(errno));
        xExit2("RemoveDir: ERR");
        return ERR;
    }

    /// Loop through every single entry inside the directory.
    while ((entry = readdir(dir_stream)) != NULL) {
        /// Skip the special directories "." (current) and ".." (parent) to avoid infinite loops
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /// Construct the absolute path for the child item
        snprintf(sub_path, sizeof(sub_path), "%s/%s", path, entry->d_name);

        /// If the child item is another directory, recursively call RemoveDir to dive deeper
        if (entry->d_type == DT_DIR) {
            RemoveDir(sub_path);
        } else {
            /// If it is a file or symlink, delete it permanently using unlink()
            if (unlink(sub_path) == 0) {
                xLog2("[RemoveDir] File deleted: %s", sub_path);
            } else {
                xError("[RemoveDir] Failed to delete file: %s", sub_path);
            }
        }
    }

    /// Close the directory stream to prevent file descriptor leaks
    closedir(dir_stream);

    /// Final step: remove the current directory which should now be completely empty
    if (rmdir(path) == 0) {
        xLog2("[RemoveDir] Directory cleaned and removed: %s", path);
    } else {
        xError("[RemoveDir] Final rmdir failed for %s: %s", path, strerror(errno));
        status = ERR;
    }

    xExit2("RemoveDir");
    return status;
}

/**
 * @brief Initialize the database directory structure.
 * @return OKE if all directories are ready, ERR otherwise.
 */
RetType EnsureDB(void){
    xEntry1("EnsureDB");
    RetType RetVal;
    struct stat Status;

    xLog1("[EnsureDB] PATH_DIR_ROOT=%s", PATH_DIR_ROOT);
    xLog1("[EnsureDB] PATH_DIR_DB=%s", PATH_DIR_DB);
    
    /// Ensure the main hidden cache directory exists
    RetVal = EnsureDir(PATH_DIR_ROOT);
    if(RetVal != OKE) return RetVal;
    
    /// Ensure the sub-directory specifically holding the raw data files exists
    RetVal = EnsureDir(PATH_DIR_DB);
    if(RetVal != OKE) return RetVal;

    xExit1("EnsureDB");
    return OKE;
}

/**
 * @brief Simple helper to save text data to a specific file.
 * @param data The data buffer to be written.
 * @param len Length of the data.
 */
void SaveClipboardToFile(const char* data, int len) {
    xEntry1("SaveClipboardToFile");
    
    /// Open the file in write mode, overriding any existing content
    FILE *fp = fopen(PATH_ITEM, "w"); 
    if (fp) {
        fwrite(data, 1, len, fp);
        fclose(fp);
        xLog1("Saved %d bytes to %s", len, PATH_ITEM);
    } else {
        xError("Failed to open file for writing: %s", PATH_ITEM);
    }
    xExit1("SaveClipboardToFile");
}

/**************************************************************************************************
 * EOF ********************************************************************************************
 **************************************************************************************************/
