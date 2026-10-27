#include "preferences_test_ports.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/resources/resource_scope.h"
#include "next/ui/initial_page.h"
#include "next/ui/page_host.h"

#include <QComboBox>
#include <QColorDialog>
#include <QDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QTest>
#include <QVector>

#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

class ProbePage;

struct PageProbe final
{
    QString key;
    QString name;
    QStringList* lifecycleLog = nullptr;
    Resources::ResourceId resourceId = *Resources::ResourceId::fromString(
        QStringLiteral("document.body:Book Reports/Book Report Grading Rubric.pdf")
        );
    Resources::ResourceLoader* observedLoader = nullptr;
    QString observedPageKey;
    Resources::ResourceScopeKind observedScopeKind = Resources::ResourceScopeKind::Startup;
    QString observedScopeLabel;
    Resources::ResourceScopeKind pageHandleScopeKind = Resources::ResourceScopeKind::Startup;
    QString pageHandleScopeLabel;
    Resources::ResourceScopeKind featureHandleScopeKind = Resources::ResourceScopeKind::Startup;
    QString featureHandleScopeLabel;
    QString resourceSetupError;
    QStringList events;
    QVector<QPointer<ProbePage>> pages;
    int factoryCalls = 0;
    int constructed = 0;
    int destroyed = 0;
    int activationCalls = 0;
    int suspensionCalls = 0;
    bool failActivation = false;
    bool pageResourceOpenAtSuspension = false;
    bool featureResourceOpenAtSuspension = false;
    bool featureResourceClearedAtSuspension = false;
    bool pageResourceOpenAtDestruction = false;

    void recordEvent(const QString& event)
    {
        events.push_back(event);
        if (lifecycleLog)
        {
            lifecycleLog->push_back(event);
        }
    }
};

class ProbePage final : public Ui::Page
{
public:
    ProbePage(
        const Ui::PageContext& context,
        PageProbe& probe
        )
        : Ui::Page(context)
        , m_probe(probe)
    {
        ++m_probe.constructed;
        m_probe.recordEvent(m_probe.name + QStringLiteral(".construct"));
        m_probe.observedLoader = &context.resourceLoader();
        m_probe.observedPageKey = context.pageKey();
        m_probe.observedScopeKind = context.pageScope().kind();
        m_probe.observedScopeLabel = context.pageScope().ownerLabel();

        auto pageRequest = Resources::ResourceRequest::create(
            m_probe.resourceId,
            context.pageScope()
            );
        if (!pageRequest)
        {
            m_probe.resourceSetupError = pageRequest.error().message;
            return;
        }
        auto pageStream = context.resourceLoader().openStream(*pageRequest);
        if (!pageStream)
        {
            m_probe.resourceSetupError = pageStream.error().message;
            return;
        }
        m_pageResource.emplace(std::move(*pageStream));
        m_probe.pageHandleScopeKind = m_pageResource->scope().kind();
        m_probe.pageHandleScopeLabel = m_pageResource->scope().ownerLabel();
        m_probe.pages.push_back(this);
    }

    ~ProbePage() override
    {
        m_probe.pageResourceOpenAtDestruction =
            m_pageResource.has_value() && m_pageResource->get().isOpen();
        ++m_probe.destroyed;
        m_probe.recordEvent(m_probe.name + QStringLiteral(".destroy"));
    }

    Ui::PageOperationResult<void> onActivating() override
    {
        ++m_probe.activationCalls;
        m_probe.recordEvent(m_probe.name + QStringLiteral(".activate"));
        if (!m_probe.resourceSetupError.isEmpty())
        {
            return std::unexpected(Ui::PageOperationError{m_probe.resourceSetupError});
        }

        if (!m_featureResource.has_value())
        {
            auto featureScope = Resources::ResourceScope::create(
                Resources::ResourceScopeKind::Feature,
                m_probe.key + QStringLiteral(".feature")
                );
            if (!featureScope)
            {
                return std::unexpected(Ui::PageOperationError{
                    featureScope.error().message
                });
            }
            auto featureRequest = Resources::ResourceRequest::create(
                m_probe.resourceId,
                std::move(*featureScope)
                );
            if (!featureRequest)
            {
                return std::unexpected(Ui::PageOperationError{
                    featureRequest.error().message
                });
            }
            auto featureStream = resourceContext().resourceLoader().openStream(*featureRequest);
            if (!featureStream)
            {
                return std::unexpected(Ui::PageOperationError{
                    featureStream.error().message
                });
            }
            m_featureResource.emplace(std::move(*featureStream));
            m_probe.featureHandleScopeKind = m_featureResource->scope().kind();
            m_probe.featureHandleScopeLabel = m_featureResource->scope().ownerLabel();
        }

        if (m_probe.failActivation)
        {
            return std::unexpected(Ui::PageOperationError{
                QStringLiteral("requested activation failure")
            });
        }
        return {};
    }

    void onSuspending() noexcept override
    {
        ++m_probe.suspensionCalls;
        m_probe.recordEvent(m_probe.name + QStringLiteral(".suspend"));
        m_probe.pageResourceOpenAtSuspension =
            m_pageResource.has_value() && m_pageResource->get().isOpen();
        m_probe.featureResourceOpenAtSuspension =
            m_featureResource.has_value() && m_featureResource->get().isOpen();
        m_featureResource.reset();
        m_probe.featureResourceClearedAtSuspension = !m_featureResource.has_value();
    }

private:
    PageProbe& m_probe;
    std::optional<Resources::ScopedResource<Resources::ResourceFileStream>> m_pageResource;
    std::optional<Resources::ScopedResource<Resources::ResourceFileStream>> m_featureResource;
};

Ui::PageHost::PageFactory factoryFor(PageProbe& probe)
{
    return [&probe](const Ui::PageContext& context)
        -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
        ++probe.factoryCalls;
        probe.recordEvent(probe.name + QStringLiteral(".factory"));
        auto page = std::make_unique<ProbePage>(context, probe);
        return std::unique_ptr<Ui::Page>(std::move(page));
    };
}

Ui::PageDescriptor descriptorFor(
    const QString& pageId,
    Ui::PageFactory factory
    )
{
    return Ui::PageDescriptor{
        .id = *Ui::PageId::fromString(pageId),
        .label = pageId.trimmed(),
        .iconResourceId = std::nullopt,
        .requiredPermissions = {},
        .factory = std::move(factory),
        .resourcePolicy = Ui::PageResourcePolicy::PageScoped,
        .initialDataPolicy = Ui::PageInitialDataPolicy::None
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

}  // namespace

class PageHostTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void preferencesModelIsSharedThroughPageContext();
    void descriptorRegistrationRetainsMetadataWithoutConstructingPages();
    void invalidAndDuplicateDescriptorsAreRejected();
    void lifecycleStatesAreQueryableObservableAndRejectReentrantMutations();
    void registrationIsLazyAndPagesSuspendReuseReleaseAndRecreate();
    void factoryAndActivationFailuresPreserveTheCurrentPage();
    void initialSetupPageShowsPendingAndRecoveryActions();
    void initialSetupTeacherEntryValidatesAndRetainsFailedForm();
    void initialSetupTeacherEntrySkipsWhenAChoiceExists();
    void initialSetupTeacherEntryOptionalOnlyDataIsNotSkipped_data();
    void initialSetupTeacherEntryOptionalOnlyDataIsNotSkipped();
    void initialSetupTeacherEntryChoiceLoadFailureIsVisible();
    void initialSetupTeacherEntryAddAnotherThenContinueSavesBoth();
    void initialSetupClassDetailsChoicesValidationColorAndDraft();
    void initialSetupClassDetailsAutoSelectsOnlyOneTeacher();

private:
    Test::MemoryPreferences m_preferences;
    std::unique_ptr<Resources::ResourceLoader> m_loader;
};

namespace
{

void completeNextColorDialog(const QColor& color, const bool accept)
{
    QTimer::singleShot(0, [color, accept] {
        auto* const dialog = qobject_cast<QColorDialog*>(
            QApplication::activeModalWidget()
            );
        if (!dialog)
        {
            return;
        }
        dialog->setCurrentColor(color);
        if (accept)
        {
            dialog->accept();
        }
        else
        {
            dialog->reject();
        }
    });
}

Application::InitialSetupTeacherChoicesSnapshot twoTeachers()
{
    const auto first = Domain::TeacherId::fromString("11");
    const auto second = Domain::TeacherId::fromString("12");
    Q_ASSERT(first && second);
    return {
        .teachers = {
            {
                .teacherId = *first,
                .teacherEn = u"Alex Kim",
                .preferredName = u"Alex"
            },
            {
                .teacherId = *second,
                .teacherEn = u"Sam Lee",
                .preferredName = u"Sam"
            }
        }
    };
}

} // namespace

