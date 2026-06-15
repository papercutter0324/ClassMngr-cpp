#ifndef CAMPUS_DASHBOARD_PAGE_H
#define CAMPUS_DASHBOARD_PAGE_H

#include "../basepage.h"
#include "models/campus_info.h"
#include "services/campus_json_repository.h"

#include <QList>

class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTabWidget;
class QTimer;
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
    QWidget* createDirectionsTab();
    QWidget* createInformationTab();
    QWidget* createMapTab();

    QLineEdit* addLineField(
        QFormLayout* form,
        const QString& labelText
        );

    QPlainTextEdit* addTextField(
        QFormLayout* form,
        const QString& labelText,
        int minimumHeight
        );

    void buildUi();
    void applyAdminMode();
    void loadCampuses();

    void populateFields(
        const CampusInfo& campus
        );

    bool readFieldsIntoCampus(
        CampusInfo* campus,
        QString* errorMessage = nullptr
        ) const;

    void scheduleSave();
    bool saveCurrentCampus();
    void updateMapPreview();
    void setStatus(
        const QString& message
        );

private:
    bool m_adminMode = false;
    bool m_loading = false;
    bool m_dirty = false;

    CampusJsonRepository m_repository;
    CampusInfo m_currentCampus;

    QTimer* m_saveTimer = nullptr;

    QComboBox* m_campusCombo = nullptr;
    QTabWidget* m_tabs = nullptr;

    QWidget* m_directionsTab = nullptr;
    QWidget* m_informationTab = nullptr;
    QWidget* m_mapTab = nullptr;

    QLabel* m_statusLabel = nullptr;
    QLabel* m_mapPreviewLabel = nullptr;

    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_buildingEdit = nullptr;
    QLineEdit* m_addressEdit = nullptr;
    QLineEdit* m_phoneEdit = nullptr;
    QLineEdit* m_officeNumberEdit = nullptr;
    QLineEdit* m_officeWifiEdit = nullptr;
    QLineEdit* m_officeWifiPasswordEdit = nullptr;
    QLineEdit* m_printerNameEdit = nullptr;
    QLineEdit* m_photocopierCodeEdit = nullptr;
    QLineEdit* m_imagePathEdit = nullptr;

    QPlainTextEdit* m_transitStepsEdit = nullptr;
    QPlainTextEdit* m_arrivalInfoEdit = nullptr;
    QPlainTextEdit* m_printerStepsEdit = nullptr;
    QPlainTextEdit* m_housingLocationsEdit = nullptr;

    QList<QLineEdit*> m_lineEdits;
    QList<QPlainTextEdit*> m_textEdits;
};

#endif // CAMPUS_DASHBOARD_PAGE_H
