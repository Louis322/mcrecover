/***************************************************************************
 * GameCube Tools Library.                                                 *
 * Checksum.cpp: Checksum algorithm class.                                 *
 *                                                                         *
 * Copyright (c) 2013 by David Korth.                                      *
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

#include "Checksum.hpp"
#include "SonicChaoGarden.inc.h"

#include "util/byteswap.h"

// C includes. (C++ namespace)
#include <cstdio>
#include <cstring>

// C++ includes.
#include <string>
#include <vector>
using std::string;
using std::vector;

/** Algorithms. **/

/**
 * CRC-16 algorithm.
 * @param buf Data buffer.
 * @param siz Length of data buffer.
 * @param poly Polynomial.
 * @return Checksum.
 */
uint16_t Checksum::Crc16(const uint8_t *buf, uint32_t siz, uint16_t poly)
{
	// TODO: Add optimized version for poly == CRC16_POLY_CCITT.
	uint16_t crc = 0xFFFF;

	for (; siz != 0; siz--, buf++) {
		crc ^= (*buf & 0xFF);
		for (int i = 8; i > 0; i--) {
			if (crc & 1)
				crc = ((crc >> 1) ^ poly);
			else
				crc >>= 1;
		}
	}

	return ~crc;
}

/**
 * AddInvDual16 algorithm.
 * Adds 16-bit words together in a uint16_t.
 * First word is a simple addition.
 * Second word adds (word ^ 0xFFFF).
 * If either word equals 0xFFFF, it's changed to 0.
 * @param buf Data buffer.
 * @param siz Length of data buffer.
 * @return Checksum.
 */
uint32_t Checksum::AddInvDual16(const uint16_t *buf, uint32_t siz)
{
	// NOTE: Integer overflow/underflow is expected here.
	uint16_t chk1 = 0;
	uint16_t chk2 = 0;

	// We're operating on words, not bytes.
	// siz is in bytes, so we have to divide it by two.
	siz /= 2;

	// Do four words at a time.
	// TODO: Optimize byteswapping?
	for (; siz > 4; siz -= 4, buf += 4) {
		chk1 += be16_to_cpu(buf[0]); chk2 += (be16_to_cpu(buf[0]) ^ 0xFFFF);
		chk1 += be16_to_cpu(buf[1]); chk2 += (be16_to_cpu(buf[1]) ^ 0xFFFF);
		chk1 += be16_to_cpu(buf[2]); chk2 += (be16_to_cpu(buf[2]) ^ 0xFFFF);
		chk1 += be16_to_cpu(buf[3]); chk2 += (be16_to_cpu(buf[3]) ^ 0xFFFF);
	}

	// Remaining words.
	for (; siz != 0; siz--, buf++) {
		chk1 += be16_to_cpu(*buf);
		chk2 += (be16_to_cpu(*buf) ^ 0xFFFF);
	}

	// 0xFFFF is an invalid checksum value.
	// Reset it to 0 if it shows up.
	if (chk1 == 0xFFFF)
		chk1 = 0;
	if (chk2 == 0xFFFF)
		chk2 = 0;

	// Combine the checksum into a dword.
	// chk1 == high word; chk2 == low word.
	return ((chk1 << 16) | chk2);
}

/**
 * AddBytes32 algorithm.
 * Adds all bytes together in a uint32_t.
 * @param buf Data buffer.
 * @param siz Length of data buffer.
 * @return Checksum.
 */
uint32_t Checksum::AddBytes32(const uint8_t *buf, uint32_t siz)
{
	uint32_t checksum = 0;

	// Do four bytes at a time.
	for (; siz > 4; siz -= 4, buf += 4) {
		checksum += buf[0];
		checksum += buf[1];
		checksum += buf[2];
		checksum += buf[3];
	}

	// Remaining bytes.
	for (; siz != 0; siz--, buf++)
		checksum += *buf;

	return checksum;
}

/**
 * SonicChaoGarden algorithm.
 * @param buf Data buffer.
 * @param siz Length of data buffer.
 * @return Checksum.
 */
uint32_t Checksum::SonicChaoGarden(const uint8_t *buf, uint32_t siz)
{
	// Ported from MainMemory's C# SADX/SA2B Chao Garden checksum code.
	const uint32_t a4 = 0x686F6765;
	uint32_t v4 = 0x6368616F;

	for (; siz != 0; siz--, buf++) {
		v4 = SonicChaoGarden_CRC32_Table[*buf ^ (v4 & 0xFF)] ^ (v4 >> 8);
	}

	return (a4 ^ v4);
}

