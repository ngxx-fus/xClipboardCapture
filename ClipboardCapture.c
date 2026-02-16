#include "ClipboardCapture.h"
#include "CBC_Setup.h"
#include "CBC_SysFile.h"
#include "xUniversal.h"
#include <xUniversalReturn.h>
#include <xcb/xcb.h>
#include <sys/time.h>
#include <semaphore.h>

/**************************************************************************************************
 * FORWARD DECLARATIONS ***************************************************************************
 **************************************************************************************************/ 
void HandleSelectionNotify(xcb_generic_event_t *Event);
void HandleSelectionRequest(xcb_generic_event_t *Event);
void HandlePropertyNotify(xcb_generic_event_t *Event);
void HandleXFixesNotify(xcb_generic_event_t *Event);

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
 * @brief Atom representing BMP image MIME type ("image/bmp").
 */
xcb_atom_t AtomBmp;

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
 * @brief Flag to control the visibility state of the UI popup menu (e.g., Rofi).
 * @note Modified asynchronously by the OS signal handler.
 */
volatile sig_atomic_t TogglePopUpStatus = eNOT_STARTED;

/**
 * @brief Global flag to signal all threads to gracefully terminate.
 */
volatile sig_atomic_t RequestExit       = eDEACTIVATE;

/**
 * @brief Flag to trigger the injection of the selected clipboard item into the active window.
 */
volatile sig_atomic_t ReqTestInject     = eDEACTIVATE;

/**
 * @brief Synchronization flag used to block the Provider thread until the Receiver thread finishes X11 setup.
 */
volatile sig_atomic_t ReqWaitForSetup   = eACTIVATE;      

/**
 * @brief POSIX Semaphore used to block and wake up the Provider thread without consuming CPU cycles.
 */
static sem_t SemProviderWakeup;

/**
 * @brief Thread handle for the OS signal listener (SIGINT, SIGUSR1, SIGUSR2).
 */
static pthread_t SignalRuntimeThread;

/**
 * @brief Thread handle for the X11 event loop that continuously receives clipboard data.
 */
static pthread_t XClipboardRuntimeThread_Receiver;

/**
 * @brief Thread handle for the background loop that serves clipboard data to other applications.
 */
static pthread_t XClipboardRuntimeThread_Provider;


/**************************************************************************************************
 * INCR PROTOCOL (PROVIDER) SECTION ***************************************************************
 **************************************************************************************************/ 

/**
 * @brief Maximum chunk size (64KB) for sending data via the INCR protocol.
 */
#define INCR_CHUNK_SIZE 65536

/**
 * @brief Pointer to the payload currently being transmitted to another application.
 */
uint8_t *IncrData = NULL;

/**
 * @brief Total size in bytes of the payload being transmitted.
 */
size_t IncrDataLen = 0;

/**
 * @brief Current byte offset indicating how much data has been successfully transmitted.
 */
size_t IncrOffset = 0;

/**
 * @brief The X11 Window ID of the application requesting the clipboard data.
 */
xcb_window_t IncrRequestor = XCB_NONE;

/**
 * @brief The X11 Atom representing the property used to stage outgoing chunks.
 */
xcb_atom_t IncrProperty = XCB_NONE;

/**
 * @brief The X11 Atom representing the requested data format (e.g., UTF8_STRING, image/png).
 */
xcb_atom_t IncrTarget = XCB_NONE;


/**************************************************************************************************
 * FULL-TRANSACTION LOCK & 128MB RAM CACHING (RECEIVER) SECTION ***********************************
 **************************************************************************************************/ 

/**
 * @brief Maximum size of the RAM cache buffer (128MB).
 */
#define MAX_RAM_CACHE (128U * 1024U * 1024U)

/**
 * @brief Safety timeout in milliseconds to automatically break a stuck transaction.
 */
#define TRANSACTION_TIMEOUT_MS 5000          

/**
 * @brief Global lock flag to prevent interleaved or spamming transactions (0 = free, 1 = busy).
 */
volatile sig_atomic_t TransactionLock = 0;   

/**
 * @brief Flag indicating whether the Receiver is currently processing an incoming INCR stream.
 */
static int IsReceivingIncr = 0;              

/**
 * @brief Timestamp of the current X11 transaction used to prevent race conditions.
 */
static xcb_timestamp_t CurrentTransactionTime = XCB_CURRENT_TIME;

/**
 * @brief Heartbeat timestamp (in milliseconds) used to track transaction timeouts.
 */
static long long TransactionStartMs = 0;     

