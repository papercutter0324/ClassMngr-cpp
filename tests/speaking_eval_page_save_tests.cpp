#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/utils/student_name_utils.h"
#include "core/utils/sidebar_node_naming.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/roster.h"
#include "domain/models/speaking_evaluation.h"
#include "domain/models/teacher.h"
#include "features/speaking_eval/ui/speaking_eval_model.h"
#include "features/speaking_eval/ui/speaking_eval_ai_batch_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_page_p.h"
#include "features/speaking_eval/ui/speaking_eval_page.h"
#include "features/speaking_eval/ui/speaking_eval_table_view.h"
#include "fakes/fake_user_prompt_service.h"
#include "next/application/class_visibility_preferences.h"
#include "next/platform/application_services_class_visibility_preferences_port.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/widgets/navigation_tab_widget.h"
#include "ui/shared/validation/form_validation_binder.h"

#include <QCoreApplication>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QPushButton>
#include <QSqlDatabase>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QSqlError>
#include <QSqlQuery>
#include <QUndoStack>
#include <QtTest/QtTest>
#include <QUuid>

#include <algorithm>

namespace
{

class ScopedPromptService final
{
public:
    explicit ScopedPromptService(IUserPromptService* service)
    {
        DialogServices::setUserPromptServiceForTesting(service);
    }

    ~ScopedPromptService()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }
};

struct SpeakingEvalPageFixture final
{
    QTemporaryDir directory;
    ApplicationServices services;
    QList<int> classIds;
    QList<QString> classNames;

    bool initialize(const int classCount, QString* error)
    {
        if (!directory.isValid())
        {
            *error = QStringLiteral("Temporary directory is invalid.");
            return false;
        }

        const QString path = directory.filePath(
            QStringLiteral("speaking-eval-page-save-%1.tps").arg(
                QUuid::createUuid().toString(QUuid::WithoutBraces)
                )
            );
        const Status opened = services.openDatabase(path);
        if (!opened)
        {
            *error = opened.error();
            return false;
        }

        for (int index = 0; index < classCount; ++index)
        {
            const QString name = QStringLiteral("Speaking Save Class %1")
                .arg(index + 1);
            const auto created = services.classService()->create(name);
            if (!created)
            {
                *error = created.error();
                return false;
            }
            classIds.append(*created);
            classNames.append(name);
        }

        return true;
    }
};

bool setStudent(
    SpeakingEvalModel* model,
    const int row,
    const QString& english,
    const QString& korean
    )
{
    return model
        && model->setData(
            model->index(row, SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)),
            english,
            Qt::EditRole
            )
        && model->setData(
            model->index(row, SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)),
            korean,
            Qt::EditRole
            );
}

bool seedRosterWithoutValidation(
    QSqlDatabase database,
    const int classId,
    const Roster& roster,
    QString* error
    )
{
    QSqlQuery query(database);
    for (const QString& statement : {
             QStringLiteral("DELETE FROM roster_columns WHERE class_id=?"),
             QStringLiteral("DELETE FROM roster_data WHERE class_id=?")
         })
    {
        query.prepare(statement);
        query.addBindValue(classId);
        if (!query.exec())
        {
            *error = query.lastError().text();
            return false;
        }
    }

    for (int column = 0; column < roster.columns.size(); ++column)
    {
        query.prepare(
            "INSERT INTO roster_columns (class_id, name, position, width) "
            "VALUES (?, ?, ?, ?)"
            );
        query.addBindValue(classId);
        query.addBindValue(roster.columns[column]);
        query.addBindValue(column);
        query.addBindValue(0);
        if (!query.exec())
        {
            *error = query.lastError().text();
            return false;
        }
    }

    for (int row = 0; row < roster.rows.size(); ++row)
    {
        for (int column = 0; column < roster.columns.size(); ++column)
        {
            const QString value = roster.rows[row].value(column);
            if (value.isEmpty())
            {
                continue;
            }

            query.prepare(
                "INSERT INTO roster_data (class_id, row_index, col_index, value) "
                "VALUES (?, ?, ?, ?)"
                );
            query.addBindValue(classId);
            query.addBindValue(row);
            query.addBindValue(column);
            query.addBindValue(value);
            if (!query.exec())
            {
                *error = query.lastError().text();
                return false;
            }
        }
    }

    return true;
}

QPushButton* findImportNamesButton(SpeakingEvalPage& page)
{
    QPushButton* importNamesButton = nullptr;
    for (QPushButton* button : page.findChildren<QPushButton*>())
    {
        if (button->text() != QStringLiteral("Import Names"))
        {
            continue;
        }

        if (importNamesButton)
        {
            return nullptr;
        }
        importNamesButton = button;
    }

    return importNamesButton;
}

bool saveSubtitleForReportTests(
    ApplicationServices& services,
    const int classId,
    const QString& grade,
    const QString& level,
    const QString& englishTeacher,
    const QString& koreanTeacher,
    QString* error
    )
{
    Teacher teacher;
    teacher.teacherEn = englishTeacher;
    teacher.teacherKr = koreanTeacher;
    teacher.preferredRomanization = QStringLiteral("Romanized");
    teacher.preferredName = englishTeacher;
    const auto teacherId = services.teacherService()->create(teacher);
    if (!teacherId)
    {
        *error = teacherId.error();
        return false;
    }

    const auto loaded = services.classService()->classInfo(classId);
    if (!loaded)
    {
        *error = loaded.error();
        return false;
    }

    ClassInfo classInfo = *loaded;
    classInfo.teacherId = *teacherId;
    classInfo.classGrade = grade;
    classInfo.classLevel = level;
    classInfo.classTimes = {
        {
            .day = QStringLiteral("Monday"),
            .startTime = QStringLiteral("4:00 PM"),
            .endTime = QStringLiteral("4:50 PM")
        },
        {
            .day = QStringLiteral("Wednesday"),
            .startTime = QStringLiteral("5:00 PM"),
            .endTime = QStringLiteral("5:50 PM")
        }
    };
    classInfo.intensiveTimes = {
        {
            .day = QStringLiteral("Tuesday"),
            .startTime = QStringLiteral("7:00 PM"),
            .endTime = QStringLiteral("7:50 PM")
        }
    };
    const Status saved = services.classService()->saveClassInfo(classInfo);
    if (!saved)
    {
        *error = saved.error();
        return false;
    }

    return true;
}

struct OutputDirectoryCapture final
{
    bool foundLineEdit = false;
    QString directory;
};

OutputDirectoryCapture openSaveAsAndCaptureDirectory(SpeakingEvalPage& page)
{
    OutputDirectoryCapture capture;
    QTimer::singleShot(0, &page, [&capture]
    {
        QWidget* const modal = QApplication::activeModalWidget();
        auto* const directoryEdit = modal
            ? modal->findChild<QLineEdit*>()
            : nullptr;
        if (directoryEdit)
        {
            capture.foundLineEdit = true;
            capture.directory = directoryEdit->text();
        }

        if (auto* const dialog = qobject_cast<QDialog*>(modal))
        {
            dialog->reject();
        }
    });
    page.saveCurrentPageAs();
    return capture;
}

}

class SpeakingEvalPageSaveTests final : public QObject
{
    Q_OBJECT

private slots:
    void automaticSavePersistsMatrixWithoutInteractiveNotices();
    void manualValidationAndConfirmedSaveKeepTheirCurrentTiming();
    void failedClassAndEvaluationSwitchesRestoreTheCurrentSelection();
    void classTabsKeepOrderAndSelectedClass();
    void headerSubtitlePreservesClassAndTeacherDisplayMetadata();
    void scheduleModesAndVisibilityScopeFilterNavigationTabs();
    void unavailableClassListClearsThePageWithoutWarning();
    void failedClassListWarnsAndDisablesEditing();
    void failedNavigationReadKeepsNameOnlyTabs();
    void successfulLoadPreservesOrderedUnicodeMatrixAndCleanState();
    void emptyReadFallsBackToBlankGridAndCleanState();
    void failedReadFallsBackToBlankGridAndCleanState();
    void speakingEvalModelSuggestionMatchesLegacyHelper();
    void suffixChoiceAppliesSuggestedNameThroughExistingPageFlow();
    void duplicateLocateSelectsFirstPeerRow();
    void duplicateRosterMatchesKeepTrimmedFirstSeenOrder();
    void importNamesButtonAppliesRosterPairsAndPreservesMessages();
    void emptyRosterImportShowsNoDataWarningWithoutMutation();
    void unavailableRosterImportShowsNoDataWarningWithoutMutation();
    void missingImportColumnsShowsWarningWithoutMutation();
    void reportClassInfoProjectionMapsSubtitleFields();
    void reportClassInfoProjectionDefaultsWhenClassFieldsFail();
    void reportClassInfoProjectionDefaultsWhenOuterReadFails();
    void reportClassInfoProjectionKeepsClassFieldsWhenTeacherReadFails();
    void outputReportsUsesProjectedRegularSchedule();
    void generateCommentsAppliesToOriginalRowAndUndoesAsOneAction();
    void generateCommentsAppliesMultipleStudentsAndUndoesAsOneAction();
    void cancelingGenerateCommentsLeavesPageStateUntouched();
};

void SpeakingEvalPageSaveTests::
automaticSavePersistsMatrixWithoutInteractiveNotices()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    SpeakingEvalPage page(&fixture.services);
    page.setDatabaseOpen(true);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    auto* model = page.findChild<SpeakingEvalModel*>();
    auto* autosave = page.findChild<AutosaveCoordinator*>();
    QVERIFY(model);
    QVERIFY(autosave);
    QSignalSpy saveSpy(autosave, &AutosaveCoordinator::saveRequested);
    QVERIFY(saveSpy.isValid());

    QVERIFY(setStudent(model, 0, QStringLiteral("Alice"), QStringLiteral("\uAE40\uBBFC\uC9C0")));
    QVERIFY(page.hasUnsavedChanges());
    QTRY_VERIFY_WITH_TIMEOUT(saveSpy.count() >= 1, 5'000);
    QCOMPARE(saveSpy.constFirst().at(0).toBool(), false);
    QTRY_VERIFY_WITH_TIMEOUT(!page.hasUnsavedChanges(), 5'000);
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.messages.isEmpty());

    const auto persisted = fixture.services.speakingEvaluationService()->evaluation(
        fixture.classIds.first(),
        QStringLiteral("Winter")
        );
    QVERIFY(persisted);
    QCOMPARE(persisted->size(), 25);
    QCOMPARE(persisted->at(0).size(), 11);
    QCOMPARE(persisted->at(0).at(1), QStringLiteral("Alice"));
    QCOMPARE(persisted->at(0).at(2), QStringLiteral("\uAE40\uBBFC\uC9C0"));
}

