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
 * CONFIG SECTION *********************************************************************************
 **************************************************************************************************/ 

#define PATH_DIR_ROOT       "/home/fus/Documents/ClipboardCaputure/.tmp"
#define PATH_DIR_DB         PATH_DIR_ROOT "/DBs"
#define PATH_ITEM           PATH_DIR_ROOT "/ClipboardItem"

/// @brief Official program name used for logging and metadata.
#define PROG_NAME           "ClipboardCapture v0.0.1"
/// @brief Unique string ID for X11 internal property exchange.
#define PROP_NAME           "NGXX_FUS_CLIPBOARD_PROP"

#define MAX_HISTORY_ITEMS   1000 


/**************************************************************************************************
 * CONFIG/DECLARE SECTION *************************************************************************
 **************************************************************************************************/ 

enum eTogglePopUpStatus {eNOT_STARTED, eREQ_START, eREQ_HIDE, eHIDEN, eREQ_SHOW, eSHOWN};
enum eBinaryState {eACTIVATE = 1, eDEACTIVATE = 0};
/* * GLOBAL ATOM DEFINITIONS 
 * (In a real project, these should be in a .c file, but for this header-impl style, 
 * we define them here).
 */
extern xcb_atom_t                   AtomClipboard;
extern xcb_atom_t                   AtomUtf8;
extern xcb_atom_t                   AtomTarget;
extern xcb_atom_t                   AtomPng;
extern xcb_atom_t                   AtomJpeg;
extern xcb_atom_t                   AtomProperty;
extern xcb_window_t                 MyWindow;
extern xcb_connection_t *           Connection;
extern volatile sig_atomic_t        TogglePopUpStatus;
extern volatile sig_atomic_t        RequestExit;
extern void *                       ActiveData;
extern size_t                       ActiveDataLen;
extern xcb_atom_t                   ActiveDataType;

#endif /*__SETUP_H__*/
