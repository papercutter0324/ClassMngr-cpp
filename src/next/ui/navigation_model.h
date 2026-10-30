#pragma once

#include "next/application/workspace_state.h"
#include "next/ui/page_descriptor.h"

#include <QUrl>
#include <QStringList>

#include <expected>
#include <functional>
#include <optional>
#include <vector>

namespace ClassMngr::Next::Ui
{

class ApplicationShell;

enum class NavigationWorkspaceAvailability
{
    Always,
    WorkspaceOpen
};

// A value-only node. Group labels live here; page labels, icons, and required
// permissions are read from the referenced PageDescriptor.
struct NavigationNode final
{
    enum class Kind
    {
        Group,
        Page,
        Placeholder,
        ExternalLink
    };

    Kind kind = Kind::Group;
    QString id;
    QString label;
    // Source text for retranslation when the active Qt language changes.
    // Empty labels remain literal, such as names loaded from user data.
    QString labelTranslationSource;
    std::optional<PageId> pageId;
    std::optional<QUrl> externalUrl;
    std::vector<NavigationNode> children;
    NavigationWorkspaceAvailability availability =
        NavigationWorkspaceAvailability::Always;
    bool retainWhenEmpty = false;
    bool displayInSidebar = true;

    [[nodiscard]] static NavigationNode group(
        QString id,
        QString label,
        std::vector<NavigationNode> children = {},
        NavigationWorkspaceAvailability availability =
            NavigationWorkspaceAvailability::Always,
        bool retainWhenEmpty = false,
        QString labelTranslationSource = {}
        );

    [[nodiscard]] static NavigationNode page(
        PageId pageId,
        NavigationWorkspaceAvailability availability =
            NavigationWorkspaceAvailability::Always,
        bool displayInSidebar = true
        );

    [[nodiscard]] static NavigationNode placeholder(
        QString id,
        QString label,
        NavigationWorkspaceAvailability availability =
            NavigationWorkspaceAvailability::Always,
        QString labelTranslationSource = {}
        );

    [[nodiscard]] static NavigationNode externalLink(
        QString id,
        QString label,
        QUrl url,
        NavigationWorkspaceAvailability availability =
            NavigationWorkspaceAvailability::Always,
        QString labelTranslationSource = {}
        );
};

enum class NavigationModelErrorCode
{
    InvalidNode,
    DuplicateNodeId,
    UnknownPage,
    DuplicatePageDescriptor,
    InvalidPageDescriptor,
    InitialPageUnavailable
};

struct NavigationModelError final
{
    NavigationModelErrorCode code;
    QString nodeId;
    QString message;
};

template <typename Value>
using NavigationModelResult = std::expected<Value, NavigationModelError>;

enum class NavigationCause
{
    Initial,
    Selection,
    Back,
    Forward
};

struct NavigationGuardRequest final
{
    std::optional<PageId> source;
    PageId target;
    NavigationCause cause;
};

enum class NavigationGuardDecision
{
    Allow,
    Reject
};

struct NavigationPageMetadata final
{
    PageId id;
    QString label;
    QString labelTranslationSource;
    std::optional<Resources::ResourceId> iconResourceId;
    QStringList requiredPermissions;
};

struct NavigationHistoryActionState final
{
    bool backEnabled = false;
    bool forwardEnabled = false;
};

// Derives the value state for history actions from history position and
// whether each adjacent target is currently available. The availability
// predicate is supplied by the owning navigation policy.
[[nodiscard]] NavigationHistoryActionState deriveNavigationHistoryActionState(
    const std::vector<PageId>& history,
    std::optional<std::size_t> historyIndex,
    const std::function<bool(const PageId&)>& targetIsAvailable
    );

// Stores the sidebar graph, descriptor-derived route metadata, access context,
// selection, and committed page-ID history. It owns no widgets or page
// instances; ApplicationShell performs guard and PageHost activation before
// committing a requested transition.
class NavigationModel final
{
public:
    NavigationModel(const NavigationModel&) = default;
    NavigationModel(NavigationModel&&) noexcept = default;
    NavigationModel& operator=(const NavigationModel&) = default;
    NavigationModel& operator=(NavigationModel&&) noexcept = default;

    [[nodiscard]] static NavigationModelResult<NavigationModel> create(
        std::vector<NavigationNode> nodes,
        std::vector<PageDescriptor> descriptors,
        PageId initialPageWhenClosed,
        PageId initialPageWhenOpen,
        Application::WorkspaceLifecycleState workspaceState,
        std::optional<QStringList> grantedPermissionIds = std::nullopt
        );

    [[nodiscard]] const std::vector<NavigationNode>& nodes() const noexcept;
    [[nodiscard]] std::vector<NavigationNode> visibleNodes() const;
    [[nodiscard]] std::optional<NavigationPageMetadata> pageMetadata(
        const PageId& pageId
        ) const;
    void retranslate();
    [[nodiscard]] bool isPageVisible(const PageId& pageId) const;

    [[nodiscard]] std::optional<PageId> initialPageId() const;
    [[nodiscard]] std::optional<PageId> currentPageId() const;
    [[nodiscard]] std::optional<PageId> selectedPageId() const;
    [[nodiscard]] const std::vector<PageId>& history() const noexcept;
    [[nodiscard]] std::optional<std::size_t> historyIndex() const noexcept;
    [[nodiscard]] NavigationHistoryActionState historyActionState() const;
    [[nodiscard]] bool canGoBack() const;
    [[nodiscard]] bool canGoForward() const;
    [[nodiscard]] std::optional<PageId> backTarget() const;
    [[nodiscard]] std::optional<PageId> forwardTarget() const;
    [[nodiscard]] Application::WorkspaceLifecycleState workspaceState() const noexcept;
    [[nodiscard]] const std::optional<QStringList>& grantedPermissionIds() const noexcept;

    void updateAccessContext(
        Application::WorkspaceLifecycleState workspaceState,
        std::optional<QStringList> grantedPermissionIds
        );

private:
    friend class ApplicationShell;

    explicit NavigationModel(
        std::vector<NavigationNode> nodes,
        std::vector<NavigationPageMetadata> pages,
        PageId initialPageWhenClosed,
        PageId initialPageWhenOpen,
        Application::WorkspaceLifecycleState workspaceState,
        std::optional<QStringList> grantedPermissionIds
        );

    [[nodiscard]] bool canCommit(
        const PageId& target,
        NavigationCause cause
        ) const;
    void commit(const PageId& target, NavigationCause cause);
    [[nodiscard]] bool permits(const NavigationPageMetadata& metadata) const;

    std::vector<NavigationNode> m_nodes;
    std::vector<NavigationPageMetadata> m_pages;
    PageId m_initialPageWhenClosed;
    PageId m_initialPageWhenOpen;
    Application::WorkspaceLifecycleState m_workspaceState;
    std::optional<QStringList> m_grantedPermissionIds;
    std::optional<PageId> m_currentPageId;
    std::vector<PageId> m_history;
    std::optional<std::size_t> m_historyIndex;
};

}  // namespace ClassMngr::Next::Ui
