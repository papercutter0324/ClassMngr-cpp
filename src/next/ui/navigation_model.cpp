#include "next/ui/navigation_model.h"

#include <QCoreApplication>
#include <QSet>

#include <algorithm>
#include <functional>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

NavigationModelError failure(
    const NavigationModelErrorCode code,
    const QString& nodeId,
    QString message
    )
{
    return {code, nodeId, std::move(message)};
}

bool validWorkspaceState(const Application::WorkspaceLifecycleState state)
{
    return state == Application::WorkspaceLifecycleState::Closed
        || state == Application::WorkspaceLifecycleState::Open;
}

bool validAvailability(const NavigationWorkspaceAvailability availability)
{
    return availability == NavigationWorkspaceAvailability::Always
        || availability == NavigationWorkspaceAvailability::WorkspaceOpen;
}

}  // namespace

NavigationHistoryActionState deriveNavigationHistoryActionState(
    const std::vector<PageId>& history,
    const std::optional<std::size_t> historyIndex,
    const std::function<bool(const PageId&)>& targetIsAvailable
    )
{
    if (!historyIndex || *historyIndex >= history.size())
    {
        return {};
    }

    const bool backEnabled = *historyIndex > 0
        && targetIsAvailable(history[*historyIndex - 1]);
    const bool forwardEnabled = *historyIndex + 1 < history.size()
        && targetIsAvailable(history[*historyIndex + 1]);
    return NavigationHistoryActionState{
        .backEnabled = backEnabled,
        .forwardEnabled = forwardEnabled
    };
}

NavigationNode NavigationNode::group(
    QString id,
    QString label,
    std::vector<NavigationNode> children,
    const NavigationWorkspaceAvailability availability,
    const bool retainWhenEmpty,
    QString labelTranslationSource
    )
{
    return NavigationNode{
        .kind = Kind::Group,
        .id = std::move(id),
        .label = std::move(label),
        .labelTranslationSource = std::move(labelTranslationSource),
        .pageId = std::nullopt,
        .children = std::move(children),
        .availability = availability,
        .retainWhenEmpty = retainWhenEmpty
    };
}

NavigationNode NavigationNode::page(
    PageId pageId,
    const NavigationWorkspaceAvailability availability,
    const bool displayInSidebar
    )
{
    return NavigationNode{
        .kind = Kind::Page,
        .id = pageId.value(),
        .label = {},
        .labelTranslationSource = {},
        .pageId = std::move(pageId),
        .children = {},
        .availability = availability,
        .retainWhenEmpty = false,
        .displayInSidebar = displayInSidebar
    };
}

NavigationNode NavigationNode::placeholder(
    QString id,
    QString label,
    const NavigationWorkspaceAvailability availability,
    QString labelTranslationSource
    )
{
    return NavigationNode{
        .kind = Kind::Placeholder,
        .id = std::move(id),
        .label = std::move(label),
        .labelTranslationSource = std::move(labelTranslationSource),
        .pageId = std::nullopt,
        .children = {},
        .availability = availability,
        .retainWhenEmpty = false
    };
}