void PageHostTests::initTestCase()
{
    auto catalog = Resources::ResourceCatalog::load(catalogPath(), resourceRoot());
    QVERIFY2(catalog.has_value(), catalog ? "" : qPrintable(catalog.error().message));
    m_loader = std::make_unique<Resources::ResourceLoader>(std::move(*catalog));

    const auto resourceId = Resources::ResourceId::fromString(
        QStringLiteral("document.body:Book Reports/Book Report Grading Rubric.pdf")
        );
    QVERIFY(resourceId.has_value());
    const Resources::ResourceDescriptor* const descriptor = m_loader->catalog().find(*resourceId);
    QVERIFY(descriptor != nullptr);
    QCOMPARE(descriptor->streamabilityStatus(), QStringLiteral("candidate_file_backed"));
}

void PageHostTests::preferencesModelIsSharedThroughPageContext()
{
    Ui::PageHost host(*m_loader, m_preferences.model);
    Application::PreferencesModel* factoryModel = nullptr;
    auto descriptor = Ui::PageDescriptor{
        .id = *Ui::PageId::fromString(QStringLiteral("preferences-probe")),
        .label = QStringLiteral("Preferences probe"),
        .factory = [&](const Ui::PageContext& context) -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            factoryModel = &context.preferences();
            return std::make_unique<Ui::Page>(context);
        }
    };
    QVERIFY(host.registerPage(std::move(descriptor)));
    QVERIFY(host.activatePage(QStringLiteral("preferences-probe")));
    QCOMPARE(factoryModel, &m_preferences.model);
    auto* page = host.pageInstance(QStringLiteral("preferences-probe"));
    QVERIFY(page);
    QCOMPARE(&page->resourceContext().preferences(), &m_preferences.model);
    auto candidate = m_preferences.model.state();
    QVERIFY(candidate.setFontSize(Application::FontSize::Large));
    QVERIFY(m_preferences.model.apply(candidate));
    QCOMPARE(page->resourceContext().preferences().snapshot().fontSize(), Application::FontSize::Large);
}

void PageHostTests::descriptorRegistrationRetainsMetadataWithoutConstructingPages()
{
    Ui::PageHost host(*m_loader, m_preferences.model);
    PageProbe reports;
    reports.key = QStringLiteral("reports");
    reports.name = QStringLiteral("reports");

    const auto pageId = Ui::PageId::fromString(QStringLiteral(" reports "));
    QVERIFY(pageId.has_value());
    QCOMPARE(pageId->value(), QStringLiteral("reports"));

    const auto iconResourceId = Resources::ResourceId::fromString(
        QStringLiteral("document.body:Book Reports/Book Report Grading Rubric.pdf")
        );
    QVERIFY(iconResourceId.has_value());

    const Ui::PageDescriptor descriptor{
        .id = *pageId,
        .label = QStringLiteral("Reports"),
        .iconResourceId = *iconResourceId,
        .requiredPermissions = {
            QStringLiteral("reports.view"),
            QStringLiteral("workspace.read")
        },
        .factory = factoryFor(reports),
        .resourcePolicy = Ui::PageResourcePolicy::PageScoped,
        .initialDataPolicy = Ui::PageInitialDataPolicy::None
    };

    QVERIFY(host.registerPage(descriptor));
    QCOMPARE(reports.factoryCalls, 0);
    QCOMPARE(reports.constructed, 0);
    QVERIFY(!host.hasPageInstance(QStringLiteral("reports")));
    QVERIFY(!host.activePageKey().has_value());
    QCOMPARE(
        host.pageLifecycleState(*pageId),
        std::optional<Ui::PageLifecycleState>(Ui::PageLifecycleState::Declared)
        );

    const auto missingPageId = Ui::PageId::fromString(QStringLiteral("missing"));
    QVERIFY(missingPageId.has_value());
    QVERIFY(!host.pageLifecycleState(*missingPageId).has_value());

    const auto retained = host.pageDescriptor(*pageId);
    QVERIFY(retained.has_value());
    QCOMPARE(retained->id.value(), QStringLiteral("reports"));
    QCOMPARE(retained->label, QStringLiteral("Reports"));
    QVERIFY(retained->iconResourceId.has_value());
    QCOMPARE(retained->iconResourceId->value(), iconResourceId->value());
    QCOMPARE(retained->requiredPermissions, descriptor.requiredPermissions);
    QVERIFY(retained->resourcePolicy == Ui::PageResourcePolicy::PageScoped);
    QVERIFY(retained->initialDataPolicy == Ui::PageInitialDataPolicy::None);

    const std::vector<Ui::PageDescriptor> registered = host.pageDescriptors();
    QCOMPARE(registered.size(), std::size_t{1});
    QCOMPARE(registered.front().id.value(), QStringLiteral("reports"));
    QCOMPARE(reports.factoryCalls, 0);
    QCOMPARE(reports.constructed, 0);
}

void PageHostTests::invalidAndDuplicateDescriptorsAreRejected()
{
    Ui::PageHost host(*m_loader, m_preferences.model);
    PageProbe first;
    first.key = QStringLiteral("duplicate");
    first.name = QStringLiteral("first");
    PageProbe second;
    second.key = QStringLiteral("duplicate");
    second.name = QStringLiteral("second");

    QVERIFY(!Ui::PageId::fromString(QStringLiteral("   ")).has_value());

    const auto invalidId = Ui::PageId::fromString(QStringLiteral("invalid"));
    QVERIFY(invalidId.has_value());
    const auto invalid = host.registerPage(Ui::PageDescriptor{
        .id = *invalidId,
        .label = QStringLiteral("  "),
        .iconResourceId = std::nullopt,
        .requiredPermissions = {},
        .factory = factoryFor(first)
    });
    QVERIFY(!invalid.has_value());
    QCOMPARE(invalid.error().code, Ui::PageHostErrorCode::InvalidPageDescriptor);
    QCOMPARE(invalid.error().pageKey, QStringLiteral("invalid"));
    QVERIFY(!invalid.error().message.trimmed().isEmpty());
    QCOMPARE(first.factoryCalls, 0);
    QCOMPARE(first.constructed, 0);

    const auto emptyFactoryId = Ui::PageId::fromString(QStringLiteral("empty-factory"));
    QVERIFY(emptyFactoryId.has_value());
    const auto emptyFactory = host.registerPage(Ui::PageDescriptor{
        .id = *emptyFactoryId,
        .label = QStringLiteral("Empty factory"),
        .iconResourceId = std::nullopt,
        .requiredPermissions = {},
        .factory = Ui::PageFactory{}
    });
    QVERIFY(!emptyFactory.has_value());
    QCOMPARE(emptyFactory.error().code, Ui::PageHostErrorCode::InvalidFactory);
    QCOMPARE(emptyFactory.error().pageKey, QStringLiteral("empty-factory"));
    QVERIFY(!emptyFactory.error().message.trimmed().isEmpty());
    QCOMPARE(first.factoryCalls, 0);
    QCOMPARE(first.constructed, 0);

    const auto blankPermissionId = Ui::PageId::fromString(
        QStringLiteral("blank-permission")
        );
    QVERIFY(blankPermissionId.has_value());
    const auto blankPermission = host.registerPage(Ui::PageDescriptor{
        .id = *blankPermissionId,
        .label = QStringLiteral("Blank permission"),
        .iconResourceId = std::nullopt,
        .requiredPermissions = {QStringLiteral(" ")},
        .factory = factoryFor(first)
    });
    QVERIFY(!blankPermission.has_value());
    QCOMPARE(
        blankPermission.error().code,
        Ui::PageHostErrorCode::InvalidPageDescriptor
        );
    QCOMPARE(blankPermission.error().pageKey, QStringLiteral("blank-permission"));
    QVERIFY(!blankPermission.error().message.trimmed().isEmpty());
    QCOMPARE(first.factoryCalls, 0);
    QCOMPARE(first.constructed, 0);

    const auto duplicateId = Ui::PageId::fromString(QStringLiteral("duplicate"));
    QVERIFY(duplicateId.has_value());
    QVERIFY(host.registerPage(Ui::PageDescriptor{
        .id = *duplicateId,
        .label = QStringLiteral("First"),
        .iconResourceId = std::nullopt,
        .requiredPermissions = {},
        .factory = factoryFor(first)
    }));
    const auto duplicate = host.registerPage(Ui::PageDescriptor{
        .id = *duplicateId,
        .label = QStringLiteral("Second"),
        .iconResourceId = std::nullopt,
        .requiredPermissions = {},
        .factory = factoryFor(second)
    });
    QVERIFY(!duplicate.has_value());
    QCOMPARE(duplicate.error().code, Ui::PageHostErrorCode::DuplicatePageKey);
    QCOMPARE(duplicate.error().pageKey, QStringLiteral("duplicate"));
    QVERIFY(!duplicate.error().message.trimmed().isEmpty());
    QCOMPARE(first.factoryCalls, 0);
    QCOMPARE(first.constructed, 0);
    QCOMPARE(second.factoryCalls, 0);
    QCOMPARE(second.constructed, 0);
}

