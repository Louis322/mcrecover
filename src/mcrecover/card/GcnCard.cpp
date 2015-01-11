/***************************************************************************
 * GameCube Memory Card Recovery Program.                                  *
 * GcnCard.hpp: GameCube memory card class.                                *
 *                                                                         *
 * Copyright (c) 2012-2015 by David Korth.                                 *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.           *
 ***************************************************************************/

#include "GcnCard.hpp"
#include "util/byteswap.h"

// GcnFile
#include "GcnFile.hpp"

// C includes. (C++ namespace)
#include <cstring>
#include <cstdio>

// C++ includes.
#include <limits>

// Qt includes.
#include <QtCore/QTextCodec>

#define NUM_ELEMENTS(x) ((int)(sizeof(x) / sizeof(x[0])))

/** GcnCardPrivate **/

#include "Card_p.hpp"
class GcnCardPrivate : public CardPrivate
{
	public:
		GcnCardPrivate(GcnCard *q);
		virtual ~GcnCardPrivate();
		void init(void);

	protected:
		Q_DECLARE_PUBLIC(GcnCard)
	private:
		Q_DISABLE_COPY(GcnCardPrivate)

	public:
		/**
		 * Open an existing Memory Card image.
		 * @param filename Memory Card image filename.
		 * @return 0 on success; non-zero on error. (also check errorString)
		 */
		int open(const QString &filename);

		/**
		 * Format a new Memory Card image.
		 * @param filename Memory Card image filename.
		 * TODO: Parameters.
		 * @return 0 on success; non-zero on error. (also check errorString)
		 */
		int format(const QString &filename);

	public:
		// Header checksum.
		Checksum::ChecksumValue headerChecksumValue;

		// Memory card data.
		// Table 0 == main; Table 1 == backup.
		card_header mc_header;		// Header.
		card_dat mc_dat_int[2];		// Directory tables.
		card_bat mc_bat_int[2];		// Block allocation tables.

		// Table checksums.
		uint32_t mc_dat_chk_actual[2];
		uint32_t mc_dat_chk_expected[2];
		uint32_t mc_bat_chk_actual[2];
		uint32_t mc_bat_chk_expected[2];
		bool mc_dat_valid[2];
		bool mc_bat_valid[2];

		// Active tables according to the card headers.
		// 0, 1 == valid
		//   -1 == both tables are invalid, default to 0
		int8_t mc_dat_hdr_idx, mc_bat_hdr_idx;

		// Active tables.
		card_dat *mc_dat;
		card_bat *mc_bat;

		/**
		 * Used block map.
		 * NOTE: This is only valid for regular files, not "lost" files.
		 * Value indicates how many files are "using" that block.
		 * Should be 0 for free, 1 for normal files,
		 * and >1 for "lost" files that are overlapping other files.
		 */
		QVector<uint8_t> usedBlockMap;

	private:
		/**
		 * Reset the used block map.
		 * This function should be called on initial load
		 * and on directory/block table reload.
		 */
		void resetUsedBlockMap(void);

		/**
		 * Load the memory card system information.
		 * @return 0 on success; non-zero on error.
		 */
		int loadSysInfo(void);

		/**
		 * Load a directory table.
		 * @param dat		[out] card_dat to store the directory table in.
		 * @param address	[in] Directory table address.
		 * @param checksum	[out] Calculated checksum. (AddSubDual16)
		 * @return 0 on success; non-zero on error.
		 */
		int loadDirTable(card_dat *dat, uint32_t address, uint32_t *checksum);

		/**
		 * Load a block allocation table.
		 * @param bat		[out] card_bat to store the block allocation table in.
		 * @param address	[in] Directory table address.
		 * @param checksum	[out] Calculated checksum. (AddSubDual16)
		 * @return 0 on success; non-zero on error.
		 */
		int loadBlockTable(card_bat *bat, uint32_t address, uint32_t *checksum);

		/**
		 * Determine which tables are active.
		 * Sets mc_dat_hdr_idx and mc_bat_hdr_idx.
		 * NOTE: All tables must be loaded first!
		 * @return 0 on success; non-zero on error.
		 */
		int checkTables(void);

		/**
		 * Load the GcnFile list.
		 */
		void loadGcnFileList(void);
};

GcnCardPrivate::GcnCardPrivate(GcnCard *q)
	: CardPrivate(q, 8192, 64, 2048)
	, mc_dat_hdr_idx(-1)
	, mc_bat_hdr_idx(-1)
	, mc_dat(nullptr)
	, mc_bat(nullptr)
{
	// Clear variables.
	memset(&mc_header, 0, sizeof(mc_header));
	memset(mc_dat_int, 0, sizeof(mc_dat_int));
	memset(mc_bat_int, 0, sizeof(mc_bat_int));
	memset(mc_dat_chk_actual, 0, sizeof(mc_dat_chk_actual));
	memset(mc_dat_chk_expected, 0, sizeof(mc_dat_chk_expected));
	memset(mc_bat_chk_actual, 0, sizeof(mc_bat_chk_actual));
	memset(mc_bat_chk_expected, 0, sizeof(mc_bat_chk_expected));
	memset(mc_dat_valid, 0, sizeof(mc_dat_valid));
	memset(mc_bat_valid, 0, sizeof(mc_bat_valid));
}

