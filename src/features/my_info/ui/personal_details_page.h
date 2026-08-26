#pragma once

#include "features/my_info/data/personal_details_repository.h"
#include "features/my_info/data/typed_signature_renderer.h"
#include "ui/shared/pages/basepage.h"

#include <QByteArray>
#include <QList>

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

    void setPageHeaderVisible(bool visible);
    void scrollToTop();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void handleEditableChanged();
    void handleZoomNotAvailableChanged(bool checked);
    void selectSignatureMode();
    void selectTypedSignatureFont();
    void handleTypedSignatureChanged();
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
    void setSignatureMode(SignatureMode mode);
    void updateTypedSignatureImage();
    void updateSignatureControls();
    void updateTypedSignatureFontOptions();
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
    QPushButton* m_signatureImageModeButton = nullptr;
    QPushButton* m_signatureTypeModeButton = nullptr;
    QWidget* m_signatureImageControls = nullptr;
    QWidget* m_typedSignatureControls = nullptr;
    QLabel* m_typedSignatureLabel = nullptr;
    QLabel* m_typedSignatureFontLabel = nullptr;
    QLineEdit* m_typedSignatureEdit = nullptr;
    QList<QLabel*> m_typedSignatureFontPreviews;
    QList<QPushButton*> m_typedSignatureFontButtons;
    QPushButton* m_chooseSignatureButton = nullptr;
    QPushButton* m_removeSignatureButton = nullptr;
    QByteArray m_signatureImageData;
    SignatureMode m_signatureMode = SignatureMode::Image;
    TypedSignatureFont m_typedSignatureFont =
        TypedSignatureFont::JustAnotherHand;

};
