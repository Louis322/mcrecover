/***************************************************************************
 * GameCube Memory Card Recovery Program.                                  *
 * BitFlagsModel.cpp: QAbstractListModel for BitFlags.                     *
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

// Reference: http://programmingexamples.net/wiki/Qt/ModelView/AbstractListModelCheckable

#include "BitFlagsModel.hpp"
#include "sa_defs.h"

// C includes.
#include <limits.h>

// Qt includes.
#include <QtCore/QHash>

// Event flags.
#include "BitFlags.hpp"

/** BitFlagsModelPrivate **/

class BitFlagsModelPrivate
{
	public:
		BitFlagsModelPrivate(BitFlagsModel *q);

	protected:
		BitFlagsModel *const q_ptr;
		Q_DECLARE_PUBLIC(BitFlagsModel)
	private:
		Q_DISABLE_COPY(BitFlagsModelPrivate)

	public:
		// BitFlags.
		// TODO: Signals for data changes?
		BitFlags *bitFlags;

		/**
		 * Cached copy of bitFlags->count().
		 * This value is needed after the card is destroyed,
		 * so we need to cache it here, since the destroyed()
		 * slot might be run *after* the Card is deleted.
		 */
		int flagCount;
};

BitFlagsModelPrivate::BitFlagsModelPrivate(BitFlagsModel *q)
	: q_ptr(q)
	, bitFlags(nullptr)
	, flagCount(0)
{ }

/** BitFlagsModel **/

BitFlagsModel::BitFlagsModel(QObject *parent)
	: QAbstractListModel(parent)
	, d_ptr(new BitFlagsModelPrivate(this))
{ }

BitFlagsModel::~BitFlagsModel()
{
	Q_D(BitFlagsModel);
	delete d;
}

/** Qt Model/View interface. **/

int BitFlagsModel::rowCount(const QModelIndex& parent) const
{
	Q_UNUSED(parent)
	Q_D(const BitFlagsModel);
	return (d->bitFlags ? d->bitFlags->count() : 0);
}

int BitFlagsModel::columnCount(const QModelIndex& parent) const
{
	Q_UNUSED(parent);
	Q_D(const BitFlagsModel);
	// Only one column: Event name.
	return (d->bitFlags ? 1 : 0);
}

// FIXME: Backport some stuff to MemCardModel.
QVariant BitFlagsModel::data(const QModelIndex& index, int role) const
{
	Q_D(const BitFlagsModel);
	if (!d->bitFlags)
		return QVariant();
	if (!index.isValid())
		return QVariant();
	if (index.row() < 0 || index.row() >= rowCount())
		return QVariant();
	if (index.column() != 0)
		return QVariant();

	switch (role) {
		case Qt::DisplayRole:
			return d->bitFlags->description(index.row());

		case Qt::CheckStateRole:
			// TODO
			return (d->bitFlags->flag(index.row()) ? Qt::Checked : Qt::Unchecked);

		default:
			break;
	}

	// Default value.
	return QVariant();
}

QVariant BitFlagsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	Q_UNUSED(orientation);
	Q_D(const BitFlagsModel);
	if (!d->bitFlags)
		return QVariant();
	if (section != 0)
		return QVariant();

	switch (role) {
		case Qt::DisplayRole:
			if (section == 0)
				return tr("Event");
			break;

		default:
			break;
	}

	// Default value.
	return QVariant();
}

Qt::ItemFlags BitFlagsModel::flags(const QModelIndex &index) const
{
	Q_D(const BitFlagsModel);
	if (!d->bitFlags)
		return Qt::NoItemFlags;
	if (!index.isValid())
		return Qt::NoItemFlags;
	if (index.row() < 0 || index.row() >= rowCount())
		return Qt::NoItemFlags;

	return (Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
}

bool BitFlagsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	Q_D(BitFlagsModel);
	if (!d->bitFlags)
		return false;
	if (!index.isValid())
		return false;
	if (index.row() < 0 || index.row() >= rowCount())
		return false;
	if (index.column() != 0)
		return false;

	switch (role) {
		case Qt::CheckStateRole:
			// Event flag value has changed.
			// TODO: Map row to event ID.
			d->bitFlags->setFlag(index.row(), (value.toUInt() == Qt::Checked));
			break;

		default:
			// Unsupported.
			return false;
	}

	// Data has changed.
	emit dataChanged(index, index);
	return true;
}

/** Data access. **/

/**
 * Get the BitFlags this model is showing.
 * @return BitFlags this model is showing.
 */
BitFlags *BitFlagsModel::bitFlags(void) const
{
	Q_D(const BitFlagsModel);
	return d->bitFlags;
}

/**
 * Set the BitFlags for this model to show.
 * @param bitFlags BitFlags to show.
 */
void BitFlagsModel::setBitFlags(BitFlags *bitFlags)
{
	Q_D(BitFlagsModel);

	// Disconnect the BitFlags's destroyed() signal if BitFlags is already set.
	if (d->bitFlags) {
		// Notify the view that we're about to remove all rows.
		// TODO: flagCount should already be cached...
		const int flagCount = d->bitFlags->count();
		if (flagCount > 0)
			beginRemoveRows(QModelIndex(), 0, (flagCount - 1));

		// Disconnect the BitFlags's signals.
		disconnect(d->bitFlags, SIGNAL(destroyed(QObject*)),
			   this, SLOT(bitFlags_destroyed_slot(QObject*)));

		d->bitFlags = nullptr;

		// Done removing rows.
		d->flagCount = 0;
		if (flagCount > 0)
			endRemoveRows();
	}

	// Connect the bitFlags's destroyed() signal.
	if (bitFlags) {
		// Notify the view that we're about to add rows.
		const int flagCount = bitFlags->count();
		if (flagCount > 0)
			beginInsertRows(QModelIndex(), 0, (flagCount - 1));

		// Set the BitFlags.
		d->bitFlags = bitFlags;

		// Connect the BitFlags's signals.
		connect(d->bitFlags, SIGNAL(destroyed(QObject*)),
			this, SLOT(bitFlags_destroyed_slot(QObject*)));

		// Done adding rows.
		if (flagCount > 0) {
			d->flagCount = flagCount;
			endInsertRows();
		}
	}
}

/** Slots. **/

/**
 * BitFlags object was destroyed.
 * @param obj QObject that was destroyed.
 */
void BitFlagsModel::bitFlags_destroyed_slot(QObject *obj)
{
	Q_D(BitFlagsModel);

	if (obj == d->bitFlags) {
		// Our Card was destroyed.
		d->bitFlags = nullptr;
		int old_flagCount = d->flagCount;
		if (old_flagCount > 0)
			beginRemoveRows(QModelIndex(), 0, (old_flagCount - 1));
		d->flagCount = 0;
		if (old_flagCount > 0)
			endRemoveRows();
	}
}