void PageHostTests::lifecycleStatesAreQueryableObservableAndRejectReentrantMutations()
{
    PageProbe alpha;
    alpha.key = QStringLiteral("alpha");
    alpha.name = QStringLiteral("alpha");
    PageProbe beta;
    beta.key = QStringLiteral("beta");
    beta.name = QStringLiteral("beta");
    // Keep page probes alive until the host has destroyed any retained pages.
    Ui::PageHost host(*m_loader, m_preferences.model);

    const Ui::PageId alphaId = *Ui::PageId::fromString(alpha.key);
    const Ui::PageId betaId = *Ui::PageId::fromString(beta.key);
    QVERIFY(host.registerPage(descriptorFor(alpha.key, factoryFor(alpha))));
    QVERIFY(host.registerPage(descriptorFor(beta.key, factoryFor(beta))));
    QCOMPARE(
        host.pageLifecycleState(alphaId),
        std::optional<Ui::PageLifecycleState>(Ui::PageLifecycleState::Declared)
        );
    QCOMPARE(
        host.pageLifecycleState(betaId),
        std::optional<Ui::PageLifecycleState>(Ui::PageLifecycleState::Declared)
        );

    QVector<QPair<QString, Ui::PageLifecycleState>> transitions;
    bool notificationsMatchQueriedState = true;
    bool reentrantMutationChecked = false;
    std::optional<Ui::PageHostErrorCode> reentrantError;
    QObject::connect(
        &host,
        &Ui::PageHost::pageLifecycleStateChanged,
        &host,
        [&](const QString& pageKey, const Ui::PageLifecycleState state) {
            transitions.push_back({pageKey, state});
            const auto pageId = Ui::PageId::fromString(pageKey);
            const auto currentState = pageId
                ? host.pageLifecycleState(*pageId)
                : std::nullopt;
            notificationsMatchQueriedState = notificationsMatchQueriedState
                && currentState == std::optional<Ui::PageLifecycleState>(state);

            if (!reentrantMutationChecked
                && state == Ui::PageLifecycleState::Created)
            {
                reentrantMutationChecked = true;
                const auto result = host.releasePage(pageKey);
                if (!result)
                {
                    reentrantError = result.error().code;
                }
            }
        }
        );

    QVERIFY(host.activatePage(alpha.key));
    QCOMPARE(transitions.size(), 2);
    QCOMPARE(transitions[0].first, alpha.key);
    QCOMPARE(transitions[0].second, Ui::PageLifecycleState::Created);
    QCOMPARE(transitions[1].first, alpha.key);
    QCOMPARE(transitions[1].second, Ui::PageLifecycleState::Activated);
    QCOMPARE(
        host.pageLifecycleState(alphaId),
        std::optional<Ui::PageLifecycleState>(Ui::PageLifecycleState::Activated)
        );
    QVERIFY(reentrantMutationChecked);
    QCOMPARE(reentrantError, std::optional<Ui::PageHostErrorCode>(
        Ui::PageHostErrorCode::ReentrantOperation
        ));

    QVERIFY(host.activatePage(beta.key));
    QCOMPARE(transitions.size(), 5);
    QCOMPARE(transitions[2].first, beta.key);
    QCOMPARE(transitions[2].second, Ui::PageLifecycleState::Created);
    QCOMPARE(transitions[3].first, alpha.key);
    QCOMPARE(transitions[3].second, Ui::PageLifecycleState::Suspended);
    QCOMPARE(transitions[4].first, beta.key);
    QCOMPARE(transitions[4].second, Ui::PageLifecycleState::Activated);
    QCOMPARE(
        host.pageLifecycleState(alphaId),
        std::optional<Ui::PageLifecycleState>(Ui::PageLifecycleState::Suspended)
        );

    QVERIFY(host.releasePage(alpha.key));
    QCOMPARE(transitions.size(), 6);
    QCOMPARE(transitions[5].first, alpha.key);
    QCOMPARE(transitions[5].second, Ui::PageLifecycleState::Released);
    QCOMPARE(
        host.pageLifecycleState(alphaId),
        std::optional<Ui::PageLifecycleState>(Ui::PageLifecycleState::Released)
        );

    QVERIFY(host.activatePage(alpha.key));
    QCOMPARE(transitions.size(), 9);
    QCOMPARE(transitions[6].first, alpha.key);
    QCOMPARE(transitions[6].second, Ui::PageLifecycleState::Created);
    QCOMPARE(transitions[7].first, beta.key);
    QCOMPARE(transitions[7].second, Ui::PageLifecycleState::Suspended);
    QCOMPARE(transitions[8].first, alpha.key);
    QCOMPARE(transitions[8].second, Ui::PageLifecycleState::Activated);

    QVERIFY(host.releasePage(alpha.key));
    QCOMPARE(transitions.size(), 11);
    QCOMPARE(transitions[9].first, alpha.key);
    QCOMPARE(transitions[9].second, Ui::PageLifecycleState::Suspended);
    QCOMPARE(transitions[10].first, alpha.key);
    QCOMPARE(transitions[10].second, Ui::PageLifecycleState::Released);

    QVERIFY(host.releasePage(beta.key));
    QCOMPARE(transitions.size(), 12);
    QCOMPARE(transitions[11].first, beta.key);
    QCOMPARE(transitions[11].second, Ui::PageLifecycleState::Released);
    QCOMPARE(
        host.pageLifecycleState(betaId),
        std::optional<Ui::PageLifecycleState>(Ui::PageLifecycleState::Released)
        );
    QVERIFY(notificationsMatchQueriedState);
}

