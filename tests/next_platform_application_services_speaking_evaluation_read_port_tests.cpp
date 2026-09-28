#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "next/application/speaking_evaluation_query.h"
#include "next/platform/application_services_speaking_evaluation_read_port.h"

#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("speaking-evaluation-read-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Application::SpeakingEvaluationReadQuery query(
    const int classId,
    std::u16string evaluationName
    )
{
    const auto typedId = Domain::ClassId::fromString(std::to_string(classId));
    if (!typedId)
    {
        qFatal("Test class ID must have a typed representation.");
    }

    return {
        .classId = *typedId,
        .evaluationName = std::move(evaluationName)
    };
}

SpeakingEvalRows makeRows()
{
    SpeakingEvalRows rows = SpeakingEval::emptyRows();
    for (int row = 0; row < rows.size(); ++row)
    {
        rows[row][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
            QStringLiteral("Student %1").arg(QChar(u'A' + row));
        rows[row][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
            QStringLiteral("\uAE40\uBBFC\uC9C0");
    }
    rows[0][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QStringLiteral("\uAE40\uBBFC\uC9C0");
    rows[7][SpeakingEval::toInt(SpeakingEvalColumn::Comments)] =
        QStringLiteral("Review \U0001F4DA");
    rows[24][SpeakingEval::toInt(SpeakingEvalColumn::Notes)] =
        QStringLiteral("\uC218\uC5C5 \U0001F4DA");
    return rows;
}

}

class NextPlatformApplicationServicesSpeakingEvaluationReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsExactNameAndPreservesOrderedUnicodeMatrix();
    void closedSessionFailsWithoutDataServiceFallback();
};

void NextPlatformApplicationServicesSpeakingEvaluationReadPortTests::
readsExactNameAndPreservesOrderedUnicodeMatrix()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Read Test")
        );
    QVERIFY(createdClass);

    const SpeakingEvalRows expected = makeRows();
    QVERIFY(services.speakingEvaluationService()->saveEvaluation(
        *createdClass,
        QStringLiteral("Winter"),
        expected
        ));

    Platform::ApplicationServicesSpeakingEvaluationReadPort port(services);
    auto result = Application::SpeakingEvaluationQuery::execute(
        query(*createdClass, u"Winter"),
        port
        );

    QVERIFY2(
        result,
        qPrintable(result ? QString() : QString::fromStdString(result.error().message))
        );
    QCOMPARE(result.value().rows.size(), std::size_t(25));
    QCOMPARE(result.value().rows[0].size(), std::size_t(11));
    QCOMPARE(result.value().rows[0][1], std::u16string(u"Student A"));
    QCOMPARE(result.value().rows[0][2], std::u16string(u"\uAE40\uBBFC\uC9C0"));
    QCOMPARE(result.value().rows[7][1], std::u16string(u"Student H"));
    QCOMPARE(result.value().rows[7][9], std::u16string(u"Review \U0001F4DA"));
    QCOMPARE(result.value().rows[24][1], std::u16string(u"Student Y"));
    QCOMPARE(result.value().rows[24][10], std::u16string(u"\uC218\uC5C5 \U0001F4DA"));

    const auto exactNameMiss = Application::SpeakingEvaluationQuery::execute(
        query(*createdClass, u" Winter "),
        port
        );
    QVERIFY(exactNameMiss);
    QVERIFY(exactNameMiss.value().rows.empty());
    QCOMPARE(exactNameMiss.value().evaluationName, std::u16string(u" Winter "));
}

void NextPlatformApplicationServicesSpeakingEvaluationReadPortTests::
closedSessionFailsWithoutDataServiceFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Closed Speaking Evaluation Read Test")
        );
    QVERIFY(createdClass);

    services.closeDatabase();
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(services.dataService());
    QVERIFY(!services.dataService()->isOpen());

    Platform::ApplicationServicesSpeakingEvaluationReadPort port(services);
    const auto result = Application::SpeakingEvaluationQuery::execute(
        query(*createdClass, u"Winter"),
        port
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(QString::fromStdString(result.error().message).contains(
        QStringLiteral("unavailable")
        ));
}

QTEST_MAIN(NextPlatformApplicationServicesSpeakingEvaluationReadPortTests)

#include "next_platform_application_services_speaking_evaluation_read_port_tests.moc"
