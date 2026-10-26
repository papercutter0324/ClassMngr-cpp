#include "next/application/theme_preferences_port.h"
#include "next/application/user_preferences_state.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/resources/resource_scope.h"
#include "next/ui/theme_presenter.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString readTextFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

class ApplicationStyleGuard final
{
public:
    explicit ApplicationStyleGuard(QApplication& application)
        : m_application(application)
        , m_stylesheet(application.styleSheet())
    {
    }

    ~ApplicationStyleGuard()
    {
        m_application.setStyleSheet(m_stylesheet);
    }

private:
    QApplication& m_application;
    QString m_stylesheet;
};

class ThemeResourceFixture final
{
public:
    ThemeResourceFixture()
        : m_resourceRoot(QDir(m_temporaryDirectory.path()).filePath(
            QStringLiteral("resources")
            ))
    {
    }

    [[nodiscard]] bool initialize(
        const bool includeLight,
        const bool includeDark
        )
    {
        if (!m_temporaryDirectory.isValid()
            || !QDir().mkpath(m_resourceRoot)
            || (includeLight && !copyTheme(QStringLiteral("light")))
            || (includeDark && !copyTheme(QStringLiteral("dark"))))
        {
            return false;
        }

        auto catalog = Resources::ResourceCatalog::load(
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
            m_resourceRoot
            );
        if (!catalog)
        {
            return false;
        }

        auto scope = Resources::ResourceScope::create(
            Resources::ResourceScopeKind::Startup,
            QStringLiteral("theme-presenter-tests")
            );
        if (!scope)
        {
            return false;
        }

        m_scope.emplace(std::move(*scope));
        m_loader = std::make_unique<Resources::ResourceLoader>(
            std::move(*catalog)
            );
        return true;
    }

    [[nodiscard]] Resources::ResourceLoader& loader() const
    {
        return *m_loader;
    }

    [[nodiscard]] const Resources::ResourceScope& scope() const
    {
        return *m_scope;
    }

    [[nodiscard]] QString stylesheet(const QString& theme) const
    {
        return readTextFile(QDir(m_resourceRoot).filePath(
            QStringLiteral("assets/styles/%1.qss").arg(theme)
            ));
    }

private:
    QTemporaryDir m_temporaryDirectory;
    QString m_resourceRoot;
    std::optional<Resources::ResourceScope> m_scope;
    std::unique_ptr<Resources::ResourceLoader> m_loader;

    [[nodiscard]] bool copyTheme(const QString& theme) const
    {
        const QString relativePath =
            QStringLiteral("assets/styles/%1.qss").arg(theme);
        const QString sourcePath = QDir(
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)
            ).filePath(relativePath);
        const QString destinationPath = QDir(m_resourceRoot).filePath(relativePath);
        return QDir().mkpath(QFileInfo(destinationPath).absolutePath())
            && QFile::copy(sourcePath, destinationPath);
    }
};

class FakeThemePreferencesPort final
    : public Application::ThemePreferencesPort
{
public:
    mutable Application::Theme storedPreference =
        Application::Theme::SystemDefault;
    mutable int readCount = 0;
    mutable std::vector<Application::Theme> writes;
    mutable bool failWrites = false;
    mutable std::function<void(Application::Theme)> beforeWrite;

    [[nodiscard]] Application::Theme read() const override
    {
        ++readCount;
        return storedPreference;
    }

    [[nodiscard]] Domain::Result<void> write(
        const Application::Theme preference
        ) const override
    {
        writes.push_back(preference);
        if (beforeWrite)
        {
            beforeWrite(preference);
        }
        if (failWrites)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "Injected theme preference write failure.",
                .recoverable = true
            });
        }
        storedPreference = preference;
        return Domain::Result<void>::success();
    }
};

}  // namespace

class ThemePresenterTests final : public QObject
{
    Q_OBJECT

private slots:
    void allPreferencesMapExplicitlyAndPersistBeforeApplyingStyle();
    void systemDefaultUsesDarkOnlyForDarkScheme();
    void startupPreferenceAppliesWithoutWritingStorage();
    void repeatedPreferenceIsANoOp();
    void stylesheetLoadFailureKeepsAppliedThemeAndSkipsPersistence();
    void persistenceFailureKeepsAppliedTheme();
    void teardownRestoresPriorStylesheetBeforeReleasingTheme();
    void invalidPreferenceIsRejectedWithoutChangingTheme();
};

