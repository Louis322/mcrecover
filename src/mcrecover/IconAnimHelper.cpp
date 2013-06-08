/***************************************************************************
 * GameCube Memory Card Recovery Program.                                  *
 * IconAnimHelper.cpp: Icon animation helper.                              *
 *                                                                         *
 * Copyright (c) 2012-2013 by David Korth.                                 *
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

#include "IconAnimHelper.hpp"

#include "card.h"
#include "MemCardFile.hpp"

class IconAnimHelperPrivate
{
	public:
		IconAnimHelperPrivate(IconAnimHelper *q);

	private:
		IconAnimHelper *const q;
		Q_DISABLE_COPY(IconAnimHelperPrivate);

	public:
		const MemCardFile *file;

		/**
		 * If a file is specified and has an animated icon,
		 * this is set to true.
		 */
		bool enabled;

		// Animation data.
		uint8_t frame;		// Current frame.
		uint8_t lastValidFrame;	// Last valid frame.
		bool frameHasIcon;	// If false, use previous frame.
		uint8_t delayCnt;	// Delay counter.
		uint8_t delayLen;	// Delay length.
		uint8_t mode;		// Animation mode.
		bool direction;		// Current direction for CARD_ANIM_BOUNCE.

		/**
		 * Reset the animation state.
		 */
		void reset(void);

		/**
		 * Timer tick for the animation counter.
		 * @return True if the current icon has been changed; false if not.
		 */
		bool tick(void);
};


IconAnimHelperPrivate::IconAnimHelperPrivate(IconAnimHelper *q)
	: q(q)
	, file(NULL)
{
	reset();
}


/**
 * Reset the animation state.
 */
void IconAnimHelperPrivate::reset(void)
{
	frame = 0;
	lastValidFrame = 0;
	delayCnt = 0;
	direction = false;

	if (!file || file->numIcons() <= 1) {
		// No file specified, or icon is not animated.
		enabled = false;
		frameHasIcon = 0;
		delayLen = 0;
		mode = 0;
	} else {
		// MemCardFile is specified.
		// Determine the initial state.
		enabled = true;
		frameHasIcon = !(file->icon(frame).isNull());
		delayLen = file->iconDelay(frame);
		mode = file->iconAnimMode();
	}
}


/**
 * Timer tick for the animation counter.
 * @return True if the current icon has been changed; false if not.
 */
bool IconAnimHelperPrivate::tick(void)
{
	if (!enabled)
		return false;

	// Check the delay counter.
	delayCnt++;
	if (delayCnt < delayLen) {
		// Animation delay hasn't expired yet.
		return false;
	}

	// Animation delay has expired.
	// Go to the next frame.
	if (!direction) {
		// Animation is moving forwards.
		// Check if we're at the last frame.
		if (frame == (CARD_MAXICONS - 1) ||
		    (file->iconDelay(frame + 1) == CARD_SPEED_END))
		{
			// Last frame.
			if (mode == CARD_ANIM_BOUNCE) {
				// "Bounce" animation. Start playing backwards.
				direction = true;
				frame--;	// Go to the previous frame.
			} else {
				// "Looping" animation.
				// Reset to frame 0.
				frame = 0;
			}
		} else {
			// Not the last frame.
			// Go to the next frame.
			frame++;
		}
	} else {
		// Animation is moving backwards. ("Bounce" animation only.)
		// Check if we're at the first frame.
		if (frame == 0) {
			// First frame. Start playing forwards.
			direction = false;
			frame++;	// Go to the next frame.
		} else {
			// Not the first frame.
			// Go to the previous frame.
			frame--;
		}
	}

	// Update the frame delay data.
	delayCnt = 0;
	delayLen = file->iconDelay(frame);

	// Check if this frame has an icon.
	frameHasIcon = !file->icon(frame).isNull();
	if (frameHasIcon && lastValidFrame != frame) {
		// Frame has an icon. Save this frame as the last valid frame.
		lastValidFrame = frame;

		// Current icon has been updated.
		return true;
	}

	// Current icon has not been updated.
	return false;
}


/** IconAnimHelper **/


IconAnimHelper::IconAnimHelper()
	: d(new IconAnimHelperPrivate(this))
{ }

IconAnimHelper::IconAnimHelper(const MemCardFile* file)
	: d(new IconAnimHelperPrivate(this))
{
	// Set the initial MemCardFile.
	setFile(file);
}

IconAnimHelper::~IconAnimHelper()
{
	delete d;
}


/**
 * Get the MemCardFile this IconAnimHelper is handling.
 * @return MemCardFile.
 */
const MemCardFile *IconAnimHelper::file(void) const
	{ return d->file; }

/**
 * Set the MemCardFile this IconAnimHelper should handle.
 * @param file MemCardFile.
 */
void IconAnimHelper::setFile(const MemCardFile *file)
{
	// Disconnect the MemCardFile's destroyed() signal if a MemCardFile is already set.
	if (d->file) {
		disconnect(d->file, SIGNAL(destroyed(QObject*)),
			   this, SLOT(memCardFile_destroyed_slot(QObject*)));
	}

	d->file = file;

	// Connect the MemCardFile's destroyed() signal.
	if (d->file) {
		connect(d->file, SIGNAL(destroyed(QObject*)),
			this, SLOT(memCardFile_destroyed_slot(QObject*)));
	}

	// Reset the animation state.
	d->reset();
}


/**
 * Reset the animation state.
 * WRAPPER FUNCTION for d->reset().
 */
void IconAnimHelper::reset(void)
	{ d->reset(); }


/**
 * Does this file have an animated icon?
 * @return True if the icon is animated; false if not, or if no file is loaded.
 */
bool IconAnimHelper::isAnimated(void) const
	{ return d->enabled; }


/**
 * Get the current icon for this file.
 * @return Current icon.
 */
QPixmap IconAnimHelper::icon(void) const
{
	if (!d->file)
		return QPixmap();

	// Get the current icon from this file.
	// If the icon is not animated, this will always be icon 0.
	return d->file->icon(d->lastValidFrame);
}


/**
 * Timer tick for the animation counter.
 * WRAPPER FUNCTION for d->tick().
 * @return True if the current icon has been changed; false if not.
 */
bool IconAnimHelper::tick(void)
	{ return d->tick(); }


/** Slots. **/


/**
 * MemCardFile object was destroyed.
 * @param obj QObject that was destroyed.
 */
void IconAnimHelper::memCardFile_destroyed_slot(QObject *obj)
{
	if (obj == d->file) {
		// Our MemCardFile was destroyed.
		d->file = NULL;
		d->reset();
	}
}