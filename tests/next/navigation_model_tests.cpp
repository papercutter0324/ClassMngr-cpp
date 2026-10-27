#include "preferences_test_ports.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/application_shell.h"
#include "next/ui/navigation_model.h"
#include "next/ui/page_host.h"

#include <QPushButton>
#include <QAction>
#include <QKeySequence>
#include <QMenu>
#include <QPointer>
#include <QStatusBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QtTest/QtTest>

#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

Ui::PageId pageId(const QString& value)
{
    return *Ui::PageId::fromString(value);
}

struct PageProbe final
{
    QString id;
    int factoryCalls = 0;
    int activationCalls = 0;
    bool failActivation = false;
};

class ProbePage final : public Ui::Page
{
public:
    ProbePage(const Ui::PageContext& context, PageProbe& probe)
        : Ui::Page(context)
        , m_probe(probe)
    {
    }

    Ui::PageOperationResult<void> onActivating() override
    {
        ++m_probe.activationCalls;
        if (m_probe.failActivation)
        {
            return std::unexpected(Ui::PageOperationError{
                QStringLiteral("probe activation failure")
            });
        }
        return {};
    }

private:
    PageProbe& m_probe;
};

Ui::PageDescriptor descriptorFor(
    const QString& id,
    const QString& label,
    QStringList requiredPermissions = {},
    std::optional<Resources::ResourceId> iconResourceId = std::nullopt,
    PageProbe* probe = nullptr
    )
{
    return Ui::PageDescriptor{
        .id = pageId(id),
        .label = label,
        .iconResourceId = std::move(iconResourceId),
        .requiredPermissions = std::move(requiredPermissions),
        .factory = [probe](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            if (!probe)
            {
                return std::unexpected(Ui::PageOperationError{
                    QStringLiteral("factory is not expected to run in this test")
                });
            }
            ++probe->factoryCalls;
            std::unique_ptr<Ui::Page> page = std::make_unique<ProbePage>(context, *probe);
            return page;
        },
        .resourcePolicy = Ui::PageResourcePolicy::PageScoped,
        .initialDataPolicy = Ui::PageInitialDataPolicy::None
    };
}

Ui::NavigationModel modelFor(
    std::vector<Ui::NavigationNode> nodes,
    std::vector<Ui::PageDescriptor> descriptors,
    const Ui::PageId& closedInitial,
    const Ui::PageId& openInitial,
    const Application::WorkspaceLifecycleState workspaceState,
    std::optional<QStringList> permissionIds = std::nullopt
    )
{
    auto result = Ui::NavigationModel::create(
        std::move(nodes),
        std::move(descriptors),
        closedInitial,
        openInitial,
        workspaceState,
        std::move(permissionIds)
        );
    if (!result)
    {
        throw std::runtime_error(result.error().message.toStdString());
    }
    return std::move(*result);
}

std::vector<Ui::PageDescriptor> commonDescriptors(
    PageProbe* campus = nullptr,
    PageProbe* workspace = nullptr,
    PageProbe* secondWorkspace = nullptr,
    PageProbe* failing = nullptr,
    std::optional<Resources::ResourceId> campusIcon = std::nullopt
    )
{
    return {
        descriptorFor(QStringLiteral("campus_information"), QStringLiteral("Campus Info"),
                      {}, std::move(campusIcon), campus),
        descriptorFor(QStringLiteral("my_workspace"), QStringLiteral("My Workspace"), {}, {}, workspace),
        descriptorFor(QStringLiteral("classes"), QStringLiteral("Classes"), {}, {}, secondWorkspace),
        descriptorFor(QStringLiteral("failing"), QStringLiteral("Failing Page"), {}, {}, failing)
    };
}

std::vector<Ui::NavigationNode> commonTree()
{
    using Ui::NavigationNode;
    using Ui::NavigationWorkspaceAvailability;
    constexpr auto workspace = NavigationWorkspaceAvailability::WorkspaceOpen;
    return {
        NavigationNode::page(pageId(QStringLiteral("my_workspace")), workspace),
        NavigationNode::group(
            QStringLiteral("workspace_pages"),
            QStringLiteral("Workspace Pages"),
            {
                NavigationNode::page(pageId(QStringLiteral("classes")), workspace),
                NavigationNode::page(pageId(QStringLiteral("failing")), workspace)
            },
            workspace
            ),
        NavigationNode::group(
            QStringLiteral("useful_links"),
            QStringLiteral("Useful Links"),
            {
                NavigationNode::placeholder(QStringLiteral("useful_dropbox"),
                                            QStringLiteral("Dropbox")),
                NavigationNode::placeholder(QStringLiteral("useful_vacation_calendar"),
                                            QStringLiteral("Vacation Calendar")),
                NavigationNode::placeholder(QStringLiteral("useful_yearly_calendar"),
                                            QStringLiteral("Yearly Calendar")),
                NavigationNode::placeholder(QStringLiteral("useful_training_website"),
                                            QStringLiteral("Training Website")),
                NavigationNode::placeholder(QStringLiteral("useful_net_website"),
                                            QStringLiteral("NET Website")),
                NavigationNode::placeholder(QStringLiteral("useful_lms_website"),
                                            QStringLiteral("LMS Website")),
                NavigationNode::placeholder(QStringLiteral("useful_highlights_library"),
                                            QStringLiteral("Highlights Library"))
            }
            ),
        NavigationNode::group(
            QStringLiteral("campus_directory"),
            QStringLiteral("Campus Directory"),
            {
                NavigationNode::page(pageId(QStringLiteral("campus_information")))
            }
            )
    };
}

