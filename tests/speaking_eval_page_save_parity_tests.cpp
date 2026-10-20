#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "domain/models/classroom.h"
#include "domain/models/speaking_evaluation.h"
#include "features/speaking_eval/ui/speaking_eval_model.h"
#include "features/speaking_eval/ui/speaking_eval_page.h"
#include "features/speaking_eval/ui/speaking_eval_page_validation_adapter.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <QUuid>

#include <cstdint>
#include <string>
#include <vector>

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
    void toFormValidationPreservesValidationCodeKeys();
    void toFormValidationPreservesIssueCoordinates();
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

void SpeakingEvalPageSaveParityTests::
toFormValidationPreservesValidationCodeKeys()
{
    using Code =
        ClassMngr::Next::Application::SpeakingEvaluationValidationCode;
    using ApplicationValidationResult =
        ClassMngr::Next::Application::SpeakingEvaluationValidationResult;
    using ApplicationSaveRequest =
        ClassMngr::Next::Application::SpeakingEvaluationSaveRequest;
    using Severity =
        ClassMngr::Next::Application::SpeakingEvaluationValidationSeverity;
    using DuplicateRowIndex =
        ClassMngr::Next::Domain::DuplicateStudentNamePairRowIndex;

    const std::vector<Code> codes{
        Code::StudentNameEnglishTooLong,
        Code::StudentNameEnglishNonAscii,
        Code::StudentNameEnglishInvalidCharacters,
        Code::StudentNameKoreanInvalidCharacters,
        Code::StudentNameKoreanTooShort,
        Code::StudentNameKoreanTooLong,
        Code::StudentNameKoreanUnusualLength,
        Code::StudentNameDuplicatePair,
        Code::SpeakingEvaluationClassIdInvalid,
        Code::ValidationLengthOutOfBounds,
        Code::SpeakingEvaluationRowsTooMany,
        Code::SpeakingEvaluationRowTooManyCells,
        Code::SpeakingEvaluationStudentNameRequired,
        Code::ValidationEnumInvalidValue
    };
    const std::vector<QString> expectedKeys{
        QStringLiteral("student_name.english.too_long"),
        QStringLiteral("student_name.english.non_ascii"),
        QStringLiteral("student_name.english.invalid_characters"),
        QStringLiteral("student_name.korean.invalid_characters"),
        QStringLiteral("student_name.korean.too_short"),
        QStringLiteral("student_name.korean.too_long"),
        QStringLiteral("student_name.korean.unusual_length"),
        QStringLiteral("student_name.duplicate_pair"),
        QStringLiteral("speaking_evaluation.class_id.invalid"),
        QStringLiteral("validation.length.out_of_bounds"),
        QStringLiteral("speaking_evaluation.rows.too_many"),
        QStringLiteral("speaking_evaluation.row.too_many_cells"),
        QStringLiteral("speaking_evaluation.student_name.required"),
        QStringLiteral("validation.enum.invalid_value")
    };

    const auto classId = ClassMngr::Next::Domain::ClassId::fromString("42");
    QVERIFY(classId.has_value());
    ApplicationValidationResult input{
        .normalizedRequest = ApplicationSaveRequest{
            .classId = *classId,
            .evaluationName = u"Evaluation",
            .evaluation = {},
            .allowQuestionableKoreanNameLengths = false
        },
        .issues = {}
    };

    for (std::size_t index = 0; index < codes.size(); ++index)
    {
        const std::string token = "issue-" + std::to_string(index);
        const std::u16string token16(token.begin(), token.end());
        input.issues.push_back({
            .code = codes[index],
            .field = "field-" + std::to_string(index),
            .row = ClassMngr::Next::Application::SpeakingEvaluationValidationIssueRowIndex{
                static_cast<int>(100 + index)
            },
            .column = ClassMngr::Next::Application::SpeakingEvaluationValidationIssueColumnIndex{
                static_cast<int>(200 + index)
            },
            .severity = index % 2 == 0 ? Severity::Error : Severity::Warning,
            .arguments = {
                {"count", static_cast<std::int64_t>(1000 + index)},
                {"text", token16},
                {"rows", std::vector<DuplicateRowIndex>{
                    DuplicateRowIndex{index + 1},
                    DuplicateRowIndex{index + 11}
                }},
                {"labels", std::vector<std::u16string>{
                    token16,
                    token16 + u"-second"
                }}
            }
        });
    }

    const ValidationResult converted =
        SpeakingEvalPageValidationAdapter::toFormValidation(input);
    QCOMPARE(
        converted.issues().size(),
        static_cast<qsizetype>(expectedKeys.size())
        );

    for (std::size_t index = 0; index < expectedKeys.size(); ++index)
    {
        const ValidationIssue& issue =
            converted.issues().at(static_cast<qsizetype>(index));
        QCOMPARE(issue.code, expectedKeys[index]);
        QCOMPARE(
            issue.field,
            QString::fromStdString("field-" + std::to_string(index))
            );
        QCOMPARE(issue.row, static_cast<int>(100 + index));
        QCOMPARE(issue.column, static_cast<int>(200 + index));
        QCOMPARE(
            issue.severity,
            index % 2 == 0 ? ValidationSeverity::Error : ValidationSeverity::Warning
            );
        QCOMPARE(issue.arguments.size(), 4);

        const QString token =
            QString::fromStdString("issue-" + std::to_string(index));
        QCOMPARE(
            issue.arguments.value(QStringLiteral("count")).toLongLong(),
            static_cast<qlonglong>(1000 + index)
            );
        QCOMPARE(
            issue.arguments.value(QStringLiteral("text")).toString(),
            token
            );

        const QVariantList rows =
            issue.arguments.value(QStringLiteral("rows")).toList();
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows[0].toInt(), static_cast<int>(index + 1));
        QCOMPARE(rows[1].toInt(), static_cast<int>(index + 11));

        const QVariantList labels =
            issue.arguments.value(QStringLiteral("labels")).toList();
        QCOMPARE(labels.size(), 2);
        QCOMPARE(labels[0].toString(), token);
        QCOMPARE(
            labels[1].toString(),
            token + QStringLiteral("-second")
            );
    }
}