NavigationModelResult<NavigationModel> NavigationModel::create(
    std::vector<NavigationNode> nodes,
    std::vector<PageDescriptor> descriptors,
    PageId initialPageWhenClosed,
    PageId initialPageWhenOpen,
    const Application::WorkspaceLifecycleState workspaceState,
    std::optional<QStringList> grantedPermissionIds
    )
{
    if (!validWorkspaceState(workspaceState))
    {
        return std::unexpected(failure(
            NavigationModelErrorCode::InvalidNode, {},
            QStringLiteral("workspace state is not recognized")
            ));
    }

    std::vector<NavigationPageMetadata> pages;
    pages.reserve(descriptors.size());
    QSet<QString> pageIds;
    for (const PageDescriptor& descriptor : descriptors)
    {
        const QString id = descriptor.id.value();
        if (id.trimmed().isEmpty() || descriptor.label.trimmed().isEmpty())
        {
            return std::unexpected(failure(
                NavigationModelErrorCode::InvalidPageDescriptor, id,
                QStringLiteral("page descriptor ID and label must not be empty")
                ));
        }
        if (pageIds.contains(id))
        {
            return std::unexpected(failure(
                NavigationModelErrorCode::DuplicatePageDescriptor, id,
                QStringLiteral("page descriptor IDs must be unique")
                ));
        }
        for (const QString& permission : descriptor.requiredPermissions)
        {
            if (permission.trimmed().isEmpty())
            {
                return std::unexpected(failure(
                    NavigationModelErrorCode::InvalidPageDescriptor, id,
                    QStringLiteral("required permission IDs must not be empty")
                    ));
            }
        }
        pageIds.insert(id);
        pages.push_back({
            .id = descriptor.id,
            .label = descriptor.label,
            .labelTranslationSource = descriptor.labelTranslationSource,
            .iconResourceId = descriptor.iconResourceId,
            .requiredPermissions = descriptor.requiredPermissions
        });
    }

    QSet<QString> nodeIds;
    QSet<QString> referencedPages;
    std::function<NavigationModelResult<void>(const NavigationNode&)> validateNode;
    validateNode = [&](const NavigationNode& node) -> NavigationModelResult<void> {
        if (node.id.trimmed().isEmpty()
            || node.id != node.id.trimmed()
            || !validAvailability(node.availability))
        {
            return std::unexpected(failure(
                NavigationModelErrorCode::InvalidNode, node.id,
                QStringLiteral("navigation node ID and availability must be valid")
                ));
        }
        if (nodeIds.contains(node.id))
        {
            return std::unexpected(failure(
                NavigationModelErrorCode::DuplicateNodeId, node.id,
                QStringLiteral("navigation node IDs must be unique")
                ));
        }
        nodeIds.insert(node.id);

        if (node.kind == NavigationNode::Kind::Group)
        {
            if (node.pageId || node.label.trimmed().isEmpty())
            {
                return std::unexpected(failure(
                    NavigationModelErrorCode::InvalidNode, node.id,
                    QStringLiteral("group nodes need a label and cannot reference a page")
                    ));
            }
            for (const NavigationNode& child : node.children)
            {
                auto result = validateNode(child);
                if (!result)
                {
                    return result;
                }
            }
            return {};
        }

        if (node.kind == NavigationNode::Kind::Placeholder)
        {
            if (node.pageId || !node.children.empty() || node.label.trimmed().isEmpty()
                || node.retainWhenEmpty)
            {
                return std::unexpected(failure(
                    NavigationModelErrorCode::InvalidNode, node.id,
                    QStringLiteral("placeholder nodes need a label and cannot reference a page")
                    ));
            }
            return {};
        }

        if (node.kind != NavigationNode::Kind::Page || !node.pageId
            || !node.children.empty() || !node.label.isEmpty()
            || node.retainWhenEmpty || node.id != node.pageId->value())
        {
            return std::unexpected(failure(
                NavigationModelErrorCode::InvalidNode, node.id,
                QStringLiteral("page nodes must be leaves referencing a page ID")
                ));
        }
        const QString pageId = node.pageId->value();
        if (!pageIds.contains(pageId))
        {
            return std::unexpected(failure(
                NavigationModelErrorCode::UnknownPage, pageId,
                QStringLiteral("navigation page node has no registered descriptor")
                ));
        }
        if (referencedPages.contains(pageId))
        {
            return std::unexpected(failure(
                NavigationModelErrorCode::DuplicateNodeId, pageId,
                QStringLiteral("a page descriptor may appear only once in the tree")
                ));
        }
        referencedPages.insert(pageId);
        return {};
    };
    for (const NavigationNode& node : nodes)
    {
        auto result = validateNode(node);
        if (!result)
        {
            return std::unexpected(result.error());
        }
    }

    NavigationModel model(
        std::move(nodes), std::move(pages),
        std::move(initialPageWhenClosed), std::move(initialPageWhenOpen),
        workspaceState, std::move(grantedPermissionIds)
        );
    const auto initial = model.initialPageId();
    if (!initial)
    {
        return std::unexpected(failure(
            NavigationModelErrorCode::InitialPageUnavailable, {},
            QStringLiteral("the initial page is not visible in the current access context")
            ));
    }
    return model;
}

