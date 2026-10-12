#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "next/domain/operation_result.h"
#include "next/platform/settings_manager_sidebar_display_preferences_port.h"
#include "ui/shared/state/option_state_keys.h"
#include "ui/shared/widgets/sidebar/sidebar.h"
#include "ui/shared/widgets/sidebar/sidebar_marquee_delegate.h"

#include <QAction>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QRect>
#include <QSettings>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QVariant>
#include <QtTest>

#include <memory>
#include <string>
#include <utility>

namespace
{
class SidebarTooltipsRestorer final
{
public:
    explicit SidebarTooltipsRestorer(MainWindow& window)
        : m_window(window)
    {
    }

    ~SidebarTooltipsRestorer()
    {
        QAction* const action = m_window.actions().showSidebarTooltips;
        if (action && action->isChecked())
        {
            action->trigger();
        }

        SettingsManager::instance().sync();
    }

    SidebarTooltipsRestorer(const SidebarTooltipsRestorer&) = delete;
    SidebarTooltipsRestorer& operator=(
        const SidebarTooltipsRestorer&
        ) = delete;

private:
    MainWindow& m_window;
};
}

class SidebarMarqueeRestorer final
{
public:
    explicit SidebarMarqueeRestorer(MainWindow& window)
        : m_action(window.actions().animateSidebarText)
        , m_originalEnabled(m_action && m_action->isChecked())
    {
    }

    ~SidebarMarqueeRestorer()
    {
        if (m_action && m_action->isChecked() != m_originalEnabled)
        {
            m_action->trigger();
        }

        SettingsManager::instance().sync();
    }

    SidebarMarqueeRestorer(const SidebarMarqueeRestorer&) = delete;
    SidebarMarqueeRestorer& operator=(
        const SidebarMarqueeRestorer&
        ) = delete;

private:
    QAction* const m_action = nullptr;
    const bool m_originalEnabled = false;
};

class MainWindowSidebarOverflowTooltipsActionParityTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void actionUpdatesOverflowTooltipAndPersistsPreference();
    void actionUpdatesMarqueeDelegateAndPersistsPreference();

private:
    QTemporaryDir m_settingsDirectory;
};

void MainWindowSidebarOverflowTooltipsActionParityTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QVERIFY(
        qputenv(
            "CLASSMNGR_SETTINGS_ROOT",
            m_settingsDirectory.path().toUtf8()
            )
        );

    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void MainWindowSidebarOverflowTooltipsActionParityTests::
