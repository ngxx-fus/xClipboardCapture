#include "CBC_SysFile.h"
#include "CBC_Setup.h"
#include <xUniversal.h>
#include <xUniversalReturn.h>

/**************************************************************************************************
 * INTERNAL DATA SECTION **************************************************************************
 **************************************************************************************************/ 

static sClipboardItem   XCBList[MAX_HISTORY_ITEMS];
static int              XCBListSize = 0;
static int              HeadIndex   = -1;
static pthread_mutex_t  ListMutex = PTHREAD_MUTEX_INITIALIZER;
static int              XCBList_SelectedItem = -1;


/**************************************************************************************************
 * LOCKING HELPERS *******************************************************************************
 **************************************************************************************************/ 

static void LockList(void)   { pthread_mutex_lock(&ListMutex);   }
static void UnlockList(void) { pthread_mutex_unlock(&ListMutex); }

/**************************************************************************************************
 * INTERNAL HELPERS: INDEX CONVERSION & FILE TYPE *************************************************
 **************************************************************************************************/ 

/// @brief Converts a logical UI index (0 = Newest) to the physical array index.
static int Convert2AllocatedIndex(int LinearIndex) {
    if (LinearIndex < 0 || LinearIndex >= XCBListSize) return -1;
    return (HeadIndex - LinearIndex + MAX_HISTORY_ITEMS) % MAX_HISTORY_ITEMS;
}

/// @brief Converts a physical array index to the logical UI index.
static int Convert2LinearIndex(int AllocatedIndex) {
    if (XCBListSize == 0) return -1;
    return (HeadIndex - AllocatedIndex + MAX_HISTORY_ITEMS) % MAX_HISTORY_ITEMS;
}

/// @brief Parses file extension to determine FileType enum.
static enum XCBFileType GetFileTypeFromName(const char* Filename) {
    char *ext = strrchr(Filename, '.');
    if (!ext) return eFMT_NONE;
    if (strcasecmp(ext, ".txt") == 0) return eFMT_TXT;
    if (strcasecmp(ext, ".png") == 0) return eFMT_IMG_PNG;
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) return eFMT_IMG_JGP;
    return eFMT_NONE;
}

/// @brief Sort comparator (Ascending): Oldest first. Used ONLY during initial Scan to setup buffer.
static int CompareItemsAsc(const void *a, const void *b) {
    sClipboardItem *itemA = (sClipboardItem *)a;
    sClipboardItem *itemB = (sClipboardItem *)b;
    if (itemA->Timestamp > itemB->Timestamp) return 1;
    if (itemA->Timestamp < itemB->Timestamp) return -1;
    return 0;
}

/// @brief Internal PopOldest for Circle Buffer. Physically removes from disk.
static RetType Internal_PopOldest(sClipboardItem *Output) {
    if (XCBListSize <= 0) return ERR;
    
    /// The oldest item is always at the last linear index
    int OldestAllocIdx = Convert2AllocatedIndex(XCBListSize - 1);
    if (OldestAllocIdx < 0) return ERR;
    
    char FullPath[PATH_MAX];
    snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, XCBList[OldestAllocIdx].Filename);

    if (Output != NULL) {
        memcpy(Output, &XCBList[OldestAllocIdx], sizeof(sClipboardItem));
    }

    /// Delete using RemoveDir for safety (handles files and folders)
    RemoveDir(FullPath);

    XCBListSize--;
    return OKE;
}

/**************************************************************************************************
 * SYSTEM / UTILS IMPLEMENTATION ******************************************************************
 **************************************************************************************************/ 

RetType GetFileNameFromPath(char *Path, char *OutputFileName, int MaxFileNameSize) {
    if (!Path || !OutputFileName) return ERR;
    char *LastSlash = strrchr(Path, '/');
    char *CleanName = (LastSlash) ? (LastSlash + 1) : Path;
    if (*CleanName == '\0') return ERR;

    strncpy(OutputFileName, CleanName, MaxFileNameSize - 1);
    OutputFileName[MaxFileNameSize - 1] = '\0';
    return OKE;
}


/**
 * @brief Helper to generate a unique filename based on current time.
 * @param buffer Output buffer for the filename.
 * @param size Size of the buffer.
 */
