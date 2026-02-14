#include "CBC_SysFile.h"


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


