#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/page_host.h"
#include "next/ui/sub_prep_page.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QCoreApplication>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTranslator>
#include <QtTest/QtTest>

#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

const Domain::ClassId& classId()
{
    static const Domain::ClassId id =
        *Domain::ClassId::fromString("42");
    return id;
}

Application::SubPrepPreferences savedPreferences(
    const std::string& materials,
    const std::string& grading,
    const std::string& special,
    const std::string& notes
    )
{
    return {
        .classMaterials = materials,
        .bookReportGrading = grading,
        .bookReportSpecialInstructions = special,
        .subComments = notes
    };
}

Application::SubPrepClassSummary classSummary()
{
    return {
        .id = classId(),
        .grade = "Grade 1",
        .level = "Level A",
        .displayLabel = "Class 1A",
        .meetingText = "Monday 09:00-10:00",
        .studentCount = Application::ClassSummaryStudentCount(12),
        .order = Application::ClassSummaryOrder(0),
        .navigationLabel = "Class 1A",
        .navigationTeacherLabel = "Teacher",
        .navigationFirstDayOrder = Application::ClassTabNavigationOrderRank(0),
        .navigationFirstTimeOrder = Application::ClassTabNavigationOrderRank(540)
    };
}

Application::SchedulePageReadSnapshot scheduleSnapshot()
{
    Application::SchedulePageReadSnapshot result;
    result.source.classes.push_back({
        .classId = classId(),
        .grade = u"Grade 1",
        .level = u"Level A",
        .regularSchedule = {{
            .day = Application::ScheduleBuilderSourceScheduleText(u"Monday"),
            .startTime = Application::ScheduleBuilderSourceScheduleText(u"09:00"),
            .endTime = Application::ScheduleBuilderSourceScheduleText(u"10:00")
        }}
    });
    return result;
}

struct PageFixture final
{
    Test::MemoryPreferences preferences;
    std::unique_ptr<Resources::ResourceCatalog> catalog;
    std::unique_ptr<Resources::ResourceLoader> loader;
    std::unique_ptr<Ui::PageHost> host;
    Application::SubPrepPreferences storedPreferences = savedPreferences(
        "stored materials", "stored grading", "stored special", "stored notes");
    Application::SchedulePageReadSnapshot schedule = scheduleSnapshot();
    Application::SubPrepPreferences lastSaved;
    int preferencesReadCount = 0;
    int preferencesSaveCount = 0;
    bool preferencesReadFails = false;

    bool initialize()
    {
        auto loaded = Resources::ResourceCatalog::load(
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT));
        if (!loaded)
        {
            return false;
        }
        catalog = std::make_unique<Resources::ResourceCatalog>(
            std::move(*loaded));
        loader = std::make_unique<Resources::ResourceLoader>(*catalog);
        host = std::make_unique<Ui::PageHost>(*loader, preferences.model);

        Ui::PageDescriptor descriptor{
            .id = *Ui::PageId::fromString(QStringLiteral("sub_prep_preferences")),
            .label = QStringLiteral("Sub Prep Preferences"),
            .factory = [this](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                Ui::SubPrepPageActions actions;
                actions.workspaceIdentity = [] {
                    return QStringLiteral("workspace-a");
                };
                actions.readSchedulePage = [this] {
                    return Application::SchedulePageReadResult::success(schedule);
                };
                actions.readSummaries = [](const auto&) {
                    return Application::ClassSummaryProjection::create({
                        .classes = {classSummary()}
                    });
                };
                actions.readSelectedDetails = [](const Domain::ClassId& id) {
                    return Application::SubPrepClassDetailsQueryResult::success({
                        .classId = id,
                        .classNotes = "Class notes",
                        .teacherDisplayName = "Teacher"
                    });
                };
                actions.readPreferences = [this] {
                    ++preferencesReadCount;
                    if (preferencesReadFails)
                    {
                        return Application::SubPrepPreferencesResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "preferences read failed",
                            .recoverable = true
                        });
                    }
                    return Application::SubPrepPreferencesResult::success(
                        storedPreferences);
                };
                actions.savePreferences = [this](const auto& updated) {
                    ++preferencesSaveCount;
                    lastSaved = updated;
                    storedPreferences = updated;
                    return Application::SubPrepPreferencesSaveResult::success();
                };
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<Ui::SubPrepPage>(context, std::move(actions)));
            }
        };
        return host->registerPage(std::move(descriptor))
            && host->activatePage(QStringLiteral("sub_prep_preferences"));
    }

    Ui::SubPrepPage* page() const
    {
        return host
            ? dynamic_cast<Ui::SubPrepPage*>(
                  host->pageInstance(QStringLiteral("sub_prep_preferences")))
            : nullptr;
    }
};

} // namespace

class SubPrepPageTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void preferencesReadFailureRetainsDraftAndBlocksWritesUntilReload();
};

void SubPrepPageTests::preferencesReadFailureRetainsDraftAndBlocksWritesUntilReload()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    Ui::SubPrepPage* const page = fixture.page();
    QVERIFY(page);

    const auto editor = [page](const char* name) {
        return page->findChild<QPlainTextEdit*>(QString::fromLatin1(name));
    };
    QPlainTextEdit* const materials = editor("subPrepClassMaterials");
    QPlainTextEdit* const grading = editor("subPrepGradingInstructions");
    QPlainTextEdit* const special = editor("subPrepSpecialInstructions");
    QPlainTextEdit* const notes = editor("subPrepSubComments");
    QLabel* const status = page->findChild<QLabel*>(
        QStringLiteral("subPrepRouteStatus"));
    QPushButton* const save = page->findChild<QPushButton*>(
        QStringLiteral("subPrepSavePreferences"));
    QVERIFY(materials);
    QVERIFY(grading);
    QVERIFY(special);
    QVERIFY(notes);
    QVERIFY(status);
    QVERIFY(save);
    QCOMPARE(materials->toPlainText(), QStringLiteral("stored materials"));
    QCOMPARE(grading->toPlainText(), QStringLiteral("stored grading"));
    QCOMPARE(special->toPlainText(), QStringLiteral("stored special"));
    QCOMPARE(notes->toPlainText(), QStringLiteral("stored notes"));
    QCOMPARE(page->visualMetrics().visibleSummaryCount, 1);

    QTranslator koreanTranslator;
    QVERIFY(koreanTranslator.load(
        QStringLiteral(CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY)
        + QStringLiteral("/ClassMngr_ko_KR.qm")));
    QVERIFY(QCoreApplication::installTranslator(&koreanTranslator));
    fixture.preferencesReadFails = true;
    QString reloadError;
    QVERIFY2(!page->reload(&reloadError), qPrintable(reloadError));
    const QString koreanFailure = QStringLiteral(
        "대체 수업 준비 설정을 불러오지 못했습니다. 저장하기 전에 페이지를 새로고침하세요.");
    QCOMPARE(reloadError, koreanFailure);
    QCOMPARE(status->text(), koreanFailure);
    QVERIFY(status->property("error").toBool());
    QVERIFY(QApplication::activeModalWidget() == nullptr);
    QCOMPARE(materials->toPlainText(), QStringLiteral("stored materials"));
    QCOMPARE(grading->toPlainText(), QStringLiteral("stored grading"));
    QCOMPARE(special->toPlainText(), QStringLiteral("stored special"));
    QCOMPARE(notes->toPlainText(), QStringLiteral("stored notes"));
    QCOMPARE(page->visualMetrics().visibleSummaryCount, 1);

    materials->setPlainText(QStringLiteral("unsaved draft"));
    QCOMPARE(fixture.preferencesSaveCount, 0);
    QCOMPARE(materials->toPlainText(), QStringLiteral("unsaved draft"));
    QTest::mouseClick(save, Qt::LeftButton);
    QCOMPARE(fixture.preferencesSaveCount, 0);
    page->onSuspending();
    QCOMPARE(fixture.preferencesSaveCount, 0);
    QVERIFY(QCoreApplication::removeTranslator(&koreanTranslator));

    fixture.storedPreferences = savedPreferences(
        "reloaded materials", "reloaded grading", "reloaded special", "reloaded notes");
    fixture.preferencesReadFails = false;
    const auto activated = page->onActivating();
    QVERIFY(activated.has_value());
    QCOMPARE(materials->toPlainText(), QStringLiteral("reloaded materials"));
    QCOMPARE(grading->toPlainText(), QStringLiteral("reloaded grading"));
    QCOMPARE(special->toPlainText(), QStringLiteral("reloaded special"));
    QCOMPARE(notes->toPlainText(), QStringLiteral("reloaded notes"));
    QVERIFY(status->text().isEmpty());
    QVERIFY(!status->property("error").toBool());

    materials->setPlainText(QStringLiteral("saved after reload"));
    QCOMPARE(fixture.preferencesSaveCount, 1);
    QVERIFY(fixture.lastSaved.classMaterials == std::string("saved after reload"));
}

QTEST_MAIN(SubPrepPageTests)

#include "sub_prep_page_tests.moc"
