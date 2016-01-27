/***************************************************************************
 * GameCube Memory Card Recovery Program.                                  *
 * ByteFlags.cpp: Generic byte flags base class.                           *
 * Used for things where a single object has multiple flags                *
 * stored as a byte.                                                       *
 *                                                                         *
 * Copyright (c) 2015-2016 by David Korth.                                 *
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

#include "ByteFlags.hpp"
#include "ByteFlags_p.hpp"

// C includes. (C++ namespace)
#include <cassert>

// TODO: Put this in a common header file somewhere.
#define NUM_ELEMENTS(x) ((int)(sizeof(x) / sizeof(x[0])))

/** ByteFlagsPrivate **/

ByteFlagsPrivate::ByteFlagsPrivate(int total_flags, const bit_flag_t *byte_flags, int count)
{
	// This is initialized by a derived private class.
	assert(total_flags > 0);
	assert(total_flags >= count);
	assert(count >= 0);

	// Initialize flags.
	// QVector automatically initializes the new elements to false.
	objs.resize(total_flags);

	// Initialize flags_desc.
	// TODO: Once per derived class, rather than once per instance?
	objs_desc.clear();
	objs_desc.reserve(count);
	for (int i = 0; i < count; i++, byte_flags++) {
		if (byte_flags->event < 0 || !byte_flags->description) {
			// End of list.
			// NOTE: count should have been set correctly...
			break;
		}

		objs_desc.insert(byte_flags->event, QLatin1String(byte_flags->description));
	}
}

ByteFlagsPrivate::~ByteFlagsPrivate()
{ }

/** ByteFlags **/

ByteFlags::ByteFlags(ByteFlagsPrivate *d, QObject *parent)
	: QObject(parent)
	, d_ptr(d)
{ }

ByteFlags::~ByteFlags()
{
	Q_D(ByteFlags);
	delete d;
}

/**
 * Get the total number of objects.
 * @return Total number of objects.
 */
int ByteFlags::count(void) const
{
	Q_D(const ByteFlags);
	return d->objs.size();
}

/**
 * Get an object's description.
 * @param id Object ID.
 * @return Description.
 */
QString ByteFlags::description(int id) const
{
	// TODO: Translate using the subclass?
	if (id < 0 || id >= count())
		return tr("Invalid object ID");

	Q_D(const ByteFlags);
	return d->objs_desc.value(id, tr("Unknown"));
}

/**
 * Get an object's flags.
 * @param id Object ID.
 * @return Object's flags.
 */
uint8_t ByteFlags::flag(int id) const
{
	if (id < 0 || id >= count())
		return false;

	Q_D(const ByteFlags);
	return d->objs.at(id);
}

/**
 * Set an object's flags.
 * @param id Object ID.
 * @param value New flag value.
 */
void ByteFlags::setFlag(int id, uint8_t value)
{
	if (id < 0 || id >= count())
		return;

	Q_D(ByteFlags);
	d->objs[id] = value;
	emit flagChanged(id, value);
}

/**
 * Get the object flags as an array of bytes.
 *
 * If the array doesn't match the size of this ByteFlags:
 * - Too small: Array will be used for the first sz flags.
 * - Too big: Array will be used for count() flags.
 *
 * TODO: Various byte flag encodings.
 *
 * @param data Byte flags.
 * @param sz Number of bytes in data.
 * @return Number of byte flags retrieved.
 */
int ByteFlags::allFlags(uint8_t *data, int sz) const
{
	Q_D(const ByteFlags);
	assert(sz > 0);
	if (sz <= 0)
		return 0;
	if (sz > d->objs.count())
		sz = d->objs.count();

	memcpy(data, d->objs.constData(), sz);
	return sz;
}

/**
 * Set the bit flags from an array of bytes.
 *
 * If the array doesn't match the size of this ByteFlags:
 * - Too small: Array will be used for the first sz flags.
 * - Too big: Array will be used for count() flags.
 *
 * TODO: Various byte flag encodings.
 *
 * @param data Byte flags.
 * @param sz Number of bytes in data.
 * @return Number of byte flags loaded.
 */
int ByteFlags::setAllFlags(const uint8_t *data, int sz)
{
	Q_D(ByteFlags);
	assert(sz > 0);
	if (sz <= 0)
		return 0;
	if (sz > d->objs.count())
		sz = d->objs.count();

	memcpy(d->objs.data(), data, sz);
	emit flagsChanged(0, sz-1);
	return sz;
}

/**
 * Get a character icon representing a flag.
 * TODO: Make this more generic?
 * @param id Object ID.
 * @return Character icon.
 */
QPixmap ByteFlags::icon(int id) const
{
	// No icons by default...
	return QPixmap();
}