/**
 * @brief Pointer to the statically allocated 128MB RAM buffer for fast data ingestion.
 */
static uint8_t *IncrRecvBuf = NULL;          

/**
 * @brief Current write offset within the 128MB RAM cache.
 */
static size_t IncrRecvOffset = 0;          

/**
 * @brief Cumulative count of bytes received across all chunks and disk flushes.
 */
static size_t TotalBytesReceived = 0;      

/**
 * @brief File handle used to flush the RAM cache to disk when full or finished.
 */
FILE *IncrRecvFile = NULL;                   

/**
 * @brief The generated filename for the current incoming clipboard item.
 */
char IncrRecvFilename[NAME_MAX];

/**************************************************************************************************
 * HELPER FUNCTIONS *******************************************************************************
 **************************************************************************************************/ 

/**
 * @brief Returns current time in milliseconds.
 */
static inline long long GetNowMs(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/**
 * @brief Generates a unique filename using timestamp and a static counter.
 */
static inline void GetUniqueFilename(char *buf, size_t len, const char *ext) {
    static int FileCounter = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);

    snprintf(buf, len, "%04d%02d%02d_%02d%02d%02d_%03ld_%d.%s",
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
             tv.tv_usec / 1000, FileCounter++, ext);
    if (FileCounter > 999) FileCounter = 0;
}

/**
 * @brief Pushes incoming data to the 128MB RAM Cache. Flushes to disk if full.
 * @param data The incoming byte payload.
 * @param len The length of the payload.
 */
static inline void PushToCache(const uint8_t *data, size_t len) {
    size_t Written = 0;
    while (Written < len) {
        size_t SpaceLeft = MAX_RAM_CACHE - IncrRecvOffset;
        size_t ToWrite = (len - Written < SpaceLeft) ? (len - Written) : SpaceLeft;
        
        memcpy(IncrRecvBuf + IncrRecvOffset, data + Written, ToWrite);
        IncrRecvOffset += ToWrite;
        Written += ToWrite;
        TotalBytesReceived += ToWrite;

        /// RAM is full (128MB reached) -> Flush to Disk to free up space
        if (IncrRecvOffset == MAX_RAM_CACHE) {
            if (IncrRecvFile) {
                xLog1("[RAM CACHE] 128MB Full! Flushing to disk...");
                fwrite(IncrRecvBuf, 1, MAX_RAM_CACHE, IncrRecvFile);
                fflush(IncrRecvFile);
            }
            IncrRecvOffset = 0; /// Reset RAM pointer
        }
    }
}

/**
 * @brief Finalizes the transaction, flushes remaining RAM to disk, and unlocks the fortress.
 */
static inline void FinalizeTransactionAndUnlock(void) {
    if (IncrRecvFile) {
        if (IncrRecvOffset > 0) {
            xLog1("[RAM CACHE] Flushing remaining %zu bytes to disk.", IncrRecvOffset);
            fwrite(IncrRecvBuf, 1, IncrRecvOffset, IncrRecvFile);
            fflush(IncrRecvFile);
        }
        fclose(IncrRecvFile);
        IncrRecvFile = NULL;
        XCBList_PushItem(IncrRecvFilename);
    }
    
    /// Reset States
    IncrRecvOffset = 0;
    TotalBytesReceived = 0;
    IsReceivingIncr = 0;
    TransactionLock = 0; /// UNLOCK THE FORTRESS
    
    xLog1("[FORTRESS] Transaction finalized and unlocked.");
}

/**************************************************************************************************
 * X11 SERVER SETUP SECTION ***********************************************************************
 **************************************************************************************************/ 

/**
 * @brief Interns a single X11 atom by its string name.
 */
