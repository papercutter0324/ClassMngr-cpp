#include "next/ui/application_shell.h"

#include "next/ui/page_host.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <exception>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

constexpr int NodeKindRole = Qt::UserRole;
constexpr int PageIdRole = Qt::UserRole + 1;
constexpr int NodeIdRole = Qt::UserRole + 2;

[[nodiscard]] NavigationModelError shellError(
    const NavigationModelErrorCode code,
    const QString& message
    )
{
    return NavigationModelError{code, {}, message};
}

}  // namespace

ApplicationShell::ApplicationShell(
    Resources::ResourceLoader& resourceLoader,
    QWidget* parent
    )
    : QMainWindow(parent)
    , m_resourceLoader(&resourceLoader)
    , m_contentWidget(new QWidget(this))
    , m_pageHost(new PageHost(resourceLoader, m_contentWidget))
    , m_navigationTree(new QTreeWidget(m_contentWidget))
    , m_backButton(new QPushButton(tr("Back"), m_contentWidget))
    , m_forwardButton(new QPushButton(tr("Forward"), m_contentWidget))
{
    setObjectName(QStringLiteral("classMngrNextApplicationShell"));
    setWindowTitle(tr("ClassMngr Next"));
    setMinimumSize(720, 480);
    resize(1024, 720);

    auto* const contentLayout = new QHBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    auto* const navigationPanel = new QWidget(m_contentWidget);
    navigationPanel->setObjectName(QStringLiteral("classMngrNextNavigationPanel"));
    navigationPanel->setMinimumWidth(190);
    navigationPanel->setMaximumWidth(270);
    auto* const navigationLayout = new QVBoxLayout(navigationPanel);
    navigationLayout->setContentsMargins(8, 8, 8, 8);
    navigationLayout->setSpacing(6);

    auto* const historyControls = new QWidget(navigationPanel);
    auto* const historyLayout = new QHBoxLayout(historyControls);
    historyLayout->setContentsMargins(0, 0, 0, 0);
    historyLayout->setSpacing(6);
    m_backButton->setObjectName(QStringLiteral("classMngrNextNavigationBack"));
    m_forwardButton->setObjectName(QStringLiteral("classMngrNextNavigationForward"));
    m_backButton->setEnabled(false);
    m_forwardButton->setEnabled(false);
    historyLayout->addWidget(m_backButton);
    historyLayout->addWidget(m_forwardButton);
    navigationLayout->addWidget(historyControls);

    m_navigationTree->setObjectName(QStringLiteral("classMngrNextNavigationTree"));
    m_navigationTree->setHeaderHidden(true);
    m_navigationTree->setSelectionMode(QAbstractItemView::SingleSelection);
    navigationLayout->addWidget(m_navigationTree, 1);

    contentLayout->addWidget(navigationPanel);
    contentLayout->addWidget(m_pageHost, 1);
    setCentralWidget(m_contentWidget);

    connect(m_backButton, &QPushButton::clicked, this, [this] {
        static_cast<void>(navigateBack());
    });
    connect(m_forwardButton, &QPushButton::clicked, this, [this] {
        static_cast<void>(navigateForward());
    });
    connect(m_navigationTree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* current, QTreeWidgetItem*) {
        if (!current
            || current->data(0, NodeKindRole).toInt()
                != static_cast<int>(NavigationNode::Kind::Page))
        {
            synchronizeNavigationSelection();
            return;
        }
        const auto pageId = PageId::fromString(
            current->data(0, PageIdRole).toString()
            );
        if (!pageId || !navigateTo(*pageId, NavigationCause::Selection))
        {
            synchronizeNavigationSelection();
        }
    });
}

void ApplicationShell::setCloseHandler(CloseHandler handler)
{
    m_closeHandler = std::move(handler);
}

void ApplicationShell::setNavigationGuard(NavigationGuard guard)
{
    m_navigationGuard = std::move(guard);
}

