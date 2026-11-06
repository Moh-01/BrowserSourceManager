#pragma once

#include "browser-sources.hpp"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

class BulkEditDialog : public QDialog {
	Q_OBJECT

public:
	BulkEditDialog(const std::vector<BrowserSourceInfo> &sources, QWidget *parent = nullptr);

	BrowserEdit Edit() const;

private:
	void UpdateState();

	int onAirCount = 0;

	QCheckBox *sizeEnabled;
	QSpinBox *width;
	QSpinBox *height;

	QCheckBox *fpsEnabled;
	QCheckBox *canvasFps;
	QSpinBox *fps;

	QCheckBox *shutdownEnabled;
	QComboBox *shutdown;

	QCheckBox *restartEnabled;
	QComboBox *restart;

	QCheckBox *audioEnabled;
	QComboBox *audio;

	QLabel *warning;
	QPushButton *apply;
};