xcb_atom_t GetAtomByName(xcb_connection_t *c, const char *name) {
    xEntry1("GetAtomByName(name=%s)", name);
    
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
 */
RetType CheckSingleInstance(xcb_connection_t *c, xcb_window_t win) {
    xcb_atom_t LockAtom = GetAtomByName(c, "CLIPBOARD_CAPTURE_SINGLE_INSTANCE_LOCK");

    xcb_get_selection_owner_cookie_t cookie = xcb_get_selection_owner(c, LockAtom);
    xcb_get_selection_owner_reply_t *r = xcb_get_selection_owner_reply(c, cookie, NULL);

    if (r && r->owner != XCB_NONE) {
        free(r);
        return ERR; 
    }
    if (r) free(r);

    xcb_set_selection_owner(c, win, LockAtom, XCB_CURRENT_TIME);
    
    cookie = xcb_get_selection_owner(c, LockAtom);
    r = xcb_get_selection_owner_reply(c, cookie, NULL);
    if (r && r->owner == win) {
        free(r);
        return OKE;
    }

    if (r) free(r);
    return ERR;
}

/**
 * @brief Initializes all global atoms used by the application.
 */
void InitAtoms(xcb_connection_t *c) {
    xEntry1("InitAtoms");
    
    AtomClipboard = GetAtomByName(c, "CLIPBOARD");
    AtomUtf8      = GetAtomByName(c, "UTF8_STRING");
    AtomTarget    = GetAtomByName(c, "TARGETS");
    AtomPng       = GetAtomByName(c, "image/png");
    AtomJpeg      = GetAtomByName(c, "image/jpeg");
    AtomBmp       = GetAtomByName(c, "image/bmp");
    AtomProperty  = GetAtomByName(c, PROP_NAME);
    AtomTimestamp = GetAtomByName(c, "TIMESTAMP"); 
    AtomIncr      = GetAtomByName(c, "INCR");
    
    xExit1("InitAtoms");
}

/**
 * @brief Creates a hidden dummy window to receive XFixes events.
 */
xcb_window_t CreateListenerWindow(xcb_connection_t *c) {
    xEntry1("CreateListenerWindow");
    
    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    xcb_window_t win = xcb_generate_id(c);
    
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

/**
 * @brief Requests the XFixes extension to notify us of clipboard ownership changes.
 */
void SubscribeClipboardEvents(xcb_connection_t *c, xcb_window_t window) {
    xEntry1("SubscribeClipboardEvents");
    
    xcb_xfixes_query_version_cookie_t ck = xcb_xfixes_query_version(c, XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION);
    xcb_xfixes_query_version_reply_t *r = xcb_xfixes_query_version_reply(c, ck, NULL);
    if(r) free(r);

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

void SetClipboardData(xcb_connection_t *c, xcb_window_t win, void *data, size_t len, xcb_atom_t type) {
    xEntry1("SetClipboardData");
    
    long long Now = GetNowMs();
    if (TransactionLock) {
        if (Now - TransactionStartMs < TRANSACTION_TIMEOUT_MS) {
            xWarn("[SetClipboardData] TransactionLock active; discarding SetClipboardData request.");
            return;
        } else {
            xWarn("[SetClipboardData] TransactionLock timed out; forcing reset and accepting new data.");
            TransactionLock = 0;
            IncrRequestor = XCB_NONE;
            IncrProperty  = XCB_NONE;
            IncrTarget    = XCB_NONE;
            IncrOffset    = 0;
            IncrData      = NULL;
            IncrDataLen   = 0;
        }
    }

    if (ActiveData) {
        free(ActiveData);
    }
    
    ActiveData = malloc(len);
    if (!ActiveData) {
        xError("[SetClipboardData] Out of memory!");
        ActiveData = NULL;
        ActiveDataLen = 0;
        ActiveDataType = 0;
        return;
    }
    memcpy(ActiveData, data, len);
    ActiveDataLen = len;
    ActiveDataType = type;

    xcb_set_selection_owner(c, win, AtomClipboard, XCB_CURRENT_TIME);
    
    xcb_get_selection_owner_cookie_t ck = xcb_get_selection_owner(c, AtomClipboard);
    xcb_get_selection_owner_reply_t *r = xcb_get_selection_owner_reply(c, ck, NULL);
    
    if (r && r->owner == win) {
        xLog1("[SetClipboardData] Successfully claimed Clipboard ownership!");
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

void SignalEventHandler(int SigNum) {
    xLog1("[SignalEventHandler] Was called with SigNum=%d", SigNum);

    if (SigNum == SIGINT || SigNum == SIGTERM) {
        RequestExit = eACTIVATE;
        xLog1("[SignalEventHandler] Activate RequestExit!");
    } 
    else if (SigNum == SIGUSR1) {
        if (TogglePopUpStatus == eHIDEN || TogglePopUpStatus == eNOT_STARTED) {
            TogglePopUpStatus = eREQ_SHOW;
        } 
        else if (TogglePopUpStatus == eSHOWN) {
            TogglePopUpStatus = eREQ_HIDE;
        }
    }
    else if (SigNum == SIGUSR2) {
        xLog1("[SignalEventHandler] Injecting selected item into X11 Clipboard...");
        ReqTestInject = eACTIVATE;
        sem_post(&SemProviderWakeup);
    }
}

RetType RegisterSignal(void) {
    xEntry1("RegisterSignal");

    signal(SIGUSR2, SignalEventHandler); 
    signal(SIGUSR1, SignalEventHandler); 
    signal(SIGINT,  SignalEventHandler); 
    signal(SIGTERM, SignalEventHandler); 

    xLog1("[RegisterSignal] Listening for OS signals... (PID: %d)", getpid());

    xExit1("RegisterSignal");
    return OKE; 
}

RetType SignalRuntime(int Param) {
    (void)Param;
    xEntry1("SignalRuntime");

    RegisterSignal();

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
 * CLIPBOARD EVENT HANDLERS IMPLEMENTATION (FULL-TRANSACTION FORTRESS) ****************************
 **************************************************************************************************/

/**
 * @brief Handle XFixes selection notify when another app claims the clipboard.
 */
void HandleXFixesNotify(xcb_generic_event_t *Event) {
    xcb_xfixes_selection_notify_event_t *Sevent = (xcb_xfixes_selection_notify_event_t *)Event;
    if (Sevent->owner == MyWindow) return;

    long long Now = GetNowMs();

    /// [FORTRESS LOCK]: Ignore spamming if we are busy handling an active transaction
    if (TransactionLock) {
        if (Now - TransactionStartMs < TRANSACTION_TIMEOUT_MS) {
            xLog1("[XFixes] FORTRESS LOCK: Discarding new owner %u.", Sevent->owner);
            return;
        } else {
            xWarn("[XFixes] TIMEOUT: Previous transaction stuck. Breaking lock.");
            if (IncrRecvFile) { fclose(IncrRecvFile); IncrRecvFile = NULL; }
            IncrRecvOffset = 0;
            TotalBytesReceived = 0;
            IsReceivingIncr = 0;
            TransactionLock = 0;
        }
    }

    xLog1("[XFixes] New Owner %u. Locking transaction and cleaning property...", Sevent->owner);
    
    TransactionLock = 1; 
    TransactionStartMs = Now;
    CurrentTransactionTime = Sevent->timestamp;

    xcb_delete_property(Connection, MyWindow, AtomProperty);
    xcb_flush(Connection);

    xcb_convert_selection(Connection, MyWindow, AtomClipboard, AtomTarget, AtomProperty, CurrentTransactionTime);
    xcb_flush(Connection);
}

/**
 * @brief Handle format negotiation and select the best media type.
 */
static inline void HandleSelectionNotify_Negotiate(xcb_selection_notify_event_t *Nevent, void *Data, int ByteLen) {
    xcb_atom_t *Atoms = (xcb_atom_t *)Data;
    int Count = ByteLen / sizeof(xcb_atom_t);
    xcb_atom_t Target = XCB_ATOM_NONE;

    for (int i = 0; i < Count; i++) {
        if (Atoms[i] == AtomPng) { Target = AtomPng; break; }
        if (Atoms[i] == AtomJpeg) { Target = AtomJpeg; break; }
        if (Atoms[i] == AtomBmp) { Target = AtomBmp; break; }
        if (Atoms[i] == AtomUtf8 && Target == XCB_ATOM_NONE) Target = AtomUtf8;
    }

    if (Target != XCB_ATOM_NONE) {
        xLog1("[Negotiate] Chosen Target: %u. Requesting data...", Target);
        
        TransactionStartMs = GetNowMs(); /// Update heartbeat
        
        xcb_delete_property(Connection, MyWindow, AtomProperty);
        xcb_flush(Connection);
        
        xcb_convert_selection(Connection, MyWindow, AtomClipboard, Target, AtomProperty, CurrentTransactionTime);
        xcb_flush(Connection);
    } else {
        xWarn("[Negotiate] No supported target found. Unlocking.");
        FinalizeTransactionAndUnlock();
    }
}

/**
 * @brief Handles the initial response. Sets up the INCR RAM cache or saves Single-shot data.
 */
static inline void HandleSelectionNotify_ReceiveAndSave(xcb_selection_notify_event_t *Nevent, xcb_get_property_reply_t *reply, void *Data, int ByteLen) {
    const char *Ext = (Nevent->target == AtomPng) ? "png" : (Nevent->target == AtomJpeg) ? "jpg" : (Nevent->target == AtomBmp) ? "bmp" : "txt";
    
    GetUniqueFilename(IncrRecvFilename, sizeof(IncrRecvFilename), Ext);
    char FullPath[PATH_MAX];
    snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, IncrRecvFilename);
    
    if (IncrRecvFile) fclose(IncrRecvFile);
    IncrRecvFile = fopen(FullPath, "wb");
    
    if (!IncrRecvFile) {
        xError("[Receive] Failed to open file! Unlocking.");
        FinalizeTransactionAndUnlock();
        return;
    }

    IncrRecvOffset = 0;
    TotalBytesReceived = 0;

    if (reply->type == AtomIncr) {
        uint32_t SizeEst = 0;
        if (ByteLen >= 4) memcpy(&SizeEst, Data, 4);
        
        xLog1("[INCR] Started! Est Size: %u bytes. Processing to 128MB RAM Cache...", SizeEst);
        IsReceivingIncr = 1;
        TransactionStartMs = GetNowMs(); /// Update heartbeat

        xcb_delete_property(Connection, MyWindow, AtomProperty);
        xcb_flush(Connection);
    } 
    else {
        xLog1("[Single-shot] Received directly. Pumping to RAM Cache...");
        
        PushToCache((uint8_t *)Data, ByteLen);
        
        uint32_t BytesAfter = reply->bytes_after;
        uint32_t WordOffset = (ByteLen + 3) / 4; 

        /// Drain loop if X Server hides data in Single-shot
        while (BytesAfter > 0) {
            
            xcb_get_property_cookie_t ck = xcb_get_property(Connection, 0, MyWindow, AtomProperty, XCB_GET_PROPERTY_TYPE_ANY, WordOffset, 262144);
            xcb_get_property_reply_t *r = xcb_get_property_reply(Connection, ck, NULL);
            if (!r) break;
            
            int nLen = xcb_get_property_value_length(r);
            if (nLen > 0) {
                PushToCache(xcb_get_property_value(r), nLen);
                WordOffset += (nLen + 3) / 4;
            }
            BytesAfter = r->bytes_after;
            free(r);
        }
        
        xLog1("[Single-shot] DONE. Final size: %zu bytes.", TotalBytesReceived);
        
        xcb_delete_property(Connection, MyWindow, AtomProperty);
        xcb_flush(Connection);
        
        FinalizeTransactionAndUnlock();
    }
}

/**
 * @brief Handles Property change events (INCR chunk stream).
 */
void HandlePropertyNotify(xcb_generic_event_t *Event) {
    xcb_property_notify_event_t *PropEv = (xcb_property_notify_event_t *)Event;
    
    /// --- [RECEIVER MODE] ---
    if (IsReceivingIncr && PropEv->window == MyWindow && PropEv->atom == AtomProperty && PropEv->state == XCB_PROPERTY_NEW_VALUE) {
        
        TransactionStartMs = GetNowMs(); /// Update heartbeat to prevent timeout

        xcb_get_property_cookie_t ck = xcb_get_property(Connection, 0, MyWindow, AtomProperty, XCB_GET_PROPERTY_TYPE_ANY, 0, 262144);
        xcb_get_property_reply_t *r = xcb_get_property_reply(Connection, ck, NULL);
        
        if (r) {
            int ChunkLen = xcb_get_property_value_length(r);
            uint32_t BytesAfter = r->bytes_after;

            if (ChunkLen > 0) {
                PushToCache(xcb_get_property_value(r), ChunkLen);

                /// THE DRAIN: Exhaust the current X Server property before deleting it
                uint32_t WordOffset = (ChunkLen + 3) / 4;
                while (BytesAfter > 0) {
                    xcb_get_property_cookie_t nck = xcb_get_property(Connection, 0, MyWindow, AtomProperty, XCB_GET_PROPERTY_TYPE_ANY, WordOffset, 262144);
                    xcb_get_property_reply_t *nr = xcb_get_property_reply(Connection, nck, NULL);
                    if (!nr) break;

                    int nLen = xcb_get_property_value_length(nr);
                    if (nLen > 0) {
                        PushToCache(xcb_get_property_value(nr), nLen);
                        WordOffset += (nLen + 3) / 4;
                    }
                    BytesAfter = nr->bytes_after;
                    free(nr);
                }

                /// Signal the sender that we have exhausted the chunk
                xcb_delete_property(Connection, MyWindow, AtomProperty);
                xcb_flush(Connection);
            } 
            else {
                /// 0-byte chunk means EOF. Close transaction.
                xLog1("[INCR DONE] Total transferred: %zu bytes. Finalizing...", TotalBytesReceived);
                xcb_delete_property(Connection, MyWindow, AtomProperty);
                xcb_flush(Connection);
                
                FinalizeTransactionAndUnlock();
            }
            free(r);
        }
        return; 
    }
    
    /// --- [PROVIDER MODE] ---
    if (PropEv->state == XCB_PROPERTY_DELETE && PropEv->window == IncrRequestor && PropEv->atom == IncrProperty) {
        size_t BytesLeft = IncrDataLen - IncrOffset;
        if (BytesLeft > 0) {
            size_t ChunkSize = (BytesLeft > INCR_CHUNK_SIZE) ? INCR_CHUNK_SIZE : BytesLeft;
            
            /// @brief xcb_change_property changes a property on a window.
            /// @param Mode XCB_PROP_MODE_REPLACE overwrites the property.
            /// @param Format 8 (8-bit elements for binary stream).
            xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, IncrRequestor, IncrProperty, IncrTarget, 8, ChunkSize, (uint8_t *)IncrData + IncrOffset);
            IncrOffset += ChunkSize;
        } else {
            uint8_t EOF_D = 0;
            xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, IncrRequestor, IncrProperty, IncrTarget, 8, 0, &EOF_D);
            IncrRequestor = XCB_NONE;
            TransactionLock = 0; /// Unlock provider
        }
        xcb_flush(Connection);
    }
}

/**
 * @brief Handles Selection Notify events (Triggered when requested data arrives).
 */
void HandleSelectionNotify(xcb_generic_event_t *Event) {
    xcb_selection_notify_event_t *Nevent = (xcb_selection_notify_event_t *)Event;

    if (Nevent->property == XCB_NONE) {
        xWarn("[SelectionNotify] Conversion REJECTED. Unlocking.");
        xcb_delete_property(Connection, MyWindow, AtomProperty);
        xcb_flush(Connection);
        FinalizeTransactionAndUnlock();
        return;
    }

    xcb_get_property_cookie_t cookie = xcb_get_property(Connection, 0, MyWindow, AtomProperty, XCB_GET_PROPERTY_TYPE_ANY, 0, 2097152);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(Connection, cookie, NULL);

    if (reply) {
        int ByteLen = xcb_get_property_value_length(reply);
        void *Data = xcb_get_property_value(reply);

        if (ByteLen > 0) {
            if (Nevent->target == AtomTarget) {
                HandleSelectionNotify_Negotiate(Nevent, Data, ByteLen);
            }
            else if (Nevent->target == AtomUtf8 || Nevent->target == AtomPng || 
                     Nevent->target == AtomJpeg || Nevent->target == AtomBmp) {
                HandleSelectionNotify_ReceiveAndSave(Nevent, reply, Data, ByteLen);
            }
        } else {
            xWarn("[SelectionNotify] Empty property. Unlocking.");
            xcb_delete_property(Connection, MyWindow, AtomProperty);
            xcb_flush(Connection);
            FinalizeTransactionAndUnlock();
        }
        free(reply);
    } else {
        FinalizeTransactionAndUnlock();
    }
}

/**
 * @brief Handles Selection Request events, providing clipboard data to other apps.
 */
void HandleSelectionRequest(xcb_generic_event_t *Event) {
    xEntry1("HandleSelectionRequest");
    xcb_selection_request_event_t *Req = (xcb_selection_request_event_t *)Event;
    
    xcb_selection_notify_event_t Reply;
    memset(&Reply, 0, sizeof(Reply));
    Reply.response_type = XCB_SELECTION_NOTIFY;
    Reply.requestor     = Req->requestor;
    Reply.selection     = Req->selection;
    Reply.target        = Req->target;
    Reply.time          = Req->time;
    Reply.property      = XCB_NONE; 
    
    xcb_atom_t ValidProperty = (Req->property == XCB_NONE) ? Req->target : Req->property;

    if (Req->target == AtomTarget) { 
        xcb_atom_t SupportedTargets[] = { AtomTarget, AtomTimestamp, ActiveDataType };
        xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Req->requestor, ValidProperty, XCB_ATOM_ATOM, 32, 3, SupportedTargets);
        Reply.property = ValidProperty;
    }
    else if (Req->target == AtomTimestamp) {
        xcb_timestamp_t CurrentTime = Req->time; 
        xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Req->requestor, ValidProperty, XCB_ATOM_INTEGER, 32, 1, &CurrentTime);
        Reply.property = ValidProperty;
    }
    else if (Req->target == ActiveDataType && ActiveData != NULL) {
        if (ActiveDataLen > INCR_CHUNK_SIZE) {
            if (IncrRequestor != XCB_NONE) {
                xWarn("[HandleSelectionRequest] Provider busy. Rejecting req.");
                Reply.property = XCB_NONE;
            } else {
                IncrData = (uint8_t *)ActiveData; 
                IncrDataLen = ActiveDataLen; 
                IncrOffset = 0;
                IncrRequestor = Req->requestor; 
                IncrProperty = ValidProperty; 
                IncrTarget = ActiveDataType;

                uint32_t EventMask[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
                xcb_change_window_attributes(Connection, Req->requestor, XCB_CW_EVENT_MASK, EventMask);
                uint32_t TotalSize = ActiveDataLen;
                xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Req->requestor, ValidProperty, AtomIncr, 32, 1, &TotalSize);
                
                TransactionLock = 1; /// Lock provider transaction
                TransactionStartMs = GetNowMs();
                Reply.property = ValidProperty;
            }
        } else {
            xcb_change_property(Connection, XCB_PROP_MODE_REPLACE, Req->requestor, ValidProperty, ActiveDataType, 8, ActiveDataLen, ActiveData);
            Reply.property = ValidProperty;
        }
    }

    /// @brief xcb_send_event transmits an event directly to a client.
    /// @param Propagate Mask XCB_EVENT_MASK_NO_EVENT ensures targeted delivery.
    xcb_send_event(Connection, 0, Req->requestor, XCB_EVENT_MASK_NO_EVENT, (const char *)&Reply);
    xcb_flush(Connection);
    xExit1("HandleSelectionRequest");
}

/**
 * @brief Sends a dummy client message to unblock xcb_wait_for_event.
 * @note This is crucial for cleanly shutting down the Receiver thread which
 * would otherwise remain blocked indefinitely waiting for X11 activity.
 */
static inline void WakeUpReceiverThread(void) {
    if (Connection && MyWindow != XCB_NONE) {
        xcb_client_message_event_t DummyEvent;
        memset(&DummyEvent, 0, sizeof(DummyEvent));
        DummyEvent.response_type = XCB_CLIENT_MESSAGE;
        DummyEvent.format = 32;
        DummyEvent.window = MyWindow;
        DummyEvent.type = AtomTarget; /// Attach an arbitrary valid Atom

        /// @brief xcb_send_event forces the X Server to route an event directly to MyWindow
        xcb_send_event(Connection, 0, MyWindow, XCB_EVENT_MASK_NO_EVENT, (const char *)&DummyEvent);
        xcb_flush(Connection);
        xLog1("[Finalize] Sent Dummy Event to wake up Receiver thread.");
    }
}

/**************************************************************************************************
 * THREAD RUNTIME SECTION *************************************************************************
 **************************************************************************************************/

/**
 * @brief Provider Thread: Idles at 0% CPU waiting to inject data into the Clipboard.
 */
void* XClipboardRuntime_Provider(void* Param){
    (void)Param;
    xEntry1("XClipboardRuntime_Provider");
    
    /// Wait until the Receiver thread has fully initialized the XCB Connection and Window
    while(ReqWaitForSetup == eACTIVATE) {
        usleep(5U * 1000U); 
    }

    while (RequestExit != eACTIVATE) {
        /// [BLOCK-WAIT]: Completely suspend this thread, consuming 0% CPU.
        /// It will wake up when another thread calls sem_post(&SemProviderWakeup).
        sem_wait(&SemProviderWakeup); 
        
        if (RequestExit == eACTIVATE) break;

        if (ReqTestInject == eACTIVATE) {
            ReqTestInject = eDEACTIVATE;
            sClipboardItem LatestItem;
            
            if (XCBList_GetSelectedItem(&LatestItem) == OKE) {
                xcb_atom_t TargetAtom = AtomUtf8; 
                if (LatestItem.FileType == eFMT_IMG_PNG) TargetAtom = AtomPng;
                else if (LatestItem.FileType == eFMT_IMG_JGP) TargetAtom = AtomJpeg;
                else if (LatestItem.FileType == eFMT_IMG_BMP) TargetAtom = AtomBmp; 

                static uint8_t RawData[8U * 1024U * 1024U]; 
                char FullPath[PATH_MAX];
                snprintf(FullPath, sizeof(FullPath), "%s/%s", PATH_DIR_DB, LatestItem.Filename);
                struct stat FileStat;
                
                if (stat(FullPath, &FileStat) == 0 && FileStat.st_size > 0 && (size_t)FileStat.st_size <= sizeof(RawData)) {
                    int SelectedIdx = XCBList_GetSelectedNum();
                    if (XCBList_ReadAsBinary(SelectedIdx, RawData, sizeof(RawData)) >= 0) {
                        SetClipboardData(Connection, MyWindow, (void *)RawData, FileStat.st_size, TargetAtom);
                    }
                }
            }
        }
    }
    
    xExit1("XClipboardRuntime_Provider");
    return NULL;
}

/**
 * @brief Receiver Thread: Blocks continuously to catch events from the X Server.
 */
void* XClipboardRuntime_Receiver(void* Param) {
    (void)Param; 
    xEntry1("XClipboardRuntime_Receiver");

    Connection = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(Connection)) return NULL;

    const xcb_query_extension_reply_t *xfixes_data = xcb_get_extension_data(Connection, &xcb_xfixes_id);
    if (!xfixes_data || !xfixes_data->present) return NULL;
    uint8_t XFixesEventBase = xfixes_data->first_event;

    InitAtoms(Connection);
    MyWindow = CreateListenerWindow(Connection);
    SubscribeClipboardEvents(Connection, MyWindow);

    if(CheckSingleInstance(Connection, MyWindow) != OKE){
        xError("[XClipboardRuntime] Another app already started!");
        exit(-1);
    }

    /// Signal the Provider thread that X11 setup is complete
    ReqWaitForSetup = eDEACTIVATE; 
    xLog1("[XClipboardRuntime_Receiver] Setup Done. Listening for events...");

    xcb_generic_event_t *Event;

    while (RequestExit != eACTIVATE) {
        
        /// [BLOCK-WAIT]: Suspend execution here until an Event (or the exit Dummy Event) arrives
        Event = xcb_wait_for_event(Connection);
        if (Event == NULL) {
            if (xcb_connection_has_error(Connection)) {
                xError("[XClipboardRuntime_Receiver] Connection has an error!");
                break;
            }
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
        /// Note: Dummy events (XCB_CLIENT_MESSAGE) used for waking up the thread are safely ignored here.

        free(Event);
    }

    xcb_disconnect(Connection);
    xExit1("XClipboardRuntime_Receiver");
    return NULL;
}

/**************************************************************************************************
 * LIFECYCLE SECTION IMPLEMENTATION ***************************************************************
 **************************************************************************************************/ 

void ClipboardCaptureFinalize(void) {
    xLog1("[Finalize] Initiating shutdown sequence...");
    
    /// 1. Raise the exit flag for the entire system
    RequestExit = eACTIVATE;

    /// 2. Wake up the Signal Thread (if it is suspended in pause())
    int SigKillStatus = pthread_kill(SignalRuntimeThread, SIGINT);
    if (SigKillStatus == 0) {
        pthread_join(SignalRuntimeThread, NULL);
    }

    /// 3. Wake up the Provider Thread (via Semaphore)
    sem_post(&SemProviderWakeup);
    pthread_join(XClipboardRuntimeThread_Provider, NULL);
    xLog1("[Finalize] Provider Thread joined.");

    /// 4. Wake up the Receiver Thread (via Dummy X11 Event)
    WakeUpReceiverThread();
    pthread_join(XClipboardRuntimeThread_Receiver, NULL);
    xLog1("[Finalize] Receiver Thread joined.");
    
    if (ActiveData != NULL) {
        free(ActiveData);
        ActiveData = NULL;
    }
    if (IncrRecvBuf) { 
        free(IncrRecvBuf); 
        IncrRecvBuf = NULL; 
    }
    
    /// Cleanup the Semaphore resource
    sem_destroy(&SemProviderWakeup);
}

RetType ClipboardCaptureInitialize(void) {
    atexit(ClipboardCaptureFinalize);

    if (EnsureDB() != OKE) return ERR;
    if (XCBList_Scan(0) < 0) return ERR;

    /// Initialize the Semaphore (pshared = 0, initial_value = 0 to start in a blocking state)
    sem_init(&SemProviderWakeup, 0, 0);

    IncrRecvBuf = malloc(MAX_RAM_CACHE);
    if (!IncrRecvBuf) {
        xError("[Initialize] FATAL: Out of memory for 128MB RAM Cache!");
        return ERR;
    }

    /// Spawn the three independent application threads
    if (pthread_create(&SignalRuntimeThread, NULL, (void *(*)(void *))SignalRuntime, NULL) != 0) return ERR;
    if (pthread_create(&XClipboardRuntimeThread_Provider, NULL, (void *(*)(void *))XClipboardRuntime_Provider, NULL) != 0) return ERR;
    if (pthread_create(&XClipboardRuntimeThread_Receiver, NULL, (void *(*)(void *))XClipboardRuntime_Receiver, NULL) != 0) return ERR;

    xLog1("[Initialize] Started. Threads Online. 128MB RAM Cache Online.");
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
                        sem_post(&SemProviderWakeup);
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