PageHost& ApplicationShell::pageHost() noexcept
{
    return *m_pageHost;
}

const PageHost& ApplicationShell::pageHost() const noexcept
{
    return *m_pageHost;
}

NavigationModel* ApplicationShell::navigationModel() noexcept
{
    return m_navigationModel ? &*m_navigationModel : nullptr;
}

const NavigationModel* ApplicationShell::navigationModel() const noexcept
{
    return m_navigationModel ? &*m_navigationModel : nullptr;
}

NavigationModelResult<void> ApplicationShell::configureNavigation(
    std::vector<NavigationNode> nodes,
    PageId initialPageWhenClosed,
    PageId initialPageWhenOpen,
    const Application::WorkspaceLifecycleState workspaceState,
    std::optional<QStringList> grantedPermissionIds
    )
{
    if (m_navigationModel.has_value())
    {
        return std::unexpected(shellError(
            NavigationModelErrorCode::InvalidNode,
            QStringLiteral("navigation is already configured")
            ));
    }

    auto model = NavigationModel::create(
        std::move(nodes),
        m_pageHost->pageDescriptors(),
        std::move(initialPageWhenClosed),
        std::move(initialPageWhenOpen),
        workspaceState,
        std::move(grantedPermissionIds)
        );
    if (!model)
    {
        return std::unexpected(model.error());
    }

    m_navigationModel.emplace(std::move(*model));
    rebuildNavigationTree();
    synchronizeHistoryControls();
    return {};
}

bool ApplicationShell::navigateTo(
    const PageId& pageId,
    const NavigationCause cause
    )
{
    if (!m_navigationModel
        || !m_navigationModel->canCommit(pageId, cause))
    {
        return false;
    }
    if (cause == NavigationCause::Selection
        && m_navigationModel->currentPageId() == std::optional<PageId>(pageId))
    {
        return true;
    }

    const NavigationGuardRequest request{
        .source = m_navigationModel->currentPageId(),
        .target = pageId,
        .cause = cause
    };
    if (m_navigationGuard)
    {
        try
        {
            if (m_navigationGuard(request) != NavigationGuardDecision::Allow)
            {
                return false;
            }
        }
        catch (...)
        {
            return false;
        }
    }

    const PageHostResult<void> activation = m_pageHost->activatePage(pageId.value());
    if (!activation)
    {
        return false;
    }

    m_navigationModel->commit(pageId, cause);
    synchronizeNavigationSelection();
    synchronizeHistoryControls();
    return true;
}

bool ApplicationShell::navigateBack()
{
    if (!m_navigationModel)
    {
        return false;
    }
    const std::optional<PageId> target = m_navigationModel->backTarget();
    return target && navigateTo(*target, NavigationCause::Back);
}

bool ApplicationShell::navigateForward()
{
    if (!m_navigationModel)
    {
        return false;
    }
    const std::optional<PageId> target = m_navigationModel->forwardTarget();
    return target && navigateTo(*target, NavigationCause::Forward);
}

void ApplicationShell::updateNavigationAccessContext(
    const Application::WorkspaceLifecycleState workspaceState,
    std::optional<QStringList> grantedPermissionIds
    )
{
    if (!m_navigationModel)
    {
        return;
    }
    m_navigationModel->updateAccessContext(
        workspaceState,
        std::move(grantedPermissionIds)
        );
    rebuildNavigationTree();
    synchronizeHistoryControls();
}