GcnCardPrivate::~GcnCardPrivate()
{
	// TODO: Remove this?
}

/**
 * Open an existing Memory Card image.
 * @param filename Memory Card image filename.
 * @return 0 on success; non-zero on error. (also check errorString)
 */
int GcnCardPrivate::open(const QString &filename)
{
	int ret = CardPrivate::open(filename, QIODevice::ReadOnly);
	if (ret != 0) {
		// Error opening the file.
		return ret;
	}

	// Load the GCN-specific data.

	// Reset the used block map.
	resetUsedBlockMap();

	// Load the memory card system information.
	// This includes the header, directory, and block allocation table.
	loadSysInfo();

	// Load the GcnFile list.
	loadGcnFileList();

	return 0;
}

/**
 * Format a new Memory Card image.
 * @param filename Memory Card image filename.
 * TODO: Parameters.
 * @return 0 on success; non-zero on error. (also check errorString)
 */
int GcnCardPrivate::format(const QString &filename)
{
	int ret = CardPrivate::open(filename, QIODevice::ReadWrite);
	if (ret != 0) {
		// Error opening the file.
		return ret;
	}

	// Clear errors.
	// TODO: Separate CardPrivate::create() or format() function
	// that doesn't check for errors?
	errors = 0;

	// Create a 251-block card.
	// Formatting routine based on the Nintendont Loader (r254).
	// TODO: Parameters.
	// TODO: Separate Card::open()'s block count initialization
	// so it can be used in this function.
	totalPhysBlocks = 256;
	file->resize(totalPhysBlocks * blockSize);
	filesize = file->size();
	// TODO: Verify that the filesize matches.

	/**
	 * NOTE: We're storing data as Big-Endian because it's
	 * being written to the Memory Card image file.
	 * The data will need to be reloaded afterwards.
	 */

	// Create the header. (block 0)
	memset(&mc_header, 0xFF, sizeof(mc_header));
	// The first 0x14 bytes contains SRAM and formatting time information.
	// Nintendont skips this by simply writing all 0s here.
	memset(mc_header.serial, 0, sizeof(mc_header.serial));
	// TODO: Set formatTime? (tick rate depends on whether GCN or Wii is used)
	memset(&mc_header.formatTime, 0, sizeof(mc_header.formatTime));
	// SRAM bias. (FIXME: 'U' suffix required due to issues with byteswap code.)
	mc_header.sramBias = cpu_to_be32(0x17CA2A85U);
	// SRAM language. (TODO)
	mc_header.sramLang = cpu_to_be32(0);
	// Reserved.
	memset(mc_header.reserved1, 0, sizeof(mc_header.reserved1));
	// Device ID. (Assume formatted in slot A.)
	mc_header.device_id = cpu_to_be16(0);
	// Memory Card size, in megabits.
	mc_header.size = cpu_to_be16(totalPhysBlocks / 16);
	// Encoding. (Assume cp1252 for now.)
	mc_header.encoding = cpu_to_be16(SYS_FONT_ENCODING_ANSI);
	this->encoding = Card::ENCODING_CP1252;
	// Calculate the header checksum.
	uint32_t chksum = Checksum::AddInvDual16((uint16_t*)&mc_header, 0x1FC, Checksum::CHKENDIAN_BIG);
	mc_header.chksum1 = cpu_to_be16(chksum >> 16);
	mc_header.chksum2 = cpu_to_be16(chksum & 0xFFFF);

	// Create the directory tables. (blocks 1, 2)
	memset(mc_dat_int, 0xFF, sizeof(mc_dat_int));
	// TODO: Compare to GCN/Wii IPL.
	mc_dat_int[0].dircntrl.updated = cpu_to_be16(0);
	mc_dat_int[1].dircntrl.updated = cpu_to_be16(1);
	// Calculate the directory checksums.
	for (int i = 0; i < 2; i++) {
		mc_dat_chk_actual[i] = Checksum::AddInvDual16((uint16_t*)&mc_dat_int[i], 0x1FFC, Checksum::CHKENDIAN_BIG);
		mc_dat_chk_expected[i] = mc_dat_chk_actual[i];
		mc_dat_int[i].dircntrl.chksum1 = cpu_to_be16(mc_dat_chk_actual[i] >> 16);
		mc_dat_int[i].dircntrl.chksum2 = cpu_to_be16(mc_dat_chk_actual[i] & 0xFFFF);
	}

	// Create the block tables. (blocks 3, 4)
	memset(mc_bat_int, 0xFF, sizeof(mc_bat_int));
	// TODO: Compare to GCN/Wii IPL.
	mc_bat_int[0].updated = cpu_to_be16(0);
	mc_bat_int[1].updated = cpu_to_be16(1);
	// Free block counter.
	mc_bat_int[0].freeblocks = cpu_to_be16(freeBlocks);
	mc_bat_int[1].freeblocks = cpu_to_be16(freeBlocks);
	// Last allocated block.
	mc_bat_int[0].lastalloc = cpu_to_be16(4);
	mc_bat_int[1].lastalloc = cpu_to_be16(4);
	// Calculate the block table checksums.
	for (int i = 0; i < 2; i++) {
		mc_bat_chk_actual[i] = Checksum::AddInvDual16((uint16_t*)&mc_bat_int[i], 0x1FFC, Checksum::CHKENDIAN_BIG);
		mc_bat_chk_expected[i] = mc_bat_chk_actual[i];
		mc_bat_int[i].chksum1 = cpu_to_be16(mc_bat_chk_actual[i] >> 16);
		mc_bat_int[i].chksum2 = cpu_to_be16(mc_bat_chk_actual[i] & 0xFFFF);
	}

	// Write everything to the file.
	// TODO: Check for errors.
	file->seek(0);
	file->write((char*)&mc_header, sizeof(mc_header));
	file->seek(1*blockSize);
	file->write((char*)mc_dat_int, sizeof(mc_dat_int));
	file->write((char*)mc_bat_int, sizeof(mc_bat_int));
	file->flush();

	// Un-byteswap the tables.
	// TODO: Skip this on big-endian.
	// Header.
	mc_header.sramBias	= be32_to_cpu(mc_header.sramBias);
	mc_header.sramLang	= be32_to_cpu(0);
	mc_header.device_id	= be16_to_cpu(mc_header.device_id);
	mc_header.size		= be16_to_cpu(mc_header.size);
	mc_header.encoding	= be16_to_cpu(mc_header.encoding);
	mc_header.chksum1	= be16_to_cpu(mc_header.chksum1);
	mc_header.chksum2	= be16_to_cpu(mc_header.chksum2);
	headerChecksumValue.actual = ((mc_header.chksum1 << 16) | mc_header.chksum2);
	headerChecksumValue.expected = headerChecksumValue.actual;
	for (int i = 0; i < 2; i++) {
		// Directory Table.
		mc_dat_int[i].dircntrl.updated = cpu_to_be16(mc_dat_int[i].dircntrl.updated);
		mc_dat_int[i].dircntrl.chksum1 = cpu_to_be16(mc_dat_int[i].dircntrl.chksum1);
		mc_dat_int[i].dircntrl.chksum2 = cpu_to_be16(mc_dat_int[i].dircntrl.chksum2);
		mc_dat_valid[i] = true;

		// Block Table.
		mc_bat_int[i].updated		= cpu_to_be16(mc_bat_int[i].updated);
		mc_bat_int[i].freeblocks	= cpu_to_be16(mc_bat_int[i].freeblocks);
		mc_bat_int[i].lastalloc		= cpu_to_be16(mc_bat_int[i].lastalloc);
		mc_bat_int[i].chksum1		= cpu_to_be16(mc_bat_int[i].chksum1);
		mc_bat_int[i].chksum2		= cpu_to_be16(mc_bat_int[i].chksum2);
		mc_bat_valid[i] = true;
	}

	// Reset the used block map.
	resetUsedBlockMap();

	// Check which table is active.
	checkTables();

	// Load the GcnFile list.
	// FIXME: Probably not necessary.
	// (Was previously needed to emit blockCountChanged(),
	//  but that's now done in checkTables() as well...)
	loadGcnFileList();

	return 0;
}

