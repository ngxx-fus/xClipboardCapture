#ifndef __HELPER_LOOP_H__
#define __HELPER_LOOP_H__

/**
 * @brief Loop macro for iterating over a range with a specified type.
 * @param type The data type of the loop variable.
 * @param i The loop variable.
 * @param a The starting value (inclusive).
 * @param b The ending value (exclusive).
 * @example REPT(int, i, 0, 5) iterates i from 0 to 4.
 */
#ifndef REPT
    #define REPT(type, i, a, b) for(type i = (a); (i) < (b); ++(i))
#endif

/**
 * @brief Reverse loop macro for iterating downward with a specified type.
 * @param type The data type of the loop variable.
 * @param i The loop variable.
 * @param a The starting value (inclusive).
 * @param b The ending value (exclusive).
 * @example REVT(int, i, 5, 0) iterates i from 5 to 1.
 */
#ifndef REVT
    #define REVT(type, i, a, b) for(type i = (a); (i) > (b); --(i))
#endif

/**
 * @brief Loop macro for iterating over a range using size_t.
 * @param i The loop variable.
 * @param a The starting value (inclusive).
 * @param b The ending value (exclusive).
 * @note Assumes size_t is defined elsewhere (e.g., in global.h).
 */
#ifndef REP
    #define REP(i, a, b) for(size_t i = (a); (i) < (b); ++(i))
#endif

/**
 * @brief Reverse loop macro for iterating downward using size_t.
 * @param i The loop variable.
 * @param a The starting value (inclusive).
 * @param b The ending value (exclusive).
 * @note Assumes size_t is defined elsewhere (e.g., in global.h).
 */
#ifndef REV
    #define REV(i, a, b) for(size_t i = (a); (i) > (b); --(i))
#endif



#endif ///__HELPER_LOOP_H__
