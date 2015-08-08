/****************************************************************
 * strl-funcs.c
 *
 * Implementation of OpenBSD's strlcat() and strlcpy() 
 * functions. This was created based on the man page describing
 * usage of these functions.
 *
 * Copyright (C) 2015 Timo Kokkonen <tjko@iki.fi>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA  02110-1301, USA. 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string.h>


#ifndef HAVE_STRLCPY

size_t strlcpy(char *dst, const char *src, size_t dstsize)
{
  size_t srclen = strlen(src);
  size_t tocopy;

  /* do nothing, if no space even for the NUL... */
  if (dstsize == 0)
    return srclen;

  tocopy=(srclen < dstsize ? srclen : dstsize - 1);
  if (tocopy > 0)
    memcpy(dst, src, tocopy);
  dst[tocopy] = 0;
  
  return srclen;
}

#endif


#ifndef HAVE_STRLCAT

size_t strlcat(char *dst, const char *src, size_t dstsize)
{
  size_t dstlen = strlen(dst);
  size_t srclen = strlen(src);
  size_t dstfree = dstsize - dstlen;
  size_t tocopy;

  /* do nothing if dst is not NUL-terminated (no space to add anything...) */
  if (dstsize <= dstlen)
    return dstlen + srclen;

  tocopy = (srclen < dstfree ? srclen : dstfree - 1);
  if (tocopy > 0)
    memcpy(dst + dstlen, src, tocopy);
  dst[dstlen + tocopy] = 0;

  return dstlen + srclen;
}

#endif


/* eof :-) */