/**
 * Reset the used block map.
 * This function should be called on initial load
 * and on directory/block table reload.
 */
void GcnCardPrivate::resetUsedBlockMap(void)
{
	// Initialize the used block map.
	// (The first 5 blocks are always used.)
	usedBlockMap = QVector<uint8_t>(totalPhysBlocks, 0);
	for (int i = 0; i < 5 && i < totalPhysBlocks; i++)
		usedBlockMap[i] = 1;
}

/**
 * Load the memory card system information.
 * @return 0 on success; non-zero on error.
 */
int GcnCardPrivate::loadSysInfo(void)
{
	if (!file)
		return -1;

	// Header.
	file->seek(0);
	qint64 sz = file->read((char*)&mc_header, sizeof(mc_header));
	if (sz < (qint64)sizeof(mc_header)) {
		// Error reading the card header.
		// Zero the header and block tables,
		// and 0xFF the directory tables.
		memset(&mc_header, 0x00, sizeof(mc_header));
		// This checksum can never appear in a valid header.
		mc_header.chksum1 = 0xAA55;
		mc_header.chksum2 = 0xAA55;

		// Header checksum.
		headerChecksumValue.actual = Checksum::AddInvDual16((uint16_t*)&mc_header, 0x1FC, Checksum::CHKENDIAN_BIG);
		headerChecksumValue.expected = (mc_header.chksum1 << 16) |
					       (mc_header.chksum2);

		memset(mc_dat_int, 0xFF, sizeof(mc_dat_int));
		// This checksum can never appear in a valid table.
		mc_dat_int[0].dircntrl.chksum1 = 0xAA55;
		mc_dat_int[0].dircntrl.chksum2 = 0xAA55;
		mc_dat_int[1].dircntrl.chksum1 = 0xAA55;
		mc_dat_int[1].dircntrl.chksum2 = 0xAA55;

		memset(mc_bat_int, 0x00, sizeof(mc_bat_int));
		// This checksum can never appear in a valid table.
		mc_bat_int[0].chksum1 = 0xAA55;
		mc_bat_int[0].chksum2 = 0xAA55;
		mc_bat_int[1].chksum1 = 0xAA55;
		mc_bat_int[1].chksum2 = 0xAA55;

		// Use cp1252 encoding by default.
		this->encoding = Card::ENCODING_CP1252;

		// Make sure mc_dat and mc_bat are initialized.
		checkTables();
		return -2;
	}

	// Calculate the header checksum.
	headerChecksumValue.actual = Checksum::AddInvDual16((uint16_t*)&mc_header, 0x1FC, Checksum::CHKENDIAN_BIG);

	// Byteswap the header contents.
	mc_header.formatTime	= be64_to_cpu(mc_header.formatTime);
	mc_header.sramBias	= be32_to_cpu(mc_header.sramBias);
	mc_header.sramLang	= be32_to_cpu(mc_header.sramLang);
	mc_header.device_id	= be16_to_cpu(mc_header.device_id);
	mc_header.size		= be16_to_cpu(mc_header.size);
	mc_header.encoding	= be16_to_cpu(mc_header.encoding);
	mc_header.chksum1	= be16_to_cpu(mc_header.chksum1);
	mc_header.chksum2	= be16_to_cpu(mc_header.chksum2);

	// Check the encoding.
	switch (mc_header.encoding & SYS_FONT_ENCODING_MASK) {
		case SYS_FONT_ENCODING_ANSI:
		default:
			this->encoding = Card::ENCODING_CP1252;
			break;
		case SYS_FONT_ENCODING_SJIS:
			this->encoding = Card::ENCODING_SHIFTJIS;
			break;
	}

	// Get the expected header checksum.
	headerChecksumValue.expected = (mc_header.chksum1 << 16) |
				       (mc_header.chksum2);

	if (headerChecksumValue.expected != headerChecksumValue.actual) {
		// Header checksum is invalid.
		this->errors |= GcnCard::MCE_INVALID_HEADER;
	}

	/**
	 * NOTE: formatTime appears to be in units of (CPU clock / 12).
	 * This means the time format will be different depending on if
	 * the card was formatted on a GameCube (or Wii in GCN mode)
	 * or on a Wii in Wii mode.
	 *
	 * Don't bother determining the actual formatTime right now.
	 */

	// TODO: Adjust for block size?
	// TODO: Verify that chksum1 and chksum2 are valid complements.

	// NOTE: If an error occurs while loading a DAT or BAT,
	// it will be zeroed out.
	static const uint32_t DAT_addr[2] = {CARD_SYSDIR, CARD_SYSDIR_BACK};
	static const uint32_t BAT_addr[2] = {CARD_SYSBAT, CARD_SYSBAT_BACK};
	for (int i = 0; i < 2; i++) {
		// Load the directory table.
		int ret = loadDirTable(&mc_dat_int[i], DAT_addr[i], &mc_dat_chk_actual[i]);
		if (ret != 0) {
			memset(&mc_dat_int[i], 0xFF, sizeof(mc_dat_int[i]));
			// This checksum can never appear in a valid table.
			mc_dat_int[i].dircntrl.chksum1 = 0xAA55;
			mc_dat_int[i].dircntrl.chksum2 = 0xAA55;
		}

		// Get the expected checksum.
		mc_dat_chk_expected[i] = (mc_dat_int[i].dircntrl.chksum1 << 16) |
					 (mc_dat_int[i].dircntrl.chksum2);

		// Check if the directory table is valid.
		mc_dat_valid[i] = (mc_dat_chk_expected[i] == mc_dat_chk_actual[i]);

		// Load the block table.
		ret = loadBlockTable(&mc_bat_int[i], BAT_addr[i], &mc_bat_chk_actual[i]);
		if (ret != 0) {
			memset(&mc_bat_int[i], 0x00, sizeof(mc_bat_int[i]));
			// This checksum can never appear in a valid table.
			mc_bat_int[i].chksum1 = 0xAA55;
			mc_bat_int[i].chksum2 = 0xAA55;
		}

		// Get the expected checksum.
		mc_bat_chk_expected[i] = (mc_bat_int[i].chksum1 << 16) |
					 (mc_bat_int[i].chksum2);

		// Check if the block table is valid.
		mc_bat_valid[i] = (mc_bat_chk_expected[i] == mc_bat_chk_actual[i]);
	}

	// Determine which tables are active.
	checkTables();
	return 0;
}

