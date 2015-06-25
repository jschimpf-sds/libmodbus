/*
 * Copyright © 2010-2011 Stéphane Raimbault <stephane.raimbault@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include <stdlib.h>
#ifndef _MSC_VER
#include <stdint.h>
#else
#include "stdint.h"
#endif
#include <string.h>
#include <assert.h>

#include "modbus.h"

#if defined(HAVE_BYTESWAP_H)
#  include <byteswap.h>
#endif

#if defined(__GNUC__)
#  define GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__ * 10)
#  if GCC_VERSION >= 430
// Since GCC >= 4.30, GCC provides __builtin_bswapXX() alternatives so we switch to them
#    undef bswap_32
#    define bswap_32 __builtin_bswap32
#  endif
#endif
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
# define bswap_32 _byteswap_ulong
#endif

#if !defined(bswap_32)

#if !defined(bswap_16)
#   warning "Fallback on C functions for bswap_16"
static inline uint16_t bswap_16(uint16_t x)
{
    return (x >> 8) | (x << 8);
}
#endif

#   warning "Fallback on C functions for bswap_32"
static inline uint32_t bswap_32(uint32_t x)
{
    return (bswap_16(x & 0xffff) << 16) | (bswap_16(x >> 16));
}
#endif

/* Sets many bits from a single byte value (all 8 bits of the byte value are
   set) */
void modbus_set_bits_from_byte(uint8_t *dest, int idx, const uint8_t value)
{
    int i;

    for (i=0; i < 8; i++) {
        dest[idx+i] = (value & (1 << i)) ? 1 : 0;
    }
}

/* Sets many bits from a table of bytes (only the bits between idx and
   idx + nb_bits are set) */
void modbus_set_bits_from_bytes(uint8_t *dest, int idx, unsigned int nb_bits,
                                const uint8_t *tab_byte)
{
    unsigned int i;
    int shift = 0;

    for (i = idx; i < idx + nb_bits; i++) {
        dest[i] = tab_byte[(i - idx) / 8] & (1 << shift) ? 1 : 0;
        /* gcc doesn't like: shift = (++shift) % 8; */
        shift++;
        shift %= 8;
    }
}

/* Gets the byte value from many bits.
   To obtain a full byte, set nb_bits to 8. */
uint8_t modbus_get_byte_from_bits(const uint8_t *src, int idx,
                                  unsigned int nb_bits)
{
    unsigned int i;
    uint8_t value = 0;

    if (nb_bits > 8) {
        /* Assert is ignored if NDEBUG is set */
        assert(nb_bits < 8);
        nb_bits = 8;
    }

    for (i=0; i < nb_bits; i++) {
        value |= (src[idx+i] << i);
    }

    return value;
}

/* Get a float from 4 bytes in Modbus format (ABCD) */
float modbus_get_float(const uint16_t *src)
{
    float f;
    uint32_t i;

    i = (((uint32_t)src[1]) << 16) + src[0];
    memcpy(&f, &i, sizeof(float));

    return f;
}

/* Get a float from 4 bytes in inversed Modbus format (DCBA) */
float modbus_get_float_dcba(const uint16_t *src)
{
    float f;
    uint32_t i;

    i = bswap_32((((uint32_t)src[1]) << 16) + src[0]);
    memcpy(&f, &i, sizeof(float));

    return f;
}

/* Set a float to 4 bytes in Modbus format (ABCD) */
void modbus_set_float(float f, uint16_t *dest)
{
    uint32_t i;

    memcpy(&i, &f, sizeof(uint32_t));
    dest[0] = (uint16_t)i;
    dest[1] = (uint16_t)(i >> 16);
}

/* Set a float to 4 bytes in inversed Modbus format (DCBA) */
void modbus_set_float_dcba(float f, uint16_t *dest)
{
    uint32_t i;

    memcpy(&i, &f, sizeof(uint32_t));
    i = bswap_32(i);
    dest[0] = (uint16_t)i;
    dest[1] = (uint16_t)(i >> 16);
}

//------------------------------------------
// MODBUS ASCII

/* Convert binary nibble -> ASCII

	INPUT	byte	- Binary 0,15
	
	OUTPUT:	nibble	- ASCII (0-F) of binary
	
*/

static uint8_t nib_to_ASCII(char byte)
{
	uint8_t rtnval = 0;

	switch( byte )
	{
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
				rtnval = byte | 0x30;
				break;

		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
				rtnval = byte + 0x37;
				break;
	}

	return rtnval;
}

/* 
	Convert a byte -> buffer with 2 ASCII nibles
	
	INPUT:	byte - binary
	
	OUTPUT:	result	- two ASCII nibbles 
	NOTE: Assumes result 2 bytes or larger
*/

void modbus_hex_from_byte( unsigned char byte, uint8_t *result)
{
	result[0] = nib_to_ASCII(byte>>4);
	result[1] = nib_to_ASCII(byte&0xF);
}
/* 
	Convert a HEX value -> binary
	
	INPUT:	psn	- Current psn in buffer
			buffer	- ASCII hex buffer
			result	- Binary value from 2 byte hex
	
	OUTPUT:	Next buffer psn
*/


int modbus_byte_from_hex(int psn,unsigned char *buffer)
{
	uint8_t rtnval = 0;
	uint8_t ch;
	int i;

	// Convert it one byte (ascii char -> nibble at a time

	for( i=0; i< 2; i++ )
	{
		rtnval = rtnval << 4;
		ch = buffer[psn++];
		if( ch <= 0x39 )
			rtnval |= ch - 0x30;		// Convert 0-9
		else
			rtnval |= ch - 0x37;		// A - F
	}

	return rtnval;
}