void SpeakingEvalPageSaveTests::
manualValidationAndConfirmedSaveKeepTheirCurrentTiming()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    SpeakingEvalPage page(&fixture.services);
    page.setDatabaseOpen(true);
    page.setSaveMode(SaveMode::Manual);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    auto* model = page.findChild<SpeakingEvalModel*>();
    auto* table = page.findChild<SpeakingEvalTableView*>();
    auto* saveButton = page.findChild<QPushButton*>(
        QStringLiteral("speakingEvalSaveButton")
        );
    auto* validationBinder = page.findChild<FormValidationBinder*>();
    auto* validationMessage = page.findChild<QLabel*>(
        QStringLiteral("speakingEvalValidationMessage")
        );
    QVERIFY(model);
    QVERIFY(table);
    QVERIFY(saveButton);
    QVERIFY(validationBinder);
    QVERIFY(validationMessage);

    QVERIFY(setStudent(model, 0, QStringLiteral("Alice"), QStringLiteral("\uAE40\uBBFC\uC9C0")));
    saveButton->click();
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Information);
    QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Saved"));

    QVERIFY(model->setData(
        model->index(0, SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)),
        QStringLiteral("\uAC00\uB098"),
        Qt::EditRole
        ));
    const ValidationIssues unusualNameIssues =
        validationBinder->validation().forField(
            QStringLiteral("rows[0].Korean Name")
            );
    QCOMPARE(unusualNameIssues.size(), 1);
    QCOMPARE(
        unusualNameIssues.constFirst().code,
        QStringLiteral("student_name.korean.unusual_length")
        );
    QVERIFY(unusualNameIssues.constFirst().isWarning());
    QCOMPARE(
        validationMessage->property("formValidationSeverity").toString(),
        QStringLiteral("warning")
        );

    QVERIFY(model->setData(
        model->index(0, SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)),
        QStringLiteral("\uAE40"),
        Qt::EditRole
        ));
    QVERIFY(page.hasUnsavedChanges());
    const ValidationIssues koreanNameIssues =
        validationBinder->validation().forField(
            QStringLiteral("rows[0].Korean Name")
            );
    QCOMPARE(koreanNameIssues.size(), 1);
    QCOMPARE(
        koreanNameIssues.constFirst().code,
        QStringLiteral("student_name.korean.too_short")
        );
    QVERIFY(koreanNameIssues.constFirst().isError());
    QCOMPARE(
        validationMessage->property("formValidationSeverity").toString(),
        QStringLiteral("error")
        );
    QCOMPARE(
        validationMessage->text(),
        QStringLiteral("Correct the highlighted evaluation cells.")
        );

    page.saveData();
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(prompts.confirmations.isEmpty());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        table->currentIndex(),
        model->index(0, SpeakingEval::toInt(SpeakingEvalColumn::KoreanName))
        );

    prompts.scriptedChoices.enqueue(PromptChoice::Rejected);
    QVERIFY(!page.saveChanges());
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(prompts.confirmations.size(), 1);
    QCOMPARE(
        prompts.confirmations.constFirst().title,
        QStringLiteral("Verify Korean Name Lengths")
        );
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        table->currentIndex(),
        model->index(0, SpeakingEval::toInt(SpeakingEvalColumn::KoreanName))
        );

    const auto unchanged = fixture.services.speakingEvaluationService()->evaluation(
        fixture.classIds.first(),
        QStringLiteral("Winter")
        );
    QVERIFY(unchanged);
    QCOMPARE(unchanged->at(0).at(1), QStringLiteral("Alice"));
    QCOMPARE(unchanged->at(0).at(2), QStringLiteral("\uAE40\uBBFC\uC9C0"));

    prompts.scriptedChoices.enqueue(PromptChoice::Accepted);
    QVERIFY(page.saveChanges());
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(prompts.confirmations.size(), 2);
    QCOMPARE(
        prompts.confirmations.constFirst().title,
        QStringLiteral("Verify Korean Name Lengths")
        );
    QCOMPARE(prompts.messages.size(), 1);

    const auto persisted = fixture.services.speakingEvaluationService()->evaluation(
        fixture.classIds.first(),
        QStringLiteral("Winter")
        );
    QVERIFY(persisted);
    QCOMPARE(persisted->at(0).at(1), QStringLiteral("Alice"));
    QCOMPARE(persisted->at(0).at(2), QStringLiteral("\uAE40"));
}

void SpeakingEvalPageSaveTests::
failedClassAndEvaluationSwitchesRestoreTheCurrentSelection()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(2, &error), qPrintable(error));
    QVERIFY(fixture.services.settingsService()->save(
        QStringLiteral("classes_navigation_visibility_scope"),
        QStringLiteral("all_classes")
        ));

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    SpeakingEvalPage page(&fixture.services);
    page.setDatabaseOpen(true);
    page.setSaveMode(SaveMode::Manual);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    auto* model = page.findChild<SpeakingEvalModel*>();
    auto* evaluationTabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("speakingEvalEvaluationTabs")
        );
    const QList<NavigationTabWidget*> classTabsList =
        page.findChildren<NavigationTabWidget*>(
            QStringLiteral("speakingEvalClassTabs")
            );
    QVERIFY(model);
    QVERIFY(evaluationTabs);
    QCOMPARE(classTabsList.size(), 1);
    auto* classTabs = classTabsList.constFirst();
    QCOMPARE(classTabs->count(), 2);
    QVERIFY(setStudent(model, 0, QStringLiteral("Alice"), QStringLiteral("\uAE40\uBBFC\uC9C0")));
    QVERIFY(page.hasUnsavedChanges());

    int secondClassIndex = -1;
    for (int index = 0; index < classTabs->count(); ++index)
    {
        if (classTabs->widget(index)->property("class_id").toInt()
            == fixture.classIds.last())
        {
            secondClassIndex = index;
            break;
        }
    }
    QVERIFY(secondClassIndex >= 0);

    int otherEvaluationIndex = -1;
    for (int index = 0; index < evaluationTabs->count(); ++index)
    {
        if (evaluationTabs->widget(index)->property("evaluation_name").toString()
            != QStringLiteral("Winter"))
        {
            otherEvaluationIndex = index;
            break;
        }
    }
    QVERIFY(otherEvaluationIndex >= 0);

    fixture.services.closeDatabase();
    evaluationTabs->setCurrentIndex(otherEvaluationIndex);

    QCOMPARE(
        evaluationTabs->currentWidget()->property("evaluation_name").toString(),
        QStringLiteral("Winter")
        );
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);

    classTabs->setCurrentIndex(secondClassIndex);
    QCOMPARE(
        classTabs->currentWidget()->property("class_id").toInt(),
        fixture.classIds.first()
        );
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(model->data(model->index(
        0,
        SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)
        )).toString(), QStringLiteral("Alice"));
    QCOMPARE(prompts.messages.size(), 2);
}

void SpeakingEvalPageSaveTests::
classTabsKeepOrderAndSelectedClass()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(3, &error), qPrintable(error));

    const QStringList names{
        QStringLiteral("Zulu Class"),
        QStringLiteral("Alpha Class"),
        QStringLiteral("Middle Class")
    };
    for (int index = 0; index < names.size(); ++index)
    {
        QVERIFY(fixture.services.classService()->rename(
            fixture.classIds.at(index),
            names.at(index)
            ));
    }
    ClassMngr::Next::Platform::
        ApplicationServicesClassVisibilityPreferencesPort(fixture.services)
            .save(ClassMngr::Next::Application::ClassVisibilityScope::AllClasses);

    SpeakingEvalPage page(&fixture.services);
    page.loadEvaluations();

    auto* classTabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("speakingEvalClassTabs")
        );
    QVERIFY(classTabs);
    QCOMPARE(classTabs->count(), 3);
    QCOMPARE(
        classTabs->widget(0)->property("class_id").toInt(),
        fixture.classIds.at(1)
        );
    QCOMPARE(
        classTabs->widget(1)->property("class_id").toInt(),
        fixture.classIds.at(2)
        );
    QCOMPARE(
        classTabs->widget(2)->property("class_id").toInt(),
        fixture.classIds.at(0)
        );
    QCOMPARE(
        classTabs->currentWidget()->property("class_id").toInt(),
        fixture.classIds.at(1)
        );

    page.loadEvaluations(
        fixture.classIds.at(0),
        QStringLiteral("Summer")
        );
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    classTabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("speakingEvalClassTabs")
        );
    QVERIFY(classTabs);
    QCOMPARE(
        classTabs->currentWidget()->property("class_id").toInt(),
        fixture.classIds.at(0)
        );
}

void SpeakingEvalPageSaveTests::
headerSubtitlePreservesClassAndTeacherDisplayMetadata()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("English Teacher");
    teacher.teacherKr = QStringLiteral("\uAE40\uBBFC\uC9C0");
    teacher.preferredRomanization = QStringLiteral("Romanized Teacher");
    teacher.preferredName = QStringLiteral("Romanized Teacher");
    const auto teacherId = fixture.services.teacherService()->create(teacher);
    QVERIFY(teacherId);

    auto classInfoResult =
        fixture.services.classService()->classInfo(fixture.classIds.first());
    QVERIFY(classInfoResult);
    ClassInfo classInfo = *classInfoResult;
    classInfo.teacherId = *teacherId;
    classInfo.classGrade = QStringLiteral("E5");
    classInfo.classLevel = QStringLiteral("Artemis");
    classInfo.readingBook = QStringLiteral("Reading Explorer 2");
    classInfo.essayBook = QStringLiteral("5A");
    classInfo.classTimes = {
        ClassTime{
            .day = QStringLiteral("Monday"),
            .startTime = QStringLiteral("4:00 PM"),
            .endTime = QStringLiteral("4:50 PM")
        }
    };
    QVERIFY(fixture.services.classService()->saveClassInfo(classInfo));

    classInfoResult =
        fixture.services.classService()->classInfo(fixture.classIds.first());
    const auto teacherResult =
        fixture.services.teacherService()->teacher(*teacherId);
    QVERIFY(classInfoResult);
    QVERIFY(teacherResult);

    SpeakingEvalPage page(&fixture.services);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    auto* header = page.findChild<PageHeader*>();
    QVERIFY(header);
    const QString expectedSubtitle =
        SidebarNodeNaming::formatClassDisplayName(
            *classInfoResult,
            *teacherResult
            );
    QVERIFY(expectedSubtitle.contains(QStringLiteral("E5 Artemis")));
    QVERIFY(expectedSubtitle.contains(QStringLiteral("Romanized Teacher")));
    QVERIFY(expectedSubtitle.contains(QStringLiteral("Mon")));
    QVERIFY(expectedSubtitle.contains(QStringLiteral("4:00")));
    QCOMPARE(header->subtitle(), expectedSubtitle);
}