/**
 * Load a directory table.
 * @param dat		[out] card_dat to store the directory table in.
 * @param address	[in] Directory table address.
 * @param checksum	[out] Calculated checksum. (AddSubDual16)
 * @return 0 on success; non-zero on error.
 */
int GcnCardPrivate::loadDirTable(card_dat *dat, uint32_t address, uint32_t *checksum)
{
	file->seek(address);
	qint64 sz = file->read((char*)dat, sizeof(*dat));
	if (sz < (qint64)sizeof(*dat)) {
		// Error reading the directory table.
		return -1;
	}

	// Calculate the checksums.
	if (checksum != nullptr) {
		*checksum = Checksum::AddInvDual16(
			reinterpret_cast<const uint16_t*>(dat),
			(uint32_t)(sizeof(*dat) - 4),
			Checksum::CHKENDIAN_BIG);
	}

	// Byteswap the directory table contents.
	for (int i = 0; i < NUM_ELEMENTS(dat->entries); i++) {
		card_direntry *dirEntry	= &dat->entries[i];
		dirEntry->lastmodified	= be32_to_cpu(dirEntry->lastmodified);
		dirEntry->iconaddr	= be32_to_cpu(dirEntry->iconaddr);
		dirEntry->iconfmt	= be16_to_cpu(dirEntry->iconfmt);
		dirEntry->iconspeed	= be16_to_cpu(dirEntry->iconspeed);
		dirEntry->block		= be16_to_cpu(dirEntry->block);
		dirEntry->length	= be16_to_cpu(dirEntry->length);
		dirEntry->commentaddr	= be32_to_cpu(dirEntry->commentaddr);
	}

	// Byteswap the directory control block.
	dat->dircntrl.updated = be16_to_cpu(dat->dircntrl.updated);
	dat->dircntrl.chksum1 = be16_to_cpu(dat->dircntrl.chksum1);
	dat->dircntrl.chksum2 = be16_to_cpu(dat->dircntrl.chksum2);
	return 0;
}