void PageHostTests::registrationIsLazyAndPagesSuspendReuseReleaseAndRecreate()
{
    Ui::PageHost host(*m_loader, m_preferences.model);
    host.resize(360, 240);
    host.show();

    PageProbe alpha;
    alpha.key = QStringLiteral("alpha");
    alpha.name = QStringLiteral("alpha");
    PageProbe beta;
    beta.key = QStringLiteral("beta");
    beta.name = QStringLiteral("beta");
    QStringList lifecycleLog;
    alpha.lifecycleLog = &lifecycleLog;
    beta.lifecycleLog = &lifecycleLog;

    QVERIFY(host.registerPage(descriptorFor(
        QStringLiteral("alpha"),
        factoryFor(alpha)
        )));
    QVERIFY(host.registerPage(descriptorFor(
        QStringLiteral("beta"),
        factoryFor(beta)
        )));
    QCOMPARE(alpha.factoryCalls, 0);
    QCOMPARE(alpha.constructed, 0);
    QCOMPARE(beta.factoryCalls, 0);
    QVERIFY(!host.activePageKey().has_value());

    QVERIFY(host.activatePage(QStringLiteral("alpha")));
    QCOMPARE(alpha.factoryCalls, 1);
    QCOMPARE(alpha.constructed, 1);
    QVERIFY(alpha.pages.size() == 1 && !alpha.pages.front().isNull());
    ProbePage* const alphaFirst = alpha.pages.front();
    QCOMPARE(alpha.observedLoader, m_loader.get());
    QCOMPARE(alpha.observedPageKey, QStringLiteral("alpha"));
    QCOMPARE(alpha.observedScopeKind, Resources::ResourceScopeKind::Page);
    QCOMPARE(alpha.observedScopeLabel, QStringLiteral("alpha"));
    QCOMPARE(alpha.pageHandleScopeKind, Resources::ResourceScopeKind::Page);
    QCOMPARE(alpha.pageHandleScopeLabel, QStringLiteral("alpha"));
    QCOMPARE(alpha.featureHandleScopeKind, Resources::ResourceScopeKind::Feature);
    QCOMPARE(alpha.featureHandleScopeLabel, QStringLiteral("alpha.feature"));
    QCOMPARE(host.activePageKey(), std::optional<QString>(QStringLiteral("alpha")));
    QCOMPARE(host.pageInstance(QStringLiteral("alpha")), alphaFirst);
    QCOMPARE(alpha.events, QStringList({
        QStringLiteral("alpha.factory"),
        QStringLiteral("alpha.construct"),
        QStringLiteral("alpha.activate")
    }));
    QCOMPARE(lifecycleLog, alpha.events);

    QVERIFY(host.activatePage(QStringLiteral("alpha")));
    QCOMPARE(alpha.factoryCalls, 1);
    QCOMPARE(alpha.constructed, 1);
    QCOMPARE(alpha.activationCalls, 1);

    QVERIFY(host.activatePage(QStringLiteral("beta")));
    QCOMPARE(beta.factoryCalls, 1);
    QCOMPARE(beta.constructed, 1);
    QCOMPARE(alpha.suspensionCalls, 1);
    QVERIFY(alpha.pageResourceOpenAtSuspension);
    QVERIFY(alpha.featureResourceOpenAtSuspension);
    QVERIFY(alpha.featureResourceClearedAtSuspension);
    QCOMPARE(host.activePageKey(), std::optional<QString>(QStringLiteral("beta")));
    QVERIFY(!alphaFirst->isVisible());
    QVERIFY(beta.pages.front()->isVisible());
    QCOMPARE(host.pageInstance(QStringLiteral("alpha")), alphaFirst);
    QCOMPARE(lifecycleLog, QStringList({
        QStringLiteral("alpha.factory"),
        QStringLiteral("alpha.construct"),
        QStringLiteral("alpha.activate"),
        QStringLiteral("beta.factory"),
        QStringLiteral("beta.construct"),
        QStringLiteral("beta.activate"),
        QStringLiteral("alpha.suspend")
    }));

    QVERIFY(host.activatePage(QStringLiteral("alpha")));
    QCOMPARE(alpha.factoryCalls, 1);
    QCOMPARE(alpha.constructed, 1);
    QCOMPARE(host.pageInstance(QStringLiteral("alpha")), alphaFirst);
    QVERIFY(alphaFirst->isVisible());
    QVERIFY(!beta.pages.front()->isVisible());
    QCOMPARE(beta.suspensionCalls, 1);
    QVERIFY(beta.pageResourceOpenAtSuspension);
    QVERIFY(beta.featureResourceOpenAtSuspension);
    QVERIFY(beta.featureResourceClearedAtSuspension);
    QCOMPARE(lifecycleLog.mid(7), QStringList({
        QStringLiteral("alpha.activate"),
        QStringLiteral("beta.suspend")
    }));

    const QPointer<ProbePage> betaFirst = beta.pages.front();
    QVERIFY(host.releasePage(QStringLiteral("beta")));
    QVERIFY(betaFirst.isNull());
    QVERIFY(host.isPageRegistered(QStringLiteral("beta")));
    QVERIFY(!host.hasPageInstance(QStringLiteral("beta")));
    QCOMPARE(host.activePageKey(), std::optional<QString>(QStringLiteral("alpha")));
    QCOMPARE(beta.destroyed, 1);
    QCOMPARE(beta.suspensionCalls, 1);
    QCOMPARE(lifecycleLog.back(), QStringLiteral("beta.destroy"));
    QVERIFY(beta.pageResourceOpenAtDestruction);

    const QPointer<ProbePage> alphaFirstGuard = alphaFirst;
    QVERIFY(host.releasePage(QStringLiteral("alpha")));
    QVERIFY(alphaFirstGuard.isNull());
    QVERIFY(host.isPageRegistered(QStringLiteral("alpha")));
    QVERIFY(!host.hasPageInstance(QStringLiteral("alpha")));
    QVERIFY(!host.activePageKey().has_value());
    QVERIFY(!alphaFirstGuard);
    QCOMPARE(alpha.destroyed, 1);
    QCOMPARE(alpha.suspensionCalls, 2);
    QVERIFY(alpha.pageResourceOpenAtSuspension);
    QVERIFY(alpha.featureResourceClearedAtSuspension);
    QVERIFY(alpha.pageResourceOpenAtDestruction);
    QCOMPARE(lifecycleLog.mid(lifecycleLog.size() - 2), QStringList({
        QStringLiteral("alpha.suspend"),
        QStringLiteral("alpha.destroy")
    }));

    QVERIFY(host.activatePage(QStringLiteral("alpha")));
    QCOMPARE(alpha.factoryCalls, 2);
    QCOMPARE(alpha.constructed, 2);
    QVERIFY(alpha.pages.size() == 2 && !alpha.pages.back().isNull());
    QVERIFY(alpha.pages.back() != alphaFirstGuard);
    QCOMPARE(host.pageInstance(QStringLiteral("alpha")), alpha.pages.back().data());
    QCOMPARE(lifecycleLog.mid(lifecycleLog.size() - 3), QStringList({
        QStringLiteral("alpha.factory"),
        QStringLiteral("alpha.construct"),
        QStringLiteral("alpha.activate")
    }));
    const int finalInstanceEventStart = lifecycleLog.size();
    QVERIFY(host.releasePage(QStringLiteral("alpha")));
    QCOMPARE(alpha.destroyed, 2);
    QCOMPARE(lifecycleLog.mid(finalInstanceEventStart), QStringList({
        QStringLiteral("alpha.suspend"),
        QStringLiteral("alpha.destroy")
    }));
}

void PageHostTests::factoryAndActivationFailuresPreserveTheCurrentPage()
{
    Ui::PageHost host(*m_loader, m_preferences.model);
    host.resize(360, 240);
    host.show();

    PageProbe stable;
    stable.key = QStringLiteral("stable");
    stable.name = QStringLiteral("stable");
    PageProbe activationFailure;
    activationFailure.key = QStringLiteral("activation-failure");
    activationFailure.name = QStringLiteral("activation-failure");
    activationFailure.failActivation = true;
    PageProbe factoryPartial;
    factoryPartial.key = QStringLiteral("factory-failure");
    factoryPartial.name = QStringLiteral("factory-failure-partial");
    QStringList lifecycleLog;
    stable.lifecycleLog = &lifecycleLog;
    activationFailure.lifecycleLog = &lifecycleLog;
    factoryPartial.lifecycleLog = &lifecycleLog;

    QVERIFY(host.registerPage(descriptorFor(
        QStringLiteral("stable"),
        factoryFor(stable)
        )));
    QVERIFY(host.registerPage(descriptorFor(
        QStringLiteral("factory-failure"),
        [&factoryPartial](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            ++factoryPartial.factoryCalls;
            factoryPartial.recordEvent(
                factoryPartial.name + QStringLiteral(".factory")
                );
            [[maybe_unused]] auto partialPage =
                std::make_unique<ProbePage>(context, factoryPartial);
            return std::unexpected(Ui::PageOperationError{
                QStringLiteral("requested factory failure")
            });
        }
        )));
    QVERIFY(host.registerPage(descriptorFor(
        QStringLiteral("factory-throws"),
        [](const Ui::PageContext&) -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            throw std::runtime_error("requested factory exception");
        }
        )));
    QVERIFY(host.registerPage(descriptorFor(
        QStringLiteral("factory-null"),
        [](const Ui::PageContext&)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>{};
        }
        )));
    QVERIFY(host.registerPage(descriptorFor(
        activationFailure.key,
        factoryFor(activationFailure)
        )));

    QVERIFY(host.activatePage(QStringLiteral("stable")));
    auto* const stablePage = dynamic_cast<ProbePage*>(
        host.pageInstance(QStringLiteral("stable"))
        );
    QVERIFY(stablePage != nullptr);

    const auto factoryError = host.activatePage(QStringLiteral("factory-failure"));
    QVERIFY(!factoryError.has_value());
    QCOMPARE(factoryError.error().code, Ui::PageHostErrorCode::FactoryFailed);
    QCOMPARE(factoryError.error().pageKey, QStringLiteral("factory-failure"));
    QCOMPARE(host.activePageKey(), std::optional<QString>(QStringLiteral("stable")));
    QCOMPARE(host.pageInstance(QStringLiteral("stable")), stablePage);
    QVERIFY(!host.hasPageInstance(QStringLiteral("factory-failure")));
    QCOMPARE(factoryPartial.factoryCalls, 1);
    QCOMPARE(factoryPartial.constructed, 1);
    QCOMPARE(factoryPartial.destroyed, 1);
    QCOMPARE(factoryPartial.pages.size(), 1);
    QVERIFY(factoryPartial.pages.front().isNull());
    QVERIFY(factoryPartial.pageResourceOpenAtDestruction);

    const auto thrownFactoryError = host.activatePage(QStringLiteral("factory-throws"));
    QVERIFY(!thrownFactoryError.has_value());
    QCOMPARE(thrownFactoryError.error().code, Ui::PageHostErrorCode::FactoryFailed);
    QCOMPARE(host.activePageKey(), std::optional<QString>(QStringLiteral("stable")));
    QCOMPARE(host.pageInstance(QStringLiteral("stable")), stablePage);
    QVERIFY(!host.hasPageInstance(QStringLiteral("factory-throws")));

    const auto nullFactoryError = host.activatePage(QStringLiteral("factory-null"));
    QVERIFY(!nullFactoryError.has_value());
    QCOMPARE(nullFactoryError.error().code, Ui::PageHostErrorCode::FactoryReturnedNull);
    QCOMPARE(host.activePageKey(), std::optional<QString>(QStringLiteral("stable")));
    QCOMPARE(host.pageInstance(QStringLiteral("stable")), stablePage);

    const auto activationError = host.activatePage(activationFailure.key);
    QVERIFY(!activationError.has_value());
    QCOMPARE(activationError.error().code, Ui::PageHostErrorCode::ActivationFailed);
    QCOMPARE(activationError.error().pageKey, activationFailure.key);
    QCOMPARE(host.activePageKey(), std::optional<QString>(QStringLiteral("stable")));
    QCOMPARE(host.pageInstance(QStringLiteral("stable")), stablePage);
    QVERIFY(stablePage->isVisible());
    QVERIFY(!host.hasPageInstance(activationFailure.key));
    const auto activationFailureId = Ui::PageId::fromString(
        activationFailure.key
        );
    QVERIFY(activationFailureId.has_value());
    QCOMPARE(
        host.pageLifecycleState(*activationFailureId),
        std::optional<Ui::PageLifecycleState>(Ui::PageLifecycleState::Released)
        );
    QCOMPARE(activationFailure.factoryCalls, 1);
    QCOMPARE(activationFailure.constructed, 1);
    QCOMPARE(activationFailure.destroyed, 1);
    QCOMPARE(activationFailure.suspensionCalls, 1);
    QCOMPARE(activationFailure.pages.size(), 1);
    QVERIFY(activationFailure.pageResourceOpenAtSuspension);
    QVERIFY(activationFailure.featureResourceOpenAtSuspension);
    QVERIFY(activationFailure.featureResourceClearedAtSuspension);
    QVERIFY(activationFailure.pageResourceOpenAtDestruction);
    QVERIFY(activationFailure.pages.front().isNull());
    QCOMPARE(stable.events, QStringList({
        QStringLiteral("stable.factory"),
        QStringLiteral("stable.construct"),
        QStringLiteral("stable.activate")
    }));
    QVERIFY(!lifecycleLog.contains(QStringLiteral("stable.suspend")));
    QVERIFY(host.releasePage(QStringLiteral("stable")));
    QCOMPARE(stable.suspensionCalls, 1);
}

