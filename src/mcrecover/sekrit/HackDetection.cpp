/***************************************************************************
 * GameCube Memory Card Recovery Program.                                  *
 * HackDetection.cpp: About Dialog.                                          *
 *                                                                         *
 * Copyright (c) 2013-2014 by David Korth.                                 *
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

#include "HackDetection.hpp"

// Qt includes.
#include <QtCore/QEvent>
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QPainter>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QRawFont>

/** HackDetectionPrivate **/

class HackDetectionPrivate
{
	public:
		HackDetectionPrivate(HackDetection *q);

	protected:
		HackDetection *const q_ptr;
		Q_DECLARE_PUBLIC(HackDetection)
	private:
		Q_DISABLE_COPY(HackDetectionPrivate)

	public:
		QRect winRect;

		// Screen index.
		int screenIdx;
		void setScreen(int screenIdx);

		// Font.
		QFont fntHack;
		int hMargin;
		QChar chrStar;
		QPoint drpTranslate;
		void initFont(void);

		// "Hack Detection" message.
		QString hdTitle;
		QString hdMessage;
		void initMessage(void);
};

HackDetectionPrivate::HackDetectionPrivate(HackDetection* q)
	: q_ptr(q)
	, screenIdx(0)
	, hMargin(0)
{
	// Assume 640x480 for now.
	winRect = QRect(0, 0, 640, 480);
}

/**
 * Set the screen.
 * @param screenIdx Screen index.
 */
void HackDetectionPrivate::setScreen(int screenIdx)
{
	QDesktopWidget *desktop = QApplication::desktop();
	if (screenIdx < 0 || screenIdx >= desktop->numScreens()) {
		// Invalid. Assume the default screen.
		screenIdx = desktop->primaryScreen();
	}

	this->screenIdx = screenIdx;

	// Get the screen dimensions.
	QWidget *screen = QApplication::desktop()->screen(screenIdx);
	winRect = screen->rect();
}

/**
 * Initialize the font.
 */
void HackDetectionPrivate::initFont(void)
{
	/**
	 * Determine the font to use.
	 *
	 * Preferences:
	 * 1. DejaVu Sans Mono
	 * 2. Fixedsys
	 * 3. Courier New
	 */
	static const char *FontNames[] = {
		"DejaVu Sans Mono",
		"Fixedsys",
		"Courier New",
		nullptr
	};

	for (int i = 0; i < (int)(sizeof(FontNames)/sizeof(FontNames[0])-1); i++) {
		fntHack = QFont(QLatin1String(FontNames[i]));
		fntHack.setStyleHint(QFont::TypeWriter);
		if (fntHack.exactMatch())
			break;
	}

	if (!fntHack.exactMatch()) {
		// Cannot find an exact match.
		// Use the system default Monospace font.
		fntHack = QFont(QLatin1String("Monospace"));
		fntHack.setStyleHint(QFont::TypeWriter);
	}

	// Make the font bold and italic.
	fntHack.setBold(true);
	fntHack.setItalic(true);

	// Check if the font has "BLACK STAR" (U+2605).
	QRawFont rawFont = QRawFont::fromFont(fntHack);
	if (rawFont.supportsCharacter(0x2605))
		chrStar = QChar(0x2605);
	else
		chrStar = QChar(L'*');

	/**
	 * With 640x480, the original "Hack Detection"
	 * used an 18px font. Calculate the font size
	 * relative to 18/480.
	 */
	int fntPx = (winRect.height() * 18 / 480);
	fntHack.setPixelSize(fntPx);

	/**
	 * Drop shadow should be 2px for 18px font.
	 */
	int drpSz = (fntPx / 9);
	drpTranslate = QPoint(drpSz, drpSz);

	/**
	 * Horizontal margin was around 40px on 640x480.
	 * Calculate the margin relative to 40/640.
	 */
	hMargin = (winRect.width() * 40 / 640);
}

/**
 * Initialize the message.
 * This may be called if the UI language is changed.
 */
void HackDetectionPrivate::initMessage(void)
{
	//: "Hack Detection" title.
	hdTitle = HackDetection::tr("Hack Detection").toUpper();
	//: "Hack Detection" message. Preserve the linebreaks!
	hdMessage = HackDetection::tr(
		"One or more game resources were manipulated by an\n"
		"outside source. This is not allowed as specified in\n"
		"the game license.\n"
		"You must reinstall the game and accept the game\n"
		"license again, to continue to play the game.\n"
		"\n"
		"Game halted.").toUpper();
}

/** HackDetection **/

/**
 * Create a Hack Detection window.
 * Uses the default screen.
 * @param parent Parent.
 */
HackDetection::HackDetection(QWidget *parent)
	: QWidget(parent,
		Qt::Window |
		Qt::WindowStaysOnTopHint |
		Qt::FramelessWindowHint |
		Qt::CustomizeWindowHint)
	, d_ptr(new HackDetectionPrivate(this))
{
	// Default screen index.
	init(QApplication::desktop()->primaryScreen());
}

