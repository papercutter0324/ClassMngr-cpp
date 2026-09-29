#include "next/application/class_details_validation_policy.h"

#include <QtTest/QtTest>

using namespace ClassMngr::Next::Application;

namespace
{
ClassDetailsValidationCatalog catalog()
{
    return {
        .grades = {
            {
                .name = u"E4",
                .levels = {
                    {
                        .name = u"Theseus",
                        .readingBooks = {u"", u"Reading Explorer 1"},
                        .essayBooks = {u"", u"4A"}
                    }
                }
            },
            {
                .name = u"M1",
                .levels = {
                    {
                        .name = u"Elephantus",
                        .readingBooks = {u"", u"GraVoca 1A"},
                        .essayBooks = {u"N/A"}
                    }
                }
            }
        }
    };
}

ClassDetailsValidationInput validInput()
{
    ClassDetailsValidationInput input;
    input.classId = 21;
    input.teacherId = -1;
    input.classGrade = u" e4 ";
    input.classLevel = u"theseus";
    input.readingBook = u"reading explorer 1";
    input.essayBook = u"4a";
    input.classColor = u" #aabbcc ";
    input.fontColor = u" #112233 ";
    input.notes = u"  Keep notes  ";
    input.timeFillerActivities = u"  Quiet reading  ";
    input.regularTimes = {{u" monday ", u"16:00", u"16:55"}};
    return input;
}
}

class NextApplicationClassDetailsValidationPolicyTests final : public QObject
{
    Q_OBJECT

private slots:
    void normalizesAgainstTheSuppliedCatalogAndRawScheduleRows();
    void reportsDisallowedGradeAndLevelAgainstTheCatalog();
    void reportsDisallowedCatalogValuesInLegacyFieldOrder();
    void preservesLegacyValidationIssueOrderAndMalformedRowDetails();
    void reportsDuplicateGroupsInDeterministicFirstSeenOrder();
    void reportsBooksWithoutGradeAndLevelInBookOrder();
};

void NextApplicationClassDetailsValidationPolicyTests::
normalizesAgainstTheSuppliedCatalogAndRawScheduleRows()
{
    const ClassDetailsValidationOutput output =
        ClassDetailsValidationPolicy::normalizeAndValidate(
            validInput(),
            catalog()
            );

    QVERIFY(!output.hasErrors());
    QCOMPARE(output.normalized.classGrade, std::u16string(u"E4"));
    QCOMPARE(output.normalized.classLevel, std::u16string(u"Theseus"));
    QCOMPARE(
        output.normalized.readingBook,
        std::u16string(u"Reading Explorer 1")
        );
    QCOMPARE(output.normalized.essayBook, std::u16string(u"4A"));
    QCOMPARE(output.normalized.classColor, std::u16string(u"#AABBCC"));
    QCOMPARE(output.normalized.fontColor, std::u16string(u"#112233"));
    QCOMPARE(output.normalized.notes, std::u16string(u"Keep notes"));
    QCOMPARE(
        output.normalized.timeFillerActivities,
        std::u16string(u"Quiet reading")
        );
    QCOMPARE(output.normalized.regularTimes.size(), std::size_t(1));
    QCOMPARE(output.normalized.regularTimes[0].day, std::u16string(u"Monday"));
    QCOMPARE(
        output.normalized.regularTimes[0].startTime,
        std::u16string(u"4:00 PM")
        );
    QCOMPARE(
        output.normalized.regularTimes[0].endTime,
        std::u16string(u"4:55 PM")
        );
}

