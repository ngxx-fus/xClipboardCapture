#include "ClipboardCapture.h"
#include "CBC_Setup.h"
#include <xUniversal.h>



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

/// @brief Global variables to hold the data we want to "paste" to other apps.
void *ActiveData = NULL;
size_t ActiveDataLen = 0;
xcb_atom_t ActiveDataType = 0; /// E.g., AtomUtf8 or AtomPng

xcb_window_t MyWindow                   = 0;
xcb_connection_t *Connection            = NULL;


volatile sig_atomic_t TogglePopUpStatus = eNOT_STARTED;
volatile sig_atomic_t RequestExit       = eDEACTIVATE;
volatile sig_atomic_t ReqTestInject     = eDEACTIVATE;

static pthread_t SignalRuntimeThread;    /// Thread 1: OS Signal Handling
static pthread_t XClipboardRuntimeThread; /// Thread 2: X11 Clipboard Logic

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
        RequestExit = eACTIVATE;
        xLog1("[SignalEventHandler] Activate RequestExit!");
    } 
    else if (SigNum == SIGUSR1) {
        /* Custom signal received from another terminal (pkill -SIGUSR1 TestProg) */
        const char *TestStr = "Hello from Phu's Clipboard! Khai code thành công :>";
        xLog1("[SignalEventHandler] Injecting string (`%s`) into X11 Clipboard...", TestStr);
            
        /* Gọi hàm SetClipboardData mình đã viết ở phần trước */
        /// SetClipboardData(Connection, MyWindow, (void *)TestStr, strlen(TestStr), AtomUtf8);
        ReqTestInject = eACTIVATE;
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
    while (1) {
        
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


/**************************************************************************************************
 * CLIPBOARD EVENT HANDLERS IMPLEMENTATION ********************************************************
 **************************************************************************************************/

/**
 * @brief Handles XFixes Selection Notify events.
 */
void HandleXFixesNotify(xcb_generic_event_t *Event) {
    xcb_xfixes_selection_notify_event_t *Sevent = (xcb_xfixes_selection_notify_event_t *)Event;
    
    /* Avoid processing events triggered by our own application */
    if (Sevent->owner == MyWindow) {
        return;
    }

    xLog1("[XClipboardRuntime] [Event] Clipboard Owner Changed! OwnerID: %u", Sevent->owner);

    /* Request the TARGETS atom to see what formats are available */
    xcb_convert_selection(Connection, MyWindow, AtomClipboard, AtomTarget, AtomProperty, Sevent->timestamp);
    xcb_flush(Connection);
}

/**
 * @brief Handles Selection Notify events and processes the received data.
 */
void HandleSelectionNotify(xcb_generic_event_t *Event) {
    xcb_selection_notify_event_t *Nevent = (xcb_selection_notify_event_t *)Event;

    if (Nevent->property == XCB_NONE) {
        xWarn2("[XClipboardRuntime] [Event] Target conversion failed or denied by owner.");
        return;
    }

    /* Request the actual property value from our window */
    xcb_get_property_cookie_t cookie = xcb_get_property(Connection, 1, MyWindow, AtomProperty, XCB_GET_PROPERTY_TYPE_ANY, 0, 1048576);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(Connection, cookie, NULL);

    if (reply && xcb_get_property_value_length(reply) > 0) {
        int ByteLen = xcb_get_property_value_length(reply);
        void *Data = xcb_get_property_value(reply);

        /* Case 1: Negotiating formats (TARGETS) */
        if (Nevent->target == AtomTarget) {
            xLog1("[XClipboardRuntime] [Target Negotiation] Received format menu. Length: %d bytes", ByteLen);
            xcb_atom_t *SupportedAtoms = (xcb_atom_t *)Data;
            int AtomCount = ByteLen / sizeof(xcb_atom_t);
            xcb_atom_t BestTarget = XCB_ATOM_NONE;

            for (int i = 0; i < AtomCount; i++) {
                if (SupportedAtoms[i] == AtomPng) { BestTarget = AtomPng; break; }
                else if (SupportedAtoms[i] == AtomJpeg && BestTarget != AtomPng) { BestTarget = AtomJpeg; }
                else if (SupportedAtoms[i] == AtomUtf8 && BestTarget == XCB_ATOM_NONE) { BestTarget = AtomUtf8; }
            }

            if (BestTarget != XCB_ATOM_NONE) {
                xLog1("[XClipboardRuntime] [Target Negotiation] Found matching target: %u. Requesting data...", BestTarget);
                xcb_convert_selection(Connection, MyWindow, AtomClipboard, BestTarget, AtomProperty, Nevent->time);
                xcb_flush(Connection);
            }
        }
        /* Case 2: Saving received data (UTF8/PNG/JPG) */
        else if (Nevent->target == AtomUtf8 || Nevent->target == AtomPng || Nevent->target == AtomJpeg) {
            char Filename[128], FullPath[1024];
            GetTimeBasedFilename(Filename, sizeof(Filename));

            if (Nevent->target == AtomPng) strcpy(strrchr(Filename, '.'), ".png");
            else if (Nevent->target == AtomJpeg) strcpy(strrchr(Filename, '.'), ".jpg");

            snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, Filename);
            FILE *fp = fopen(FullPath, "w");
            if (fp) {
                fwrite(Data, 1, ByteLen, fp);
                fclose(fp);
                xLog1("[XClipboardRuntime] Saved %d bytes to %s", ByteLen, Filename);
            }
        }
    }
    if (reply) free(reply);
}

/**
 * @brief Handles Selection Request events (Providing data to other apps).
 */
void HandleSelectionRequest(xcb_generic_event_t *Event) {
    xcb_selection_request_event_t *Req = (xcb_selection_request_event_t *)Event;
    xLog1("[XClipboardRuntime] [Event] Received SelectionRequest from window: %u", Req->requestor);

    xcb_selection_notify_event_t Reply = {0};
    memset(&Reply, 0, sizeof(Reply));
    Reply.response_type = XCB_SELECTION_NOTIFY;
    Reply.requestor = Req->requestor;
    Reply.selection = Req->selection;
    Reply.target = Req->target;
    Reply.time = Req->time;
    Reply.property = XCB_NONE;

    if (Req->target == AtomTarget) {
        xcb_atom_t SupportedTargets[] = { AtomTarget, ActiveDataType };
        xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Req->requestor, Req->property, XCB_ATOM_ATOM, 32, 2, SupportedTargets);
        Reply.property = Req->property;
    }
    else if (Req->target == ActiveDataType && ActiveData != NULL) {
        xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Req->requestor, Req->property, ActiveDataType, 8, ActiveDataLen, ActiveData);
        Reply.property = Req->property;
    }

    xcb_send_event(Connection, 0, Req->requestor, XCB_EVENT_MASK_NO_EVENT, (const char *)&Reply);
    xcb_flush(Connection);
}

