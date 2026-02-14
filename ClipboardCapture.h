#ifndef __CLIPBOARD_CAPTURE_H__
#define __CLIPBOARD_CAPTURE_H__

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

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xfixes.h> /* IMPORTANT: Need libxcb-xfixes */

#include <zlib.h> /* link -lz */

#include "xUniversal.h"

/**************************************************************************************************
 * CONFIG/DECLARE SECTION *************************************************************************
 **************************************************************************************************/ 

#define PATH_DIR_ROOT       "/home/fus/Documents/ClipboardCaputure/.tmp"
#define PATH_DIR_DB         PATH_DIR_ROOT "/DBs"
#define PATH_ITEM           PATH_DIR_ROOT "/ClipboardItem"

/// @brief Official program name used for logging and metadata.
#define PROG_NAME "ClipboardCapture v0.0.1"
/// @brief Unique string ID for X11 internal property exchange.
#define PROP_NAME "NGXX_FUS_CLIPBOARD_PROP"

/* * GLOBAL ATOM DEFINITIONS 
 * (In a real project, these should be in a .c file, but for this header-impl style, 
 * we define them here).
 */
xcb_atom_t AtomClipboard;
xcb_atom_t AtomUtf8;
xcb_atom_t AtomTarget;
xcb_atom_t AtomPng;
xcb_atom_t AtomJpeg;
xcb_atom_t AtomProperty;

xcb_window_t MyWindow                   = 0;
xcb_connection_t *Connection            = NULL;


enum eTogglePopUpStatus {eNOT_STARTED, eREQ_START, eREQ_HIDE, eHIDEN, eREQ_SHOW, eSHOWN};
volatile sig_atomic_t TogglePopUpStatus = eNOT_STARTED;
volatile sig_atomic_t RequestExit       = 0;

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
 * X11 SEVER SECTION ******************************************************************************
 **************************************************************************************************/ 

/** * @brief Intern a single atom by string name.
 * @param c Connection to X server.
 * @param name String name of the atom.
 * @return The atom ID.
 */
xcb_atom_t GetAtomByName(xcb_connection_t *c, const char *name) {
    xEntry1("GetAtomByName(name=%s)", name);
    xcb_intern_atom_cookie_t ck = xcb_intern_atom(c, 0, strlen(name), name);
    xcb_intern_atom_reply_t *r = xcb_intern_atom_reply(c, ck, NULL);
    if (!r) {
        xExit("GetAtomByName(): XCB_ATOM_NONE, !r");
        return XCB_ATOM_NONE;
    }
    xcb_atom_t a = r->atom;
    free(r);
    xExit("GetAtomByName(): %u", a);
    return a;
}

/**
 * @brief Initialize all global atoms used in the application.
 * @param c Connection to X server.
 */
void InitAtoms(xcb_connection_t *c) {
    xEntry1("InitAtoms");
    AtomClipboard = GetAtomByName(c, "CLIPBOARD");
    AtomUtf8      = GetAtomByName(c, "UTF8_STRING");
    AtomTarget    = GetAtomByName(c, "TARGETS");
    AtomPng       = GetAtomByName(c, "image/png");
    AtomJpeg      = GetAtomByName(c, "image/jpeg");
    AtomProperty  = GetAtomByName(c, PROP_NAME);
    xExit1("InitAtoms");
}

/**
 * @brief Create a hidden dummy window to receive XFixes events.
 * @param c Connection to X server.
 * @param screen The screen structure.
 * @return The window ID.
 */
xcb_window_t CreateListenerWindow(xcb_connection_t *c) {
    xEntry1("CreateListenerWindow");
    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    xcb_window_t win = xcb_generate_id(c);
    
    /* Create an unmapped (invisible) window */
    xcb_create_window(c, XCB_COPY_FROM_PARENT, win, screen->root,
                      0, 0, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      0, NULL);
    
    xExit1("CreateListenerWindow: %u", win);
    return win;
}

/** * @brief Request XFixes extension to notify us on clipboard changes.
 * @param c Connection to X server.
 * @param window The dummy window to receive events.
 */
