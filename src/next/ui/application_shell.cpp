#include "next/ui/application_shell.h"

#include "next/ui/document_catalog_navigation.h"
#include "next/ui/page_host.h"
#include "next/ui/navigation_font_policy.h"

#include <algorithm>
#include <QDir>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTimer>
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
constexpr int TeacherIdRole = Qt::UserRole + 3;
constexpr auto KoreanTeacherGroupId = "teachers_all_korean";

class NavigationTreeItemDelegate final : public QStyledItemDelegate
{
public:
    explicit NavigationTreeItemDelegate(QTreeWidget* tree)
        : QStyledItemDelegate(tree)
        , m_tree(tree)
        , m_timer(this)
    {
        if (m_tree && m_tree->viewport())
        {
            m_tree->viewport()->installEventFilter(this);
        }
        m_timer.setInterval(40);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this] {
            advanceMarquee();
        });
    }

    void setMarqueeEnabled(const bool enabled)
    {
        if (m_enabled == enabled)
        {
            return;
        }
        m_enabled = enabled;
        if (!enabled)
        {
            m_timer.stop();
            m_hoveredIndex = QPersistentModelIndex{};
        }
        if (m_tree && m_tree->viewport())
        {
            m_tree->viewport()->update();
        }
    }

    [[nodiscard]] QSize sizeHint(
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override
    {
        QSize result = QStyledItemDelegate::sizeHint(option, index);
        result.setHeight(std::max(
            result.height(),
            NavigationFontPolicy::MinimumRowHeightPixels
            ));
        return result;
    }

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override
    {
        if (!painter || !m_tree || !m_enabled || index != m_hoveredIndex
            || !isOverflowing(option, index))
        {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem background(option);
        initStyleOption(&background, index);
        const QString label = background.text;
        background.text.clear();
        QStyledItemDelegate::paint(painter, background, index);

        QStyleOptionViewItem textOption(option);
        initStyleOption(&textOption, index);
        const QRect textRect = m_tree->style()->subElementRect(
            QStyle::SE_ItemViewItemText, &textOption, m_tree);
        const QFontMetrics metrics(textOption.font);
        const int labelWidth = metrics.horizontalAdvance(label);
        constexpr int gap = 32;
        const int offset = m_offset % std::max(1, labelWidth + gap);
        const int baseline = textRect.y()
            + (textRect.height() - metrics.height()) / 2
            + metrics.ascent();
        const QPalette::ColorRole colorRole =
            option.state.testFlag(QStyle::State_Selected)
                ? QPalette::HighlightedText
                : QPalette::Text;
        painter->save();
        painter->setClipRect(textRect);
        painter->setFont(textOption.font);
        painter->setPen(textOption.palette.color(colorRole));
        painter->drawText(textRect.x() - offset, baseline,
            label + QStringLiteral("   ") + label);
        painter->restore();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (m_tree && watched == m_tree->viewport() && event)
        {
            if (event->type() == QEvent::MouseMove)
            {
                const auto* const mouse = static_cast<QMouseEvent*>(event);
                setHoveredIndex(m_tree->indexAt(mouse->position().toPoint()));
            }
            else if (event->type() == QEvent::Leave)
            {
                setHoveredIndex({});
            }
        }
        return QStyledItemDelegate::eventFilter(watched, event);
    }

private:
    [[nodiscard]] bool isOverflowing(
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const
    {
        if (!m_tree || !index.isValid())
        {
            return false;
        }
        const QString label = index.data(Qt::DisplayRole).toString();
        const QFontMetrics metrics(option.font);
        const QRect textRect = m_tree->style()->subElementRect(
            QStyle::SE_ItemViewItemText, &option, m_tree);
        return metrics.horizontalAdvance(label) > textRect.width();
    }

    void setHoveredIndex(const QModelIndex& index)
    {
        if (!m_enabled || index == m_hoveredIndex)
        {
            return;
        }
        const QPersistentModelIndex previous = m_hoveredIndex;
        m_hoveredIndex = index;
        m_offset = 0;
        updateTimer();
        if (m_tree && m_tree->viewport())
        {
            if (previous.isValid()) m_tree->viewport()->update(
                m_tree->visualRect(previous));
            if (m_hoveredIndex.isValid()) m_tree->viewport()->update(
                m_tree->visualRect(m_hoveredIndex));
        }
    }

    void updateTimer()
    {
        if (!m_enabled || !m_tree || !m_hoveredIndex.isValid())
        {
            m_timer.stop();
            return;
        }
        QStyleOptionViewItem option;
        option.initFrom(m_tree);
        option.rect = m_tree->visualRect(m_hoveredIndex);
        option.font = m_tree->font();
        if (isOverflowing(option, m_hoveredIndex))
        {
            if (!m_timer.isActive()) m_timer.start();
        }
        else
        {
            m_timer.stop();
            m_offset = 0;
        }
    }

    void advanceMarquee()
    {
        if (!m_enabled || !m_tree || !m_hoveredIndex.isValid())
        {
            m_timer.stop();
            return;
        }
        ++m_offset;
        m_tree->viewport()->update(m_tree->visualRect(m_hoveredIndex));
    }

    QTreeWidget* m_tree = nullptr;
    QTimer m_timer;
    QPersistentModelIndex m_hoveredIndex;
    int m_offset = 0;
    bool m_enabled = false;
};

class KoreanTeacherNavigationItem final : public QTreeWidgetItem
{
public:
    KoreanTeacherNavigationItem(
        QString label,
        Domain::TeacherId teacherId
        )
        : QTreeWidgetItem(QStringList{std::move(label)})
        , m_teacherId(std::move(teacherId))
    {
    }

    [[nodiscard]] const Domain::TeacherId& teacherId() const noexcept
    {
        return m_teacherId;
    }

private:
    Domain::TeacherId m_teacherId;
};

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
    Application::PreferencesModel& preferences,
    QWidget* parent
    )
    : QMainWindow(parent)
    , m_resourceLoader(&resourceLoader)
    , m_contentWidget(new QWidget(this))
    , m_pageHost(new PageHost(resourceLoader, preferences, m_contentWidget))
    , m_navigationTree(new QTreeWidget(m_contentWidget))
    , m_backButton(new QPushButton(
          QCoreApplication::translate("NextShell", "Back"), m_contentWidget))
    , m_forwardButton(new QPushButton(
          QCoreApplication::translate("NextShell", "Forward"), m_contentWidget))
    , m_notificationPresenter(*statusBar())
    , m_preferences(&preferences)
{
    statusBar()->setObjectName(QStringLiteral("classMngrNextNotificationStatusBar"));
    setObjectName(QStringLiteral("classMngrNextApplicationShell"));
    setWindowTitle(QCoreApplication::translate("NextShell", "ClassMngr Next"));
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
    m_navigationTree->setFont(
        NavigationFontPolicy::standardFont(
            m_navigationTree->font(),
            preferences.snapshot().fontSize()
            )
        );
    m_navigationTree->setIndentation(12);
    m_navigationTree->setUniformRowHeights(true);
    m_navigationTree->setItemDelegate(
        new NavigationTreeItemDelegate(m_navigationTree)
        );
    navigationLayout->addWidget(m_navigationTree, 1);

    contentLayout->addWidget(navigationPanel);
    contentLayout->addWidget(m_pageHost, 1);
    setCentralWidget(m_contentWidget);

    m_fileMenu = menuBar()->addMenu(
        QCoreApplication::translate("NextShell", "File")
        );
    m_fileMenu->setObjectName(QStringLiteral("classMngrNextFileMenu"));
    m_newTeacherProfileAction = m_fileMenu->addAction(
        QCoreApplication::translate("NextShell", "New Teacher Profile…")
        );
    m_newTeacherProfileAction->setObjectName(
        QStringLiteral("classMngrNextNewTeacherProfile")
        );
    m_newTeacherProfileAction->setShortcut(QKeySequence::New);
    m_openTeacherProfileAction = m_fileMenu->addAction(
        QCoreApplication::translate("NextShell", "Open Teacher Profile…")
        );
    m_openTeacherProfileAction->setObjectName(
        QStringLiteral("classMngrNextOpenTeacherProfile")
        );
    m_openTeacherProfileAction->setShortcut(QKeySequence::Open);
    m_recoverInterruptedTeacherProfileAction = m_fileMenu->addAction(
        QCoreApplication::translate(
            "NextShell",
            "Recover Interrupted Teacher Profile…"
            )
        );
    m_recoverInterruptedTeacherProfileAction->setObjectName(
        QStringLiteral("classMngrNextRecoverInterruptedTeacherProfile")
        );
    m_recentWorkspaceMenu = m_fileMenu->addMenu(
        QCoreApplication::translate("NextShell", "Recent Files")
        );
    m_recentWorkspaceMenu->setObjectName(
        QStringLiteral("classMngrNextRecentWorkspaceMenu")
        );
    m_fileMenu->addSeparator();
    m_saveWorkspaceAction = m_fileMenu->addAction(
        QCoreApplication::translate("NextShell", "Save")
        );
    m_saveWorkspaceAction->setObjectName(
        QStringLiteral("classMngrNextSaveWorkspace")
        );
    m_saveWorkspaceAction->setShortcut(QKeySequence::Save);
    m_saveWorkspaceAction->setEnabled(false);
    m_saveWorkspaceAsAction = m_fileMenu->addAction(
        QCoreApplication::translate("NextShell", "Save As…")
        );
    m_saveWorkspaceAsAction->setObjectName(
        QStringLiteral("classMngrNextSaveWorkspaceAs")
        );
    m_saveWorkspaceAsAction->setShortcut(QKeySequence::SaveAs);
    m_saveWorkspaceAsAction->setEnabled(false);
    m_exportWorkspaceAction = m_fileMenu->addAction(
        QCoreApplication::translate("NextShell", "Export Teacher Profile…")
        );
    m_exportWorkspaceAction->setObjectName(
        QStringLiteral("classMngrNextExportWorkspace")
        );
    m_exportWorkspaceAction->setEnabled(false);
    m_closeWorkspaceAction = m_fileMenu->addAction(
        QCoreApplication::translate("NextShell", "Close Teacher Profile")
        );
    m_closeWorkspaceAction->setObjectName(
        QStringLiteral("classMngrNextCloseWorkspace")
        );
    m_closeWorkspaceAction->setShortcut(QKeySequence::Close);
    m_closeWorkspaceAction->setEnabled(false);

    m_editMenu = menuBar()->addMenu(
        QCoreApplication::translate("NextShell", "Edit")
        );
    m_editMenu->setObjectName(QStringLiteral("classMngrNextEditMenu"));
    m_preferencesAction = m_editMenu->addAction(
        QCoreApplication::translate("NextShell", "Preferences…")
        );
    m_preferencesAction->setObjectName(QStringLiteral("classMngrNextPreferences"));

    m_teachersMenu = menuBar()->addMenu(
        QCoreApplication::translate("NextShell", "Teachers")
        );
    m_importTeachersAction = m_teachersMenu->addAction(
        QCoreApplication::translate("NextShell", "Import Teachers…")
        );
    m_importTeachersAction->setObjectName(
        QStringLiteral("classMngrNextImportTeachers")
        );
    m_importTeachersAction->setEnabled(false);
    m_upcomingBirthdaysAction = m_teachersMenu->addAction(
        QCoreApplication::translate("NextShell", "Upcoming Birthdays…")
        );
    m_upcomingBirthdaysAction->setObjectName(
        QStringLiteral("classMngrNextUpcomingBirthdays")
        );
    m_upcomingBirthdaysAction->setEnabled(false);

    connect(m_backButton, &QPushButton::clicked, this, [this] {
        static_cast<void>(navigateBack());
    });
    connect(m_forwardButton, &QPushButton::clicked, this, [this] {
        static_cast<void>(navigateForward());
    });
    connect(m_newTeacherProfileAction, &QAction::triggered, this, [this] {
        if (m_newTeacherProfileHandler)
        {
            m_newTeacherProfileHandler();
        }
    });
    connect(m_openTeacherProfileAction, &QAction::triggered, this, [this] {
        if (m_openTeacherProfileHandler)
        {
            m_openTeacherProfileHandler();
        }
    });
    connect(
        m_recoverInterruptedTeacherProfileAction,
        &QAction::triggered,
        this,
        [this] {
            if (m_recoverInterruptedTeacherProfileHandler)
            {
                m_recoverInterruptedTeacherProfileHandler();
            }
        }
        );
    connect(m_recentWorkspaceMenu, &QMenu::aboutToShow, this, [this] {
        refreshRecentWorkspaceMenu();
    });
    connect(m_saveWorkspaceAction, &QAction::triggered, this, [this] {
        if (m_saveWorkspaceHandler)
        {
            m_saveWorkspaceHandler();
        }
    });
    connect(m_saveWorkspaceAsAction, &QAction::triggered, this, [this] {
        if (m_saveWorkspaceAsHandler)
        {
            m_saveWorkspaceAsHandler();
        }
    });
    connect(m_exportWorkspaceAction, &QAction::triggered, this, [this] {
        if (m_exportWorkspaceHandler)
        {
            m_exportWorkspaceHandler();
        }
    });
    connect(m_closeWorkspaceAction, &QAction::triggered, this, [this] {
        if (m_closeWorkspaceHandler)
        {
            m_closeWorkspaceHandler();
        }
    });
    connect(m_preferencesAction, &QAction::triggered, this, [this] {
        if (m_preferencesHandler)
        {
            m_preferencesHandler();
        }
    });
    connect(m_importTeachersAction, &QAction::triggered, this, [this] {
        if (m_importTeachersHandler)
        {
            m_importTeachersHandler();
        }
    });
    connect(m_upcomingBirthdaysAction, &QAction::triggered, this, [this] {
        if (m_upcomingBirthdaysHandler)
        {
            m_upcomingBirthdaysHandler();
        }
    });
    connect(m_navigationTree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* current, QTreeWidgetItem*) {
        if (const auto documentId =
                DocumentCatalogNavigation::documentIdFor(current))
        {
            if (m_documentCatalogSelectionHandler)
            {
                m_documentCatalogSelectionHandler(*documentId);
            }
            return;
        }
        if (current)
        {
            const QVariant catalogKindData = current->data(
                0,
                static_cast<int>(DocumentCatalogItemRole::Kind)
                );
            if (catalogKindData.isValid()
                && (catalogKindData.toInt()
                        == static_cast<int>(DocumentCatalogItemKind::Root)
                    || catalogKindData.toInt()
                        == static_cast<int>(DocumentCatalogItemKind::Folder)))
            {
                if (m_documentCatalogSelectionClearedHandler)
                {
                    m_documentCatalogSelectionClearedHandler();
                }
                return;
            }
        }
        if (auto* const teacherItem =
                dynamic_cast<KoreanTeacherNavigationItem*>(current))
        {
            const auto previousTeacherId = m_lastOpenedKoreanTeacherId;
            m_lastOpenedKoreanTeacherId = teacherItem->teacherId();
            if (!m_koreanTeacherNavigationActions.openTeacherProfile
                || !m_koreanTeacherNavigationActions.openTeacherProfile(
                    teacherItem->teacherId()
                    ))
            {
                m_lastOpenedKoreanTeacherId = previousTeacherId;
                synchronizeNavigationSelection();
            }
            return;
        }
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
    m_navigationTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_navigationTree, &QWidget::customContextMenuRequested, this,
            &ApplicationShell::showKoreanTeacherContextMenu);

    applyPreferences(preferences.snapshot());
    const auto observer = preferences.subscribe(
        [this](Application::UserPreferencesSnapshot snapshot) {
            applyPreferences(snapshot);
        }
        );
    if (observer)
    {
        m_preferencesObserverId = observer.value();
    }
}

ApplicationShell::~ApplicationShell()
{
    if (m_preferences && m_preferencesObserverId)
    {
        static_cast<void>(m_preferences->unsubscribe(*m_preferencesObserverId));
    }
}

void ApplicationShell::setCloseHandler(CloseHandler handler)
{
    m_closeHandler = std::move(handler);
}

void ApplicationShell::setNewTeacherProfileHandler(std::function<void()> handler)
{
    m_newTeacherProfileHandler = std::move(handler);
}

void ApplicationShell::setOpenTeacherProfileHandler(std::function<void()> handler)
{
    m_openTeacherProfileHandler = std::move(handler);
}

void ApplicationShell::setRecoverInterruptedTeacherProfileHandler(
    std::function<void()> handler
    )
{
    m_recoverInterruptedTeacherProfileHandler = std::move(handler);
}

void ApplicationShell::setSaveWorkspaceHandler(std::function<void()> handler)
{
    m_saveWorkspaceHandler = std::move(handler);
}

void ApplicationShell::setSaveWorkspaceAsHandler(std::function<void()> handler)
{
    m_saveWorkspaceAsHandler = std::move(handler);
}

void ApplicationShell::setExportWorkspaceHandler(std::function<void()> handler)
{
    m_exportWorkspaceHandler = std::move(handler);
}

void ApplicationShell::setImportTeachersHandler(std::function<void()> handler)
{
    m_importTeachersHandler = std::move(handler);
    synchronizeTeacherImportAction();
}

void ApplicationShell::setUpcomingBirthdaysHandler(
    std::function<void()> handler
    )
{
    m_upcomingBirthdaysHandler = std::move(handler);
    if (m_upcomingBirthdaysAction)
    {
        m_upcomingBirthdaysAction->setEnabled(m_workspaceIsOpen);
    }
}

void ApplicationShell::setCloseWorkspaceHandler(std::function<void()> handler)
{
    m_closeWorkspaceHandler = std::move(handler);
}

void ApplicationShell::setPreferencesHandler(std::function<void()> handler)
{
    m_preferencesHandler = std::move(handler);
}

void ApplicationShell::setRecentWorkspaceMenuHandlers(
    RecentWorkspaceHistoryLoader historyLoader,
    RecentWorkspaceOpenHandler openHandler,
    std::function<void()> clearHandler
    )
{
    m_recentWorkspaceHistoryLoader = std::move(historyLoader);
    m_recentWorkspaceOpenHandler = std::move(openHandler);
    m_clearRecentWorkspaceHistoryHandler = std::move(clearHandler);
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

Application::NotificationPort& ApplicationShell::notificationPort() noexcept
{
    return m_notificationPresenter;
}

QMenu* ApplicationShell::recentWorkspaceMenu() noexcept
{
    return m_recentWorkspaceMenu;
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
    synchronizeWorkspaceActions(workspaceState);
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
    synchronizeTeacherImportAction();
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
    synchronizeWorkspaceActions(workspaceState);
}

void ApplicationShell::setKoreanTeacherNavigationActions(
    KoreanTeacherNavigationActions actions
    )
{
    m_koreanTeacherNavigationActions = std::move(actions);
}

void ApplicationShell::setKoreanTeacherNavigationRows(
    std::vector<KoreanTeacherNavigationRow> rows
    )
{
    if (m_koreanTeacherNavigationRows == rows)
    {
        return;
    }
    m_koreanTeacherNavigationRows = std::move(rows);
    if (m_lastOpenedKoreanTeacherId
        && std::none_of(
            m_koreanTeacherNavigationRows.begin(),
            m_koreanTeacherNavigationRows.end(),
            [this](const KoreanTeacherNavigationRow& row) {
                return row.teacherId == *m_lastOpenedKoreanTeacherId;
            }
            ))
    {
        m_lastOpenedKoreanTeacherId.reset();
    }
    rebuildNavigationTree();
}

void ApplicationShell::setDocumentCatalogProjection(
    Application::DocumentCatalogProjection projection
    )
{
    m_documentCatalogProjection.emplace(std::move(projection));
    rebuildNavigationTree();
}

void ApplicationShell::setDocumentCatalogProjectionLoader(
    DocumentCatalogProjectionLoader loader
    )
{
    m_documentCatalogProjectionLoader = std::move(loader);
}

Domain::Result<void> ApplicationShell::refreshDocumentCatalogProjection(
    const QString& localeName
    )
{
    if (!m_documentCatalogProjectionLoader)
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Document catalog locale refresh is unavailable.",
            .recoverable = false
        });
    }

    try
    {
        auto projection = m_documentCatalogProjectionLoader(localeName);
        if (!projection)
        {
            return Domain::Result<void>::failure(projection.error());
        }
        setDocumentCatalogProjection(std::move(projection.value()));
        return Domain::Result<void>::success();
    }
    catch (const std::exception&)
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Document catalog locale refresh failed.",
            .recoverable = false
        });
    }
    catch (...)
    {
        return Domain::Result<void>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Document catalog locale refresh failed.",
            .recoverable = false
        });
    }
}