/** General functions. **/

/**
 * Get the checksum for a block of data.
 * @param algorithm Checksum algorithm.
 * @param buf Data buffer.
 * @param siz Length of data buffer.
 * @param param Algorithm parameter, e.g. polynomial or sum.
 * @return Checksum.
 */
uint32_t Checksum::Exec(ChkAlgorithm algorithm, const void *buf, uint32_t siz, uint32_t param)
{
	switch (algorithm) {
		case CHKALG_CRC16:
			if (param == 0)
				param = CRC16_POLY_CCITT;
			return Crc16(reinterpret_cast<const uint8_t*>(buf),
				     siz, (uint16_t)(param & 0xFFFF));

		case CHKALG_ADDINVDUAL16:
			return AddInvDual16(reinterpret_cast<const uint16_t*>(buf), siz);

		case CHKALG_ADDBYTES32:
			return AddBytes32(reinterpret_cast<const uint8_t*>(buf), siz);

		case CHKALG_SONICCHAOGARDEN:
			return SonicChaoGarden(reinterpret_cast<const uint8_t*>(buf), siz);

		case CHKALG_CRC32:
			// TODO

		default:
			break;
	}

	// Unknown algorithm.
	return 0;
}

/**
 * Get a ChkAlgorithm from a checksum algorithm name.
 * @param algorithm Checksum algorithm name.
 * @return ChkAlgorithm. (If unknown, returns CHKALG_NONE.)
 */
Checksum::ChkAlgorithm Checksum::ChkAlgorithmFromString(const char *algorithm)
{
	if (!strcmp(algorithm, "crc16") ||
	    !strcmp(algorithm, "crc-16"))
	{
		return CHKALG_CRC16;
	}
	else if (!strcmp(algorithm, "crc32") ||
		 !strcmp(algorithm, "crc-32"))
	{
		return CHKALG_CRC32;
	} else if (!strcmp(algorithm, "addinvdual16")) {
		return CHKALG_ADDINVDUAL16;
	} else if (!strcmp(algorithm, "addbytes32")) {
		return CHKALG_ADDBYTES32;
	}
	else if (!strcmp(algorithm, "sonicchaogarden") ||
		 !strcmp(algorithm, "sonic chao garden"))
	{
		return CHKALG_SONICCHAOGARDEN;
	}

	// Unknown algorithm name.
	return CHKALG_NONE;
}

/**
 * Get a checksum algorithm name from a ChkAlgorithm.
 * @param algorithm ChkAlgorithm.
 * @return Checksum algorithm name, or nullptr if CHKALG_NONE or unknown.
 */
const char *Checksum::ChkAlgorithmToString(ChkAlgorithm algorithm)
{
	switch (algorithm) {
		default:
		case CHKALG_NONE:
			return nullptr;

		case CHKALG_CRC16:
			return "CRC-16";
		case CHKALG_CRC32:
			return "CRC-32";
		case CHKALG_ADDINVDUAL16:
			return "AddInvDual16";
		case CHKALG_ADDBYTES32:
			return "AddBytes32";
		case CHKALG_SONICCHAOGARDEN:
			return "SonicChaoGarden";
	}
}

/**
 * Get a nicely formatted checksum algorithm name from a ChkAlgorithm.
 * @param algorithm ChkAlgorithm.
 * @return Checksum algorithm name, or nullptr if CHKALG_NONE or unknown.
 */
const char *Checksum::ChkAlgorithmToStringFormatted(ChkAlgorithm algorithm)
{
	switch (algorithm) {
		default:
		case CHKALG_NONE:
			return nullptr;

		case CHKALG_CRC16:
			return "CRC-16";
		case CHKALG_CRC32:
			return "CRC-32";
		case CHKALG_ADDINVDUAL16:
			return "AddInvDual16";
		case CHKALG_ADDBYTES32:
			return "AddBytes32";
		case CHKALG_SONICCHAOGARDEN:
			return "Sonic Chao Garden";
	}
}

/**
 * Get the checksum field width.
 * @param checksumValues Checksum values to check.
 * @return 4 for 16-bit checksums; 8 for 32-bit checksums.
 */
