#ifndef __SETUP_H__
#define __SETUP_H__

/**************************************************************************************************
 * INCLUDE SECTION ********************************************************************************
 **************************************************************************************************/ 

#include <asm-generic/errno-base.h>
/* Ensure xUniversalReturn.h defines xEntry, xExit, xLog, xError macros */
#include <xUniversalReturn.h> 
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
#include <signal.h>
#include <pthread.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xfixes.h> /* IMPORTANT: Need libxcb-xfixes */

#include <zlib.h> /* link -lz */

#include "xUniversal.h"


/**************************************************************************************************
 * MACRO CONFIGURATIONS SECTION *******************************************************************
 **************************************************************************************************/ 

/// @brief Official program name used for logging and metadata.
#define PROG_NAME               "ClipboardCapture v0.0.1"
/// @brief Unique string ID for X11 internal property exchange.
#define PROP_NAME               "NGXX_FUS_CLIPBOARD_PROP"

/// @brief Maximum number of items retained in the clipboard history.
#define MAX_HISTORY_ITEMS       1000 

/// @brief Root directory for all temporary runtime files.
#define PATH_DIR_ROOT           "/home/fus/Documents/ClipboardCaputure/.tmp"
/// @brief Sub-directory storing the raw clipboard data chunks/files.
#define PATH_DIR_DB             PATH_DIR_ROOT "/DBs"
/// @brief Path to the file storing the serialized clipboard history list.
#define PATH_ITEM               PATH_DIR_ROOT "/ClipboardItem"
/// @brief Path to the temporary text file used to feed the Rofi menu.
#define PATH_FILE_ROFI_MENU     PATH_DIR_ROOT "/XCBRofiMenu.txt"

/// @brief Toggle switch to enable (1) or disable (0) Rofi UI integration.
#define ROFI_SUPPORT            1
/// @brief Maximum character length for text previews in the Rofi menu.
#define PREVIEW_TXT_LEN         80

/**************************************************************************************************
 * ENUMERATIONS SECTION ***************************************************************************
 **************************************************************************************************/ 

/// @brief State machine values for managing the UI PopUp visibility.
enum eTogglePopUpStatus {
    eNOT_STARTED, 
    eREQ_START, 
    eREQ_HIDE, 
    eHIDEN, 
    eREQ_SHOW, 
    eSHOWN
};

/// @brief Standard binary state enumeration.
enum eBinaryState {
    eDEACTIVATE = 0,
    eACTIVATE   = 1
};

/**************************************************************************************************
 * GLOBAL EXTERN DECLARATIONS *********************************************************************
 **************************************************************************************************/ 

/// --- X11 Atoms ---
/// @brief Atom representing the X11 CLIPBOARD selection.
extern xcb_atom_t               AtomClipboard;
/// @brief Atom representing UTF8_STRING text format.
extern xcb_atom_t               AtomUtf8;
/// @brief Atom used to request supported data formats.
extern xcb_atom_t               AtomTarget;
/// @brief Atom representing PNG image MIME type.
extern xcb_atom_t               AtomPng;
/// @brief Atom representing JPEG image MIME type.
extern xcb_atom_t               AtomJpeg;
/// @brief Custom property Atom used as a temporary buffer for selection transfers.
extern xcb_atom_t               AtomProperty;

/// --- Active Clipboard Data ---
/// @brief Pointer to the active data currently held in the clipboard.
extern void * ActiveData;
/// @brief Size of the active clipboard data in bytes.
extern size_t                   ActiveDataLen;
/// @brief The X11 Atom representing the format of the active data.
extern xcb_atom_t               ActiveDataType;

/// --- X11 Core ---
/// @brief Hidden X11 window used to listen for clipboard events.
extern xcb_window_t             MyWindow;
/// @brief Pointer to the active XCB connection.
extern xcb_connection_t * Connection;

/// --- Thread Signaling Flags ---
/// @brief Flag to trigger the UI popup menu.
extern volatile sig_atomic_t    TogglePopUpStatus;
/// @brief Flag to signal all threads to gracefully exit.
extern volatile sig_atomic_t    RequestExit;

#endif /*__SETUP_H__*/
