#pragma once

#include "domain/models/teacher_import.h"

#include <QDialog>
#include <QList>

class QCheckBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QVBoxLayout;
class QWidget;

class TeacherImportDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit TeacherImportDialog(QWidget* parent = nullptr);

    void setFilePath(const QString& filePath);
    [[nodiscard]] TeacherImportPlan importPlan() const;

private:
    struct GroupControls
    {
        QString level;
        QRadioButton* all = nullptr;
        QRadioButton* selected = nullptr;
        QRadioButton* none = nullptr;
        QWidget* checklistWidget = nullptr;
        QList<QCheckBox*> candidateChecks;
    };

    void browseForFile();
    void validateSelectedFile();
    void rebuildOptions();
    void clearOptions();
    void updateImportEnabled();

    QLineEdit* m_fileEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_templateLabel = nullptr;
    QLabel* m_automaticLabel = nullptr;
    QVBoxLayout* m_optionsHostLayout = nullptr;
    QWidget* m_optionsWidget = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    QPushButton* m_importButton = nullptr;

    TeacherImportPreview m_preview;
    QList<GroupControls> m_groupControls;
    bool m_valid = false;
};
