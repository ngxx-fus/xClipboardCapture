#ifndef __X_UNIVERSAL_RETURN_H__
#define __X_UNIVERSAL_RETURN_H__

#include <stdio.h>
#include <stdint.h>

/// Default return type
typedef int32_t RetType;

//
// ──────────────────────────────────────────────────────────────
//   String representations for errors
// ──────────────────────────────────────────────────────────────
//

#define STR_OKE                     "OKE"
#define STR_ERR                     "ERR"

#define STR_ERR_NULL                "ERR_NULL"
#define STR_ERR_MALLOC_FAILED       "ERR_MALLOC_FAILED"
#define STR_ERR_TIMEOUT             "ERR_TIMEOUT"
#define STR_ERR_BUSY                "ERR_BUSY"
#define STR_ERR_INVALID_ARG         "ERR_INVALID_ARG"
#define STR_ERR_OVERFLOW            "ERR_OVERFLOW"
#define STR_ERR_UNDERFLOW           "ERR_UNDERFLOW"
#define STR_ERR_NOT_FOUND           "ERR_NOT_FOUND"
#define STR_ERR_ALREADY_EXISTS      "ERR_ALREADY_EXISTS"
#define STR_ERR_NOT_IMPLEMENTED     "ERR_NOT_IMPLEMENTED"
#define STR_ERR_UNSUPPORTED         "ERR_UNSUPPORTED"
#define STR_ERR_IO                  "ERR_IO"
#define STR_ERR_PERMISSION          "ERR_PERMISSION"
#define STR_ERR_CRC                 "ERR_CRC"
#define STR_ERR_INIT_FAILED         "ERR_INIT_FAILED"
#define STR_ERR_PSRAM_FAILED        "ERR_PSRAM_FAILED"

// ─ SDL / Xorg / Platform-specific
#define STR_ERR_SDL_INIT_FAILED     "ERR_SDL_INIT_FAILED"
#define STR_ERR_SDL_WINDOW_FAILED   "ERR_SDL_WINDOW_FAILED"
#define STR_ERR_SDL_RENDER_FAILED   "ERR_SDL_RENDER_FAILED"
#define STR_ERR_SDL_TTF_FAILED      "ERR_SDL_TTF_FAILED"
#define STR_ERR_SDL_IMAGE_FAILED    "ERR_SDL_IMAGE_FAILED"

#define STR_ERR_XORG_DISPLAY_FAILED "ERR_XORG_DISPLAY_FAILED"
#define STR_ERR_XORG_WINDOW_FAILED  "ERR_XORG_WINDOW_FAILED"
#define STR_ERR_XORG_DRAW_FAILED    "ERR_XORG_DRAW_FAILED"

#define STR_ERR_FILE_NOT_FOUND      "ERR_FILE_NOT_FOUND"
#define STR_ERR_FILE_READ_FAILED    "ERR_FILE_READ_FAILED"
#define STR_ERR_FILE_WRITE_FAILED   "ERR_FILE_WRITE_FAILED"
#define STR_ERR_PATH_INVALID        "ERR_PATH_INVALID"

//
// ──────────────────────────────────────────────────────────────
//   Enumerated return codes
// ──────────────────────────────────────────────────────────────
//

enum DEFAULT_RETURN_STATUS {
    OKE                     = 0,    ///< Success
    ERR                     = -1,   ///< Generic error
    ERR_NULL                = -2,   ///< Null pointer
    ERR_MALLOC_FAILED       = -3,   ///< Memory allocation failed
    ERR_TIMEOUT             = -4,   ///< Timeout occurred
    ERR_BUSY                = -5,   ///< Resource busy
    ERR_INVALID_ARG         = -6,   ///< Invalid argument
    ERR_OVERFLOW            = -7,   ///< Buffer overflow
    ERR_UNDERFLOW           = -8,   ///< Buffer underflow
    ERR_NOT_FOUND           = -9,   ///< Resource not found
    ERR_ALREADY_EXISTS      = -10,  ///< Resource already exists
    ERR_NOT_IMPLEMENTED     = -11,  ///< Feature not implemented
    ERR_UNSUPPORTED         = -12,  ///< Operation unsupported
    ERR_IO                  = -13,  ///< Input/output error
    ERR_PERMISSION          = -14,  ///< Permission denied
    ERR_CRC                 = -15,  ///< CRC mismatch
    ERR_INIT_FAILED         = -16,  ///< Initialization failed
    ERR_PSRAM_FAILED        = -17,  ///< PSRAM init failed

