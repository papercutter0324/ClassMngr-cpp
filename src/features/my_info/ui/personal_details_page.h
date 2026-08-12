#pragma once

#include "ui/shared/pages/basepage.h"

#include <QByteArray>

class ApplicationServices;
class AutosaveCoordinator;
class PageHeader;
class ScrollablePageBody;
class QCheckBox;
class QComboBox;
class QEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QVBoxLayout;
class QWidget;

class PersonalDetailsPage : public BasePage
{
    Q_OBJECT

public:
    explicit PersonalDetailsPage(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void refresh() override;
    void clearDatabaseState() override;
    void retranslateUi() override;
    void saveData() override;
    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;
    void setSaveMode(SaveMode mode) override;

    void scrollToTop();

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void handleEditableChanged();
    void handleZoomNotAvailableChanged(bool checked);
    void chooseSignatureImage();
    void removeSignatureImage();

private:
    void buildUi();
    void buildMyInformationSection();
    void buildSignatureSection();
    void loadPageData();
    void loadStoredSettings();
    bool saveMyInfoInternal();
    bool normalizeZoomFields();
    bool normalizeLineEdit(QLineEdit* edit, const QString& defaultText);
    void setZoomFieldsEnabled();
    void updateMyInformationFieldWidths();
    void updateSignaturePreview();
    void clearDirty();

    QLabel* createTopLevelHeading(
        const QString& text,
        QWidget* parent
        ) const;
    QLabel* createFieldLabel(
        const QString& text,
        QWidget* parent
        ) const;

    ApplicationServices* m_services = nullptr;
    AutosaveCoordinator* m_autosave = nullptr;

    ScrollablePageBody* m_pageBody = nullptr;
    QWidget* m_scrollContent = nullptr;
    QVBoxLayout* m_scrollContentLayout = nullptr;

    PageHeader* m_pageHeader = nullptr;
    QLabel* m_myInformationHeading = nullptr;
    QLabel* m_signatureHeading = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_campusLabel = nullptr;
    QLabel* m_zoomLoginIdLabel = nullptr;
    QLabel* m_zoomPasswordLabel = nullptr;
    QLabel* m_zoomLabel = nullptr;

    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_campusCombo = nullptr;
    QLineEdit* m_zoomLoginIdEdit = nullptr;
    QLineEdit* m_zoomPasswordEdit = nullptr;
    QCheckBox* m_zoomNotAvailableCheck = nullptr;
    QLabel* m_signatureInstructionsLabel = nullptr;
    QLabel* m_signaturePreviewLabel = nullptr;
    QPushButton* m_chooseSignatureButton = nullptr;
    QPushButton* m_removeSignatureButton = nullptr;
    QByteArray m_signatureImageData;

};
