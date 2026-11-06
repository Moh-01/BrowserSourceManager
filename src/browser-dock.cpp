#include "browser-dock.hpp"
#include "bulk-edit-dialog.hpp"

#include <obs-module.h>
#include <obs.hpp>
#include <util/platform.h>

#include <climits>

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
constexpr int GroupRole = Qt::UserRole + 6;

const QString kNoSceneKey = QStringLiteral("\x01");

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

// Edit here to change the state dot colors. If you change one, every dot for that state uses the new color.
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

// Edit the ellipse here to change the dot size. If you make it larger than the 24x24 pixmap, the dot gets cut off.
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

// Edit here to change what the Page column shows. If you return the full url, overlay keys and tokens become visible.
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
		QVariant groupA = data(0, GroupRole);
		QVariant groupB = other.data(0, GroupRole);
		if (groupA.isValid() && groupB.isValid()) {
			bool descending = treeWidget() &&
					  treeWidget()->header()->sortIndicatorOrder() == Qt::DescendingOrder;
			return descending ? groupA.toInt() > groupB.toInt() : groupA.toInt() < groupB.toInt();
		}

		int column = treeWidget() ? treeWidget()->sortColumn() : 0;
		QVariant a = data(column, SortRole);
		QVariant b = other.data(column, SortRole);
		if (a.isValid() && b.isValid())
			return a.toLongLong() < b.toLongLong();
		return text(column).localeAwareCompare(other.text(column)) < 0;
	}
};

}

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

	editButton = new QToolButton(this);
	editButton->setText(Text("Action.Edit"));
	editButton->setToolTip(Text("Action.Edit.Tooltip"));
	editButton->setEnabled(false);

	groupButton = new QToolButton(this);
	groupButton->setText(Text("GroupByScene"));
	groupButton->setToolTip(Text("GroupByScene.Tooltip"));
	groupButton->setCheckable(true);

	auto *topRow = new QHBoxLayout();
	topRow->addWidget(search, 1);
	topRow->addWidget(stateFilter);

	auto *actionRow = new QHBoxLayout();
	actionRow->addWidget(refreshAll);
	actionRow->addWidget(unloadHidden);
	actionRow->addWidget(editButton);
	actionRow->addStretch(1);
	actionRow->addWidget(groupButton);

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
	// Edit here to change how often the list updates, in milliseconds. Lower updates faster but uses more CPU.
	timer->setInterval(1000);

	connect(timer, &QTimer::timeout, this, &BrowserDock::Reload);
	connect(search, &QLineEdit::textChanged, this, &BrowserDock::ApplyFilter);
	connect(stateFilter, &QComboBox::currentIndexChanged, this, &BrowserDock::ApplyFilter);
	connect(refreshAll, &QToolButton::clicked, this, &BrowserDock::RefreshAll);
	connect(unloadHidden, &QToolButton::clicked, this, &BrowserDock::UnloadHidden);
	connect(tree, &QTreeWidget::customContextMenuRequested, this, &BrowserDock::ShowContextMenu);
	connect(editButton, &QToolButton::clicked, this, &BrowserDock::EditSelected);
	connect(groupButton, &QToolButton::toggled, this, &BrowserDock::SetGrouped);
	connect(tree, &QTreeWidget::itemSelectionChanged, this,
		[this]() { editButton->setEnabled(!SelectedUuids().isEmpty()); });
	connect(tree, &QTreeWidget::itemDoubleClicked, this, [](QTreeWidgetItem *item, int) {
		QString uuid = item->data(ColName, UuidRole).toString();
		if (!uuid.isEmpty())
			OpenSourceProperties(uuid.toStdString());
	});

	LoadSettings();
	groupButton->setChecked(grouped);
	tree->setRootIsDecorated(grouped);
	tree->setColumnHidden(ColScenes, grouped);
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
		ClearRows();
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

void BrowserDock::ClearRows()
{
	tree->clear();
	rows.clear();
	groups.clear();
}

void BrowserDock::SetGrouped(bool enabled)
{
	if (grouped == enabled)
		return;
	grouped = enabled;
	ClearRows();
	tree->setRootIsDecorated(grouped);
	tree->setColumnHidden(ColScenes, grouped);
	SaveSettings();
	Reload();
}

QTreeWidgetItem *BrowserDock::GroupItem(const QString &key, int order)
{
	QTreeWidgetItem *group = groups.value(key);
	if (!group) {
		group = new SortableItem(tree);
		group->setFlags(Qt::ItemIsEnabled);
		group->setFirstColumnSpanned(true);
		QFont font = group->font(0);
		font.setBold(true);
		group->setFont(0, font);
		group->setExpanded(true);
		groups.insert(key, group);
	}
	group->setData(0, GroupRole, order);
	return group;
}

