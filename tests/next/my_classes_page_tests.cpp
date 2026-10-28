#include "data/database/workspace_store.h"
#include "next/application/my_classes_page_read_query.h"
#include "next/platform/workspace_store_my_classes_page_read_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/my_classes_page.h"
#include "next/ui/page_host.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QFrame>
#include <QFile>
#include <QLabel>
#include <QItemSelectionModel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTabBar>
#include <QTranslator>
#include <QListView>
#include <QtTest/QtTest>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

Domain::ClassId classId(const int id)
{
    return *Domain::ClassId::fromString(std::to_string(id));
}

Application::MyClassesPageClassSummary summary(
    const int id,
    const QString& grade,
    const QString& level,
    const QString& name,
    const QString& day = {},
    const QString& start = {},
    const QString& end = {},
    const QString& teacherEnglish = {},
    const QString& teacherKorean = {}
    )
{
    Application::MyClassesPageClassSummary value{
        .classId = classId(id),
        .className = name.toStdU16String(),
        .grade = grade.toStdU16String(),
        .level = level.toStdU16String(),
        .teacherEnglishName = teacherEnglish.toStdU16String(),
        .teacherKoreanName = teacherKorean.toStdU16String()
    };
    if (!day.isEmpty())
    {
        value.regularSchedule.push_back({
            .day = day.toStdU16String(),
            .startTime = start.toStdU16String(),
            .endTime = end.toStdU16String()
        });
    }
    return value;
}

Application::MyClassesPageSelectedDetails detailsFor(
    const Application::MyClassesPageClassSummary& row,
    const bool assigned = false
    )
{
    Application::MyClassesPageSelectedDetails result{
        .classId = row.classId,
        .className = row.className,
        .grade = row.grade,
        .level = row.level,
        .studentCount = 2,
        .classNotes = u"Class notes",
        .timeFillerActivities = u"Quiet reading"
    };
    result.regularSchedule = row.regularSchedule;
    result.intensiveSchedule = row.intensiveSchedule;
    if (assigned)
    {
        result.teacherId = *Domain::TeacherId::fromString("12");
        result.teacher = Application::MyClassesPageTeacherDetails{
            .englishName = u"Dana Kim",
            .roomNumber = u"204",
            .internetType = u"WiFi",
            .wifiName = u"Teachers",
            .wifiPassword = u"wifi-secret",
            .projectionType = u"HDMI",
            .zoomId = u"dana.kim",
            .zoomPassword = u"zoom-secret",
            .notes = u"Bring the adapter."
        };
    }
    return result;
}

struct ReadState final
{
    Application::MyClassesPageSummarySnapshot summaries;
    std::unordered_map<std::string, Application::MyClassesPageSelectedDetails> details;
    std::vector<std::string> requestedIds;
    QString identity = QStringLiteral("workspace-a");
    bool summaryFailure = false;
    bool detailsFailure = false;
};

class EmptyPage final : public Ui::Page
{
public:
    using Ui::Page::Page;
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
        const auto loadedCatalog = Resources::ResourceCatalog::load(
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)
            );
        if (!loadedCatalog)
        {
            return false;
        }
        catalog = std::make_unique<Resources::ResourceCatalog>(
            std::move(*loadedCatalog));
        loader = std::make_unique<Resources::ResourceLoader>(*catalog);
        host = std::make_unique<Ui::PageHost>(*loader, preferences.model);

        Ui::PageDescriptor myClasses{
            .id = *Ui::PageId::fromString(QStringLiteral("my_classes")),
            .label = QStringLiteral("My Classes"),
            .factory = [this](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                Ui::MyClassesPageActions actions;
                actions.workspaceIdentity = [this] { return state.identity; };
                actions.readSummaries = [this] {
                    if (state.summaryFailure)
                    {
                        return Application::MyClassesPageSummaryResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "summary read failed",
                            .recoverable = true
                        });
                    }
                    return Application::MyClassesPageSummaryResult::success(
                        state.summaries);
                };
                actions.readSelectedDetails = [this](const Domain::ClassId& id) {
                    state.requestedIds.push_back(id.value());
                    if (state.detailsFailure)
                    {
                        return Application::MyClassesPageSelectedDetailsResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "detail read failed",
                            .recoverable = true
                        });
                    }
                    const auto found = state.details.find(id.value());
                    if (found == state.details.end())
                    {
                        return Application::MyClassesPageSelectedDetailsResult::failure({
                            .code = Domain::ErrorCode::NotFound,
                            .message = "detail missing",
                            .recoverable = true
                        });
                    }
                    return Application::MyClassesPageSelectedDetailsResult::success(
                        found->second);
                };
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<Ui::MyClassesPage>(context, std::move(actions)));
            }
        };
        Ui::PageDescriptor other{
            .id = *Ui::PageId::fromString(QStringLiteral("other")),
            .label = QStringLiteral("Other"),
            .factory = [](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                return std::unique_ptr<Ui::Page>(std::make_unique<EmptyPage>(context));
            }
        };
        return host->registerPage(std::move(myClasses))
            && host->registerPage(std::move(other))
            && host->activatePage(QStringLiteral("my_classes"))
            && page() != nullptr;
    }

    Ui::MyClassesPage* page() const
    {
        return host
            ? dynamic_cast<Ui::MyClassesPage*>(
                  host->pageInstance(QStringLiteral("my_classes")))
            : nullptr;
    }
};

