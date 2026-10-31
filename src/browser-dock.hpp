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

#pragma once

#include <obs-frontend-api.h>

#include <QHash>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QTimer;
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
	void ShowContextMenu(const QPoint &pos);
	QStringList SelectedUuids() const;
	void RefreshAll();
	void UnloadHidden();

	QLineEdit *search;
	QComboBox *stateFilter;
	QTreeWidget *tree;
	QLabel *summary;
	QTimer *timer;
	QHash<QString, QTreeWidgetItem *> items;
	bool paused = true;
	bool columnsFitted = false;
};