QString catalogPath()
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG);
}

QString resourceRoot()
{
    return QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
}

QTreeWidgetItem* itemByDataRole(
    QTreeWidget* tree,
    const int role,
    const QString& value
    )
{
    QTreeWidgetItemIterator iterator(tree);
    while (*iterator)
    {
        if ((*iterator)->data(0, role).toString() == value)
        {
            return *iterator;
        }
        ++iterator;
    }
    return nullptr;
}

}  // namespace

class NavigationModelTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void orderedTreeUsesDescriptorMetadataAndValidatesReferences();
    void workspaceAndPermissionsFilterRoutesAndChooseInitialPages();
    void historyActionStateRequiresAvailableAdjacentRoutes();
    void shellCommitsHistoryOnlyAfterGuardAndActivationSuccess();
    void shellHistoryControlsTrackAccessRevocation();
    void shellExposesNewTeacherProfileAction();
    void shellExposesOpenTeacherProfileAction();
    void shellExposesInterruptedProfileRecoveryAction();
    void shellOwnsTransientNotificationPortAndKeepsIdleStatusBarHidden();

private:
    Test::MemoryPreferences m_preferences;
    std::unique_ptr<Resources::ResourceLoader> m_loader;
};

void NavigationModelTests::initTestCase()
{
    auto catalog = Resources::ResourceCatalog::load(catalogPath(), resourceRoot());
    QVERIFY2(catalog.has_value(), catalog ? "" : qPrintable(catalog.error().message));
    m_loader = std::make_unique<Resources::ResourceLoader>(std::move(*catalog));
}

void NavigationModelTests::orderedTreeUsesDescriptorMetadataAndValidatesReferences()
{
    const auto icon = Resources::ResourceId::fromString(
        QStringLiteral("document.body:Book Reports/Book Report Grading Rubric.pdf")
        );
    QVERIFY(icon.has_value());
    auto model = modelFor(
        {
            Ui::NavigationNode::group(
                QStringLiteral("campus"),
                QStringLiteral("Campus Directory"),
                {
                    Ui::NavigationNode::page(pageId(QStringLiteral("campus_information"))),
                    Ui::NavigationNode::group(
                        QStringLiteral("details"),
                        QStringLiteral("Details"),
                        {
                            Ui::NavigationNode::page(pageId(QStringLiteral("classes")))
                        }
                        )
                }
                )
        },
        {
            descriptorFor(QStringLiteral("campus_information"), QStringLiteral("Information"),
                          {QStringLiteral("directory.view")}, *icon),
            descriptorFor(QStringLiteral("classes"), QStringLiteral("Classes"))
        },
        pageId(QStringLiteral("campus_information")),
        pageId(QStringLiteral("campus_information")),
        Application::WorkspaceLifecycleState::Closed,
        QStringList{QStringLiteral("directory.view")}
        );

    const std::vector<Ui::NavigationNode>& rootNodes = model.nodes();
    QCOMPARE(rootNodes.size(), std::size_t(1));
    QCOMPARE(rootNodes.front().id, QStringLiteral("campus"));
    QCOMPARE(rootNodes.front().label, QStringLiteral("Campus Directory"));
    QCOMPARE(rootNodes.front().children.size(), std::size_t(2));
    QCOMPARE(rootNodes.front().children.front().pageId->value(), QStringLiteral("campus_information"));
    QCOMPARE(rootNodes.front().children.back().id, QStringLiteral("details"));
    QCOMPARE(rootNodes.front().children.back().children.front().pageId->value(), QStringLiteral("classes"));

    const auto information = model.pageMetadata(pageId(QStringLiteral("campus_information")));
    QVERIFY(information.has_value());
    QCOMPARE(information->label, QStringLiteral("Information"));
    QCOMPARE(information->iconResourceId, icon);
    QCOMPARE(information->requiredPermissions, QStringList{QStringLiteral("directory.view")});
    QCOMPARE(model.visibleNodes().front().children.front().label, QString{});

    auto unknownPage = Ui::NavigationModel::create(
        {Ui::NavigationNode::page(pageId(QStringLiteral("missing")))},
        {descriptorFor(QStringLiteral("campus_information"), QStringLiteral("Information"))},
        pageId(QStringLiteral("missing")),
        pageId(QStringLiteral("missing")),
        Application::WorkspaceLifecycleState::Closed
        );
    QVERIFY(!unknownPage);
    QCOMPARE(unknownPage.error().code, Ui::NavigationModelErrorCode::UnknownPage);

    auto duplicateReference = Ui::NavigationModel::create(
        {
            Ui::NavigationNode::page(pageId(QStringLiteral("campus_information"))),
            Ui::NavigationNode::page(pageId(QStringLiteral("campus_information")))
        },
        {descriptorFor(QStringLiteral("campus_information"), QStringLiteral("Information"))},
        pageId(QStringLiteral("campus_information")),
        pageId(QStringLiteral("campus_information")),
        Application::WorkspaceLifecycleState::Closed
        );
    QVERIFY(!duplicateReference);
    QCOMPARE(duplicateReference.error().code, Ui::NavigationModelErrorCode::DuplicateNodeId);

    auto missingGroupLabel = Ui::NavigationModel::create(
        {
            Ui::NavigationNode::group(
                QStringLiteral("empty_label"),
                QStringLiteral("  "),
                {Ui::NavigationNode::page(pageId(QStringLiteral("campus_information")))}
                )
        },
        {descriptorFor(QStringLiteral("campus_information"), QStringLiteral("Information"))},
        pageId(QStringLiteral("campus_information")),
        pageId(QStringLiteral("campus_information")),
        Application::WorkspaceLifecycleState::Closed
        );
    QVERIFY(!missingGroupLabel);
    QCOMPARE(missingGroupLabel.error().code, Ui::NavigationModelErrorCode::InvalidNode);
}