void PageHostTests::initialSetupPageShowsPendingAndRecoveryActions()
{
    Ui::PageHost host(*m_loader, m_preferences.model);
    Ui::InitialSetupPageState state;
    int createCalls = 0;
    int resumeCalls = 0;
    int restoreCalls = 0;
    int cancelCalls = 0;
    const Ui::InitialSetupPageActions actions{
        .state = [&state] { return state; },
        .createProfile = [&] {
            ++createCalls;
            state.setupPending = true;
            state.profileLocation = QStringLiteral("/profiles/class.tps");
        },
        .resumeSetup = [&] { ++resumeCalls; },
        .restoreSetup = [&] { ++restoreCalls; },
        .cancelSetup = [&] {
            ++cancelCalls;
            state.setupPending = false;
        }
    };
    const Ui::PageDescriptor descriptor{
        .id = *Ui::PageId::fromString(QStringLiteral("initial_setup")),
        .label = QStringLiteral("Initial Setup"),
        .factory = [actions](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::InitialPage>(context, actions)
                );
        }
    };
    QVERIFY(host.registerPage(descriptor));
    QVERIFY(host.activatePage(QStringLiteral("initial_setup")));

    auto* const page = dynamic_cast<Ui::InitialPage*>(
        host.pageInstance(QStringLiteral("initial_setup"))
        );
    QVERIFY(page);
    auto* const createButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupCreateProfile")
        );
    auto* const resumeButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupResume")
        );
    auto* const restoreButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupRestore")
        );
    auto* const cancelButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupCancel")
        );
    auto* const profileLabel = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupProfileLocation")
        );
    QVERIFY(createButton);
    QVERIFY(resumeButton);
    QVERIFY(restoreButton);
    QVERIFY(cancelButton);
    QVERIFY(profileLabel);

    QVERIFY(!createButton->isHidden());
    QVERIFY(resumeButton->isHidden());
    QVERIFY(restoreButton->isHidden());
    QVERIFY(cancelButton->isHidden());
    QVERIFY(page->styleSheet().isEmpty());

    createButton->click();
    QCOMPARE(createCalls, 1);
    QVERIFY(createButton->isHidden());
    QVERIFY(!cancelButton->isHidden());
    QVERIFY(profileLabel->text().contains(QStringLiteral("/profiles/class.tps")));
    cancelButton->click();
    QCOMPARE(cancelCalls, 1);

    state = Ui::InitialSetupPageState{
        .recoveryAvailable = true,
        .recoveryHasOriginalProfile = true,
        .recoveryProfileLocation = QStringLiteral("/profiles/recover.tps")
    };
    page->refreshSetupState();
    QVERIFY(createButton->isHidden());
    QVERIFY(!resumeButton->isHidden());
    QVERIFY(!restoreButton->isHidden());
    QVERIFY(cancelButton->isHidden());
    QCOMPARE(restoreButton->text(), QStringLiteral("Restore Previous Profile"));
    resumeButton->click();
    restoreButton->click();
    QCOMPARE(resumeCalls, 1);
    QCOMPARE(restoreCalls, 1);

    state.recoveryHasOriginalProfile = false;
    page->refreshSetupState();
    QCOMPARE(restoreButton->text(), QStringLiteral("Discard Incomplete Profile"));
}

void PageHostTests::initialSetupTeacherEntryValidatesAndRetainsFailedForm()
{
    Ui::PageHost host(*m_loader, m_preferences.model);
    Ui::InitialSetupPageState state;
    state.setupPending = true;
    int choiceReads = 0;
    int createCalls = 0;
    std::optional<Application::TeacherCreateRequest> lastRequest;
    const auto teacherId = Domain::TeacherId::fromString("73");
    QVERIFY(teacherId.has_value());

    Application::PersonalDetailsSaveRequest personalDetails;
    personalDetails.name = "Setup Owner";
    const Ui::InitialSetupPageActions actions{
        .state = [&state] { return state; },
        .loadPersonalDetails = [personalDetails] {
            return Application::PersonalDetailsReadResult::success(
                personalDetails
                );
        },
        .loadTeacherChoices = [&choiceReads, teacherId, &createCalls] {
            ++choiceReads;
            if (createCalls > 1)
            {
                return Application::InitialSetupTeacherChoicesResult::success({
                    .teachers = {{.teacherId = *teacherId}}
                });
            }
            return Application::InitialSetupTeacherChoicesResult::success({});
        },
        .createTeacher = [&createCalls, &lastRequest, teacherId](
            const Application::TeacherCreateRequest& request
            ) {
            ++createCalls;
            lastRequest = request;
            if (createCalls == 1)
            {
                return Application::TeacherCreateResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = "fixture insert failure",
                    .recoverable = true
                });
            }
            return Application::TeacherCreateResult::success(*teacherId);
        }
    };
    const Ui::PageDescriptor descriptor{
        .id = *Ui::PageId::fromString(QStringLiteral("teacher-entry")),
        .label = QStringLiteral("Teacher Entry"),
        .factory = [actions](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::InitialPage>(context, actions)
                );
        }
    };
    QVERIFY(host.registerPage(descriptor));
    QVERIFY(host.activatePage(QStringLiteral("teacher-entry")));
    auto* const page = dynamic_cast<Ui::InitialPage*>(
        host.pageInstance(QStringLiteral("teacher-entry"))
        );
    QVERIFY(page);

    auto* const englishName = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherEnglishName")
        );
    auto* const koreanName = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherKoreanName")
        );
    auto* const preferredSpelling = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherPreferredSpelling")
        );
    auto* const room = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherRoom")
        );
    auto* const wifiPassword = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherWifiPassword")
        );
    auto* const status = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupTeacherEntryStatus")
        );
    auto* const continueButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupContinueTeacherEntry")
        );
    QVERIFY(englishName);
    QVERIFY(koreanName);
    QVERIFY(preferredSpelling);
    QVERIFY(room);
    QVERIFY(wifiPassword);
    QVERIFY(status);
    QVERIFY(continueButton);

    continueButton->click();
    QCOMPARE(choiceReads, 1);
    QCOMPARE(createCalls, 0);
    QVERIFY(status->text().startsWith(
        QStringLiteral("Complete at least 2 of these fields")
        ));
    QVERIFY(status->text().contains(QStringLiteral("English Name")));
    QVERIFY(status->text().contains(QStringLiteral("Korean Name")));
    QVERIFY(status->text().contains(QStringLiteral("Preferred Spelling")));

    const QString savedKoreanName = QString::fromUtf8("김선생님");
    koreanName->setText(savedKoreanName);
    preferredSpelling->setText(QStringLiteral("Kim Seonsaeng"));
    room->setText(QStringLiteral("Room 7"));
    wifiPassword->setText(QStringLiteral("  wifi secret  "));
    continueButton->click();

    QCOMPARE(createCalls, 1);
    QVERIFY(lastRequest.has_value());
    QCOMPARE(lastRequest->fields.teacherKr, std::u16string(u"김선생님"));
    QCOMPARE(lastRequest->fields.teacherEn, std::u16string{});
    QCOMPARE(
        lastRequest->fields.preferredRomanization,
        std::u16string(u"Kim Seonsaeng")
        );
    QCOMPARE(lastRequest->fields.preferredName, std::u16string(u"Kim Seonsaeng"));
    QCOMPARE(lastRequest->fields.roomNumber, std::u16string(u"Room 7"));
    QCOMPARE(lastRequest->fields.wifiPassword, std::u16string(u"  wifi secret  "));
    QCOMPARE(koreanName->text(), savedKoreanName);
    QCOMPARE(room->text(), QStringLiteral("Room 7"));
    QVERIFY(status->text().startsWith(
        QStringLiteral("The teacher could not be saved. Your entries are still here")
        ));
    QVERIFY(status->text().contains(QStringLiteral("fixture insert failure")));
    QVERIFY(state.setupPending);

    continueButton->click();
    QCOMPARE(createCalls, 2);
    QCOMPARE(koreanName->text(), savedKoreanName);
    QCOMPARE(room->text(), QStringLiteral("Room 7"));
    QCOMPARE(status->text(), QStringLiteral("Teacher saved. Setup remains pending."));
    QVERIFY(state.setupPending);
    QVERIFY(!continueButton->isEnabled());
    auto* const classTeacher = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupClassTeacher")
        );
    QVERIFY(classTeacher);
    QCOMPARE(classTeacher->currentData().toString(), QStringLiteral("73"));
    QCOMPARE(choiceReads, 2);
}

