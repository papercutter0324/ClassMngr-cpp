#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "domain/models/classroom.h"
#include "domain/models/speaking_evaluation.h"
#include "features/speaking_eval/ui/speaking_eval_model.h"
#include "features/speaking_eval/ui/speaking_eval_page.h"
#include "features/speaking_eval/ui/speaking_eval_table_view.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QSignalSpy>
#include <QPushButton>
#include <QTemporaryDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QtTest/QtTest>
#include <QUuid>

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

}

class SpeakingEvalPageSaveTests final : public QObject
{
    Q_OBJECT

private slots:
    void automaticSavePersistsMatrixWithoutInteractiveNotices();
    void manualValidationAndConfirmedSaveKeepTheirCurrentTiming();
    void failedClassAndEvaluationSwitchesRestoreTheCurrentSelection();
    void successfulLoadPreservesOrderedUnicodeMatrixAndCleanState();
    void emptyReadFallsBackToBlankGridAndCleanState();
    void failedReadFallsBackToBlankGridAndCleanState();
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
    QVERIFY(model);
    QVERIFY(table);
    QVERIFY(saveButton);

    QVERIFY(setStudent(model, 0, QStringLiteral("Alice"), QStringLiteral("\uAE40\uBBFC\uC9C0")));
    saveButton->click();
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Information);
    QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Saved"));

    QVERIFY(model->setData(
        model->index(0, SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)),
        QStringLiteral("\uAE40"),
        Qt::EditRole
        ));
    QVERIFY(page.hasUnsavedChanges());

    page.saveData();
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(prompts.confirmations.isEmpty());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        table->currentIndex(),
        model->index(0, SpeakingEval::toInt(SpeakingEvalColumn::KoreanName))
        );

    prompts.scriptedChoices.enqueue(PromptChoice::Accepted);
    QVERIFY(page.saveChanges());
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(prompts.confirmations.size(), 1);
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

QTEST_MAIN(SpeakingEvalPageSaveTests)

#include "speaking_eval_page_save_tests.moc"
