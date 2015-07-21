/*
 * Copyright (C) 2000, Matias Atria
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#ifndef _SYSDEP_H
#define _SYSDEP_H 1

typedef unsigned long    Ulong;
typedef unsigned int    Uint;
typedef unsigned short    Ushort;
typedef unsigned char    Uchar;

/* this one's easy */
typedef unsigned char    Uint8;
typedef char        Int8;

typedef unsigned int    Uint32;
typedef int        Int32;

typedef unsigned short    Uint16;
typedef short        Int16;

typedef unsigned long    UINT;
//typedef long        INT;

/* nice, uh? */
typedef void    *Pointer;

/* macros to do the safe pointer <-> integer conversions */
#define Ptr2Int(x)    ((long)((Pointer)(x)))
#define Int2Ptr(x)    ((Pointer)((long)(x)))

#define __PROTO(x)    x

#endif    /* _SYSDEP_H */