void PageHostTests::initialSetupTeacherEntrySkipsWhenAChoiceExists()
{
    Ui::PageHost host(*m_loader, m_preferences.model);
    Ui::InitialSetupPageState state;
    state.setupPending = true;
    int choiceReads = 0;
    int createCalls = 0;
    const auto teacherId = Domain::TeacherId::fromString("11");
    QVERIFY(teacherId.has_value());
    const Ui::InitialSetupPageActions actions{
        .state = [&state] { return state; },
        .loadPersonalDetails = [] {
            Application::PersonalDetailsSaveRequest details;
            details.name = "Setup Owner";
            return Application::PersonalDetailsReadResult::success(details);
        },
        .loadTeacherChoices = [&choiceReads, teacherId] {
            ++choiceReads;
            return Application::InitialSetupTeacherChoicesResult::success({
                .teachers = {{.teacherId = *teacherId}}
            });
        },
        .createTeacher = [&createCalls](
            const Application::TeacherCreateRequest&
            ) {
            ++createCalls;
            return Application::TeacherCreateResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "creation should not run",
                .recoverable = false
            });
        }
    };
    const Ui::PageDescriptor descriptor{
        .id = *Ui::PageId::fromString(QStringLiteral("teacher-entry-skip")),
        .label = QStringLiteral("Teacher Entry"),
        .factory = [actions](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::InitialPage>(context, actions)
                );
        }
    };
    QVERIFY(host.registerPage(descriptor));
    QVERIFY(host.activatePage(QStringLiteral("teacher-entry-skip")));
    auto* const page = dynamic_cast<Ui::InitialPage*>(
        host.pageInstance(QStringLiteral("teacher-entry-skip"))
        );
    QVERIFY(page);
    auto* const status = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupTeacherEntryStatus")
        );
    auto* const continueButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupContinueTeacherEntry")
        );
    QVERIFY(status);
    QVERIFY(continueButton);

    continueButton->click();

    QCOMPARE(choiceReads, 1);
    QCOMPARE(createCalls, 0);
    QCOMPARE(
        status->text(),
        QStringLiteral(
            "Continuing with the teachers already in this profile. Setup remains pending."
            )
        );
    QVERIFY(!continueButton->isEnabled());
    QVERIFY(state.setupPending);
}

void PageHostTests::initialSetupTeacherEntryOptionalOnlyDataIsNotSkipped_data()
{
    QTest::addColumn<QString>("field");
    QTest::newRow("optional-text") << QStringLiteral("wifi");
    QTest::newRow("non-default-combos") << QStringLiteral("combos");
}

void PageHostTests::initialSetupTeacherEntryOptionalOnlyDataIsNotSkipped()
{
    QFETCH(QString, field);
    Ui::PageHost host(*m_loader, m_preferences.model);
    Ui::InitialSetupPageState state;
    state.setupPending = true;
    int choiceReads = 0;
    int createCalls = 0;
    const auto teacherId = Domain::TeacherId::fromString("11");
    QVERIFY(teacherId.has_value());
    const Ui::InitialSetupPageActions actions{
        .state = [&state] { return state; },
        .loadPersonalDetails = [] {
            Application::PersonalDetailsSaveRequest details;
            details.name = "Setup Owner";
            return Application::PersonalDetailsReadResult::success(details);
        },
        .loadTeacherChoices = [&choiceReads, teacherId] {
            ++choiceReads;
            return Application::InitialSetupTeacherChoicesResult::success({
                .teachers = {{.teacherId = *teacherId}}
            });
        },
        .createTeacher = [&createCalls](const Application::TeacherCreateRequest&) {
            ++createCalls;
            return Application::TeacherCreateResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "identity validation should run first",
                .recoverable = false
            });
        }
    };
    const Ui::PageDescriptor descriptor{
        .id = *Ui::PageId::fromString(QStringLiteral("teacher-entry-optional")),
        .label = QStringLiteral("Teacher Entry"),
        .factory = [actions](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::InitialPage>(context, actions)
                );
        }
    };
    QVERIFY(host.registerPage(descriptor));
    QVERIFY(host.activatePage(QStringLiteral("teacher-entry-optional")));
    auto* const page = dynamic_cast<Ui::InitialPage*>(
        host.pageInstance(QStringLiteral("teacher-entry-optional"))
        );
    QVERIFY(page);
    auto* const wifiName = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherWifiName")
        );
    auto* const internetType = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupTeacherInternetType")
        );
    auto* const projectionType = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupTeacherProjectionType")
        );
    auto* const status = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupTeacherEntryStatus")
        );
    auto* const continueButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupContinueTeacherEntry")
        );
    QVERIFY(wifiName && internetType && projectionType && status && continueButton);

    if (field == QStringLiteral("wifi"))
    {
        wifiName->setText(QStringLiteral("Classroom WiFi"));
    }
    else
    {
        internetType->setCurrentIndex(1);
        projectionType->setCurrentIndex(1);
    }
    continueButton->click();

    QCOMPARE(choiceReads, 0);
    QCOMPARE(createCalls, 0);
    QVERIFY(status->text().startsWith(
        QStringLiteral("Complete at least 2 of these fields")
        ));
    QVERIFY(continueButton->isEnabled());
    QVERIFY(state.setupPending);
    if (field == QStringLiteral("wifi"))
    {
        QCOMPARE(wifiName->text(), QStringLiteral("Classroom WiFi"));
    }
    else
    {
        QCOMPARE(internetType->currentData().toString(), QStringLiteral("LAN"));
        QCOMPARE(projectionType->currentData().toString(), QStringLiteral("Zoom"));
    }
}

