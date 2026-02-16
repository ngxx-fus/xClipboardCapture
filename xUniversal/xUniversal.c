#include "xUniversal.h" 

/// @brief Static initialization of the global log mutex
pthread_mutex_t logMutex = PTHREAD_MUTEX_INITIALIZER;

/// @brief Implementation of logger locking mechanism
void xLockLogger(pthread_mutex_t* mutex) {
    int rc = pthread_mutex_lock(mutex);
    if (rc) {
        fprintf(stderr, "[err] [critical section] Lock failed: %d\n", rc);
    }
}

/// @brief Implementation of logger unlocking mechanism
void xUnlockLogger(pthread_mutex_t* mutex) {
    int rc = pthread_mutex_unlock(mutex);
    if (rc) {
        fprintf(stderr, "[err] [critical section] Unlock failed: %d\n", rc);
    }
}

/// @brief Handles internal logging logic including timestamps and formatting
void xCoreLog(const char* tag, const char* format, ...) {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm tm_buf;
    localtime_r(&tv.tv_sec, &tm_buf);

    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);

    va_list args;
    va_start(args, format);

    xLockLogger(&logMutex);
    
    /// Print timestamp and tag
    fprintf(stderr, "[%s.%06ld] [%s] ", time_str, tv.tv_usec, tag);
    
    /// Print the actual log message
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    
    xUnlockLogger(&logMutex);

    va_end(args);
}
