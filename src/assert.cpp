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

/** @file assert.cpp
 *
 * Wsrep library informative assert.
 */

#include <cassert>
#include <cstring>
#include <stdarg.h>
#include <stdio.h>
#include "wsrep/logger.hpp"

/**
 * Function to create informative text from format
 * and variable number of parameters
 *
 * @param format text format strict
 * @param ... variable number of parameters
 *
 * @retval Formated informative text
 *
*/
#define WSREP_MAX_INFO_BUF 1024

char* wsrep_info_assert_text(const char* format, ...)
{
  static char wsrep_buf[WSREP_MAX_INFO_BUF];
  va_list ap;

  va_start(ap, format);
  int n= vsnprintf(wsrep_buf, WSREP_MAX_INFO_BUF,(char *)format, ap);
  if (n >= WSREP_MAX_INFO_BUF)
  {
    wsrep::log_debug() << "wsrep_info_assert_text too long and truncated"
                       << " used length: " << WSREP_MAX_INFO_BUF
                       << " needed lenght: " << n;
  }
  va_end(ap);
  return wsrep_buf;
}

/**
 * Function to do informative assert with actual file name,
 * line and formatted info text. Function will not return.
 *
 * @param file File name string
 * @param line File line number
 * @param info Formatted info text
 *
*/
void wsrep_info_assert(const char* expr, const char* file, const int line, const char* info)
{
  wsrep::log_error() << "Assertion: '" << expr << "' failed in: " << file << ":" << line
                     << " Info: " << info;
}