void SpeakingEvalPageSaveTests::
scheduleModesAndVisibilityScopeFilterNavigationTabs()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(4, &error), qPrintable(error));

    const QStringList names{
        QStringLiteral("Alpha Regular"),
        QStringLiteral("Beta Intensive"),
        QStringLiteral("Gamma Both"),
        QStringLiteral("Delta Unscheduled")
    };
    for (int index = 0; index < names.size(); ++index)
    {
        QVERIFY(fixture.services.classService()->rename(
            fixture.classIds.at(index),
            names.at(index)
            ));
    }

    const QList<ClassTime> regularMeetings{
        {
            .day = QStringLiteral("Monday"),
            .startTime = QStringLiteral("4:00 PM"),
            .endTime = QStringLiteral("4:50 PM")
        },
        {
            .day = QStringLiteral("Monday"),
            .startTime = QStringLiteral("6:00 PM"),
            .endTime = QStringLiteral("6:50 PM")
        }
    };
    const QList<ClassTime> intensiveMeetings{
        {
            .day = QStringLiteral("Monday"),
            .startTime = QStringLiteral("5:00 PM"),
            .endTime = QStringLiteral("5:50 PM")
        },
        {
            .day = QStringLiteral("Monday"),
            .startTime = QStringLiteral("7:00 PM"),
            .endTime = QStringLiteral("7:50 PM")
        }
    };
    for (int index = 0; index < fixture.classIds.size(); ++index)
    {
        const auto loadedInfo =
            fixture.services.classService()->classInfo(
                fixture.classIds.at(index)
                );
        QVERIFY(loadedInfo);

        ClassInfo classInfo = *loadedInfo;
        if (index == 0)
        {
            classInfo.classTimes = {regularMeetings.at(0)};
        }
        else if (index == 1)
        {
            classInfo.intensiveTimes = {intensiveMeetings.at(0)};
        }
        else if (index == 2)
        {
            classInfo.classTimes = {regularMeetings.at(1)};
            classInfo.intensiveTimes = {intensiveMeetings.at(1)};
        }
        const Status saved =
            fixture.services.databaseSession()->classInfoRepository()
                ->saveClassInfo(classInfo);
        if (!saved)
        {
            const QString saveError = saved.error();
            QFAIL(qPrintable(saveError));
        }
    }

    ClassMngr::Next::Platform::
        ApplicationServicesClassVisibilityPreferencesPort visibilityPreferences(
            fixture.services
            );
    visibilityPreferences.save(
        ClassMngr::Next::Application::ClassVisibilityScope::ActiveSchedule
        );

    SpeakingEvalPage page(&fixture.services);
    page.loadEvaluations(
        fixture.classIds.at(2),
        QStringLiteral("Winter")
        );

    const auto visibleClassIds = [&page]()
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        auto* classTabs = page.findChild<NavigationTabWidget*>(
            QStringLiteral("speakingEvalClassTabs")
            );
        QList<int> classIds;
        if (!classTabs)
        {
            return classIds;
        }

        classIds.reserve(classTabs->count());
        for (int index = 0; index < classTabs->count(); ++index)
        {
            classIds.append(
                classTabs->widget(index)->property("class_id").toInt()
                );
        }
        return classIds;
    };

    const QList<int> regularClassIds{
        fixture.classIds.at(0),
        fixture.classIds.at(2)
    };
    QCOMPARE(visibleClassIds(), regularClassIds);

    page.setScheduleDisplayMode(ScheduleDisplayMode::Intensive);
    const QList<int> intensiveClassIds{
        fixture.classIds.at(1),
        fixture.classIds.at(2)
    };
    QCOMPARE(visibleClassIds(), intensiveClassIds);
    auto* classTabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("speakingEvalClassTabs")
        );
    QVERIFY(classTabs);
    QCOMPARE(
        classTabs->currentWidget()->property("class_id").toInt(),
        fixture.classIds.at(2)
        );

    visibilityPreferences.save(
        ClassMngr::Next::Application::ClassVisibilityScope::AllClasses
        );
    page.refreshNavigationPreferences();
    const QList<int> allClassIds{
        fixture.classIds.at(0),
        fixture.classIds.at(1),
        fixture.classIds.at(2),
        fixture.classIds.at(3)
    };
    QCOMPARE(visibleClassIds(), allClassIds);

    visibilityPreferences.save(
        ClassMngr::Next::Application::ClassVisibilityScope::ActiveSchedule
        );
    page.refreshNavigationPreferences();
    QCOMPARE(visibleClassIds(), intensiveClassIds);
    classTabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("speakingEvalClassTabs")
        );
    QVERIFY(classTabs);
    QCOMPARE(
        classTabs->currentWidget()->property("class_id").toInt(),
        fixture.classIds.at(2)
        );
}

void SpeakingEvalPageSaveTests::
unavailableClassListClearsThePageWithoutWarning()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    SpeakingEvalPage page(&fixture.services);
    ClassMngr::Next::Platform::
        ApplicationServicesClassVisibilityPreferencesPort(fixture.services)
            .save(ClassMngr::Next::Application::ClassVisibilityScope::AllClasses);
    SpeakingEvalRows persistedRows = SpeakingEval::emptyRows();
    persistedRows[0][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("Persisted");
    persistedRows[0][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QStringLiteral("\uAE40\uBBFC\uC9C0");
    QVERIFY(fixture.services.speakingEvaluationService()->saveEvaluation(
        fixture.classIds.first(),
        QStringLiteral("Winter"),
        persistedRows
        ));
    page.loadEvaluations();

    auto* classTabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("speakingEvalClassTabs")
        );
    auto* table = page.findChild<SpeakingEvalTableView*>();
    auto* model = page.findChild<SpeakingEvalModel*>();
    QVERIFY(classTabs);
    QVERIFY(table);
    QVERIFY(model);
    QCOMPARE(classTabs->count(), 1);
    QVERIFY(table->isEnabled());
    QCOMPARE(model->data(model->index(0, 1)).toString(), QStringLiteral("Persisted"));

    fixture.services.closeDatabase();
    page.loadEvaluations();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    classTabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("speakingEvalClassTabs")
        );

    QCOMPARE(prompts.messages.size(), 0);
    QVERIFY(classTabs);
    QCOMPARE(classTabs->count(), 0);
    QVERIFY(!table->isEnabled());
    QVERIFY(model->data(model->index(0, 1)).toString().isEmpty());
}

void SpeakingEvalPageSaveTests::
failedClassListWarnsAndDisablesEditing()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    SpeakingEvalPage page(&fixture.services);
    page.loadEvaluations();

    auto* table = page.findChild<SpeakingEvalTableView*>();
    QVERIFY(table);
    QVERIFY(table->isEnabled());

    QSqlQuery dropClasses(fixture.services.databaseSession()->database());
    QVERIFY2(
        dropClasses.exec(QStringLiteral("DROP TABLE classes")),
        qPrintable(dropClasses.lastError().text())
        );
    page.loadEvaluations();

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Load Speaking Evaluations")
        );
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral("Classes could not be loaded.")
        );
    QVERIFY(!prompts.messages.constFirst().details.isEmpty());
    QVERIFY(!table->isEnabled());
}

void SpeakingEvalPageSaveTests::
failedNavigationReadKeepsNameOnlyTabs()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));

    ClassInfo classInfo;
    const auto loadedInfo =
        fixture.services.classService()->classInfo(fixture.classIds.first());
    QVERIFY(loadedInfo);
    classInfo = *loadedInfo;
    classInfo.classGrade = QStringLiteral("E2");
    classInfo.classLevel = QStringLiteral("Advanced");
    classInfo.classTimes = {
        ClassTime{
            .day = QStringLiteral("Monday"),
            .startTime = QStringLiteral("4:00 PM"),
            .endTime = QStringLiteral("4:50 PM")
        }
    };
    classInfo.intensiveTimes = {
        ClassTime{
            .day = QStringLiteral("Tuesday"),
            .startTime = QStringLiteral("6:00 PM"),
            .endTime = QStringLiteral("6:50 PM")
        }
    };
    QVERIFY(fixture.services.databaseSession()->classInfoRepository()
                ->saveClassInfo(classInfo));
    ClassMngr::Next::Platform::
        ApplicationServicesClassVisibilityPreferencesPort(fixture.services)
            .save(ClassMngr::Next::Application::ClassVisibilityScope::AllClasses);

    SpeakingEvalPage page(&fixture.services);
    page.loadEvaluations();

    auto* classTabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("speakingEvalClassTabs")
        );
    QVERIFY(classTabs);
    QCOMPARE(classTabs->count(), 1);
    QVERIFY(classTabs->tabText(0).contains(QStringLiteral("E2")));
    QVERIFY(classTabs->tabText(0).contains(QStringLiteral("4:00")));

    fixture.services.closeDatabase();
    page.setScheduleDisplayMode(ScheduleDisplayMode::Intensive);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    classTabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("speakingEvalClassTabs")
        );

    QVERIFY(classTabs);
    QCOMPARE(classTabs->count(), 1);
    QCOMPARE(
        classTabs->widget(0)->property("class_id").toInt(),
        fixture.classIds.first()
        );
    QVERIFY(classTabs->tabText(0).contains(fixture.classNames.first()));
    QVERIFY(classTabs->tabText(0).contains(QStringLiteral("No time")));
    QVERIFY(!classTabs->tabText(0).contains(QStringLiteral("E2")));
    QVERIFY(!classTabs->tabText(0).contains(QStringLiteral("6:00")));
}