void NavigationModelTests::workspaceAndPermissionsFilterRoutesAndChooseInitialPages()
{
    using Ui::NavigationNode;
    using Ui::NavigationWorkspaceAvailability;
    constexpr auto workspace = NavigationWorkspaceAvailability::WorkspaceOpen;
    const std::vector<NavigationNode> nodes{
        NavigationNode::page(pageId(QStringLiteral("campus_information"))),
        NavigationNode::group(
            QStringLiteral("workspace"),
            QStringLiteral("My Workspace"),
            {
                NavigationNode::page(pageId(QStringLiteral("my_workspace")), workspace),
                NavigationNode::page(pageId(QStringLiteral("classes")), workspace)
            },
            workspace
            )
    };
    const std::vector<Ui::PageDescriptor> descriptors{
        descriptorFor(QStringLiteral("campus_information"), QStringLiteral("Information")),
        descriptorFor(QStringLiteral("my_workspace"), QStringLiteral("My Workspace")),
        descriptorFor(QStringLiteral("classes"), QStringLiteral("Classes"),
                      {QStringLiteral("classes.view")})
    };

    auto closed = modelFor(
        nodes,
        descriptors,
        pageId(QStringLiteral("campus_information")),
        pageId(QStringLiteral("my_workspace")),
        Application::WorkspaceLifecycleState::Closed
        );
    QCOMPARE(closed.initialPageId()->value(), QStringLiteral("campus_information"));
    QCOMPARE(closed.visibleNodes().size(), std::size_t(1));
    QCOMPARE(closed.visibleNodes().front().pageId->value(), QStringLiteral("campus_information"));
    QVERIFY(!closed.isPageVisible(pageId(QStringLiteral("my_workspace"))));

    auto openWithoutPermissions = modelFor(
        nodes,
        descriptors,
        pageId(QStringLiteral("campus_information")),
        pageId(QStringLiteral("my_workspace")),
        Application::WorkspaceLifecycleState::Open
        );
    QCOMPARE(openWithoutPermissions.initialPageId()->value(), QStringLiteral("my_workspace"));
    QCOMPARE(openWithoutPermissions.visibleNodes().size(), std::size_t(2));
    QVERIFY(openWithoutPermissions.isPageVisible(pageId(QStringLiteral("my_workspace"))));
    QVERIFY(!openWithoutPermissions.isPageVisible(pageId(QStringLiteral("classes"))));
    QVERIFY(!openWithoutPermissions.grantedPermissionIds().has_value());

    auto openWithPermissions = modelFor(
        nodes,
        descriptors,
        pageId(QStringLiteral("campus_information")),
        pageId(QStringLiteral("my_workspace")),
        Application::WorkspaceLifecycleState::Open,
        QStringList{QStringLiteral("classes.view")}
        );
    QVERIFY(openWithPermissions.isPageVisible(pageId(QStringLiteral("classes"))));
    QCOMPARE(openWithPermissions.pageMetadata(pageId(QStringLiteral("classes")))
                 ->requiredPermissions,
             QStringList{QStringLiteral("classes.view")});
    openWithPermissions.updateAccessContext(
        Application::WorkspaceLifecycleState::Closed,
        QStringList{QStringLiteral("classes.view")}
        );
    QVERIFY(!openWithPermissions.isPageVisible(pageId(QStringLiteral("classes"))));
    QCOMPARE(openWithPermissions.grantedPermissionIds(),
             std::optional<QStringList>(QStringList{QStringLiteral("classes.view")}));
}