void PageHostTests::initialSetupTeacherEntryChoiceLoadFailureIsVisible()
{
    Ui::PageHost host(*m_loader, m_preferences.model);
    Ui::InitialSetupPageState state;
    state.setupPending = true;
    int choiceReads = 0;
    int createCalls = 0;
    const Ui::InitialSetupPageActions actions{
        .state = [&state] { return state; },
        .loadPersonalDetails = [] {
            Application::PersonalDetailsSaveRequest details;
            details.name = "Setup Owner";
            return Application::PersonalDetailsReadResult::success(details);
        },
        .loadTeacherChoices = [&choiceReads] {
            ++choiceReads;
            return Application::InitialSetupTeacherChoicesResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "teacher choices fixture read failure",
                .recoverable = true
            });
        },
        .createTeacher = [&createCalls](const Application::TeacherCreateRequest&) {
            ++createCalls;
            return Application::TeacherCreateResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "creation should not run",
                .recoverable = false
            });
        }
    };
    const Ui::PageDescriptor descriptor{
        .id = *Ui::PageId::fromString(QStringLiteral("teacher-entry-choice-error")),
        .label = QStringLiteral("Teacher Entry"),
        .factory = [actions](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::InitialPage>(context, actions)
                );
        }
    };
    QVERIFY(host.registerPage(descriptor));
    QVERIFY(host.activatePage(QStringLiteral("teacher-entry-choice-error")));
    auto* const page = dynamic_cast<Ui::InitialPage*>(
        host.pageInstance(QStringLiteral("teacher-entry-choice-error"))
        );
    QVERIFY(page);
    auto* const status = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupTeacherEntryStatus")
        );
    auto* const continueButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupContinueTeacherEntry")
        );
    QVERIFY(status && continueButton);

    continueButton->click();

    QCOMPARE(choiceReads, 1);
    QCOMPARE(createCalls, 0);
    QVERIFY(status->text().startsWith(
        QStringLiteral("Teacher choices could not be loaded. Retry to continue.")
        ));
    QVERIFY(status->text().contains(QStringLiteral("teacher choices fixture read failure")));
    QVERIFY(!status->text().startsWith(QStringLiteral("Complete at least")));
    QVERIFY(continueButton->isEnabled());
    QVERIFY(state.setupPending);
}

void PageHostTests::initialSetupTeacherEntryAddAnotherThenContinueSavesBoth()
{
    Ui::PageHost host(*m_loader, m_preferences.model);
    Ui::InitialSetupPageState state;
    state.setupPending = true;
    int createCalls = 0;
    QVector<Application::TeacherCreateRequest> requests;
    const auto firstTeacherId = Domain::TeacherId::fromString("80");
    const auto secondTeacherId = Domain::TeacherId::fromString("81");
    QVERIFY(firstTeacherId && secondTeacherId);
    const Ui::InitialSetupPageActions actions{
        .state = [&state] { return state; },
        .loadPersonalDetails = [] {
            Application::PersonalDetailsSaveRequest details;
            details.name = "Setup Owner";
            return Application::PersonalDetailsReadResult::success(details);
        },
        .loadTeacherChoices = [] {
            return Application::InitialSetupTeacherChoicesResult::success({});
        },
        .createTeacher = [&createCalls, &requests, firstTeacherId, secondTeacherId](
            const Application::TeacherCreateRequest& request
            ) {
            ++createCalls;
            requests.push_back(request);
            return Application::TeacherCreateResult::success(
                createCalls == 1 ? *firstTeacherId : *secondTeacherId
                );
        }
    };
    const Ui::PageDescriptor descriptor{
        .id = *Ui::PageId::fromString(QStringLiteral("teacher-entry-add-another")),
        .label = QStringLiteral("Teacher Entry"),
        .factory = [actions](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::InitialPage>(context, actions)
                );
        }
    };
    QVERIFY(host.registerPage(descriptor));
    QVERIFY(host.activatePage(QStringLiteral("teacher-entry-add-another")));
    auto* const page = dynamic_cast<Ui::InitialPage*>(
        host.pageInstance(QStringLiteral("teacher-entry-add-another"))
        );
    QVERIFY(page);
    auto* const englishName = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherEnglishName")
        );
    auto* const koreanName = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherKoreanName")
        );
    auto* const preferredSpelling = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherPreferredSpelling")
        );
    auto* const room = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherRoom")
        );
    auto* const wifiName = page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherWifiName")
        );
    auto* const internetType = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupTeacherInternetType")
        );
    auto* const projectionType = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupTeacherProjectionType")
        );
    auto* const status = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupTeacherEntryStatus")
        );
    auto* const addAnotherButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupAddAnotherTeacher")
        );
    auto* const continueButton = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupContinueTeacherEntry")
        );
    QVERIFY(englishName && koreanName && preferredSpelling && room && wifiName);
    QVERIFY(internetType && projectionType && status && addAnotherButton && continueButton);

    englishName->setText(QStringLiteral("First Teacher"));
    koreanName->setText(QString::fromUtf8("첫 번째 교사"));
    preferredSpelling->setText(QStringLiteral("First"));
    room->setText(QStringLiteral("Room A"));
    wifiName->setText(QStringLiteral("First WiFi"));
    internetType->setCurrentIndex(1);
    projectionType->setCurrentIndex(1);
    addAnotherButton->click();

    QCOMPARE(createCalls, 1);
    QCOMPARE(requests.size(), 1);
    QCOMPARE(requests[0].fields.teacherEn, std::u16string(u"First Teacher"));
    QCOMPARE(requests[0].fields.teacherKr, std::u16string(u"첫 번째 교사"));
    QCOMPARE(requests[0].fields.roomNumber, std::u16string(u"Room A"));
    QCOMPARE(requests[0].fields.wifiName, std::u16string(u"First WiFi"));
    QCOMPARE(requests[0].fields.internetType, std::u16string(u"LAN"));
    QCOMPARE(requests[0].fields.projectionType, std::u16string(u"Zoom"));
    QCOMPARE(englishName->text(), QString{});
    QCOMPARE(koreanName->text(), QString{});
    QCOMPARE(preferredSpelling->text(), QString{});
    QCOMPARE(room->text(), QString{});
    QCOMPARE(wifiName->text(), QString{});
    QCOMPARE(internetType->currentIndex(), 0);
    QCOMPARE(projectionType->currentIndex(), 0);
    QVERIFY(addAnotherButton->isEnabled());
    QVERIFY(continueButton->isEnabled());
    QCOMPARE(
        status->text(),
        QStringLiteral("Teacher saved. Enter another teacher or continue to the next step.")
        );
    QVERIFY(state.setupPending);

    koreanName->setText(QString::fromUtf8("두 번째 교사"));
    preferredSpelling->setText(QStringLiteral("Second"));
    continueButton->click();

    QCOMPARE(createCalls, 2);
    QCOMPARE(requests.size(), 2);
    QCOMPARE(requests[1].fields.teacherEn, std::u16string{});
    QCOMPARE(requests[1].fields.teacherKr, std::u16string(u"두 번째 교사"));
    QCOMPARE(requests[1].fields.preferredRomanization, std::u16string(u"Second"));
    QCOMPARE(requests[1].fields.preferredName, std::u16string(u"Second"));
    QCOMPARE(status->text(), QStringLiteral("Teacher saved. Setup remains pending."));
    QVERIFY(state.setupPending);
    QVERIFY(!addAnotherButton->isEnabled());
    QVERIFY(!continueButton->isEnabled());
}