void SpeakingEvalPageSaveTests::
successfulLoadPreservesOrderedUnicodeMatrixAndCleanState()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));

    SpeakingEvalRows expected = SpeakingEval::emptyRows();
    expected[0][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("First");
    expected[0][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QStringLiteral("\uAE40\uBBFC\uC9C0");
    expected[7][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("Commenter");
    expected[7][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QStringLiteral("\uAE40\uBBFC\uC9C0");
    expected[7][SpeakingEval::toInt(SpeakingEvalColumn::Comments)] =
        QStringLiteral("Review \U0001F4DA");
    expected[24][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("Last");
    expected[24][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QStringLiteral("\uAE40\uBBFC\uC9C0");
    expected[24][SpeakingEval::toInt(SpeakingEvalColumn::Notes)] =
        QStringLiteral("\uC218\uC5C5 \U0001F4DA");
    QVERIFY(fixture.services.speakingEvaluationService()->saveEvaluation(
        fixture.classIds.first(),
        QStringLiteral("Winter"),
        expected
        ));

    SpeakingEvalPage page(&fixture.services);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    auto* model = page.findChild<SpeakingEvalModel*>();
    QVERIFY(model);
    QCOMPARE(model->rowCount(), 25);
    QCOMPARE(model->columnCount(), 11);
    QCOMPARE(model->data(model->index(0, 1)).toString(), QStringLiteral("First"));
    QCOMPARE(model->data(model->index(0, 2)).toString(), QStringLiteral("\uAE40\uBBFC\uC9C0"));
    QCOMPARE(model->data(model->index(7, 9)).toString(), QStringLiteral("Review \U0001F4DA"));
    QCOMPARE(model->data(model->index(24, 1)).toString(), QStringLiteral("Last"));
    QCOMPARE(model->data(model->index(24, 10)).toString(), QStringLiteral("\uC218\uC5C5 \U0001F4DA"));
    QVERIFY(!page.hasUnsavedChanges());
}

void SpeakingEvalPageSaveTests::
emptyReadFallsBackToBlankGridAndCleanState()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));

    SpeakingEvalPage page(&fixture.services);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    auto* model = page.findChild<SpeakingEvalModel*>();
    QVERIFY(model);
    QCOMPARE(model->rowCount(), 25);
    QCOMPARE(model->columnCount(), 11);
    QVERIFY(model->data(model->index(0, 1)).toString().isEmpty());
    QVERIFY(model->data(model->index(24, 10)).toString().isEmpty());
    QVERIFY(!page.hasUnsavedChanges());
}

void SpeakingEvalPageSaveTests::
failedReadFallsBackToBlankGridAndCleanState()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));

    SpeakingEvalRows expected = SpeakingEval::emptyRows();
    expected[0][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("Persisted");
    expected[0][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QStringLiteral("\uAE40\uBBFC\uC9C0");
    QVERIFY(fixture.services.speakingEvaluationService()->saveEvaluation(
        fixture.classIds.first(),
        QStringLiteral("Winter"),
        expected
        ));

    SpeakingEvalPage page(&fixture.services);
    page.setSaveMode(SaveMode::Manual);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    auto* model = page.findChild<SpeakingEvalModel*>();
    QVERIFY(model);
    QCOMPARE(model->data(model->index(0, 1)).toString(), QStringLiteral("Persisted"));
    QVERIFY(model->setData(model->index(0, 1), QStringLiteral("Unsaved"), Qt::EditRole));
    QVERIFY(page.hasUnsavedChanges());

    QSqlQuery dropDataTable(fixture.services.databaseSession()->database());
    QVERIFY2(
        dropDataTable.exec(QStringLiteral("DROP TABLE speaking_eval_data")),
        qPrintable(dropDataTable.lastError().text())
        );

    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    QCOMPARE(model->rowCount(), 25);
    QCOMPARE(model->columnCount(), 11);
    QVERIFY(model->data(model->index(0, 1)).toString().isEmpty());
    QVERIFY(model->data(model->index(24, 10)).toString().isEmpty());
    QVERIFY(!page.hasUnsavedChanges());
}

void SpeakingEvalPageSaveTests::
speakingEvalModelSuggestionMatchesLegacyHelper()
{
    SpeakingEvalRows input = SpeakingEval::emptyRows();
    const int englishColumn = SpeakingEval::toInt(SpeakingEvalColumn::EnglishName);
    const int koreanColumn = SpeakingEval::toInt(SpeakingEvalColumn::KoreanName);
    input[0][englishColumn] = QStringLiteral(" Alex ");
    input[0][koreanColumn] = QStringLiteral("\uAE40 \uBBFC\uC218 (a)");
    input[1][englishColumn] = QStringLiteral("Alex");
    input[1][koreanColumn] = QStringLiteral("\uAE40\uBBFC\uC218(c)");
    input[2][englishColumn] = QStringLiteral("alex");
    input[2][koreanColumn] = QStringLiteral("\uAE40\uBBFC\uC218(A)");
    input[3][englishColumn] = QStringLiteral("Alex");
    input[3][koreanColumn] = QStringLiteral("invalid name(A)");

    SpeakingEvalModel model;
    model.loadData(input);
    const SpeakingEvalRows normalizedRows = model.rows();
    const QString legacySuggestion = StudentNameUtils::suggestedKoreanNameWithSuffix(
        normalizedRows,
        0,
        englishColumn,
        koreanColumn
        );
    QCOMPARE(model.suggestedKoreanNameWithSuffix(0), legacySuggestion);
    QCOMPARE(model.suggestedKoreanNameWithSuffix(0), QStringLiteral("\uAE40\uBBFC\uC218(B)"));
    QVERIFY(model.suggestedKoreanNameWithSuffix(-1).isEmpty());
    QVERIFY(model.suggestedKoreanNameWithSuffix(model.rowCount()).isEmpty());
    QVERIFY(model.suggestedKoreanNameWithSuffix(4).isEmpty());

    const QStringList unusualKoreanNames{
        QStringLiteral("  \uAE40 \uBBFC\uC218 (a)\u3000"),
        QStringLiteral("invalid name(a)\u3000"),
        QStringLiteral("\uAE40\uBBFC\uC218(A)") + QChar(0x3000),
        QString(QChar(0xd800)) + QStringLiteral("\uAE40\uBBFC\uC218(A)")
    };
    for (const QString& koreanName : unusualKoreanNames)
    {
        SpeakingEvalRows unusualRows = SpeakingEval::emptyRows();
        unusualRows[0][englishColumn] = QStringLiteral("Alex");
        unusualRows[0][koreanColumn] = koreanName;
        unusualRows[1][englishColumn] = QStringLiteral("Alex");
        unusualRows[1][koreanColumn] = koreanName;

        model.loadData(unusualRows);
        const SpeakingEvalRows projectedRows = model.rows();
        QCOMPARE(
            model.suggestedKoreanNameWithSuffix(0),
            StudentNameUtils::suggestedKoreanNameWithSuffix(
                projectedRows,
                0,
                englishColumn,
                koreanColumn
                )
            );
    }
}

void SpeakingEvalPageSaveTests::
suffixChoiceAppliesSuggestedNameThroughExistingPageFlow()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    SpeakingEvalPage page(&fixture.services);
    page.setDatabaseOpen(true);
    page.setSaveMode(SaveMode::Manual);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    auto* model = page.findChild<SpeakingEvalModel*>();
    auto* table = page.findChild<SpeakingEvalTableView*>();
    QVERIFY(model);
    QVERIFY(table);

    QVERIFY(setStudent(
        model,
        0,
        QStringLiteral("Alex"),
        QStringLiteral("\uAE40\uBBFC\uC218(A)")
        ));
    prompts.scriptedActionIds.enqueue(QStringLiteral("suffix"));
    QVERIFY(setStudent(
        model,
        1,
        QStringLiteral("Alex"),
        QStringLiteral("\uAE40\uBBFC\uC218(A)")
        ));

    QCOMPARE(prompts.actionPrompts.size(), 1);
    const ActionPromptRequest& request = prompts.actionPrompts.constFirst();
    QCOMPARE(request.defaultActionId, QStringLiteral("suffix"));
    const auto suffixAction = std::find_if(
        request.actions.cbegin(),
        request.actions.cend(),
        [](const PromptAction& action)
        {
            return action.id == QStringLiteral("suffix");
        }
        );
    QVERIFY(suffixAction != request.actions.cend());
    QVERIFY(suffixAction->enabled);
    QCOMPARE(suffixAction->text, QStringLiteral("Use \uAE40\uBBFC\uC218(B)"));

    QCOMPARE(
        model->data(
            model->index(1, SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)),
            Qt::EditRole
            ).toString(),
        QStringLiteral("\uAE40\uBBFC\uC218(B)")
        );
    QCOMPARE(
        table->currentIndex(),
        model->index(1, SpeakingEval::toInt(SpeakingEvalColumn::KoreanName))
        );
}

void SpeakingEvalPageSaveTests::duplicateLocateSelectsFirstPeerRow()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    SpeakingEvalPage page(&fixture.services);
    page.setDatabaseOpen(true);
    page.setSaveMode(SaveMode::Manual);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    auto* model = page.findChild<SpeakingEvalModel*>();
    auto* table = page.findChild<SpeakingEvalTableView*>();
    QVERIFY(model);
    QVERIFY(table);
    QVERIFY(model->duplicateNameRows(-1).isEmpty());
    QVERIFY(model->duplicateNameRows(model->rowCount()).isEmpty());

    QVERIFY(setStudent(
        model,
        0,
        QStringLiteral("Alex"),
        QStringLiteral("\uAE40\uBBFC\uC218")
        ));
    prompts.scriptedActionIds.enqueue(QStringLiteral("keep"));
    QVERIFY(setStudent(
        model,
        1,
        QStringLiteral("Alex"),
        QStringLiteral("\uAE40\uBBFC\uC218")
        ));

    QCOMPARE(model->duplicateNameRows(1), QList<int>{0});
    QCOMPARE(prompts.actionPrompts.size(), 1);
    prompts.scriptedActionIds.enqueue(QStringLiteral("locate"));
    QVERIFY(setStudent(
        model,
        2,
        QStringLiteral("Alex"),
        QStringLiteral("\uAE40\uBBFC\uC218")
        ));

    QCOMPARE(model->duplicateNameRows(2), (QList<int>{0, 1}));
    QCOMPARE(prompts.actionPrompts.size(), 2);
    QCOMPARE(
        table->currentIndex(),
        model->index(0, SpeakingEval::toInt(SpeakingEvalColumn::EnglishName))
        );
}