void SubscribeClipboardEvents(xcb_connection_t *c, xcb_window_t window) {
    xEntry1("SubscribeClipboardEvents");
    
    /* 1. Query XFixes version to ensure it's supported and initialize extension */
    xcb_xfixes_query_version_cookie_t ck = xcb_xfixes_query_version(c, XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION);
    xcb_xfixes_query_version_reply_t *r = xcb_xfixes_query_version_reply(c, ck, NULL);
    if(r) free(r);

    /* 2. Select selection events for the CLIPBOARD atom */
    uint32_t mask = XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER |
                    XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY |
                    XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE;

    xcb_xfixes_select_selection_input(c, window, AtomClipboard, mask);
    xcb_flush(c);
    xExit1("SubscribeClipboardEvents: Done");
}

/**
 * @brief Helper to generate a unique filename based on current time.
 * @param buffer Output buffer for the filename.
 * @param size Size of the buffer.
 */
void GetTimeBasedFilename(char *buffer, size_t size) {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    /* Format: YYYYMMDD_HHMMSS.txt */
    strftime(buffer, size, "%Y%m%d_%H%M%S.txt", timeinfo);
}


/**************************************************************************************************
 * CLIPBOARD PROVIDER SECTION *********************************************************************
 **************************************************************************************************/ 

/// @brief Global variables to hold the data we want to "paste" to other apps.
void *ActiveData = NULL;
size_t ActiveDataLen = 0;
xcb_atom_t ActiveDataType = 0; /// E.g., AtomUtf8 or AtomPng

/// @brief Loads data into memory and claims ownership of the X11 Clipboard.
/// @param c The XCB connection.
/// @param win Our listener window ID.
/// @param data Pointer to the actual data (text string or image bytes).
/// @param len Length of the data in bytes.
/// @param type The format of the data (AtomUtf8, AtomPng, etc.).
void SetClipboardData(xcb_connection_t *c, xcb_window_t win, void *data, size_t len, xcb_atom_t type) {
    xEntry1("SetClipboardData");
    
    /// 1. Store the data locally (Free previous data if it exists to avoid memory leak)
    if (ActiveData) {
        free(ActiveData);
    }
    ActiveData = malloc(len);
    memcpy(ActiveData, data, len);
    ActiveDataLen = len;
    ActiveDataType = type;

    /// 2. Tell X Server: "I am the new owner of the CLIPBOARD!"
    xcb_set_selection_owner(c, win, AtomClipboard, XCB_CURRENT_TIME);
    
    /// 3. Verify if we successfully became the owner
    xcb_get_selection_owner_cookie_t ck = xcb_get_selection_owner(c, AtomClipboard);
    xcb_get_selection_owner_reply_t *r = xcb_get_selection_owner_reply(c, ck, NULL);
    
    if (r && r->owner == win) {
        xLog1("[SetClipboardData] Successfully claimed Clipboard ownership! Ready to serve.");
    } else {
        xError("[SetClipboardData] Failed to claim ownership.");
    }
    
    if (r) free(r);
    xcb_flush(c);
    
    xExit1("SetClipboardData");
}



/**************************************************************************************************
 * SIGNAL HANDLER SECTION *************************************************************************
 **************************************************************************************************/ 

/**
 * @brief Asynchronous handler called directly by the Linux Kernel when a signal arrives.
 * @param SigNum The ID of the received POSIX signal (e.g., SIGINT, SIGUSR1).
 */
