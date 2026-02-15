#include "ClipboardCapture.h"
#include "CBC_Setup.h"
#include "CBC_SysFile.h"
#include "xUniversal.h"

/**************************************************************************************************
 * X11 ATOMS SECTION ******************************************************************************
 **************************************************************************************************/ 

/**
 * @brief Atom representing the X11 CLIPBOARD selection.
 */
xcb_atom_t AtomClipboard;

/**
 * @brief Atom representing UTF8_STRING text format.
 */
xcb_atom_t AtomUtf8;

/**
 * @brief Atom used to request supported data formats (TARGETS negotiation).
 */
xcb_atom_t AtomTarget;

/**
 * @brief Atom representing PNG image MIME type ("image/png").
 */
xcb_atom_t AtomPng;

/**
 * @brief Atom representing JPEG image MIME type ("image/jpeg").
 */
xcb_atom_t AtomJpeg;

/**
 * @brief Custom property Atom used as a temporary buffer for selection transfers.
 */
xcb_atom_t AtomProperty;

/**
 * @brief Atom representing TIMESTAMP target for clipboard ownership time.
 */
xcb_atom_t AtomTimestamp;

/**
 * @brief Atom indicating usage of the INCR protocol for large transfers.
 */
xcb_atom_t AtomIncr;

/**************************************************************************************************
 * ACTIVE CLIPBOARD DATA SECTION ******************************************************************
 **************************************************************************************************/ 

/**
 * @brief Pointer to the active data (text/image) currently held in the clipboard.
 */
void *ActiveData = NULL;

/**
 * @brief Size of the active clipboard data in bytes.
 */
size_t ActiveDataLen = 0;

/**
 * @brief The X11 Atom representing the format of the active data (e.g., AtomUtf8 or AtomPng).
 */
xcb_atom_t ActiveDataType = 0;

/**************************************************************************************************
 * X11 CORE & CONNECTION SECTION ******************************************************************
 **************************************************************************************************/ 

/**
 * @brief Hidden X11 window used to listen for clipboard events.
 */
xcb_window_t MyWindow = 0;

/**
 * @brief Pointer to the active XCB connection.
 */
xcb_connection_t *Connection = NULL;

/**************************************************************************************************
 * THREADS & SIGNALS SECTION **********************************************************************
 **************************************************************************************************/ 

/**
 * @brief Flag to trigger the UI popup menu (eREQ_SHOW, eREQ_HIDE, or eNOT_STARTED).
 * @note This variable is modified asynchronously by the OS signal handler.
 */
volatile sig_atomic_t TogglePopUpStatus = eNOT_STARTED;

/**
 * @brief Flag to signal all threads to gracefully exit.
 * @note Set to eACTIVATE upon receiving SIGINT or SIGTERM.
 */
volatile sig_atomic_t RequestExit = eDEACTIVATE;

/**
 * @brief Flag to trigger the injection of the selected clipboard item into the active window.
 */
volatile sig_atomic_t ReqTestInject = eDEACTIVATE;

/**
 * @brief Thread handle for the OS signal listener (SIGUSR1, SIGINT).
 */
static pthread_t SignalRuntimeThread;

/**
 * @brief Thread handle for the main X11 event loop and clipboard logic.
 */
static pthread_t XClipboardRuntimeThread;

/**************************************************************************************************
 * INCR PROTOCOL (PROVIDER) SECTION ***************************************************************
 **************************************************************************************************/ 

/**
 * @brief Maximum chunk size (64KB) for INCR protocol transfers.
 */
#define INCR_CHUNK_SIZE 65536

/**
 * @brief Pointer to the data currently being transmitted via INCR protocol.
 */
uint8_t *IncrData = NULL;

/**
 * @brief Total size of the data being transmitted via INCR protocol.
 */
size_t IncrDataLen = 0;

/**
 * @brief Current byte offset of the INCR transmission.
 */
size_t IncrOffset = 0;

/**
 * @brief The window ID of the application requesting the INCR transfer.
 */
xcb_window_t IncrRequestor = XCB_NONE;

/**
 * @brief The property atom used for the current INCR transfer.
 */
xcb_atom_t IncrProperty = XCB_NONE;

/**
 * @brief The target format atom of the current INCR transfer.
 */
xcb_atom_t IncrTarget = XCB_NONE;

/**************************************************************************************************
 * INCR PROTOCOL (RECEIVER) SECTION ***************************************************************
 **************************************************************************************************/ 

/**
 * @brief Flag indicating if the application is currently receiving an INCR transfer.
 */
int IsReceivingIncr = 0;

/**
 * @brief File pointer used to append incoming INCR chunks to disk.
 */
FILE *IncrRecvFile = NULL;

/**
 * @brief Filename of the current INCR transfer being received.
 */
char IncrRecvFilename[NAME_MAX];

/**************************************************************************************************
 * X11 SERVER SETUP SECTION ***********************************************************************
 **************************************************************************************************/ 

/**
 * @brief Interns a single X11 atom by its string name.
 * @param c The XCB connection.
 * @param name The string name of the atom.
 * @return The XCB atom ID, or XCB_ATOM_NONE on failure.
 */
