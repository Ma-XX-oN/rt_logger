/**
 * @file ThrowNoThrow.hpp
 * @author Adrian Hawryluk (adrian.hawryluk@gmail.com)
 * @brief Tagging structures to indicate if to use a throwing or non-throwing
 *   instance of a function.
 * @version 0.1
 * @date 2026-05-21
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef THROW_NO_THROW_HPP
#define THROW_NO_THROW_HPP

/**
 * @brief Indicates that the function will throw on error.
 */
struct Throw {};

/**
 * @brief Indicates that the function will NOT throw on error.
 */
struct NoThrow {};

#endif // THROW_NO_THROW_HPP
