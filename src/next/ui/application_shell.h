#pragma once

#include "next/application/workspace_state.h"
#include "next/resources/resource_scope.h"
#include "next/ui/navigation_model.h"
#include "next/ui/qt_notification_presenter.h"

#include <QHash>
#include <QIcon>
#include <QImage>
#include <QMainWindow>

#include <functional>
#include <optional>
#include <vector>

class QCloseEvent;
class QTreeWidget;
class QPushButton;

namespace ClassMngr::Next::Resources
{
class ResourceLoader;
}

namespace ClassMngr::Next::Ui
{

class PageHost;

class ApplicationShell final : public QMainWindow
{
public:
    // Returning false rejects the close and leaves the shell available for a
    // retry when an application shutdown step cannot complete.
    using CloseHandler = std::function<bool()>;
    using NavigationGuard = std::function<NavigationGuardDecision(
        const NavigationGuardRequest&)>;

    // The resource loader is borrowed and must outlive this shell.
    explicit ApplicationShell(
        Resources::ResourceLoader& resourceLoader,
        QWidget* parent = nullptr
        );

    void setCloseHandler(CloseHandler handler);
    void setNavigationGuard(NavigationGuard guard);
    [[nodiscard]] PageHost& pageHost() noexcept;
    [[nodiscard]] const PageHost& pageHost() const noexcept;
    [[nodiscard]] Application::NotificationPort& notificationPort() noexcept;
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

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void rebuildNavigationTree();
    void synchronizeNavigationSelection();
    void synchronizeHistoryControls();
    [[nodiscard]] std::optional<QIcon> navigationIconFor(
        const NavigationPageMetadata& metadata
        );

    Resources::ResourceLoader* m_resourceLoader = nullptr;
    QWidget* m_contentWidget = nullptr;
    PageHost* m_pageHost = nullptr;
    QTreeWidget* m_navigationTree = nullptr;
    QPushButton* m_backButton = nullptr;
    QPushButton* m_forwardButton = nullptr;
    std::optional<NavigationModel> m_navigationModel;
    QHash<QString, QIcon> m_navigationIcons;
    std::vector<Resources::ScopedResource<QImage>> m_navigationIconImages;
    NavigationGuard m_navigationGuard;
    CloseHandler m_closeHandler;
    QtNotificationPresenter m_notificationPresenter;
};

}  // namespace ClassMngr::Next::Ui
