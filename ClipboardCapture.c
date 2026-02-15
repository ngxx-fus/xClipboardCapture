#include "ClipboardCapture.h"
#include "CBC_Setup.h"
#include "CBC_SysFile.h"
#include <xUniversal.h>

/**************************************************************************************************
 * GLOBAL VARIABLES & ATOMS SECTION ***************************************************************
 **************************************************************************************************/ 

/// @brief Core X11 Atoms used for clipboard communication.
xcb_atom_t              AtomClipboard;
xcb_atom_t              AtomUtf8;
xcb_atom_t              AtomTarget;
xcb_atom_t              AtomPng;
xcb_atom_t              AtomJpeg;
xcb_atom_t              AtomProperty;
xcb_atom_t              AtomTimestamp;
xcb_atom_t              AtomIncr;

/// @brief Global variables to hold the data we want to "paste" to other apps.
void * ActiveData = NULL;
size_t                  ActiveDataLen = 0;
xcb_atom_t              ActiveDataType = 0; /// E.g., AtomUtf8 or AtomPng

/// @brief X11 Connection and Listener Window.
xcb_window_t            MyWindow                = 0;
xcb_connection_t *      Connection              = NULL;

/// @brief Inter-thread signaling flags.
volatile sig_atomic_t   TogglePopUpStatus = eNOT_STARTED;
volatile sig_atomic_t   RequestExit       = eDEACTIVATE;
volatile sig_atomic_t   ReqTestInject     = eDEACTIVATE;

/// @brief Application worker threads.
static pthread_t        SignalRuntimeThread;    /// Thread 1: OS Signal Handling
static pthread_t        XClipboardRuntimeThread; /// Thread 2: X11 Clipboard Logic

#define                 INCR_CHUNK_SIZE 65536

/// @brief INCR Provider (Sender) State Machine Variables.
uint8_t *               IncrData = NULL;
size_t                  IncrDataLen = 0;
size_t                  IncrOffset = 0;
xcb_window_t            IncrRequestor = XCB_NONE;
xcb_atom_t              IncrProperty = XCB_NONE;
xcb_atom_t              IncrTarget = XCB_NONE;

/// @brief INCR Receiver State Machine Variables.
int                     IsReceivingIncr = 0;
FILE *                  IncrRecvFile    = NULL;
char                    IncrRecvFilename[NAME_MAX];

/**************************************************************************************************
 * X11 SERVER SETUP SECTION ***********************************************************************
 **************************************************************************************************/ 

/// @brief Interns a single X11 atom by its string name.
/// @param c The XCB connection.
/// @param name The string name of the atom.
/// @return The XCB atom ID.
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

/// @brief Initializes all global atoms used by the application.
/// @param c The XCB connection.
void InitAtoms(xcb_connection_t *c) {
    xEntry1("InitAtoms");
    AtomClipboard = GetAtomByName(c, "CLIPBOARD");
    AtomUtf8      = GetAtomByName(c, "UTF8_STRING");
    AtomTarget    = GetAtomByName(c, "TARGETS");
    AtomPng       = GetAtomByName(c, "image/png");
    AtomJpeg      = GetAtomByName(c, "image/jpeg");
    AtomProperty  = GetAtomByName(c, PROP_NAME);
    AtomTimestamp = GetAtomByName(c, "TIMESTAMP"); 
    AtomIncr      = GetAtomByName(c, "INCR");
    xExit1("InitAtoms");
}

