/***************************************************************************
 * GameCube Memory Card Recovery Program.                                  *
 * TimeCodeEdit.cpp: sa_time_code editor widget.                           *
 *                                                                         *
 * Copyright (c) 2016 by David Korth.                                      *
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

#include "TimeCodeEdit.hpp"

// Qt widgets.
#include <QtGui/QHBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QButtonGroup>
#include <QtCore/QSignalMapper>

// Sonic Adventure save file definitions.
#include "sa_defs.h"

/** TimeCodeEditPrivate **/

#include "ui_TimeCodeEdit.h"
class TimeCodeEditPrivate
{
	public:
		TimeCodeEditPrivate(TimeCodeEdit *q);
		~TimeCodeEditPrivate();

	protected:
		TimeCodeEdit *const q_ptr;
		Q_DECLARE_PUBLIC(TimeCodeEdit)
	private:
		Q_DISABLE_COPY(TimeCodeEditPrivate)

	public:
		Ui_TimeCodeEdit ui;

		/**
		 * Are we showing the hours field?
		 * NOTE: spnHours->isVisible() doesn't work if the
		 * window isn't visible, so we're storing the
		 * hours visibility property here.
		 */
		bool showHours;

		/**
		 * Are we showing weights for Big the Cat?
		 * If we are, the MSF fields will show "grams",
		 * and the hours field will always be hidden.
		 */
		bool showWeight;

		// Suppress signals when modifying the QSpinBoxes.
		bool suppressSignals;

		/**
		 * Update the display mode.
		 */
		void updateDisplayMode(void);
};

TimeCodeEditPrivate::TimeCodeEditPrivate(TimeCodeEdit *q)
	: q_ptr(q)
	, showHours(false)
	, showWeight(false)
	, suppressSignals(false)
{ }

TimeCodeEditPrivate::~TimeCodeEditPrivate()
{ }

/**
 * Update the display mode.
 */
void TimeCodeEditPrivate::updateDisplayMode(void)
{
	suppressSignals = true;

	if (showWeight) {
		// Weight mode.

		// Hours is always hidden here.
		ui.spnHours->hide();

		// Adjust the MSF spinboxes to show weight.
		QString suffix = QLatin1String("g");
		ui.spnMinutes->setRange(0, 655350);
		ui.spnMinutes->setSingleStep(10);
		ui.spnMinutes->setSuffix(suffix);
		ui.spnSeconds->setRange(0, 655350);
		ui.spnSeconds->setSingleStep(10);
		ui.spnSeconds->setSuffix(suffix);
		ui.spnFrames->setRange(0, 655350);
		ui.spnFrames->setSingleStep(10);
		ui.spnFrames->setSuffix(suffix);
	} else {
		// Time mode.
		// NOTE: If weight mode was previously set,
		// switching to time mode will result in
		// the display showing weird data.
		ui.spnFrames->setRange(0, 59);
		ui.spnFrames->setSingleStep(1);
		ui.spnFrames->setSuffix(QString());
		ui.spnSeconds->setRange(0, 59);
		ui.spnSeconds->setSingleStep(1);
		ui.spnSeconds->setSuffix(QString());
		// spnMinutes's maximum value is set below.
		ui.spnMinutes->setSingleStep(1);
		ui.spnMinutes->setSuffix(QString());
		ui.spnHours->setRange(0, 11930);
		ui.spnHours->setSingleStep(1);
		ui.spnHours->setSuffix(QString());

		if (showHours) {
			// Show the hours field.
			int minutes = ui.spnMinutes->value();
			if (minutes > 59) {
				ui.spnHours->setValue(minutes % 60);
				ui.spnMinutes->setValue(minutes / 60);
				ui.spnMinutes->setMaximum(59);
				// TODO: Emit valueChanged()?
			}
			ui.spnHours->show();
		} else {
			// Hide the hours field.
			ui.spnMinutes->setMaximum(99);
			int hours = ui.spnHours->value();
			if (hours > 0) {
				int minutes = ui.spnMinutes->value();
				minutes += (hours * 60);
				ui.spnMinutes->setValue(minutes);
			}
			ui.spnHours->hide();
		}
	}

	suppressSignals = false;
}

/** TimeCodeEdit **/

TimeCodeEdit::TimeCodeEdit(QWidget *parent)
	: QWidget(parent)
	, d_ptr(new TimeCodeEditPrivate(this))
{
	Q_D(TimeCodeEdit);
	d->ui.setupUi(this);

	// Don't show the hours field by default.
	d->ui.spnHours->hide();

	// Connect additional signals.
	// FIXME: Qt Designer won't let me connect a signal
	// from a QSpinBox to a TimeCodeEdit signal, so I
	// have to do it here.
	connect(d->ui.spnHours, SIGNAL(valueChanged(int)),
		this, SIGNAL(valueChangedHours(int)));
}