/**
 * Create a Hack Detection window.
 * @param parent Parent.
 * @param screen Screen index.
 */
HackDetection::HackDetection(int screen, QWidget *parent)
	: QWidget(parent, Qt::Window)
	, d_ptr(new HackDetectionPrivate(this))
{
	init(screen);
}

/**
 * Initialize the Hack Detection window.
 * (Called from the constructor.)
 * @param screen Screen index.
 */
void HackDetection::init(int screen)
{
	// Make sure the window is deleted on close.
	this->setAttribute(Qt::WA_DeleteOnClose, true);

#ifdef Q_OS_MAC
	// Remove the window icon. (Mac "proxy icon")
	this->setWindowIcon(QIcon());
#endif

	// We're painting the entire window.
	this->setAttribute(Qt::WA_OpaquePaintEvent, true);

	Q_D(HackDetection);
	d->setScreen(screen);

	// Initialize the window size.
	this->setMinimumSize(d->winRect.size());
	this->setMaximumSize(d->winRect.size());
	this->setFixedSize(d->winRect.size());
	this->setBaseSize(d->winRect.size());

	// Initialize the window position.
	this->move(d->winRect.topLeft());

	// TODO: Set always on top.

	// Initialize the font.
	d->initFont();
	// Initialize the message.
	d->initMessage();

	// Set fullscreen.
	this->showFullScreen();
}

/**
 * Shut down the About Dialog.
 */
HackDetection::~HackDetection()
{
	delete d_ptr;
}

/** Events. **/

/**
 * Widget state has changed.
 * @param event State change event.
 */
void HackDetection::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::LanguageChange) {
		// Retranslate the UI.
		Q_D(HackDetection);
		// TODO
	}

	// Pass the event to the base class.
	this->QWidget::changeEvent(event);
}

/**
 * Paint event.
 * @param event QPaintEvent.
 */
#include <QtGui/QPaintEvent>
void HackDetection::paintEvent(QPaintEvent *event)
{
	// TODO: Use clipping region?
	Q_UNUSED(event);

	QPainter painter(this);
	painter.setRenderHint(QPainter::TextAntialiasing);

	/** Colors **/

	// Background color: #006600
	QBrush brushBg(QColor(0x00, 0x66, 0x00));
	// Title text color: #FFFF00
	QColor colorTxtTitle(0xFF, 0xFF, 0x00);
	// Message text color: #FFFFFF
	QColor colorTxtMessage(0xFF, 0xFF, 0xFF);
	// Drop shadow text color: #000000
	QColor colorDropShadow(0x00, 0x00, 0x00);

	// Draw the background.
	painter.fillRect(this->rect(), brushBg);

	Q_D(HackDetection);
	QRect margins(d->hMargin, d->hMargin,
		      (d->winRect.width() - (d->hMargin * 2)),
		      (d->winRect.height() - (d->hMargin * 2)));

	// Initialize the font metrics.
	QFontMetrics mtrHack(d->fntHack);

	// Add stars to the title.
	QString drawTitle = (d->chrStar + d->hdTitle + d->chrStar);
	// Calculate the title margins.
	QRect rectTitle = mtrHack.boundingRect(margins, Qt::AlignHCenter, drawTitle);

	// Calculate the message margins.
	QRect rectMessage = mtrHack.boundingRect(margins, 0, d->hdMessage);

	// Total height of the two messages.
	int height = (rectTitle.height() * 2) + rectMessage.height();
	int x = ((d->winRect.height() - height) / 2);

	// Center the messages on the screen.
	rectTitle.moveTop(x);
	rectMessage.moveTop(x + (rectTitle.height() * 2));

	// Draw the title.
	painter.setFont(d->fntHack);
	// (drop shadow)
	QRect drpRect = rectTitle;
	drpRect.translate(d->drpTranslate);
	painter.setPen(colorDropShadow);
	painter.drawText(drpRect, 0, drawTitle);
	// (regular text)
	painter.setPen(colorTxtTitle);
	painter.drawText(rectTitle, 0, drawTitle);

	// Draw the message.
	// (drop shadow)
	drpRect = rectMessage;
	drpRect.translate(d->drpTranslate);
	painter.setPen(colorDropShadow);
	painter.drawText(drpRect, 0, d->hdMessage);
	// (regular text)
	painter.setPen(colorTxtMessage);
	painter.drawText(rectMessage, 0, d->hdMessage);
}

/**
 * Minimum size hint.
 * @return Minimum size hint.
 */
QSize HackDetection::minimumSizeHint(void) const
{
	Q_D(const HackDetection);
	return d->winRect.size();
}

/**
 * Size hint.
 * @return Size hint.
 */
QSize HackDetection::sizeHint(void) const
{
	Q_D(const HackDetection);
	return d->winRect.size();
}