const Application::DocumentCatalogProjection*
ApplicationShell::documentCatalogProjection() const noexcept
{
    return m_documentCatalogProjection
        ? &*m_documentCatalogProjection
        : nullptr;
}

void ApplicationShell::setDocumentCatalogSelectionHandler(
    std::function<void(const Domain::DocumentId&)> handler
    )
{
    m_documentCatalogSelectionHandler = std::move(handler);
}

void ApplicationShell::setDocumentCatalogSelectionClearedHandler(
    std::function<void()> handler
    )
{
    m_documentCatalogSelectionClearedHandler = std::move(handler);
}

void ApplicationShell::selectKoreanTeacherNavigationRow(
    const Domain::TeacherId& teacherId
    )
{
    m_lastOpenedKoreanTeacherId = teacherId;
    synchronizeNavigationSelection();
}

void ApplicationShell::synchronizeWorkspaceActions(
    const Application::WorkspaceLifecycleState workspaceState
    ) noexcept
{
    const bool open = workspaceState == Application::WorkspaceLifecycleState::Open;
    m_workspaceIsOpen = open;
    if (m_saveWorkspaceAction)
    {
        m_saveWorkspaceAction->setEnabled(open);
    }
    if (m_saveWorkspaceAsAction)
    {
        m_saveWorkspaceAsAction->setEnabled(open);
    }
    if (m_exportWorkspaceAction)
    {
        m_exportWorkspaceAction->setEnabled(open);
    }
    if (m_closeWorkspaceAction)
    {
        m_closeWorkspaceAction->setEnabled(open);
    }
    if (m_upcomingBirthdaysAction)
    {
        m_upcomingBirthdaysAction->setEnabled(open);
    }
    synchronizeTeacherImportAction();
}

