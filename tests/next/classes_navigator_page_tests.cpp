#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/classes_navigator_page.h"
#include "next/ui/page_host.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QComboBox>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QTabBar>
#include <QToolButton>
#include <QTranslator>
#include <QtTest/QtTest>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

Application::ClassesNavigationEntry navigationEntry(
    const int id,
    const QString& name,
    const QString& grade,
    const QString& level,
    const QString& regularDay = {},
    const QString& regularStart = {},
    const QString& intensiveDay = {},
    const QString& intensiveStart = {},
    const QString& teacherEnglish = {},
    const QString& teacherKorean = {}
    )
{
    Application::ClassesNavigationDetails details{
        .grade = grade.toStdU16String(),
        .level = level.toStdU16String(),
        .teacherEnglishName = teacherEnglish.toStdU16String(),
        .teacherKoreanName = teacherKorean.toStdU16String()
    };
    if (!regularDay.isEmpty())
    {
        details.regularSchedule.push_back({
            .day = regularDay.toStdU16String(),
            .startTime = regularStart.toStdU16String(),
            .endTime = (regularStart + QStringLiteral(" + 50m")).toStdU16String()
        });
    }
    if (!intensiveDay.isEmpty())
    {
        details.intensiveSchedule.push_back({
            .day = intensiveDay.toStdU16String(),
            .startTime = intensiveStart.toStdU16String(),
            .endTime = (intensiveStart + QStringLiteral(" + 50m")).toStdU16String()
        });
    }
    return {
        .classId = classId(id),
        .className = name.toStdU16String(),
        .details = Application::ClassesNavigationDetailsResult::success(
            std::move(details)
            )
    };
}

Application::ClassesListSnapshot classList(
    const std::vector<std::pair<int, QString>>& rows
    )
{
    Application::ClassesListSnapshot result;
    result.classes.reserve(rows.size());
    for (const auto& [id, name] : rows)
    {
        result.classes.push_back({
            .classId = classId(id),
            .className = name.toStdU16String()
        });
    }
    return result;
}

Application::ClassesNavigationSnapshot navigation(
    std::vector<Application::ClassesNavigationEntry> rows
    )
{
    return {.classes = std::move(rows)};
}

class EmptyPage final : public Ui::Page
{
public:
    using Ui::Page::Page;
};

struct ReadState final
{
    Application::ClassesListSnapshot classes;
    Application::ClassesNavigationSnapshot navigation;
    Application::ClassesNavigatorPreferences preferences{
        .scheduleDisplayMode = Application::ScheduleDisplayMode::Regular,
        .visibilityScope = Application::ClassVisibilityScope::ActiveSchedule,
        .dayFilterResetPolicy =
            Application::ClassDayFilterResetPolicy::OnApplicationClose,
        .classSelectionResetPolicy =
            Application::ClassSelectionResetPolicy::OnApplicationClose
    };
    QString identity = QStringLiteral("workspace-a");
    QString path = QStringLiteral("workspace-a.tps");
    bool workspaceOpen = true;
    bool failFirstListRead = false;
    bool failEveryListRead = false;
    bool failFirstNavigationRead = false;
    bool failEveryNavigationRead = false;
    bool failScheduleSave = false;
    bool failVisibilitySave = false;
    int listReadCount = 0;
    int navigationReadCount = 0;
    int scheduleSaveCount = 0;
    int visibilitySaveCount = 0;
    std::vector<Application::ClassesNavigationSnapshotQuery> navigationQueries;
};

struct PageFixture final
{
    Test::MemoryPreferences preferences;
    std::unique_ptr<Resources::ResourceCatalog> catalog;
    std::unique_ptr<Resources::ResourceLoader> loader;
    std::unique_ptr<Ui::PageHost> host;
    ReadState state;

    bool initialize()
    {
        const auto loaded = Resources::ResourceCatalog::load(
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)
            );
        if (!loaded)
        {
            return false;
        }
        catalog = std::make_unique<Resources::ResourceCatalog>(std::move(*loaded));
        loader = std::make_unique<Resources::ResourceLoader>(*catalog);
        host = std::make_unique<Ui::PageHost>(*loader, preferences.model);