NavigationModel::NavigationModel(
    std::vector<NavigationNode> nodes,
    std::vector<NavigationPageMetadata> pages,
    PageId initialPageWhenClosed,
    PageId initialPageWhenOpen,
    const Application::WorkspaceLifecycleState workspaceState,
    std::optional<QStringList> grantedPermissionIds
    )
    : m_nodes(std::move(nodes))
    , m_pages(std::move(pages))
    , m_initialPageWhenClosed(std::move(initialPageWhenClosed))
    , m_initialPageWhenOpen(std::move(initialPageWhenOpen))
    , m_workspaceState(workspaceState)
    , m_grantedPermissionIds(std::move(grantedPermissionIds))
{
}

const std::vector<NavigationNode>& NavigationModel::nodes() const noexcept
{
    return m_nodes;
}

std::vector<NavigationNode> NavigationModel::visibleNodes() const
{
    std::function<std::optional<NavigationNode>(const NavigationNode&)> filter;
    filter = [&](const NavigationNode& node) -> std::optional<NavigationNode> {
        if (node.availability == NavigationWorkspaceAvailability::WorkspaceOpen
            && m_workspaceState != Application::WorkspaceLifecycleState::Open)
        {
            return std::nullopt;
        }
        if (node.kind == NavigationNode::Kind::Page)
        {
            const auto metadata = pageMetadata(*node.pageId);
            return metadata && permits(*metadata)
                ? std::optional<NavigationNode>(node)
                : std::nullopt;
        }
        if (node.kind == NavigationNode::Kind::Placeholder)
        {
            return node;
        }

        NavigationNode visible = node;
        visible.children.clear();
        for (const NavigationNode& child : node.children)
        {
            auto visibleChild = filter(child);
            if (visibleChild)
            {
                visible.children.push_back(std::move(*visibleChild));
            }
        }
        if (visible.children.empty() && !node.retainWhenEmpty)
        {
            return std::nullopt;
        }
        return visible;
    };

    std::vector<NavigationNode> visible;
    for (const NavigationNode& node : m_nodes)
    {
        auto visibleNode = filter(node);
        if (visibleNode)
        {
            visible.push_back(std::move(*visibleNode));
        }
    }
    return visible;
}

std::optional<NavigationPageMetadata> NavigationModel::pageMetadata(
    const PageId& pageId
    ) const
{
    const auto found = std::find_if(m_pages.cbegin(), m_pages.cend(),
        [&pageId](const NavigationPageMetadata& metadata) {
            return metadata.id == pageId;
        });
    return found == m_pages.cend()
        ? std::nullopt
        : std::optional<NavigationPageMetadata>(*found);
}

void NavigationModel::retranslate()
{
    std::function<void(NavigationNode&)> translateNode;
    translateNode = [&translateNode](NavigationNode& node) {
        if (!node.labelTranslationSource.isEmpty())
        {
            node.label = QCoreApplication::translate(
                "NextNavigation", node.labelTranslationSource.toUtf8().constData());
        }
        for (NavigationNode& child : node.children)
        {
            translateNode(child);
        }
    };
    for (NavigationNode& node : m_nodes)
    {
        translateNode(node);
    }
    for (NavigationPageMetadata& page : m_pages)
    {
        if (!page.labelTranslationSource.isEmpty())
        {
            page.label = QCoreApplication::translate(
                "NextNavigation", page.labelTranslationSource.toUtf8().constData());
        }
    }
}

bool NavigationModel::isPageVisible(const PageId& pageId) const
{
    const auto metadata = pageMetadata(pageId);
    if (!metadata || !permits(*metadata))
    {
        return false;
    }
    std::function<bool(const NavigationNode&)> contains;
    contains = [&](const NavigationNode& node) {
        if (node.availability == NavigationWorkspaceAvailability::WorkspaceOpen
            && m_workspaceState != Application::WorkspaceLifecycleState::Open)
        {
            return false;
        }
        if (node.kind == NavigationNode::Kind::Page)
        {
            return node.pageId == std::optional<PageId>(pageId);
        }
        return std::any_of(node.children.cbegin(), node.children.cend(), contains);
    };
    return std::any_of(m_nodes.cbegin(), m_nodes.cend(), contains);
}

