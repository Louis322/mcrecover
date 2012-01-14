/***************************************************************************
 * GameCube Memory Card Recovery Program.                                  *
 * MemCardFile.hpp: Memory Card file entry class.                          *
 *                                                                         *
 * Copyright (c) 2011 by David Korth.                                      *
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

#ifndef __MCRECOVER_MEMCARDFILE_HPP__
#define __MCRECOVER_MEMCARDFILE_HPP__

#include "card.h"

#include <QtCore/QObject>
#include <QtCore/QDateTime>

// MemCard class.
class MemCard;

class MemCardFilePrivate;

class MemCardFile : public QObject
{
	Q_OBJECT
	
	public:
		MemCardFile(MemCard *card, const int fileIdx,
				const card_dat *dat, const card_bat *bat);
		~MemCardFile();
	
	private:
		friend class MemCardFilePrivate;
		MemCardFilePrivate *const d;
		Q_DISABLE_COPY(MemCardFile);
	
	public:
		/**
		 * Get the game code.
		 * @return Game code.
		 */
		QString gamecode(void) const;
		
		/**
		 * Get the company code.
		 * @return Company code.
		 */
		QString company(void) const;
		
		/**
		 * Get the GC filename.
		 * @return GC filename.
		 */
		QString filename(void) const;
		
		/**
		 * Get the last modified time.
		 * @return Last modified time.
		 */
		QDateTime lastModified(void) const;
		
		/**
		 * Get the game description. ("Comments" field.)
		 * @return Game description.
		 */
		QString gameDesc(void) const;
		
		/**
		 * Get the file description. ("Comments" field.)
		 * @return File description.
		 */
		QString fileDesc(void) const;
};

#endif /* __MCRECOVER_MEMCARDFILE_HPP__ */