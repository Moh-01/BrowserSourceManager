#pragma once

#include "browser-sources.hpp"

#include <obs-frontend-api.h>

#include <QHash>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QTimer;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

class BrowserDock : public QWidget {
	Q_OBJECT

public:
	explicit BrowserDock(QWidget *parent = nullptr);

	void HandleFrontendEvent(enum obs_frontend_event event);

protected:
	void showEvent(QShowEvent *event) override;

private:
	void Reload();
	void ApplyFilter();
	void ClearRows();
	void SetGrouped(bool enabled);
	QTreeWidgetItem *GroupItem(const QString &key, int order);
	void ShowContextMenu(const QPoint &pos);
	QStringList SelectedUuids() const;
	void EditSelected();
	void RefreshAll();
	void UnloadHidden();
	void LoadSettings();
	void SaveSettings() const;

	QLineEdit *search;
	QComboBox *stateFilter;
	QToolButton *groupButton;
	QToolButton *editButton;
	QTreeWidget *tree;
	QLabel *summary;
	QTimer *timer;
	QHash<QString, QTreeWidgetItem *> rows;
	QHash<QString, QTreeWidgetItem *> groups;
	QHash<QString, BrowserSourceInfo> infos;
	bool grouped = false;
	bool paused = true;
	bool columnsFitted = false;
};
