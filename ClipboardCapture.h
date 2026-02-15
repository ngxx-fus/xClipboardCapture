#ifndef __CLIPBOARD_CAPTURE_H__
#define __CLIPBOARD_CAPTURE_H__

#include "CBC_Setup.h"
#include "CBC_SysFile.h"

/**************************************************************************************************
 * GLOBAL VARIABLES & ATOMS SECTION ***************************************************************
 **************************************************************************************************/ 

/// @brief Flag to trigger the UI popup menu (eREQ_SHOW, eREQ_HIDE, or eNOT_STARTED).
extern volatile sig_atomic_t   TogglePopUpStatus;
/// @brief Flag to signal all threads to gracefully exit.
extern volatile sig_atomic_t   RequestExit;
/// @brief Flag to trigger the injection of the selected clipboard item into the active window.
extern volatile sig_atomic_t   ReqTestInject;

/**************************************************************************************************
 * X11 SERVER SECTION PROTOTYPES ******************************************************************
 **************************************************************************************************/ 

/** * @brief Intern a single atom by string name.
 * @param c Connection to X server.
 * @param name String name of the atom.
 * @return The atom ID.
 */
xcb_atom_t GetAtomByName(xcb_connection_t *c, const char *name);

/**
 * @brief Initialize all global atoms used in the application.
 * @param c Connection to X server.
 */
void InitAtoms(xcb_connection_t *c);

/**
 * @brief Create a hidden dummy window to receive XFixes events.
 * @param c Connection to X server.
 * @param screen The screen structure.
 * @return The window ID.
 */
xcb_window_t CreateListenerWindow(xcb_connection_t *c);

/** * @brief Request XFixes extension to notify us on clipboard changes.
 * @param c Connection to X server.
 * @param window The dummy window to receive events.
 */
void SubscribeClipboardEvents(xcb_connection_t *c, xcb_window_t window);

/**************************************************************************************************
 * CLIPBOARD PROVIDER SECTION PROTOTYPES **********************************************************
 **************************************************************************************************/ 

/// @brief Loads data into memory and claims ownership of the X11 Clipboard.
/// @param c The XCB connection.
/// @param win Our listener window ID.
/// @param data Pointer to the actual data (text string or image bytes).
/// @param len Length of the data in bytes.
/// @param type The format of the data (AtomUtf8, AtomPng, etc.).
void SetClipboardData(xcb_connection_t *c, xcb_window_t win, void *data, size_t len, xcb_atom_t type);


/**************************************************************************************************
 * SIGNAL HANDLER SECTION PROTOTYPES **************************************************************
 **************************************************************************************************/ 

/**
 * @brief Asynchronous handler called directly by the Linux Kernel when a signal arrives.
 * @param SigNum The ID of the received POSIX signal (e.g., SIGINT, SIGUSR1).
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
 * @brief The main blocking loop of the application.
 * NOW COMPLETED with Data Fetching Logic.
 */
RetType XClipboardRuntime(int Param);

/**************************************************************************************************
 * LIFECYCLE SECTION PROTOTYPES *******************************************************************
 **************************************************************************************************/ 

/**
 * @brief Cleans up resources and waits for the background threads to exit safely.
 * Registered via atexit() to run automatically upon termination.
 */
void ClipboardCaptureFinalize(void);

/**
 * @brief Initializes directories, starts the Signal Thread, and blocks on X11 loop.
 * @return OKE on success, ERR on failure.
 */
RetType ClipboardCaptureInitialize(void);

/**************************************************************************************************
 * ROFI SUPPORT ***********************************************************************************
 **************************************************************************************************/ 

#if (ROFI_SUPPORT==1)

    /// @brief Reads file content and writes a formatted Rofi menu item directly to the temp file.
    /// @param OutFile The temporary file stream (/tmp/cbc_rofi.txt).
    /// @param Index The logical index of the clipboard item.
    /// @param Item Pointer to the clipboard item data.
    /// @return OKE on success.
    RetType WriteRofiMenuItem(FILE *OutFile, int Index, sClipboardItem *Item);
    
    /// @brief Calls the Rofi dmenu interface to let the user select a clipboard item.
    void ShowRofiMenu(void);

#endif /*ROFI_SUPPORT*/


#endif /*__CLIPBOARD_CAPTURE_H__*/

/**************************************************************************************************
 * EOF ********************************************************************************************
 **************************************************************************************************/ 