        Ui::PageDescriptor classes{
            .id = *Ui::PageId::fromString(QStringLiteral("classes")),
            .label = QStringLiteral("Classes"),
            .factory = [this](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                Ui::ClassesNavigatorPageActions actions;
                actions.workspaceIdentity = [this] { return state.identity; };
                actions.activeWorkspacePath = [this]() -> std::optional<QString> {
                    return state.workspaceOpen
                        ? std::optional<QString>(state.path)
                        : std::nullopt;
                };
                actions.readClasses = [this](const QString&) {
                    ++state.listReadCount;
                    if (state.failEveryListRead
                        || (state.failFirstListRead && state.listReadCount == 1))
                    {
                        return Application::ClassesListReadResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "class list source failed",
                            .recoverable = true
                        });
                    }
                    return Application::ClassesListReadResult::success(state.classes);
                };
                actions.readNavigation = [this](
                    const QString&,
                    const Application::ClassesNavigationSnapshotQuery& query
                    ) {
                    ++state.navigationReadCount;
                    state.navigationQueries.push_back(query);
                    if (state.failEveryNavigationRead
                        || (state.failFirstNavigationRead
                            && state.navigationReadCount == 1))
                    {
                        return Application::ClassesNavigationSnapshotResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "navigation metadata source failed",
                            .recoverable = true
                        });
                    }
                    return Application::ClassesNavigationSnapshotResult::success(
                        state.navigation
                        );
                };
                actions.readPreferences = [this](const QString&) {
                    return state.preferences;
                };
                actions.saveScheduleMode = [this](
                    const QString&,
                    const Application::ScheduleDisplayMode mode
                    ) {
                    ++state.scheduleSaveCount;
                    if (state.failScheduleSave)
                    {
                        return Application::ScheduleDisplayModePreferencesSaveResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "schedule source save failed",
                            .recoverable = true
                        });
                    }
                    state.preferences.scheduleDisplayMode = mode;
                    return Application::ScheduleDisplayModePreferencesSaveResult::success();
                };
                actions.saveVisibilityScope = [this](
                    const QString&,
                    const Application::ClassVisibilityScope scope
                    ) {
                    ++state.visibilitySaveCount;
                    if (state.failVisibilitySave)
                    {
                        return Application::ClassVisibilityPreferencesSaveResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "visibility save failed",
                            .recoverable = true
                        });
                    }
                    state.preferences.visibilityScope = scope;
                    return Application::ClassVisibilityPreferencesSaveResult::success();
                };
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<Ui::ClassesNavigatorPage>(
                        context,
                        std::move(actions)
                        )
                    );
            }
        };
        Ui::PageDescriptor other{
            .id = *Ui::PageId::fromString(QStringLiteral("other")),
            .label = QStringLiteral("Other"),
            .factory = [](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<EmptyPage>(context)
                    );
            }
        };
        return host->registerPage(std::move(classes))
            && host->registerPage(std::move(other));
    }

    Ui::ClassesNavigatorPage* page() const
    {
        return dynamic_cast<Ui::ClassesNavigatorPage*>(
            host ? host->pageInstance(QStringLiteral("classes")) : nullptr
            );
    }

    bool activate()
    {
        return host && host->activatePage(QStringLiteral("classes"));
    }
};

class KoreanAllTranslator final : public QTranslator
{
public:
    QString translate(
        const char* context,
        const char* sourceText,
        const char* = nullptr,
        const int = -1
        ) const override
    {
        if (QByteArray(context) == QByteArrayLiteral("NextClassesPage")
            && QByteArray(sourceText) == QByteArrayLiteral("All"))
        {
            return QStringLiteral("모두");
        }
        if (QByteArray(context) == QByteArrayLiteral("NextClassesPage")
            && QByteArray(sourceText) == QByteArrayLiteral("Classes"))
        {
            return QStringLiteral("수업");
        }
        return {};
    }
};

