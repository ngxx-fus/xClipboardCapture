#ifndef __CLIPBOARD_CAPTURE_H__
#define __CLIPBOARD_CAPTURE_H__

#include "CBC_Setup.h"
#include "CBC_SysFile.h"

/**************************************************************************************************
 * ENUMERATIONS SECTION ***************************************************************************
 **************************************************************************************************/ 

/**
 * @brief State machine values for managing the UI PopUp visibility.
 */
enum eTogglePopUpStatus {
    eNOT_STARTED, 
    eREQ_START, 
    eREQ_HIDE, 
    eHIDEN, 
    eREQ_SHOW, 
    eSHOWN
};

/**
 * @brief Standard binary state enumeration.
 */
enum eBinaryState {
    eDEACTIVATE = 0,
    eACTIVATE   = 1
};

/**************************************************************************************************
 * X11 ATOMS GLOBAL *******************************************************************************
 **************************************************************************************************/ 

/**
 * @brief Atom representing the X11 CLIPBOARD selection.
 */
extern xcb_atom_t               AtomClipboard;

/**
 * @brief Atom representing UTF8_STRING text format.
 */
extern xcb_atom_t               AtomUtf8;

/**
 * @brief Atom used to request supported data formats.
 */
extern xcb_atom_t               AtomTarget;

/**
 * @brief Atom representing PNG image MIME type.
 */
extern xcb_atom_t               AtomPng;

/**
 * @brief Atom representing JPEG image MIME type.
 */
extern xcb_atom_t               AtomJpeg;

/**
 * @brief Custom property Atom used as a temporary buffer for selection transfers.
 */
extern xcb_atom_t               AtomProperty;

/* --- Active Clipboard Data --- */

/**************************************************************************************************
 * ACTIVE CLIPBOARD DATA **************************************************************************
 **************************************************************************************************/ 


/**
 * @brief Pointer to the active data currently held in the clipboard.
 */
extern void * ActiveData;

/**
 * @brief Size of the active clipboard data in bytes.
 */
extern size_t                   ActiveDataLen;

/**
 * @brief The X11 Atom representing the format of the active data.
 */
extern xcb_atom_t               ActiveDataType;

/* --- X11 Core --- */

/**
 * @brief Hidden X11 window used to listen for clipboard events.
 */
extern xcb_window_t             MyWindow;

/**
 * @brief Pointer to the active XCB connection.
 */
extern xcb_connection_t * Connection;

/* --- Thread Signaling Flags --- */

/**
 * @brief Flag to trigger the UI popup menu.
 * @note Modified asynchronously by signal handlers.
 */
extern volatile sig_atomic_t    TogglePopUpStatus;

/**
 * @brief Flag to signal all threads to gracefully exit.
 * @note Modified asynchronously by signal handlers.
 */
extern volatile sig_atomic_t    RequestExit;

/**************************************************************************************************
 * GLOBAL VARIABLES & ATOMS SECTION ***************************************************************
 **************************************************************************************************/ 

/**
 * @brief Flag to trigger the UI popup menu (eREQ_SHOW, eREQ_HIDE, or eNOT_STARTED).
 * @note This variable is modified by the signal handler and read by the main thread.
 */
extern volatile sig_atomic_t TogglePopUpStatus;

/**
 * @brief Flag to signal all threads to gracefully exit.
 * @note Set to eACTIVATE upon receiving SIGINT or SIGTERM.
 */
extern volatile sig_atomic_t RequestExit;

/**
 * @brief Flag to trigger the injection of the selected clipboard item into the active window.
 */
extern volatile sig_atomic_t ReqTestInject;

/**************************************************************************************************
 * X11 SERVER SECTION PROTOTYPES ******************************************************************
 **************************************************************************************************/ 

/**
 * @brief Intern a single atom by string name.
 * @param c Connection to the X server.
 * @param name String name of the atom to be interned.
 * @return The requested atom ID.
 */
xcb_atom_t GetAtomByName(xcb_connection_t *c, const char *name);

/**
 * @brief Initializes all global atoms used in the application.
 * @param c Connection to the X server.
 * @note Must be called before any clipboard property operations.
 */