    // SDL-related
    ERR_SDL_INIT_FAILED     = -30,  ///< SDL initialization failed
    ERR_SDL_WINDOW_FAILED   = -31,  ///< SDL window creation failed
    ERR_SDL_RENDER_FAILED   = -32,  ///< SDL renderer creation failed
    ERR_SDL_TTF_FAILED      = -33,  ///< SDL_ttf font load/render failed
    ERR_SDL_IMAGE_FAILED    = -34,  ///< SDL_image failed to load file

    // Xorg-related
    ERR_XORG_DISPLAY_FAILED = -40,  ///< X display connection failed
    ERR_XORG_WINDOW_FAILED  = -41,  ///< X window creation failed
    ERR_XORG_DRAW_FAILED    = -42,  ///< X drawing operation failed

    // File & path
    ERR_FILE_NOT_FOUND      = -50,  ///< File not found
    ERR_FILE_READ_FAILED    = -51,  ///< File read error
    ERR_FILE_WRITE_FAILED   = -52,  ///< File write error
    ERR_PATH_INVALID        = -53,  ///< Invalid file path
};

//
// ──────────────────────────────────────────────────────────────
//   Helper: Convert error code to string
// ──────────────────────────────────────────────────────────────
//

/// @brief Get a string for a given return code.
/// @param ret Error/status code
/// @return String representation (never NULL)
static inline const char * DEFAULT_RETURN_STATUS_STR(enum DEFAULT_RETURN_STATUS ret)
{
    switch (ret) {
        case OKE:                     return STR_OKE;
        case ERR:                     return STR_ERR;
        case ERR_NULL:                return STR_ERR_NULL;
        case ERR_MALLOC_FAILED:       return STR_ERR_MALLOC_FAILED;
        case ERR_TIMEOUT:             return STR_ERR_TIMEOUT;
        case ERR_BUSY:                return STR_ERR_BUSY;
        case ERR_INVALID_ARG:         return STR_ERR_INVALID_ARG;
        case ERR_OVERFLOW:            return STR_ERR_OVERFLOW;
        case ERR_UNDERFLOW:           return STR_ERR_UNDERFLOW;
        case ERR_NOT_FOUND:           return STR_ERR_NOT_FOUND;
        case ERR_ALREADY_EXISTS:      return STR_ERR_ALREADY_EXISTS;
        case ERR_NOT_IMPLEMENTED:     return STR_ERR_NOT_IMPLEMENTED;
        case ERR_UNSUPPORTED:         return STR_ERR_UNSUPPORTED;
        case ERR_IO:                  return STR_ERR_IO;
        case ERR_PERMISSION:          return STR_ERR_PERMISSION;
        case ERR_CRC:                 return STR_ERR_CRC;
        case ERR_INIT_FAILED:         return STR_ERR_INIT_FAILED;
        case ERR_PSRAM_FAILED:        return STR_ERR_PSRAM_FAILED;

        case ERR_SDL_INIT_FAILED:     return STR_ERR_SDL_INIT_FAILED;
        case ERR_SDL_WINDOW_FAILED:   return STR_ERR_SDL_WINDOW_FAILED;
        case ERR_SDL_RENDER_FAILED:   return STR_ERR_SDL_RENDER_FAILED;
        case ERR_SDL_TTF_FAILED:      return STR_ERR_SDL_TTF_FAILED;
        case ERR_SDL_IMAGE_FAILED:    return STR_ERR_SDL_IMAGE_FAILED;

        case ERR_XORG_DISPLAY_FAILED: return STR_ERR_XORG_DISPLAY_FAILED;
        case ERR_XORG_WINDOW_FAILED:  return STR_ERR_XORG_WINDOW_FAILED;
        case ERR_XORG_DRAW_FAILED:    return STR_ERR_XORG_DRAW_FAILED;

        case ERR_FILE_NOT_FOUND:      return STR_ERR_FILE_NOT_FOUND;
        case ERR_FILE_READ_FAILED:    return STR_ERR_FILE_READ_FAILED;
        case ERR_FILE_WRITE_FAILED:   return STR_ERR_FILE_WRITE_FAILED;
        case ERR_PATH_INVALID:        return STR_ERR_PATH_INVALID;

        default:                      return "ERR_UNKNOWN";
    }
}

#endif /* __RETURN_H__ */