void ApplicationShell::synchronizeTeacherImportAction() noexcept
{
    if (!m_importTeachersAction)
    {
        return;
    }
    const bool setupPageActive = m_pageHost
        && m_pageHost->activePageKey()
            == std::optional<QString>(QStringLiteral("initial_setup"));
    m_importTeachersAction->setEnabled(
        m_workspaceIsOpen && !setupPageActive
        );
}

void ApplicationShell::rebuildNavigationTree()
{
    const std::optional<Domain::DocumentId> selectedDocumentId =
        DocumentCatalogNavigation::documentIdFor(
            m_navigationTree->currentItem()
            );
    const QSignalBlocker signalBlocker(m_navigationTree);
    m_navigationTree->clear();
    if (m_navigationModel)
    {
        std::function<void(const NavigationNode&, QTreeWidgetItem*)> appendNode;
        appendNode = [this, &appendNode](const NavigationNode& node,
                                         QTreeWidgetItem* parent) {
            const bool isPage = node.kind == NavigationNode::Kind::Page;
            if (isPage && !node.displayInSidebar)
            {
                return;
            }
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
                if (node.id == QString::fromLatin1(KoreanTeacherGroupId))
                {
                    for (const KoreanTeacherNavigationRow& teacher :
                         m_koreanTeacherNavigationRows)
                    {
                        auto* const teacherItem = new KoreanTeacherNavigationItem(
                            teacher.label,
                            teacher.teacherId
                            );
                        teacherItem->setData(
                            0,
                            TeacherIdRole,
                            QString::fromStdString(teacher.teacherId.value())
                            );
                        item->addChild(teacherItem);
                    }
                }
            }
        };

        for (const NavigationNode& node : m_navigationModel->visibleNodes())
        {
            appendNode(node, nullptr);
        }
    }

    if (m_documentCatalogProjection)
    {
        DocumentCatalogNavigation::append(
            *m_navigationTree,
            *m_documentCatalogProjection
            );
    }

    if (selectedDocumentId)
    {
        QTreeWidgetItemIterator iterator(m_navigationTree);
        while (*iterator)
        {
            const auto candidate =
                DocumentCatalogNavigation::documentIdFor(*iterator);
            if (candidate && *candidate == *selectedDocumentId)
            {
                m_navigationTree->setCurrentItem(*iterator);
                updateNavigationTooltips(m_preferences
                    && m_preferences->snapshot().sidebarTooltipsEnabled());
                return;
            }
            ++iterator;
        }
    }
    synchronizeNavigationSelection();
    updateNavigationTooltips(m_preferences
        && m_preferences->snapshot().sidebarTooltipsEnabled());
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
    if (m_lastOpenedKoreanTeacherId)
    {
        QTreeWidgetItemIterator teacherIterator(m_navigationTree);
        while (*teacherIterator)
        {
            auto* const teacherItem =
                dynamic_cast<KoreanTeacherNavigationItem*>(*teacherIterator);
            if (teacherItem
                && teacherItem->teacherId() == *m_lastOpenedKoreanTeacherId)
            {
                m_navigationTree->setCurrentItem(teacherItem);
                return;
            }
            ++teacherIterator;
        }
    }
    m_navigationTree->setCurrentItem(nullptr);
}