void SpeakingEvalPageSaveTests::
duplicateRosterMatchesKeepTrimmedFirstSeenOrder()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));

    Roster roster;
    roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean")
    };
    roster.rows = {
        {QStringLiteral(" Current "), QStringLiteral(" 김민수 ")},
        {QStringLiteral(" Zoe "), QStringLiteral(" 조은 ")},
        {QStringLiteral("Amy"), QStringLiteral("김희")},
        {QStringLiteral("Zoe"), QStringLiteral("조은")},
        {QStringLiteral(" "), QStringLiteral("정하")}
    };
    QVERIFY2(
        seedRosterWithoutValidation(
            fixture.services.databaseSession()->database(),
            fixture.classIds.first(),
            roster,
            &error
            ),
        qPrintable(error)
        );

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    SpeakingEvalPage page(&fixture.services);
    page.setSaveMode(SaveMode::Manual);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );
    auto* model = page.findChild<SpeakingEvalModel*>();
    QVERIFY(model);
    QVERIFY(setStudent(
        model,
        0,
        QStringLiteral("Current"),
        QStringLiteral("김민수")
        ));
    QVERIFY(setStudent(
        model,
        1,
        QStringLiteral("Other"),
        QStringLiteral("이수민")
        ));

    QStringList rosterOptions;
    bool dialogShown = false;
    QTimer::singleShot(
        0,
        &page,
        [&rosterOptions, &dialogShown]
        {
            auto* const dialog = qobject_cast<QInputDialog*>(
                QApplication::activeModalWidget()
                );
            if (!dialog)
            {
                return;
            }
            dialogShown = dialog->windowTitle()
                == QStringLiteral("Match Roster Student");
            if (auto* const choices = dialog->findChild<QComboBox*>())
            {
                for (int index = 0; index < choices->count(); ++index)
                {
                    rosterOptions.append(choices->itemText(index));
                }
            }
            dialog->accept();
        }
        );
    prompts.scriptedActionIds.enqueue(QStringLiteral("match"));
    QVERIFY(model->setData(
        model->index(1, SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)),
        QStringLiteral("Current"),
        Qt::EditRole
        ));
    QVERIFY(model->setData(
        model->index(1, SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)),
        QStringLiteral("김민수"),
        Qt::EditRole
        ));

    QVERIFY(dialogShown);
    QCOMPARE(
        rosterOptions,
        (QStringList{
            QStringLiteral("Zoe / 조은"),
            QStringLiteral("Amy / 김희")
        })
        );
    QCOMPARE(
        model->data(
            model->index(1, SpeakingEval::toInt(SpeakingEvalColumn::EnglishName))
            ).toString(),
        QStringLiteral("Zoe")
        );
    QCOMPARE(
        model->data(
            model->index(1, SpeakingEval::toInt(SpeakingEvalColumn::KoreanName))
            ).toString(),
        QStringLiteral("조은")
        );
}

void SpeakingEvalPageSaveTests::
importNamesButtonAppliesRosterPairsAndPreservesMessages()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));

    Roster roster;
    roster.columns = {
        QStringLiteral("english"),
        QStringLiteral("English"),
        QStringLiteral("kOrEaN")
    };
    roster.rows = {
        {
            QStringLiteral(" Alice "),
            QStringLiteral("Wrong Alice"),
            QStringLiteral(" \uAE40\uBBFC\uC9C0 ")
        },
        {
            QStringLiteral("Bob"),
            QStringLiteral("Wrong Bob"),
            QStringLiteral("\uC774\uC608\uC740")
        }
    };
    // Keep duplicate mixed-case headers in storage so the page's
    // case-insensitive first-column lookup is exercised before normalization.
    QVERIFY2(
        seedRosterWithoutValidation(
            fixture.services.databaseSession()->database(),
            fixture.classIds.first(),
            roster,
            &error
            ),
        qPrintable(error)
        );

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    SpeakingEvalPage page(&fixture.services);
    page.setDatabaseOpen(true);
    page.setSaveMode(SaveMode::Manual);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    auto* model = page.findChild<SpeakingEvalModel*>();
    QVERIFY(model);
    QVERIFY(!page.hasUnsavedChanges());

    const QList<QPushButton*> buttons = page.findChildren<QPushButton*>();
    QPushButton* importNamesButton = nullptr;
    for (QPushButton* button : buttons)
    {
        if (button->text() == QStringLiteral("Import Names"))
        {
            QVERIFY(!importNamesButton);
            importNamesButton = button;
        }
    }
    QVERIFY(importNamesButton);
    QVERIFY(importNamesButton->isEnabled());

    page.show();
    QCoreApplication::processEvents();
    QTest::mouseClick(importNamesButton, Qt::LeftButton);

    const int englishColumn = SpeakingEval::toInt(SpeakingEvalColumn::EnglishName);
    const int koreanColumn = SpeakingEval::toInt(SpeakingEvalColumn::KoreanName);
    QCOMPARE(model->data(model->index(0, englishColumn)).toString(), QStringLiteral("Alice"));
    QCOMPARE(model->data(model->index(0, koreanColumn)).toString(), QStringLiteral("\uAE40\uBBFC\uC9C0"));
    QCOMPARE(model->data(model->index(1, englishColumn)).toString(), QStringLiteral("Bob"));
    QCOMPARE(model->data(model->index(1, koreanColumn)).toString(), QStringLiteral("\uC774\uC608\uC740"));
    QVERIFY(model->data(model->index(2, englishColumn)).toString().isEmpty());
    QVERIFY(model->data(model->index(2, koreanColumn)).toString().isEmpty());
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Import Names"));
    QCOMPARE(prompts.messages.constFirst().message, QStringLiteral("Roster names imported successfully."));
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Information);

    QTest::mouseClick(importNamesButton, Qt::LeftButton);

    QCOMPARE(model->data(model->index(0, englishColumn)).toString(), QStringLiteral("Alice"));
    QCOMPARE(model->data(model->index(1, englishColumn)).toString(), QStringLiteral("Bob"));
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 2);
    QCOMPARE(prompts.messages.constLast().title, QStringLiteral("Import Names"));
    QCOMPARE(prompts.messages.constLast().message, QStringLiteral("Names are already up to date."));
    QCOMPARE(prompts.messages.constLast().severity, PromptSeverity::Information);
    QVERIFY(prompts.actionPrompts.isEmpty());
}

void SpeakingEvalPageSaveTests::
emptyRosterImportShowsNoDataWarningWithoutMutation()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));

    Roster roster;
    roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean")
    };
    QVERIFY2(
        seedRosterWithoutValidation(
            fixture.services.databaseSession()->database(),
            fixture.classIds.first(),
            roster,
            &error
            ),
        qPrintable(error)
        );

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    SpeakingEvalPage page(&fixture.services);
    page.setSaveMode(SaveMode::Manual);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    auto* model = page.findChild<SpeakingEvalModel*>();
    QPushButton* importNamesButton = findImportNamesButton(page);
    QVERIFY(model);
    QVERIFY(importNamesButton);
    const SpeakingEvalRows before = model->rows();
    const bool wasDirty = page.hasUnsavedChanges();

    page.show();
    QCoreApplication::processEvents();
    QTest::mouseClick(importNamesButton, Qt::LeftButton);

    QCOMPARE(model->rows(), before);
    QCOMPARE(page.hasUnsavedChanges(), wasDirty);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Import Names"));
    QCOMPARE(prompts.messages.constFirst().message, QStringLiteral("No roster data found."));
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
}

void SpeakingEvalPageSaveTests::
unavailableRosterImportShowsNoDataWarningWithoutMutation()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));

    Roster roster;
    roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean")
    };
    roster.rows = {
        {
            QStringLiteral("Roster Student"),
            QStringLiteral("\uAE40\uBBFC\uC9C0")
        }
    };
    QVERIFY2(
        seedRosterWithoutValidation(
            fixture.services.databaseSession()->database(),
            fixture.classIds.first(),
            roster,
            &error
            ),
        qPrintable(error)
        );

    SpeakingEvalPage page(&fixture.services);
    page.setSaveMode(SaveMode::Manual);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    QSqlQuery dropDataTable(fixture.services.databaseSession()->database());
    QVERIFY2(
        dropDataTable.exec(QStringLiteral("DROP TABLE roster_data")),
        qPrintable(dropDataTable.lastError().text())
        );

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);
    auto* model = page.findChild<SpeakingEvalModel*>();
    QPushButton* importNamesButton = findImportNamesButton(page);
    QVERIFY(model);
    QVERIFY(importNamesButton);
    const SpeakingEvalRows before = model->rows();
    const bool wasDirty = page.hasUnsavedChanges();

    page.show();
    QCoreApplication::processEvents();
    QTest::mouseClick(importNamesButton, Qt::LeftButton);

    QCOMPARE(model->rows(), before);
    QCOMPARE(page.hasUnsavedChanges(), wasDirty);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Import Names"));
    QCOMPARE(prompts.messages.constFirst().message, QStringLiteral("No roster data found."));
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
}

void SpeakingEvalPageSaveTests::
missingImportColumnsShowsWarningWithoutMutation()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));

    Roster roster;
    roster.columns = {
        QStringLiteral("Notes")
    };
    roster.rows = {
        {
            QStringLiteral("Unrelated roster data")
        }
    };
    QVERIFY2(
        seedRosterWithoutValidation(
            fixture.services.databaseSession()->database(),
            fixture.classIds.first(),
            roster,
            &error
            ),
        qPrintable(error)
        );

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    SpeakingEvalPage page(&fixture.services);
    page.setSaveMode(SaveMode::Manual);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    auto* model = page.findChild<SpeakingEvalModel*>();
    QPushButton* importNamesButton = findImportNamesButton(page);
    QVERIFY(model);
    QVERIFY(importNamesButton);
    const SpeakingEvalRows before = model->rows();
    const bool wasDirty = page.hasUnsavedChanges();

    page.show();
    QCoreApplication::processEvents();
    QTest::mouseClick(importNamesButton, Qt::LeftButton);

    QCOMPARE(model->rows(), before);
    QCOMPARE(page.hasUnsavedChanges(), wasDirty);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Import Names"));
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral("Roster must contain 'English' and 'Korean' columns.")
        );
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
}

void SpeakingEvalPageSaveTests::
reportClassInfoProjectionMapsSubtitleFields()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));
    QVERIFY2(
        saveSubtitleForReportTests(
            fixture.services,
            fixture.classIds.first(),
            QStringLiteral("E5"),
            QStringLiteral("Athena"),
            QStringLiteral("English Teacher"),
            QStringLiteral("\uAE40\uBBFC\uC9C0"),
            &error
            ),
        qPrintable(error)
        );

    const ClassInfo projected =
        readReportClassInfoForSpeakingEvaluationPage(
            &fixture.services,
            fixture.classIds.first()
            );

    QCOMPARE(projected.classGrade, QStringLiteral("E5"));
    QCOMPARE(projected.classLevel, QStringLiteral("Athena"));
    QCOMPARE(projected.classTimes.size(), 2);
    QCOMPARE(projected.classTimes.at(0).day, QStringLiteral("Monday"));
    QCOMPARE(projected.classTimes.at(0).startTime, QStringLiteral("4:00 PM"));
    QVERIFY(projected.classTimes.at(0).endTime.isEmpty());
    QCOMPARE(projected.classTimes.at(1).day, QStringLiteral("Wednesday"));
    QCOMPARE(projected.classTimes.at(1).startTime, QStringLiteral("5:00 PM"));
    QVERIFY(projected.classTimes.at(1).endTime.isEmpty());
    QCOMPARE(projected.teacherEn, QStringLiteral("English Teacher"));
    QCOMPARE(projected.teacherKr, QStringLiteral("\uAE40\uBBFC\uC9C0"));
    QVERIFY(projected.intensiveTimes.isEmpty());
}