void SpeakingEvalPageSaveParityTests::
toFormValidationPreservesIssueCoordinates()
{
    using namespace ClassMngr::Next::Application;

    const auto classId = ClassMngr::Next::Domain::ClassId::fromString("42");
    QVERIFY(classId.has_value());

    SpeakingEvaluationValidationResult input{
        .normalizedRequest = SpeakingEvaluationSaveRequest{
            .classId = *classId,
            .evaluationName = u"Evaluation",
            .evaluation = {},
            .allowQuestionableKoreanNameLengths = false
        },
        .issues = {
            {
                .code = SpeakingEvaluationValidationCode::SpeakingEvaluationRowsTooMany,
                .field = "rows",
                .severity = SpeakingEvaluationValidationSeverity::Error
            },
            {
                .code = SpeakingEvaluationValidationCode::SpeakingEvaluationRowTooManyCells,
                .field = "rows[31]",
                .row = SpeakingEvaluationValidationIssueRowIndex{31},
                .severity = SpeakingEvaluationValidationSeverity::Error
            },
            {
                .code = SpeakingEvaluationValidationCode::ValidationEnumInvalidValue,
                .field = "rows[0].Grammar",
                .column = SpeakingEvaluationValidationIssueColumnIndex{12},
                .severity = SpeakingEvaluationValidationSeverity::Error
            },
            {
                .code = SpeakingEvaluationValidationCode::ValidationEnumInvalidValue,
                .field = "rows[100].Grammar",
                .row = SpeakingEvaluationValidationIssueRowIndex{100},
                .column = SpeakingEvaluationValidationIssueColumnIndex{200},
                .severity = SpeakingEvaluationValidationSeverity::Error
            }
        }
    };

    const ValidationResult converted =
        SpeakingEvalPageValidationAdapter::toFormValidation(input);
    QCOMPARE(converted.issues().size(), qsizetype(4));
    QCOMPARE(converted.issues().at(0).row, -1);
    QCOMPARE(converted.issues().at(0).column, -1);
    QCOMPARE(converted.issues().at(1).row, 31);
    QCOMPARE(converted.issues().at(1).column, -1);
    QCOMPARE(converted.issues().at(2).row, -1);
    QCOMPARE(converted.issues().at(2).column, 12);
    QCOMPARE(converted.issues().at(3).row, 100);
    QCOMPARE(converted.issues().at(3).column, 200);
}

QTEST_MAIN(SpeakingEvalPageSaveParityTests)

#include "speaking_eval_page_save_parity_tests.moc"