actionUpdatesOverflowTooltipAndPersistsPreference()
{
    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    SettingsManager& settings = SettingsManager::instance();
    const QString preferenceKey = QString::fromUtf8(
        OptionKeys::SidebarTooltipsEnabled
        );
    const QString marqueePreferenceKey = QString::fromUtf8(
        OptionKeys::SidebarMarqueeEnabled
        );
    settings.set(preferenceKey, true);
    settings.set(marqueePreferenceKey, true);
    settings.sync();

    bool injectAccessError = false;
    int synchronizationAttempts = 0;
    const auto sidebarDisplayPreferences =
        std::make_shared<
            ClassMngr::Next::Platform::
                SettingsManagerSidebarDisplayPreferencesPort
            >(
                [&injectAccessError, &synchronizationAttempts](
                    QSettings& settings
                    )
                {
                    ++synchronizationAttempts;
                    if (injectAccessError)
                    {
                        return QSettings::AccessError;
                    }

                    settings.sync();
                    return settings.status();
                }
                );
    int diagnosticCount = 0;
    ClassMngr::Next::Domain::OperationError lastDiagnostic;

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;
    startupOptions.sidebarDisplayPreferencesPort =
        sidebarDisplayPreferences;
    startupOptions.sidebarDisplayPreferencesErrorHandler =
        [&diagnosticCount, &lastDiagnostic](
            const ClassMngr::Next::Domain::OperationError& error
            )
        {
            ++diagnosticCount;
            lastDiagnostic = error;
        };

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    SidebarTooltipsRestorer tooltipRestorer(window);
    QCOMPARE(synchronizationAttempts, 0);
    QCOMPARE(diagnosticCount, 0);

    auto* const sidebar = window.findChild<Sidebar*>(
        QStringLiteral("sidebarWidget")
        );
    QVERIFY(sidebar);

    const QString teacherName = QStringLiteral(
        "Synthetic Teacher With A Deliberately Extremely Long Name "
        "For Sidebar Overflow Tooltip Action Parity Verification"
        );
    sidebar->addTeacherNode(teacherName, 987654321, false);
    sidebar->setDatabaseSectionsVisible(true);

    QTreeWidget* const tree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    const QList<QTreeWidgetItem*> matchingItems = tree->findItems(
        teacherName,
        Qt::MatchExactly | Qt::MatchRecursive,
        0
        );
    QCOMPARE(matchingItems.size(), 1);
    QTreeWidgetItem* const teacherItem = matchingItems.constFirst();

    for (
        QTreeWidgetItem* parent = teacherItem->parent();
        parent;
        parent = parent->parent()
        )
    {
        parent->setExpanded(true);
    }

    sidebar->setFixedWidth(150);
    tree->setFixedWidth(130);
    window.show();
    QCoreApplication::processEvents();

    QCOMPARE(sidebar->width(), 150);
    QCOMPARE(tree->width(), 130);
    QVERIFY(tree->viewport());

    const QRect itemRect = tree->visualItemRect(teacherItem);
    QVERIFY(itemRect.isValid());
    const int textWidth = QFontMetrics(tree->font()).horizontalAdvance(
        teacherName
        );
    const int textLeft = qMax(0, itemRect.left());
    const int availableTextWidth = tree->viewport()->width()
        - textLeft
        - 8;
    QVERIFY(textWidth > availableTextWidth);

    QAction* const tooltipsAction = window.actions().showSidebarTooltips;
    QVERIFY(tooltipsAction);
    QVERIFY(tooltipsAction->isCheckable());
    QVERIFY(tooltipsAction->isEnabled());
    QVERIFY(tooltipsAction->isChecked());
    QCOMPARE(teacherItem->toolTip(0), teacherName);

    tooltipsAction->trigger();
    QVERIFY(!tooltipsAction->isChecked());
    SettingsManager::instance().sync();
    const QVariant disabledPreference = SettingsManager::instance().get(
        preferenceKey
        );
    QVERIFY(disabledPreference.isValid());
    QVERIFY(!disabledPreference.toBool());
    QVERIFY(teacherItem->toolTip(0).isEmpty());

    tooltipsAction->trigger();
    QVERIFY(tooltipsAction->isChecked());
    SettingsManager::instance().sync();
    const QVariant enabledPreference = SettingsManager::instance().get(
        preferenceKey
        );
    QVERIFY(enabledPreference.isValid());
    QVERIFY(enabledPreference.toBool());
    QCOMPARE(teacherItem->toolTip(0), teacherName);

    tooltipsAction->trigger();
    QVERIFY(!tooltipsAction->isChecked());
    SettingsManager::instance().sync();
    const QVariant finalPreference = SettingsManager::instance().get(
        preferenceKey
        );
    QVERIFY(finalPreference.isValid());
    QVERIFY(!finalPreference.toBool());
    QVERIFY(teacherItem->toolTip(0).isEmpty());

    tooltipsAction->trigger();
    QVERIFY(tooltipsAction->isChecked());
    QCOMPARE(synchronizationAttempts, 4);
    QCOMPARE(diagnosticCount, 0);
    QCOMPARE(settings.get(preferenceKey).toBool(), true);
    QCOMPARE(settings.get(marqueePreferenceKey).toBool(), true);
    QCOMPARE(teacherItem->toolTip(0), teacherName);

    QAction* const marqueeAction = window.actions().animateSidebarText;
    QVERIFY(marqueeAction);
    QVERIFY(marqueeAction->isChecked());
    auto* const delegate = dynamic_cast<SidebarMarqueeDelegate*>(
        tree->itemDelegate()
        );
    QVERIFY(delegate);
    QVERIFY(delegate->marqueeEnabled());

    const QVariant tooltipsBeforeFailure = settings.get(preferenceKey);
    const QVariant marqueeBeforeFailure = settings.get(marqueePreferenceKey);
    const int synchronizationAttemptsBeforeFailure =
        synchronizationAttempts;
    injectAccessError = true;
    tooltipsAction->trigger();

    QCOMPARE(
        synchronizationAttempts,
        synchronizationAttemptsBeforeFailure + 1
        );
    QCOMPARE(diagnosticCount, 1);
    QCOMPARE(
        lastDiagnostic.code,
        ClassMngr::Next::Domain::ErrorCode::Technical
        );
    QVERIFY(lastDiagnostic.recoverable);
    QVERIFY(
        lastDiagnostic.message.find("AccessError") != std::string::npos
        );
    QVERIFY(tooltipsAction->isChecked());
    QVERIFY(marqueeAction->isChecked());
    QCOMPARE(settings.get(preferenceKey), tooltipsBeforeFailure);
    QCOMPARE(settings.get(marqueePreferenceKey), marqueeBeforeFailure);
    QCOMPARE(teacherItem->toolTip(0), teacherName);
    QVERIFY(delegate->marqueeEnabled());

    injectAccessError = false;
    tooltipsAction->trigger();

    QCOMPARE(
        synchronizationAttempts,
        synchronizationAttemptsBeforeFailure + 2
        );
    QCOMPARE(diagnosticCount, 1);
    QVERIFY(!tooltipsAction->isChecked());
    QVERIFY(marqueeAction->isChecked());
    QVERIFY(!settings.get(preferenceKey).toBool());
    QVERIFY(settings.get(marqueePreferenceKey).toBool());
    QVERIFY(teacherItem->toolTip(0).isEmpty());
    QVERIFY(delegate->marqueeEnabled());
}

