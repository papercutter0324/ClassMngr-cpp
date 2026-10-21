#include "features/classes/ui/class_analytics_ranking_model.h"

#include "features/classes/ui/class_analytics_ranking_header.h"

#include <QtTest>

class ClassAnalyticsRankingModelTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesRankingColumnsAndDelegateRoles();
    void typedAttentionPolicyOverridesGradeLabelAndOnlyAffectsAverage();
    void missingTypedAttentionKeepsGradeLabelFallback();
    void updatesHeadersAndRowsWithModelReset();
};

void ClassAnalyticsRankingModelTests::exposesRankingColumnsAndDelegateRoles()
{
    ClassAnalyticsRankingModel model;
    SpeakingAnalytics::StudentRank student;
    student.englishName = QStringLiteral("Alex Kim");
    student.koreanName = QStringLiteral("김알렉스");
    student.overall3 = 2.667;
    student.overallLetter = QStringLiteral("B");
    student.criterionLetters = {
        QStringLiteral("B+"),
        QStringLiteral("A"),
        QStringLiteral("B"),
        QStringLiteral("A+"),
        QStringLiteral("B+"),
        QStringLiteral("A")
    };
    model.setRankings({student});

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.columnCount(), 10);
    QCOMPARE(
        model.data(model.index(0, ClassAnalyticsRankingModel::RankColumn)).toString(),
        QStringLiteral("1")
        );
    QCOMPARE(
        model.data(
            model.index(0, ClassAnalyticsRankingModel::EnglishNameColumn)
            ).toString(),
        QStringLiteral("Alex Kim")
        );
    QCOMPARE(
        model.data(
            model.index(0, ClassAnalyticsRankingModel::AverageColumn)
            ).toString(),
        QStringLiteral("2.7")
        );
    QCOMPARE(
        model.data(
            model.index(0, ClassAnalyticsRankingModel::AverageColumn),
            AnalyticsRankingRoles::Grade
            ).toString(),
        QStringLiteral("B")
        );
    QVERIFY(
        model.data(
            model.index(0, ClassAnalyticsRankingModel::AverageColumn),
            AnalyticsRankingRoles::NeedsAttention
            ).toBool()
        );
    for (int column = 0; column < model.columnCount(); ++column)
    {
        QCOMPARE(
            model.data(
                model.index(0, column),
                AnalyticsRankingRoles::NeedsAttention
                ).toBool(),
            column == ClassAnalyticsRankingModel::AverageColumn
            );
    }
    QCOMPARE(
        model.data(
            model.index(0, ClassAnalyticsRankingModel::GrammarColumn),
            AnalyticsRankingRoles::Grade
            ).toString(),
        QStringLiteral("B+")
        );
    QCOMPARE(
        model.data(
            model.index(0, ClassAnalyticsRankingModel::ContentColumn),
            Qt::DisplayRole
            ).toString(),
        QString()
        );
    QCOMPARE(
        model.data(
            model.index(0, ClassAnalyticsRankingModel::KoreanNameColumn),
            Qt::TextAlignmentRole
            ).toInt(),
        static_cast<int>(Qt::AlignCenter)
        );
}

void ClassAnalyticsRankingModelTests::
typedAttentionPolicyOverridesGradeLabelAndOnlyAffectsAverage()
{
    ClassAnalyticsRankingModel model;
    SpeakingAnalytics::StudentRank typedAttention;
    typedAttention.overallLetter = QStringLiteral("A+");
    typedAttention.gradeNeedsAttention = true;
    SpeakingAnalytics::StudentRank typedNoAttention;
    typedNoAttention.overallLetter = QStringLiteral("C");
    typedNoAttention.gradeNeedsAttention = false;
    model.setRankings({typedAttention, typedNoAttention});

    QVERIFY(
        model.data(
            model.index(0, ClassAnalyticsRankingModel::AverageColumn),
            AnalyticsRankingRoles::NeedsAttention
            ).toBool()
        );
    QVERIFY(
        !model.data(
            model.index(1, ClassAnalyticsRankingModel::AverageColumn),
            AnalyticsRankingRoles::NeedsAttention
            ).toBool()
        );
    for (int row = 0; row < model.rowCount(); ++row)
    {
        for (int column = 0; column < model.columnCount(); ++column)
        {
            if (column == ClassAnalyticsRankingModel::AverageColumn)
            {
                continue;
            }

            QVERIFY(
                !model.data(
                    model.index(row, column),
                    AnalyticsRankingRoles::NeedsAttention
                    ).toBool()
                );
        }
    }
}

void ClassAnalyticsRankingModelTests::
missingTypedAttentionKeepsGradeLabelFallback()
{
    ClassAnalyticsRankingModel model;
    SpeakingAnalytics::StudentRank emptyGrade;
    emptyGrade.overallLetter = QString();
    SpeakingAnalytics::StudentRank unrecognizedGrade;
    unrecognizedGrade.overallLetter = QStringLiteral("not a grade");
    model.setRankings({emptyGrade, unrecognizedGrade});

    for (int row = 0; row < model.rowCount(); ++row)
    {
        QVERIFY(
            model.data(
                model.index(row, ClassAnalyticsRankingModel::AverageColumn),
                AnalyticsRankingRoles::NeedsAttention
                ).toBool()
            );
        QVERIFY(
            !model.data(
                model.index(row, ClassAnalyticsRankingModel::GrammarColumn),
                AnalyticsRankingRoles::NeedsAttention
                ).toBool()
            );
    }
}

void ClassAnalyticsRankingModelTests::updatesHeadersAndRowsWithModelReset()
{
    ClassAnalyticsRankingModel model;
    model.setHeaderLabels({
        QString(),
        QStringLiteral("English"),
        QStringLiteral("Korean"),
        QStringLiteral("Average")
    });
    QCOMPARE(
        model.headerData(3, Qt::Horizontal).toString(),
        QStringLiteral("Average")
        );
    QVERIFY(
        !model.headerData(10, Qt::Horizontal).isValid()
        );

    SpeakingAnalytics::StudentRank first;
    first.englishName = QStringLiteral("First");
    first.overallLetter = QStringLiteral("A");
    SpeakingAnalytics::StudentRank second;
    second.englishName = QStringLiteral("Second");
    second.overallLetter = QStringLiteral("A+");
    model.setRankings({first, second});
    QCOMPARE(model.rowCount(), 2);
    QVERIFY(
        !model.data(
            model.index(0, ClassAnalyticsRankingModel::AverageColumn),
            AnalyticsRankingRoles::NeedsAttention
            ).toBool()
        );
    QVERIFY(
        !model.data(
            model.index(1, ClassAnalyticsRankingModel::AverageColumn),
            AnalyticsRankingRoles::NeedsAttention
            ).toBool()
        );

    model.setRankings({});
    QCOMPARE(model.rowCount(), 0);
}

QTEST_GUILESS_MAIN(ClassAnalyticsRankingModelTests)

#include "class_analytics_ranking_model_tests.moc"