void NextApplicationClassDetailsValidationPolicyTests::
preservesLegacyValidationIssueOrderAndMalformedRowDetails()
{
    ClassDetailsValidationInput input;
    input.classId = 0;
    input.teacherId = -2;
    input.classGrade = u"E4";
    input.classColor = u"blue";
    input.fontColor = u"not-a-color";
    input.notes = std::u16string(10001, u'n');
    input.timeFillerActivities = std::u16string(10002, u'a');
    input.regularTimes = {
        {u"Funday", u"bad-start", u"25:00"},
        {u"Monday", u"9:00 PM", u"8:00 PM"}
    };
    input.intensiveTimes = {{u"Blursday", u"broken", u"also-bad"}};

    const ClassDetailsValidationOutput output =
        ClassDetailsValidationPolicy::normalizeAndValidate(input, catalog());

    QCOMPARE(output.issues.size(), std::size_t(15));
    const ClassDetailsValidationCode expectedCodes[] = {
        ClassDetailsValidationCode::ClassIdInvalid,
        ClassDetailsValidationCode::TeacherIdInvalid,
        ClassDetailsValidationCode::LevelRequired,
        ClassDetailsValidationCode::InvalidHexColor,
        ClassDetailsValidationCode::InvalidHexColor,
        ClassDetailsValidationCode::ClassIdInvalid,
        ClassDetailsValidationCode::TextLengthOutOfBounds,
        ClassDetailsValidationCode::TextLengthOutOfBounds,
        ClassDetailsValidationCode::InvalidWeekday,
        ClassDetailsValidationCode::InvalidTimeFormat,
        ClassDetailsValidationCode::InvalidTimeFormat,
        ClassDetailsValidationCode::EndNotAfterStart,
        ClassDetailsValidationCode::InvalidWeekday,
        ClassDetailsValidationCode::InvalidTimeFormat,
        ClassDetailsValidationCode::InvalidTimeFormat
    };
    for (std::size_t index = 0; index < output.issues.size(); ++index)
    {
        QCOMPARE(output.issues[index].code, expectedCodes[index]);
    }

    QCOMPARE(
        output.issues[0].field,
        ClassDetailsValidationField::ClassId
        );
    QCOMPARE(output.issues[0].integerValue.value(), 0);
    QCOMPARE(
        output.issues[1].field,
        ClassDetailsValidationField::TeacherId
        );
    QCOMPARE(output.issues[1].integerValue.value(), -2);
    QCOMPARE(
        output.issues[2].field,
        ClassDetailsValidationField::ClassLevel
        );
    QCOMPARE(
        output.issues[5].integerValue.value(),
        0
        );
    QCOMPARE(output.issues[6].field, ClassDetailsValidationField::Notes);
    QCOMPARE(output.issues[6].length.value(), 10001);
    QCOMPARE(output.issues[6].minimum.value(), 0);
    QCOMPARE(output.issues[6].maximum.value(), 10000);
    QCOMPARE(
        output.issues[7].field,
        ClassDetailsValidationField::TimeFillerActivities
        );

    QCOMPARE(
        output.issues[8].field,
        ClassDetailsValidationField::ScheduleDay
        );
    QCOMPARE(output.issues[8].schedule.value(),
        ClassDetailsValidationSchedule::Regular);
    QCOMPARE(output.issues[8].row, std::size_t(0));
    QCOMPARE(output.issues[8].value.value(), std::u16string(u"Funday"));
    QCOMPARE(
        output.issues[9].field,
        ClassDetailsValidationField::ScheduleStartTime
        );
    QCOMPARE(output.issues[9].value.value(), std::u16string(u"bad-start"));
    QCOMPARE(
        output.issues[10].field,
        ClassDetailsValidationField::ScheduleEndTime
        );
    QCOMPARE(output.issues[10].value.value(), std::u16string(u"25:00"));
    QCOMPARE(
        output.issues[11].start.value(),
        std::u16string(u"9:00 PM")
        );
    QCOMPARE(output.issues[11].end.value(), std::u16string(u"8:00 PM"));
    QCOMPARE(output.issues[12].schedule.value(),
        ClassDetailsValidationSchedule::Intensive);
}

void NextApplicationClassDetailsValidationPolicyTests::
reportsDuplicateGroupsInDeterministicFirstSeenOrder()
{
    ClassDetailsValidationInput input = validInput();
    input.regularTimes = {
        {u"Monday", u"9:00 AM", u"9:55 AM"},
        {u"Tuesday", u"10:00 AM", u"10:55 AM"},
        {u"monday", u"09:00", u"09:55"},
        {u"Tuesday", u"10:00", u"10:55"},
        {u"TUESDAY", u"10:00 AM", u"10:55 AM"},
        {u"Monday", u"9:30 AM", u"10:25 AM"}
    };

    const ClassDetailsValidationOutput output =
        ClassDetailsValidationPolicy::normalizeAndValidate(input, catalog());

    QCOMPARE(output.issues.size(), std::size_t(5));
    const int expectedRows[] = {0, 2, 1, 3, 4};
    for (std::size_t index = 0; index < output.issues.size(); ++index)
    {
        const auto& issue = output.issues[index];
        QCOMPARE(issue.code, ClassDetailsValidationCode::DuplicateSlot);
        QCOMPARE(issue.field, ClassDetailsValidationField::ScheduleStartTime);
        QCOMPARE(issue.schedule.value(), ClassDetailsValidationSchedule::Regular);
        QCOMPARE(issue.row, static_cast<std::size_t>(expectedRows[index]));
    }
    const int expectedFirstGroup[] = {0, 2};
    const int expectedSecondGroup[] = {1, 3, 4};
    for (std::size_t index = 0; index < 2; ++index)
    {
        QCOMPARE(output.issues[index].duplicateRows.size(), std::size_t(2));
        QCOMPARE(output.issues[index].duplicateRows[0], expectedFirstGroup[0]);
        QCOMPARE(output.issues[index].duplicateRows[1], expectedFirstGroup[1]);
    }
    for (std::size_t index = 2; index < 5; ++index)
    {
        QCOMPARE(output.issues[index].duplicateRows.size(), std::size_t(3));
        QCOMPARE(output.issues[index].duplicateRows[0], expectedSecondGroup[0]);
        QCOMPARE(output.issues[index].duplicateRows[1], expectedSecondGroup[1]);
        QCOMPARE(output.issues[index].duplicateRows[2], expectedSecondGroup[2]);
    }
}