std::optional<PageId> NavigationModel::initialPageId() const
{
    const PageId& target =
        m_workspaceState == Application::WorkspaceLifecycleState::Open
            ? m_initialPageWhenOpen
            : m_initialPageWhenClosed;
    return isPageVisible(target) ? std::optional<PageId>(target) : std::nullopt;
}

std::optional<PageId> NavigationModel::currentPageId() const { return m_currentPageId; }
std::optional<PageId> NavigationModel::selectedPageId() const { return m_currentPageId; }
const std::vector<PageId>& NavigationModel::history() const noexcept { return m_history; }
std::optional<std::size_t> NavigationModel::historyIndex() const noexcept { return m_historyIndex; }
NavigationHistoryActionState NavigationModel::historyActionState() const
{
    return deriveNavigationHistoryActionState(
        m_history,
        m_historyIndex,
        [this](const PageId& target) { return isPageVisible(target); }
        );
}
bool NavigationModel::canGoBack() const
{
    return historyActionState().backEnabled;
}
bool NavigationModel::canGoForward() const
{
    return historyActionState().forwardEnabled;
}
std::optional<PageId> NavigationModel::backTarget() const
{
    return canGoBack() ? std::optional<PageId>(m_history[*m_historyIndex - 1]) : std::nullopt;
}
std::optional<PageId> NavigationModel::forwardTarget() const
{
    return canGoForward() ? std::optional<PageId>(m_history[*m_historyIndex + 1]) : std::nullopt;
}
Application::WorkspaceLifecycleState NavigationModel::workspaceState() const noexcept
{
    return m_workspaceState;
}
const std::optional<QStringList>& NavigationModel::grantedPermissionIds() const noexcept
{
    return m_grantedPermissionIds;
}

void NavigationModel::updateAccessContext(
    const Application::WorkspaceLifecycleState workspaceState,
    std::optional<QStringList> grantedPermissionIds
    )
{
    if (validWorkspaceState(workspaceState))
    {
        m_workspaceState = workspaceState;
        m_grantedPermissionIds = std::move(grantedPermissionIds);
    }
}

bool NavigationModel::canCommit(const PageId& target, const NavigationCause cause) const
{
    if (!isPageVisible(target))
    {
        return false;
    }
    switch (cause)
    {
    case NavigationCause::Initial:
        return !m_currentPageId && m_history.empty();
    case NavigationCause::Selection:
        return true;
    case NavigationCause::Back:
        return backTarget() == std::optional<PageId>(target);
    case NavigationCause::Forward:
        return forwardTarget() == std::optional<PageId>(target);
    }
    return false;
}

void NavigationModel::commit(const PageId& target, const NavigationCause cause)
{
    switch (cause)
    {
    case NavigationCause::Initial:
        m_history = {target};
        m_historyIndex = 0;
        break;
    case NavigationCause::Selection:
        if (m_currentPageId != std::optional<PageId>(target))
        {
            if (m_historyIndex)
            {
                m_history.erase(
                    m_history.begin() + static_cast<std::ptrdiff_t>(*m_historyIndex + 1),
                    m_history.end()
                    );
            }
            else
            {
                m_history.clear();
            }
            m_history.push_back(target);
            m_historyIndex = m_history.size() - 1;
        }
        break;
    case NavigationCause::Back:
        if (canGoBack())
        {
            --*m_historyIndex;
        }
        break;
    case NavigationCause::Forward:
        if (canGoForward())
        {
            ++*m_historyIndex;
        }
        break;
    }
    m_currentPageId = target;
}

bool NavigationModel::permits(const NavigationPageMetadata& metadata) const
{
    if (metadata.requiredPermissions.isEmpty())
    {
        return true;
    }
    if (!m_grantedPermissionIds)
    {
        return false;
    }
    return std::all_of(
        metadata.requiredPermissions.cbegin(), metadata.requiredPermissions.cend(),
        [this](const QString& required) {
            return m_grantedPermissionIds->contains(required);
        }
        );
}

}  // namespace ClassMngr::Next::Ui
