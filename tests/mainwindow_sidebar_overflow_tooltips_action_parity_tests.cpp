#include "app/mainwindow.h"
#include "core/language_service.h"
#include "core/settingsmanager.h"
#include "ui/shared/state/option_state_keys.h"
#include "ui/shared/widgets/sidebar/sidebar.h"

#include <QAction>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QRect>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QVariant>
#include <QtTest>

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

class MainWindowSidebarOverflowTooltipsActionParityTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void actionUpdatesOverflowTooltipAndPersistsPreference();

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

    MainWindowStartupOptions startupOptions;
    startupOptions.loadMostRecentDatabase = false;

    MainWindow window(
        [](const QString&) {},
        false,
        &languageService,
        nullptr,
        std::move(startupOptions)
        );
    SidebarTooltipsRestorer tooltipRestorer(window);

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

    const QString preferenceKey = QString::fromUtf8(
        OptionKeys::SidebarTooltipsEnabled
        );

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
}

QTEST_MAIN(MainWindowSidebarOverflowTooltipsActionParityTests)

#include "mainwindow_sidebar_overflow_tooltips_action_parity_tests.moc"
