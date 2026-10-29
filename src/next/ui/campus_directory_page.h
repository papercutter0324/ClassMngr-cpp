#pragma once

#include "next/application/campus_dashboard_campus_save_port.h"
#include "next/application/campus_dashboard_selected_campus_snapshot.h"
#include "next/application/campus_directory_read_port.h"
#include "next/application/campus_edit_capability.h"
#include "next/resources/resource_scope.h"
#include "next/ui/page.h"

#include <QHash>
#include <QString>

#include <functional>
#include <optional>
#include <vector>

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;

namespace ClassMngr::Next::Ui
{

enum class CampusDirectorySection
{
    Information,
    Directions,
    Address,
    Housing,
    Maps
};

struct CampusDirectoryPageActions final
{
    std::function<Application::CampusDirectoryReadResult()> loadCampuses;
    std::function<Application::CampusDashboardSelectedCampusReadResult(
        const Domain::CampusId&)> loadCampus;
    std::function<Domain::Result<void>(
        const Application::CampusDashboardCampusSnapshot&,
        Application::CampusDashboardCampusSaveIntent)> saveCampus;
    std::function<std::optional<Domain::CampusId>()> readLastSelectedCampus;
    std::function<Domain::Result<void>(
        const std::optional<Domain::CampusId>&)> saveLastSelectedCampus;
    std::function<Domain::Result<std::size_t>()> importLegacyCampuses;
    std::function<Domain::Result<std::vector<Resources::ResourceId>>(
        const Application::CampusDashboardCampusSnapshot&)> mapResourcesFor;
    Application::CampusEditCapability editCapability =
        Application::CampusEditCapability::readOnly();
};

class CampusDirectoryPage final : public Page
{
public:
    CampusDirectoryPage(
        const PageContext& context,
        CampusDirectorySection section,
        CampusDirectoryPageActions actions,
        QWidget* parent = nullptr
        );
    ~CampusDirectoryPage() override;

    [[nodiscard]] PageOperationResult<void> onActivating() override;
    [[nodiscard]] bool hasUnsavedChanges() const noexcept override;
    [[nodiscard]] PageOperationResult<void> saveChanges() override;
    [[nodiscard]] PageOperationResult<void> discardChanges() override;
    void onSuspending() noexcept override;

    [[nodiscard]] CampusDirectorySection section() const noexcept { return m_section; }
    [[nodiscard]] const std::optional<Application::CampusDashboardCampusSnapshot>&
    currentCampus() const noexcept { return m_currentCampus; }
    [[nodiscard]] bool isDirty() const noexcept { return m_dirty; }

private:
    void buildUi();
    void buildSectionForm();
    void populateCampusSelector();
    void loadCurrentCampus(const Domain::CampusId& campusId);
    void loadSelectedCampusFromPreference();
    void populateFields(const Application::CampusDashboardCampusSnapshot& campus);
    [[nodiscard]] Application::CampusDashboardCampusSnapshot collectFields() const;
    void clearMapResources() noexcept;
    void loadMapResources();
    void refreshMapLinkButtons();
    void updateDirtyState();
    void setStatus(const QString& text, bool isError = false);
    void retry();
    void importLegacy();
    void fieldEdited();
    void openHttpsUrl(const QString& url);
    [[nodiscard]] QString pageTitle() const;

    CampusDirectorySection m_section;
    CampusDirectoryPageActions m_actions;
    QComboBox* m_campusSelector = nullptr;
    QWidget* m_formContainer = nullptr;
    QVBoxLayout* m_formLayout = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_saveButton = nullptr;
    QPushButton* m_discardButton = nullptr;
    QPushButton* m_retryButton = nullptr;
    QPushButton* m_importButton = nullptr;
    QCheckBox* m_printerDriverUnavailable = nullptr;
    QHash<QString, QLineEdit*> m_lineFields;
    QHash<QString, QPlainTextEdit*> m_textFields;
    std::vector<Application::CampusDirectoryEntry> m_directory;
    std::optional<Application::CampusDashboardCampusSnapshot> m_currentCampus;
    std::vector<Resources::ScopedResource<QImage>> m_mapImages;
    std::vector<QWidget*> m_mapCards;
    bool m_loading = false;
    bool m_loaded = false;
    bool m_dirty = false;
};

} // namespace ClassMngr::Next::Ui
