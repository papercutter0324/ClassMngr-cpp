#pragma once

#include "next/application/workspace_state.h"
#include "next/application/recent_workspace_history.h"
#include "next/application/preferences_model.h"
#include "next/application/document_catalog_projection.h"
#include "next/domain/domain_types.h"
#include "next/resources/resource_scope.h"
#include "next/ui/navigation_model.h"
#include "next/ui/qt_notification_presenter.h"

#include <QAction>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QMainWindow>

#include <functional>
#include <optional>
#include <vector>

class QCloseEvent;
class QEvent;
class QResizeEvent;
class QTreeWidget;
class QPushButton;
class QMenu;

namespace ClassMngr::Next::Resources
{
class ResourceLoader;
}

namespace ClassMngr::Next::Ui
{

class PageHost;

struct KoreanTeacherNavigationRow final
{
    Domain::TeacherId teacherId;
    QString label;

    friend bool operator==(
        const KoreanTeacherNavigationRow&,
        const KoreanTeacherNavigationRow&
        ) = default;
};

struct KoreanTeacherNavigationActions final
{
    std::function<bool(const Domain::TeacherId&)> openTeacherProfile;
    std::function<void()> createTeacher;
    std::function<void(std::optional<Domain::TeacherId>)> deleteTeacher;
};

class ApplicationShell final : public QMainWindow
{
public:
    // Returning false rejects the close and leaves the shell available for a
    // retry when an application shutdown step cannot complete.
    using CloseHandler = std::function<bool()>;
    using NavigationGuard = std::function<NavigationGuardDecision(
        const NavigationGuardRequest&)>;
    using RecentWorkspaceHistoryLoader =
        std::function<Application::RecentWorkspaceHistory()>;
    using RecentWorkspaceOpenHandler =
        std::function<void(const Application::RecentWorkspacePath&)>;
    using DocumentCatalogProjectionLoader = std::function<
        Domain::Result<Application::DocumentCatalogProjection>(const QString&)>;

    // The resource loader and preferences model are borrowed and must outlive this shell.
    explicit ApplicationShell(
        Resources::ResourceLoader& resourceLoader,
        Application::PreferencesModel& preferences,
        QWidget* parent = nullptr
        );
    ~ApplicationShell() override;

    void setCloseHandler(CloseHandler handler);
    void setNewTeacherProfileHandler(std::function<void()> handler);
    void setOpenTeacherProfileHandler(std::function<void()> handler);
    void setRecoverInterruptedTeacherProfileHandler(
        std::function<void()> handler
        );
    void setSaveWorkspaceHandler(std::function<void()> handler);
    void setSaveWorkspaceAsHandler(std::function<void()> handler);
    void setExportWorkspaceHandler(std::function<void()> handler);
    void setImportTeachersHandler(std::function<void()> handler);
    void setUpcomingBirthdaysHandler(std::function<void()> handler);
    void setCloseWorkspaceHandler(std::function<void()> handler);
    void setPreferencesHandler(std::function<void()> handler);
    void setRecentWorkspaceMenuHandlers(
        RecentWorkspaceHistoryLoader historyLoader,
        RecentWorkspaceOpenHandler openHandler,
        std::function<void()> clearHandler
        );
    void setNavigationGuard(NavigationGuard guard);
    [[nodiscard]] PageHost& pageHost() noexcept;
    [[nodiscard]] const PageHost& pageHost() const noexcept;
    [[nodiscard]] Application::NotificationPort& notificationPort() noexcept;
    [[nodiscard]] QMenu* recentWorkspaceMenu() noexcept;
    [[nodiscard]] NavigationModel* navigationModel() noexcept;
    [[nodiscard]] const NavigationModel* navigationModel() const noexcept;
    [[nodiscard]] NavigationModelResult<void> configureNavigation(
        std::vector<NavigationNode> nodes,
        PageId initialPageWhenClosed,
        PageId initialPageWhenOpen,
        Application::WorkspaceLifecycleState workspaceState,
        std::optional<QStringList> grantedPermissionIds = std::nullopt
        );
    [[nodiscard]] bool navigateTo(
        const PageId& pageId,
        NavigationCause cause = NavigationCause::Selection
        );
    [[nodiscard]] bool navigateBack();
    [[nodiscard]] bool navigateForward();
    void updateNavigationAccessContext(
        Application::WorkspaceLifecycleState workspaceState,
        std::optional<QStringList> grantedPermissionIds
        );
    void setKoreanTeacherNavigationActions(
        KoreanTeacherNavigationActions actions
        );
    void setKoreanTeacherNavigationRows(
        std::vector<KoreanTeacherNavigationRow> rows
        );
    void setDocumentCatalogProjection(
        Application::DocumentCatalogProjection projection
        );
    void setDocumentCatalogProjectionLoader(
        DocumentCatalogProjectionLoader loader
        );
    [[nodiscard]] Domain::Result<void> refreshDocumentCatalogProjection(
        const QString& localeName
        );
    [[nodiscard]] const Application::DocumentCatalogProjection*
        documentCatalogProjection() const noexcept;
    void setDocumentCatalogSelectionHandler(
        std::function<void(const Domain::DocumentId&)> handler
        );
    void setDocumentCatalogSelectionClearedHandler(
        std::function<void()> handler
        );
    void selectKoreanTeacherNavigationRow(
        const Domain::TeacherId& teacherId
        );

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void synchronizeWorkspaceActions(
        Application::WorkspaceLifecycleState workspaceState
        ) noexcept;
    void synchronizeTeacherImportAction() noexcept;
    void rebuildNavigationTree();
    void showKoreanTeacherContextMenu(const QPoint& position);
    void synchronizeNavigationSelection();
    void synchronizeHistoryControls();
    void refreshRecentWorkspaceMenu();
    void retranslateUi();
    void applyPreferences(
        const Application::UserPreferencesSnapshot& snapshot
        );
    void updateNavigationTooltips(bool enabled);
    [[nodiscard]] std::optional<QIcon> navigationIconFor(
        const NavigationPageMetadata& metadata
        );