void ApplicationShell::showKoreanTeacherContextMenu(
    const QPoint& position
    )
{
    QTreeWidgetItem* const contextItem = m_navigationTree->itemAt(position);
    auto* const contextTeacher =
        dynamic_cast<KoreanTeacherNavigationItem*>(contextItem);
    const bool koreanTeacherGroup = contextItem
        && contextItem->data(0, NodeIdRole).toString()
            == QString::fromLatin1(KoreanTeacherGroupId);
    if (!contextTeacher && !koreanTeacherGroup)
    {
        return;
    }

    std::optional<Domain::TeacherId> teacherToDelete;
    if (contextTeacher)
    {
        teacherToDelete = contextTeacher->teacherId();
    }
    else if (auto* const selectedTeacher =
                 dynamic_cast<KoreanTeacherNavigationItem*>(
                     m_navigationTree->currentItem()
                     ))
    {
        teacherToDelete = selectedTeacher->teacherId();
    }

    QMenu menu(this);
    QAction* const createAction = menu.addAction(
        QCoreApplication::translate("KoreanTeacherListPage", "New Teacher")
        );
    createAction->setObjectName(QStringLiteral("koreanTeacherNewContextAction"));
    QAction* const deleteAction = menu.addAction(
        QCoreApplication::translate("KoreanTeacherListPage", "Delete Teacher")
        );
    deleteAction->setObjectName(QStringLiteral("koreanTeacherDeleteContextAction"));
    deleteAction->setEnabled(
        teacherToDelete.has_value() || !m_koreanTeacherNavigationRows.empty()
        );

    QAction* const chosen = menu.exec(
        m_navigationTree->viewport()->mapToGlobal(position)
        );
    if (chosen == createAction && m_koreanTeacherNavigationActions.createTeacher)
    {
        m_koreanTeacherNavigationActions.createTeacher();
    }
    else if (chosen == deleteAction
             && m_koreanTeacherNavigationActions.deleteTeacher)
    {
        m_koreanTeacherNavigationActions.deleteTeacher(teacherToDelete);
    }
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
    const NavigationHistoryActionState state = m_navigationModel
        ? m_navigationModel->historyActionState()
        : NavigationHistoryActionState{};
    m_backButton->setEnabled(state.backEnabled);
    m_forwardButton->setEnabled(state.forwardEnabled);
}

