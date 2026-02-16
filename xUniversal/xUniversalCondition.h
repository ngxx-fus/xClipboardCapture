#ifndef __X_UNIVERSAL_HELPER_CONDITION_H_
#define __X_UNIVERSAL_HELPER_CONDITION_H_

/**
 * @file helper_condition.h
 * @brief Utility macros for common operations, including loops, delays, 
 * color manipulation, and bit mask operations.
 */

#ifdef LOG_HEADER_INCLUDE
#pragma message("INCLUDE: helper_condition.h")
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <limits.h>

/**
 * @brief Return the maximum of two values.
 * @note Warning: Potential double evaluation of parameters.
 */
#define Max(a, b) ((a) > (b) ? (a) : (b))

/**
 * @brief Return the minimum of two values.
 * @note Warning: Potential double evaluation of parameters.
 */
#define Min(a, b) ((a) < (b) ? (a) : (b))

/**
 * @brief Return the absolute value of a number.
 * @note Warning: Potential double evaluation of parameters.
 */
#define Abs(x) ((x) >= 0 ? (x) : -(x))

/**
 * @brief Check if a pointer is NULL.
 */
#define IsNull(ptr) ((ptr) == NULL)

/**
 * @brief Check if a pointer is not NULL.
 */
#define IsNotNull(ptr) ((ptr) != NULL)

/**
 * @brief Check if a value is zero.
 */
#define IsZero(x) ((x) == 0)

/**
 * @brief Check if a value is positive.
 */
#define IsPositive(x) ((x) > 0)

/**
 * @brief Check if a value is negative.
 */
#define IsNegative(x) ((x) < 0)

/**
 * @brief Check if a value is not zero.
 */
#define IsNotZero(x) ((x) != 0)

/**
 * @brief Check if a value is not positive.
 */
#define IsNotPositive(x) ((x) <= 0)

/**
 * @brief Check if a value is not negative.
 */
#define IsNotNegative(x) ((x) >= 0)

#endif /* HELPER_CONDITION_H_ */