void MainWindowSidebarOverflowTooltipsActionParityTests::
actionUpdatesMarqueeDelegateAndPersistsPreference()
{
    const QString preferenceKey = QString::fromUtf8(
        OptionKeys::SidebarMarqueeEnabled
        );
    SettingsManager::instance().set(preferenceKey, false);
    SettingsManager::instance().sync();

    const QVariant initialPreference = SettingsManager::instance().get(
        preferenceKey
        );
    QVERIFY(initialPreference.isValid());
    QVERIFY(!initialPreference.toBool());

    LanguageService languageService;
    QVERIFY(languageService.setLanguage(Language::English));

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    SidebarMarqueeRestorer marqueeRestorer(window);

    QAction* const marqueeAction = window.actions().animateSidebarText;
    QVERIFY(marqueeAction);
    QVERIFY(marqueeAction->isCheckable());
    QVERIFY(!marqueeAction->isChecked());

    auto* const sidebar = window.findChild<Sidebar*>(
        QStringLiteral("sidebarWidget")
        );
    QVERIFY(sidebar);

    QTreeWidget* const tree = sidebar->findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    QVERIFY(tree);

    auto* const delegate = dynamic_cast<SidebarMarqueeDelegate*>(
        tree->itemDelegate()
        );
    QVERIFY(delegate);
    QVERIFY(!delegate->marqueeEnabled());

    marqueeAction->trigger();
    QVERIFY(marqueeAction->isChecked());
    QVERIFY(delegate->marqueeEnabled());
    SettingsManager::instance().sync();
    const QVariant enabledPreference = SettingsManager::instance().get(
        preferenceKey
        );
    QVERIFY(enabledPreference.isValid());
    QVERIFY(enabledPreference.toBool());

    marqueeAction->trigger();
    QVERIFY(!marqueeAction->isChecked());
    QVERIFY(!delegate->marqueeEnabled());
    SettingsManager::instance().sync();
    const QVariant disabledPreference = SettingsManager::instance().get(
        preferenceKey
        );
    QVERIFY(disabledPreference.isValid());
    QVERIFY(!disabledPreference.toBool());
}

QTEST_MAIN(MainWindowSidebarOverflowTooltipsActionParityTests)

#include "mainwindow_sidebar_overflow_tooltips_action_parity_tests.moc"