void GetTimeBasedFilenameTxt(char *buffer, size_t size) {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    /* Format: YYYYMMDD_HHMMSS.txt */
    strftime(buffer, size, "%Y%m%d_%H%M%S.txt", timeinfo);
}

/**
 * @brief Helper to generate a filename based on current time with optional extension.
 * @param buffer Output buffer for the filename.
 * @param size Size of the buffer.
 * @param ext Extension string (e.g., "png", "txt"). If NULL, no extension is added.
 */
void GetTimeBasedFilename(char *buffer, size_t size, const char *ext) {
    time_t rawtime;
    struct tm *timeinfo;
    char TimeStr[64];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    /* 1. Generate the base timestamp: YYYYMMDD_HHMMSS */
    strftime(TimeStr, sizeof(TimeStr), "%Y%m%d_%H%M%S", timeinfo);

    /* 2. Handle extension logic */
    if (ext == NULL || ext[0] == '\0') {
        /// If ext is NULL, just copy the timestamp
        snprintf(buffer, size, "%s", TimeStr);
    } else {
        /// If ext exists, append it with a dot
        snprintf(buffer, size, "%s.%s", TimeStr, ext);
    }
}



/**************************************************************************************************
 * PUBLIC LIST IMPLEMENTATION *********************************************************************
 **************************************************************************************************/ 

int XCBList_Scan(int WithNoLock) {
    if (!WithNoLock) LockList();

    DIR *DirStream = opendir(PATH_DIR_DB);
    struct dirent *Entry;
    struct stat FileStat;
    char FullPath[PATH_MAX];
    
    XCBListSize = 0;
    HeadIndex = -1; /// Reset Circle Buffer

    if (DirStream == NULL) {
        if (!WithNoLock) UnlockList();
        return ERR;
    }

    while ((Entry = readdir(DirStream)) != NULL) {
        if (Entry->d_name[0] == '.') continue;
        
        snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, Entry->d_name);
        
        if (XCBListSize < MAX_HISTORY_ITEMS) {
            if (stat(FullPath, &FileStat) == 0) {
                strncpy(XCBList[XCBListSize].Filename, Entry->d_name, NAME_MAX);
                XCBList[XCBListSize].Filename[NAME_MAX] = '\0';
                XCBList[XCBListSize].Timestamp = FileStat.st_mtime;
                XCBList[XCBListSize].FileType = GetFileTypeFromName(Entry->d_name);
                XCBListSize++;
            }
        } else {
            RemoveDir(FullPath); /// Excess items purged safely
        }
    }
    closedir(DirStream);

    /// Setup valid chronological Circle Buffer order
    if (XCBListSize > 0) {
        qsort(XCBList, XCBListSize, sizeof(sClipboardItem), CompareItemsAsc);
        HeadIndex = XCBListSize - 1; 
    }

    if (!WithNoLock) UnlockList();
    return XCBListSize;
}

RetType XCBList_PushItem(char Path[]) {
    char CleanName[256];

    xEntry1("XCBList_PushItem(%s)", Path);

    if (GetFileNameFromPath(Path, CleanName, sizeof(CleanName)) != OKE) return ERR;

    LockList();
    if (XCBListSize >= MAX_HISTORY_ITEMS) {
        Internal_PopOldest(NULL); 
    }

    /// Advance Head Index and wrap around
    HeadIndex = (HeadIndex + 1) % MAX_HISTORY_ITEMS;

    strncpy(XCBList[HeadIndex].Filename, CleanName, NAME_MAX);
    XCBList[HeadIndex].Filename[NAME_MAX] = '\0';
    XCBList[HeadIndex].Timestamp = time(NULL);
    XCBList[HeadIndex].FileType = GetFileTypeFromName(CleanName);

    XCBListSize++;
    UnlockList();
    return OKE;
}

RetType XCBList_PushItemWithExistCheck(char Path[]) {
    char CleanName[256];
    if (GetFileNameFromPath(Path, CleanName, sizeof(CleanName)) != OKE) return ERR;

    struct stat FileStat;
    char FullPath[PATH_MAX];
    snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, CleanName);

    if (stat(FullPath, &FileStat) != 0) return ERR;

    XCBList_PushItem(CleanName);

    LockList();
    XCBList[HeadIndex].Timestamp = FileStat.st_mtime;
    UnlockList();
    return OKE;
}

