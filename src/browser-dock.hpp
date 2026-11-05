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