void ThemePresenterTests::allPreferencesMapExplicitlyAndPersistBeforeApplyingStyle()
{
    QApplication& application = *qApp;
    ApplicationStyleGuard styleGuard(application);
    application.setStyleSheet(QStringLiteral("initial application stylesheet"));

    ThemeResourceFixture resources;
    QVERIFY(resources.initialize(true, true));
    FakeThemePreferencesPort preferences;
    std::vector<QString> stylesheetsSeenWhilePersisting;
    preferences.beforeWrite = [&application, &stylesheetsSeenWhilePersisting](
        const Application::Theme
        ) {
        stylesheetsSeenWhilePersisting.push_back(application.styleSheet());
    };
    Ui::ThemePresenter presenter(
        application,
        resources.loader(),
        resources.scope(),
        preferences,
        [] { return Qt::ColorScheme::Light; }
        );

    struct PreferenceCase final
    {
        Application::ThemePreference preference;
        Application::Theme persistedPreference;
        QString stylesheetName;
    };
    const PreferenceCase cases[] = {
        {
            Application::ThemePreference::SystemDefault,
            Application::Theme::SystemDefault,
            QStringLiteral("light")
        },
        {
            Application::ThemePreference::Light,
            Application::Theme::Light,
            QStringLiteral("light")
        },
        {
            Application::ThemePreference::Dark,
            Application::Theme::Dark,
            QStringLiteral("dark")
        }
    };

    for (const PreferenceCase& testCase : cases)
    {
        const QString priorStylesheet = application.styleSheet();
        const Domain::Result<void> changed = presenter.changePreference(
            testCase.preference
            );
        QVERIFY(changed);
        QCOMPARE(preferences.writes.back(), testCase.persistedPreference);
        QCOMPARE(stylesheetsSeenWhilePersisting.back(), priorStylesheet);
        QCOMPARE(application.styleSheet(), resources.stylesheet(testCase.stylesheetName));
        QVERIFY(presenter.ownsActiveStylesheet());
    }
}

void ThemePresenterTests::systemDefaultUsesDarkOnlyForDarkScheme()
{
    QApplication& application = *qApp;
    ApplicationStyleGuard styleGuard(application);
    const QString priorStylesheet = QStringLiteral("prior system theme stylesheet");

    ThemeResourceFixture resources;
    QVERIFY(resources.initialize(true, true));

    struct SchemeCase final
    {
        Qt::ColorScheme scheme;
        QString stylesheetName;
    };
    const SchemeCase cases[] = {
        {Qt::ColorScheme::Dark, QStringLiteral("dark")},
        {Qt::ColorScheme::Light, QStringLiteral("light")},
        {Qt::ColorScheme::Unknown, QStringLiteral("light")}
    };

    for (const SchemeCase& testCase : cases)
    {
        application.setStyleSheet(priorStylesheet);
        FakeThemePreferencesPort preferences;
        {
            Ui::ThemePresenter presenter(
                application,
                resources.loader(),
                resources.scope(),
                preferences,
                [scheme = testCase.scheme] { return scheme; }
                );
            const Domain::Result<void> applied = presenter.applyStartupPreference(
                Application::Theme::SystemDefault
                );
            QVERIFY(applied);
            QCOMPARE(
                application.styleSheet(),
                resources.stylesheet(testCase.stylesheetName)
                );
            QVERIFY(preferences.writes.empty());
        }
        QCOMPARE(application.styleSheet(), priorStylesheet);
    }
}

void ThemePresenterTests::startupPreferenceAppliesWithoutWritingStorage()
{
    QApplication& application = *qApp;
    ApplicationStyleGuard styleGuard(application);
    application.setStyleSheet(QStringLiteral("prior startup stylesheet"));

    ThemeResourceFixture resources;
    QVERIFY(resources.initialize(true, true));
    FakeThemePreferencesPort preferences;
    preferences.storedPreference = Application::Theme::Dark;
    Ui::ThemePresenter presenter(
        application,
        resources.loader(),
        resources.scope(),
        preferences,
        [] { return Qt::ColorScheme::Light; }
        );

    const Domain::Result<void> applied = presenter.applyStartupPreference(
        preferences.read()
        );
    QVERIFY(applied);
    QCOMPARE(preferences.readCount, 1);
    QVERIFY(preferences.writes.empty());
    QCOMPARE(application.styleSheet(), resources.stylesheet(QStringLiteral("dark")));
}

void ThemePresenterTests::repeatedPreferenceIsANoOp()
{
    QApplication& application = *qApp;
    ApplicationStyleGuard styleGuard(application);
    ThemeResourceFixture resources;
    QVERIFY(resources.initialize(true, true));
    FakeThemePreferencesPort preferences;
    Ui::ThemePresenter presenter(
        application,
        resources.loader(),
        resources.scope(),
        preferences
        );

    QVERIFY(presenter.changePreference(Application::ThemePreference::Dark));
    const QString installedStylesheet = application.styleSheet();
    const std::size_t writeCount = preferences.writes.size();
    QVERIFY(presenter.changePreference(Application::ThemePreference::Dark));
    QCOMPARE(preferences.writes.size(), writeCount);
    QCOMPARE(application.styleSheet(), installedStylesheet);
}

