#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "domain/models/classroom.h"
#include "domain/models/speaking_evaluation.h"
#include "features/speaking_eval/ui/speaking_eval_model.h"
#include "features/speaking_eval/ui/speaking_eval_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QTemporaryDir>
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

struct SpeakingEvalPageSaveFixture final
{
    QTemporaryDir directory;
    ApplicationServices services;
    int classId = -1;
    QString className;

    bool initialize(QString* error)
    {
        if (!directory.isValid())
        {
            *error = QStringLiteral("Temporary directory is invalid.");
            return false;
        }

        const QString path = directory.filePath(
            QStringLiteral("speaking-eval-save-parity-%1.tps").arg(
                QUuid::createUuid().toString(QUuid::WithoutBraces)
                )
            );
        const Status opened = services.openDatabase(path);
        if (!opened)
        {
            *error = opened.error();
            return false;
        }

        className = QStringLiteral("Speaking Save Parity Class");
        const auto created = services.classService()->create(className);
        if (!created)
        {
            *error = created.error();
            return false;
        }
        classId = *created;
        return true;
    }
};

QString cell(
    const SpeakingEvalRows& rows,
    const int row,
    const SpeakingEvalColumn column
    )
{
    return rows.value(row).value(SpeakingEval::toInt(column));
}

}

class SpeakingEvalPageSaveParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void manualSavePreservesVisibleAndPersistedValues();
};

void SpeakingEvalPageSaveParityTests::
manualSavePreservesVisibleAndPersistedValues()
{
    SpeakingEvalPageSaveFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    const QString evaluationName = QStringLiteral("Winter");
    const QString sentinel = QStringLiteral("Keep this note");
    const QString koreanName = QStringLiteral("\uAE40\uBBFC\uC9C0");
    const QString koreanUtf8Hex = QString::fromLatin1(koreanName.toUtf8().toHex());
    SpeakingEvalRows seededRows = SpeakingEval::emptyRows();
    seededRows[0][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("Before");
    seededRows[0][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        koreanName;
    seededRows[0][SpeakingEval::toInt(SpeakingEvalColumn::Notes)] =
        sentinel;
    QVERIFY(fixture.services.speakingEvaluationService()->saveEvaluation(
        fixture.classId,
        evaluationName,
        seededRows
        ));

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    SpeakingEvalPage page(&fixture.services);
    page.setDatabaseOpen(true);
    page.setSaveMode(SaveMode::Manual);
    page.loadEvaluation(
        Classroom(fixture.className, fixture.classId),
        evaluationName
        );

    auto* const model = page.findChild<SpeakingEvalModel*>();
    auto* const saveButton = page.findChild<QPushButton*>(
        QStringLiteral("speakingEvalSaveButton")
        );
    QVERIFY(model);
    QVERIFY(saveButton);
    QCOMPARE(
        model->data(
            model->index(0, SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)),
            Qt::DisplayRole
            ).toString(),
        QStringLiteral("Before")
        );

    QVERIFY(model->setData(
        model->index(0, SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)),
        QStringLiteral("After"),
        Qt::EditRole
        ));
    QVERIFY(page.hasUnsavedChanges());
    saveButton->click();

    QCOMPARE(
        model->data(
            model->index(0, SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)),
            Qt::DisplayRole
            ).toString(),
        QStringLiteral("After")
        );
    QCOMPARE(
        model->data(
            model->index(0, SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)),
            Qt::DisplayRole
            ).toString(),
        koreanName
        );
    QCOMPARE(
        model->data(
            model->index(0, SpeakingEval::toInt(SpeakingEvalColumn::Notes)),
            Qt::DisplayRole
            ).toString(),
        sentinel
        );
    QVERIFY(!page.hasUnsavedChanges());

    const auto persisted = fixture.services.speakingEvaluationService()->evaluation(
        fixture.classId,
        evaluationName
        );
    QVERIFY(persisted);
    QCOMPARE(persisted->size(), SpeakingEval::RowCount);
    QCOMPARE(cell(*persisted, 0, SpeakingEvalColumn::EnglishName), QStringLiteral("After"));
    QCOMPARE(cell(*persisted, 0, SpeakingEvalColumn::KoreanName), koreanName);
    QCOMPARE(cell(*persisted, 0, SpeakingEvalColumn::Notes), sentinel);

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Information);
    QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Saved"));
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral("Speaking evaluation saved.")
        );
    QVERIFY(prompts.confirmations.isEmpty());

    const QJsonObject snapshot{
        {QStringLiteral("visible"), QJsonObject{
             {QStringLiteral("english"), QStringLiteral("After")},
             {QStringLiteral("koreanUtf8Hex"), koreanUtf8Hex},
             {QStringLiteral("sentinelNote"), sentinel}
         }},
        {QStringLiteral("persisted"), QJsonObject{
             {QStringLiteral("english"), cell(*persisted, 0, SpeakingEvalColumn::EnglishName)},
             {QStringLiteral("koreanUtf8Hex"), QString::fromLatin1(
                  cell(*persisted, 0, SpeakingEvalColumn::KoreanName).toUtf8().toHex()
                  )},
             {QStringLiteral("sentinelNote"), cell(*persisted, 0, SpeakingEvalColumn::Notes)}
         }},
        {QStringLiteral("dirty"), page.hasUnsavedChanges()},
        {QStringLiteral("notice"), QJsonObject{
             {QStringLiteral("count"), prompts.messages.size()},
             {QStringLiteral("severity"), QStringLiteral("information")},
             {QStringLiteral("title"), prompts.messages.constFirst().title}
         }}
    };
    qInfo().noquote() << QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
}

QTEST_MAIN(SpeakingEvalPageSaveParityTests)

#include "speaking_eval_page_save_parity_tests.moc"