void SignalEventHandler(int SigNum) {
    /* * NOTE: In strict system programming, calling printf/xLog inside a signal handler 
     * can be unsafe if it interrupts another printf. However, for learning and 
     * debugging the flow, it is acceptable to log it here.
     */
    xLog1("[SignalEventHandler] Was called with SigNum=%d", SigNum);

    if (SigNum == SIGINT || SigNum == SIGTERM) {
        /* User pressed Ctrl+C or sent a termination kill command */
        /// RequestExit = 1;
    } 
    else if (SigNum == SIGUSR1) {
        /* Custom signal received from another terminal (pkill -SIGUSR1 TestProg) */
        const char *TestStr = "Hello from Phu's Clipboard! Khai code thành công :>";
        xLog1("[Test] Injecting string into X11 Clipboard...");
            
        /* Gọi hàm SetClipboardData mình đã viết ở phần trước */
        /// / SetClipboardData(Connection, MyWindow, (void *)TestStr, strlen(TestStr), AtomUtf8);

        /* Toggle logic: If currently hidden or not started, request to show */
        if (TogglePopUpStatus == eHIDEN || TogglePopUpStatus == eNOT_STARTED) {
            TogglePopUpStatus = eREQ_SHOW;
        } 
        /* If currently shown, request to hide */
        else if (TogglePopUpStatus == eSHOWN) {
            TogglePopUpStatus = eREQ_HIDE;
        }
    }
}

/**
 * @brief Registers the application's signal handlers with the Operating System.
 * @return OKE on success.
 */
RetType RegisterSignal(void) {
    xEntry1("RegisterSignal");

    /* Bind specific OS signals to our custom handler function */
    signal(SIGUSR1, SignalEventHandler); /* Catch custom command: kill -SIGUSR1 <PID> */
    signal(SIGINT,  SignalEventHandler); /* Catch Ctrl+C to exit gracefully */
    signal(SIGTERM, SignalEventHandler); /* Catch standard kill command */

    xLog1("[RegisterSignal] Listening for OS signals... (PID: %d)", getpid());

    xExit1("RegisterSignal");
    return OKE; 
}

/**
 * @brief The main loop for the Signal Thread. It idles and waits for OS signals.
 * @param Param Optional parameter passed when the thread starts (can be 0 or NULL).
 * @return OKE when the thread exits cleanly.
 */
RetType SignalRuntime(int Param) {
    xEntry1("SignalRuntime");

    /* 1. Register the signals before entering the wait loop */
    RegisterSignal();

    /* * 2. Infinite loop to keep this thread alive, waiting for OS signals.
     * We do NOT use xcb_generic_event_t here because this thread does not 
     * talk to the X Server. It only talks to the Linux Kernel.
     */
    while (RequestExit == 0) {
        
        /* * pause() suspends the thread, consuming 0% CPU. 
         * It will instantly wake up ONLY when an OS signal arrives.
         */
        pause(); 

        /* * 3. When pause() wakes up, SignalEventHandler has already finished running.
         * We can now check the global flags that the handler modified.
         */
        if (RequestExit != 0) {
            xLog1("[SignalRuntime] Exit flag detected. Stopping signal thread...");
            break;
        }

        if (TogglePopUpStatus == eREQ_SHOW) {
            xLog1("[SignalRuntime] Action required: SHOW PopUp!");
            /* * Note: The actual drawing of the UI should happen in CoreRuntime. 
             * CoreRuntime will check this flag, draw the window, and change 
             * the status to eSHOWN.
             */
        } 
        else if (TogglePopUpStatus == eREQ_HIDE) {
            xLog1("[SignalRuntime] Action required: HIDE PopUp!");
            /* * Similarly, CoreRuntime will destroy/hide the window and 
             * change the status to eHIDEN.
             */
        }
    }

    xExit1("SignalRuntime");
    return OKE;
}

/**************************************************************************************************
 * CLIPBOARD RUNTIME SECTION **********************************************************************
 **************************************************************************************************/ 

/**
 * @brief The main blocking loop of the application.
 * NOW COMPLETED with Data Fetching Logic.
 */