void NavigationModelTests::historyActionStateRequiresAvailableAdjacentRoutes()
{
    const std::vector<Ui::PageId> history{
        pageId(QStringLiteral("campus_information")),
        pageId(QStringLiteral("my_workspace")),
        pageId(QStringLiteral("classes"))
    };
    const auto allAvailable = [](const Ui::PageId&) { return true; };

    const Ui::NavigationHistoryActionState initial =
        Ui::deriveNavigationHistoryActionState({}, std::nullopt, allAvailable);
    QVERIFY(!initial.backEnabled);
    QVERIFY(!initial.forwardEnabled);

    const Ui::NavigationHistoryActionState available =
        Ui::deriveNavigationHistoryActionState(history, std::size_t(1), allAvailable);
    QVERIFY(available.backEnabled);
    QVERIFY(available.forwardEnabled);

    const QStringList withoutBackTarget{
        QStringLiteral("my_workspace"), QStringLiteral("classes")
    };
    const Ui::NavigationHistoryActionState unavailableBack =
        Ui::deriveNavigationHistoryActionState(
            history,
            std::size_t(1),
            [&withoutBackTarget](const Ui::PageId& target) {
                return withoutBackTarget.contains(target.value());
            }
            );
    QVERIFY(!unavailableBack.backEnabled);
    QVERIFY(unavailableBack.forwardEnabled);

    const QStringList withoutForwardTarget{
        QStringLiteral("campus_information"), QStringLiteral("my_workspace")
    };
    const Ui::NavigationHistoryActionState unavailableForward =
        Ui::deriveNavigationHistoryActionState(
            history,
            std::size_t(1),
            [&withoutForwardTarget](const Ui::PageId& target) {
                return withoutForwardTarget.contains(target.value());
            }
            );
    QVERIFY(unavailableForward.backEnabled);
    QVERIFY(!unavailableForward.forwardEnabled);

    const Ui::NavigationHistoryActionState invalidIndex =
        Ui::deriveNavigationHistoryActionState(history, std::size_t(3), allAvailable);
    QVERIFY(!invalidIndex.backEnabled);
    QVERIFY(!invalidIndex.forwardEnabled);
}

