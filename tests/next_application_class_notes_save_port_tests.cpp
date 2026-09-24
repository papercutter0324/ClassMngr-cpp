#include "next/application/class_notes_save_port.h"

#include <QtTest/QtTest>

#include <utility>

using namespace ClassMngr::Next;

class NextApplicationClassNotesSavePortTests final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsExactlyTenThousandUtf16CodeUnits();
    void rejectsTextBeyondTenThousandUtf16CodeUnits();
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

    const Domain::Result<void> result = request.validate();
    QVERIFY(result);
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

    const Domain::Result<void> result = request.validate();
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Validation);
}

QTEST_APPLESS_MAIN(NextApplicationClassNotesSavePortTests)

#include "next_application_class_notes_save_port_tests.moc"