    Resources::ResourceLoader* m_resourceLoader = nullptr;
    QWidget* m_contentWidget = nullptr;
    PageHost* m_pageHost = nullptr;
    QTreeWidget* m_navigationTree = nullptr;
    QPushButton* m_backButton = nullptr;
    QPushButton* m_forwardButton = nullptr;
    QAction* m_newTeacherProfileAction = nullptr;
    QAction* m_openTeacherProfileAction = nullptr;
    QAction* m_recoverInterruptedTeacherProfileAction = nullptr;
    QAction* m_saveWorkspaceAction = nullptr;
    QAction* m_saveWorkspaceAsAction = nullptr;
    QAction* m_exportWorkspaceAction = nullptr;
    QAction* m_importTeachersAction = nullptr;
    QAction* m_upcomingBirthdaysAction = nullptr;
    QAction* m_closeWorkspaceAction = nullptr;
    QAction* m_preferencesAction = nullptr;
    QMenu* m_fileMenu = nullptr;
    QMenu* m_editMenu = nullptr;
    QMenu* m_teachersMenu = nullptr;
    QMenu* m_recentWorkspaceMenu = nullptr;
    std::optional<NavigationModel> m_navigationModel;
    std::optional<Application::DocumentCatalogProjection>
        m_documentCatalogProjection;
    DocumentCatalogProjectionLoader m_documentCatalogProjectionLoader;
    std::function<void(const Domain::DocumentId&)>
        m_documentCatalogSelectionHandler;
    std::function<void()> m_documentCatalogSelectionClearedHandler;
    std::vector<KoreanTeacherNavigationRow> m_koreanTeacherNavigationRows;
    std::optional<Domain::TeacherId> m_lastOpenedKoreanTeacherId;
    KoreanTeacherNavigationActions m_koreanTeacherNavigationActions;
    QHash<QString, QIcon> m_navigationIcons;
    std::vector<Resources::ScopedResource<QImage>> m_navigationIconImages;
    NavigationGuard m_navigationGuard;
    CloseHandler m_closeHandler;
    std::function<void()> m_newTeacherProfileHandler;
    std::function<void()> m_openTeacherProfileHandler;
    std::function<void()> m_recoverInterruptedTeacherProfileHandler;
    std::function<void()> m_saveWorkspaceHandler;
    std::function<void()> m_saveWorkspaceAsHandler;
    std::function<void()> m_exportWorkspaceHandler;
    std::function<void()> m_importTeachersHandler;
    std::function<void()> m_upcomingBirthdaysHandler;
    std::function<void()> m_closeWorkspaceHandler;
    std::function<void()> m_preferencesHandler;
    RecentWorkspaceHistoryLoader m_recentWorkspaceHistoryLoader;
    RecentWorkspaceOpenHandler m_recentWorkspaceOpenHandler;
    std::function<void()> m_clearRecentWorkspaceHistoryHandler;
    bool m_workspaceIsOpen = false;
    QtNotificationPresenter m_notificationPresenter;
    Application::PreferencesModel* m_preferences = nullptr;
    std::optional<Application::PreferencesModel::ObserverId>
        m_preferencesObserverId;
};

}  // namespace ClassMngr::Next::Ui
