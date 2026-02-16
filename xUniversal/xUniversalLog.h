#ifndef __LOG_AND_DEBUG_H__
#define __LOG_AND_DEBUG_H__ 

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stddef.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XLOG_EN
    #define XLOG_EN                     1
#endif
#ifndef XLOG_LEVEL
    #define XLOG_LEVEL                  1 /*MIN:Lv0 to MAX:Lv2*/
#endif

extern pthread_mutex_t  logMutex;   /*Mutex lock for logging lock*/

void xLockLogger(pthread_mutex_t* mutex);
void xUnlockLogger(pthread_mutex_t* mutex);
void xCoreLog(const char* tag, const char* format, ...)
     __attribute__((format(printf, 2, 3)));

/// Always on
#define xError(...)	    xCoreLog("error",   __VA_ARGS__)
#define xWarn(...)	    xCoreLog("WARN",    __VA_ARGS__)

#if (XLOG_LEVEL >= 0) && (XLOG_EN == 1)
    #define xLog(...)	    xCoreLog("LOG",     __VA_ARGS__)
    #define xEntry(...)	    xCoreLog("-->",     __VA_ARGS__)
    #define xExit(...)	    xCoreLog("<--",     __VA_ARGS__)
#else 
    #define xLog(...)
    #define xEntry(...)
    #define xExit(...)
#endif


#if (XLOG_LEVEL >= 1) && (XLOG_EN == 1)
    #define xLog1(...)	    xCoreLog("LOG",     __VA_ARGS__)
    #define xEntry1(...)	xCoreLog("-->",     __VA_ARGS__)
    #define xExit1(...)	    xCoreLog("<--",     __VA_ARGS__)
#else
    #define xLog1(...)
    #define xEntry1(...)
    #define xExit1(...)
#endif 

#if (XLOG_LEVEL >= 2) && (XLOG_EN == 1)
    #define xLog2(...)	    xCoreLog("LOG",     __VA_ARGS__)
    #define xEntry2(...)	xCoreLog("-->",     __VA_ARGS__)
    #define xExit2(...)	    xCoreLog("<--",     __VA_ARGS__)
#else
    #define xLog2(...)
    #define xEntry2(...)
    #define xExit2(...)
#endif 


#ifdef __cplusplus
}
#endif

#else 
    #pragma error("Duplicated include of LogAndDebug.h!")
#endif /// __LOG_AND_DEBUG_H__
