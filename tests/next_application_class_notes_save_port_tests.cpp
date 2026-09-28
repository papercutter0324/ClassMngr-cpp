#include "next/application/class_notes_save_use_case.h"

#include <QtTest/QtTest>

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
        .classId = *Domain::ClassId::fromString("42"),
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
    QCOMPARE(port.callCount, 0);
    QVERIFY(!port.lastRequest.has_value());
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