void InitAtoms(xcb_connection_t *c);

/**
 * @brief Creates a hidden dummy window to receive XFixes events.
 * @param c Connection to the X server.
 * @return The window ID of the created dummy window.
 */
xcb_window_t CreateListenerWindow(xcb_connection_t *c);

/**
 * @brief Requests the XFixes extension to notify us on clipboard ownership changes.
 * @param c Connection to the X server.
 * @param window The dummy window ID to receive the events.
 */
void SubscribeClipboardEvents(xcb_connection_t *c, xcb_window_t window);

/**************************************************************************************************
 * CLIPBOARD PROVIDER SECTION PROTOTYPES **********************************************************
 **************************************************************************************************/ 

/**
 * @brief Loads data into memory and claims ownership of the X11 Clipboard.
 * @param c Connection to the X server.
 * @param win Our listener window ID.
 * @param data Pointer to the actual data (text string or image bytes).
 * @param len Length of the data in bytes.
 * @param type The format of the data (e.g., AtomUtf8, AtomPng).
 * @note If the requested data size exceeds INCR limit, the INCR protocol will be triggered automatically.
 */
void SetClipboardData(xcb_connection_t *c, xcb_window_t win, void *data, size_t len, xcb_atom_t type);

/**************************************************************************************************
 * SIGNAL HANDLER SECTION PROTOTYPES **************************************************************
 **************************************************************************************************/ 

/**
 * @brief Asynchronous handler called directly by the Linux Kernel when a signal arrives.
 * @param SigNum The ID of the received POSIX signal (e.g., SIGINT, SIGUSR1).
 * @note Avoid calling non-reentrant functions (like printf/malloc) inside this handler.
 */
void SignalEventHandler(int SigNum);

/**
 * @brief Registers the application's signal handlers with the Operating System.
 * @return OKE on success.
 */
RetType RegisterSignal(void);

/**
 * @brief The main loop for the Signal Thread. It idles and waits for OS signals.
 * @param Param Optional parameter passed when the thread starts (can be 0 or NULL).
 * @return OKE when the thread exits cleanly.
 */
RetType SignalRuntime(int Param);

/**************************************************************************************************
 * CLIPBOARD RUNTIME SECTION PROTOTYPES ***********************************************************
 **************************************************************************************************/ 

/**
 * @brief The main blocking loop of the application handling X11 events and data fetching.
 * @param Param Optional parameter passed when the thread starts.
 * @return OKE upon successful termination.
 */
RetType XClipboardRuntime(int Param);

/**************************************************************************************************
 * LIFECYCLE SECTION PROTOTYPES *******************************************************************
 **************************************************************************************************/ 

/**
 * @brief Cleans up resources and waits for the background threads to exit safely.
 * @note Registered via atexit() to run automatically upon termination.
 */
void ClipboardCaptureFinalize(void);

/**
 * @brief Initializes directories, starts the Signal Thread, and prepares the X11 environment.
 * @return OKE on success, ERR on initialization failure.
 */
RetType ClipboardCaptureInitialize(void);

/**************************************************************************************************
 * ROFI SUPPORT ***********************************************************************************
 **************************************************************************************************/ 

#if (ROFI_SUPPORT == 1)

/**
 * @brief Reads file content and writes a formatted Rofi menu item directly to the temp file.
 * @param OutFile The temporary file stream (/tmp/cbc_rofi.txt).
 * @param Index The logical index of the clipboard item.
 * @param Item Pointer to the clipboard item data.
 * @return OKE on success.
 * @note Newlines and unprintable characters are sanitized to prevent Rofi parsing errors.
 */
RetType WriteRofiMenuItem(FILE *OutFile, int Index, sClipboardItem *Item);

/**
 * @brief Calls the Rofi dmenu interface to let the user select a clipboard item.
 * @note This function blocks the caller thread until the user makes a selection or presses ESC.
 */
void ShowRofiMenu(void);

#endif /*(ROFI_SUPPORT == 1)*/

#endif /*__CLIPBOARD_CAPTURE_H__*/

/**************************************************************************************************
 * EOF ********************************************************************************************
 **************************************************************************************************/