/// @brief Creates a hidden dummy window to receive XFixes events.
/// @param c The XCB connection.
/// @return The generated window ID.
xcb_window_t CreateListenerWindow(xcb_connection_t *c) {
    xEntry1("CreateListenerWindow");
    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    xcb_window_t win = xcb_generate_id(c);
    
    /// Create an unmapped (invisible) window
    xcb_create_window(c, XCB_COPY_FROM_PARENT, win, screen->root,
                      0, 0, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      0, NULL);

    uint32_t event_mask[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
    xcb_change_window_attributes(c, win, XCB_CW_EVENT_MASK, event_mask);

    xExit1("CreateListenerWindow: %u", win);
    return win;
}

/// @brief Requests the XFixes extension to notify us of clipboard ownership changes.
/// @param c The XCB connection.
/// @param window The dummy window to receive events.
void SubscribeClipboardEvents(xcb_connection_t *c, xcb_window_t window) {
    xEntry1("SubscribeClipboardEvents");
    
    /// Query XFixes version to ensure it is supported
    xcb_xfixes_query_version_cookie_t ck = xcb_xfixes_query_version(c, XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION);
    xcb_xfixes_query_version_reply_t *r = xcb_xfixes_query_version_reply(c, ck, NULL);
    if(r) free(r);

    /// Select selection events for the CLIPBOARD atom
    uint32_t mask = XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER |
                    XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY |
                    XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE;

    xcb_xfixes_select_selection_input(c, window, AtomClipboard, mask);
    xcb_flush(c);
    xExit1("SubscribeClipboardEvents: Done");
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
    
    /// Store the data locally (Free previous data if it exists to avoid memory leaks)
    if (ActiveData) {
        free(ActiveData);
    }
    ActiveData = malloc(len);
    if (!ActiveData) {
        xError("[SetClipboardData] Out of memory!");
        return;
    }
    memcpy(ActiveData, data, len);
    ActiveDataLen = len;
    ActiveDataType = type;

    /// Tell X Server that we are the new owner of the CLIPBOARD
    xcb_set_selection_owner(c, win, AtomClipboard, XCB_CURRENT_TIME);
    
    /// Verify if we successfully became the owner
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

/// @brief Asynchronous handler called directly by the Linux Kernel when a signal arrives.
/// @param SigNum The ID of the received POSIX signal (e.g., SIGINT, SIGUSR1).
void SignalEventHandler(int SigNum) {
    xLog1("[SignalEventHandler] Was called with SigNum=%d", SigNum);

    if (SigNum == SIGINT || SigNum == SIGTERM) {
        /// User pressed Ctrl+C or sent a termination kill command
        RequestExit = eACTIVATE;
        xLog1("[SignalEventHandler] Activate RequestExit!");
    } 
    else if (SigNum == SIGUSR1) {
        /// Custom signal received from another terminal (e.g., pkill -SIGUSR1)
        xLog1("[SignalEventHandler] Injecting selected item into X11 Clipboard...");
        ReqTestInject = eACTIVATE;
        
        /// Toggle PopUp visibility state
        if (TogglePopUpStatus == eHIDEN || TogglePopUpStatus == eNOT_STARTED) {
            TogglePopUpStatus = eREQ_SHOW;
        } 
        else if (TogglePopUpStatus == eSHOWN) {
            TogglePopUpStatus = eREQ_HIDE;
        }
    }
}

/// @brief Registers the application's signal handlers with the Operating System.
/// @return OKE on success.
RetType RegisterSignal(void) {
    xEntry1("RegisterSignal");

    signal(SIGUSR1, SignalEventHandler); /// Catch custom command
    signal(SIGINT,  SignalEventHandler); /// Catch Ctrl+C
    signal(SIGTERM, SignalEventHandler); /// Catch standard kill command

    xLog1("[RegisterSignal] Listening for OS signals... (PID: %d)", getpid());

    xExit1("RegisterSignal");
    return OKE; 
}

/// @brief The main loop for the Signal Thread. It idles and waits for OS signals.
/// @param Param Optional parameter passed when the thread starts.
/// @return OKE when the thread exits cleanly.
RetType SignalRuntime(int Param) {
    xEntry1("SignalRuntime");

    RegisterSignal();

    /// Infinite loop to keep this thread alive, consuming 0% CPU via pause()
    while (1) {
        pause(); 

        if (RequestExit != 0) {
            xLog1("[SignalRuntime] Exit flag detected. Stopping signal thread...");
            break;
        }

        if (TogglePopUpStatus == eREQ_SHOW) {
            xLog1("[SignalRuntime] Action required: SHOW PopUp!");
        } 
        else if (TogglePopUpStatus == eREQ_HIDE) {
            xLog1("[SignalRuntime] Action required: HIDE PopUp!");
        }
    }

    xExit1("SignalRuntime");
    return OKE;
}

/**************************************************************************************************
 * CLIPBOARD EVENT HANDLERS IMPLEMENTATION ********************************************************
 **************************************************************************************************/

/// @brief Handles XFixes Selection Notify events (Triggered when someone else copies data).
void HandleXFixesNotify(xcb_generic_event_t *Event) {
    xcb_xfixes_selection_notify_event_t *Sevent = (xcb_xfixes_selection_notify_event_t *)Event;
    
    /// Avoid processing events triggered by our own application
    if (Sevent->owner == MyWindow) {
        return;
    }

    xLog1("[XClipboardRuntime] [Event] Clipboard Owner Changed! OwnerID: %u", Sevent->owner);

    /// Request the TARGETS atom to see what formats the new owner supports
    xcb_convert_selection(Connection, MyWindow, AtomClipboard, AtomTarget, AtomProperty, Sevent->timestamp);
    xcb_flush(Connection);
}

/// @brief Helper function to handle TARGETS negotiation response.
static inline void HandleSelectionNotify_Negotiate(xcb_selection_notify_event_t *Nevent, void *Data, int ByteLen) {
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


    xcb_delete_property(Connection, MyWindow, AtomProperty);
    xcb_flush(Connection);
}

/// @brief Helper function to handle actual data receiving (Single-shot or INCR setup).
static inline void HandleSelectionNotify_ReceiveAndSave(xcb_selection_notify_event_t *Nevent, xcb_get_property_reply_t *reply, void *Data, int ByteLen) {
    const char *Extension = "txt"; 
    if (Nevent->target == AtomPng) Extension = "png";
    else if (Nevent->target == AtomJpeg) Extension = "jpg";

    /// 1. Handle Large Files via INCR Protocol
    if (reply->type == AtomIncr) {
        xLog1("[XClipboardRuntime] Sender initiated INCR Protocol. Preparing to receive chunks...");
        
        GetTimeBasedFilename(IncrRecvFilename, sizeof(IncrRecvFilename), Extension);
        char FullPath[PATH_MAX];
        snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, IncrRecvFilename);
        
        /// Open file in append/binary mode
        IncrRecvFile = fopen(FullPath, "wb");
        if (IncrRecvFile) {
            IsReceivingIncr = 1;
            /// Delete property to signal readiness for the first chunk
            xcb_delete_property(Connection, MyWindow, AtomProperty);
            xcb_flush(Connection);
        } else {
            xError("[XRuntime] Failed to open INCR receive file.");
        }
    } 
    /// 2. Handle Small Files via Single-shot Transfer
    else {
        char Filename[NAME_MAX], FullPath[PATH_MAX];
        GetTimeBasedFilename(Filename, sizeof(Filename), Extension);
        
        XCBList_PushItem(Filename);

        snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, Filename);
        FILE *fp = fopen(FullPath, "wb"); 
        if (fp) {
            fwrite(Data, 1, ByteLen, fp);
            fclose(fp);
            xLog1("[XClipboardRuntime] Saved %d bytes to %s", ByteLen, Filename);
        } else {
            xError("[XClipboardRuntime] Failed to open file for writing: %s", FullPath);
        }
    }
}