int Checksum::ChecksumFieldWidth(const vector<ChecksumValue>& checksumValues)
{
	if (checksumValues.empty())
		return 4;

	for (int i = ((int)checksumValues.size() - 1); i >= 0; i--) {
		const Checksum::ChecksumValue &value = checksumValues.at(i);
		if (value.expected > 0xFFFF || value.actual > 0xFFFF) {
			// Checksums are 32-bit.
			return 8;
		}
	}

	// Checksums are 16-bit.
	return 4;
}


/**
 * Get the checksum status.
 * @param checksumValues Checksum values to check.
 * @return Checksum status.
 */
Checksum::ChkStatus Checksum::ChecksumStatus(const vector<ChecksumValue>& checksumValues)
{
	if (checksumValues.empty())
		return Checksum::CHKST_UNKNOWN;

	for (int i = ((int)checksumValues.size() - 1); i >= 0; i--) {
		const Checksum::ChecksumValue &checksumValue = checksumValues.at(i);
		if (checksumValue.expected != checksumValue.actual)
			return Checksum::CHKST_INVALID;
	}

	// All checksums are good.
	return Checksum::CHKST_GOOD;
}


/**
 * Format checksum values as HTML for display purposes.
 * @param checksumValues Checksum values to format.
 * @return QVector containing one or two HTML strings.
 * - String 0 contains the actual checksums.
 * - String 1, if present, contains the expected checksums.
 */
vector<string> Checksum::ChecksumValuesFormatted(const vector<ChecksumValue>& checksumValues)
{
	// Checksum colors.
	// TODO: Better colors?
	static const char s_chkHtmlLinebreak[] = "<br/>";

	// Get the checksum values.
	const int fieldWidth = ChecksumFieldWidth(checksumValues);
	// Assume 34 characters per checksum entry.
	const int reserveSize = ((34 + fieldWidth + 5) * (int)checksumValues.size());

	// Get the checksum status.
	const Checksum::ChkStatus checksumStatus = ChecksumStatus(checksumValues);

	string s_chkActual_all; s_chkActual_all.reserve(reserveSize);
	string s_chkExpected_all;
	if (checksumStatus == Checksum::CHKST_INVALID)
		s_chkExpected_all.reserve(reserveSize);

	for (int i = 0; i < (int)checksumValues.size(); i++) {
		const Checksum::ChecksumValue &value = checksumValues.at(i);

		if (i > 0) {
			// Add linebreaks or spaces to the checksum strings.
			if ((i % 2) && fieldWidth <= 4) {
				// Odd checksum index, 16-bit checksum.
				// Add a space.
				s_chkActual_all += ' ';
				s_chkExpected_all += ' ';
			} else {
				// Add a linebreak.
				s_chkActual_all += s_chkHtmlLinebreak;
				s_chkExpected_all += s_chkHtmlLinebreak;
			}
		}

		char s_chkActual[12];
		char s_chkExpected[12];
		if (fieldWidth <= 4) {
			snprintf(s_chkActual, sizeof(s_chkActual), "%04X", value.actual);
			snprintf(s_chkExpected, sizeof(s_chkExpected), "%04X", value.expected);
		} else {
			snprintf(s_chkActual, sizeof(s_chkActual), "%08X", value.actual);
			snprintf(s_chkExpected, sizeof(s_chkExpected), "%08X", value.expected);
		}

		// Check if the checksum is valid.
		if (value.actual == value.expected) {
			// Checksum is valid.
			s_chkActual_all += "<span style='color: #080'>";
			s_chkActual_all += + s_chkActual;
			s_chkActual_all += "</span>";
			if (checksumStatus == Checksum::CHKST_INVALID) {
				s_chkExpected_all += "<span style='color: #080'>";
				s_chkExpected_all += s_chkExpected;
				s_chkExpected_all += "</span>";
			}
		} else {
			// Checksum is invalid.
			s_chkActual_all += "<span style='color: #F00'>";
			s_chkActual_all += s_chkActual;
			s_chkActual_all += "</span>";
			if (checksumStatus == Checksum::CHKST_INVALID) {
				s_chkExpected_all += "<span style='color: #F00'>";
				s_chkExpected_all += s_chkExpected;
				s_chkExpected_all += "</span>";
			}
		}
	}

	// Return the checksum strings.
	vector<string> ret;
	ret.push_back(s_chkActual_all);
	if (checksumStatus == Checksum::CHKST_INVALID)
		ret.push_back(s_chkExpected_all);
	return ret;
}