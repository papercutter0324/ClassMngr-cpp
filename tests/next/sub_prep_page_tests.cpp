#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/page_host.h"
#include "next/ui/sub_prep_page.h"
#include "features/sub_prep/services/sub_prep_package_service.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
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

struct OutputDirectoryDialogProbe final
{
    std::vector<Application::SelectDirectoryDialogRequest> requests;
    std::optional<Application::FileDialogPath> selection;
    int scheduleReadCount = 0;
    int printSourceReadCount = 0;
    int rosterOutputPortCreateCount = 0;
};

class SubPrepPageFileDialogPort final : public Application::FileDialogPort
{
public:
    explicit SubPrepPageFileDialogPort(
        std::shared_ptr<OutputDirectoryDialogProbe> probe)
        : m_probe(std::move(probe))
    {
    }

    [[nodiscard]] Domain::Result<Application::FileDialogResponse<
        Application::FileDialogPath>>
    openOneFile(const Application::OpenFileDialogRequest&) override
    {
        return unexpectedCall<Application::FileDialogResponse<
            Application::FileDialogPath>>();
    }

    [[nodiscard]] Domain::Result<Application::FileDialogResponse<
        std::vector<Application::FileDialogPath>>>
    openManyFiles(const Application::OpenFileDialogRequest&) override
    {
        return unexpectedCall<Application::FileDialogResponse<
            std::vector<Application::FileDialogPath>>>();
    }

    [[nodiscard]] Domain::Result<Application::FileDialogResponse<
        Application::SavedFileDialogSelection>>
    saveFile(const Application::SaveFileDialogRequest&) override
    {
        return unexpectedCall<Application::FileDialogResponse<
            Application::SavedFileDialogSelection>>();
    }

