#ifndef CAMPUS_DASHBOARD_PAGE_H
#define CAMPUS_DASHBOARD_PAGE_H

#include "core/result.h"
#include "ui/shared/pages/basepage.h"
#include "domain/models/campus_info.h"
#include "features/campus/data/campus_json_repository.h"

#include <QList>
#include <QPointer>

class QComboBox;
class QCheckBox;
class QFormLayout;
class QJsonObject;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QTimer;
class QVBoxLayout;
class QWidget;
class CampusMapPreview;
class SectionCard;

class CampusDashboardPage : public BasePage
{
    Q_OBJECT

public:

    explicit CampusDashboardPage(
        bool adminMode,
        QWidget *parent = nullptr
        );

    void showDirections();
    void showInformation();
    void showHousing();
    void showMap();
    QString currentSectionName() const;
    QString currentSectionKey() const;

    void refresh() override;
    void retranslateUi() override;

    void saveData() override;
    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;
    void setSaveMode(
        SaveMode mode
        ) override;

signals:
    void sectionChanged(
        const QString& sectionKey
        );

private slots:
    void loadSelectedCampus();
    void handleFieldEdited();
    void handleSaveTimeout();

private:
    static constexpr int CompleteAddressMinimumHeight = 150;

    struct AddressSectionWidgets;
    struct HousingSectionWidgets;

    QWidget* createDirectionsTab();
    QWidget* createInformationTab();
    QWidget* createHousingTab();
    QWidget* createMapTab();

    QLineEdit* addLineField(
        QFormLayout* form,
        const char* labelText
        );

    QPlainTextEdit* addTextField(
        QFormLayout* form,
        const char* labelText,
        int minimumLines,
        int maximumLines
        );

    QLabel* createTranslatableLabel(
        const char* sourceText,
        QWidget* parent = nullptr
        );
    void addFormRow(
        QFormLayout* form,
        const char* labelText,
        QWidget* field
        );
    void insertFormRow(
        QFormLayout* form,
        int row,
        const char* labelText,
        QWidget* field
        );
    void retranslateRegisteredLabels();

    AddressSectionWidgets createAddressSection(
        QWidget* parent,
        bool koreanAddress,
        bool includeBuildingName = false
        );

    void configureExpandingTextField(
        QPlainTextEdit* edit,
        int minimumLines,
        int maximumLines
        );

    void updateTextFieldHeight(
        QPlainTextEdit* edit
        );

    void buildUi();
    void applyAdminMode();
    void loadCampuses();

    void populateFields(
        const CampusInfo& campus
        );

    void populateHousingSections(
        const QJsonArray& housingLocations
        );

    void clearHousingSections();
    void addHousingSectionFromJson(
        const QJsonObject& housing
        );
    QJsonArray housingSectionsToJson() const;
    void populateAddressSection(
        AddressSectionWidgets* section,
        const QJsonObject& address
        );
    QJsonObject addressSectionToJson(
        const AddressSectionWidgets& section
        ) const;
    QString completeAddressFor(
        const AddressSectionWidgets& section
        ) const;
    void updateCompleteAddress(
        AddressSectionWidgets* section
        );
    void updateCompleteAddressPair(
        AddressSectionWidgets* english,
        AddressSectionWidgets* korean
        );
    void updateCompleteAddressPairFor(
        AddressSectionWidgets* section
        );
    void handleAddressSystemToggle(
        QPushButton* button
        );
    void handleAddressComponentsToggle(
        QPushButton* button
        );
    void handleAddressLanguageToggle(
        QPushButton* button
        );
    void toggleAddressSystem(
        AddressSectionWidgets* section
        );
    void setAddressComponentsVisible(
        AddressSectionWidgets* section,
        bool visible
        );
    void storeCurrentAddressVariant(
        AddressSectionWidgets* section
        ) const;
    void loadAddressFields(
        AddressSectionWidgets* section,
        const QJsonObject& address
        ) const;
    QJsonObject addressFieldsToJson(
        const AddressSectionWidgets& section
        ) const;
    void updateAddressSystemButton(
        AddressSectionWidgets* section
        ) const;
    void updateAddressComponentsButton(
        AddressSectionWidgets* section
        ) const;
    void updateHousingRemoveButtonVisibility();
    void updatePrinterDriverUrlState();
    void updatePhotocopierCodeState();
    void showDirectionsLanguage(
        bool showEnglish
        );
    void syncPhoneFields(
        QLineEdit* source
        );
    void handleAddressVariantFieldEdited(
        QLineEdit* edit,
        const QString& key
        );
    AddressSectionWidgets* addressSectionForField(
        QLineEdit* edit
        );
    void normalizeCampusNameField();
    void handleNewCampus();
    void handleManualCampusSave();

    [[nodiscard]] Status readFieldsIntoCampus(
        CampusInfo* campus
        ) const;

