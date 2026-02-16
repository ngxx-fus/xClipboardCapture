#ifndef __SETUP_H__
#define __SETUP_H__

/**************************************************************************************************
 * MACRO CONFIGURATIONS SECTION *******************************************************************
 **************************************************************************************************/ 

/**
 * @brief Official program name used for logging and metadata.
 */
#define PROG_NAME               "ClipboardCapture v0.0.1"

/**
 * @brief Unique string ID for X11 internal property exchange.
 */
#define PROP_NAME               "NGXX_FUS_CLIPBOARD_PROP"

/**
 * @brief Maximum number of items retained in the clipboard history ring buffer.
 */
#define MAX_HISTORY_ITEMS       1000 

/**
 * @brief Root directory for all temporary runtime files.
 */
#define PATH_DIR_ROOT           "/home/fus/.fus/.XCBC_Data"

/**
 * @brief Sub-directory storing the raw clipboard data chunks/files.
 */
#define PATH_DIR_DB             PATH_DIR_ROOT "/DBs"

/**
 * @brief Path to the file storing the serialized clipboard history list.
 */
#define PATH_ITEM               PATH_DIR_ROOT "/ClipboardItem"

/**
 * @brief Path to the temporary text file used to feed the Rofi menu.
 */
#define PATH_FILE_ROFI_MENU     PATH_DIR_ROOT "/XCBRofiMenu.txt"

/**
 * @brief Toggle switch to enable (1) or disable (0) Rofi UI integration.
 */
#define ROFI_SUPPORT            1

/**
 * @brief Maximum character length for text previews in the Rofi menu.
 */
#define PREVIEW_TXT_LEN         80

#endif /*__SETUP_H__*/

/**************************************************************************************************
 * EOF ********************************************************************************************
 **************************************************************************************************/