/**
 * @brief The main loop that manages X11 connections and dispatches events.
 */
RetType XClipboardRuntime(int Param) {
    xEntry1("XClipboardRuntime");

    Connection = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(Connection)) return ERR;

    const xcb_query_extension_reply_t *xfixes_data = xcb_get_extension_data(Connection, &xcb_xfixes_id);
    if (!xfixes_data || !xfixes_data->present) return ERR;
    uint8_t XFixesEventBase = xfixes_data->first_event;

    InitAtoms(Connection);
    MyWindow = CreateListenerWindow(Connection);
    SubscribeClipboardEvents(Connection, MyWindow);

    xLog1("[XClipboardRuntime] Listening for Clipboard events...");
    xcb_generic_event_t *Event;

    while (RequestExit != eACTIVATE) {
        /* Check for manual injection first */
        if (ReqTestInject == 1) {
            ReqTestInject = 0;
            const char *TestStr = "Hello from Phu's Clipboard! Khai code thành công :>";
            SetClipboardData(Connection, MyWindow, (void *)TestStr, strlen(TestStr), AtomUtf8);
        }

        Event = xcb_poll_for_event(Connection);
        if (Event == NULL) {
            if (xcb_connection_has_error(Connection)) break;
            usleep(100U /*ms*/ * 1000U /*us*/);
            continue;
        }

        uint8_t EventType = Event->response_type & ~0x80;

        /* Dispatch events to their respective handlers */
        if (EventType == (XFixesEventBase + XCB_XFIXES_SELECTION_NOTIFY)) {
            HandleXFixesNotify(Event);
        }
        else if (EventType == XCB_SELECTION_NOTIFY) {
            HandleSelectionNotify(Event);
        }
        else if (EventType == XCB_SELECTION_REQUEST) {
            HandleSelectionRequest(Event);
        }

        free(Event);
    }

    xcb_disconnect(Connection);
    xExit1("XClipboardRuntime");
    return OKE;
}

/**************************************************************************************************
 * LIFECYCLE SECTION IMPLEMENTATION ***************************************************************
 **************************************************************************************************/ 

/// @brief Cleans up resources and waits for all background threads to exit safely.
void ClipboardCaptureFinalize(void) {
    xLog1("[Finalize] Initiating shutdown sequence...");

    /// 1. Wake up and join the Signal Thread
    int SigKillStatus = pthread_kill(SignalRuntimeThread, SIGINT);
    if (SigKillStatus == 0) {
        pthread_join(SignalRuntimeThread, NULL);
        xLog1("[Finalize] Signal Thread joined.");
    }

    /// 2. Join the XClipboard Thread
    /// Note: XClipboardRuntime will exit its while-loop because RequestExit is now eACTIVATE
    pthread_join(XClipboardRuntimeThread, NULL);
    xLog1("[Finalize] XClipboard Thread joined.");

    xLog1("[Finalize] Application exited gracefully.");
    xExit1("ClipboardCaptureFinalize");
}

/**
 * @brief Initializes the system and launches worker threads.
 * @return OKE on success, ERR on failure.
 */
RetType ClipboardCaptureInitialize(void) {
    xEntry1("ClipboardCaptureInitialize");
    
    /* 1. Register the Finalize function for automatic cleanup */
    atexit(ClipboardCaptureFinalize);

    /* 2. Prepare Directories */
    if (EnsureDB() != OKE) {
        xError("[Initialize] System Check Failed!");
        return ERR; 
    }

    /* 3. Create Thread 1: Signal Runtime */
    if (pthread_create(&SignalRuntimeThread, NULL, (void *(*)(void *))SignalRuntime, NULL) != 0) {
        xError("[Initialize] Failed to create Signal Thread!");
        return ERR;
    }

    /* 4. Create Thread 2: XClipboard Runtime */
    /* This thread will now run XClipboardRuntime in the background */
    if (pthread_create(&XClipboardRuntimeThread, NULL, (void *(*)(void *))XClipboardRuntime, NULL) != 0) {
        xError("[Initialize] Failed to create XClipboard Thread!");
        return ERR;
    }

    xLog1("[Initialize] All systems started. Main thread is now free.");
    xExit1("ClipboardCaptureInitialize");
    return OKE;
}