TimeCodeEdit::~TimeCodeEdit()
{
	Q_D(TimeCodeEdit);
	delete d;
}

/** Public functions. **/

/**
 * Set the minutes/seconds/frames using an sa_time_code.
 *
 * If the value is out of range, nothing will be done.
 *
 * If hours are visible, and minutes is larger than 59,
 * hours will be adjusted; if hours is larger than 99,
 * hours will be clamped to 99.
 *
 * If hours are not visible, and minutes is larger than 99,
 * minutes will be clamped to 99.
 *
 * @param time_code [in] sa_time_code.
 */
void TimeCodeEdit::setValue(const sa_time_code *time_code)
{
	Q_D(TimeCodeEdit);
	if (d->showWeight) {
		// Display is in weight mode.
		return;
	}

	// Validate the time code first.
	if (time_code->seconds > 59 || time_code->frames > 59) {
		// Time code is invalid.
		return;
	}

	// NOTE: Suppressing signals this way is not thread-safe.
	d->suppressSignals = true;

	if (d->showHours) {
		// Handle more than 59 minutes as hours.
		// TODO: If more than 99 hours, this will be clamped.
		int hours = time_code->minutes / 60;
		d->ui.spnHours->setValue(hours);
		d->ui.spnMinutes->setValue(time_code->minutes % 60);
	} else {
		// No special processing for minutes.
		d->ui.spnMinutes->setValue(time_code->minutes);
	}

	d->ui.spnSeconds->setValue(time_code->seconds);
	// TODO: Convert to 1/100ths of a second?
	d->ui.spnFrames->setValue(time_code->frames);

	// Allow signals.
	d->suppressSignals = false;
}

/**
 * Get the minutes/seconds/frames as an sa_time_code.
 * @param time_code [out] sa_time_code.
 */
void TimeCodeEdit::value(sa_time_code *time_code) const
{
	Q_D(const TimeCodeEdit);
	if (d->showWeight) {
		// Display is in weight mode.
		// TODO: Error code?
		return;
	}

	time_code->minutes = d->ui.spnMinutes->value();
	if (d->showHours) {
		// Include hours in the time code.
		time_code->minutes += (d->ui.spnHours->value() * 60);
	}

	time_code->seconds = d->ui.spnSeconds->value();
	time_code->frames = d->ui.spnFrames->value();
}

/**
 * Set the three weights.
 *
 * The three values are the weight divided by 10.
 * Range: [0, 65535]
 *
 * If the display mode is time, this function will do nothing.
 *
 * TODO: Pass an array instead of individual weights?
 * @param weights Array of 3 uint16_t weight values.
 */
void TimeCodeEdit::setValue(const uint16_t weights[3])
{
	Q_D(TimeCodeEdit);
	if (!d->showWeight) {
		// Display is in time mode.
		return;
	}

	// NOTE: Suppressing signals this way is not thread-safe.
	d->suppressSignals = true;

	// Set the weights.
	// TODO: Is the casting required?
	d->ui.spnMinutes->setValue((uint32_t)weights[0] * 10);
	d->ui.spnSeconds->setValue((uint32_t)weights[1] * 10);
	d->ui.spnFrames->setValue((uint32_t)weights[2] * 10);

	d->suppressSignals = false;
}

/**
 * Get the three weights.
 * @param weights Array of 3 uint16_t to put the weights in.
 */
void TimeCodeEdit::value(uint16_t weights[3]) const
{
	Q_D(const TimeCodeEdit);
	if (!d->showWeight) {
		// Display is in time mode.
		// TODO: Error code?
		return;
	}

	// Get the weights.
	weights[0] = d->ui.spnMinutes->value() / 10;
	weights[1] = d->ui.spnSeconds->value() / 10;
	weights[2] = d->ui.spnFrames->value() / 10;
}

/**
 * Set the time in NTSC frames. (1/60th of a second)
 * @param ntscFrames Time in NTSC frames.
 */
void TimeCodeEdit::setValueInNtscFrames(uint32_t ntscFrames)
{
	Q_D(TimeCodeEdit);
	if (d->showWeight) {
		// Display is in weight mode.
		// TODO: Error code?
		return;
	}

	d->suppressSignals = true;
	d->ui.spnFrames->setValue(ntscFrames % 60);
	ntscFrames /= 60;
	d->ui.spnSeconds->setValue(ntscFrames % 60);
	ntscFrames /= 60;

	// FIXME: Value may be too large for these fields...
	if (d->showHours) {
		// Set hours and minutes.
		d->ui.spnMinutes->setValue(ntscFrames % 60);
		ntscFrames /= 60;
		d->ui.spnHours->setValue(ntscFrames);
	} else {
		// Set minutes.
		d->ui.spnMinutes->setValue(ntscFrames);
	}

	d->suppressSignals = false;
}

/**
 * Get the time in NTSC frames. (1/60th of a second)
 * @return Time in NTSC frames.
 */