int widgetCount(const QWidget* root)
{
    return root ? root->findChildren<QWidget*>().size() + 1 : 0;
}

} // namespace

class ClassesNavigatorPageTests final : public QObject
{
    Q_OBJECT

private slots:
    void loadsLazilyAndKeepsAlwaysGroupedSmallListsPlusAll();
    void appliesSourceVisibilityDayAndWeekendFilters();
    void listFailureClearsProjectionAndRetryRecovers();
    void metadataFailureKeepsNameOnlyListAndRetryEnriches();
    void selectionSurvivesRefreshAndFallsBackWhenClassDisappears();
    void remembersSelectionIndependentlyForEachGradeAndAllGroup();
    void distinguishesNoWorkspaceFromEmptyClasses();
    void suspensionClearsProjectionAndHonorsResetPolicies();
    void ninetySixClassesUseOneCompactModelWithoutClassWidgets();
    void languageRefreshChangesLabelsWithoutChangingCanonicalSelection();
    void productionKoreanCatalogTranslatesNavigatorDisplay();
};

void ClassesNavigatorPageTests::
loadsLazilyAndKeepsAlwaysGroupedSmallListsPlusAll()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({
        {12, QStringLiteral("Level twelve")},
        {7, QStringLiteral("Level seven")}
    });
    fixture.state.navigation = navigation({
        navigationEntry(12, QStringLiteral("Level twelve"), QStringLiteral("E5"),
            QStringLiteral("Artemis"), QStringLiteral("Tuesday"), QStringLiteral("10:00")),
        navigationEntry(7, QStringLiteral("Level seven"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"), QStringLiteral("09:00"))
    });

    QVERIFY(!fixture.host->hasPageInstance(QStringLiteral("classes")));
    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    QCOMPARE(fixture.state.listReadCount, 1);
    QCOMPARE(fixture.state.navigationReadCount, 1);
    QCOMPARE(fixture.state.navigationQueries.front().classes.size(), std::size_t(2));
    QCOMPARE(page->model()->classCount(), 2);
    QCOMPARE(page->model()->visibleClassCount(), 2);
    QCOMPARE(page->model()->gradeGroupCount(), 2);
    QCOMPARE(page->gradeTabs()->count(), 3);
    QCOMPARE(page->gradeTabs()->tabText(0), QStringLiteral("E4"));
    QCOMPARE(page->gradeTabs()->tabText(1), QStringLiteral("E5"));
    QCOMPARE(page->gradeTabs()->tabText(2), QStringLiteral("All"));
    QCOMPARE(page->model()->currentGroup(),
        Ui::ClassesNavigatorModel::allClassesGroupKey());
    QVERIFY(page->selectedClassId().has_value());
    QCOMPARE(page->selectedClassId()->value(), std::string("7"));
    QCOMPARE(page->classList()->model()->rowCount(), 2);
}

void ClassesNavigatorPageTests::
appliesSourceVisibilityDayAndWeekendFilters()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({
        {1, QStringLiteral("Regular and weekend")},
        {2, QStringLiteral("Intensive Tuesday")},
        {3, QStringLiteral("No schedule")}
    });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("Regular and weekend"),
            QStringLiteral("E4"), QStringLiteral("Theseus"),
            QStringLiteral("Monday"), QStringLiteral("09:00"),
            QStringLiteral("Saturday"), QStringLiteral("10:00")),
        navigationEntry(2, QStringLiteral("Intensive Tuesday"),
            QStringLiteral("E4"), QStringLiteral("Perseus"), {}, {},
            QStringLiteral("Tuesday"), QStringLiteral("11:00")),
        navigationEntry(3, QStringLiteral("No schedule"),
            QStringLiteral("E5"), QStringLiteral("Artemis"))
    });

    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    QCOMPARE(page->model()->visibleClassCount(), 1);
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QVERIFY(page->weekendAvailable());
    auto* weekend = page->findChild<QToolButton*>(
        QStringLiteral("classesNavigatorWeekendFilter"));
    QVERIFY(weekend);
    QVERIFY(!weekend->isHidden());

    page->scheduleSourceCombo()->setCurrentIndex(1);
    QCOMPARE(fixture.state.scheduleSaveCount, 1);
    QCOMPARE(page->model()->visibleClassCount(), 2);
    auto* tuesday = page->findChild<QToolButton*>(
        QStringLiteral("classesNavigatorTuesdayFilter"));
    QVERIFY(tuesday);
    tuesday->click();
    QCOMPARE(page->model()->visibleClassCount(), 1);
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));

    weekend->click();
    QCOMPARE(page->model()->visibleClassCount(), 2);
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));

    page->visibilityScopeCombo()->setCurrentIndex(0);
    QCOMPARE(fixture.state.visibilitySaveCount, 1);
    QCOMPARE(page->model()->visibleClassCount(), 2);
    tuesday->click();
    QCOMPARE(page->model()->visibleClassCount(), 1);
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    weekend->click();
    QCOMPARE(page->model()->visibleClassCount(), 3);
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));

    page->scheduleSourceCombo()->setCurrentIndex(0);
    QCOMPARE(page->model()->visibleClassCount(), 3);
    QCOMPARE(fixture.state.scheduleSaveCount, 2);
}

