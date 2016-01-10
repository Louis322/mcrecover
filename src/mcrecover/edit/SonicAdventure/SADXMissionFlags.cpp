/***************************************************************************
 * GameCube Memory Card Recovery Program.                                  *
 * SADXMissionFlags.cpp: Sonic Adventure DX - Mission flags.               *
 *                                                                         *
 * Copyright (c) 2015 by David Korth.                                      *
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

#include "SADXMissionFlags.hpp"
#include "SAData.h"

// TODO: Put this in a common header file somewhere.
#define NUM_ELEMENTS(x) ((int)(sizeof(x) / sizeof(x[0])))

/** SADXMissionFlagsPrivate **/
#include "ByteFlags_p.hpp"
class SADXMissionFlagsPrivate : public ByteFlagsPrivate
{
	public:
		SADXMissionFlagsPrivate();

	private:
		Q_DISABLE_COPY(SADXMissionFlagsPrivate)

	public:
		static const int flagCount = 60;

		// Pixmaps.
		QPixmap pixmaps[6];
};

// NOTE: NUM_ELEMENTS() includes the NULL-terminator.
SADXMissionFlagsPrivate::SADXMissionFlagsPrivate()
	: ByteFlagsPrivate(flagCount, &sadx_mission_flags_desc[0], NUM_ELEMENTS(sadx_mission_flags_desc)-1)
{
	// Load the pixmaps. (TODO: Once per class?)
	static const char *const pixmap_filenames[NUM_ELEMENTS(pixmaps)] = {
		":/sonic/SA1/sonic.png",
		":/sonic/SA1/tails.png",
		":/sonic/SA1/knuckles.png",
		":/sonic/SA1/amy.png",
		":/sonic/SA1/gamma.png",
		":/sonic/SA1/big.png",
	};

	for (int i = 0; i < NUM_ELEMENTS(pixmaps); i++) {
		QPixmap pxm = QPixmap(QLatin1String(pixmap_filenames[i]));
		// Reduce to 16x16. [TODO: Both sizes for high-DPI?]
		pixmaps[i] = pxm.scaled(16, 16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}
}

/** SADXMissionFlags **/

SADXMissionFlags::SADXMissionFlags(QObject *parent)
	: ByteFlags(new SADXMissionFlagsPrivate(), parent)
{ }

/**
 * Get a description of the type of object that is represented by the class.
 * @return Flag type, e.g. "Mission".
 */
QString SADXMissionFlags::objectType(void) const
{
	return tr("Mission");
}

/**
 * Get a description of the type of flag represented by a given bit.
 * @param bit Bit index. (LSB == 0)
 * @return Flag type, e.g. "Completed". (If bit is unused, empty QString is returned.)
 */
QString SADXMissionFlags::flagType(int bit) const
{
	// TODO: Flag ordering for display purposes.
	// SADXMissionFlags should be: Unlocked, Active, Completed
	switch (bit) {
		case 0:	return tr("Active");
		case 6:	return tr("Unlocked");
		case 7:	return tr("Completed");
		default: break;
	}

	// Unused flag.
	// Don't show it.
	return QString();
}

/**
 * Get a character icon representing a flag.
 * TODO: Make this more generic?
 * @param id Object ID.
 * @return Character icon.
 */
QPixmap SADXMissionFlags::icon(int id) const
{
	if (id < 0 || id >= NUM_ELEMENTS(sadx_mission_flags_char))
		return QPixmap();

	Q_D(const SADXMissionFlags);
	return d->pixmaps[sadx_mission_flags_char[id]];
}