void BrowserDock::Reload()
{
	if (paused || !isVisible())
		return;

	std::vector<BrowserSourceInfo> sources = CollectBrowserSources();
	QHash<QString, int> sceneOrder;
	if (grouped) {
		int index = 0;
		for (const auto &scene : SceneNames())
			sceneOrder.insert(QString::fromStdString(scene), index++);
	}

	int counts[4] = {};
	QSet<QString> seenRows;
	QSet<QString> seenGroups;
	infos.clear();

	tree->setSortingEnabled(false);

	for (const auto &info : sources) {
		QString uuid = QString::fromStdString(info.uuid);
		infos.insert(uuid, info);
		counts[(int)info.state]++;

		QString name = QString::fromStdString(info.name);
		QString url = QString::fromStdString(info.url);
		QStringList scenes;
		for (const auto &scene : info.scenes)
			scenes << QString::fromStdString(scene);
		QString sceneText = scenes.isEmpty() ? Text("Scenes.None") : scenes.join(QStringLiteral(", "));

		QStringList groupKeys;
		if (!grouped)
			groupKeys << QString();
		else if (scenes.isEmpty())
			groupKeys << kNoSceneKey;
		else
			groupKeys = scenes;

		for (const QString &key : groupKeys) {
			QTreeWidgetItem *parent = nullptr;
			if (grouped) {
				int order = key == kNoSceneKey ? INT_MAX : sceneOrder.value(key, INT_MAX - 1);
				parent = GroupItem(key, order);
				seenGroups.insert(key);
			}

			QString rowKey = key + QLatin1Char('\n') + uuid;
			seenRows.insert(rowKey);
			QTreeWidgetItem *item = rows.value(rowKey);
			if (!item) {
				item = parent ? new SortableItem(parent) : new SortableItem(tree);
				item->setData(ColName, UuidRole, uuid);
				rows.insert(rowKey, item);
			}

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

			item->setText(ColFps,
				      info.customFps ? QString::number(info.fps) : Text("Fps.Canvas").arg(info.fps));
			item->setData(ColFps, SortRole, info.fps);

			item->setText(ColScenes, sceneText);
			item->setToolTip(ColScenes, scenes.join(QStringLiteral("\n")));

			item->setData(ColName, SearchRole, (name + ' ' + url + ' ' + sceneText).toLower());
		}
	}

	for (auto it = rows.begin(); it != rows.end();) {
		if (!seenRows.contains(it.key())) {
			delete it.value();
			it = rows.erase(it);
		} else {
			++it;
		}
	}
	for (auto it = groups.begin(); it != groups.end();) {
		if (!seenGroups.contains(it.key())) {
			delete it.value();
			it = groups.erase(it);
		} else {
			++it;
		}
	}

	tree->setSortingEnabled(true);

	if (!columnsFitted && !rows.isEmpty()) {
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

	for (QTreeWidgetItem *item : rows) {
		bool matchesText = needle.isEmpty() || item->data(ColName, SearchRole).toString().contains(needle);
		bool matchesState = wanted == kFilterAll || item->data(ColState, StateRole).toInt() == wanted;
		item->setHidden(!(matchesText && matchesState));
	}

	for (auto it = groups.begin(); it != groups.end(); ++it) {
		QTreeWidgetItem *group = it.value();
		int visible = 0;
		for (int i = 0; i < group->childCount(); i++) {
			if (!group->child(i)->isHidden())
				visible++;
		}
		QString title = it.key() == kNoSceneKey ? Text("Scenes.None") : it.key();
		group->setText(0, QStringLiteral("%1  (%2)").arg(title).arg(visible));
		group->setHidden(visible == 0);
	}
}

QStringList BrowserDock::SelectedUuids() const
{
	QStringList uuids;
	for (QTreeWidgetItem *item : tree->selectedItems()) {
		QString uuid = item->data(ColName, UuidRole).toString();
		if (!uuid.isEmpty() && !uuids.contains(uuid))
			uuids << uuid;
	}
	return uuids;
}

void BrowserDock::ShowContextMenu(const QPoint &pos)
{
	QTreeWidgetItem *clicked = tree->itemAt(pos);
	if (!clicked)
		return;

	bool isGroup = clicked->data(0, GroupRole).isValid();
	if (isGroup) {
		tree->clearSelection();
		for (int i = 0; i < clicked->childCount(); i++) {
			if (!clicked->child(i)->isHidden())
				clicked->child(i)->setSelected(true);
		}
	} else if (!clicked->isSelected()) {
		tree->clearSelection();
		clicked->setSelected(true);
	}

	QStringList uuids = SelectedUuids();
	if (uuids.isEmpty())
		return;
	QList<QTreeWidgetItem *> selected = tree->selectedItems();
	bool single = uuids.size() == 1;
	QString uuid = uuids.first();
	QString url = isGroup ? QString() : clicked->data(ColPage, UrlRole).toString();

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
	menu.addAction(Text("Action.Edit"), this, &BrowserDock::EditSelected);

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

void BrowserDock::EditSelected()
{
	QStringList uuids = SelectedUuids();
	if (uuids.isEmpty())
		return;

	std::vector<BrowserSourceInfo> selected;
	for (const QString &uuid : uuids) {
		if (infos.contains(uuid))
			selected.push_back(infos.value(uuid));
	}
	if (selected.empty())
		return;

	BulkEditDialog dialog(selected, this);
	if (dialog.exec() != QDialog::Accepted)
		return;

	BrowserEdit edit = dialog.Edit();
	for (const auto &info : selected)
		ApplyBrowserEdit(info.uuid, edit);
	Reload();
}

void BrowserDock::RefreshAll()
{
	for (const QString &uuid : infos.keys())
		RefreshBrowserSource(uuid.toStdString());
}

void BrowserDock::UnloadHidden()
{
	// Edit here to change which sources Unload hidden affects. If you include sources that are showing, they reload on air.
	QStringList targets;
	for (const auto &info : infos) {
		if (info.state == BrowserState::RunningHidden)
			targets << QString::fromStdString(info.uuid);
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

void BrowserDock::LoadSettings()
{
	char *path = obs_module_config_path("settings.json");
	OBSDataAutoRelease data = obs_data_create_from_json_file_safe(path, "bak");
	bfree(path);
	if (data)
		grouped = obs_data_get_bool(data, "group_by_scene");
}

void BrowserDock::SaveSettings() const
{
	char *dir = obs_module_config_path("");
	os_mkdirs(dir);
	bfree(dir);

	char *path = obs_module_config_path("settings.json");
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_bool(data, "group_by_scene", grouped);
	obs_data_save_json_safe(data, path, "tmp", "bak");
	bfree(path);
}