void ClassesNavigatorPageTests::
listFailureClearsProjectionAndRetryRecovers()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.failFirstListRead = true;
    fixture.state.classes = classList({{1, QStringLiteral("One")} });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("One"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"), QStringLiteral("09:00"))
    });

    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    QCOMPARE(page->model()->classCount(), 0);
    QVERIFY(page->findChild<QLabel*>(QStringLiteral("classesNavigatorStatus"))
        ->text().contains(QStringLiteral("could not be loaded")));
    QVERIFY(!page->findChild<QToolButton*>(QStringLiteral("classesNavigatorRetry"))
        ->isHidden());

    page->findChild<QToolButton*>(QStringLiteral("classesNavigatorRetry"))->click();
    QCOMPARE(fixture.state.listReadCount, 2);
    QCOMPARE(fixture.state.navigationReadCount, 1);
    QCOMPARE(page->model()->classCount(), 1);
    QCOMPARE(page->model()->visibleClassCount(), 1);
}

void ClassesNavigatorPageTests::
metadataFailureKeepsNameOnlyListAndRetryEnriches()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.failFirstNavigationRead = true;
    fixture.state.classes = classList({
        {11, QStringLiteral("Alpha")},
        {22, QStringLiteral("Beta")}
    });
    fixture.state.navigation = navigation({
        navigationEntry(11, QStringLiteral("Alpha"), QStringLiteral("E5"),
            QStringLiteral("Artemis"), QStringLiteral("Tuesday"), QStringLiteral("10:00")),
        navigationEntry(22, QStringLiteral("Beta"), QStringLiteral("E4"),
            QStringLiteral("Perseus"), QStringLiteral("Monday"), QStringLiteral("09:00"))
    });

    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    QVERIFY(!page->navigationMetadataAvailable());
    QCOMPARE(page->model()->classCount(), 2);
    QCOMPARE(page->model()->visibleClassCount(), 2);
    QCOMPARE(page->model()->gradeGroupCount(), 1);
    QCOMPARE(page->model()->data(page->model()->index(0, 0)).toString(),
        QStringLiteral("Alpha"));
    QVERIFY(!page->weekendAvailable());
    auto* const status = page->findChild<QLabel*>(
        QStringLiteral("classesNavigatorStatus"));
    QVERIFY(status->text().contains(QStringLiteral("Showing class names")));
    QVERIFY(status->text().contains(QStringLiteral("navigation metadata source failed")));

    page->findChild<QToolButton*>(QStringLiteral("classesNavigatorRetry"))->click();
    QVERIFY(page->navigationMetadataAvailable());
    QCOMPARE(fixture.state.listReadCount, 2);
    QCOMPARE(fixture.state.navigationReadCount, 2);
    QCOMPARE(page->model()->gradeGroupCount(), 2);
}

