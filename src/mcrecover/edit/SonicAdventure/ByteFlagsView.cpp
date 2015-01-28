/***************************************************************************
 * GameCube Memory Card Recovery Program.                                  *
 * ByteFlagsView.hpp: Byte Flags editor.                                   *
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

#include "ByteFlagsView.hpp"

#include "ByteFlags.hpp"
#include "ByteFlagsModel.hpp"
#include "PageFilterModel.hpp"
#include "CenteredCheckBoxDelegate.hpp"

// C includes. (C++ namespace)
#include <cassert>

// Sonic Adventure save file definitions.
#include "sa_defs.h"

// TODO: Put this in a common header file somewhere.
#define NUM_ELEMENTS(x) ((int)(sizeof(x) / sizeof(x[0])))

/** ByteFlagsViewPrivate **/

#include "ui_ByteFlagsView.h"
class ByteFlagsViewPrivate
{
	public:
		ByteFlagsViewPrivate(ByteFlagsView *q);

	protected:
		ByteFlagsView *const q_ptr;
		Q_DECLARE_PUBLIC(ByteFlagsView)
	private:
		Q_DISABLE_COPY(ByteFlagsViewPrivate)

	public:
		Ui_ByteFlagsView ui;

		// Page Filter model. (owned by this widget)
		PageFilterModel *pageFilterModel;

		// Default page size.
		static const int defaultPageSize = 64;

		/**
		 * Update the display.
		 */
		void updateDisplay(void);
};

ByteFlagsViewPrivate::ByteFlagsViewPrivate(ByteFlagsView *q)
	: q_ptr(q)
	, pageFilterModel(nullptr)
{
	// Initialize the page filter model.
	pageFilterModel = new PageFilterModel(q);
	pageFilterModel->setPageSize(defaultPageSize);
}

/**
 * Update the display.
 */
void ByteFlagsViewPrivate::updateDisplay(void)
{
	QAbstractItemModel *const model = pageFilterModel->sourceModel();
	if (!model) {
		// No model is set.
		// TODO: Keep tabs visible?
		ui.tabBar->setVisible(false);
		return;
	}

	// TODO: Add/remove tabs as necessary.
	// For now, just hide the entire tab bar if it's a single page.
	ui.tabBar->setVisible(pageFilterModel->pageCount() > 1);

	// Resize the columns to fit the contents.
	// TODO: On theme change, pageSize change...?
	for (int i = ByteFlagsModel::COL_BIT0; i < ByteFlagsModel::COL_BIT7; i++) {
		ui.lstEventFlags->resizeColumnToContents(i);
	}

	// ID should be as wide as the largest ID number.
	QFontMetrics fm = ui.lstEventFlags->fontMetrics();
	int id_width = fm.width(QString::number(model->rowCount()-1));
	// FIXME: Add text margins. For now, just add width of 'W'.
	id_width += fm.width(QChar(L'W'));
	ui.lstEventFlags->setColumnWidth(ByteFlagsModel::COL_ID, id_width+1);
	
	// Event Description and overall width.
	//ui.lstEventFlags->resizeColumnToContents(ByteFlagsModel::COL_DESCRIPTION);
	ui.lstEventFlags->resizeColumnToContents(model->columnCount());
}

/** ByteFlagsView **/

ByteFlagsView::ByteFlagsView(QWidget *parent)
	: QWidget(parent)
	, d_ptr(new ByteFlagsViewPrivate(this))
{
	Q_D(ByteFlagsView);
	d->ui.setupUi(this);

	// Set lstEventFlags' model.
	d->ui.lstEventFlags->setModel(d->pageFilterModel);

	// Set up a CentereCheckBoxDelegate.
	d->ui.lstEventFlags->setItemDelegate(new CenteredCheckBoxDelegate(this));
	d->ui.lstEventFlags->header()->setStretchLastSection(false);

	// NOTE: QTabBar is initialized after the model is set to prevent
	// signals from being triggered before pageFilterModel is valid.

	// Qt Designer doesn't have QTabBar, so we have to set it up manually.
	// Disable expanding so the tabs look like normal tabs.
	// Disable drawBase because the tabs are right above the QTreeView.
	// (FIXME: With drawBase disabled, there's a 1px empty space for unselected tabs on Win7...)
	// TODO: Retranslate the tabs on language change.
	// TODO: Parent should set the tab names...
	d->ui.tabBar->setExpanding(false);
	d->ui.tabBar->setDrawBase(true);
	d->ui.tabBar->addTab(tr("Unused?"));
	d->ui.tabBar->addTab(tr("General"));
	d->ui.tabBar->addTab(tr("Sonic"));
	d->ui.tabBar->addTab(tr("Tails"));
	d->ui.tabBar->addTab(tr("Knuckles"));
	d->ui.tabBar->addTab(tr("Amy"));
	d->ui.tabBar->addTab(tr("Gamma"));
	d->ui.tabBar->addTab(tr("Big"));

	// Update the display.
	d->updateDisplay();

	// Connect tabBar's signals.
	connect(d->ui.tabBar, SIGNAL(currentChanged(int)),
		d->pageFilterModel, SLOT(setCurrentPage(int)));
	connect(d->pageFilterModel, SIGNAL(currentPageChanged(int)),
		d->ui.tabBar, SLOT(setCurrentIndex(int)));
}

ByteFlagsView::~ByteFlagsView()
{
	Q_D(ByteFlagsView);
	delete d;
}

/** Model access. **/

/**
 * Get the ByteFlagsModel this widget is editing.
 * @return ByteFlagsModel.
 */
ByteFlagsModel *ByteFlagsView::bitFlagsModel(void) const
{
	Q_D(const ByteFlagsView);
	return qobject_cast<ByteFlagsModel*>(d->pageFilterModel->sourceModel());
}

/**
 * Set the ByteFlagsModel to edit.
 * @param bitFlagsModel ByteFlagsModel.
 */
void ByteFlagsView::setByteFlagsModel(ByteFlagsModel *bitFlagsModel)
{
	// TODO: Connect destroyed() signal for ByteFlagsModel?
	Q_D(ByteFlagsView);
	d->pageFilterModel->setSourceModel(bitFlagsModel);

	// Update the QTabBar.
	d->updateDisplay();
}

/** Data access. **/

/**
 * Get the page size.
 * @return Page size.
 */
int ByteFlagsView::pageSize(void) const
{
	Q_D(const ByteFlagsView);
	return d->pageFilterModel->pageSize();
}

/**
 * Set the page size.
 * @param pageSize Page size.
 */
void ByteFlagsView::setPageSize(int pageSize)
{
	Q_D(ByteFlagsView);
	// TODO: Signal from pageFilterModel to adjust tabs?
	d->pageFilterModel->setPageSize(pageSize);

	// Update the display.
	d->updateDisplay();
}

// TODO: Page count?
// TODO: Set tab names.
