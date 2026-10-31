/*
Browser Source Manager
Copyright (C) 2026 Moh-01

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "browser-dock.hpp"
#include "browser-sources.hpp"

#include <obs-module.h>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QMessageBox>
#include <QSet>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {

enum Column { ColState, ColName, ColPage, ColSize, ColFps, ColScenes, ColCount };

constexpr int UuidRole = Qt::UserRole;
constexpr int SortRole = Qt::UserRole + 1;
constexpr int StateRole = Qt::UserRole + 2;
constexpr int ShutdownRole = Qt::UserRole + 3;
constexpr int UrlRole = Qt::UserRole + 4;
constexpr int SearchRole = Qt::UserRole + 5;

// Filter combo entries, in order. -1 means "all states".
constexpr int kFilterAll = -1;

QString Text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

QString StateLabel(BrowserState state)
{
	switch (state) {
	case BrowserState::OnAir:
		return Text("State.OnAir");
	case BrowserState::Preview:
		return Text("State.Preview");
	case BrowserState::RunningHidden:
		return Text("State.RunningHidden");
	case BrowserState::Unloaded:
		return Text("State.Unloaded");
	}
	return {};
}

QString StateTooltip(BrowserState state)
{
	switch (state) {
	case BrowserState::OnAir:
		return Text("State.OnAir.Tooltip");
	case BrowserState::Preview:
		return Text("State.Preview.Tooltip");
	case BrowserState::RunningHidden:
		return Text("State.RunningHidden.Tooltip");
	case BrowserState::Unloaded:
		return Text("State.Unloaded.Tooltip");
	}
	return {};
}

QColor StateColor(BrowserState state)
{
	switch (state) {
	case BrowserState::OnAir:
		return QColor(63, 185, 80);
	case BrowserState::Preview:
		return QColor(88, 166, 255);
	case BrowserState::RunningHidden:
		return QColor(230, 162, 40);
	case BrowserState::Unloaded:
		return QColor(139, 148, 158);
	}
	return {};
}

// A colored dot drawn as an icon. Themes restyle item text colors, but not icons.
QIcon StateIcon(BrowserState state)
{
	static QIcon icons[4];
	QIcon &icon = icons[(int)state];
	if (icon.isNull()) {
		QPixmap pixmap(24, 24);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(Qt::NoPen);
		painter.setBrush(StateColor(state));
		painter.drawEllipse(QRectF(5, 5, 14, 14));
		icon = QIcon(pixmap);
	}
	return icon;
}

// Short, token-free label for the page: the host for web pages, the file name for local files.
QString PageLabel(const BrowserSourceInfo &info)
{
	QString url = QString::fromStdString(info.url);
	if (url.isEmpty())
		return Text("Page.None");
	if (info.isLocalFile)
		return QFileInfo(url).fileName();

	QUrl parsed(url);
	QString host = parsed.host();
	return host.isEmpty() ? url : host;
}

class SortableItem : public QTreeWidgetItem {
public:
	using QTreeWidgetItem::QTreeWidgetItem;

	bool operator<(const QTreeWidgetItem &other) const override
	{
		int column = treeWidget() ? treeWidget()->sortColumn() : 0;
		QVariant a = data(column, SortRole);
		QVariant b = other.data(column, SortRole);
		if (a.isValid() && b.isValid())
			return a.toLongLong() < b.toLongLong();
		return text(column).localeAwareCompare(other.text(column)) < 0;
	}
};

} // namespace

BrowserDock::BrowserDock(QWidget *parent) : QWidget(parent)
{
	search = new QLineEdit(this);
	search->setPlaceholderText(Text("Search"));
	search->setClearButtonEnabled(true);

	stateFilter = new QComboBox(this);
	stateFilter->addItem(Text("Filter.All"), kFilterAll);
	stateFilter->addItem(Text("State.OnAir"), (int)BrowserState::OnAir);
	stateFilter->addItem(Text("State.Preview"), (int)BrowserState::Preview);
	stateFilter->addItem(Text("State.RunningHidden"), (int)BrowserState::RunningHidden);
	stateFilter->addItem(Text("State.Unloaded"), (int)BrowserState::Unloaded);

	auto *refreshAll = new QToolButton(this);
	refreshAll->setText(Text("RefreshAll"));
	refreshAll->setToolTip(Text("RefreshAll.Tooltip"));

	auto *unloadHidden = new QToolButton(this);
	unloadHidden->setText(Text("UnloadHidden"));
	unloadHidden->setToolTip(Text("UnloadHidden.Tooltip"));

	auto *topRow = new QHBoxLayout();
	topRow->addWidget(search, 1);
	topRow->addWidget(stateFilter);

	auto *actionRow = new QHBoxLayout();
	actionRow->addWidget(refreshAll);
	actionRow->addWidget(unloadHidden);
	actionRow->addStretch(1);

	tree = new QTreeWidget(this);
	tree->setColumnCount(ColCount);
	tree->setHeaderLabels({Text("Column.State"), Text("Column.Name"), Text("Column.Page"), Text("Column.Size"),
			       Text("Column.Fps"), Text("Column.Scenes")});
	tree->setRootIsDecorated(false);
	tree->setUniformRowHeights(true);
	tree->setAlternatingRowColors(true);
	tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	tree->setContextMenuPolicy(Qt::CustomContextMenu);
	tree->setSortingEnabled(true);
	tree->sortByColumn(ColState, Qt::AscendingOrder);
	tree->header()->setStretchLastSection(true);
	tree->header()->setSectionResizeMode(QHeaderView::Interactive);

	summary = new QLabel(this);
	summary->setWordWrap(true);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->addLayout(topRow);
	layout->addLayout(actionRow);
	layout->addWidget(tree, 1);
	layout->addWidget(summary);

	timer = new QTimer(this);
	timer->setInterval(1000);

	connect(timer, &QTimer::timeout, this, &BrowserDock::Reload);
	connect(search, &QLineEdit::textChanged, this, &BrowserDock::ApplyFilter);
	connect(stateFilter, &QComboBox::currentIndexChanged, this, &BrowserDock::ApplyFilter);
	connect(refreshAll, &QToolButton::clicked, this, &BrowserDock::RefreshAll);
	connect(unloadHidden, &QToolButton::clicked, this, &BrowserDock::UnloadHidden);
	connect(tree, &QTreeWidget::customContextMenuRequested, this, &BrowserDock::ShowContextMenu);
	connect(tree, &QTreeWidget::itemDoubleClicked, this, [](QTreeWidgetItem *item, int) {
		OpenSourceProperties(item->data(ColName, UuidRole).toString().toStdString());
	});
}

void BrowserDock::HandleFrontendEvent(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		paused = false;
		timer->start();
		Reload();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
		paused = true;
		tree->clear();
		items.clear();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		paused = true;
		timer->stop();
		break;
	default:
		break;
	}
}

void BrowserDock::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	Reload();
}

void BrowserDock::Reload()
{
	if (paused || !isVisible())
		return;

	std::vector<BrowserSourceInfo> sources = CollectBrowserSources();
	int counts[4] = {};
	QSet<QString> seen;

	tree->setSortingEnabled(false);

	for (const auto &info : sources) {
		QString uuid = QString::fromStdString(info.uuid);
		seen.insert(uuid);
		counts[(int)info.state]++;

		QTreeWidgetItem *item = items.value(uuid);
		if (!item) {
			item = new SortableItem(tree);
			item->setData(ColName, UuidRole, uuid);
			items.insert(uuid, item);
		}

		QString name = QString::fromStdString(info.name);
		QString url = QString::fromStdString(info.url);
		QStringList scenes;
		for (const auto &scene : info.scenes)
			scenes << QString::fromStdString(scene);

		item->setIcon(ColState, StateIcon(info.state));
		item->setText(ColState, StateLabel(info.state));
		item->setToolTip(ColState, StateTooltip(info.state));
		item->setData(ColState, SortRole, (int)info.state);
		item->setData(ColState, StateRole, (int)info.state);
		item->setData(ColState, ShutdownRole, info.shutdownWhenHidden);

		item->setText(ColName, name);
		item->setToolTip(ColName, name);

		item->setText(ColPage, PageLabel(info));
		item->setToolTip(ColPage, url);
		item->setData(ColPage, UrlRole, url);

		item->setText(ColSize, QStringLiteral("%1x%2").arg(info.width).arg(info.height));
		item->setData(ColSize, SortRole, (qlonglong)info.width * info.height);

		item->setText(ColFps, info.customFps ? QString::number(info.fps) : Text("Fps.Canvas").arg(info.fps));
		item->setData(ColFps, SortRole, info.fps);

		QString sceneText = scenes.isEmpty() ? Text("Scenes.None") : scenes.join(QStringLiteral(", "));
		item->setText(ColScenes, sceneText);
		item->setToolTip(ColScenes, scenes.join(QStringLiteral("\n")));

		item->setData(ColName, SearchRole, (name + ' ' + url + ' ' + sceneText).toLower());
	}

	for (auto it = items.begin(); it != items.end();) {
		if (!seen.contains(it.key())) {
			delete it.value();
			it = items.erase(it);
		} else {
			++it;
		}
	}

	tree->setSortingEnabled(true);

	if (!columnsFitted && !items.isEmpty()) {
		tree->header()->resizeSections(QHeaderView::ResizeToContents);
		columnsFitted = true;
	}

	summary->setText(Text("Summary")
				 .arg(sources.size())
				 .arg(counts[(int)BrowserState::OnAir])
				 .arg(counts[(int)BrowserState::RunningHidden])
				 .arg(counts[(int)BrowserState::Unloaded]));

	ApplyFilter();
}

void BrowserDock::ApplyFilter()
{
	QString needle = search->text().trimmed().toLower();
	int wanted = stateFilter->currentData().toInt();

	for (QTreeWidgetItem *item : items) {
		bool matchesText = needle.isEmpty() || item->data(ColName, SearchRole).toString().contains(needle);
		bool matchesState = wanted == kFilterAll || item->data(ColState, StateRole).toInt() == wanted;
		item->setHidden(!(matchesText && matchesState));
	}
}

QStringList BrowserDock::SelectedUuids() const
{
	QStringList uuids;
	for (QTreeWidgetItem *item : tree->selectedItems())
		uuids << item->data(ColName, UuidRole).toString();
	return uuids;
}

void BrowserDock::ShowContextMenu(const QPoint &pos)
{
	QTreeWidgetItem *clicked = tree->itemAt(pos);
	if (!clicked)
		return;
	if (!clicked->isSelected()) {
		tree->clearSelection();
		clicked->setSelected(true);
	}

	QStringList uuids = SelectedUuids();
	QList<QTreeWidgetItem *> selected = tree->selectedItems();
	bool single = selected.size() == 1;
	QString uuid = clicked->data(ColName, UuidRole).toString();
	QString url = clicked->data(ColPage, UrlRole).toString();

	bool allShutdown = true;
	for (QTreeWidgetItem *item : selected)
		allShutdown = allShutdown && item->data(ColState, ShutdownRole).toBool();

	QMenu menu(this);

	menu.addAction(Text("Action.Refresh"), this, [this, uuids]() {
		for (const QString &id : uuids)
			RefreshBrowserSource(id.toStdString());
	});

	if (single) {
		menu.addAction(Text("Action.Properties"), this, [uuid]() { OpenSourceProperties(uuid.toStdString()); });
		menu.addAction(Text("Action.Interact"), this, [uuid]() { OpenSourceInteraction(uuid.toStdString()); });
	}

	menu.addSeparator();

	QAction *shutdown = menu.addAction(Text("Action.Shutdown"));
	shutdown->setCheckable(true);
	shutdown->setChecked(allShutdown);
	shutdown->setToolTip(Text("Action.Shutdown.Tooltip"));
	menu.setToolTipsVisible(true);
	connect(shutdown, &QAction::triggered, this, [this, uuids](bool checked) {
		for (const QString &id : uuids)
			SetShutdownWhenHidden(id.toStdString(), checked);
		Reload();
	});

	if (single && !url.isEmpty()) {
		menu.addSeparator();
		menu.addAction(Text("Action.CopyUrl"), this, [url]() { QApplication::clipboard()->setText(url); });

		bool isWeb = url.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
			     url.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
		if (isWeb)
			menu.addAction(Text("Action.OpenUrl"), this, [url]() { QDesktopServices::openUrl(QUrl(url)); });
	}

	menu.exec(tree->viewport()->mapToGlobal(pos));
}

void BrowserDock::RefreshAll()
{
	for (const QString &uuid : items.keys())
		RefreshBrowserSource(uuid.toStdString());
}

void BrowserDock::UnloadHidden()
{
	// Only touch sources that are not shown anywhere: changing the setting reloads the page,
	// which must never happen to something that is on air.
	QStringList targets;
	for (QTreeWidgetItem *item : items) {
		if (item->data(ColState, StateRole).toInt() == (int)BrowserState::RunningHidden)
			targets << item->data(ColName, UuidRole).toString();
	}

	if (targets.isEmpty()) {
		QMessageBox::information(this, Text("UnloadHidden"), Text("UnloadHidden.None"));
		return;
	}

	auto answer =
		QMessageBox::question(this, Text("UnloadHidden"), Text("UnloadHidden.Confirm").arg(targets.size()));
	if (answer != QMessageBox::Yes)
		return;

	for (const QString &uuid : targets)
		SetShutdownWhenHidden(uuid.toStdString(), true);
	Reload();
}