/// @brief Handles Selection Notify events (Triggered when requested data arrives).
void HandleSelectionNotify(xcb_generic_event_t *Event) {
    xcb_selection_notify_event_t *Nevent = (xcb_selection_notify_event_t *)Event;

    if (Nevent->property == XCB_NONE) {
        xWarn2("[XClipboardRuntime] [Event] Target conversion failed or denied by owner.");
        return;
    }

    /// Read up to 32MB to prevent truncation of non-INCR files
    xcb_get_property_cookie_t cookie = xcb_get_property(Connection, 0, MyWindow, AtomProperty, XCB_GET_PROPERTY_TYPE_ANY, 0, 8388608);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(Connection, cookie, NULL);

    if (reply && xcb_get_property_value_length(reply) > 0) {
        int ByteLen = xcb_get_property_value_length(reply);
        void *Data = xcb_get_property_value(reply);

        if (Nevent->target == AtomTarget) {
            HandleSelectionNotify_Negotiate(Nevent, Data, ByteLen);
        }
        else if (Nevent->target == AtomUtf8 || Nevent->target == AtomPng || Nevent->target == AtomJpeg) {
            HandleSelectionNotify_ReceiveAndSave(Nevent, reply, Data, ByteLen);
        }
    }
    
    if (reply) free(reply);
}