void NavigationModelTests::shellCommitsHistoryOnlyAfterGuardAndActivationSuccess()
{
    PageProbe campus{QStringLiteral("campus_information")};
    PageProbe workspace{QStringLiteral("my_workspace")};
    PageProbe classes{QStringLiteral("classes")};
    PageProbe failing{QStringLiteral("failing"), 0, 0, true};
    const auto checkIcon = Resources::ResourceId::fromString(
        QStringLiteral("resource:assets/icons/check.png")
        );
    QVERIFY(checkIcon.has_value());
    Ui::ApplicationShell shell(*m_loader, m_preferences.model);
    for (Ui::PageDescriptor descriptor : commonDescriptors(
             &campus,
             &workspace,
             &classes,
             &failing,
             *checkIcon
             ))
    {
        QVERIFY(shell.pageHost().registerPage(std::move(descriptor)));
    }

    auto configuration = shell.configureNavigation(
        commonTree(),
        pageId(QStringLiteral("campus_information")),
        pageId(QStringLiteral("my_workspace")),
        Application::WorkspaceLifecycleState::Closed
        );
    QVERIFY(configuration);
    QVERIFY(shell.navigationModel() != nullptr);
    auto* tree = shell.findChild<QTreeWidget*>(QStringLiteral("classMngrNextNavigationTree"));
    QVERIFY(tree != nullptr);
    QTreeWidgetItem* const campusItem = itemByDataRole(
        tree, Qt::UserRole + 1, QStringLiteral("campus_information")
        );
    QVERIFY(campusItem != nullptr);
    QVERIFY(!campusItem->icon(0).isNull());
    QVERIFY(!campusItem->icon(0).pixmap(QSize(16, 16)).isNull());
    QCOMPARE(shell.navigationModel()->pageMetadata(pageId(QStringLiteral("campus_information")))
                 ->iconResourceId,
             checkIcon);
    QCOMPARE(campus.factoryCalls, 0);
    QCOMPARE(workspace.factoryCalls, 0);
    QCOMPARE(classes.factoryCalls, 0);
    QCOMPARE(failing.factoryCalls, 0);
    QVERIFY(!shell.pageHost().activePageKey().has_value());
    QVERIFY(shell.navigateTo(pageId(QStringLiteral("campus_information")), Ui::NavigationCause::Initial));
    QCOMPARE(shell.navigationModel()->currentPageId()->value(), QStringLiteral("campus_information"));
    QCOMPARE(shell.navigationModel()->selectedPageId()->value(), QStringLiteral("campus_information"));
    QCOMPARE(shell.navigationModel()->history().size(), std::size_t(1));
    QCOMPARE(campus.factoryCalls, 1);
    QCOMPARE(workspace.factoryCalls, 0);
    QCOMPARE(classes.factoryCalls, 0);
    QCOMPARE(failing.factoryCalls, 0);
    QVERIFY(!shell.navigateTo(pageId(QStringLiteral("my_workspace"))));
    QCOMPARE(workspace.factoryCalls, 0);

    shell.updateNavigationAccessContext(
        Application::WorkspaceLifecycleState::Open,
        std::nullopt
        );
    QTreeWidgetItem* workspaceItem = nullptr;
    QTreeWidgetItemIterator workspaceIterator(tree);
    while (*workspaceIterator)
    {
        if ((*workspaceIterator)->data(0, Qt::UserRole + 1).toString()
            == QStringLiteral("my_workspace"))
        {
            workspaceItem = *workspaceIterator;
            break;
        }
        ++workspaceIterator;
    }
    QVERIFY(workspaceItem != nullptr);
    QVERIFY(workspaceItem->icon(0).isNull());
    tree->setCurrentItem(workspaceItem);
    QCOMPARE(workspace.factoryCalls, 1);
    QCOMPARE(shell.navigationModel()->history().size(), std::size_t(2));

    std::vector<Ui::NavigationGuardRequest> guardRequests;
    std::optional<Ui::NavigationCause> rejectedCause;
    QString rejectedTarget;
    shell.setNavigationGuard([&](const Ui::NavigationGuardRequest& request) {
        guardRequests.push_back(request);
        if (request.cause == rejectedCause
            || request.target.value() == rejectedTarget)
        {
            return Ui::NavigationGuardDecision::Reject;
        }
        return Ui::NavigationGuardDecision::Allow;
    });

    const Ui::PageId usefulDropbox = pageId(QStringLiteral("useful_dropbox"));
    QVERIFY(!shell.navigationModel()->isPageVisible(usefulDropbox));
    QTreeWidgetItem* const dropboxItem = itemByDataRole(
        tree, Qt::UserRole + 2, QStringLiteral("useful_dropbox")
        );
    QVERIFY(dropboxItem != nullptr);
    QCOMPARE(dropboxItem->data(0, Qt::UserRole).toInt(),
             static_cast<int>(Ui::NavigationNode::Kind::Placeholder));
    QVERIFY(dropboxItem->data(0, Qt::UserRole + 1).toString().isEmpty());
    QVERIFY(dropboxItem->icon(0).isNull());
    QVERIFY(dropboxItem->parent() != nullptr);
    dropboxItem->parent()->setExpanded(true);
    const auto currentBeforePlaceholder = shell.navigationModel()->currentPageId();
    const auto historyBeforePlaceholder = shell.navigationModel()->history();
    const auto activeBeforePlaceholder = shell.pageHost().activePageKey();
    const std::size_t guardCountBeforePlaceholder = guardRequests.size();
    tree->setCurrentItem(dropboxItem);
    QCOMPARE(shell.navigationModel()->currentPageId(), currentBeforePlaceholder);
    QCOMPARE(shell.navigationModel()->selectedPageId(), currentBeforePlaceholder);
    QCOMPARE(shell.navigationModel()->history().size(), historyBeforePlaceholder.size());
    for (std::size_t index = 0; index < historyBeforePlaceholder.size(); ++index)
    {
        QCOMPARE(shell.navigationModel()->history()[index].value(),
                 historyBeforePlaceholder[index].value());
    }
    QCOMPARE(shell.pageHost().activePageKey(), activeBeforePlaceholder);
    QCOMPARE(tree->currentItem(), workspaceItem);
    QCOMPARE(guardRequests.size(), guardCountBeforePlaceholder);

    rejectedTarget = QStringLiteral("classes");
    QTreeWidgetItem* classesItem = nullptr;
    QTreeWidgetItemIterator classesIterator(tree);
    while (*classesIterator)
    {
        if ((*classesIterator)->data(0, Qt::UserRole + 1).toString()
            == QStringLiteral("classes"))
        {
            classesItem = *classesIterator;
            break;
        }
        ++classesIterator;
    }
    QVERIFY(classesItem != nullptr);
    QVERIFY(classesItem->icon(0).isNull());
    tree->setCurrentItem(classesItem);
    QCOMPARE(classes.factoryCalls, 0);
    QCOMPARE(shell.navigationModel()->currentPageId()->value(), QStringLiteral("my_workspace"));
    QCOMPARE(shell.navigationModel()->selectedPageId()->value(), QStringLiteral("my_workspace"));
    QCOMPARE(shell.navigationModel()->history().size(), std::size_t(2));
    QCOMPARE(shell.pageHost().activePageKey(), std::optional<QString>(QStringLiteral("my_workspace")));
    QCOMPARE(guardRequests.back().source->value(), QStringLiteral("my_workspace"));
    QCOMPARE(guardRequests.back().target.value(), QStringLiteral("classes"));
    QCOMPARE(guardRequests.back().cause, Ui::NavigationCause::Selection);

    rejectedTarget.clear();
    QVERIFY(shell.navigateTo(pageId(QStringLiteral("classes"))));
    QCOMPARE(classes.factoryCalls, 1);
    QCOMPARE(shell.navigationModel()->history().size(), std::size_t(3));
    QCOMPARE(shell.navigationModel()->history().back().value(), QStringLiteral("classes"));
    QVERIFY(shell.navigateBack());
    QCOMPARE(shell.navigationModel()->currentPageId()->value(), QStringLiteral("my_workspace"));
    QVERIFY(shell.navigateForward());
    QCOMPARE(shell.navigationModel()->currentPageId()->value(), QStringLiteral("classes"));
    QVERIFY(shell.navigateBack());
    QVERIFY(shell.navigateTo(pageId(QStringLiteral("campus_information"))));
    QCOMPARE(shell.navigationModel()->history().size(), std::size_t(3));
    QCOMPARE(shell.navigationModel()->history().back().value(), QStringLiteral("campus_information"));
    QVERIFY(!shell.navigationModel()->canGoForward());
    QVERIFY(!shell.navigateForward());

    rejectedCause = Ui::NavigationCause::Back;
    const auto historyBeforeRejectedBack = shell.navigationModel()->history();
    const auto currentBeforeRejectedBack = shell.navigationModel()->currentPageId();
    QVERIFY(!shell.navigateBack());
    QCOMPARE(shell.navigationModel()->history().size(), historyBeforeRejectedBack.size());
    QCOMPARE(shell.navigationModel()->currentPageId(), currentBeforeRejectedBack);
    QCOMPARE(shell.pageHost().activePageKey(), std::optional<QString>(QStringLiteral("campus_information")));
    QCOMPARE(guardRequests.back().cause, Ui::NavigationCause::Back);

    rejectedCause = std::nullopt;
    QVERIFY(shell.navigateBack());
    rejectedCause = Ui::NavigationCause::Forward;
    const auto currentBeforeRejectedForward = shell.navigationModel()->currentPageId();
    QVERIFY(!shell.navigateForward());
    QCOMPARE(shell.navigationModel()->currentPageId(), currentBeforeRejectedForward);
    QCOMPARE(shell.pageHost().activePageKey(), std::optional<QString>(QStringLiteral("my_workspace")));
    QCOMPARE(guardRequests.back().cause, Ui::NavigationCause::Forward);

    rejectedCause = std::nullopt;
    QVERIFY(shell.navigateForward());
    QCOMPARE(shell.navigationModel()->currentPageId()->value(), QStringLiteral("campus_information"));
    const auto historyBeforeActivationFailure = shell.navigationModel()->history();
    QVERIFY(!shell.navigateTo(pageId(QStringLiteral("failing"))));
    QCOMPARE(failing.factoryCalls, 1);
    QCOMPARE(failing.activationCalls, 1);
    QCOMPARE(shell.navigationModel()->history().size(), historyBeforeActivationFailure.size());
    QCOMPARE(shell.navigationModel()->currentPageId()->value(), QStringLiteral("campus_information"));
    QCOMPARE(shell.navigationModel()->selectedPageId()->value(), QStringLiteral("campus_information"));
    QCOMPARE(shell.pageHost().activePageKey(), std::optional<QString>(QStringLiteral("campus_information")));
    QVERIFY(shell.pageHost().hasPageInstance(QStringLiteral("campus_information")));

    QVERIFY(tree->currentItem() != nullptr);
    QCOMPARE(tree->currentItem()->data(0, Qt::UserRole + 1).toString(),
             QStringLiteral("campus_information"));
}