void NextApplicationClassDetailsValidationPolicyTests::
reportsDisallowedCatalogValuesInLegacyFieldOrder()
{
    ClassDetailsValidationInput input = validInput();
    input.readingBook = u"Unknown Reading Book";
    input.essayBook = u"Unknown Essay Book";

    const ClassDetailsValidationOutput output =
        ClassDetailsValidationPolicy::normalizeAndValidate(input, catalog());

    QCOMPARE(output.issues.size(), std::size_t(2));
    QCOMPARE(output.issues[0].code, ClassDetailsValidationCode::ValueNotAllowed);
    QCOMPARE(
        output.issues[0].field,
        ClassDetailsValidationField::ReadingBook
        );
    QCOMPARE(output.issues[0].value.value(),
        std::u16string(u"Unknown Reading Book"));
    QCOMPARE(output.issues[0].allowedValues.size(), std::size_t(2));
    QCOMPARE(output.issues[0].allowedValues[0], std::u16string(u""));
    QCOMPARE(
        output.issues[0].allowedValues[1],
        std::u16string(u"Reading Explorer 1")
        );
    QCOMPARE(output.issues[1].code, ClassDetailsValidationCode::ValueNotAllowed);
    QCOMPARE(
        output.issues[1].field,
        ClassDetailsValidationField::EssayBook
        );
    QCOMPARE(output.issues[1].value.value(),
        std::u16string(u"Unknown Essay Book"));
    QCOMPARE(output.issues[1].allowedValues.size(), std::size_t(2));
}

void NextApplicationClassDetailsValidationPolicyTests::
reportsDisallowedGradeAndLevelAgainstTheCatalog()
{
    ClassDetailsValidationInput input;
    input.classId = 3;
    input.classGrade = u"E9";
    input.classLevel = u"Unknown";

    const ClassDetailsValidationOutput output =
        ClassDetailsValidationPolicy::normalizeAndValidate(input, catalog());

    QCOMPARE(output.issues.size(), std::size_t(2));
    QCOMPARE(output.issues[0].code, ClassDetailsValidationCode::ValueNotAllowed);
    QCOMPARE(output.issues[0].field, ClassDetailsValidationField::ClassGrade);
    QCOMPARE(output.issues[0].value.value(), std::u16string(u"E9"));
    QCOMPARE(output.issues[0].allowedValues.size(), std::size_t(2));
    QCOMPARE(output.issues[1].code, ClassDetailsValidationCode::ValueNotAllowed);
    QCOMPARE(output.issues[1].field, ClassDetailsValidationField::ClassLevel);
    QCOMPARE(output.issues[1].value.value(), std::u16string(u"Unknown"));
    QVERIFY(output.issues[1].allowedValues.empty());
}

void NextApplicationClassDetailsValidationPolicyTests::
reportsBooksWithoutGradeAndLevelInBookOrder()
{
    ClassDetailsValidationInput input;
    input.classId = 2;
    input.readingBook = u" Reading Explorer 1 ";
    input.essayBook = u" 4A ";

    const ClassDetailsValidationOutput output =
        ClassDetailsValidationPolicy::normalizeAndValidate(input, catalog());

    QCOMPARE(output.issues.size(), std::size_t(2));
    QCOMPARE(
        output.issues[0].code,
        ClassDetailsValidationCode::BookRequiresGradeLevel
        );
    QCOMPARE(
        output.issues[0].field,
        ClassDetailsValidationField::ReadingBook
        );
    QCOMPARE(
        output.issues[1].code,
        ClassDetailsValidationCode::BookRequiresGradeLevel
        );
    QCOMPARE(
        output.issues[1].field,
        ClassDetailsValidationField::EssayBook
        );
}

QTEST_APPLESS_MAIN(NextApplicationClassDetailsValidationPolicyTests)

#include "next_application_class_details_validation_policy_tests.moc"