/**
 * Load a block allocation table.
 * @param bat		[out] card_bat to store the block allocation table in.
 * @param address	[in] Directory table address.
 * @param checksum	[out] Calculated checksum. (AddSubDual16)
 * @return 0 on success; non-zero on error.
 */
int GcnCardPrivate::loadBlockTable(card_bat *bat, uint32_t address, uint32_t *checksum)
{
	file->seek(address);
	qint64 sz = file->read((char*)bat, sizeof(*bat));
	if (sz < (qint64)sizeof(*bat)) {
		// Error reading the directory table.
		return -1;
	}

	// Calculate the checksums.
	if (checksum != nullptr) {
		*checksum = Checksum::AddInvDual16(
			(reinterpret_cast<const uint16_t*>(bat) + 2),
			(uint32_t)(sizeof(*bat) - 4),
			Checksum::CHKENDIAN_BIG);
	}

	// Byteswap the block allocation table contents.
	bat->chksum1	= be16_to_cpu(bat->chksum1);
	bat->chksum2	= be16_to_cpu(bat->chksum2);
	bat->updated	= be16_to_cpu(bat->updated);
	bat->freeblocks	= be16_to_cpu(bat->freeblocks);
	bat->lastalloc	= be16_to_cpu(bat->lastalloc);

	for (int i = 0; i < NUM_ELEMENTS(bat->fat); i++)
		bat->fat[i] = be16_to_cpu(bat->fat[i]);

	return 0;
}

/**
 * Determine which tables are active.
 * Sets mc_dat_hdr_idx and mc_bat_hdr_idx.
 * NOTE: All tables must be loaded first!
 * @return 0 on success; non-zero on error.
 */
