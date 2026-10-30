#pragma once

#include "next/ui/schedule_output_request.h"

#include <QDialog>
#include <QPageLayout>
#include <QString>

#include <functional>
#include <optional>

class QDialogButtonBox;
class QEvent;
class QGroupBox;
class QRadioButton;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace ClassMngr::Next::Ui
{

class ScheduleOutputDialog final : public QDialog
{
    Q_OBJECT

public:
    using SavePathChooser = std::function<std::optional<QString>(QWidget*)>;

    explicit ScheduleOutputDialog(
        SavePathChooser chooseSavePath,
        QWidget* parent = nullptr
        );

    [[nodiscard]] ScheduleOutputKind selectedKind() const noexcept;
    [[nodiscard]] SchedulePrintStyle selectedStyle() const noexcept;
    [[nodiscard]] QPageLayout::Orientation selectedOrientation() const noexcept;
    [[nodiscard]] QString selectedPath() const;

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void acceptPrint();
    void choosePdfPath();

    SavePathChooser m_chooseSavePath;
    ScheduleOutputKind m_selectedKind;
    QString m_selectedPath;
    QVBoxLayout* m_layout = nullptr;
    QGroupBox* m_styleGroup = nullptr;
    QGroupBox* m_orientationGroup = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QPushButton* m_printButton = nullptr;
    QPushButton* m_saveButton = nullptr;
    QRadioButton* m_currentAppearance = nullptr;
    QRadioButton* m_light = nullptr;
    QRadioButton* m_dark = nullptr;
    QRadioButton* m_excel = nullptr;
    QRadioButton* m_landscape = nullptr;
    QRadioButton* m_portrait = nullptr;
};

} // namespace ClassMngr::Next::Ui