void ClassesNavigatorPageTests::
selectionSurvivesRefreshAndFallsBackWhenClassDisappears()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({
        {1, QStringLiteral("First")},
        {2, QStringLiteral("Second")}
    });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"), QStringLiteral("09:00")),
        navigationEntry(2, QStringLiteral("Second"), QStringLiteral("E4"),
            QStringLiteral("Perseus"), QStringLiteral("Tuesday"), QStringLiteral("10:00"))
    });

    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    const QModelIndex second = page->model()->indexForClassId(classId(2));
    QVERIFY(second.isValid());
    page->classList()->setCurrentIndex(second);
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));
    QVERIFY(page->refreshFromStore());
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));

    fixture.state.classes = classList({{1, QStringLiteral("First")} });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"), QStringLiteral("09:00"))
    });
    QVERIFY(page->refreshFromStore());
    QVERIFY(page->selectedClassId().has_value());
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
}

void ClassesNavigatorPageTests::
remembersSelectionIndependentlyForEachGradeAndAllGroup()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({
        {1, QStringLiteral("E4 One")},
        {2, QStringLiteral("E4 Two")},
        {3, QStringLiteral("E5 Three")},
        {4, QStringLiteral("E5 Four")}
    });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("E4 One"), QStringLiteral("E4"),
            QStringLiteral("One"), QStringLiteral("Monday"), QStringLiteral("09:00")),
        navigationEntry(2, QStringLiteral("E4 Two"), QStringLiteral("E4"),
            QStringLiteral("Two"), QStringLiteral("Tuesday"), QStringLiteral("10:00")),
        navigationEntry(3, QStringLiteral("E5 Three"), QStringLiteral("E5"),
            QStringLiteral("Three"), QStringLiteral("Wednesday"), QStringLiteral("11:00")),
        navigationEntry(4, QStringLiteral("E5 Four"), QStringLiteral("E5"),
            QStringLiteral("Four"), QStringLiteral("Thursday"), QStringLiteral("12:00"))
    });

    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    page->visibilityScopeCombo()->setCurrentIndex(
        page->visibilityScopeCombo()->findData(
            static_cast<int>(Application::ClassVisibilityScope::AllClasses)));
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));

    const auto activateTab = [page](const QString& key) {
        for (int index = 0; index < page->gradeTabs()->count(); ++index)
        {
            if (page->gradeTabs()->tabData(index).toString() == key)
            {
                page->gradeTabs()->setCurrentIndex(index);
                return true;
            }
        }
        return false;
    };
    QVERIFY(activateTab(QStringLiteral("grade:E4")));
    page->classList()->setCurrentIndex(
        page->model()->indexForClassId(classId(2)));
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));
    QVERIFY(activateTab(QStringLiteral("grade:E5")));
    QCOMPARE(page->selectedClassId()->value(), std::string("3"));
    page->classList()->setCurrentIndex(
        page->model()->indexForClassId(classId(4)));
    QCOMPARE(page->selectedClassId()->value(), std::string("4"));

    QVERIFY(activateTab(QStringLiteral("grade:E4")));
    QCOMPARE(page->selectedClassId()->value(), std::string("2"));
    QVERIFY(activateTab( Ui::ClassesNavigatorModel::allClassesGroupKey()));
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    page->classList()->setCurrentIndex(
        page->model()->indexForClassId(classId(3)));
    QCOMPARE(page->selectedClassId()->value(), std::string("3"));
    QVERIFY(activateTab(QStringLiteral("grade:E5")));
    QCOMPARE(page->selectedClassId()->value(), std::string("4"));
    QVERIFY(activateTab(Ui::ClassesNavigatorModel::allClassesGroupKey()));
    QCOMPARE(page->selectedClassId()->value(), std::string("3"));
}