int GcnCardPrivate::checkTables(void)
{
	/**
	 * Determine which directory table to use.
	 * - 1. Check for higher "updated" value.
	 * - 2. Validate checksums.
	 * - 3. If invalid checksum, use other one.
	 * - 4. If both are invalid, error!
	 */
	int idx = (mc_dat_int[1].dircntrl.updated > mc_dat_int[0].dircntrl.updated);

	// Verify the checksums of the selected directory table.
	if (!mc_dat_valid[idx]) {
		// Invalid checksum. Check the other directory table.
		idx = !idx;
		if (!mc_dat_valid[idx]) {
			// Both directory tables are invalid.
			this->errors |= GcnCard::MCE_INVALID_DATS;
			idx = -1;
		}
	}

	// Select the directory table.
	int tmp_idx = (idx >= 0 ? idx : 0);
	this->mc_dat = &mc_dat_int[tmp_idx];
	this->mc_dat_hdr_idx = idx;

	/**
	 * Determine which block allocation table to use.
	 * - 1. Check for higher "updated" value.
	 * - 2. Validate checksums.
	 * - 3. If invalid checksum, use other one.
	 * - 4. If both are invalid, error!
	 */
	idx = (mc_bat_int[1].updated > mc_bat_int[0].updated);

	// Verify the checksums of the selected block allocation table.
	if (!mc_bat_valid[idx]) {
		// Invalid checksum. Check the other block allocation table.
		idx = !idx;
		if (!mc_bat_valid[idx]) {
			// Both block allocation tables are invalid.
			this->errors |= GcnCard::MCE_INVALID_BATS;
			idx = -1;
		}
	}

	// Select the directory table.
	tmp_idx = (idx >= 0 ? idx : 0);
	this->mc_bat = &mc_bat_int[tmp_idx];
	this->mc_bat_hdr_idx = idx;

	// Update block counts.
	totalUserBlocks = (totalPhysBlocks - 5);
	if (totalUserBlocks < 0)
		totalUserBlocks = 0;
	freeBlocks = this->mc_bat->freeblocks;
	Q_Q(GcnCard);
	emit q->blockCountChanged(totalPhysBlocks, totalUserBlocks, freeBlocks);

	// Determine the card color based on the block count.
	// TODO: Better colors?
	QColor color;
	switch (totalUserBlocks) {
		case 59:
			color = QColor(Qt::darkGray);
			break;
		case 251:
			color = QColor(Qt::black);
			break;
		case 1019:
			color = QColor(Qt::white);
			break;
		default:
			color = QColor();
			break;
	}
	if (this->color != color) {
		// Color has changed.
		this->color = color;
		emit q->colorChanged(color);
	}

	return 0;
}

/**
 * Load the GcnFile list.
 */
void GcnCardPrivate::loadGcnFileList(void)
{
	if (!file)
		return;

	Q_Q(GcnCard);

	// Clear the current GcnFile list.
	int init_size = lstFiles.size();
	if (init_size > 0)
		emit q->filesAboutToBeRemoved(0, (init_size - 1));
	qDeleteAll(lstFiles);
	lstFiles.clear();
	if (init_size > 0)
		emit q->filesRemoved();

	// Reset the used block map.
	resetUsedBlockMap();

	QVector<File*> lstFiles_new;
	lstFiles_new.reserve(NUM_ELEMENTS(mc_dat->entries));

	// Byteswap the directory table contents.
	for (int i = 0; i < NUM_ELEMENTS(mc_dat->entries); i++) {
		const card_direntry *dirEntry = &mc_dat->entries[i];

		// If the game code is 0xFFFFFFFF, the entry is empty.
		static const uint8_t gamecode_empty[4] = {0xFF, 0xFF, 0xFF, 0xFF};
		if (!memcmp(dirEntry->gamecode, gamecode_empty, sizeof(gamecode_empty)))
			continue;

		// Valid directory entry.
		GcnFile *mcFile = new GcnFile(q, dirEntry, mc_bat);
		lstFiles_new.append(mcFile);

		// Mark the file's blocks as used.
		QVector<uint16_t> fatEntries = mcFile->fatEntries();
		foreach (uint16_t block, fatEntries) {
			if (block >= 5 && block < usedBlockMap.size()) {
				// Valid block.
				// Increment its entry in the usedBlockMap.
				if (usedBlockMap[block] < std::numeric_limits<uint8_t>::max())
					usedBlockMap[block]++;
			} else {
				// Invalid block.
				// TODO: Store an error value somewhere.
				fprintf(stderr, "WARNING: File %d has invalid FAT entry 0x%04X.\n", i, block);
			}
		}
	}

	if (!lstFiles_new.isEmpty()) {
		// Files have been added to the memory card.
		emit q->filesAboutToBeInserted(0, (lstFiles_new.size() - 1));
		// NOTE: QVector::swap() was added in qt-4.8.
		lstFiles = lstFiles_new;
		emit q->filesInserted();
	}

	// Block count has changed.
	emit q->blockCountChanged(totalPhysBlocks, totalUserBlocks, freeBlocks);
}

/** GcnCard **/

