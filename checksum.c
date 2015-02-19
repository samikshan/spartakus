
/*
 * This code has been taken from the source code of genksyms which is available
 * with the linux kernel source code.
 *
 * Copyright (C) 2014  Red Hat Inc.
 * Author: Samikshan Bairagya <sbairagy@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "checksum.h"

unsigned long partial_crc32_one(unsigned char c, unsigned long crc)
{
    return crctab32[(crc ^ c) & 0xff] ^ (crc >> 8);
}

unsigned long partial_crc32(const char *s, unsigned long crc)
{
    while (*s)
        crc = partial_crc32_one(*s++, crc);
    return crc;
}

unsigned long crc32(const char *s, unsigned long int crc)
{
    crc = partial_crc32(s, crc);
    crc = partial_crc32_one(' ', crc);
    return crc;
}

unsigned long raw_crc32(const char *s)
{
    return partial_crc32(s, 0xffffffff) ^ 0xffffffff;
}