RetType XClipboardRuntime(int Param){
    xEntry1("CoreRuntime");

    /* 1. Connect to X Server */
    Connection = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(Connection)) {
        xError("[CoreRuntime] Failed to connect to X Server!");
        return ERR;
    }

    /* 2. Get XFixes Extension Data */
    const xcb_query_extension_reply_t *xfixes_data;
    xfixes_data = xcb_get_extension_data(Connection, &xcb_xfixes_id);
    if (!xfixes_data || !xfixes_data->present) {
        xError("[CoreRuntime] XFixes extension not available!");
        xcb_disconnect(Connection);
        return ERR;
    }
    uint8_t XFixesEventBase = xfixes_data->first_event;

    /* 3. Initialize Environment */
    InitAtoms(Connection);
    /* Create a window to act as the "Receiver" of clipboard data */
    MyWindow = CreateListenerWindow(Connection);
    SubscribeClipboardEvents(Connection, MyWindow);

    xLog1("[CoreRuntime] Listening for Clipboard events...");

    xcb_generic_event_t *Event;
    
    /* 4. Main Event Loop (Blocking) */
    while ((Event = xcb_wait_for_event(Connection))) {
        
        /* Mask out the highest bit (indicates sent from another client) */
        uint8_t EventType = Event->response_type & ~0x80;


/* CASE A: Clipboard Owner Changed (Someone pressed Ctrl+C) */
        if (EventType == (XFixesEventBase + XCB_XFIXES_SELECTION_NOTIFY)) {
            xcb_xfixes_selection_notify_event_t *Sevent = (xcb_xfixes_selection_notify_event_t *)Event;
            xLog1("[Event] Clipboard Owner Changed! OwnerID: %u", Sevent->owner);

            /* * STEP 1: ASK FOR THE MENU
             * Instead of asking for Text or Image directly, we ask for TARGETS
             * to see what formats the owner supports.
             */
            xcb_convert_selection(Connection, 
                                  MyWindow,      
                                  AtomClipboard, 
                                  AtomTarget,    /* Requesting TARGETS */
                                  AtomProperty,  
                                  Sevent->timestamp);
            xcb_flush(Connection);
        }

        /* CASE B: Data or Menu is Ready */
        else if (EventType == XCB_SELECTION_NOTIFY) {
            xcb_selection_notify_event_t *Nevent = (xcb_selection_notify_event_t *)Event;

            /* If property is NONE, the conversion failed or target is not supported */
            if (Nevent->property == XCB_NONE) {
                xWarn2("[Event] Target conversion failed or denied by owner.");
                free(Event);
                continue;
            }

            /* Read the property. 
             * Note: The length parameter is in 4-byte units. 
             * 1048576 units = 4MB, which is safe for most clipboard images.
             */
            xcb_get_property_cookie_t cookie = xcb_get_property(Connection, 1, MyWindow, AtomProperty, XCB_GET_PROPERTY_TYPE_ANY, 0, 1048576);
            xcb_get_property_reply_t *reply = xcb_get_property_reply(Connection, cookie, NULL);

            if (reply && xcb_get_property_value_length(reply) > 0) {
                int ByteLen = xcb_get_property_value_length(reply);
                void *Data = xcb_get_property_value(reply);

                /* * STATE 1: WE RECEIVED THE MENU (TARGETS) */
                if (Nevent->target == AtomTarget) {
                    xLog1("[Target Negotiation] Received format menu. Length: %d bytes", ByteLen);
                    
                    /* The data is an array of xcb_atom_t (32-bit integers) */
                    xcb_atom_t *SupportedAtoms = (xcb_atom_t *)Data;
                    int AtomCount = ByteLen / sizeof(xcb_atom_t);
                    
                    xcb_atom_t BestTarget = XCB_ATOM_NONE;

                    /* Browse the menu to find our preferred format */
                    for (int i = 0; i < AtomCount; i++) {
                        if (SupportedAtoms[i] == AtomPng) {
                            BestTarget = AtomPng;
                            break; /* Highest priority, stop searching */
                        } 
                        else if (SupportedAtoms[i] == AtomJpeg && BestTarget != AtomPng) {
                            BestTarget = AtomJpeg;
                        }
                        else if (SupportedAtoms[i] == AtomUtf8 && BestTarget == XCB_ATOM_NONE) {
                            BestTarget = AtomUtf8;
                        }
                    }

                    /* * STEP 2: ORDER THE ACTUAL DATA */
                    if (BestTarget != XCB_ATOM_NONE) {
                        xLog1("[Target Negotiation] Found matching target: %u. Requesting data...", BestTarget);
                        xcb_convert_selection(Connection, MyWindow, AtomClipboard, 
                                              BestTarget, /* Ask for the specific format */
                                              AtomProperty, Nevent->time);
                        xcb_flush(Connection);
                    } else {
                        xLog1("[Target Negotiation] No supported formats found in this clipboard item.");
                    }
                }
                
                /* * STATE 2: WE RECEIVED THE ACTUAL DATA (UTF8 or PNG/JPEG) */
                else if (Nevent->target == AtomUtf8 || Nevent->target == AtomPng || Nevent->target == AtomJpeg) {
                    char Filename[128];
                    char FullPath[1024];
                    GetTimeBasedFilename(Filename, sizeof(Filename));

                    /* Modify extension based on the target we received */
                    if (Nevent->target == AtomPng) {
                        strcpy(strrchr(Filename, '.'), ".png");
                        xLog1("[Data Ready] Received PNG Image!");
                    } else if (Nevent->target == AtomJpeg) {
                        strcpy(strrchr(Filename, '.'), ".jpg");
                        xLog1("[Data Ready] Received JPEG Image!");
                    } else {
                        xLog1("[Data Ready] Received UTF-8 Text!");
                    }

                    snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, Filename);

                    /* Save to disk */
                    FILE *fp = fopen(FullPath, "w");
                    if (fp) {
                        fwrite(Data, 1, ByteLen, fp);
                        fclose(fp);
                        xLog1("[CoreRuntime] Saved %d bytes to %s", ByteLen, Filename);
                    } else {
                        xError("[CoreRuntime] Failed to write file: %s (errno: %d)", FullPath, errno);
                    }
                }
                /* CASE C: Someone is pasting! They are asking US for data */
                else if ( 0 && EventType == XCB_SELECTION_REQUEST) {
                    xcb_selection_request_event_t *Req = (xcb_selection_request_event_t *)Event;
                    xLog1("[Event] Received SelectionRequest from window: %u", Req->requestor);

                    /// 1. Prepare the reply event framework
                    xcb_selection_notify_event_t Reply = {0};
                    Reply.response_type = XCB_SELECTION_NOTIFY;
                    Reply.requestor = Req->requestor;
                    Reply.selection = Req->selection;
                    Reply.target = Req->target;
                    Reply.time = Req->time;
                    Reply.property = XCB_NONE; /// Default to NONE (Meaning: Request Denied)

                    /// 2. Check what they are asking for. Are they asking for the Menu (TARGETS)?
                    if (Req->target == AtomTarget) {
                        /* We reply that we support TARGETS and whatever format we are currently holding */
                        xcb_atom_t SupportedTargets[] = { AtomTarget, ActiveDataType };
                        
                        xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Req->requestor, Req->property,
                                            XCB_ATOM_ATOM, 32, 2, SupportedTargets);
                        
                        Reply.property = Req->property; /// Mark as Success
                        xLog1("[Provider] Sent TARGETS menu.");
                    }
                    
                    /// 3. Are they asking for the exact data format we have? (e.g., UTF8 or PNG)
                    else if (Req->target == ActiveDataType && ActiveData != NULL) {
                        /* Write our actual data into the requestor's property */
                        xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Req->requestor, Req->property,
                                            ActiveDataType, 8, ActiveDataLen, ActiveData);
                        
                        Reply.property = Req->property; /// Mark as Success
                        xLog1("[Provider] Sent %zu bytes of actual data!", ActiveDataLen);
                    }
                    
                    /// 4. They asked for something we don't have
                    else {
                        xWarn2("[Provider] Request denied. Requested target not supported.");
                    }

                    /// 5. Send the reply back to the requestor so they can read the property
                    xcb_send_event(Connection, 0, Req->requestor, XCB_EVENT_MASK_NO_EVENT, (const char *)&Reply);
                    xcb_flush(Connection);
                }
            }
            if (reply) free(reply);
        }
        free(Event);
    }

    /* Cleanup */
    xcb_disconnect(Connection);
    xExit1("CoreRuntime");
    return OKE;
}

#endif /*__CLIPBOARD_CAPTURE_H__*/
