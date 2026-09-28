#include "next/application/speaking_evaluation_query.h"

#include <QtTest/QtTest>

#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

namespace
{

Domain::ClassId classId(std::string value)
{
    return *Domain::ClassId::fromString(std::move(value));
}

SpeakingEvaluationReadQuery query(
    std::string classIdentifier,
    std::u16string name
    )
{
    return {
        .classId = classId(std::move(classIdentifier)),
        .evaluationName = std::move(name)
    };
}

SpeakingEvaluationReadSnapshot snapshot(
    const SpeakingEvaluationReadQuery& request
    )
{
    SpeakingEvaluationReadSnapshot result{
        .classId = request.classId,
        .evaluationName = request.evaluationName
    };
    result.rows.resize(25);
    for (int row = 0; row < 25; ++row)
    {
        std::vector<std::u16string>& cells =
            result.rows[static_cast<std::size_t>(row)];
        cells.resize(11);
        cells[1] = row == 0 ? u"First" : row == 24 ? u"Last" : u"Middle";
    }
    result.rows[0][2] = u"\uAE40\uBBFC\uC9C0";
    result.rows[7][9] = u"Review \U0001F4DA";
    result.rows[24][10] = u"\uC218\uC5C5 \U0001F4DA";
    return result;
}

class RecordingSpeakingEvaluationReadPort final
    : public SpeakingEvaluationReadPort
{
public:
    [[nodiscard]] SpeakingEvaluationReadResult readEvaluation(
        const SpeakingEvaluationReadQuery& value
        ) const override
    {
        requests.push_back(value);
        return result;
    }

    mutable std::vector<SpeakingEvaluationReadQuery> requests;
    SpeakingEvaluationReadResult result =
        SpeakingEvaluationReadResult::failure({
            .code = Domain::ErrorCode::Technical,
            .message = "Unconfigured fake speaking evaluation read port",
            .recoverable = false
        });
};

}

class NextApplicationSpeakingEvaluationQueryTests final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsInvalidClassIdsWithoutCallingPort();
    void rejectsEmptyNameWithoutCallingPort();
    void preservesExactQueryIdentityAndOrderedUnicodeMatrix();
    void preservesSuccessfulEmptyResult();
    void propagatesPortErrorAndRejectsMismatchedIdentity();
};

void NextApplicationSpeakingEvaluationQueryTests::
rejectsInvalidClassIdsWithoutCallingPort()
{
    RecordingSpeakingEvaluationReadPort port;
    const std::vector<std::string> invalidIds{
        "0",
        "-3",
        "+42",
        "class-42",
        " 42",
        "42 ",
        "01",
        "999999999999999999999"
    };

    for (const std::string& id : invalidIds)
    {
        const auto result = SpeakingEvaluationQuery::execute(
            query(id, u"Winter"),
            port
            );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }
    QVERIFY(port.requests.empty());
}

void NextApplicationSpeakingEvaluationQueryTests::
rejectsEmptyNameWithoutCallingPort()
{
    RecordingSpeakingEvaluationReadPort port;
    const auto result = SpeakingEvaluationQuery::execute(
        query("42", {}),
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(port.requests.empty());
}

void NextApplicationSpeakingEvaluationQueryTests::
preservesExactQueryIdentityAndOrderedUnicodeMatrix()
{
    RecordingSpeakingEvaluationReadPort port;
    const SpeakingEvaluationReadQuery requested = query(
        "42",
        u"  Winter \U0001F4DA  "
        );
    port.result = SpeakingEvaluationReadResult::success(snapshot(requested));

    const auto result = SpeakingEvaluationQuery::execute(requested, port);

    QVERIFY(result);
    QCOMPARE(port.requests.size(), std::size_t(1));
    QVERIFY(port.requests.front() == requested);
    QVERIFY(result.value().classId == requested.classId);
    QCOMPARE(result.value().evaluationName, std::u16string(u"  Winter \U0001F4DA  "));
    QCOMPARE(result.value().rows.size(), std::size_t(25));
    QCOMPARE(result.value().rows[0].size(), std::size_t(11));
    QCOMPARE(result.value().rows[0][1], std::u16string(u"First"));
    QCOMPARE(result.value().rows[0][2], std::u16string(u"\uAE40\uBBFC\uC9C0"));
    QCOMPARE(result.value().rows[7][1], std::u16string(u"Middle"));
    QCOMPARE(result.value().rows[7][9], std::u16string(u"Review \U0001F4DA"));
    QCOMPARE(result.value().rows[24][1], std::u16string(u"Last"));
    QCOMPARE(result.value().rows[24][10], std::u16string(u"\uC218\uC5C5 \U0001F4DA"));
}

void NextApplicationSpeakingEvaluationQueryTests::
preservesSuccessfulEmptyResult()
{
    RecordingSpeakingEvaluationReadPort port;
    const SpeakingEvaluationReadQuery requested = query("42", u"Winter");
    port.result = SpeakingEvaluationReadResult::success({
        .classId = requested.classId,
        .evaluationName = requested.evaluationName,
        .rows = {}
    });

    const auto result = SpeakingEvaluationQuery::execute(requested, port);

    QVERIFY(result);
    QVERIFY(result.value().rows.empty());
}

void NextApplicationSpeakingEvaluationQueryTests::
propagatesPortErrorAndRejectsMismatchedIdentity()
{
    RecordingSpeakingEvaluationReadPort port;
    const Domain::OperationError portError{
        .code = Domain::ErrorCode::Technical,
        .message = "query failed",
        .recoverable = true
    };
    port.result = SpeakingEvaluationReadResult::failure(portError);

    const SpeakingEvaluationReadQuery requested = query("42", u"Winter");
    auto result = SpeakingEvaluationQuery::execute(requested, port);
    QVERIFY(!result);
    QVERIFY(result.error() == portError);

    port.result = SpeakingEvaluationReadResult::success({
        .classId = classId("43"),
        .evaluationName = requested.evaluationName,
        .rows = {}
    });
    result = SpeakingEvaluationQuery::execute(requested, port);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
}

QTEST_APPLESS_MAIN(NextApplicationSpeakingEvaluationQueryTests)

#include "next_application_speaking_evaluation_query_tests.moc"
