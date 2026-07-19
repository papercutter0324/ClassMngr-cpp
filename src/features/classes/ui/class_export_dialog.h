#pragma once

#include <QDialog>
#include <QList>

class DataService;
class QListWidget;
class QPushButton;

class ClassExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ClassExportDialog(
        DataService* dataService,
        QWidget* parent = nullptr
        );

    [[nodiscard]] QList<int> selectedClassIds() const;

private:
    void setAllChecked(bool checked);
    void updateExportEnabled();

    QListWidget* m_classList = nullptr;
    QPushButton* m_exportButton = nullptr;
};