RetType XCBList_PopItem(sClipboardItem *Output) {
    LockList();
    RetType Ret = Internal_PopOldest(Output);
    UnlockList();
    return Ret;
}

RetType XCBList_GetItem(int n, sClipboardItem *Output) {
    LockList();
    int AllocIdx = Convert2AllocatedIndex(n);
    if (AllocIdx < 0) {
        UnlockList();
        return ERR;
    }
    
    if (Output != NULL) {
        memcpy(Output, &XCBList[AllocIdx], sizeof(sClipboardItem));
    }
    UnlockList();
    return OKE;
}

RetType XCBList_GetLatestItem(sClipboardItem *Output) {
    return XCBList_GetItem(0, Output);
}

int XCBList_GetItemSize(void) {
    int Size;
    LockList();
    Size = XCBListSize;
    UnlockList();
    return Size;
}

RetType XCBList_ReadAsBinary(int n, void* Output, int MaxOutputSize) {
    xEntry1("XCBList_ReadAsBinary(%d, %p, %d)", n, Output, MaxOutputSize);
    LockList();
    int AllocIdx = Convert2AllocatedIndex(n);
    if (AllocIdx < 0) { 
        UnlockList(); 
        return ERR_OVERFLOW; 
    }
    
    char FullPath[PATH_MAX];
    snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, XCBList[AllocIdx].Filename);
    UnlockList();

    xLog1("[XCBList_ReadAsBinary] FullPath=%s", FullPath);

    FILE *f = fopen(FullPath, "rb");
    if (!f) return ERR;
    
    if (Output == NULL) { 
        fclose(f); 
        return OKE; 
    }

    fseek(f, 0, SEEK_END);
    long FileSize = ftell(f);
    rewind(f);

    if (FileSize > (long)MaxOutputSize) { 
        fclose(f); 
        return ERR; 
    }
    
    size_t ReadSize = fread(Output, 1, FileSize, f);
    fclose(f);

    xLog1("[XCBList_ReadAsBinary] ReadSize=%d", ReadSize);
    
    return (ReadSize == (size_t)FileSize) ? OKE : ERR;
}

int XCBList_SetSelectedNum(int LinearIndex) {
    LockList();
    
    if (LinearIndex < 0 || LinearIndex >= XCBListSize) {
        xError("[XCBList_SetSelectedNum] Invalid index = %d", LinearIndex);
        UnlockList();
        return ERR;
    }
    
    XCBList_SelectedItem = LinearIndex;
    UnlockList();
    
    return OKE;
}

int XCBList_GetSelectedNum(void) {
    int SelectedNum;
    
    LockList();
    SelectedNum = XCBList_SelectedItem;
    UnlockList();
    
    if(SelectedNum < 0 || SelectedNum >= XCBListSize) {
        SelectedNum = 0;
    }

    return SelectedNum;
}

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
    
    /// 3. Copy data if Output is provided
    if (Output != NULL) {
        memcpy(Output, &XCBList[AllocIdx], sizeof(sClipboardItem));
    }
    
    UnlockList();
    return OKE;
}

/**************************************************************************************************
 * SYSTEMCALL HELPER SECTION **********************************************************************
**************************************************************************************************/ 

/** * @brief Handle and log errors occurred during directory creation.
 * @param FunctionName Name of the caller function for tracing.
 * @param Path The directory path that failed to be created.
 * @param ErrorCode The errno value captured after mkdir() failure.
 */
void PrintError_Mkdir(const char FunctionName[], const char Path[], int ErrorCode) {
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
            // Use strerror to print the standard system error message
            xError("[%s] Failed to create %s: %s (errno: %d)", 
                   FunctionName, Path, strerror(ErrorCode), ErrorCode);
            break;
    }
}

/** * @brief Check for directory existence and create it if missing.
 * @param path The string path of the directory to ensure.
 * @return OKE on success or if exists, otherwise returns error code from mkdir.
 */
