#pragma once

#include "domain/models/campus_info.h"
#include "ui/shared/pages/basepage.h"

#include <QList>

class ApplicationServices;
class QEvent;
class QLabel;
class QLineEdit;
class QScrollArea;
class QShowEvent;
class QTabWidget;
class QTextEdit;
class QTimer;
class QVBoxLayout;
class QWidget;
class SectionCard;

enum class SubPrepSection
{
    ImportantInformation,
    SubComments
};

class SubPrepPage : public BasePage
{
    Q_OBJECT

public:
    explicit SubPrepPage(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void saveData() override;
    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;
    void refresh() override;
    void retranslateUi() override;

    void scrollToSection(
        SubPrepSection section
        );
    void scrollToTop();
    QString currentSectionName() const;
    QString currentSectionKey() const;

protected:
    void showEvent(
        QShowEvent* event
        ) override;

    bool eventFilter(
        QObject* watched,
        QEvent* event
        ) override;

private slots:
    void handleEditableChanged();
    void autosave();

private:
    void buildUi();
    void loadPageData();
    void loadStoredSettings();
    void loadCampuses();
    void loadCampusFields(
        const QString& campusId
        );
    void updateCampusFieldWidths();

    bool saveSubPrepInternal();

    void refreshGeneratedContent();
    void rebuildClassInformation();

    bool normalizeLineEdit(
        QLineEdit* edit,
        const QString& defaultText
        );
    bool restoreBookReportDefaultIfNeeded();
    QString defaultBookReportText() const;

    QLabel* createTopLevelHeading(
        const QString& text,
        QWidget* parent
        ) const;
    QLabel* createFieldLabel(
        const QString& text,
        QWidget* parent
        ) const;
    QTextEdit* createTextEdit(
        int minimumLines,
        bool readOnly,
        QWidget* parent
        ) const;
    void clearClassInformation();
    void clearDirty();

private:
    ApplicationServices* m_services = nullptr;

    bool m_loading = false;
    bool m_dirty = false;
    SubPrepSection m_currentSection = SubPrepSection::ImportantInformation;
    QList<CampusInfo> m_campuses;

    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_scrollContent = nullptr;
    QVBoxLayout* m_scrollContentLayout = nullptr;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;

    QLineEdit* m_officeNumberEdit = nullptr;
    QLineEdit* m_officeWifiEdit = nullptr;
    QLineEdit* m_officeWifiPasswordEdit = nullptr;
    QLineEdit* m_photocopierCodeEdit = nullptr;

    QTextEdit* m_classMaterialsEdit = nullptr;
    QTextEdit* m_bookReportGradingEdit = nullptr;
    QTextEdit* m_subCommentsEdit = nullptr;

    QLabel* m_importantInformationHeading = nullptr;
    QLabel* m_classInformationHeading = nullptr;
    QLabel* m_subCommentsHeading = nullptr;
    QLabel* m_classInfoSubtitle = nullptr;

    SectionCard* m_campusCard = nullptr;
    SectionCard* m_materialsCard = nullptr;
    SectionCard* m_commentsCard = nullptr;

    QLabel* m_officeNumberLabel = nullptr;
    QLabel* m_officeWifiLabel = nullptr;
    QLabel* m_officeWifiPasswordLabel = nullptr;
    QLabel* m_photocopierCodeLabel = nullptr;
    QLabel* m_classMaterialsLabel = nullptr;
    QLabel* m_bookReportGradingLabel = nullptr;

    QWidget* m_classInformationContent = nullptr;
    QVBoxLayout* m_classInformationLayout = nullptr;
    QTabWidget* m_classInformationTabs = nullptr;
    int m_selectedClassId = -1;

    QTimer* m_autosaveTimer = nullptr;
};
