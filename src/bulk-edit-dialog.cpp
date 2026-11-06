#include "bulk-edit-dialog.hpp"

#include <obs-module.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

// Edit here to change the limits in the edit dialog. If you raise them above what the browser source allows, it clamps the value.
constexpr int kMaxSize = 8192;
constexpr int kMaxFps = 60;

QString Text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

QComboBox *OnOffBox(bool on, QWidget *parent)
{
	auto *box = new QComboBox(parent);
	box->addItem(Text("Edit.On"), true);
	box->addItem(Text("Edit.Off"), false);
	box->setCurrentIndex(on ? 0 : 1);
	return box;
}

}

BulkEditDialog::BulkEditDialog(const std::vector<BrowserSourceInfo> &sources, QWidget *parent) : QDialog(parent)
{
	setWindowTitle(Text("Edit.Title").arg(sources.size()));

	const BrowserSourceInfo first = sources.empty() ? BrowserSourceInfo{} : sources.front();
	for (const auto &info : sources) {
		if (info.state == BrowserState::OnAir)
			onAirCount++;
	}

	sizeEnabled = new QCheckBox(Text("Edit.Size"), this);
	width = new QSpinBox(this);
	width->setRange(1, kMaxSize);
	width->setValue(first.width);
	width->setSuffix(QStringLiteral(" px"));
	height = new QSpinBox(this);
	height->setRange(1, kMaxSize);
	height->setValue(first.height);
	height->setSuffix(QStringLiteral(" px"));
	auto *sizeRow = new QHBoxLayout();
	sizeRow->addWidget(width);
	sizeRow->addWidget(new QLabel(QStringLiteral("x"), this));
	sizeRow->addWidget(height);
	sizeRow->addStretch(1);

	fpsEnabled = new QCheckBox(Text("Edit.Fps"), this);
	canvasFps = new QCheckBox(Text("Edit.CanvasFps"), this);
	canvasFps->setChecked(!first.customFps);
	fps = new QSpinBox(this);
	fps->setRange(1, kMaxFps);
	fps->setValue(first.customFps ? first.fps : 30);
	auto *fpsRow = new QHBoxLayout();
	fpsRow->addWidget(canvasFps);
	fpsRow->addWidget(fps);
	fpsRow->addStretch(1);

	shutdownEnabled = new QCheckBox(Text("Edit.Shutdown"), this);
	shutdown = OnOffBox(first.shutdownWhenHidden, this);

	restartEnabled = new QCheckBox(Text("Edit.Restart"), this);
	restart = OnOffBox(first.restartWhenActive, this);

	audioEnabled = new QCheckBox(Text("Edit.Audio"), this);
	audio = OnOffBox(first.rerouteAudio, this);

	auto *grid = new QGridLayout();
	grid->addWidget(sizeEnabled, 0, 0);
	grid->addLayout(sizeRow, 0, 1);
	grid->addWidget(fpsEnabled, 1, 0);
	grid->addLayout(fpsRow, 1, 1);
	grid->addWidget(shutdownEnabled, 2, 0);
	grid->addWidget(shutdown, 2, 1, Qt::AlignLeft);
	grid->addWidget(restartEnabled, 3, 0);
	grid->addWidget(restart, 3, 1, Qt::AlignLeft);
	grid->addWidget(audioEnabled, 4, 0);
	grid->addWidget(audio, 4, 1, Qt::AlignLeft);
	grid->setColumnStretch(1, 1);

	auto *hint = new QLabel(Text("Edit.Hint"), this);
	hint->setWordWrap(true);

	warning = new QLabel(this);
	warning->setWordWrap(true);
	warning->setStyleSheet(QStringLiteral("color: rgb(230, 162, 40);"));

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
	apply = buttons->addButton(Text("Edit.Apply"), QDialogButtonBox::AcceptRole);

	auto *layout = new QVBoxLayout(this);
	layout->addWidget(hint);
	layout->addLayout(grid);
	layout->addWidget(warning);
	layout->addWidget(buttons);

	for (QCheckBox *box : {sizeEnabled, fpsEnabled, shutdownEnabled, restartEnabled, audioEnabled, canvasFps})
		connect(box, &QCheckBox::toggled, this, &BulkEditDialog::UpdateState);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	UpdateState();
}

void BulkEditDialog::UpdateState()
{
	width->setEnabled(sizeEnabled->isChecked());
	height->setEnabled(sizeEnabled->isChecked());
	canvasFps->setEnabled(fpsEnabled->isChecked());
	fps->setEnabled(fpsEnabled->isChecked() && !canvasFps->isChecked());
	shutdown->setEnabled(shutdownEnabled->isChecked());
	restart->setEnabled(restartEnabled->isChecked());
	audio->setEnabled(audioEnabled->isChecked());

	BrowserEdit edit = Edit();
	apply->setEnabled(!edit.IsEmpty());

	bool reloadsOnAir = edit.ReloadsPage() && onAirCount > 0;
	warning->setText(reloadsOnAir ? Text("Edit.OnAirWarning").arg(onAirCount) : QString());
	warning->setVisible(reloadsOnAir);
}

BrowserEdit BulkEditDialog::Edit() const
{
	BrowserEdit edit;
	if (sizeEnabled->isChecked()) {
		edit.width = width->value();
		edit.height = height->value();
	}
	if (fpsEnabled->isChecked()) {
		edit.customFps = !canvasFps->isChecked();
		if (!canvasFps->isChecked())
			edit.fps = fps->value();
	}
	if (shutdownEnabled->isChecked())
		edit.shutdownWhenHidden = shutdown->currentData().toBool();
	if (restartEnabled->isChecked())
		edit.restartWhenActive = restart->currentData().toBool();
	if (audioEnabled->isChecked())
		edit.rerouteAudio = audio->currentData().toBool();
	return edit;
}