void PageHostTests::initialSetupClassDetailsChoicesValidationColorAndDraft()
{
    Ui::PageHost host(*m_loader, m_preferences.model);
    Ui::InitialSetupPageState state;
    state.setupPending = true;
    int teacherChoiceReads = 0;
    int paletteReads = 0;
    QVector<Application::CustomColorPalette> savedPalettes;
    std::optional<Application::InitialSetupClassDetailsDraft> capturedDraft;
    bool paletteWriteFails = true;
    const auto teacherChoices = twoTeachers();
    const auto defaults = Application::defaultCustomColorPalette();
    const Ui::InitialSetupPageActions actions{
        .state = [&state] { return state; },
        .loadPersonalDetails = [] {
            Application::PersonalDetailsSaveRequest details;
            details.name = "Setup Owner";
            return Application::PersonalDetailsReadResult::success(details);
        },
        .loadTeacherChoices = [&teacherChoiceReads, teacherChoices] {
            ++teacherChoiceReads;
            if (teacherChoiceReads == 1)
            {
                return Application::InitialSetupTeacherChoicesResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = "class-details teacher read failed",
                    .recoverable = true
                });
            }
            return Application::InitialSetupTeacherChoicesResult::success(
                teacherChoices
                );
        },
        .createTeacher = [](const Application::TeacherCreateRequest&) {
            const auto teacherId = Domain::TeacherId::fromString("25");
            Q_ASSERT(teacherId.has_value());
            return Application::TeacherCreateResult::success(*teacherId);
        },
        .readClassColorPalette = [&paletteReads, defaults] {
            ++paletteReads;
            if (paletteReads == 1)
            {
                return Domain::Result<Application::CustomColorPalette>::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = "palette read failed",
                    .recoverable = true
                });
            }
            return Domain::Result<Application::CustomColorPalette>::success(
                defaults
                );
        },
        .saveClassColorPalette = [&savedPalettes, &paletteWriteFails](
            const Application::CustomColorPalette& palette
            ) {
            savedPalettes.push_back(palette);
            if (paletteWriteFails)
            {
                return Domain::Result<void>::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = "palette write failed",
                    .recoverable = true
                });
            }
            return Domain::Result<void>::success();
        },
        .captureClassDetailsDraft = [&capturedDraft](
            const Application::InitialSetupClassDetailsDraft& draft
            ) {
            capturedDraft = draft;
            return Application::InitialSetupClassDetailsDraftCaptureResult::success();
        }
    };
    const Ui::PageDescriptor descriptor{
        .id = *Ui::PageId::fromString(QStringLiteral("class-details-flow")),
        .label = QStringLiteral("Class Details"),
        .factory = [actions](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::InitialPage>(context, actions)
                );
        }
    };
    QVERIFY(host.registerPage(descriptor));
    QVERIFY(host.activatePage(QStringLiteral("class-details-flow")));
    auto* const page = dynamic_cast<Ui::InitialPage*>(
        host.pageInstance(QStringLiteral("class-details-flow"))
        );
    QVERIFY(page);

    auto* const continueTeacherEntry = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupContinueTeacherEntry")
        );
    auto* const classDetailsGroup = page->findChild<QGroupBox*>(
        QStringLiteral("classMngrNextInitialSetupClassDetails")
        );
    auto* const classTeacher = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupClassTeacher")
        );
    auto* const retryTeachers = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupClassTeacherRetry")
        );
    auto* const classDetailsStatus = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupClassDetailsStatus")
        );
    auto* const validation = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupClassDetailsValidation")
        );
    auto* const grade = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupClassGrade")
        );
    auto* const level = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupClassLevel")
        );
    auto* const readingBook = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupReadingBook")
        );
    auto* const essayBook = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupEssayBook")
        );
    auto* const chooseColor = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupClassColorChoose")
        );
    auto* const colorPreview = page->findChild<QLabel*>(
        QStringLiteral("classMngrNextInitialSetupClassColorPreview")
        );
    auto* const continueClassDetails = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupClassDetailsContinue")
        );
    QVERIFY(continueTeacherEntry && classDetailsGroup && classTeacher);
    QVERIFY(retryTeachers && classDetailsStatus && validation && grade && level);
    QVERIFY(readingBook && essayBook && chooseColor && colorPreview);
    QVERIFY(continueClassDetails);
    QVERIFY(classDetailsGroup->isHidden());

    QCOMPARE(grade->currentText(), QStringLiteral("E4"));
    QCOMPARE(level->currentText(), QStringLiteral("Theseus"));
    QVERIFY(readingBook->currentText().isEmpty());
    QVERIFY(essayBook->currentText().isEmpty());
    QVERIFY(essayBook->findText(QStringLiteral("4A")) >= 0);
    QVERIFY(readingBook->findText(QStringLiteral("Reading Explorer 1")) >= 0);

    page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherEnglishName")
        )->setText(QStringLiteral("Alex Kim"));
    page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherKoreanName")
        )->setText(QString::fromUtf8("김선생님"));
    page->findChild<QLineEdit*>(
        QStringLiteral("classMngrNextInitialSetupTeacherPreferredSpelling")
        )->setText(QStringLiteral("Alex"));
    continueTeacherEntry->click();
    QCOMPARE(teacherChoiceReads, 1);
    QVERIFY(!classDetailsGroup->isHidden());
    QVERIFY(!classTeacher->isEnabled());
    QVERIFY(!retryTeachers->isHidden());
    QVERIFY(classDetailsStatus->text().contains(
        QStringLiteral("Teacher choices could not be loaded. Retry to continue.")
        ));
    QVERIFY(classDetailsStatus->text().contains(
        QStringLiteral("class-details teacher read failed")
        ));
    QCOMPARE(classTeacher->currentData().toString(), QString{});

    retryTeachers->click();
    QCOMPARE(teacherChoiceReads, 2);
    QCOMPARE(classTeacher->count(), 3);
    QCOMPARE(classTeacher->currentIndex(), 0);
    QVERIFY(classTeacher->isEnabled());
    QVERIFY(retryTeachers->isHidden());
    QVERIFY(classDetailsStatus->text().isEmpty());

    continueClassDetails->click();
    QVERIFY(validation->text().contains(QStringLiteral("Korean Teacher is required.")));
    QVERIFY(validation->text().contains(QStringLiteral("Class Color is required.")));
    QVERIFY(!validation->text().contains(QStringLiteral("Grade is required.")));
    QVERIFY(!validation->text().contains(QStringLiteral("Level is required.")));

    classTeacher->setCurrentIndex(2);
    completeNextColorDialog(QColor(QStringLiteral("#123456")), true);
    chooseColor->click();
    QCOMPARE(paletteReads, 1);
    QVERIFY(savedPalettes.isEmpty());
    QVERIFY(colorPreview->styleSheet().contains(
        QStringLiteral("background-color:#123456;color:#FFFFFF")
        ));

    completeNextColorDialog(QColor(QStringLiteral("#223344")), true);
    chooseColor->click();
    QCOMPARE(paletteReads, 2);
    QCOMPARE(savedPalettes.size(), 1);
    QCOMPARE(savedPalettes.front().hexColors.size(), std::size_t(16));
    QVERIFY(classDetailsStatus->text().contains(
        QStringLiteral("Unable to save color preference: palette write failed")
        ));
    QVERIFY(colorPreview->styleSheet().contains(
        QStringLiteral("background-color:#223344;color:#FFFFFF")
        ));

    paletteWriteFails = false;
    completeNextColorDialog(QColor(QStringLiteral("#FFFFFF")), false);
    chooseColor->click();
    QCOMPARE(paletteReads, 3);
    QCOMPARE(savedPalettes.size(), 2);
    QCOMPARE(savedPalettes.back().hexColors.size(), std::size_t(16));
    QCOMPARE(classDetailsStatus->text(), QStringLiteral(
        "Unable to save color preference: palette write failed"
        ));
    QVERIFY(colorPreview->styleSheet().contains(
        QStringLiteral("background-color:#223344;color:#FFFFFF")
        ));

    continueClassDetails->click();
    QVERIFY(capturedDraft.has_value());
    QCOMPARE(capturedDraft->koreanTeacherId.value(), std::string("12"));
    QVERIFY(capturedDraft->classColorChosen);
    QCOMPARE(capturedDraft->classColor, std::u16string(u"#223344"));
    QCOMPARE(capturedDraft->fontColor, std::u16string(u"#FFFFFF"));
    QCOMPARE(capturedDraft->classGrade, std::u16string(u"E4"));
    QCOMPARE(capturedDraft->classLevel, std::u16string(u"Theseus"));
    QVERIFY(capturedDraft->readingBook.empty());
    QVERIFY(capturedDraft->essayBook.empty());
    QVERIFY(classDetailsStatus->text().contains(
        QStringLiteral("Class details are ready for Class Times. Setup remains pending.")
        ));
    QVERIFY(state.setupPending);
}

void PageHostTests::initialSetupClassDetailsAutoSelectsOnlyOneTeacher()
{
    Ui::PageHost host(*m_loader, m_preferences.model);
    Ui::InitialSetupPageState state;
    state.setupPending = true;
    int choiceReads = 0;
    const auto teacherId = Domain::TeacherId::fromString("73");
    QVERIFY(teacherId);
    const Ui::InitialSetupPageActions actions{
        .state = [&state] { return state; },
        .loadPersonalDetails = [] {
            return Application::PersonalDetailsReadResult::success({});
        },
        .loadTeacherChoices = [&choiceReads, teacherId] {
            ++choiceReads;
            return Application::InitialSetupTeacherChoicesResult::success({
                .teachers = {{
                    .teacherId = *teacherId,
                    .teacherEn = u"One Teacher",
                    .preferredName = u"One"
                }}
            });
        }
    };
    const Ui::PageDescriptor descriptor{
        .id = *Ui::PageId::fromString(QStringLiteral("class-details-one-teacher")),
        .label = QStringLiteral("Class Details"),
        .factory = [actions](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::InitialPage>(context, actions)
                );
        }
    };
    QVERIFY(host.registerPage(descriptor));
    QVERIFY(host.activatePage(QStringLiteral("class-details-one-teacher")));
    auto* const page = dynamic_cast<Ui::InitialPage*>(
        host.pageInstance(QStringLiteral("class-details-one-teacher"))
        );
    QVERIFY(page);
    auto* const continueTeacherEntry = page->findChild<QPushButton*>(
        QStringLiteral("classMngrNextInitialSetupContinueTeacherEntry")
        );
    auto* const classTeacher = page->findChild<QComboBox*>(
        QStringLiteral("classMngrNextInitialSetupClassTeacher")
        );
    QVERIFY(continueTeacherEntry && classTeacher);

    continueTeacherEntry->click();
    QCOMPARE(choiceReads, 1);
    QCOMPARE(classTeacher->count(), 1);
    QCOMPARE(classTeacher->currentData().toString(), QStringLiteral("73"));
}

QTEST_MAIN(PageHostTests)

#include "page_host_tests.moc"