void SpeakingEvalPageSaveTests::
reportClassInfoProjectionDefaultsWhenClassFieldsFail()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));
    QVERIFY2(
        saveSubtitleForReportTests(
            fixture.services,
            fixture.classIds.first(),
            QStringLiteral("E5"),
            QStringLiteral("Athena"),
            QStringLiteral("English Teacher"),
            QStringLiteral("\uAE40\uBBFC\uC9C0"),
            &error
            ),
        qPrintable(error)
        );

    QSqlQuery dropRegularSchedule(
        fixture.services.databaseSession()->database()
        );
    QVERIFY2(
        dropRegularSchedule.exec(QStringLiteral("DROP TABLE class_times")),
        qPrintable(dropRegularSchedule.lastError().text())
        );

    const ClassInfo projected =
        readReportClassInfoForSpeakingEvaluationPage(
            &fixture.services,
            fixture.classIds.first()
            );
    QVERIFY(projected.classGrade.isEmpty());
    QVERIFY(projected.classLevel.isEmpty());
    QVERIFY(projected.classTimes.isEmpty());
    QVERIFY(projected.teacherEn.isEmpty());
    QVERIFY(projected.teacherKr.isEmpty());
}

void SpeakingEvalPageSaveTests::
reportClassInfoProjectionDefaultsWhenOuterReadFails()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));
    QVERIFY2(
        saveSubtitleForReportTests(
            fixture.services,
            fixture.classIds.first(),
            QStringLiteral("E5"),
            QStringLiteral("Athena"),
            QStringLiteral("English Teacher"),
            QStringLiteral("\uAE40\uBBFC\uC9C0"),
            &error
            ),
        qPrintable(error)
        );

    fixture.services.closeDatabase();
    const ClassInfo projected =
        readReportClassInfoForSpeakingEvaluationPage(
            &fixture.services,
            fixture.classIds.first()
            );
    QVERIFY(projected.classGrade.isEmpty());
    QVERIFY(projected.classLevel.isEmpty());
    QVERIFY(projected.classTimes.isEmpty());
    QVERIFY(projected.teacherEn.isEmpty());
    QVERIFY(projected.teacherKr.isEmpty());
}

void SpeakingEvalPageSaveTests::
reportClassInfoProjectionKeepsClassFieldsWhenTeacherReadFails()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));
    QVERIFY2(
        saveSubtitleForReportTests(
            fixture.services,
            fixture.classIds.first(),
            QStringLiteral("E5"),
            QStringLiteral("Athena"),
            QStringLiteral("English Teacher"),
            QStringLiteral("\uAE40\uBBFC\uC9C0"),
            &error
            ),
        qPrintable(error)
        );

    QSqlQuery disableForeignKeys(
        fixture.services.databaseSession()->database()
        );
    QVERIFY2(
        disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")),
        qPrintable(disableForeignKeys.lastError().text())
        );
    QSqlQuery dropTeacherTable(
        fixture.services.databaseSession()->database()
        );
    QVERIFY2(
        dropTeacherTable.exec(QStringLiteral("DROP TABLE teachers")),
        qPrintable(dropTeacherTable.lastError().text())
        );

    const ClassInfo projected =
        readReportClassInfoForSpeakingEvaluationPage(
            &fixture.services,
            fixture.classIds.first()
            );
    QCOMPARE(projected.classGrade, QStringLiteral("E5"));
    QCOMPARE(projected.classLevel, QStringLiteral("Athena"));
    QCOMPARE(projected.classTimes.size(), 2);
    QCOMPARE(projected.classTimes.at(0).day, QStringLiteral("Monday"));
    QCOMPARE(projected.classTimes.at(0).startTime, QStringLiteral("4:00 PM"));
    QVERIFY(projected.teacherEn.isEmpty());
    QVERIFY(projected.teacherKr.isEmpty());
}

void SpeakingEvalPageSaveTests::
outputReportsUsesProjectedRegularSchedule()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));
    QVERIFY2(
        saveSubtitleForReportTests(
            fixture.services,
            fixture.classIds.first(),
            QStringLiteral("E5"),
            QStringLiteral("Athena"),
            QStringLiteral("English Teacher"),
            QStringLiteral("\uAE40\uBBFC\uC9C0"),
            &error
            ),
        qPrintable(error)
        );

    SpeakingEvalPage page(&fixture.services);
    page.setSaveMode(SaveMode::Manual);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );
    auto* const model = page.findChild<SpeakingEvalModel*>();
    QVERIFY(model);
    QVERIFY(setStudent(
        model,
        0,
        QStringLiteral("Alice"),
        QStringLiteral("\uAE40\uBBFC\uC9C0")
        ));

    page.show();
    QCoreApplication::processEvents();
    const OutputDirectoryCapture outputCapture =
        openSaveAsAndCaptureDirectory(page);
    QVERIFY(outputCapture.foundLineEdit);
    const QString normalizedOutputDirectory =
        QDir::fromNativeSeparators(outputCapture.directory);
    QVERIFY(normalizedOutputDirectory.contains(
        QStringLiteral("/E5 Athena (MW - 4pm)/Winter")
        ));
    QVERIFY(!normalizedOutputDirectory.contains(QStringLiteral("Tuesday")));
}

void SpeakingEvalPageSaveTests::
generateCommentsAppliesToOriginalRowAndUndoesAsOneAction()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));
    QVERIFY2(
        saveSubtitleForReportTests(
            fixture.services,
            fixture.classIds.first(),
            QStringLiteral("E5"),
            QStringLiteral("Athena"),
            QStringLiteral("English Teacher"),
            QStringLiteral("\uAE40\uBBFC\uC9C0"),
            &error
            ),
        qPrintable(error)
        );

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);

    SpeakingEvalPage page(&fixture.services);
    page.setDatabaseOpen(true);
    page.setSaveMode(SaveMode::Manual);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    auto* const model = page.findChild<SpeakingEvalModel*>();
    auto* const undoStack = page.findChild<QUndoStack*>();
    QVERIFY(model);
    QVERIFY(undoStack);

    const QString aliceComment = QStringLiteral("Alice's original comment.");
    const QString unnamedComment =
        QStringLiteral("The unnamed source row must stay unchanged.");
    const QString carolComment = QStringLiteral("Carol's original comment.");
    const QString carolKorean = QStringLiteral("\uBC15\uC11C\uC900");
    const QString notes = QStringLiteral(
        "[Did Well]\nClear pronunciation\n"
        "[Needs Improvement]\nAdd supporting details"
        );
    const int commentsColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::Comments);
    const int notesColumn = SpeakingEval::toInt(SpeakingEvalColumn::Notes);

    QVERIFY(setStudent(
        model,
        0,
        QStringLiteral("Alice"),
        QStringLiteral("\uAE40\uBBFC\uC9C0")
        ));
    QVERIFY(model->setData(model->index(0, commentsColumn), aliceComment));
    QVERIFY(model->setData(model->index(0, notesColumn), notes));
    QVERIFY(model->setData(model->index(1, commentsColumn), unnamedComment));
    QVERIFY(setStudent(
        model,
        2,
        QStringLiteral("Carol"),
        carolKorean
        ));
    QVERIFY(model->setData(model->index(2, commentsColumn), carolComment));
    QVERIFY(model->setData(model->index(2, notesColumn), notes));
    QCOMPARE(undoStack->count(), 0);

    QPushButton* generateCommentsButton = nullptr;
    for (QPushButton* const button : page.findChildren<QPushButton*>())
    {
        if (button->text() == QStringLiteral("Generate Comments"))
        {
            generateCommentsButton = button;
            break;
        }
    }
    QVERIFY(generateCommentsButton);

    bool dialogShown = false;
    bool reportRowsMatch = false;
    bool generatedIdsMatch = false;
    bool responseParsed = false;
    bool applyWasEnabled = false;
    QString generatedPrompt;
    QString reviewComment;
    QString parseSummary;

    const QString responseComment = QStringLiteral(
        "STD_NAME spoke clearly and used strong vocabulary. "
        "Keep adding supporting details and practice difficult sounds. "
        "Your eye contact and confident voice made the presentation engaging."
        );
    QString expectedComment = responseComment;
    expectedComment.replace(QStringLiteral("STD_NAME"), QStringLiteral("Carol"));

    page.show();
    QCoreApplication::processEvents();
    QTimer dialogDriver;
    dialogDriver.setInterval(10);
    QObject::connect(&dialogDriver, &QTimer::timeout, &page, [&]
    {
        auto* const dialog = qobject_cast<SpeakingEvalAiBatchDialog*>(
            QApplication::activeModalWidget()
            );
        if (!dialog)
        {
            return;
        }
        dialogDriver.stop();
        dialogShown = true;

        auto* const selection = dialog->findChild<QTableWidget*>(
            QStringLiteral("speakingEvalAiBatchSelectionTable")
            );
        auto* const createPromptButton = dialog->findChild<QPushButton*>(
            QStringLiteral("speakingEvalAiBatchCreatePrompt")
            );
        auto* const promptEdit = dialog->findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvalAiBatchPrompt")
            );
        auto* const responseEdit = dialog->findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvalAiBatchResponse")
            );
        auto* const parseButton = dialog->findChild<QPushButton*>(
            QStringLiteral("speakingEvalAiBatchParse")
            );
        auto* const review = dialog->findChild<QTableWidget*>(
            QStringLiteral("speakingEvalAiBatchReviewTable")
            );
        auto* const parseSummaryLabel = dialog->findChild<QLabel*>(
            QStringLiteral("speakingEvalAiBatchParseSummary")
            );
        auto* const applyButton = dialog->findChild<QPushButton*>(
            QStringLiteral("speakingEvalAiBatchApply")
            );

        if (
            !selection
            || !createPromptButton
            || !promptEdit
            || !responseEdit
            || !parseButton
            || !review
            || !parseSummaryLabel
            || !applyButton
            )
        {
            dialog->reject();
            return;
        }

        reportRowsMatch = selection->rowCount() == 2
            && selection->item(0, 1)->text()
                == QStringLiteral("Alice (\uAE40\uBBFC\uC9C0)")
            && selection->item(1, 1)->text()
                == QStringLiteral("Carol (%1)").arg(carolKorean);
        if (!reportRowsMatch)
        {
            dialog->reject();
            return;
        }

        selection->item(1, 0)->setCheckState(Qt::Checked);
        createPromptButton->click();
        generatedPrompt = promptEdit->toPlainText();
        generatedIdsMatch =
            generatedPrompt.contains(QStringLiteral("Student ID: STUDENT_02"))
            && !generatedPrompt.contains(QStringLiteral("STUDENT_03"));
        if (!generatedIdsMatch)
        {
            dialog->reject();
            return;
        }

        responseEdit->setPlainText(
            QStringLiteral(
                "<<<STUDENT_02>>>\n%1\n<<<END_STUDENT_02>>>"
                ).arg(responseComment)
            );
        parseButton->click();
        parseSummary = parseSummaryLabel->text();
        responseParsed = review->rowCount() == 1
            && review->item(0, 1)->text()
                == QStringLiteral("Carol (%1)").arg(carolKorean)
            && review->item(0, 2)->text() == QStringLiteral("Ready")
            && review->item(0, 4)->text() == expectedComment;
        if (review->rowCount() == 1 && review->item(0, 4))
        {
            reviewComment = review->item(0, 4)->text();
        }
        applyWasEnabled = applyButton->isEnabled();
        if (!responseParsed || !applyWasEnabled)
        {
            dialog->reject();
            return;
        }
        applyButton->click();
    });
    bool safetyCloseTriggered = false;
    QTimer safetyCloseTimer;
    safetyCloseTimer.setSingleShot(true);
    QObject::connect(&safetyCloseTimer, &QTimer::timeout, &page, [&]
    {
        if (QWidget* const modal = QApplication::activeModalWidget())
        {
            safetyCloseTriggered = true;
            modal->close();
        }
    });
    dialogDriver.start();
    safetyCloseTimer.start(5000);

    QTest::mouseClick(generateCommentsButton, Qt::LeftButton);

    QVERIFY(dialogShown);
    QVERIFY(!safetyCloseTriggered);
    QVERIFY(reportRowsMatch);
    QVERIFY(generatedIdsMatch);
    QVERIFY(generatedPrompt.contains(QStringLiteral("Student ID: STUDENT_02")));
    QVERIFY(!generatedPrompt.contains(QStringLiteral("STUDENT_03")));
    QVERIFY(responseParsed);
    QVERIFY(parseSummary.contains(QStringLiteral("1 of 1 selected students")));
    QVERIFY(applyWasEnabled);
    QCOMPARE(reviewComment, expectedComment);
    QCOMPARE(prompts.confirmations.size(), 1);
    QCOMPARE(
        prompts.confirmations.constFirst().title,
        QStringLiteral("Replace Existing Comments?")
        );
    QVERIFY(prompts.confirmations.constFirst().destructive);

    const SpeakingEvalRows afterApply = model->rows();
    QCOMPARE(afterApply.at(0).at(commentsColumn), aliceComment);
    QCOMPARE(afterApply.at(1).at(commentsColumn), unnamedComment);
    QCOMPARE(afterApply.at(2).at(commentsColumn), expectedComment);
    QCOMPARE(undoStack->count(), 1);

    undoStack->undo();
    const SpeakingEvalRows afterUndo = model->rows();
    QCOMPARE(afterUndo.at(0).at(commentsColumn), aliceComment);
    QCOMPARE(afterUndo.at(1).at(commentsColumn), unnamedComment);
    QCOMPARE(afterUndo.at(2).at(commentsColumn), carolComment);
    QCOMPARE(undoStack->index(), 0);
}