/// @brief Handles Property Notify events for managing both INCR Receiver and Provider flows.
void HandlePropertyNotify(xcb_generic_event_t *Event) {
    xcb_property_notify_event_t *PropEv = (xcb_property_notify_event_t *)Event;
    
    /// [RECEIVER MODE] Processing incoming data chunks
    if (IsReceivingIncr && PropEv->window == MyWindow && 
        PropEv->atom == AtomProperty && PropEv->state == XCB_PROPERTY_NEW_VALUE) {
        
        xcb_get_property_cookie_t ck = xcb_get_property(Connection, 0, MyWindow, AtomProperty, XCB_GET_PROPERTY_TYPE_ANY, 0, 8388608);
        xcb_get_property_reply_t *r = xcb_get_property_reply(Connection, ck, NULL);
        
        if (r) {
            int ChunkLen = xcb_get_property_value_length(r);
            if (ChunkLen > 0) {
                void *ChunkData = xcb_get_property_value(r);
                if (IncrRecvFile) fwrite(ChunkData, 1, ChunkLen, IncrRecvFile);
                
                /// Delete property to trigger the next chunk
                xcb_delete_property(Connection, MyWindow, AtomProperty);
                xcb_flush(Connection);
            } else {
                xLog1("[INCR RECV] Transfer Complete! Saving item to DB.");
                if (IncrRecvFile) {
                    fclose(IncrRecvFile);
                    IncrRecvFile = NULL;
                }
                IsReceivingIncr = 0;
                XCBList_PushItem(IncrRecvFilename);
            }
            free(r);
        }
        return; 
    }

    /// [PROVIDER MODE] Pumping data chunks out
    if (PropEv->state == XCB_PROPERTY_DELETE && 
        PropEv->window == IncrRequestor && 
        PropEv->atom == IncrProperty) {
        
        size_t BytesLeft = IncrDataLen - IncrOffset;
        
        if (BytesLeft > 0) {
            size_t ChunkSize = (BytesLeft > INCR_CHUNK_SIZE) ? INCR_CHUNK_SIZE : BytesLeft;
            
            xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, IncrRequestor, 
                                IncrProperty, IncrTarget, 8, ChunkSize, 
                                (uint8_t *)IncrData + IncrOffset);
            IncrOffset += ChunkSize;
        } else {
            uint8_t EOF_Dummy = 0;
            xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, IncrRequestor, 
                                IncrProperty, IncrTarget, 8, 0, &EOF_Dummy);
            xLog1("[INCR] Transfer Complete!");
            
            IncrRequestor = XCB_NONE;
            IncrProperty = XCB_NONE;
            IncrTarget = XCB_NONE;
        }
        xcb_flush(Connection);
    }
}

/// @brief Handles Selection Request events, providing clipboard data to other apps.
void HandleSelectionRequest(xcb_generic_event_t *Event) {
    xEntry1("HandleSelectionRequest");
    xcb_selection_request_event_t *Req = (xcb_selection_request_event_t *)Event;
    
    xLog1("[XClipboardRuntime] [Event] SelectionRequest from window: %u for target: %u", 
          Req->requestor, Req->target);

    xcb_selection_notify_event_t Reply;
    memset(&Reply, 0, sizeof(Reply));
    Reply.response_type = XCB_SELECTION_NOTIFY;
    Reply.requestor     = Req->requestor;
    Reply.selection     = Req->selection;
    Reply.target        = Req->target;
    Reply.time          = Req->time;
    
    xcb_atom_t ValidProperty = (Req->property == XCB_NONE) ? Req->target : Req->property;
    Reply.property = XCB_NONE; 

    if (Req->target == AtomTarget) { 
        xcb_atom_t SupportedTargets[] = { AtomTarget, AtomTimestamp, ActiveDataType };
        int NumTargets = sizeof(SupportedTargets) / sizeof(xcb_atom_t);
        
        xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Req->requestor, 
                            ValidProperty, XCB_ATOM_ATOM, 32, NumTargets, SupportedTargets);
        Reply.property = ValidProperty;
    }
    else if (Req->target == AtomTimestamp) {
        xcb_timestamp_t CurrentTime = Req->time; 
        xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Req->requestor, 
                            ValidProperty, XCB_ATOM_INTEGER, 32, 1, &CurrentTime);
        Reply.property = ValidProperty;
    }
    else if (Req->target == ActiveDataType && ActiveData != NULL) {
        if (ActiveDataLen > INCR_CHUNK_SIZE) {
            
            if (IncrRequestor != XCB_NONE && IncrRequestor != Req->requestor) {
                xWarn2("[INCR] Busy! Rejecting concurrent request.");
                Reply.property = XCB_NONE;
            } else {
                xLog1("[XRuntime] Data > 64KB. Starting INCR Protocol...");

                IncrData = ActiveData;
                IncrDataLen = ActiveDataLen;
                IncrOffset = 0;
                IncrRequestor = Req->requestor;
                IncrProperty = ValidProperty;
                IncrTarget = ActiveDataType;

                uint32_t event_mask[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
                xcb_change_window_attributes(Connection, Req->requestor, XCB_CW_EVENT_MASK, event_mask);

                uint32_t TotalSize = ActiveDataLen;
                xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Req->requestor, 
                                    ValidProperty, AtomIncr, 32, 1, &TotalSize);
                Reply.property = ValidProperty;
            }
        } 
        else {
            xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Req->requestor, 
                                ValidProperty, ActiveDataType, 8, ActiveDataLen, ActiveData);
            Reply.property = ValidProperty;
        }
    }

    xcb_send_event(Connection, 0, Req->requestor, XCB_EVENT_MASK_NO_EVENT, (const char *)&Reply);
    xcb_flush(Connection);
    
    xExit1("HandleSelectionRequest");
}