int widgetCount(QWidget* root)
{
    return root ? root->findChildren<QWidget*>().size() + 1 : 0;
}

class FakeReadPort final : public Application::MyClassesPageReadPort
{
public:
    Application::MyClassesPageSummaryResult readSummaries() const override
    {
        return summaries;
    }

    Application::MyClassesPageSelectedDetailsResult readSelectedDetails(
        const Domain::ClassId& id
        ) const override
    {
        requested.push_back(id);
        return selected;
    }

    Application::MyClassesPageSummaryResult summaries =
        Application::MyClassesPageSummaryResult::success({});
    Application::MyClassesPageSelectedDetailsResult selected =
        Application::MyClassesPageSelectedDetailsResult::success({
            .classId = classId(1)
        });
    mutable std::vector<Domain::ClassId> requested;
};

} // namespace

class NextMyClassesPageTests final : public QObject
{
    Q_OBJECT

private slots:
    void summaryModelMatchesFlatAndGroupedLegacyLabels();
    void oneModelLoadsOneSelectedIdAndReleasesOnSuspension();
    void navigationMatchesLegacyFlatAndGroupedThreshold();
    void emptyReadFailureDetailFailureAndUnassignedTeacherAreVisible();
    void queryRejectsInvalidAndUnboundedRecords();
    void workspaceStoreUsesBoundedSummaryAndSingleIdDetailReads();
    void labelsRetranslateAcrossLanguageChange();
};