void ClassesNavigatorPageTests::
distinguishesNoWorkspaceFromEmptyClasses()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.workspaceOpen = false;
    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    QCOMPARE(page->model()->classCount(), 0);
    QCOMPARE(page->findChild<QLabel*>(QStringLiteral("classesNavigatorStatus"))
            ->text(),
        QStringLiteral("No Teacher Profile is open."));
    QCOMPARE(fixture.state.listReadCount, 0);

    fixture.state.workspaceOpen = true;
    fixture.state.identity = QStringLiteral("empty-workspace");
    fixture.state.classes = {};
    QVERIFY(page->refreshFromStore());
    QCOMPARE(page->model()->classCount(), 0);
    QCOMPARE(page->findChild<QLabel*>(QStringLiteral("classesNavigatorStatus"))
            ->text(),
        QStringLiteral("No classes available."));
    QCOMPARE(fixture.state.listReadCount, 1);
    QCOMPARE(fixture.state.navigationReadCount, 0);
}

void ClassesNavigatorPageTests::
suspensionClearsProjectionAndHonorsResetPolicies()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({
        {1, QStringLiteral("First")},
        {2, QStringLiteral("Second")}
    });
    fixture.state.navigation = navigation({
        navigationEntry(1, QStringLiteral("First"), QStringLiteral("E4"),
            QStringLiteral("Theseus"), QStringLiteral("Monday"), QStringLiteral("09:00")),
        navigationEntry(2, QStringLiteral("Second"), QStringLiteral("E4"),
            QStringLiteral("Perseus"), QStringLiteral("Tuesday"), QStringLiteral("10:00"))
    });
    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    page->classList()->setCurrentIndex(
        page->model()->indexForClassId(classId(2)));
    auto* const monday = page->findChild<QToolButton*>(
        QStringLiteral("classesNavigatorMondayFilter"));
    monday->click();
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));

    QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    QCOMPARE(page->model()->classCount(), 0);
    QVERIFY(page->selectedClassId().has_value());
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QVERIFY(fixture.host->activatePage(QStringLiteral("classes")));
    QCOMPARE(page->model()->classCount(), 2);
    QVERIFY(page->selectedClassId().has_value());

    page->classList()->setCurrentIndex(
        page->model()->indexForClassId(classId(2)));
    fixture.state.preferences.classSelectionResetPolicy =
        Application::ClassSelectionResetPolicy::OnPageLeave;
    fixture.state.preferences.dayFilterResetPolicy =
        Application::ClassDayFilterResetPolicy::OnPageLeave;
    QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    QVERIFY(!page->selectedClassId().has_value());
    QVERIFY(fixture.host->activatePage(QStringLiteral("classes")));
    QCOMPARE(page->selectedClassId()->value(), std::string("1"));
    QVERIFY(!monday->isChecked());
}

void ClassesNavigatorPageTests::
ninetySixClassesUseOneCompactModelWithoutClassWidgets()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    std::vector<std::pair<int, QString>> classes;
    std::vector<Application::ClassesNavigationEntry> summaries;
    for (int index = 0; index < 96; ++index)
    {
        const int id = 1'000 + index;
        const QString name = QStringLiteral("Navigator Class %1").arg(index + 1);
        classes.emplace_back(id, name);
        summaries.push_back(navigationEntry(
            id,
            name,
            QStringLiteral("E4"),
            (index % 2) == 0 ? QStringLiteral("Theseus") : QStringLiteral("Perseus"),
            (index % 2) == 0 ? QStringLiteral("Monday") : QStringLiteral("Tuesday"),
            QStringLiteral("%1:00").arg(9 + (index % 5))
            ));
    }
    fixture.state.classes = classList(classes);
    fixture.state.navigation = navigation(std::move(summaries));

    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    QCOMPARE(page->model()->classCount(), 96);
    QCOMPARE(page->model()->visibleClassCount(), 96);
    QCOMPARE(page->model()->gradeGroupCount(), 1);
    QCOMPARE(page->gradeTabs()->count(), 2);
    QCOMPARE(page->classList()->model()->rowCount(), 96);
    QVERIFY(widgetCount(page) < 45);
    int classWidgetCount = 0;
    for (QWidget* widget : page->findChildren<QWidget*>())
    {
        classWidgetCount += widget->property("class_id").isValid() ? 1 : 0;
    }
    QCOMPARE(classWidgetCount, 0);
}

