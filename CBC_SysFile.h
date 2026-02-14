
#ifndef __CBC_SYSFILE_H__
#define __CBC_SYSFILE_H__

#include "CBC_Setup.h"

/**************************************************************************************************
 * SYSTEMCALL HELPER SECTION PROTOTYPES ***********************************************************
 **************************************************************************************************/ 

/** * @brief Handle and log errors occurred during directory creation.
 * @param FunctionName Name of the caller function for tracing.
 * @param Path The directory path that failed to be created.
 * @param ErrorCode The errno value captured after mkdir() failure.
 */
void PrintError_Mkdir(const char FunctionName[], const char Path[], int ErrorCode);

/** * @brief Check for directory existence and create it if missing.
 * @param path The string path of the directory to ensure.
 * @return OKE on success or if exists, otherwise returns error code from mkdir.
 */
RetType EnsureDir(char path[]);

/** * @brief Recursively removes a directory and all its contents.
 * @param path The directory path to be removed.
 * @return OKE on success, ERR on failure.
 */
RetType RemoveDir(const char path[]);

/** * @brief Initialize the database directory structure.
 * @return OKE if all directories are ready, error code otherwise.
 */
RetType EnsureDB(void);

/**
 * @brief Simple helper to save text data to file (Demostration).
 * @param data The data buffer.
 * @param len Length of data.
 */
void SaveClipboardToFile(const char* data, int len);

#endif /*__CBC_SYSFILE_H__*/