void NavigationModelTests::shellHistoryControlsTrackAccessRevocation()
{
    using Ui::NavigationNode;
    using Ui::NavigationWorkspaceAvailability;
    constexpr auto workspace = NavigationWorkspaceAvailability::WorkspaceOpen;
    const Ui::PageId campusPage = pageId(QStringLiteral("campus_information"));
    const Ui::PageId workspacePage = pageId(QStringLiteral("my_workspace"));
    const Ui::PageId protectedPage = pageId(QStringLiteral("classes"));
    const QStringList classPermission{QStringLiteral("classes.view")};
    PageProbe campus{QStringLiteral("campus_information")};
    PageProbe workspaceProbe{QStringLiteral("my_workspace")};
    PageProbe protectedProbe{QStringLiteral("classes")};

    Ui::ApplicationShell shell(*m_loader, m_preferences.model);
    QVERIFY(shell.pageHost().registerPage(descriptorFor(
        QStringLiteral("campus_information"), QStringLiteral("Campus Info"),
        {}, {}, &campus
        )));
    QVERIFY(shell.pageHost().registerPage(descriptorFor(
        QStringLiteral("my_workspace"), QStringLiteral("My Workspace"),
        {}, {}, &workspaceProbe
        )));
    QVERIFY(shell.pageHost().registerPage(descriptorFor(
        QStringLiteral("classes"), QStringLiteral("Classes"), classPermission,
        {}, &protectedProbe
        )));

    auto configuration = shell.configureNavigation(
        {
            NavigationNode::page(campusPage),
            NavigationNode::group(
                QStringLiteral("workspace"),
                QStringLiteral("Workspace"),
                {
                    NavigationNode::page(workspacePage, workspace),
                    NavigationNode::page(protectedPage, workspace)
                },
                workspace
                )
        },
        campusPage,
        workspacePage,
        Application::WorkspaceLifecycleState::Closed
        );
    QVERIFY(configuration);

    auto* const backButton = shell.findChild<QPushButton*>(
        QStringLiteral("classMngrNextNavigationBack")
        );
    auto* const forwardButton = shell.findChild<QPushButton*>(
        QStringLiteral("classMngrNextNavigationForward")
        );
    QVERIFY(backButton != nullptr);
    QVERIFY(forwardButton != nullptr);
    QVERIFY(!backButton->isEnabled());
    QVERIFY(!forwardButton->isEnabled());

    QVERIFY(shell.navigateTo(campusPage, Ui::NavigationCause::Initial));
    QVERIFY(!backButton->isEnabled());
    QVERIFY(!forwardButton->isEnabled());

    shell.updateNavigationAccessContext(
        Application::WorkspaceLifecycleState::Open,
        classPermission
        );
    QVERIFY(shell.navigateTo(workspacePage));
    QVERIFY(backButton->isEnabled());
    QVERIFY(!forwardButton->isEnabled());
    QVERIFY(shell.navigateBack());
    QVERIFY(!backButton->isEnabled());
    QVERIFY(forwardButton->isEnabled());

    // The forward target disappears when workspace visibility is revoked.
    shell.updateNavigationAccessContext(
        Application::WorkspaceLifecycleState::Closed,
        classPermission
        );
    QVERIFY(!backButton->isEnabled());
    QVERIFY(!forwardButton->isEnabled());
    const auto stateBeforeWorkspaceRejectedForward = shell.navigationModel()->historyActionState();
    const auto historyBeforeWorkspaceRejectedForward = shell.navigationModel()->history();
    const auto indexBeforeWorkspaceRejectedForward = shell.navigationModel()->historyIndex();
    const auto currentBeforeWorkspaceRejectedForward = shell.navigationModel()->currentPageId();
    const auto selectedBeforeWorkspaceRejectedForward = shell.navigationModel()->selectedPageId();
    const auto activeBeforeWorkspaceRejectedForward = shell.pageHost().activePageKey();
    QVERIFY(!shell.navigateForward());
    QCOMPARE(shell.navigationModel()->historyActionState().backEnabled,
             stateBeforeWorkspaceRejectedForward.backEnabled);
    QCOMPARE(shell.navigationModel()->historyActionState().forwardEnabled,
             stateBeforeWorkspaceRejectedForward.forwardEnabled);
    QCOMPARE(shell.navigationModel()->history(), historyBeforeWorkspaceRejectedForward);
    QCOMPARE(shell.navigationModel()->historyIndex(), indexBeforeWorkspaceRejectedForward);
    QCOMPARE(shell.navigationModel()->currentPageId(), currentBeforeWorkspaceRejectedForward);
    QCOMPARE(shell.navigationModel()->selectedPageId(), selectedBeforeWorkspaceRejectedForward);
    QCOMPARE(shell.pageHost().activePageKey(), activeBeforeWorkspaceRejectedForward);

    shell.updateNavigationAccessContext(
        Application::WorkspaceLifecycleState::Open,
        classPermission
        );
    QVERIFY(shell.navigateForward());
    QVERIFY(shell.navigateBack());
    QVERIFY(forwardButton->isEnabled());
    QVERIFY(shell.navigateTo(protectedPage));
    QCOMPARE(shell.navigationModel()->history().size(), std::size_t(2));
    QVERIFY(backButton->isEnabled());
    QVERIFY(!forwardButton->isEnabled());
    QVERIFY(shell.navigateTo(workspacePage));
    QVERIFY(backButton->isEnabled());
    QVERIFY(!forwardButton->isEnabled());

    // Permission revocation hides the Back target and rejects the action.
    shell.updateNavigationAccessContext(
        Application::WorkspaceLifecycleState::Open,
        std::nullopt
        );
    QVERIFY(!backButton->isEnabled());
    QVERIFY(!forwardButton->isEnabled());
    const auto stateBeforePermissionRejectedBack = shell.navigationModel()->historyActionState();
    const auto historyBeforePermissionRejectedBack = shell.navigationModel()->history();
    const auto indexBeforePermissionRejectedBack = shell.navigationModel()->historyIndex();
    const auto currentBeforePermissionRejectedBack = shell.navigationModel()->currentPageId();
    const auto selectedBeforePermissionRejectedBack = shell.navigationModel()->selectedPageId();
    const auto activeBeforePermissionRejectedBack = shell.pageHost().activePageKey();
    QVERIFY(!shell.navigateBack());
    QCOMPARE(shell.navigationModel()->historyActionState().backEnabled,
             stateBeforePermissionRejectedBack.backEnabled);
    QCOMPARE(shell.navigationModel()->historyActionState().forwardEnabled,
             stateBeforePermissionRejectedBack.forwardEnabled);
    QCOMPARE(shell.navigationModel()->history(), historyBeforePermissionRejectedBack);
    QCOMPARE(shell.navigationModel()->historyIndex(), indexBeforePermissionRejectedBack);
    QCOMPARE(shell.navigationModel()->currentPageId(), currentBeforePermissionRejectedBack);
    QCOMPARE(shell.navigationModel()->selectedPageId(), selectedBeforePermissionRejectedBack);
    QCOMPARE(shell.pageHost().activePageKey(), activeBeforePermissionRejectedBack);

    // Regrant, then move to the history start so the protected route is the
    // Forward target. Revoking its permission disables and rejects Forward.
    shell.updateNavigationAccessContext(
        Application::WorkspaceLifecycleState::Open,
        classPermission
        );
    QVERIFY(shell.navigateBack());
    QVERIFY(shell.navigateBack());
    QCOMPARE(shell.navigationModel()->currentPageId(), std::optional<Ui::PageId>(campusPage));
    QVERIFY(!backButton->isEnabled());
    QVERIFY(forwardButton->isEnabled());
    shell.updateNavigationAccessContext(
        Application::WorkspaceLifecycleState::Open,
        std::nullopt
        );
    QVERIFY(!backButton->isEnabled());
    QVERIFY(!forwardButton->isEnabled());
    const auto historyBeforePermissionRejectedForward = shell.navigationModel()->history();
    const auto indexBeforePermissionRejectedForward = shell.navigationModel()->historyIndex();
    const auto currentBeforePermissionRejectedForward = shell.navigationModel()->currentPageId();
    const auto selectedBeforePermissionRejectedForward = shell.navigationModel()->selectedPageId();
    const auto activeBeforePermissionRejectedForward = shell.pageHost().activePageKey();
    QVERIFY(!shell.navigateForward());
    QCOMPARE(shell.navigationModel()->history(), historyBeforePermissionRejectedForward);
    QCOMPARE(shell.navigationModel()->historyIndex(), indexBeforePermissionRejectedForward);
    QCOMPARE(shell.navigationModel()->currentPageId(), currentBeforePermissionRejectedForward);
    QCOMPARE(shell.navigationModel()->selectedPageId(), selectedBeforePermissionRejectedForward);
    QCOMPARE(shell.pageHost().activePageKey(), activeBeforePermissionRejectedForward);
}