/**************************************************************************************************
 * THREAD RUNTIME SECTION *************************************************************************
 **************************************************************************************************/

/// @brief The main loop that manages X11 connections and dispatches events.
/// @param Param Optional thread parameter.
/// @return OKE when the thread exits cleanly.
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
        
        /// Handle manual injection from UI
        if (ReqTestInject == eACTIVATE) {
            ReqTestInject = eDEACTIVATE;
            sClipboardItem LatestItem;
            
            /// 1. Get the currently selected item (using the new UI logic)
            if (XCBList_GetSelectedItem(&LatestItem) == OKE) {
                
                xcb_atom_t TargetAtom = AtomUtf8; 
                if (LatestItem.FileType == eFMT_IMG_PNG) TargetAtom = AtomPng;
                else if (LatestItem.FileType == eFMT_IMG_JGP) TargetAtom = AtomJpeg;

                static uint8_t RawData[8U * 1024U * 1024U]; 
                
                char FullPath[PATH_MAX];
                snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, LatestItem.Filename);
                struct stat FileStat;
                
                if (stat(FullPath, &FileStat) == 0 && FileStat.st_size > 0 && FileStat.st_size <= sizeof(RawData)) {
                    /// 2. Read binary using the selected index
                    int SelectedIdx = XCBList_GetSelectedNum();
                    if (XCBList_ReadAsBinary(SelectedIdx, RawData, sizeof(RawData)) >= 0) {
                        SetClipboardData(Connection, MyWindow, (void *)RawData, FileStat.st_size, TargetAtom);
                        xLog1("[XClipboardRuntime] Injected %s (%ld bytes) as Atom %u", 
                              LatestItem.Filename, FileStat.st_size, TargetAtom);
                    } else {
                        xWarn2("[XClipboardRuntime] ReadAsBinary failed!");
                    }
                } else {
                    xWarn2("[XClipboardRuntime] File missing, empty, or exceeds 8MB buffer!");
                }
            } else {
                xWarn2("[XClipboardRuntime] No item selected or DB is empty!");
            }
        }

        Event = xcb_poll_for_event(Connection);
        if (Event == NULL) {
            if (xcb_connection_has_error(Connection)) break;
            usleep(2U * 1000U); /// Fast polling (2ms) to maintain INCR protocol speed
            continue;
        }

        uint8_t EventType = Event->response_type & ~0x80;

        if (EventType == (XFixesEventBase + XCB_XFIXES_SELECTION_NOTIFY)) {
            HandleXFixesNotify(Event);
        }
        else if (EventType == XCB_SELECTION_NOTIFY) {
            HandleSelectionNotify(Event);
        }
        else if (EventType == XCB_SELECTION_REQUEST) {
            HandleSelectionRequest(Event);
        }
        else if (EventType == XCB_PROPERTY_NOTIFY) {
            HandlePropertyNotify(Event);
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

    int SigKillStatus = pthread_kill(SignalRuntimeThread, SIGINT);
    if (SigKillStatus == 0) {
        pthread_join(SignalRuntimeThread, NULL);
        xLog1("[Finalize] Signal Thread joined.");
    }

    pthread_join(XClipboardRuntimeThread, NULL);
    xLog1("[Finalize] XClipboard Thread joined.");

    xLog1("[Finalize] Application exited gracefully.");
    xExit1("ClipboardCaptureFinalize");
}

/// @brief Initializes the system and launches worker threads.
/// @return OKE on success, ERR on failure.
RetType ClipboardCaptureInitialize(void) {
    xEntry1("ClipboardCaptureInitialize");
    
    atexit(ClipboardCaptureFinalize);

    if (EnsureDB() != OKE) {
        xError("[Initialize] System Check Failed!");
        return ERR; 
    }

    /// Safely check scan results
    if(XCBList_Scan(0) < 0){
        xError("[Initialize] Scan DB failed!");
    }

    if (pthread_create(&SignalRuntimeThread, NULL, (void *(*)(void *))SignalRuntime, NULL) != 0) {
        xError("[Initialize] Failed to create Signal Thread!");
        return ERR;
    }

    if (pthread_create(&XClipboardRuntimeThread, NULL, (void *(*)(void *))XClipboardRuntime, NULL) != 0) {
        xError("[Initialize] Failed to create XClipboard Thread!");
        return ERR;
    }

    xLog1("[Initialize] All systems started. Main thread is now free.");
    xExit1("ClipboardCaptureInitialize");
    return OKE;
}