xcb_atom_t GetAtomByName(xcb_connection_t *c, const char *name) {
    xEntry1("GetAtomByName(name=%s)", name);
    
    /// xcb_intern_atom sends a request to the X Server to get or create a unique integer ID 
    /// for the given string. This is crucial because X11 components communicate using these IDs 
    /// (Atoms) instead of passing long strings around to save bandwidth.
    xcb_intern_atom_cookie_t ck = xcb_intern_atom(c, 0, strlen(name), name);
    xcb_intern_atom_reply_t *r = xcb_intern_atom_reply(c, ck, NULL);
    
    if (!r) {
        xExit1("GetAtomByName(): XCB_ATOM_NONE, !r");
        return XCB_ATOM_NONE;
    }
    
    xcb_atom_t a = r->atom;
    free(r);
    xExit1("GetAtomByName(): %u", a);
    return a;
}

/**
 * @brief Ensures that only one instance of xClipBoardCapture is running.
 * @return OKE if this is the first instance, ERR otherwise.
 */
RetType CheckSingleInstance(xcb_connection_t *c, xcb_window_t win) {
    /// 1. Intern a unique Atom name for our application lock
    xcb_atom_t LockAtom = GetAtomByName(c, "CLIPBOARD_CAPTURE_SINGLE_INSTANCE_LOCK");

    /// 2. Query the X Server: "Who is the current owner of this lock?"
    xcb_get_selection_owner_cookie_t cookie = xcb_get_selection_owner(c, LockAtom);
    xcb_get_selection_owner_reply_t *reply = xcb_get_selection_owner_reply(c, cookie, NULL);

    if (reply && reply->owner != XCB_NONE) {
        /// If the owner is NOT NONE, it means another process (App 1) is already holding it.
        free(reply);
        return ERR; 
    }
    if (reply) free(reply);

    /// 3. If we reached here, no one owns it. Now, we claim it!
    xcb_set_selection_owner(c, win, LockAtom, XCB_CURRENT_TIME);
    
    /// Double check to be sure we are the real owner
    cookie = xcb_get_selection_owner(c, LockAtom);
    reply = xcb_get_selection_owner_reply(c, cookie, NULL);
    if (reply && reply->owner == win) {
        free(reply);
        return OKE;
    }

    if (reply) free(reply);
    return ERR;
}

/**
 * @brief Initializes all global atoms used by the application.
 * @param c The XCB connection.
 * @note Must be called before initiating any clipboard transactions.
 */