RetType EnsureDir(char path[]){
    xEntry2("EnsureDir");
    /*Local variables*/
    RetType RetVal;
    /*Make stat instance*/
    struct stat Stat;
    /*Check root dir*/
    RetVal = stat(path, &Stat);
    if( RetVal != 0){
        /*Root dir not found --> Make root dir */
        RetVal = mkdir(path, 0755);
        if (RetVal == 0) {
            xLog2("[EnsureDir] %s created successfully!", path);
        } else {
            PrintError_Mkdir("EnsureDir", path, errno); /* Use errno here or RetVal if mkdir returns errno */
        }
    }
    xExit2("EnsureDir");
    return (RetVal == 0) ? OKE : ERR; /* Simplified return */
}

/** * @brief Recursively removes a directory and all its contents.
 * @param path The directory path to be removed.
 * @return OKE on success, ERR on failure.
 */
RetType RemoveDir(const char path[]) {
    xEntry2("RemoveDir");
    
    DIR *dir_stream = NULL;
    struct dirent *entry;
    char sub_path[PATH_MAX]; // Buffer to store the full path of child items
    RetType status = OKE;

    /// Attempt to remove as an empty directory first
    if (rmdir(path) == 0) {
        xLog2("[RemoveDir] Empty directory removed: %s", path);
        xExit2("RemoveDir: OKE");
        return OKE;
    }

    /// If rmdir fails because it's not empty, we browse its content
    dir_stream = opendir(path);
    if (dir_stream == NULL) {
        xError("[RemoveDir] Failed to open %s: %s", path, strerror(errno));
        xExit2("RemoveDir: ERR");
        return ERR;
    }

    /// Iterate through each item in the directory
    while ((entry = readdir(dir_stream)) != NULL) {
        /// Skip the special directories "." and ".." to avoid infinite recursion
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /// Construct the full path: parent_path/child_name
        snprintf(sub_path, sizeof(sub_path), "%s/%s", path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            /// Recursive call for sub-directories
            RemoveDir(sub_path);
        } else {
            /// Remove files or symbolic links
            if (unlink(sub_path) == 0) {
                xLog2("[RemoveDir] File deleted: %s", sub_path);
            } else {
                xError("[RemoveDir] Failed to delete file: %s", sub_path);
            }
        }
    }

    /// Close the stream before removing the now-empty parent directory
    closedir(dir_stream);

    /// Final step: remove the current directory after it's been emptied
    if (rmdir(path) == 0) {
        xLog2("[RemoveDir] Directory cleaned and removed: %s", path);
    } else {
        xError("[RemoveDir] Final rmdir failed for %s: %s", path, strerror(errno));
        status = ERR;
    }

    xExit2("RemoveDir");
    return status;
}

/** * @brief Initialize the database directory structure.
 * @return OKE if all directories are ready, error code otherwise.
 */
RetType EnsureDB(void){
    xEntry1("EnsureDB");
    /*Local variables*/
    RetType RetVal;
    struct stat Status;
    /*Show PATH infos */
    xLog1("[EnsureDB] PATH_DIR_ROOT=%s",   PATH_DIR_ROOT);
    xLog1("[EnsureDB] PATH_DIR_DB=%s",     PATH_DIR_DB);
    xLog1("[EnsureDB] PATH_ITEM=%s",       PATH_ITEM);
    
    /*Clear Rootdir (Optional: Be careful not to delete user data every run)*/
    /* RetVal = RemoveDir(PATH_DIR_ROOT); */ 

    /*Call EnsureDir to make ROOT_DIR, DB_DIR*/
    RetVal = EnsureDir(PATH_DIR_ROOT);
    if(RetVal != OKE) return RetVal;
    
    RetVal = EnsureDir(PATH_DIR_DB);
    if(RetVal != OKE) return RetVal;
    
    /*Check list */
    RetVal = stat(PATH_ITEM, &Status);
    if(RetVal < 0){
        xLog1("[EnsureDB] No history to load (First run)!");
    }

    xExit1("EnsureDB");
    return OKE;
}

/**
 * @brief Simple helper to save text data to file (Demostration).
 * @param data The data buffer.
 * @param len Length of data.
 */
void SaveClipboardToFile(const char* data, int len) {
    xEntry1("SaveClipboardToFile");
    /* Note: In real app, generate unique filename based on timestamp */
    FILE *fp = fopen(PATH_ITEM, "w"); // Overwrite for now
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