void ApplicationShell::rebuildNavigationTree()
{
    if (!m_navigationModel)
    {
        return;
    }

    const QSignalBlocker signalBlocker(m_navigationTree);
    m_navigationTree->clear();
    std::function<void(const NavigationNode&, QTreeWidgetItem*)> appendNode;
    appendNode = [this, &appendNode](const NavigationNode& node,
                                     QTreeWidgetItem* parent) {
        const bool isPage = node.kind == NavigationNode::Kind::Page;
        const QString label = isPage
            ? m_navigationModel->pageMetadata(*node.pageId)->label
            : node.label;
        auto* const item = parent
            ? new QTreeWidgetItem(parent, QStringList{label})
            : new QTreeWidgetItem(m_navigationTree, QStringList{label});
        item->setData(0, NodeKindRole, static_cast<int>(node.kind));
        item->setData(0, NodeIdRole, node.id);
        if (isPage)
        {
            item->setData(0, PageIdRole, node.pageId->value());
            const auto metadata = m_navigationModel->pageMetadata(*node.pageId);
            if (metadata)
            {
                const auto icon = navigationIconFor(*metadata);
                if (icon)
                {
                    item->setIcon(0, *icon);
                }
            }
        }
        else if (node.kind == NavigationNode::Kind::Group)
        {
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            for (const NavigationNode& child : node.children)
            {
                appendNode(child, item);
            }
        }
    };

    for (const NavigationNode& node : m_navigationModel->visibleNodes())
    {
        appendNode(node, nullptr);
    }
    synchronizeNavigationSelection();
}

void ApplicationShell::synchronizeNavigationSelection()
{
    if (!m_navigationModel)
    {
        return;
    }
    const QSignalBlocker signalBlocker(m_navigationTree);
    const std::optional<PageId> selectedPage = m_navigationModel->selectedPageId();
    if (!selectedPage)
    {
        m_navigationTree->setCurrentItem(nullptr);
        return;
    }

    QTreeWidgetItemIterator iterator(m_navigationTree);
    while (*iterator)
    {
        QTreeWidgetItem* const item = *iterator;
        if (item->data(0, NodeKindRole).toInt()
                == static_cast<int>(NavigationNode::Kind::Page)
            && item->data(0, PageIdRole).toString() == selectedPage->value())
        {
            for (QTreeWidgetItem* parent = item->parent(); parent != nullptr;
                 parent = parent->parent())
            {
                parent->setExpanded(true);
            }
            m_navigationTree->setCurrentItem(item);
            return;
        }
        ++iterator;
    }
    m_navigationTree->setCurrentItem(nullptr);
}

std::optional<QIcon> ApplicationShell::navigationIconFor(
    const NavigationPageMetadata& metadata
    )
{
    if (!metadata.iconResourceId)
    {
        return std::nullopt;
    }

    const QString resourceId = metadata.iconResourceId->value();
    const auto cached = m_navigationIcons.constFind(resourceId);
    if (cached != m_navigationIcons.cend())
    {
        return *cached;
    }

    auto scope = Resources::ResourceScope::create(
        Resources::ResourceScopeKind::Feature,
        QStringLiteral("navigation-icons")
        );
    if (!scope)
    {
        return std::nullopt;
    }
    auto request = Resources::ResourceRequest::create(
        *metadata.iconResourceId,
        std::move(*scope)
        );
    if (!request)
    {
        return std::nullopt;
    }
    auto image = m_resourceLoader->loadImage(*request);
    if (!image || image->get().isNull())
    {
        return std::nullopt;
    }

    QIcon icon(QPixmap::fromImage(image->get()));
    m_navigationIconImages.push_back(std::move(*image));
    m_navigationIcons.insert(resourceId, icon);
    return icon;
}

void ApplicationShell::synchronizeHistoryControls()
{
    const bool configured = m_navigationModel.has_value();
    m_backButton->setEnabled(configured && m_navigationModel->canGoBack());
    m_forwardButton->setEnabled(configured && m_navigationModel->canGoForward());
}

void ApplicationShell::closeEvent(QCloseEvent* event)
{
    QMainWindow::closeEvent(event);
    if (event->isAccepted() && m_closeHandler)
    {
        if (!m_closeHandler())
        {
            event->ignore();
        }
    }
}

}  // namespace ClassMngr::Next::Ui