void NavigationModelTests::shellExposesNewTeacherProfileAction()
{
    Ui::ApplicationShell shell(*m_loader, m_preferences.model);
    QAction* const action = shell.findChild<QAction*>(
        QStringLiteral("classMngrNextNewTeacherProfile")
        );
    QVERIFY(action != nullptr);
    QCOMPARE(action->text(), QStringLiteral("New Teacher Profile…"));
    QCOMPARE(action->shortcut(), QKeySequence::New);
    auto* const fileMenu = qobject_cast<QMenu*>(action->parent());
    QVERIFY(fileMenu != nullptr);
    QCOMPARE(fileMenu->title(), QStringLiteral("File"));

    int invocationCount = 0;
    shell.setNewTeacherProfileHandler([&invocationCount] {
        ++invocationCount;
    });
    action->trigger();
    QCOMPARE(invocationCount, 1);
}

void NavigationModelTests::shellExposesOpenTeacherProfileAction()
{
    Ui::ApplicationShell shell(*m_loader, m_preferences.model);
    QAction* const action = shell.findChild<QAction*>(
        QStringLiteral("classMngrNextOpenTeacherProfile")
        );
    QVERIFY(action != nullptr);
    QCOMPARE(action->text(), QStringLiteral("Open Teacher Profile…"));
    QCOMPARE(action->shortcut(), QKeySequence::Open);
    auto* const fileMenu = qobject_cast<QMenu*>(action->parent());
    QVERIFY(fileMenu != nullptr);
    QCOMPARE(fileMenu->title(), QStringLiteral("File"));

    int invocationCount = 0;
    shell.setOpenTeacherProfileHandler([&invocationCount] {
        ++invocationCount;
    });
    action->trigger();
    QCOMPARE(invocationCount, 1);
}