GcnCard::GcnCard(QObject *parent)
	: Card(new GcnCardPrivate(this), parent)
{ }

GcnCard::~GcnCard()
{
	// TODO: Is this needed anymore?
}

/**
 * Open an existing Memory Card image.
 * @param filename Filename.
 * @param parent Parent object.
 * @return GcnCard object, or nullptr on error.
 */
GcnCard *GcnCard::open(const QString& filename, QObject *parent)
{
	GcnCard *gcnCard = new GcnCard(parent);
	GcnCardPrivate *const d = gcnCard->d_func();
	d->open(filename);
	return gcnCard;
}

/**
 * Format a new Memory Card image.
 * @param filename Filename.
 * @param parent Parent object.
 * @return GcnCard object, or nullptr on error.
 */
GcnCard *GcnCard::format(const QString& filename, QObject *parent)
{
	// Format a new GcnCard.
	// TODO: Parameters.
	GcnCard *gcnCard = new GcnCard(parent);
	GcnCardPrivate *const d = gcnCard->d_func();
	d->format(filename);
	return gcnCard;
}

/**
 * Get the product name of this memory card.
 * This refers to the class in general,
 * and does not change based on size.
 * @return Product name.
 */
QString GcnCard::productName(void) const
{
	return tr("GameCube memory card");
}

/**
 * Get the text encoding for a given region.
 * @param region Region code. (If 0, use the memory card's encoding.)
 * @return Text encoding.
 */
Card::Encoding GcnCard::encodingForRegion(char region) const
{
	// TODO: Return an error if not open?
	if (!isOpen())
		return ENCODING_CP1252;

	Q_D(const GcnCard);
	switch (region) {
		case 0:
			// Use the memory card's encoding.
			return d->encoding;

		case 'J':
		case 'S':
			// Japanese.
			// NOTE: 'S' appears in RELSAB, which is used for
			// some prototypes, including Sonic Adventure DX
			// and Metroid Prime 3. Assume Japanese for now.
			// TODO: Implement a Shift-JIS heuristic for 'S'.
			return ENCODING_SHIFTJIS;

		default:
			// US, Europe, Australia.
			// TODO: Korea?
			return ENCODING_CP1252;
	}
}

/**
 * Get the QTextCodec for a given region.
 * @param region Region code. (If 0, use the memory card's encoding.)
 * @return QTextCodec.
 */
QTextCodec *GcnCard::textCodec(char region) const
{
	if (!isOpen())
		return nullptr;

	Q_D(const GcnCard);
	// FIXME: Calling this->textCodec() results in a stack overflow.
	// Call the base class function directly.
	// (Maybe it's a conflict between 'char' and enum?)
	return Card::textCodec(encodingForRegion(region));
}

/**
 * Get the used block map.
 * NOTE: This is only valid for regular files, not "lost" files.
 * @return Used block map.
 */
QVector<uint8_t> GcnCard::usedBlockMap(void)
{
	if (!isOpen())
		return QVector<uint8_t>();
	Q_D(GcnCard);
	return d->usedBlockMap;
}

/**
 * Add a "lost" file.
 * NOTE: This is a debugging version.
 * Add more comprehensive versions with a block map specification.
 * @return GcnFile added to the GcnCard, or nullptr on error.
 */
GcnFile *GcnCard::addLostFile(const card_direntry *dirEntry)
{
	if (!isOpen())
		return nullptr;

	// Initialize the FAT entries baesd on start/length.
	// TODO: Check for block collisions and skip used blocks.
	QVector<uint16_t> fatEntries;
	fatEntries.reserve(dirEntry->length);

	const uint16_t maxBlockNum = ((uint16_t)totalPhysBlocks() - 1);
	// FIXME: <= 5? Maybe it should be < 5, but since
	// GCN cards are supposed to be at least 59(64),
	// this probably isn't a problem.
	if (maxBlockNum <= 5 || maxBlockNum > 4091) {
		// Invalid maximum block size. Don't initialize the FAT.
		// TODO: Print an error message.
	} else {
		// Initialize the FAT.
		uint16_t block = dirEntry->block;
		uint16_t length = dirEntry->length;
		for (; length > 0; length--, block++) {
			if (block > maxBlockNum)
				block = 5;
			fatEntries.append(block);
		}
	}

	return addLostFile(dirEntry, fatEntries);
}

/**
 * Add a "lost" file.
 * @param dirEntry Directory entry.
 * @param fatEntries FAT entries.
 * @return GcnFile added to the GcnCard, or nullptr on error.
 */
GcnFile *GcnCard::addLostFile(const card_direntry *dirEntry, const QVector<uint16_t> &fatEntries)
{
	if (!isOpen())
		return nullptr;

	Q_D(GcnCard);
	GcnFile *file = new GcnFile(this, dirEntry, fatEntries);
	int idx = d->lstFiles.size();
	emit filesAboutToBeInserted(idx, idx);
	d->lstFiles.append(file);
	emit filesInserted();
	return file;
}

