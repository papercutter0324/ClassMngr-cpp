#ifndef CAMPUS_DASHBOARD_PAGE_H
#define CAMPUS_DASHBOARD_PAGE_H

#include "../basepage.h"
#include "models/campus_info.h"
#include "services/campus_json_repository.h"

#include <QList>

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

    void refresh() override;

    void saveData() override;
    bool saveChanges() override;
    bool hasUnsavedChanges() const override;
    void discardChanges() override;

private slots:
    void loadSelectedCampus();
    void handleFieldEdited();
    void handleSaveTimeout();

private:
    struct AddressSectionWidgets;
    struct HousingSectionWidgets;

    QWidget* createDirectionsTab();
    QWidget* createInformationTab();
    QWidget* createHousingTab();
    QWidget* createMapTab();

    QLineEdit* addLineField(
        QFormLayout* form,
        const QString& labelText
        );

    QPlainTextEdit* addTextField(
        QFormLayout* form,
        const QString& labelText,
        int minimumLines,
        int maximumLines
        );

    AddressSectionWidgets createAddressSection(
        QWidget* parent,
        bool koreanAddress
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
    void handleAddressSystemToggle(
        QPushButton* button
        );
    void toggleAddressSystem(
        AddressSectionWidgets* section
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
    void updateHousingRemoveButtonVisibility();
    void updatePrinterDriverUrlState();
    void showDirectionsLanguage(
        bool showEnglish
        );
    void syncPhoneFields(
        QLineEdit* source
        );
    void normalizeCampusNameField();
    void handleNewCampus();
    void handleManualCampusSave();

    bool readFieldsIntoCampus(
        CampusInfo* campus,
        QString* errorMessage = nullptr
        ) const;

    void scheduleSave();
    bool saveCurrentCampus();
    void updateCampusSelectorItem(
        int index
        );
    QString campusDisplayName(
        const CampusInfo& campus
        ) const;
    void updateMapPreview();
    void updateDirectionsCompleteAddresses();
    void updateHousingCompleteAddresses();
    void setStatus(
        const QString& message
        );

    struct AddressSectionWidgets
    {
        QWidget* container = nullptr;
        QFormLayout* form = nullptr;
        QPlainTextEdit* complete = nullptr;
        QPushButton* toggleAddressSystemButton = nullptr;
        QLineEdit* province = nullptr;
        QLineEdit* city = nullptr;
        QLineEdit* district = nullptr;
        QLineEdit* line1 = nullptr;
        QLineEdit* line2 = nullptr;
        QLineEdit* line2Suffix = nullptr;
        QLineEdit* postalCode = nullptr;
        QLineEdit* note = nullptr;
        QJsonObject modernAddress;
        QJsonObject classicAddress;
        bool koreanAddress = false;
        bool showingModernAddress = true;
    };

    struct HousingSectionWidgets
    {
        QWidget* container = nullptr;
        QLineEdit* name = nullptr;
        AddressSectionWidgets english;
        AddressSectionWidgets korean;
        QPushButton* toggleLanguageButton = nullptr;
        QPushButton* removeButton = nullptr;
        bool showingEnglish = true;
    };

private:
    bool m_adminMode = false;
    bool m_loading = false;
    bool m_dirty = false;
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
    QLabel* m_mapPreviewLabel = nullptr;
    QLabel* m_housingEmptyLabel = nullptr;
    QPushButton* m_addHousingButton = nullptr;
    QPushButton* m_newCampusButton = nullptr;
    QPushButton* m_saveCampusButton = nullptr;
    QVBoxLayout* m_housingSectionsLayout = nullptr;

    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_campusCodeEdit = nullptr;
    QLineEdit* m_buildingEdit = nullptr;
    QLineEdit* m_buildingKrEdit = nullptr;
    QLineEdit* m_phoneEdit = nullptr;
    QLineEdit* m_phoneKrEdit = nullptr;
    QLineEdit* m_officeNumberEdit = nullptr;
    QLineEdit* m_officeWifiEdit = nullptr;
    QLineEdit* m_officeWifiPasswordEdit = nullptr;
    QLineEdit* m_printerNameEdit = nullptr;
    QLineEdit* m_printerDriverUrlEdit = nullptr;
    QCheckBox* m_printerDriverUrlUnavailableCheck = nullptr;
    QLineEdit* m_photocopierCodeEdit = nullptr;
    QLineEdit* m_imagePathEdit = nullptr;

    QPlainTextEdit* m_transitStepsEdit = nullptr;
    QPlainTextEdit* m_arrivalInfoEdit = nullptr;
    QPlainTextEdit* m_printerStepsEdit = nullptr;

    QPushButton* m_directionsToggleLanguageButton = nullptr;
    AddressSectionWidgets m_directionsEnglishAddress;
    AddressSectionWidgets m_directionsKoreanAddress;
    bool m_directionsShowingEnglish = true;

    QList<QLineEdit*> m_lineEdits;
    QList<QLineEdit*> m_alwaysReadOnlyLineEdits;
    QList<QPlainTextEdit*> m_textEdits;
    QList<QPlainTextEdit*> m_alwaysReadOnlyTextEdits;
    QList<HousingSectionWidgets> m_housingSections;
};

#endif // CAMPUS_DASHBOARD_PAGE_H