void InitAtoms(xcb_connection_t *c) {
    xEntry1("InitAtoms");
    
    /// Cache all necessary Atoms at startup to avoid synchronous round-trips 
    /// to the X Server during time-critical clipboard operations.
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

/**
 * @brief Creates a hidden dummy window to receive XFixes events.
 * @param c The XCB connection.
 * @return The generated window ID.
 */
xcb_window_t CreateListenerWindow(xcb_connection_t *c) {
    xEntry1("CreateListenerWindow");
    
    /// Get the primary screen (root window) where everything is drawn.
    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    xcb_window_t win = xcb_generate_id(c);
    
    /// Create an unmapped (invisible) window. We don't map it to the screen 
    /// because it's only used as a communication endpoint for X11 events.
    xcb_create_window(c, XCB_COPY_FROM_PARENT, win, screen->root,
                      0, 0, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      0, NULL);

    /// Tell the X Server that this window wants to be notified when properties change.
    /// This is strictly required for the INCR protocol (both sending and receiving chunks).
    uint32_t event_mask[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
    xcb_change_window_attributes(c, win, XCB_CW_EVENT_MASK, event_mask);

    xExit1("CreateListenerWindow: %u", win);
    return win;
}

/**
 * @brief Requests the XFixes extension to notify us of clipboard ownership changes.
 * @param c The XCB connection.
 * @param window The dummy window to receive events.
 */
void SubscribeClipboardEvents(xcb_connection_t *c, xcb_window_t window) {
    xEntry1("SubscribeClipboardEvents");
    
    /// Query XFixes version to ensure the server supports it and initialize the extension.
    xcb_xfixes_query_version_cookie_t ck = xcb_xfixes_query_version(c, XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION);
    xcb_xfixes_query_version_reply_t *r = xcb_xfixes_query_version_reply(c, ck, NULL);
    if(r) free(r);

    /// Subscribe to selection events. The X Server will now send an event to our dummy window
    /// every time an application calls xcb_set_selection_owner (i.e., when a user copies something).
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

/**
 * @brief Loads data into memory and claims ownership of the X11 Clipboard.
 * @param c The XCB connection.
 * @param win Our listener window ID.
 * @param data Pointer to the actual data (text string or image bytes).
 * @param len Length of the data in bytes.
 * @param type The format of the data (AtomUtf8, AtomPng, etc.).
 * @note Frees any previously held active data to prevent memory leaks.
 */
void SetClipboardData(xcb_connection_t *c, xcb_window_t win, void *data, size_t len, xcb_atom_t type) {
    xEntry1("SetClipboardData");
    
    /// Free previously cached data to avoid memory leaks before taking new data.
    if (ActiveData) {
        free(ActiveData);
    }
    
    /// Allocate fresh memory and copy the payload. This acts as our holding buffer.
    ActiveData = malloc(len);
    if (!ActiveData) {
        xError("[SetClipboardData] Out of memory!");
        return;
    }
    memcpy(ActiveData, data, len);
    ActiveDataLen = len;
    ActiveDataType = type;

    /// Announce to the X Server that our window is now the definitive owner of the CLIPBOARD.
    /// Other applications will now route their paste requests to us.
    xcb_set_selection_owner(c, win, AtomClipboard, XCB_CURRENT_TIME);
    
    /// Verify that the X Server acknowledged our claim. Another app might have 
    /// snatched it at the exact same millisecond (race condition).
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
 * @note Avoid using non-reentrant functions (like printf or malloc) inside this handler.
 */
void SignalEventHandler(int SigNum) {
    xLog1("[SignalEventHandler] Was called with SigNum=%d", SigNum);

    if (SigNum == SIGINT || SigNum == SIGTERM) {
        /// User pressed Ctrl+C or sent a termination kill command.
        /// Raise the exit flag so the main loop can terminate gracefully.
        RequestExit = eACTIVATE;
        xLog1("[SignalEventHandler] Activate RequestExit!");
    } 
    else if (SigNum == SIGUSR1) {
        /// Toggle the Rofi menu visibility state.
        if (TogglePopUpStatus == eHIDEN || TogglePopUpStatus == eNOT_STARTED) {
            TogglePopUpStatus = eREQ_SHOW;
        } 
        else if (TogglePopUpStatus == eSHOWN) {
            TogglePopUpStatus = eREQ_HIDE;
        }
    }
    else if (SigNum == SIGUSR2) {
        /// Custom signal received from another terminal (e.g., pkill -SIGUSR2)
        /// Triggers the background thread to inject the currently selected UI item.
        xLog1("[SignalEventHandler] Injecting selected item into X11 Clipboard...");
        ReqTestInject = eACTIVATE;
    }
}

/**
 * @brief Registers the application's signal handlers with the Operating System.
 * @return OKE on success.
 */
RetType RegisterSignal(void) {
    xEntry1("RegisterSignal");

    /// Map specific POSIX signals to our custom callback function.
    signal(SIGUSR2, SignalEventHandler); 
    signal(SIGUSR1, SignalEventHandler); 
    signal(SIGINT,  SignalEventHandler); 
    signal(SIGTERM, SignalEventHandler); 

    xLog1("[RegisterSignal] Listening for OS signals... (PID: %d)", getpid());

    xExit1("RegisterSignal");
    return OKE; 
}

/**
 * @brief The main loop for the Signal Thread. It idles and waits for OS signals.
 * @param Param Optional parameter passed when the thread starts.
 * @return OKE when the thread exits cleanly.
 */
RetType SignalRuntime(int Param) {
    xEntry1("SignalRuntime");

    RegisterSignal();

    /// Infinite loop to keep this thread alive.
    /// The pause() function suspends the thread entirely (consuming 0% CPU)
    /// until a signal is caught and handled by SignalEventHandler.
    while (1) {
        pause(); 

        /// Check flags updated by the asynchronous handler.
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

/**
 * @brief Handles XFixes Selection Notify events (Triggered when another app copies data).
 * @param Event The generic XCB event containing the notification.
 */
void HandleXFixesNotify(xcb_generic_event_t *Event) {
    xcb_xfixes_selection_notify_event_t *Sevent = (xcb_xfixes_selection_notify_event_t *)Event;
    
    /// Avoid processing events triggered by our own application claiming the clipboard.
    if (Sevent->owner == MyWindow) {
        return;
    }

    xLog1("[XClipboardRuntime] [Event] Clipboard Owner Changed! OwnerID: %u", Sevent->owner);

    /// Step 1 of Copying: We don't grab the data blindly. We ask the new owner
    /// to provide a list of all formats (TARGETS) they can convert their data into.
    xcb_convert_selection(Connection, MyWindow, AtomClipboard, AtomTarget, AtomProperty, Sevent->timestamp);
    xcb_flush(Connection);
}

/**
 * @brief Helper function to handle TARGETS negotiation responses.
 * @param Nevent The selection notify event.
 * @param Data Pointer to the array of supported atoms.
 * @param ByteLen Length of the target data in bytes.
 */
static inline void HandleSelectionNotify_Negotiate(xcb_selection_notify_event_t *Nevent, void *Data, int ByteLen) {
    xLog1("[XClipboardRuntime] [Target Negotiation] Received format menu. Length: %d bytes", ByteLen);
    
    xcb_atom_t *SupportedAtoms = (xcb_atom_t *)Data;
    int AtomCount = ByteLen / sizeof(xcb_atom_t);
    xcb_atom_t BestTarget = XCB_ATOM_NONE;

    /// Scan the provided list of supported formats. 
    /// We prioritize rich media (PNG) over low-quality media (JPEG), and fallback to text (UTF8).
    for (int i = 0; i < AtomCount; i++) {
        if (SupportedAtoms[i] == AtomPng) { BestTarget = AtomPng; break; }
        else if (SupportedAtoms[i] == AtomJpeg && BestTarget != AtomPng) { BestTarget = AtomJpeg; }
        else if (SupportedAtoms[i] == AtomUtf8 && BestTarget == XCB_ATOM_NONE) { BestTarget = AtomUtf8; }
    }

    /// If the owner supports a format we understand, issue a new request for the actual data.
    if (BestTarget != XCB_ATOM_NONE) {
        xLog1("[XClipboardRuntime] [Target Negotiation] Found matching target: %u. Requesting data...", BestTarget);
        xcb_convert_selection(Connection, MyWindow, AtomClipboard, BestTarget, AtomProperty, Nevent->time);
        xcb_flush(Connection);
    }

    /// Cleanup the property used for the TARGETS menu.
    xcb_delete_property(Connection, MyWindow, AtomProperty);
    xcb_flush(Connection);
}

/**
 * @brief Helper function to handle actual data receiving (Single-shot or INCR setup).
 * @param Nevent The selection notify event.
 * @param reply The property reply containing the data chunk or INCR atom.
 * @param Data Pointer to the received payload.
 * @param ByteLen Length of the received payload.
 */
static inline void HandleSelectionNotify_ReceiveAndSave(xcb_selection_notify_event_t *Nevent, xcb_get_property_reply_t *reply, void *Data, int ByteLen) {
    const char *Extension = "txt"; 
    if (Nevent->target == AtomPng) Extension = "png";
    else if (Nevent->target == AtomJpeg) Extension = "jpg";

    /// --- 1. Handle Large Files via INCR Protocol ---
    /// The sender refused to send everything at once and returned the INCR atom instead.
    if (reply->type == AtomIncr) {
        xLog1("[XClipboardRuntime] Sender initiated INCR Protocol. Preparing to receive chunks...");
        
        GetTimeBasedFilename(IncrRecvFilename, sizeof(IncrRecvFilename), Extension);
        char FullPath[PATH_MAX];
        snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, IncrRecvFilename);
        
        /// Open the target file in append/binary mode to stitch incoming chunks together.
        IncrRecvFile = fopen(FullPath, "wb");
        if (IncrRecvFile) {
            IsReceivingIncr = 1;
            /// Critical step in INCR protocol: We must delete the property to tell the
            /// sender that we are ready to receive the very first data chunk.
            xcb_delete_property(Connection, MyWindow, AtomProperty);
            xcb_flush(Connection);
        } else {
            xError("[XRuntime] Failed to open INCR receive file.");
        }
    }

    /// --- 2. Handle Single-shot Transfer (With Chunking Fix for Browser limitations) ---
    /// The sender put the data directly into the property.
    else {
        char Filename[NAME_MAX], FullPath[PATH_MAX];
        GetTimeBasedFilename(Filename, sizeof(Filename), Extension);
        
        snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, Filename);
        FILE *fp = fopen(FullPath, "wb"); 
        
        if (fp) {
            /// Write the initial payload chunk to disk.
            fwrite(Data, 1, ByteLen, fp);
            int TotalBytes = ByteLen;
            
            /// Web browsers often dump massive amounts of data into a single property without using INCR.
            /// The X Server will truncate this to ~256KB to protect RAM. 
            /// We check `reply->bytes_after` to see if the server has more data waiting.
            uint32_t CurrentOffset = ByteLen / 4; 
            uint32_t BytesAfter = reply->bytes_after;

            /// Loop to vacuum up all remaining data fragments cached on the X Server.
            while (BytesAfter > 0) {
                /// Request the next segment (up to 8MB at a time).
                xcb_get_property_cookie_t ck = xcb_get_property(Connection, 0, MyWindow, AtomProperty, XCB_GET_PROPERTY_TYPE_ANY, CurrentOffset, 2097152);
                xcb_get_property_reply_t *r = xcb_get_property_reply(Connection, ck, NULL);
                
                if (!r) break;

                int ChunkLen = xcb_get_property_value_length(r);
                if (ChunkLen > 0) {
                    void *ChunkData = xcb_get_property_value(r);
                    fwrite(ChunkData, 1, ChunkLen, fp);
                    TotalBytes += ChunkLen;
                    
                    /// Shift the offset forward to prepare for the next read operation.
                    CurrentOffset += ChunkLen / 4; 
                }
                
                BytesAfter = r->bytes_after;
                free(r);
            }

            fclose(fp);
            
            /// Add the complete file to the internal history list.
            XCBList_PushItem(Filename);
            xLog1("[XClipboardRuntime] Saved %d bytes to %s", TotalBytes, Filename);
            
            /// Cleanup the property to avoid memory leaks on the X Server.
            xcb_delete_property(Connection, MyWindow, AtomProperty);
            xcb_flush(Connection);

        } else {
            xError("[XClipboardRuntime] Failed to open file for writing: %s", FullPath);
        }
    }
}

/**
 * @brief Handles Selection Notify events (Triggered when requested data arrives).
 * @param Event The generic XCB event containing the payload.
 */
void HandleSelectionNotify(xcb_generic_event_t *Event) {
    xcb_selection_notify_event_t *Nevent = (xcb_selection_notify_event_t *)Event;

    /// If property is NONE, the owner rejected our request (unsupported format or timed out).
    if (Nevent->property == XCB_NONE) {
        xWarn("[XClipboardRuntime] [Event] Target conversion failed or denied by owner.");
        return;
    }

    /// Read up to 32MB initially. If it's larger, the loop inside 
    /// HandleSelectionNotify_ReceiveAndSave will fetch the rest.
    xcb_get_property_cookie_t cookie = xcb_get_property(Connection, 0, MyWindow, AtomProperty, XCB_GET_PROPERTY_TYPE_ANY, 0, 8388608);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(Connection, cookie, NULL);

    if (reply && xcb_get_property_value_length(reply) > 0) {
        int ByteLen = xcb_get_property_value_length(reply);
        void *Data = xcb_get_property_value(reply);

        /// Dispatch to specialized handlers based on what we originally asked for.
        if (Nevent->target == AtomTarget) {
            HandleSelectionNotify_Negotiate(Nevent, Data, ByteLen);
        }
        else if (Nevent->target == AtomUtf8 || Nevent->target == AtomPng || Nevent->target == AtomJpeg) {
            HandleSelectionNotify_ReceiveAndSave(Nevent, reply, Data, ByteLen);
        }
    }
    
    if (reply) free(reply);
}

/**
 * @brief Handles Property Notify events for managing both INCR Receiver and Provider flows.
 * @param Event The generic XCB event indicating property modifications.
 */
void HandlePropertyNotify(xcb_generic_event_t *Event) {
    xcb_property_notify_event_t *PropEv = (xcb_property_notify_event_t *)Event;
    
    /// --- [RECEIVER MODE] Processing incoming data chunks ---
    /// We triggered this by deleting the property. The sender has now written a new chunk.
    if (IsReceivingIncr && PropEv->window == MyWindow && 
        PropEv->atom == AtomProperty && PropEv->state == XCB_PROPERTY_NEW_VALUE) {
        
        xcb_get_property_cookie_t ck = xcb_get_property(Connection, 0, MyWindow, AtomProperty, XCB_GET_PROPERTY_TYPE_ANY, 0, 8388608);
        xcb_get_property_reply_t *r = xcb_get_property_reply(Connection, ck, NULL);
        
        if (r) {
            int ChunkLen = xcb_get_property_value_length(r);
            if (ChunkLen > 0) {
                /// Write the chunk to our open file.
                void *ChunkData = xcb_get_property_value(r);
                if (IncrRecvFile) fwrite(ChunkData, 1, ChunkLen, IncrRecvFile);
                
                /// Delete the property again to ping the sender for the next chunk.
                xcb_delete_property(Connection, MyWindow, AtomProperty);
                xcb_flush(Connection);
            } else {
                /// INCR protocol states that a chunk of size 0 means End-Of-File.
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

    /// --- [PROVIDER MODE] Pumping data chunks out ---
    /// The receiver just deleted the property, signaling us to push the next chunk.
    if (PropEv->state == XCB_PROPERTY_DELETE && 
        PropEv->window == IncrRequestor && 
        PropEv->atom == IncrProperty) {
        
        size_t BytesLeft = IncrDataLen - IncrOffset;
        
        if (BytesLeft > 0) {
            /// Slice the next 64KB block from our buffer and inject it into the receiver's window.
            size_t ChunkSize = (BytesLeft > INCR_CHUNK_SIZE) ? INCR_CHUNK_SIZE : BytesLeft;
            
            xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, IncrRequestor, 
                                IncrProperty, IncrTarget, 8, ChunkSize, 
                                (uint8_t *)IncrData + IncrOffset);
            IncrOffset += ChunkSize;
        } else {
            /// If no bytes are left, inject a 0-byte payload to formally signal End-Of-File.
            uint8_t EOF_Dummy = 0;
            xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, IncrRequestor, 
                                IncrProperty, IncrTarget, 8, 0, &EOF_Dummy);
            xLog1("[INCR] Transfer Complete!");
            
            /// Reset the State Machine.
            IncrRequestor = XCB_NONE;
            IncrProperty = XCB_NONE;
            IncrTarget = XCB_NONE;
        }
        xcb_flush(Connection);
    }
}

/**
 * @brief Handles Selection Request events, providing clipboard data to other apps.
 * @param Event The generic XCB event containing the selection request.
 */
void HandleSelectionRequest(xcb_generic_event_t *Event) {
    xEntry1("HandleSelectionRequest");
    xcb_selection_request_event_t *Req = (xcb_selection_request_event_t *)Event;
    
    xLog1("[XClipboardRuntime] [Event] SelectionRequest from window: %u for target: %u", 
          Req->requestor, Req->target);

    /// 1. Initialize the formal response structure.
    /// By default, we set property to XCB_NONE, which means "Request Denied".
    xcb_selection_notify_event_t Reply;
    memset(&Reply, 0, sizeof(Reply));
    Reply.response_type = XCB_SELECTION_NOTIFY;
    Reply.requestor     = Req->requestor;
    Reply.selection     = Req->selection;
    Reply.target        = Req->target;
    Reply.time          = Req->time;
    Reply.property      = XCB_NONE; 
    
    /// The ICCCM standard dictates that if the requestor provides NONE as the property, 
    /// we must use their target atom as the property name for the transfer.
    xcb_atom_t ValidProperty = (Req->property == XCB_NONE) ? Req->target : Req->property;

    /// 2. Process the request based on what the other application wants.
    
    if (Req->target == AtomTarget) { 
        /// --- 2a. Target Negotiation ---
        /// They want to know what formats we can provide. We respond with an array
        /// containing TARGETS, TIMESTAMP, and the actual format of our cached data.
        xcb_atom_t SupportedTargets[] = { AtomTarget, AtomTimestamp, ActiveDataType };
        int NumTargets = sizeof(SupportedTargets) / sizeof(xcb_atom_t);
        
        xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Req->requestor, 
                            ValidProperty, XCB_ATOM_ATOM, 32, NumTargets, SupportedTargets);
        Reply.property = ValidProperty;
    }
    else if (Req->target == AtomTimestamp) {
        /// --- 2b. Timestamp Request ---
        /// Used by window managers to resolve race conditions between clipboard clients.
        xcb_timestamp_t CurrentTime = Req->time; 
        xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Req->requestor, 
                            ValidProperty, XCB_ATOM_INTEGER, 32, 1, &CurrentTime);
        Reply.property = ValidProperty;
    }
    else if (Req->target == ActiveDataType && ActiveData != NULL) {
        /// --- 2c. Actual Data Request ---
        /// They requested the correct format, so we proceed to send the payload.
        
        if (ActiveDataLen > INCR_CHUNK_SIZE) {
            
            /// [FIX DEADLOCK]: If we are stuck serving a previous app that disconnected 
            /// unexpectedly without finishing INCR, we ruthlessly abort that transfer 
            /// to serve the new incoming request.
            if (IncrData != NULL) {
                xLog1("[INCR] Alert! Aborting stuck transfer to serve new req from Window: %u", Req->requestor);
                IncrData      = NULL;
                IncrOffset    = 0;
                IncrRequestor = XCB_NONE;
            }

            xLog1("[XRuntime] Data > 64KB. Starting INCR Protocol...");

            /// Initialize the global state machine variables for the INCR process.
            IncrData      = ActiveData;
            IncrDataLen   = ActiveDataLen;
            IncrOffset    = 0;
            IncrRequestor = Req->requestor;
            IncrProperty  = ValidProperty;
            IncrTarget    = ActiveDataType;

            /// We MUST subscribe to PropertyChange events on the target window. 
            /// Otherwise, we won't hear them delete the property to trigger our next chunk.
            uint32_t EventMask[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
            xcb_change_window_attributes(Connection, Req->requestor, XCB_CW_EVENT_MASK, EventMask);

            /// Tell the requestor we are starting an INCR transfer. 
            /// We write a single 32-bit integer indicating the total expected size.
            uint32_t TotalSize = ActiveDataLen;
            xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Req->requestor, 
                                ValidProperty, AtomIncr, 32, 1, &TotalSize);
            Reply.property = ValidProperty;
        } 
        else {
            /// Payload is small enough to fit inside a single property update.
            xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Req->requestor, 
                                ValidProperty, ActiveDataType, 8, ActiveDataLen, ActiveData);
            Reply.property = ValidProperty;
        }
    }

    /// 3. Send the formal XCB_SELECTION_NOTIFY event back to the requestor
    /// to tell them the property is ready (or denied if Reply.property is NONE).
    xcb_send_event(Connection, 0, Req->requestor, XCB_EVENT_MASK_NO_EVENT, (const char *)&Reply);
    xcb_flush(Connection);
    
    xExit1("HandleSelectionRequest");
}

/**************************************************************************************************
 * THREAD RUNTIME SECTION *************************************************************************
 **************************************************************************************************/

/**
 * @brief The main loop that manages X11 connections and dispatches events.
 * @param Param Optional thread parameter.
 * @return OKE when the thread exits cleanly.
 */
RetType XClipboardRuntime(int Param) {
    xEntry1("XClipboardRuntime");

    Connection = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(Connection)) return ERR;

    /// Retrieve the base event code for XFixes. This is necessary because 
    /// extension event codes are dynamically assigned by the X Server.
    const xcb_query_extension_reply_t *xfixes_data = xcb_get_extension_data(Connection, &xcb_xfixes_id);
    if (!xfixes_data || !xfixes_data->present) return ERR;
    uint8_t XFixesEventBase = xfixes_data->first_event;

    /// Prepare environment variables.
    InitAtoms(Connection);
    MyWindow = CreateListenerWindow(Connection);
    SubscribeClipboardEvents(Connection, MyWindow);

    if(CheckSingleInstance(Connection, MyWindow) != OKE){
        xError("[XClipboardRuntime] Another app already started! Please close it before start again!");
        ClipboardCaptureFinalize();
        exit(-1);
    }

    xLog1("[XClipboardRuntime] Listening for Clipboard events...");
    xcb_generic_event_t *Event;

    while (RequestExit != eACTIVATE) {
        
        /// Handle manual injection triggered by the Rofi UI menu.
        if (ReqTestInject == eACTIVATE) {
            ReqTestInject = eDEACTIVATE;
            sClipboardItem LatestItem;
            
            /// 1. Retrieve the metadata of the user's selected history item.
            if (XCBList_GetSelectedItem(&LatestItem) == OKE) {
                
                /// Resolve the correct X11 MIME type based on the file extension.
                xcb_atom_t TargetAtom = AtomUtf8; 
                if (LatestItem.FileType == eFMT_IMG_PNG) TargetAtom = AtomPng;
                else if (LatestItem.FileType == eFMT_IMG_JGP) TargetAtom = AtomJpeg;

                static uint8_t RawData[8U * 1024U * 1024U]; 
                
                char FullPath[PATH_MAX];
                snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, LatestItem.Filename);
                struct stat FileStat;
                
                /// 2. Verify file integrity and ensure it fits into the 8MB buffer.
                if (stat(FullPath, &FileStat) == 0 && FileStat.st_size > 0 && FileStat.st_size <= sizeof(RawData)) {
                    int SelectedIdx = XCBList_GetSelectedNum();
                    
                    /// 3. Read the binary data from disk and inject it into the X11 clipboard system.
                    if (XCBList_ReadAsBinary(SelectedIdx, RawData, sizeof(RawData)) >= 0) {
                        SetClipboardData(Connection, MyWindow, (void *)RawData, FileStat.st_size, TargetAtom);
                        xLog1("[XClipboardRuntime] Injected %s (%ld bytes) as Atom %u", 
                              LatestItem.Filename, FileStat.st_size, TargetAtom);
                    } else {
                        xWarn("[XClipboardRuntime] ReadAsBinary failed!");
                    }
                } else {
                    xWarn("[XClipboardRuntime] File missing, empty, or exceeds 8MB buffer!");
                }
            } else {
                xWarn("[XClipboardRuntime] No item selected or DB is empty!");
            }
        }

        /// Use xcb_poll_for_event instead of xcb_wait_for_event. This makes the loop non-blocking 
        /// so we can periodically check the `ReqTestInject` and `RequestExit` flags.
        Event = xcb_poll_for_event(Connection);
        if (Event == NULL) {
            if (xcb_connection_has_error(Connection)) break;
            usleep(2U * 1000U); /// Fast polling (2ms) ensures INCR protocol speed remains high.
            continue;
        }

        /// Strip the highest bit (0x80) from the response type. This bit merely indicates 
        /// that the event was synthesized by another client (via xcb_send_event).
        uint8_t EventType = Event->response_type & ~0x80;

        /// Dispatch the event to the appropriate specialized handler.
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

/**
 * @brief Cleans up resources and waits for the background threads to exit safely.
 * @note Registered via atexit() to run automatically upon application termination.
 */
void ClipboardCaptureFinalize(void) {
    xLog1("[Finalize] Initiating shutdown sequence...");

    /// Send an interrupt signal to wake up the SignalRuntimeThread if it's stuck in pause().
    int SigKillStatus = pthread_kill(SignalRuntimeThread, SIGINT);
    if (SigKillStatus == 0) {
        pthread_join(SignalRuntimeThread, NULL);
        xLog1("[Finalize] Signal Thread joined.");
    }

    /// Wait for the X11 thread to finish processing its current event and exit.
    pthread_join(XClipboardRuntimeThread, NULL);
    xLog1("[Finalize] XClipboard Thread joined.");
    
    /// Free the global active clipboard payload.
    if (ActiveData != NULL) {
        free(ActiveData);
        ActiveData = NULL;
        ActiveDataLen = 0;
        xLog1("[Finalize] Freed ActiveData buffer.");
    }
    
    xLog1("[Finalize] Application exited gracefully.");
    xExit1("ClipboardCaptureFinalize");
}

/**
 * @brief Initializes database directories, starts background threads, and prepares X11 atoms.
 * @return OKE on success, ERR on failure.
 */
RetType ClipboardCaptureInitialize(void) {
    xEntry1("ClipboardCaptureInitialize");
    
    /// Register the cleanup callback to ensure resources are freed when main() exits.
    atexit(ClipboardCaptureFinalize);

    /// Verify local storage directories.
    if (EnsureDB() != OKE) {
        xError("[Initialize] System Check Failed!");
        return ERR; 
    }

    /// Load existing clipboard items from disk into the Ring Buffer.
    if(XCBList_Scan(0) < 0){
        xError("[Initialize] Scan DB failed!");
    }

    /// Spin up the POSIX Signal monitoring thread.
    if (pthread_create(&SignalRuntimeThread, NULL, (void *(*)(void *))SignalRuntime, NULL) != 0) {
        xError("[Initialize] Failed to create Signal Thread!");
        return ERR;
    }

    /// Spin up the X11 event loop thread.
    if (pthread_create(&XClipboardRuntimeThread, NULL, (void *(*)(void *))XClipboardRuntime, NULL) != 0) {
        xError("[Initialize] Failed to create XClipboard Thread!");
        return ERR;
    }

    xLog1("[Initialize] All systems started. Main thread is now free.");
    xExit1("ClipboardCaptureInitialize");
    return OKE;
}

/**************************************************************************************************
 * ROFI SUPPORT ***********************************************************************************
 **************************************************************************************************/ 

#if (ROFI_SUPPORT == 1)

    #ifndef PREVIEW_TXT_LEN
        #define PREVIEW_TXT_LEN 80
    #endif /*PREVIEW_TXT_LEN*/

    /**
     * @brief Reads file content and writes a formatted Rofi menu item directly to the temp file.
     * @param OutFile The temporary file stream (/tmp/cbc_rofi.txt).
     * @param Index The logical index of the clipboard item.
     * @param Item Pointer to the clipboard item data structure.
     * @return OKE on success.
     * @note Unprintable characters and newlines are sanitized to prevent Rofi parsing crashes.
     */
    RetType WriteRofiMenuItem(FILE *OutFile, int Index, sClipboardItem *Item) {
        char FullPath[PATH_MAX];
        snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, Item->Filename);

        if (Item->FileType == eFMT_IMG_PNG || Item->FileType == eFMT_IMG_JGP) {
            /// For images: No preview text, just print "[Image]" and pass the path as Thumbnail
            fprintf(OutFile, "%d: [Image] %s%cicon\x1f%s\n", 
                    Index, Item->Filename, '\0', FullPath);
        } 
        else {
            /// For text: Read file content to generate a preview string
            FILE *fp = fopen(FullPath, "rb");
            char Preview[PREVIEW_TXT_LEN + 1];
            memset(Preview, 0, sizeof(Preview));
            
            if (fp) {
                /// Read up to PREVIEW_TXT_LEN characters
                size_t ReadBytes = fread(Preview, 1, PREVIEW_TXT_LEN, fp);
                fclose(fp);
                
                /// Safely null-terminate the string
                Preview[ReadBytes] = '\0';
                
                /// [CRITICAL STEP]: Sanitize the string!
                /// Replace newlines/tabs with space.
                /// Replace unprintable/control characters with '?' to prevent Rofi from crashing.
                for (size_t i = 0; i < ReadBytes; i++) {
                    if (Preview[i] == '\n' || Preview[i] == '\r' || Preview[i] == '\t') {
                        Preview[i] = ' '; 
                    } 
                    /// Filter out standard ASCII control characters (0-31), except we already handled \n, \r, \t
                    else if ((unsigned char)Preview[i] < 32 || Preview[i] == 127) {
                        Preview[i] = '?';
                    }
                }
                
                /// If the text is longer than the allowed limit, append "[...]"
                if (ReadBytes == PREVIEW_TXT_LEN) {
                    Preview[PREVIEW_TXT_LEN - 5] = '[';
                    Preview[PREVIEW_TXT_LEN - 4] = '.';
                    Preview[PREVIEW_TXT_LEN - 3] = '.';
                    Preview[PREVIEW_TXT_LEN - 2] = '.';
                    Preview[PREVIEW_TXT_LEN - 1] = ']';
                }
                
                fprintf(OutFile, "%d: %s%cicon\x1ftext-x-generic\n", 
                        Index, Preview, '\0');
            } 
            else {
                /// Fallback in case the file is missing or deleted
                fprintf(OutFile, "%d: [Empty/Missing File]%cicon\x1ftext-x-generic\n", 
                        Index, '\0');
            }
        }
        return OKE;
    }

    /**
     * @brief Calls the Rofi dmenu interface to let the user select a clipboard item.
     */
    void ShowRofiMenu(void) {
        xEntry1("ShowRofiMenu");
        
        /// 1. Create a temporary file to hold the menu items
        FILE *tmp = fopen(PATH_FILE_ROFI_MENU, "w");
        if (!tmp) {
            xError("[UI] Failed to create temp file for Rofi.");
            return;
        }

        /// 2. Dump the current RAM list into the text file
        int size = XCBList_GetItemSize();
        for (int i = 0; i < size; i++) {
            sClipboardItem item;
            if (XCBList_GetItem(i, &item) == OKE) {
                WriteRofiMenuItem(tmp, i, &item);
            }
        }

        /// [NEW OPTION]: Append "Clear All History" at the end of the list
        /// We use the index equal to 'size' as a special signal
        fprintf(tmp, "%d: --- CLEAR ALL HISTORY ---%cicon\x1f" "edit-clear-all\n", size, '\0');

        fclose(tmp);

        /// 3. Execute Rofi
        static char RofiCmd[512]; 
        snprintf(RofiCmd, sizeof(RofiCmd), "rofi -dmenu -i -show-icons -p 'X11 Clipboard' < %s", PATH_FILE_ROFI_MENU);
        FILE *rofi = popen(RofiCmd, "r");
        
        if (!rofi) {
            xError("[UI] Failed to execute Rofi.");
            return;
        }

        char result[256];
        if (fgets(result, sizeof(result), rofi) != NULL) {
            int selected_index = -1;
            if (sscanf(result, "%d:", &selected_index) == 1) {
                
                /// 4. Logic Handling based on index
                if (selected_index == size) {
                    /// User selected "CLEAR ALL HISTORY"
                    xLog1("[UI] User requested to CLEAR ALL HISTORY.");
                    XCBList_ClearAllItems();
                } 
                else {
                    /// User selected a normal item
                    xLog1("[UI] User selected index: %d", selected_index);
                    if (XCBList_SetSelectedNum(selected_index) == OKE) {
                        ReqTestInject = eACTIVATE;
                    }
                }
            }
        } else {
            xLog1("[UI] User cancelled Rofi (pressed ESC).");
        }

        pclose(rofi);
        xExit1("ShowRofiMenu");
    }

#endif /*(ROFI_SUPPORT == 1)*/

/**************************************************************************************************
 * EOF ********************************************************************************************
 **************************************************************************************************/