    void scheduleSave();
    bool saveCurrentCampus();
    void updateCampusSaveButton();
    void updateCampusSelectorItem(
        int index
        );
    QString campusDisplayName(
        const CampusInfo& campus
        ) const;
    void updateMapPreview();
    void clearMapSections();
    void addMapSection(
        const QString& title,
        const QStringList& imagePaths,
        const QString& naverUrl,
        const QString& kakaoUrl
        );
    [[nodiscard]] bool housingSectionHasContent(
        const HousingSectionWidgets& section
        ) const;
    void openMapUrl(
        const QString& url
        );
    void updateDirectionsCompleteAddresses();
    void updateHousingCompleteAddresses();
    void emitCurrentSectionChanged();
    void setStatus(
        const QString& message
        );

    struct AddressSectionWidgets
    {
        QWidget* container = nullptr;
        QFormLayout* form = nullptr;
        QPlainTextEdit* complete = nullptr;
        QWidget* completeControls = nullptr;
        QPushButton* toggleLanguageButton = nullptr;
        QPushButton* toggleAddressSystemButton = nullptr;
        QPushButton* toggleAddressComponentsButton = nullptr;
        QLineEdit* buildingName = nullptr;
        QLineEdit* province = nullptr;
        QLineEdit* city = nullptr;
        QLineEdit* district = nullptr;
        QLineEdit* line1 = nullptr;
        QLineEdit* line2 = nullptr;
        QLineEdit* line2Suffix = nullptr;
        QLineEdit* postalCode = nullptr;
        QJsonObject modernAddress;
        QJsonObject classicAddress;
        QList<QWidget*> componentFields;
        bool koreanAddress = false;
        bool showingModernAddress = true;
        bool showingAddressComponents = true;
    };

    struct HousingSectionWidgets
    {
        QWidget* container = nullptr;
        QLineEdit* name = nullptr;
        AddressSectionWidgets english;
        AddressSectionWidgets korean;
        QLineEdit* note = nullptr;
        QPushButton* removeButton = nullptr;
        QJsonObject map;
        bool showingEnglish = true;
    };

    struct MapSectionWidgets
    {
        SectionCard* card = nullptr;
        QPushButton* naverButton = nullptr;
        QPushButton* kakaoButton = nullptr;
        CampusMapPreview* preview = nullptr;
    };

    struct TranslatableLabel
    {
        QPointer<QLabel> label;
        const char* sourceText = nullptr;
    };

private:
    bool m_adminMode = false;
    bool m_loading = false;
    bool m_dirty = false;
    SaveMode m_saveMode = SaveMode::Automatic;
    int m_currentCampusComboIndex = -1;

    CampusJsonRepository m_repository;
    CampusInfo m_currentCampus;

    QTimer* m_saveTimer = nullptr;

    QComboBox* m_campusCombo = nullptr;
    QTabWidget* m_tabs = nullptr;

    QWidget* m_directionsTab = nullptr;
    QWidget* m_informationTab = nullptr;
    QWidget* m_housingTab = nullptr;
    QWidget* m_mapTab = nullptr;

    QLabel* m_statusLabel = nullptr;
    QLabel* m_housingEmptyLabel = nullptr;
    QPushButton* m_addHousingButton = nullptr;
    QPushButton* m_newCampusButton = nullptr;
    QPushButton* m_saveCampusButton = nullptr;
    QVBoxLayout* m_housingSectionsLayout = nullptr;
    QVBoxLayout* m_mapSectionsLayout = nullptr;

    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_campusCodeEdit = nullptr;
    QLineEdit* m_buildingEdit = nullptr;
    QLineEdit* m_buildingKrEdit = nullptr;
    QLineEdit* m_phoneEdit = nullptr;
    QLineEdit* m_phoneKrEdit = nullptr;
    QLineEdit* m_directionsNoteEdit = nullptr;
    QLineEdit* m_officeNumberEdit = nullptr;
    QLineEdit* m_officeWifiEdit = nullptr;
    QLineEdit* m_officeWifiPasswordEdit = nullptr;
    QLineEdit* m_printerNameEdit = nullptr;
    QLineEdit* m_printerDriverUrlEdit = nullptr;
    QCheckBox* m_printerDriverUrlUnavailableCheck = nullptr;
    QLineEdit* m_photocopierCodeEdit = nullptr;
    QCheckBox* m_photocopierCodeUnavailableCheck = nullptr;

    QPlainTextEdit* m_transitStepsEdit = nullptr;
    QPlainTextEdit* m_arrivalInfoEdit = nullptr;
    QPlainTextEdit* m_printerStepsEdit = nullptr;

    AddressSectionWidgets m_directionsEnglishAddress;
    AddressSectionWidgets m_directionsKoreanAddress;
    bool m_directionsShowingEnglish = true;

    QList<QLineEdit*> m_lineEdits;
    QList<QLineEdit*> m_alwaysReadOnlyLineEdits;
    QList<QPlainTextEdit*> m_textEdits;
    QList<QPlainTextEdit*> m_alwaysReadOnlyTextEdits;
    QList<HousingSectionWidgets> m_housingSections;
    QList<MapSectionWidgets> m_mapSections;
    QList<TranslatableLabel> m_translatableLabels;
};

#endif // CAMPUS_DASHBOARD_PAGE_H
