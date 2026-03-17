/*
 * Copyright (C) 2023 Codership Oy <info@codership.com>
 *
 * This file is part of wsrep-lib.
 *
 * Wsrep-lib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Wsrep-lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wsrep-lib.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef WSREP_ASSERT_HPP
#define WSREP_ASSERT_HPP

/** @file assert.hpp
 *
 * Wsrep library informative assert.
 */

#include <assert.h>

/**
 * Function to do informative assert with actual file name,
 * line and formatted info text.
 *
 * @param expr Assertion expression string
 * @param file File name string
 * @param line File line number
 * @param info Formatted info text
 *
*/
void wsrep_info_assert(const char* expr, const char* file, const int line, const char* info);

/**
 * Function to create informative text from format
 * and variable number of parameters
 *
 * @param format strict
 * @param ... variable number of parameters
 *
 * @retval Formated informative text
 *
*/
char* wsrep_info_assert_text(const char* format, ...);

/**
 * Macro for informative assert. First actual expression
 * is printed and then informative text provided by
 * developer.
 *
 * @param exp assertion expression
 * @param p Informative text format and parameters 
 *
 * usage : WSREP_INFO_ASSERT((EXPR), (format, format_param,...))
 * example : WSREP_INFO_ASSERT(0, ("This is always false=%d", 0));
 *
*/
#ifdef NDEBUG
#define WSREP_INFO_ASSERT(exp, p) (__ASSERT_VOID_CAST (0))
#else
#define WSREP_INFO_ASSERT(exp, p) do {                     \
  if (__builtin_expect((!(exp)), false))                   \
  {                                                        \
    wsrep_info_assert(#exp, __FILE__, __LINE__,            \
                      wsrep_info_assert_text p);           \
    assert(exp);                                           \
  }                                                        \
} while (0)
#endif // NDEBUG

#endif // WSREP_ASSERT_HPP
