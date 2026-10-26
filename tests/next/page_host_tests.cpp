#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/resources/resource_scope.h"
#include "next/ui/page_host.h"

#include <QPointer>
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
    void registrationIsLazyAndPagesSuspendReuseReleaseAndRecreate();
    void factoryAndActivationFailuresPreserveTheCurrentPage();

private:
    std::unique_ptr<Resources::ResourceLoader> m_loader;
};

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

void PageHostTests::registrationIsLazyAndPagesSuspendReuseReleaseAndRecreate()
{
    Ui::PageHost host(*m_loader);
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

    QVERIFY(host.registerPage(QStringLiteral(" alpha "), factoryFor(alpha)));
    QVERIFY(host.registerPage(QStringLiteral("beta"), factoryFor(beta)));
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
    Ui::PageHost host(*m_loader);
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

    QVERIFY(host.registerPage(QStringLiteral("stable"), factoryFor(stable)));
    QVERIFY(host.registerPage(
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
        ));
    QVERIFY(host.registerPage(
        QStringLiteral("factory-throws"),
        [](const Ui::PageContext&) -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            throw std::runtime_error("requested factory exception");
        }
        ));
    QVERIFY(host.registerPage(
        QStringLiteral("factory-null"),
        [](const Ui::PageContext&)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>{};
        }
        ));
    QVERIFY(host.registerPage(
        activationFailure.key,
        factoryFor(activationFailure)
        ));

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

QTEST_MAIN(PageHostTests)

#include "page_host_tests.moc"
