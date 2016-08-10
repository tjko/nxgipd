/****************************************************************
 * strl-funcs.h
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

#ifndef STRL_FUNCS_H
#define STRL_FUNCS_H 1

#ifdef  __cplusplus
extern "C" {
#endif


size_t strlcpy(char*, const char*, size_t);
size_t strlcat(char*, const char*, size_t);


#ifdef  __cplusplus
}
#endif

#endif /* STRL_FUNCS_H */