void ThemePresenterTests::stylesheetLoadFailureKeepsAppliedThemeAndSkipsPersistence()
{
    QApplication& application = *qApp;
    ApplicationStyleGuard styleGuard(application);
    application.setStyleSheet(QStringLiteral("prior missing theme stylesheet"));

    ThemeResourceFixture resources;
    QVERIFY(resources.initialize(true, false));
    FakeThemePreferencesPort preferences;
    Ui::ThemePresenter presenter(
        application,
        resources.loader(),
        resources.scope(),
        preferences
        );

    QVERIFY(presenter.applyStartupPreference(Application::Theme::Light));
    const QString activeStylesheet = application.styleSheet();
    const Domain::Result<void> failed = presenter.changePreference(
        Application::ThemePreference::Dark
        );
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(application.styleSheet(), activeStylesheet);
    QVERIFY(preferences.writes.empty());
    QVERIFY(presenter.ownsActiveStylesheet());

    // The failed request must not replace the committed semantic preference.
    QVERIFY(presenter.changePreference(Application::ThemePreference::Light));
    QVERIFY(preferences.writes.empty());
    QCOMPARE(application.styleSheet(), activeStylesheet);
}

void ThemePresenterTests::persistenceFailureKeepsAppliedTheme()
{
    QApplication& application = *qApp;
    ApplicationStyleGuard styleGuard(application);
    application.setStyleSheet(QStringLiteral("prior persistence failure stylesheet"));

    ThemeResourceFixture resources;
    QVERIFY(resources.initialize(true, true));
    FakeThemePreferencesPort preferences;
    Ui::ThemePresenter presenter(
        application,
        resources.loader(),
        resources.scope(),
        preferences
        );

    QVERIFY(presenter.applyStartupPreference(Application::Theme::Light));
    const QString activeStylesheet = application.styleSheet();
    preferences.failWrites = true;
    const Domain::Result<void> failed = presenter.changePreference(
        Application::ThemePreference::Dark
        );
    QVERIFY(!failed);
    QCOMPARE(failed.error().message, std::string("Injected theme preference write failure."));
    QCOMPARE(application.styleSheet(), activeStylesheet);
    QCOMPARE(preferences.writes.size(), std::size_t{1});
    QCOMPARE(preferences.writes.front(), Application::Theme::Dark);

    preferences.failWrites = false;
    QVERIFY(presenter.changePreference(Application::ThemePreference::Light));
    QCOMPARE(preferences.writes.size(), std::size_t{1});
    QCOMPARE(application.styleSheet(), activeStylesheet);
}

void ThemePresenterTests::teardownRestoresPriorStylesheetBeforeReleasingTheme()
{
    QApplication& application = *qApp;
    ApplicationStyleGuard styleGuard(application);
    const QString priorStylesheet = QStringLiteral("prior teardown stylesheet");
    application.setStyleSheet(priorStylesheet);

    ThemeResourceFixture resources;
    QVERIFY(resources.initialize(true, true));
    FakeThemePreferencesPort preferences;
    {
        Ui::ThemePresenter presenter(
            application,
            resources.loader(),
            resources.scope(),
            preferences
            );
        QVERIFY(presenter.applyStartupPreference(Application::Theme::Dark));
        QVERIFY(presenter.ownsActiveStylesheet());
        QCOMPARE(application.styleSheet(), resources.stylesheet(QStringLiteral("dark")));
    }

    QCOMPARE(application.styleSheet(), priorStylesheet);
}

void ThemePresenterTests::invalidPreferenceIsRejectedWithoutChangingTheme()
{
    QApplication& application = *qApp;
    ApplicationStyleGuard styleGuard(application);
    const QString priorStylesheet = QStringLiteral("prior invalid theme stylesheet");
    application.setStyleSheet(priorStylesheet);

    ThemeResourceFixture resources;
    QVERIFY(resources.initialize(true, true));
    FakeThemePreferencesPort preferences;
    Ui::ThemePresenter presenter(
        application,
        resources.loader(),
        resources.scope(),
        preferences
        );

    const Domain::Result<void> failed = presenter.changePreference(
        static_cast<Application::ThemePreference>(99)
        );
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::InvalidInput);
    QCOMPARE(application.styleSheet(), priorStylesheet);
    QVERIFY(preferences.writes.empty());
    QVERIFY(!presenter.ownsActiveStylesheet());
}

QTEST_MAIN(ThemePresenterTests)

#include "theme_presenter_tests.moc"