void SpeakingEvalPageSaveTests::
generateCommentsAppliesMultipleStudentsAndUndoesAsOneAction()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));
    QVERIFY2(
        saveSubtitleForReportTests(
            fixture.services,
            fixture.classIds.first(),
            QStringLiteral("E5"),
            QStringLiteral("Athena"),
            QStringLiteral("English Teacher"),
            QStringLiteral("\uAE40\uBBFC\uC9C0"),
            &error
            ),
        qPrintable(error)
        );

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);

    SpeakingEvalPage page(&fixture.services);
    page.setDatabaseOpen(true);
    page.setSaveMode(SaveMode::Manual);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    auto* const model = page.findChild<SpeakingEvalModel*>();
    auto* const undoStack = page.findChild<QUndoStack*>();
    QVERIFY(model);
    QVERIFY(undoStack);

    const QString aliceComment = QStringLiteral("Alice's original comment.");
    const QString unnamedComment =
        QStringLiteral("The unnamed source row must stay unchanged.");
    const QString carolComment = QStringLiteral("Carol's original comment.");
    const QString carolKorean = QStringLiteral("\uBC15\uC11C\uC900");
    const QString notes = QStringLiteral(
        "[Did Well]\nClear pronunciation\n"
        "[Needs Improvement]\nAdd supporting details"
        );
    const int commentsColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::Comments);
    const int notesColumn = SpeakingEval::toInt(SpeakingEvalColumn::Notes);

    QVERIFY(setStudent(
        model,
        0,
        QStringLiteral("Alice"),
        QStringLiteral("\uAE40\uBBFC\uC9C0")
        ));
    QVERIFY(model->setData(model->index(0, commentsColumn), aliceComment));
    QVERIFY(model->setData(model->index(0, notesColumn), notes));
    QVERIFY(model->setData(model->index(1, commentsColumn), unnamedComment));
    QVERIFY(setStudent(
        model,
        2,
        QStringLiteral("Carol"),
        carolKorean
        ));
    QVERIFY(model->setData(model->index(2, commentsColumn), carolComment));
    QVERIFY(model->setData(model->index(2, notesColumn), notes));
    QCOMPARE(undoStack->count(), 0);

    QPushButton* generateCommentsButton = nullptr;
    for (QPushButton* const button : page.findChildren<QPushButton*>())
    {
        if (button->text() == QStringLiteral("Generate Comments"))
        {
            generateCommentsButton = button;
            break;
        }
    }
    QVERIFY(generateCommentsButton);

    bool dialogShown = false;
    bool reportRowsMatch = false;
    bool generatedIdsMatch = false;
    bool responseRowsReadyAndSelected = false;
    bool applyWasEnabled = false;
    QString generatedPrompt;
    QString aliceReviewComment;
    QString carolReviewComment;
    QString parseSummary;

    const QString responseComment = QStringLiteral(
        "STD_NAME spoke clearly and used strong vocabulary. "
        "Keep adding supporting details and practice difficult sounds. "
        "Your eye contact and confident voice made the presentation engaging."
        );
    QString expectedAliceComment = responseComment;
    expectedAliceComment.replace(QStringLiteral("STD_NAME"), QStringLiteral("Alice"));
    QString expectedCarolComment = responseComment;
    expectedCarolComment.replace(QStringLiteral("STD_NAME"), QStringLiteral("Carol"));

    page.show();
    QCoreApplication::processEvents();
    QTimer dialogDriver;
    dialogDriver.setInterval(10);
    QObject::connect(&dialogDriver, &QTimer::timeout, &page, [&]
    {
        auto* const dialog = qobject_cast<SpeakingEvalAiBatchDialog*>(
            QApplication::activeModalWidget()
            );
        if (!dialog)
        {
            return;
        }
        dialogDriver.stop();
        dialogShown = true;

        auto* const selection = dialog->findChild<QTableWidget*>(
            QStringLiteral("speakingEvalAiBatchSelectionTable")
            );
        auto* const createPromptButton = dialog->findChild<QPushButton*>(
            QStringLiteral("speakingEvalAiBatchCreatePrompt")
            );
        auto* const promptEdit = dialog->findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvalAiBatchPrompt")
            );
        auto* const responseEdit = dialog->findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvalAiBatchResponse")
            );
        auto* const parseButton = dialog->findChild<QPushButton*>(
            QStringLiteral("speakingEvalAiBatchParse")
            );
        auto* const review = dialog->findChild<QTableWidget*>(
            QStringLiteral("speakingEvalAiBatchReviewTable")
            );
        auto* const parseSummaryLabel = dialog->findChild<QLabel*>(
            QStringLiteral("speakingEvalAiBatchParseSummary")
            );
        auto* const applyButton = dialog->findChild<QPushButton*>(
            QStringLiteral("speakingEvalAiBatchApply")
            );

        if (
            !selection
            || !createPromptButton
            || !promptEdit
            || !responseEdit
            || !parseButton
            || !review
            || !parseSummaryLabel
            || !applyButton
            )
        {
            dialog->reject();
            return;
        }

        reportRowsMatch = selection->rowCount() == 2
            && selection->item(0, 1)->text()
                == QStringLiteral("Alice (\uAE40\uBBFC\uC9C0)")
            && selection->item(1, 1)->text()
                == QStringLiteral("Carol (%1)").arg(carolKorean);
        if (!reportRowsMatch)
        {
            dialog->reject();
            return;
        }

        selection->item(0, 0)->setCheckState(Qt::Checked);
        selection->item(1, 0)->setCheckState(Qt::Checked);
        createPromptButton->click();
        generatedPrompt = promptEdit->toPlainText();
        generatedIdsMatch =
            generatedPrompt.contains(QStringLiteral("Student ID: STUDENT_01"))
            && generatedPrompt.contains(QStringLiteral("Student ID: STUDENT_02"))
            && !generatedPrompt.contains(QStringLiteral("STUDENT_03"));
        if (!generatedIdsMatch)
        {
            dialog->reject();
            return;
        }

        responseEdit->setPlainText(
            QStringLiteral(
                "<<<STUDENT_01>>>\n%1\n<<<END_STUDENT_01>>>\n"
                "<<<STUDENT_02>>>\n%2\n<<<END_STUDENT_02>>>"
                ).arg(responseComment, responseComment)
            );
        parseButton->click();
        parseSummary = parseSummaryLabel->text();
        responseRowsReadyAndSelected = review->rowCount() == 2
            && review->item(0, 0)->checkState() == Qt::Checked
            && review->item(0, 1)->text()
                == QStringLiteral("Alice (\uAE40\uBBFC\uC9C0)")
            && review->item(0, 2)->text() == QStringLiteral("Ready")
            && review->item(0, 4)->text() == expectedAliceComment
            && review->item(1, 0)->checkState() == Qt::Checked
            && review->item(1, 1)->text()
                == QStringLiteral("Carol (%1)").arg(carolKorean)
            && review->item(1, 2)->text() == QStringLiteral("Ready")
            && review->item(1, 4)->text() == expectedCarolComment;
        if (review->rowCount() == 2 && review->item(0, 4) && review->item(1, 4))
        {
            aliceReviewComment = review->item(0, 4)->text();
            carolReviewComment = review->item(1, 4)->text();
        }
        applyWasEnabled = applyButton->isEnabled();
        if (!responseRowsReadyAndSelected || !applyWasEnabled)
        {
            dialog->reject();
            return;
        }
        applyButton->click();
    });
    bool safetyCloseTriggered = false;
    QTimer safetyCloseTimer;
    safetyCloseTimer.setSingleShot(true);
    QObject::connect(&safetyCloseTimer, &QTimer::timeout, &page, [&]
    {
        if (QWidget* const modal = QApplication::activeModalWidget())
        {
            safetyCloseTriggered = true;
            modal->close();
        }
    });
    dialogDriver.start();
    safetyCloseTimer.start(5000);

    QTest::mouseClick(generateCommentsButton, Qt::LeftButton);

    QVERIFY(dialogShown);
    QVERIFY(!safetyCloseTriggered);
    QVERIFY(reportRowsMatch);
    QVERIFY(generatedIdsMatch);
    QVERIFY(generatedPrompt.contains(QStringLiteral("Student ID: STUDENT_01")));
    QVERIFY(generatedPrompt.contains(QStringLiteral("Student ID: STUDENT_02")));
    QVERIFY(!generatedPrompt.contains(QStringLiteral("STUDENT_03")));
    QVERIFY(responseRowsReadyAndSelected);
    QVERIFY(parseSummary.contains(QStringLiteral("2 of 2 selected students")));
    QVERIFY(applyWasEnabled);
    QCOMPARE(aliceReviewComment, expectedAliceComment);
    QCOMPARE(carolReviewComment, expectedCarolComment);
    QCOMPARE(prompts.confirmations.size(), 1);
    QCOMPARE(
        prompts.confirmations.constFirst().title,
        QStringLiteral("Replace Existing Comments?")
        );
    QVERIFY(prompts.confirmations.constFirst().destructive);

    const SpeakingEvalRows afterApply = model->rows();
    QCOMPARE(afterApply.at(0).at(commentsColumn), expectedAliceComment);
    QCOMPARE(afterApply.at(1).at(commentsColumn), unnamedComment);
    QCOMPARE(afterApply.at(2).at(commentsColumn), expectedCarolComment);
    QCOMPARE(undoStack->count(), 1);

    undoStack->undo();
    const SpeakingEvalRows afterUndo = model->rows();
    QCOMPARE(afterUndo.at(0).at(commentsColumn), aliceComment);
    QCOMPARE(afterUndo.at(1).at(commentsColumn), unnamedComment);
    QCOMPARE(afterUndo.at(2).at(commentsColumn), carolComment);
    QCOMPARE(undoStack->index(), 0);
}