/**
 * Add "lost" files.
 * @param filesFoundList List of SearchData.
 * @return List of GcnFiles added to the GcnCard, or empty list on error.
 */
QList<GcnFile*> GcnCard::addLostFiles(const QLinkedList<SearchData> &filesFoundList)
{
	QList<GcnFile*> files;
	if (!isOpen())
		return files;
	if (filesFoundList.isEmpty())
		return files;

	Q_D(GcnCard);
	const int idx = d->lstFiles.size();
	const int idxLast = idx + filesFoundList.size() - 1;
	emit filesAboutToBeInserted(idx, idxLast);

	foreach (const SearchData &searchData, filesFoundList) {
		GcnFile *file = new GcnFile(this, &searchData.dirEntry, searchData.fatEntries);
		// NOTE: If file is nullptr, this may screw up the QTreeView
		// due to filesAboutToBeInserted().

		// TODO: Add ChecksumData parameter to addLostFile.
		// Alternatively, add SearchData overload?
		if (file) {
			files.append(file);
			d->lstFiles.append(file);
			file->setChecksumDefs(searchData.checksumDefs);
		}
	}

	emit filesInserted();
	return files;
}

/**
 * Get the header checksum value.
 * NOTE: Header checksum is always AddInvDual16.
 * @return Header checksum value.
 */
Checksum::ChecksumValue GcnCard::headerChecksumValue(void) const
{
	Q_D(const GcnCard);
	return d->headerChecksumValue;
}

/**
 * Get the active Directory Table index.
 * @return Active Directory Table index. (0 or 1)
 */
int GcnCard::activeDatIdx(void) const
{
	if (!isOpen())
		return -1;
	Q_D(const GcnCard);
	return (d->mc_dat == &d->mc_dat_int[1]);
}

/**
 * Set the active Directory Table index.
 * NOTE: This function reloads the file list, without lost files.
 * @param idx Active Directory Table index. (0 or 1)
 */
void GcnCard::setActiveDatIdx(int idx)
{
	if (!isOpen())
		return;
	Q_D(GcnCard);
	if (idx < 0 || idx >= NUM_ELEMENTS(d->mc_dat_int))
		return;
	d->mc_dat = &d->mc_dat_int[idx];
	d->loadGcnFileList();
}

/**
 * Get the active Directory Table index according to the card header.
 * @return Active Directory Table index (0 or 1), or -1 if both are invalid.
 */
int GcnCard::activeDatHdrIdx(void) const
{
	if (!isOpen())
		return -1;
	Q_D(const GcnCard);
	return d->mc_dat_hdr_idx;
}

/**
 * Is a Directory Table valid?
 * @param idx Directory Table index. (0 or 1)
 * @return True if valid; false if not valid or idx is invalid.
 */
bool GcnCard::isDatValid(int idx) const
{
	if (!isOpen())
		return false;
	Q_D(const GcnCard);
	if (idx < 0 || idx >= NUM_ELEMENTS(d->mc_dat_valid))
		return false;
	return d->mc_dat_valid[idx];
}

/**
 * Get the active Block Table index.
 * @return Active Block Table index. (0 or 1)
 */
int GcnCard::activeBatIdx(void) const
{
	if (!isOpen())
		return -1;
	Q_D(const GcnCard);
	return (d->mc_bat == &d->mc_bat_int[1]);
}

/**
 * Set the active Block Table index.
 * NOTE: This function reloads the file list, without lost files.
 * @param idx Active Block Table index. (0 or 1)
 */
void GcnCard::setActiveBatIdx(int idx)
{
	if (!isOpen())
		return;
	Q_D(GcnCard);
	if (idx < 0 || idx >= NUM_ELEMENTS(d->mc_bat_int))
		return;
	d->mc_bat = &d->mc_bat_int[idx];
	d->loadGcnFileList();
}

/**
 * Get the active Block Table index according to the card header.
 * @return Active Block Table index (0 or 1), or -1 if both are invalid.
 */
int GcnCard::activeBatHdrIdx(void) const
{
	if (!isOpen())
		return -1;
	Q_D(const GcnCard);
	return d->mc_bat_hdr_idx;
}

/**
 * Is a Block Table valid?
 * @param idx Block Table index. (0 or 1)
 * @return True if valid; false if not valid or idx is invalid.
 */
bool GcnCard::isBatValid(int idx) const
{
	if (!isOpen())
		return false;
	Q_D(const GcnCard);
	if (idx < 0 || idx >= NUM_ELEMENTS(d->mc_bat_valid))
		return false;
	return d->mc_bat_valid[idx];
}