void NavigationModelTests::shellExposesInterruptedProfileRecoveryAction()
{
    Ui::ApplicationShell shell(*m_loader, m_preferences.model);
    QAction* const action = shell.findChild<QAction*>(
        QStringLiteral("classMngrNextRecoverInterruptedTeacherProfile")
        );
    QVERIFY(action != nullptr);
    QCOMPARE(action->text(), QStringLiteral("Recover Interrupted Teacher Profile…"));
    auto* const fileMenu = qobject_cast<QMenu*>(action->parent());
    QVERIFY(fileMenu != nullptr);
    QCOMPARE(fileMenu->title(), QStringLiteral("File"));

    int invocationCount = 0;
    shell.setRecoverInterruptedTeacherProfileHandler([&invocationCount] {
        ++invocationCount;
    });
    action->trigger();
    QCOMPARE(invocationCount, 1);
}

void NavigationModelTests::shellOwnsTransientNotificationPortAndKeepsIdleStatusBarHidden()
{
    auto shell = std::make_unique<Ui::ApplicationShell>(*m_loader, m_preferences.model);
    QStatusBar* const statusBar = shell->statusBar();

    QCOMPARE(statusBar->objectName(), QStringLiteral("classMngrNextNotificationStatusBar"));
    QVERIFY(statusBar->isHidden());

    shell->show();
    QApplication::processEvents();
    QVERIFY(statusBar->isHidden());

    const auto shown = shell->notificationPort().show(Application::NotificationRequest{
        .message = "Workspace saved.",
        .severity = Application::NotificationSeverity::Information
    });
    QVERIFY(shown.hasValue());
    QVERIFY(statusBar->isVisible());
    QCOMPARE(statusBar->currentMessage(), QStringLiteral("Workspace saved."));

    QVERIFY(shell->notificationPort().clear().hasValue());
    QVERIFY(statusBar->isHidden());
    QVERIFY(statusBar->currentMessage().isEmpty());

    QVERIFY(shell->notificationPort().show(Application::NotificationRequest{
        .message = "Replacement before close",
        .severity = Application::NotificationSeverity::Warning
    }).hasValue());
    QVERIFY(statusBar->isVisible());
    QPointer<QStatusBar> statusBarLifetime(statusBar);
    shell.reset();
    QVERIFY(statusBarLifetime.isNull());
}

QTEST_MAIN(NavigationModelTests)
#include "navigation_model_tests.moc"