void NextMyClassesPageTests::summaryModelMatchesFlatAndGroupedLegacyLabels()
{
    Ui::MyClassesSummaryModel model;
    Application::MyClassesPageSummarySnapshot rows{
        summary(3, QStringLiteral("E5"), QStringLiteral("Artemis"),
                QStringLiteral("Artemis class")),
        summary(2, QStringLiteral("E4"), QStringLiteral("Perseus"),
                QStringLiteral("Perseus class"), QStringLiteral("Wednesday"),
                QStringLiteral("1:00 PM"), QStringLiteral("1:55 PM")),
        summary(1, QStringLiteral("E4"), QStringLiteral("Theseus"),
                QStringLiteral("Theseus class"), QStringLiteral("Tuesday"),
                QStringLiteral("11:05 am"), QStringLiteral("12:05 pm"))
    };
    rows[2].regularSchedule.push_back({
        .day = u"Monday", .startTime = u"9:00 AM", .endTime = u"9:45 AM"
    });
    model.setSummaries(rows);
    QVERIFY(!model.isGrouped());
    QCOMPARE(model.summaryCount(), 3);
    QCOMPARE(model.data(model.index(0, 0)).toString(),
             QStringLiteral("E4 Theseus • T 11:05; M 9:00"));
    QCOMPARE(model.data(model.index(1, 0)).toString(),
             QStringLiteral("E4 Perseus • W 1:00"));
    QCOMPARE(model.data(model.index(2, 0)).toString(),
             QStringLiteral("E5 Artemis • No time"));

    model.setSummaries({
        summary(11, QStringLiteral("E4"), QStringLiteral("Theseus"),
                QStringLiteral("Alex class"), {}, {}, {}, QStringLiteral("Alex")),
        summary(12, QStringLiteral("E4"), QStringLiteral("Theseus"),
                QStringLiteral("Jamie class"), {}, {}, {}, QStringLiteral("Jamie"))
    });
    QCOMPARE(model.data(model.index(0, 0)).toString(),
             QStringLiteral("E4 Theseus • No time • Alex"));
    QCOMPARE(model.data(model.index(1, 0)).toString(),
             QStringLiteral("E4 Theseus • No time • Jamie"));
    model.setSummaries({
        summary(21, QStringLiteral("E4"), QStringLiteral("Theseus"),
                QStringLiteral("First class"), {}, {}, {}, QStringLiteral("Alex")),
        summary(22, QStringLiteral("E4"), QStringLiteral("Theseus"),
                QStringLiteral("Second class"), {}, {}, {}, QStringLiteral("Alex"))
    });
    QVERIFY(model.data(model.index(0, 0)).toString().endsWith(QStringLiteral("#21")));
    QVERIFY(model.data(model.index(1, 0)).toString().endsWith(QStringLiteral("#22")));

    Application::MyClassesPageSummarySnapshot large;
    large.reserve(96);
    static const std::array<QString, 6> validGrades{
        QStringLiteral("E4"), QStringLiteral("E5"), QStringLiteral("E6"),
        QStringLiteral("M1"), QStringLiteral("M2"), QStringLiteral("M3")
    };
    for (int id = 1; id <= 96; ++id)
    {
        const QString grade = validGrades[static_cast<std::size_t>((id - 1) % 6)];
        large.push_back(summary(id, grade, QStringLiteral("Level %1").arg(id),
            QStringLiteral("Class %1").arg(id), QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")));
    }
    model.setSummaries(std::move(large));
    QVERIFY(model.isGrouped());
    QCOMPARE(model.summaryCount(), 96);
    QCOMPARE(model.gradeGroupCount(), 6);
    QCOMPARE(model.gradeGroupChildCount(0), 16);
    const QModelIndex group = model.index(0, 0);
    QCOMPARE(model.data(group).toString(), QStringLiteral("E4"));
    const QModelIndex firstChild = model.index(0, 0, group);
    QVERIFY(firstChild.isValid());
    QCOMPARE(firstChild.row(), 0);
    QVERIFY(model.summaryForIndex(firstChild));
    QVERIFY(model.data(firstChild).toString().startsWith(QStringLiteral("Level ")));
    QCOMPARE(model.parent(firstChild), group);
    QCOMPARE(model.indexForClassId(classId(49)).isValid(), true);
}

void NextMyClassesPageTests::oneModelLoadsOneSelectedIdAndReleasesOnSuspension()
{
    PageFixture fixture;
    fixture.state.summaries.reserve(96);
    static const std::array<QString, 6> validGrades{
        QStringLiteral("E4"), QStringLiteral("E5"), QStringLiteral("E6"),
        QStringLiteral("M1"), QStringLiteral("M2"), QStringLiteral("M3")
    };
    for (int id = 1; id <= 96; ++id)
    {
        const QString grade = validGrades[static_cast<std::size_t>((id - 1) % 6)];
        auto row = summary(id, grade, QStringLiteral("L%1").arg(id),
            QStringLiteral("Class %1").arg(id));
        fixture.state.details.emplace(row.classId.value(), detailsFor(row));
        fixture.state.summaries.push_back(std::move(row));
    }
    QVERIFY(fixture.initialize());
    Ui::MyClassesPage* const page = fixture.page();
    QVERIFY(page);
    QCOMPARE(page->summaryModel(), page->classList()->model());
    QCOMPARE(page->summaryModel()->summaryCount(), 96);
    QCOMPARE(fixture.state.requestedIds.size(), std::size_t(1));
    QCOMPARE(fixture.state.requestedIds.front(),
             page->summaryModel()->summaries().front().classId.value());

    page->resize(1024, 768);
    page->show();
    QApplication::processEvents();
    const int widgetsFor96Rows = widgetCount(page);
    QVERIFY(widgetsFor96Rows < 100);
    QCOMPARE(page->findChildren<QListView*>(QStringLiteral("myClassesSummaryList"))
                 .size(), 1);
    QCOMPARE(page->gradeTabs()->count(), 6);
    QCOMPARE(page->findChildren<QFrame*>(QStringLiteral("myClassesClassCard"))
                 .size(), 1);

    auto allSummaries = fixture.state.summaries;
    fixture.state.summaries.erase(
        fixture.state.summaries.begin() + 3,
        fixture.state.summaries.end()
        );
    QVERIFY(page->refreshFromStore());
    const int widgetsForThreeRows = widgetCount(page);
    fixture.state.summaries = std::move(allSummaries);
    QVERIFY(page->refreshFromStore());
    QCOMPARE(widgetCount(page), widgetsForThreeRows);

    const QModelIndex selected = page->summaryModel()->indexForClassId(classId(50));
    QVERIFY(selected.isValid());
    page->gradeTabs()->setCurrentIndex(selected.parent().row());
    page->classList()->selectionModel()->setCurrentIndex(
        selected,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows
        );
    QCOMPARE(fixture.state.requestedIds.back(), std::string("50"));
    QCOMPARE(page->selectedDetails()->classId.value(), std::string("50"));

    QString error;
    QVERIFY(page->refreshFromStore(&error));
    QVERIFY(error.isEmpty());
    QCOMPARE(page->selectedClassId()->value(), std::string("50"));
    QCOMPARE(fixture.state.requestedIds.back(), std::string("50"));
    QCOMPARE(widgetCount(page), widgetsFor96Rows);

    QVERIFY(fixture.host->activatePage(QStringLiteral("other")));
    QCOMPARE(page->summaryModel()->summaryCount(), 0);
    QVERIFY(page->detailsReleased());
    QVERIFY(!page->selectedDetails());
    QCOMPARE(page->selectedClassId()->value(), std::string("50"));

    QVERIFY(fixture.host->activatePage(QStringLiteral("my_classes")));
    QCOMPARE(page->summaryModel()->summaryCount(), 96);
    QCOMPARE(page->selectedClassId()->value(), std::string("50"));
    QCOMPARE(fixture.state.requestedIds.back(), std::string("50"));
    QVERIFY(page->selectedDetails());

    fixture.state.identity = QStringLiteral("workspace-b");
    page->clearForWorkspaceChange();
    QVERIFY(page->summaryModel()->summaries().empty());
    QVERIFY(page->onActivating());
    QCOMPARE(page->selectedClassId()->value(),
             page->summaryModel()->summaries().front().classId.value());
    QCOMPARE(fixture.state.requestedIds.back(),
             page->selectedClassId()->value());
}

void NextMyClassesPageTests::emptyReadFailureDetailFailureAndUnassignedTeacherAreVisible()
{
    PageFixture empty;
    QVERIFY(empty.initialize());
    QVERIFY(empty.page()->findChild<QLabel*>(QStringLiteral("myClassesLoadState")));
    QCOMPARE(empty.page()->summaryModel()->summaryCount(), 0);
    QCOMPARE(empty.page()->findChild<QLabel*>(QStringLiteral("myClassesLoadState"))
                 ->text(), QStringLiteral("No classes available."));
    QVERIFY(empty.state.requestedIds.empty());

    PageFixture failed;
    auto row = summary(1, QStringLiteral("E4"), QStringLiteral("A"),
                       QStringLiteral("One"));
    failed.state.summaries.push_back(row);
    failed.state.summaryFailure = true;
    QVERIFY(failed.initialize());
    QCOMPARE(failed.page()->findChild<QLabel*>(QStringLiteral("myClassesLoadState"))
                 ->text(), QStringLiteral("summary read failed"));
    QVERIFY(failed.page()->summaryModel()->summaries().empty());

    PageFixture unassigned;
    unassigned.state.summaries.push_back(row);
    unassigned.state.details.emplace(row.classId.value(), detailsFor(row, false));
    QVERIFY(unassigned.initialize());
    auto* const teacher = unassigned.page()->findChild<QLabel*>(
        QStringLiteral("myClassesTeacherTitle"));
    QVERIFY(teacher);
    QCOMPARE(teacher->text(), QStringLiteral("Unassigned"));
    QVERIFY(unassigned.page()->findChild<QPlainTextEdit*>(
        QStringLiteral("myClassesTeacherNotes"))->toPlainText().isEmpty());

    PageFixture detailError;
    detailError.state.summaries.push_back(row);
    detailError.state.detailsFailure = true;
    QVERIFY(detailError.initialize());
    auto* const visibleError = detailError.page()->findChild<QLabel*>(
        QStringLiteral("myClassesDetailError"));
    QVERIFY(visibleError);
    QVERIFY(!visibleError->isHidden());
    QCOMPARE(visibleError->text(), QStringLiteral("detail read failed"));
}

void NextMyClassesPageTests::queryRejectsInvalidAndUnboundedRecords()
{
    FakeReadPort port;
    Application::MyClassesPageReadQuery query(port);
    const auto valid = summary(1, QStringLiteral("E4"), QStringLiteral("A"),
                               QStringLiteral("One"));
    port.summaries = Application::MyClassesPageSummaryResult::success({valid, valid});
    QVERIFY(!query.readSummaries());

    auto tooLong = valid;
    tooLong.className.assign(
        Application::kMyClassesPageMaximumTextLength + 1, u'x'
        );
    port.summaries = Application::MyClassesPageSummaryResult::success({tooLong});
    QVERIFY(!query.readSummaries());

    auto tooManyScheduleRows = valid;
    tooManyScheduleRows.regularSchedule.assign(
        Application::kMyClassesPageMaximumScheduleRows + 1, {}
        );
    port.summaries = Application::MyClassesPageSummaryResult::success(
        {tooManyScheduleRows});
    QVERIFY(!query.readSummaries());

    auto tooLongScheduleToken = valid;
    tooLongScheduleToken.regularSchedule.push_back({
        .day = std::u16string(
            Application::kMyClassesPageMaximumScheduleTokenLength + 1, u'x'
            )
    });
    port.summaries = Application::MyClassesPageSummaryResult::success(
        {tooLongScheduleToken});
    QVERIFY(!query.readSummaries());

    Application::MyClassesPageSummarySnapshot tooManyClasses;
    tooManyClasses.reserve(Application::kMyClassesPageMaximumClasses + 1);
    for (std::size_t index = 0;
         index <= Application::kMyClassesPageMaximumClasses; ++index)
    {
        const int id = static_cast<int>(index + 1);
        tooManyClasses.push_back(summary(
            id, QStringLiteral("E4"), QStringLiteral("A"),
            QStringLiteral("Class %1").arg(id)));
    }
    port.summaries = Application::MyClassesPageSummaryResult::success(
        std::move(tooManyClasses));
    QVERIFY(!query.readSummaries());

    port.summaries = Application::MyClassesPageSummaryResult::success({valid});
    auto mismatched = detailsFor(valid);
    mismatched.classId = classId(2);
    port.selected = Application::MyClassesPageSelectedDetailsResult::success(
        std::move(mismatched));
    QVERIFY(!query.readSelectedDetails(classId(1)));
    QCOMPARE(port.requested.size(), std::size_t(1));
    QCOMPARE(port.requested.front().value(), std::string("1"));

    auto tooLongDetailsSchedule = detailsFor(valid);
    tooLongDetailsSchedule.regularSchedule.push_back({
        .day = std::u16string(
            Application::kMyClassesPageMaximumScheduleTokenLength + 1, u'x'
            )
    });
    port.selected = Application::MyClassesPageSelectedDetailsResult::success(
        std::move(tooLongDetailsSchedule));
    QVERIFY(!query.readSelectedDetails(classId(1)));
    QCOMPARE(port.requested.size(), std::size_t(2));
}

void NextMyClassesPageTests::workspaceStoreUsesBoundedSummaryAndSingleIdDetailReads()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString fixturePath = temporary.filePath(QStringLiteral("workspace.tps"));
    QVERIFY(QFile::copy(QStringLiteral(CLASSMNGR_NEXT_MY_CLASSES_LARGE_FIXTURE),
                        fixturePath));
    WorkspaceStore store;
    QVERIFY(store.openExisting(fixturePath));
    Platform::WorkspaceStoreMyClassesPageReadPort port(store);
    Application::MyClassesPageReadQuery query(port);
    const auto summaries = query.readSummaries();
    QVERIFY(summaries);
    QCOMPARE(summaries.value().size(), std::size_t(96));
    QCOMPARE(store.classRepository()->readMetrics().getClassesReturnedRowCount, 96);

    const Domain::ClassId selectedId = summaries.value().front().classId;
    const auto details = query.readSelectedDetails(selectedId);
    QVERIFY(details);
    QCOMPARE(details.value().classId.value(), selectedId.value());
    QCOMPARE(store.classRepository()->readMetrics().getClassByIdCallCount, 1);
    QCOMPARE(store.classInfoRepository()->myClassesClassInformationBatchReadMetrics()
                 .requestedClassCount, 1);
    QCOMPARE(store.rosterRepository()->myClassesStudentCountBatchReadMetrics()
                 .requestedClassCount, 1);

    QSqlQuery assignMissingTeacher(store.database());
    QVERIFY(assignMissingTeacher.exec(QStringLiteral("PRAGMA foreign_keys = OFF")));
    QVERIFY(assignMissingTeacher.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id = ? WHERE class_id = ?")));
    assignMissingTeacher.addBindValue(2147483647);
    assignMissingTeacher.addBindValue(std::stoi(selectedId.value()));
    QVERIFY2(assignMissingTeacher.exec(),
             qPrintable(assignMissingTeacher.lastError().text()));
    QVERIFY(assignMissingTeacher.exec(QStringLiteral("PRAGMA foreign_keys = ON")));
    const auto unassigned = query.readSelectedDetails(selectedId);
    QVERIFY(unassigned);
    QVERIFY(!unassigned.value().teacher);

    QSqlQuery dropClassInfo(store.database());
    QVERIFY(dropClassInfo.exec(QStringLiteral("DROP TABLE class_info")));
    const auto fallbackSummaries = query.readSummaries();
    QVERIFY(fallbackSummaries);
    QCOMPARE(fallbackSummaries.value().size(), std::size_t(96));
    const auto fallbackSelected = std::find_if(
        fallbackSummaries.value().cbegin(), fallbackSummaries.value().cend(),
        [&selectedId](const auto& record) {
            return record.classId == selectedId;
        });
    QVERIFY(fallbackSelected != fallbackSummaries.value().cend());
    QVERIFY(fallbackSelected->grade.empty());
    QVERIFY(fallbackSelected->level.empty());
}

void NextMyClassesPageTests::navigationMatchesLegacyFlatAndGroupedThreshold()
{
    PageFixture fixture;
    for (int id = 1; id <= 6; ++id)
    {
        auto row = summary(id, QStringLiteral("E4"),
            QStringLiteral("L%1").arg(id), QStringLiteral("Class %1").arg(id));
        fixture.state.details.emplace(row.classId.value(), detailsFor(row));
        fixture.state.summaries.push_back(std::move(row));
    }
    QVERIFY(fixture.initialize());
    Ui::MyClassesPage* const page = fixture.page();
    QVERIFY(page);
    QVERIFY(!page->summaryModel()->isGrouped());
    QVERIFY(page->gradeTabs()->isHidden());
    QCOMPARE(page->gradeTabs()->count(), 0);
    QVERIFY(!page->classList()->rootIndex().isValid());
    QCOMPARE(page->classList()->model()->rowCount(), 6);
    QVERIFY(page->classList()->model()->data(page->classList()->model()->index(0, 0))
                .toString().startsWith(QStringLiteral("E4 ")));

    auto seventh = summary(7, QStringLiteral("M1"), QStringLiteral("L7"),
                          QStringLiteral("Class 7"));
    fixture.state.details.emplace(seventh.classId.value(), detailsFor(seventh));
    fixture.state.summaries.push_back(std::move(seventh));
    QVERIFY(page->refreshFromStore());
    QVERIFY(page->summaryModel()->isGrouped());
    QVERIFY(!page->gradeTabs()->isHidden());
    QCOMPARE(page->gradeTabs()->count(), 2);
    QVERIFY(page->classList()->rootIndex().isValid());
    QVERIFY(page->classList()->model()->rowCount(page->classList()->rootIndex()) > 0);
}

void NextMyClassesPageTests::labelsRetranslateAcrossLanguageChange()
{
    auto* const application = qobject_cast<QApplication*>(QCoreApplication::instance());
    QVERIFY(application);
    PageFixture fixture;
    auto row = summary(1, QStringLiteral("E4"), QStringLiteral("A"),
                       QStringLiteral("One"));
    fixture.state.summaries.push_back(row);
    fixture.state.details.emplace(row.classId.value(), detailsFor(row));
    QVERIFY(fixture.initialize());
    Ui::MyClassesPage* const page = fixture.page();
    auto findFieldLabel = [page](const QString& value) {
        for (QLabel* const label : page->findChildren<QLabel*>())
        {
            if (label->text() == value)
            {
                return label;
            }
        }
        return static_cast<QLabel*>(nullptr);
    };
    QVERIFY(findFieldLabel(QStringLiteral("# of Students")));

    QTranslator translator;
    QVERIFY(translator.load(QStringLiteral(CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY)
        + QStringLiteral("/ClassMngr_ko_KR.qm")));
    QVERIFY(application->installTranslator(&translator));
    QApplication::processEvents();
    QVERIFY(findFieldLabel(QStringLiteral("학생 수")));
    QCOMPARE(page->classList()->model()->rowCount(page->classList()->rootIndex()), 1);
    application->removeTranslator(&translator);
    QApplication::processEvents();
    QVERIFY(findFieldLabel(QStringLiteral("# of Students")));
}

QTEST_MAIN(NextMyClassesPageTests)
#include "my_classes_page_tests.moc"
