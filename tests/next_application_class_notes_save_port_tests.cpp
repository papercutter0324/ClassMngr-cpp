#include "next/application/class_notes_save_use_case.h"

#include <QtTest/QtTest>

#include <limits>
#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

class RecordingSavePort final
    : public Application::ClassNotesSavePort
{
public:
    [[nodiscard]] Application::ClassNotesSaveResult saveClassNotes(
        const Application::ClassNotesSaveRequest& request
        ) const override
    {
        ++callCount;
        lastRequest = request;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::ClassNotesSaveRequest> lastRequest;
    Application::ClassNotesSaveResult result =
        Application::ClassNotesSaveResult::success();
};

}

class NextApplicationClassNotesSavePortTests final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsExactlyTenThousandUtf16CodeUnits();
    void rejectsTextBeyondTenThousandUtf16CodeUnits();
    void rejectsInvalidClassIdentifiersBeforeCallingPort();
    void acceptsPositiveClassIdentifiersAndForwardsUnchanged();
    void preservesPortFailure();
};

void NextApplicationClassNotesSavePortTests::
acceptsExactlyTenThousandUtf16CodeUnits()
{
    std::u16string emojiText;
    emojiText.reserve(Application::kClassNotesSaveMaxTextCodeUnits);
    for (std::size_t index = 0;
         index < Application::kClassNotesSaveMaxTextCodeUnits / 2;
         ++index)
    {
        emojiText.push_back(u'\xD83E');
        emojiText.push_back(u'\xDDED');
    }

    const Application::ClassNotesSaveRequest request{
        .classId = *Domain::ClassId::fromString("42"),
        .notes = std::move(emojiText),
        .timeFillerActivities = std::u16string(
            Application::kClassNotesSaveMaxTextCodeUnits,
            u'a'
            )
    };

    RecordingSavePort port;
    const Domain::Result<void> result =
        Application::ClassNotesSaveUseCase::execute(request, port);
    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(
        request.notes.size(),
        Application::kClassNotesSaveMaxTextCodeUnits
        );
    QCOMPARE(
        request.timeFillerActivities.size(),
        Application::kClassNotesSaveMaxTextCodeUnits
        );
}

void NextApplicationClassNotesSavePortTests::
rejectsTextBeyondTenThousandUtf16CodeUnits()
{
    const Application::ClassNotesSaveRequest request{
        .classId = *Domain::ClassId::fromString("0"),
        .notes = {},
        .timeFillerActivities = std::u16string(
            Application::kClassNotesSaveMaxTextCodeUnits + 1,
            u'a'
            )
    };

    RecordingSavePort port;
    const Domain::Result<void> result =
        Application::ClassNotesSaveUseCase::execute(request, port);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(
        result.error().message,
        std::string("Class notes text exceeds the 10,000 UTF-16 code-unit limit.")
        );
    QVERIFY(!result.error().recoverable);
    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
}

void NextApplicationClassNotesSavePortTests::
rejectsInvalidClassIdentifiersBeforeCallingPort()
{
    const std::string overflowingId =
        std::to_string(std::numeric_limits<int>::max()) + "0";
    const std::string invalidIds[] = {
        "0",
        "-1",
        "not-an-integer",
        overflowingId
    };

    for (const std::string& invalidId : invalidIds)
    {
        const Application::ClassNotesSaveRequest request{
            .classId = *Domain::ClassId::fromString(invalidId),
            .notes = u"Notes",
            .timeFillerActivities = u"Activities"
        };
        RecordingSavePort port;

        const Domain::Result<void> result =
            Application::ClassNotesSaveUseCase::execute(request, port);
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QCOMPARE(
            result.error().message,
            std::string("Class identifier must be a positive integer.")
            );
        QVERIFY(!result.error().recoverable);
        QCOMPARE(port.callCount, 0);
        QVERIFY(!port.lastRequest.has_value());
    }
}

void NextApplicationClassNotesSavePortTests::
acceptsPositiveClassIdentifiersAndForwardsUnchanged()
{
    const Application::ClassNotesSaveRequest request{
        .classId = *Domain::ClassId::fromString("00042"),
        .notes = u"Notes",
        .timeFillerActivities = u"Activities"
    };
    RecordingSavePort port;

    const Domain::Result<void> result =
        Application::ClassNotesSaveUseCase::execute(request, port);
    QVERIFY(result);
    QCOMPARE(port.callCount, 1);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.lastRequest->classId.value(), std::string("00042"));

    const std::string maximumClassId =
        std::to_string(std::numeric_limits<int>::max());
    const Application::ClassNotesSaveRequest maximumRequest{
        .classId = *Domain::ClassId::fromString(maximumClassId),
        .notes = u"Notes",
        .timeFillerActivities = u"Activities"
    };
    const Domain::Result<void> maximumResult =
        Application::ClassNotesSaveUseCase::execute(maximumRequest, port);
    QVERIFY(maximumResult);
    QCOMPARE(port.callCount, 2);
    QVERIFY(port.lastRequest.has_value());
    QCOMPARE(port.lastRequest->classId.value(), maximumClassId);
}

void NextApplicationClassNotesSavePortTests::preservesPortFailure()
{
    const Application::ClassNotesSaveRequest request{
        .classId = *Domain::ClassId::fromString("42"),
        .notes = u"Notes",
        .timeFillerActivities = u"Activities"
    };
    RecordingSavePort port;
    port.result = Application::ClassNotesSaveResult::failure({
        .code = Domain::ErrorCode::Conflict,
        .message = "notes rejected",
        .recoverable = true
    });

    const Domain::Result<void> result =
        Application::ClassNotesSaveUseCase::execute(request, port);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Conflict);
    QCOMPARE(result.error().message, std::string("notes rejected"));
    QCOMPARE(port.callCount, 1);
}

QTEST_APPLESS_MAIN(NextApplicationClassNotesSavePortTests)

#include "next_application_class_notes_save_port_tests.moc"
