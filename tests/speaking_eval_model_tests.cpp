#include "features/speaking_eval/ui/speaking_eval_model.h"

#include "domain/validation/speaking_eval_validator.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QtTest>

class SpeakingEvalModelTests : public QObject
{
    Q_OBJECT

private slots:
    void interactiveNameValidationMatchesEnginePolicy();
};

namespace
{
bool hasIssue(
    const ValidationResult& result,
    const QString& code,
    int row,
    int column
    )
{
    for (const ValidationIssue& issue : result.issues())
    {
        if (
            issue.code == code
            && issue.row == row
            && issue.column == column
            )
        {
            return true;
        }
    }

    return false;
}
}

void SpeakingEvalModelTests::interactiveNameValidationMatchesEnginePolicy()
{
    SpeakingEvalRows rows = SpeakingEval::emptyRows();
    rows[0][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("Amy1");
    rows[0][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QStringLiteral("김!");
    rows[1][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("  aMY  ");
    rows[1][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QStringLiteral(" 김 민 수 (a) ");
    rows[2][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("Amy");
    rows[2][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QStringLiteral("김민수(A)");
    rows[3][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("Zoe");
    rows[3][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QStringLiteral("김");

    SpeakingEvalModel model;
    model.loadData(rows);

    const int englishColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::EnglishName);
    const int koreanColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::KoreanName);

    QVERIFY(
        model.errorsForCell(0, englishColumn)
            .contains(QStringLiteral("English name contains invalid characters."))
        );
    QVERIFY(
        model.errorsForCell(0, koreanColumn)
            .contains(QStringLiteral("Korean name contains invalid characters."))
        );
    QVERIFY(
        model.errorsForCell(3, koreanColumn)
            .contains(QStringLiteral("Korean name has 1 or 5+ syllables. Verify it is correct."))
        );
    QCOMPARE(
        model.rows()[1][englishColumn],
        QStringLiteral("Amy")
        );
    QCOMPARE(
        model.rows()[1][koreanColumn],
        QStringLiteral("김민수(A)")
        );
    QVERIFY(
        model.errorsForCell(1, englishColumn)
            .contains(QStringLiteral("Duplicate student name pair. Also used on row(s): 3."))
        );
    QVERIFY(
        model.errorsForCell(2, koreanColumn)
            .contains(QStringLiteral("Duplicate student name pair. Also used on row(s): 2."))
        );

    QVERIFY(
        model.setData(
            model.index(0, englishColumn),
            QString::fromUtf8(QByteArray("\xc3(", 2))
            )
        );
    QVERIFY(
        model.errorsForCell(0, englishColumn)
            .contains(QStringLiteral("Only standard English letters are allowed."))
        );

    const SpeakingEvalRows malformedNormalized =
        SpeakingEvalValidator::normalized(model.rows());
    const ValidationResult malformedSaved =
        SpeakingEvalValidator::validate(
            1,
            QStringLiteral("Winter"),
            malformedNormalized
            );
    QVERIFY(
        hasIssue(
            malformedSaved,
            QStringLiteral("student_name.english.non_ascii"),
            0,
            englishColumn
            )
        );

    QVERIFY(
        model.setData(
            model.index(0, englishColumn),
            QStringLiteral("Amy1")
            )
        );

    const SpeakingEvalRows normalized =
        SpeakingEvalValidator::normalized(model.rows());
    const ValidationResult saved =
        SpeakingEvalValidator::validate(1, QStringLiteral("Winter"), normalized);
    QVERIFY(
        hasIssue(
            saved,
            QStringLiteral("student_name.english.invalid_characters"),
            0,
            englishColumn
            )
        );
    QVERIFY(
        hasIssue(
            saved,
            QStringLiteral("student_name.korean.invalid_characters"),
            0,
            koreanColumn
            )
        );
    QVERIFY(
        hasIssue(
            saved,
            QStringLiteral("student_name.korean.too_short"),
            3,
            koreanColumn
            )
        );
    int duplicateIssueCount = 0;
    for (const ValidationIssue& issue : saved.issues())
    {
        if (issue.code == QStringLiteral("student_name.duplicate_pair"))
        {
            ++duplicateIssueCount;
        }
    }
    QCOMPARE(duplicateIssueCount, 4);
}

int main(
    int argc,
    char** argv
    )
{
    QCoreApplication app(argc, argv);
    SpeakingEvalModelTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "speaking_eval_model_tests.moc"