    [[nodiscard]] Domain::Result<Application::FileDialogResponse<
        Application::FileDialogPath>>
    selectDirectory(
        const Application::SelectDirectoryDialogRequest& request) override
    {
        m_probe->requests.push_back(request);
        return Domain::Result<Application::FileDialogResponse<
            Application::FileDialogPath>>::success({
                .selection = m_probe->selection
            });
    }

private:
    template <typename Response>
    [[nodiscard]] static Domain::Result<Response> unexpectedCall()
    {
        return Domain::Result<Response>::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unexpected file dialog operation.",
            .recoverable = false
        });
    }

    std::shared_ptr<OutputDirectoryDialogProbe> m_probe;
};

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
    std::shared_ptr<OutputDirectoryDialogProbe> outputDirectoryDialog =
        std::make_shared<OutputDirectoryDialogProbe>();
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
                    ++outputDirectoryDialog->scheduleReadCount;
                    return Application::SchedulePageReadResult::success(schedule);
                };
                actions.selectOutputDirectory = [this](
                    const Application::SelectDirectoryDialogRequest& request,
                    QWidget*) {
                    SubPrepPageFileDialogPort fileDialog(outputDirectoryDialog);
                    return fileDialog.selectDirectory(request);
                };
                actions.readPrintSource = [this](const auto&) {
                    ++outputDirectoryDialog->printSourceReadCount;
                    return Application::SubPrepPrintSourceQueryResult::failure({
                        .code = Domain::ErrorCode::Technical,
                        .message = "Unexpected output source read.",
                        .recoverable = false
                    });
                };
                actions.createRosterOutputSourceReadPort = [this] {
                    ++outputDirectoryDialog->rosterOutputPortCreateCount;
                    return std::unique_ptr<
                        Application::SubPrepRosterOutputSourceReadPort>{};
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
    void outputDirectoryChooserUsesPortAndPreservesSelectionOnCancel();
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

void SubPrepPageTests::
outputDirectoryChooserUsesPortAndPreservesSelectionOnCancel()
{
    PageFixture fixture;
    QVERIFY(fixture.initialize());
    Ui::SubPrepPage* const page = fixture.page();
    QVERIFY(page);
    const int scheduleReadCountBeforeGeneration =
        fixture.outputDirectoryDialog->scheduleReadCount;

    const QString selectedRoot = QStringLiteral("/tmp/sub-prep-selected-root");
    fixture.outputDirectoryDialog->selection = Application::FileDialogPath(
        selectedRoot.toUtf8().toStdString());

    bool dialogSeen = false;
    bool selectionWasApplied = false;
    bool cancelPreservedSelection = false;
    bool outputDialogStayedOpen = false;
    bool automationFailed = false;
    QTimer::singleShot(0, page, [&] {
        auto* const dialog = qobject_cast<QDialog*>(
            QApplication::activeModalWidget());
        if (!dialog)
        {
            automationFailed = true;
            return;
        }
        dialogSeen = true;
        auto* const folder = dialog->findChild<QLineEdit*>(
            QStringLiteral("subPrepOutputFolder"));
        auto* const choose = dialog->findChild<QPushButton*>(
            QStringLiteral("subPrepChooseOutputFolder"));
        auto* const cancel = dialog->findChild<QPushButton*>(
            QStringLiteral("subPrepOutputCancel"));
        if (!folder || !choose || !cancel)
        {
            automationFailed = true;
            dialog->reject();
            return;
        }

        choose->click();
        selectionWasApplied = folder->text() == selectedRoot;
        fixture.outputDirectoryDialog->selection.reset();
        choose->click();
        cancelPreservedSelection = folder->text() == selectedRoot;
        outputDialogStayedOpen = QApplication::activeModalWidget() == dialog;
        cancel->click();
    });
    QTimer safetyClose;
    safetyClose.setSingleShot(true);
    QObject::connect(&safetyClose, &QTimer::timeout, page, [&] {
        if (QWidget* const modal = QApplication::activeModalWidget())
        {
            automationFailed = true;
            modal->close();
        }
    });
    safetyClose.start(15'000);
    auto* const generate = page->findChild<QPushButton*>(
        QStringLiteral("subPrepGenerateButton"));
    QVERIFY(generate);
    generate->click();
    safetyClose.stop();

    QVERIFY(dialogSeen);
    QVERIFY(!automationFailed);
    QVERIFY(selectionWasApplied);
    QVERIFY(cancelPreservedSelection);
    QVERIFY(outputDialogStayedOpen);
    QCOMPARE(fixture.outputDirectoryDialog->requests.size(), std::size_t(2));
    const auto& firstRequest = fixture.outputDirectoryDialog->requests.front();
    QCOMPARE(firstRequest.purpose, Application::FileDialogPurpose::SubPrepPackage);
    QCOMPARE(QString::fromUtf8(firstRequest.title.data(),
                               static_cast<qsizetype>(firstRequest.title.size())),
             QStringLiteral("Select Folder"));
    QCOMPARE(QString::fromUtf8(firstRequest.initialDirectory.value().data(),
                               static_cast<qsizetype>(firstRequest.initialDirectory.value().size())),
             SubPrepPackageService::defaultTargetRoot());
    const auto& secondRequest = fixture.outputDirectoryDialog->requests.back();
    QCOMPARE(QString::fromUtf8(secondRequest.initialDirectory.value().data(),
                               static_cast<qsizetype>(secondRequest.initialDirectory.value().size())),
             selectedRoot);
    QCOMPARE(fixture.outputDirectoryDialog->scheduleReadCount,
             scheduleReadCountBeforeGeneration + 1);
    QCOMPARE(fixture.outputDirectoryDialog->printSourceReadCount, 0);
    QCOMPARE(fixture.outputDirectoryDialog->rosterOutputPortCreateCount, 0);
    QCOMPARE(page->findChild<QLabel*>(QStringLiteral("subPrepRouteStatus"))->text(),
             QStringLiteral("Sub Prep generation canceled."));
}

QTEST_MAIN(SubPrepPageTests)

#include "sub_prep_page_tests.moc"