void ApplicationShell::refreshRecentWorkspaceMenu()
{
    if (!m_recentWorkspaceMenu)
    {
        return;
    }

    m_recentWorkspaceMenu->clear();
    const Application::RecentWorkspaceHistory history =
        m_recentWorkspaceHistoryLoader
            ? m_recentWorkspaceHistoryLoader()
            : Application::RecentWorkspaceHistory{};

    if (history.paths.empty())
    {
        QAction* const emptyAction = m_recentWorkspaceMenu->addAction(
            QCoreApplication::translate("NextShell", "(No Recent Files)")
            );
        emptyAction->setEnabled(false);
        return;
    }

    for (std::size_t index = 0; index < history.paths.size(); ++index)
    {
        const Application::RecentWorkspacePath path = history.paths[index];
        const QString storedPath = QString::fromUtf8(
            path.value().data(),
            static_cast<qsizetype>(path.value().size())
            );
        const QString nativePath = QDir::toNativeSeparators(storedPath);
        QString displayPath = nativePath;
        displayPath.replace(QLatin1Char('&'), QStringLiteral("&&"));

        QAction* const pathAction = m_recentWorkspaceMenu->addAction(
            QCoreApplication::translate("NextShell", "&%1 %2")
                .arg(static_cast<qulonglong>(index + 1))
                .arg(displayPath)
            );
        pathAction->setToolTip(nativePath);
        pathAction->setData(storedPath);
        connect(
            pathAction,
            &QAction::triggered,
            this,
            [this, path] {
                if (m_recentWorkspaceOpenHandler)
                {
                    m_recentWorkspaceOpenHandler(path);
                }
            }
            );
    }

    m_recentWorkspaceMenu->addSeparator();
    QAction* const clearAction = m_recentWorkspaceMenu->addAction(
        QCoreApplication::translate("NextShell", "Clear Recent Files")
        );
    connect(
        clearAction,
        &QAction::triggered,
        this,
        [this] {
            if (m_clearRecentWorkspaceHistoryHandler)
            {
                m_clearRecentWorkspaceHistoryHandler();
            }
        }
        );
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

void ApplicationShell::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
}

