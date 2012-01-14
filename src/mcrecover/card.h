/***************************************************************************
 * GameCube Memory Card Recovery Program.                                  *
 * card.h: Memory Card definitions.                                        *
 * Derived from libogc's card.c and card.h.                                *
 **************************************************************************/

/**
 * Source:
 * http://devkitpro.svn.sourceforge.net/viewvc/devkitpro/trunk/libogc/libogc/card.c?revision=4732&view=markup
 */

/*-------------------------------------------------------------

card.c -- Memory card subsystem

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#ifndef __MCRECOVER_CARD_H__
#define __MCRECOVER_CARD_H__

#include <stdint.h>

// Packed struct attribute.
#if !defined(PACKED)
#if defined(__GNUC__)
#define PACKED __attribute__ ((packed))
#else
#define PACKED
#endif /* defined(__GNUC__) */
#endif /* !defined(PACKED) */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Memory card system locations.
 */
#define CARD_SYSAREA		5
#define CARD_SYSDIR		0x2000
#define CARD_SYSDIR_BACK	0x4000
#define CARD_SYSBAT		0x6000
#define CARD_SYSBAT_BACK	0x8000

#define CARD_FILENAMELEN	32	/* Filename length. */
#define CARD_MAXFILES		128	/* Maximum number of files. */

/**
 * Memory card header.
 */
#pragma pack(1)
typedef struct PACKED _card_header
{
	uint32_t serial[0x08];	// Serial number.
	uint16_t device_id;
	uint16_t size;
	uint16_t encoding;
	uint8_t padding[0x1D6];
	uint16_t chksum1;
	uint16_t chksum2;
} card_header;
#pragma pack()

/**
 * Directory entry.
 * Addresses are relative to the start of the file.
 */
#pragma pack(1)
typedef struct PACKED _card_direntry
{
	uint8_t gamecode[4];	// Game code.
	uint8_t company[2];	// Company code.
	uint8_t pad_00;		// Padding. (0xFF)
	uint8_t bannerfmt;	// Banner format.
	char filename[CARD_FILENAMELEN];	// Filename.
	uint32_t lastmodified;	// Last modified time. (seconds since 2000/01/01)
	uint32_t iconaddr;	// Icon address.
	uint16_t iconfmt;	// Icon format.
	uint16_t iconspeed;	// Icon speed.
	uint8_t permission;	// File permissions.
	uint8_t copytimes;	// Copy counter.
	uint16_t block;		// Starting block address.
	uint16_t length;	// File length, in blocks.
	uint16_t pad_01;	// Padding. (0xFFFF)
	uint32_t commentaddr;	// Comment address.
} card_direntry;
#pragma pack()

/**
 * Directory table.
 */
#pragma pack(1)
typedef struct PACKED _card_dat
{
	struct _card_direntry entries[CARD_MAXFILES];
} card_dat;
#pragma pack()

/**
 * Block allocation table.
 */
#pragma pack(1)
typedef struct PACKED _card_bat
{
	uint8_t pad[58];	// Padding.
	uint16_t updated;	// Update serial number.
	uint16_t freeblocks;	// Number of free blocks.
	uint16_t lastalloc;	// Last block allocated.
	uint16_t fat[0xFFC];	// File allocation table.
} card_bat;

// File attributes.
#define CARD_ATTRIB_PUBLIC	0x04
#define CARD_ATTRIB_NOCOPY	0x08
#define CARD_ATTRIB_NOMOVE	0x10

// Banner size.
#define CARD_BANNER_W		96
#define CARD_BANNER_H		32

// Banner format.
#define CARD_BANNER_NONE	0x00	/* No banner. */
#define CARD_BANNER_CI		0x01	/* CI8 (256-color) */
#define CARD_BANNER_RGB		0x02	/* RGB5A1 */
#define CARD_BANNER_MASK	0x03

// Icon size.
#define CARD_MAXICONS		8	/* Maximum 8 icons per file. */
#define CARD_ICON_W		32
#define CARD_ICON_H		32

// Icon format.
#define CARD_ICON_NONE		0x00	/* No icon. */
#define CARD_ICON_CI		0x01	/* CI8 (256-color) */
#define CARD_ICON_RGB		0x02	/* RGB5A1 */
#define CARD_ICON_MASK		0x03

// Icon animation style.
#define CARD_ANIM_LOOP		0x00
#define CARD_ANIM_BOUNCE	0x04
#define CARD_ANIM_MASK		0x04

// Icon animation speed.
#define CARD_SPEED_END		0x00
#define CARD_SPEED_FAST		0x01
#define CARD_SPEED_MIDDLE	0x02
#define CARD_SPEED_SLOW		0x03
#define CARD_SPEED_MASK		0x03

#ifdef __cplusplus
}
#endif

#endif /* __MCRECOVER_CARD_H__ */