uint32_t TimeCodeEdit::valueInNtscFrames(void) const
{
	Q_D(const TimeCodeEdit);
	if (d->showWeight) {
		// Display is in weight mode.
		// TODO: Error code?
		return 0;
	}

	uint32_t ntscFrames;
	ntscFrames  =  d->ui.spnFrames->value();
	ntscFrames += (d->ui.spnSeconds->value() * 60);
	ntscFrames += (d->ui.spnMinutes->value() * 60 * 60);
	if (d->showHours) {
		ntscFrames += (d->ui.spnHours->value() * 60 * 60 * 60);
	}
	return ntscFrames;
}

// TODO: Add setHours(), setMinutes(), setSeconds(), and setFrames()?

/**
 * Get the hours.
 * @return Hours. (If hours is not visible, this will return 0.)
 */
int TimeCodeEdit::hours(void) const
{
	Q_D(const TimeCodeEdit);
	if (d->showHours && !d->showWeight) {
		return d->ui.spnHours->value();
	}
	return 0;
}

/**
 * Get the minutes.
 * @return Minutes.
 */
int TimeCodeEdit::minutes(void) const
{
	Q_D(const TimeCodeEdit);
	if (!d->showWeight) {
		return d->ui.spnMinutes->value();
	}
	return 0;
}

/**
 * Get the seconds.
 * @return Seconds.
 */
int TimeCodeEdit::seconds(void) const
{
	Q_D(const TimeCodeEdit);
	if (!d->showWeight) {
		return d->ui.spnSeconds->value();
	}
	return 0;
}

/**
 * Get the frames.
 * @return Frames.
 */
int TimeCodeEdit::frames(void) const
{
	Q_D(const TimeCodeEdit);
	if (!d->showWeight) {
		return d->ui.spnFrames->value();
	}
	return 0;
}

/**
 * Set the hours field visibility.
 * @param showHours If true, show hours.
 */
void TimeCodeEdit::setShowHours(bool showHours)
{
	Q_D(TimeCodeEdit);
	if (d->showHours == showHours)
		return;
	d->showHours = showHours;
	d->updateDisplayMode();
}

/**
 * Is the hours field visible?
 * @return True if it is; false if it isn't.
 */
bool TimeCodeEdit::isShowHours(void) const
{
	Q_D(const TimeCodeEdit);
	return d->showHours;
}

/**
 * Set the time/weight mode.
 * @param showWeight If true, show weights; otherwise, show time.
 */
void TimeCodeEdit::setShowWeight(bool showWeight)
{
	Q_D(TimeCodeEdit);
	if (d->showWeight == showWeight)
		return;
	d->showWeight = showWeight;
	d->updateDisplayMode();
}

/**
 * Are we showing time or weights?
 * @return True if showing weights; false if showing time.
 */
bool TimeCodeEdit::isShowWeight(void) const
{
	Q_D(const TimeCodeEdit);
	return d->showWeight;
}

/** Public slots. **/

/**
 * Set the minutes/seconds/frames.
 *
 * If hours are visible, and minutes is larger than 59,
 * hours will be adjusted; if hours is larger than 99,
 * hours will be clamped to 99.
 *
 * If hours are not visible, and minutes is larger than 99,
 * minutes will be clamped to 99.
 *
 * @param minutes [in] Minutes.
 * @param seconds [in] Seconds.
 * @param frames  [in] Frames.
 */
void TimeCodeEdit::setValue(int minutes, int seconds, int frames)
{
	sa_time_code time_code;
	time_code.minutes = minutes;
	time_code.seconds = seconds;
	time_code.frames = frames;
	setValue(&time_code);
}

/**
 * Set the hours value.
 *
 * If hours isn't visible, nothing will be done.
 *
 * @param hours [in] Hours.
 */
void TimeCodeEdit::setValueHours(int hours)
{
	Q_D(TimeCodeEdit);
	if (!d->showHours || d->showWeight)
		return;

	d->suppressSignals = true;
	d->ui.spnHours->setValue(hours);
	d->suppressSignals = false;
}

/** Protected slots. **/

/**
 * One of the minutes/seconds/frames spinboxes has been changed.
 */
void TimeCodeEdit::spinMSFChanged(void)
{
	Q_D(const TimeCodeEdit);
	if (!d->suppressSignals) {
		// TODO: Weight version?
		if (!d->showWeight) {
			emit valueChanged(d->ui.spnMinutes->value(),
					  d->ui.spnSeconds->value(),
					  d->ui.spnFrames->value());
		}
	}
}

/**
 * The hours spinbox has been changed.
 */
void TimeCodeEdit::spinHoursChanged(void)
{
	Q_D(const TimeCodeEdit);
	if (!d->suppressSignals) {
		// TODO: Weight version?
		if (!d->showWeight) {
			emit valueChangedHours(d->ui.spnHours->value());
		}
	}
}