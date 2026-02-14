#ifndef __CLIPBOARD_CAPTURE_H__
#define __CLIPBOARD_CAPTURE_H__

/**************************************************************************************************
 * INCLUDE SECTION ********************************************************************************
 **************************************************************************************************/ 

#include <asm-generic/errno-base.h>
#include <xUniversalReturn.h>
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include <zlib.h> /* link -lz */

#include "xUniversal.h"

/**************************************************************************************************
 * CONFIG/DECLARE SECTION *************************************************************************
 **************************************************************************************************/ 

#define PATH_DIR_ROOT       "/home/fus/Documents/ClipboardCaputure/.tmp"
#define PATH_DIR_DB         PATH_DIR_ROOT "/DBs"
#define PATH_ITEM        PATH_DIR_ROOT "/ClipboardItem"

/// @brief Official program name used for logging and metadata.
#define PROG_NAME "ClipboardCapture v0.0.1"
/// @brief Unique string ID for X11 internal property exchange.
#define PROP_NAME "NGXX_FUS_CLIPBOARD_PROP"
/// @brief Atom representing the X11 clipboard selection.
extern xcb_atom_t AtomClipboard;
/// @brief Atom representing UTF-8 string format.
extern xcb_atom_t AtomUtf8;
/// @brief Atom representing list of supported target formats.
extern xcb_atom_t AtomTarget;
/// @brief Atom representing PNG image MIME type ("image/png").
extern xcb_atom_t AtomPng;
/// @brief Atom representing JPEG image MIME type ("image/jpeg").
extern xcb_atom_t AtomJpeg;
/// @brief Atom representing internal property used for clipboard data exchange.
extern xcb_atom_t AtomProperty;

/**************************************************************************************************
 * PROTOTYPE FUNCTION SECTION *********************************************************************
 **************************************************************************************************/ 

/// @brief Handle and log errors occurred during directory creation.
/// @param FunctionName Name of the caller function for tracing.
/// @param Path The directory path that failed to be created.
/// @param ErrorCode The errno value captured after mkdir() failure.
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

/// @brief Check for directory existence and create it if missing.
/// @param path The string path of the directory to ensure.
/// @return OKE on success or if exists, otherwise returns error code from mkdir.
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
            PrintError_Mkdir("EnsureDir", path, RetVal);
        }
    }
    xExit2("EnsureDir");
    return RetVal;
}

/// @brief Recursively removes a directory and all its contents.
/// @param path The directory path to be removed.
/// @return OKE on success, ERR on failure.
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

/// @brief Initialize the database directory structure.
/// @return OKE if all directories are ready, error code otherwise.
RetType EnsureDB(void){
    xEntry1("EnsureDB");
    /*Local variables*/
    RetType RetVal;
    struct stat Status;
    /*Show PATH infos */
    xLog1("[EnsureDB] PATH_DIR_ROOT=%s",   PATH_DIR_ROOT);
    xLog1("[EnsureDB] PATH_DIR_DB=%s",     PATH_DIR_DB);
    xLog1("[EnsureDB] PATH_ITEM=%s",       PATH_ITEM);
    
    /*Clear Rootdir*/
    RetVal = RemoveDir(PATH_DIR_ROOT);

    /*Call EnsureDir to make ROOT_DIR, DB_DIR*/
    RetVal = EnsureDir(PATH_DIR_ROOT);
    if(RetVal != OKE) return RetVal;
    
    RetVal = EnsureDir(PATH_DIR_DB);
    if(RetVal != OKE) return RetVal;
    
    /*Check list */
    RetVal = stat(PATH_ITEM, &Status);
    if(RetVal<0){
        xLog1("[EnsureDB] No history to load!");
    }

    xExit1("EnsureDB");
    return OKE;
}


/* Get atom by name */
xcb_atom_t GetAtomByName(xcb_connection_t *c, const char *name) {
    xEntry1("GetAtomByName(name=%s)", name);
    xcb_intern_atom_cookie_t ck = xcb_intern_atom(c, 0, strlen(name), name);
    xcb_intern_atom_reply_t *r = xcb_intern_atom_reply(c, ck, NULL);
    if (!r) {
        xExit("get_atom(): XCB_ATOM_NONE, !r");
        return XCB_ATOM_NONE;
    }
    xcb_atom_t a = r->atom;
    free(r);
    xExit("get_atom(): %u", a);
    return a;
}

/// @brief Request XFixes extension to notify us on clipboard changes.
void SubscribeClipboardEvents(xcb_connection_t *c, xcb_window_t window) {
    // 1. Query XFixes version to ensure it's supported
    xcb_xfixes_query_version(c, XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION);

    // 2. Select selection events for the CLIPBOARD atom
    uint32_t mask = XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER |
                    XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY |
                    XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE;

    xcb_xfixes_select_selection_input(c, window, AtomClipboard, mask);
    xcb_flush(c);
}

void CoreRuntime(){


}

#endif /*__CLIPBOARD_CAPTURE_H__*/