void ApplicationShell::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (m_navigationTree && m_preferences)
    {
        updateNavigationTooltips(
            m_preferences->snapshot().sidebarTooltipsEnabled());
    }
}

void ApplicationShell::retranslateUi()
{
    setWindowTitle(QCoreApplication::translate("NextShell", "ClassMngr Next"));
    m_backButton->setText(QCoreApplication::translate("NextShell", "Back"));
    m_forwardButton->setText(QCoreApplication::translate("NextShell", "Forward"));
    if (m_fileMenu)
    {
        m_fileMenu->setTitle(QCoreApplication::translate("NextShell", "File"));
    }
    if (m_editMenu)
    {
        m_editMenu->setTitle(QCoreApplication::translate("NextShell", "Edit"));
    }
    if (m_teachersMenu)
    {
        m_teachersMenu->setTitle(QCoreApplication::translate("NextShell", "Teachers"));
    }
    const auto setActionText = [](QAction* action, const char* source) {
        if (action)
        {
            action->setText(QCoreApplication::translate("NextShell", source));
        }
    };
    setActionText(m_newTeacherProfileAction, "New Teacher Profile…");
    setActionText(m_openTeacherProfileAction, "Open Teacher Profile…");
    setActionText(m_recoverInterruptedTeacherProfileAction,
                  "Recover Interrupted Teacher Profile…");
    setActionText(m_saveWorkspaceAction, "Save");
    setActionText(m_saveWorkspaceAsAction, "Save As…");
    setActionText(m_exportWorkspaceAction, "Export Teacher Profile…");
    setActionText(m_closeWorkspaceAction, "Close Teacher Profile");
    setActionText(m_preferencesAction, "Preferences…");
    setActionText(m_importTeachersAction, "Import Teachers…");
    setActionText(m_upcomingBirthdaysAction, "Upcoming Birthdays…");
    if (m_recentWorkspaceMenu)
    {
        m_recentWorkspaceMenu->setTitle(
            QCoreApplication::translate("NextShell", "Recent Files"));
    }
    if (m_navigationModel)
    {
        m_navigationModel->retranslate();
        rebuildNavigationTree();
    }
}