void ClassesNavigatorPageTests::
languageRefreshChangesLabelsWithoutChangingCanonicalSelection()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({{8, QStringLiteral("Hercules")} });
    fixture.state.navigation = navigation({
        navigationEntry(8, QStringLiteral("Hercules"), QStringLiteral("E4"),
            QStringLiteral("Hercules"), QStringLiteral("Monday"), QStringLiteral("09:00"))
    });
    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    QCOMPARE(page->selectedClassId()->value(), std::string("8"));

    KoreanAllTranslator translator;
    qApp->installTranslator(&translator);
    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(page, &languageChange);
    QCOMPARE(page->gradeTabs()->tabText(1), QStringLiteral("모두"));
    QCOMPARE(page->model()->data(page->model()->index(0, 0)).toString(),
        QStringLiteral("E4 Hercules • M 9:00"));
    QCOMPARE(page->selectedClassId()->value(), std::string("8"));
    qApp->removeTranslator(&translator);
    QEvent englishChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(page, &englishChange);
    QCOMPARE(page->gradeTabs()->tabText(1), QStringLiteral("All"));
    QCOMPARE(page->selectedClassId()->value(), std::string("8"));
}

void ClassesNavigatorPageTests::
productionKoreanCatalogTranslatesNavigatorDisplay()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    fixture.state.classes = classList({{8, QStringLiteral("Hercules")} });
    fixture.state.navigation = navigation({
        navigationEntry(8, QStringLiteral("Hercules"), QStringLiteral("E4"),
            QStringLiteral("Hercules"), QStringLiteral("Saturday"),
            QStringLiteral("09:00"))
    });
    QVERIFY(fixture.activate());
    auto* const page = fixture.page();
    QVERIFY(page);
    QCOMPARE(page->selectedClassId()->value(), std::string("8"));

    QTranslator translator;
    const QString translationPath = QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY)
        + QStringLiteral("/ClassMngr_ko_KR.qm");
    QVERIFY2(translator.load(translationPath), qPrintable(translationPath));
    QVERIFY(qApp->installTranslator(&translator));
    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(page, &languageChange);
    QCOMPARE(page->findChild<QLabel*>(QStringLiteral("classesNavigatorTitle"))
                 ->text(),
             QStringLiteral("수업"));
    QCOMPARE(page->scheduleSourceCombo()->itemText(0), QStringLiteral("정규 일정"));
    QCOMPARE(page->visibilityScopeCombo()->itemText(0), QStringLiteral("모든 수업"));
    QCOMPARE(page->findChild<QToolButton*>(QStringLiteral(
        "classesNavigatorMondayFilter"))->text(), QStringLiteral("월"));
    QCOMPARE(page->findChild<QToolButton*>(QStringLiteral(
        "classesNavigatorWeekendFilter"))->text(), QStringLiteral("주말"));
    QCOMPARE(page->gradeTabs()->tabText(1), QStringLiteral("전체"));
    QCOMPARE(page->model()->data(page->model()->index(0, 0)).toString(),
        QStringLiteral("E4 Hercules • 토 9:00"));
    QCOMPARE(page->selectedClassId()->value(), std::string("8"));
    QVERIFY(qApp->removeTranslator(&translator));

    QEvent englishChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(page, &englishChange);
    QCOMPARE(page->gradeTabs()->tabText(1), QStringLiteral("All"));
    QCOMPARE(page->selectedClassId()->value(), std::string("8"));
}

QTEST_MAIN(ClassesNavigatorPageTests)
#include "classes_navigator_page_tests.moc"