void SpeakingEvalPageSaveTests::
cancelingGenerateCommentsLeavesPageStateUntouched()
{
    SpeakingEvalPageFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(1, &error), qPrintable(error));
    QVERIFY2(
        saveSubtitleForReportTests(
            fixture.services,
            fixture.classIds.first(),
            QStringLiteral("E5"),
            QStringLiteral("Athena"),
            QStringLiteral("English Teacher"),
            QStringLiteral("\uAE40\uBBFC\uC9C0"),
            &error
            ),
        qPrintable(error)
        );

    SpeakingEvalPage page(&fixture.services);
    page.setDatabaseOpen(true);
    page.setSaveMode(SaveMode::Manual);
    page.loadEvaluation(
        Classroom(fixture.classNames.first(), fixture.classIds.first()),
        QStringLiteral("Winter")
        );

    auto* const model = page.findChild<SpeakingEvalModel*>();
    auto* const undoStack = page.findChild<QUndoStack*>();
    QVERIFY(model);
    QVERIFY(undoStack);

    const QString originalComment =
        QStringLiteral("Alice's comment before opening the dialog.");
    const QString notes = QStringLiteral(
        "[Did Well]\nClear pronunciation\n"
        "[Needs Improvement]\nAdd supporting details"
        );
    const int commentsColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::Comments);
    const int notesColumn = SpeakingEval::toInt(SpeakingEvalColumn::Notes);
    QVERIFY(setStudent(
        model,
        0,
        QStringLiteral("Alice"),
        QStringLiteral("\uAE40\uBBFC\uC9C0")
        ));
    QVERIFY(model->setData(model->index(0, commentsColumn), originalComment));
    QVERIFY(model->setData(model->index(0, notesColumn), notes));

    const SpeakingEvalRows rowsBeforeDialog = model->rows();
    const bool dirtyBeforeDialog = page.hasUnsavedChanges();
    QVERIFY(dirtyBeforeDialog);
    QCOMPARE(undoStack->count(), 0);

    QPushButton* generateCommentsButton = nullptr;
    for (QPushButton* const button : page.findChildren<QPushButton*>())
    {
        if (button->text() == QStringLiteral("Generate Comments"))
        {
            generateCommentsButton = button;
            break;
        }
    }
    QVERIFY(generateCommentsButton);

    bool dialogShown = false;
    bool controlsFound = false;
    bool reviewReady = false;
    bool cancelClicked = false;
    bool safetyCloseTriggered = false;
    int dialogResult = -1;
    QString generatedPrompt;
    QString reviewComment;
    const QString responseComment = QStringLiteral(
        "STD_NAME spoke clearly and used strong vocabulary. "
        "Keep adding supporting details and practice difficult sounds. "
        "Your eye contact and confident voice made the presentation engaging."
        );
    QString expectedComment = responseComment;
    expectedComment.replace(QStringLiteral("STD_NAME"), QStringLiteral("Alice"));

    page.show();
    QCoreApplication::processEvents();
    QTimer dialogDriver;
    dialogDriver.setInterval(10);
    QObject::connect(&dialogDriver, &QTimer::timeout, &page, [&]
    {
        auto* const dialog = qobject_cast<SpeakingEvalAiBatchDialog*>(
            QApplication::activeModalWidget()
            );
        if (!dialog)
        {
            return;
        }
        dialogDriver.stop();
        dialogShown = true;

        auto* const selection = dialog->findChild<QTableWidget*>(
            QStringLiteral("speakingEvalAiBatchSelectionTable")
            );
        auto* const createPromptButton = dialog->findChild<QPushButton*>(
            QStringLiteral("speakingEvalAiBatchCreatePrompt")
            );
        auto* const promptEdit = dialog->findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvalAiBatchPrompt")
            );
        auto* const responseEdit = dialog->findChild<QPlainTextEdit*>(
            QStringLiteral("speakingEvalAiBatchResponse")
            );
        auto* const parseButton = dialog->findChild<QPushButton*>(
            QStringLiteral("speakingEvalAiBatchParse")
            );
        auto* const review = dialog->findChild<QTableWidget*>(
            QStringLiteral("speakingEvalAiBatchReviewTable")
            );
        auto* const applyButton = dialog->findChild<QPushButton*>(
            QStringLiteral("speakingEvalAiBatchApply")
            );
        auto* const buttonBox = dialog->findChild<QDialogButtonBox*>();
        QPushButton* const cancelButton = buttonBox
            ? buttonBox->button(QDialogButtonBox::Cancel)
            : nullptr;

        controlsFound = selection
            && createPromptButton
            && promptEdit
            && responseEdit
            && parseButton
            && review
            && applyButton
            && cancelButton;
        if (!controlsFound)
        {
            dialog->reject();
            return;
        }
        if (
            selection->rowCount() != 1
            || selection->item(0, 0)->checkState() != Qt::Unchecked
            || selection->item(0, 1)->text()
                != QStringLiteral("Alice (\uAE40\uBBFC\uC9C0)")
        )
        {
            dialog->reject();
            return;
        }

        selection->item(0, 0)->setCheckState(Qt::Checked);
        createPromptButton->click();
        generatedPrompt = promptEdit->toPlainText();
        if (
            !generatedPrompt.contains(
                QStringLiteral("Student ID: STUDENT_01")
                )
            || generatedPrompt.contains(QStringLiteral("STUDENT_02"))
        )
        {
            dialog->reject();
            return;
        }

        responseEdit->setPlainText(
            QStringLiteral(
                "<<<STUDENT_01>>>\n%1\n<<<END_STUDENT_01>>>"
                ).arg(responseComment)
            );
        parseButton->click();
        reviewReady = review->rowCount() == 1
            && review->item(0, 1)->text()
                == QStringLiteral("Alice (\uAE40\uBBFC\uC9C0)")
            && review->item(0, 2)->text() == QStringLiteral("Ready")
            && review->item(0, 0)->checkState() == Qt::Checked
            && review->item(0, 4)->text() == expectedComment
            && applyButton->isEnabled();
        if (review->rowCount() == 1 && review->item(0, 4))
        {
            reviewComment = review->item(0, 4)->text();
        }
        if (!reviewReady)
        {
            dialog->reject();
            return;
        }

        QObject::connect(
            dialog,
            &QDialog::finished,
            &page,
            [&dialogResult](const int result) { dialogResult = result; }
            );
        cancelButton->click();
        cancelClicked = true;
    });
    QTimer safetyCloseTimer;
    safetyCloseTimer.setSingleShot(true);
    QObject::connect(&safetyCloseTimer, &QTimer::timeout, &page, [&]
    {
        if (QWidget* const modal = QApplication::activeModalWidget())
        {
            safetyCloseTriggered = true;
            modal->close();
        }
    });
    dialogDriver.start();
    safetyCloseTimer.start(5000);

    QTest::mouseClick(generateCommentsButton, Qt::LeftButton);

    QVERIFY(dialogShown);
    QVERIFY(controlsFound);
    QVERIFY(reviewReady);
    QVERIFY(cancelClicked);
    QVERIFY(!safetyCloseTriggered);
    QCOMPARE(dialogResult, static_cast<int>(QDialog::Rejected));
    QVERIFY(generatedPrompt.contains(QStringLiteral("Student ID: STUDENT_01")));
    QVERIFY(!generatedPrompt.contains(QStringLiteral("STUDENT_02")));
    QCOMPARE(reviewComment, expectedComment);
    QCOMPARE(model->rows(), rowsBeforeDialog);
    QCOMPARE(
        model->data(model->index(0, commentsColumn), Qt::EditRole).toString(),
        originalComment
        );
    QCOMPARE(page.hasUnsavedChanges(), dirtyBeforeDialog);
    QCOMPARE(undoStack->count(), 0);
}

QTEST_MAIN(SpeakingEvalPageSaveTests)

#include "speaking_eval_page_save_tests.moc"