void ApplicationShell::applyPreferences(
    const Application::UserPreferencesSnapshot& snapshot
    )
{
    m_navigationTree->setFont(
        NavigationFontPolicy::standardFont(m_navigationTree->font(), snapshot.fontSize()));
    m_navigationTree->setProperty(
        "sidebarAnimationEnabled", snapshot.sidebarMarqueeEnabled());
    if (auto* const delegate = dynamic_cast<NavigationTreeItemDelegate*>(
            m_navigationTree->itemDelegate()))
    {
        delegate->setMarqueeEnabled(snapshot.sidebarMarqueeEnabled());
    }
    updateNavigationTooltips(snapshot.sidebarTooltipsEnabled());
}

void ApplicationShell::updateNavigationTooltips(const bool enabled)
{
    QTreeWidgetItemIterator iterator(m_navigationTree);
    const QFontMetrics metrics(m_navigationTree->font());
    while (*iterator)
    {
        QTreeWidgetItem* const item = *iterator;
        int depth = 0;
        for (QTreeWidgetItem* parent = item->parent(); parent; parent = parent->parent())
        {
            ++depth;
        }
        const int indentation = depth * m_navigationTree->indentation();
        const int available = std::max(
            0, m_navigationTree->viewport()->width() - indentation - 44);
        const QString label = item->text(0);
        item->setToolTip(0, enabled && metrics.horizontalAdvance(label) > available
            ? label : QString{});
        ++iterator;
    }
}

}  // namespace ClassMngr::Next::Ui
