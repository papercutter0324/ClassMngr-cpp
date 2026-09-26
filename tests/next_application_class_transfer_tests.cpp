#include "next/application/class_summary_projection.h"
#include "next/application/class_transfer_matching_policy.h"
#include "next/application/class_transfer_projection.h"
#include "next/application/schedule_view_projection.h"

#include <QtTest/QtTest>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

TransferClassTime transferTime(
    std::string day = "Monday",
    std::string start = "4:00 PM",
    std::string end = "4:50 PM"
    )
{
    return TransferClassTime{
        std::move(day),
        std::move(start),
        std::move(end)
    };
}

TransferTeacher transferTeacher(
    std::string sourceKey,
    std::string displayName = "Alex Kim",
    std::string summary = "Room 504",
    std::string notes = "Teacher notes"
    )
{
    return TransferTeacher{
        std::move(sourceKey),
        std::move(displayName),
        std::move(summary),
        std::move(notes)
    };
}

TransferClass transferClass(
    std::string sourceKey,
    std::optional<std::string> teacherSourceKey = std::string("teacher-1"),
    const std::int32_t order = 0
    )
{
    TransferClass result;
    result.sourceKey = std::move(sourceKey);
    result.name = "Stored Class";
    result.summary = "Class summary";
    result.teacherSourceKey = std::move(teacherSourceKey);
    result.grade = "E4";
    result.level = "Perseus";
    result.readingBook = "Reading Explorer 3";
    result.essayBook = "4C";
    result.classColor = "#123456";
    result.fontColor = "#FEDCBA";
    result.regularTimes = {transferTime()};
    result.intensiveTimes = {
        transferTime("Monday", "10:00 AM", "10:55 AM")
    };
    result.notes = "Class notes";
    result.timeFillerActivities = "Word chain";
    result.order = order;
    return result;
}

ClassTransferProjectionInput validInput()
{
    ClassTransferProjectionInput input;
    input.teachers = {
        transferTeacher("teacher-1", "Alex Kim", "Room 504", "Notes 1"),
        transferTeacher("teacher-2", "Jamie Lee", "Room 505", "Notes 2")
    };
    input.classes = {
        transferClass("class-1", std::string("teacher-1"), 1),
        transferClass("class-2", std::nullopt, 2)
    };
    input.classes[1].name = "No Teacher Class";
    input.classes[1].regularTimes.clear();
    input.classes[1].intensiveTimes.clear();
    return input;
}

TeacherId reviewTeacherId(int value);
ClassId reviewClassId(int value);

ClassTransferReviewDecisionRequest validReviewRequest()
{
    ClassTransferReviewDecisionRequest request;
    request.classes = {
        {0, {reviewClassId(41), reviewClassId(42)}},
        {1, {reviewClassId(41), reviewClassId(43)}},
        {2, {}}
    };
    request.teachers = {
        {"teacher-ambiguous", {reviewTeacherId(11), reviewTeacherId(12)}},
        {"teacher-two", {reviewTeacherId(12), reviewTeacherId(13)}},
        {"teacher-unique", {reviewTeacherId(14)}},
        {"teacher-new", {}}
    };
    request.classResolutions = {
        {0, ClassTransferReviewClassAction::Replace, reviewClassId(41)},
        {1, ClassTransferReviewClassAction::Create, std::nullopt},
        {2, ClassTransferReviewClassAction::Skip, std::nullopt}
    };
    request.teacherResolutions = {
        {"teacher-ambiguous", ClassTransferReviewTeacherAction::Create,
         std::nullopt},
        {"teacher-two", ClassTransferReviewTeacherAction::KeepExisting,
         reviewTeacherId(13)},
        {"teacher-unique", ClassTransferReviewTeacherAction::ReplaceExisting,
         reviewTeacherId(14)},
        {"teacher-new", ClassTransferReviewTeacherAction::Create, std::nullopt}
    };
    return request;
}

bool hasReviewIssue(
    const ClassTransferReviewDecisionResult& result,
    const ClassTransferReviewDecisionIssueCode code
    )
{
    return std::any_of(
        result.issues.cbegin(),
        result.issues.cend(),
        [code](const ClassTransferReviewDecisionIssue& issue)
        {
            return issue.code == code;
        }
        );
}

void verifyInvalid(
    const Result<ClassTransferProjection>& result
    )
{
    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
    QVERIFY(!result.error().message.empty());
}

void verifyInvalidValidation(
    const Result<void>& result
    )
{
    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
}

ClassTransferScheduleCandidate transferSchedule(
    const TransferTimeCategory category,
    const std::int64_t startMinuteOfWeek,
    const std::int64_t endMinuteOfWeek
    )
{
    const auto candidate = ClassTransferScheduleCandidate::create(
        category,
        (startMinuteOfWeek / kClassTransferMinutesPerDay) % 7,
        startMinuteOfWeek % kClassTransferMinutesPerDay,
        endMinuteOfWeek % kClassTransferMinutesPerDay
        );
    Q_ASSERT(candidate);
    return candidate.value();
}

TeacherId matchingTeacherId(std::string value)
{
    return *TeacherId::fromString(std::move(value));
}

ClassId matchingClassId(std::string value)
{
    return *ClassId::fromString(std::move(value));
}

TeacherId reviewTeacherId(const int value)
{
    return *TeacherId::fromString(std::to_string(value));
}

ClassId reviewClassId(const int value)
{
    return *ClassId::fromString(std::to_string(value));
}

template <typename Value>
concept HasRawSourceAccessor = requires(const Value& value)
{
    value.rawSource();
};

}

class NextApplicationClassTransferTests final : public QObject
{
    Q_OBJECT

private slots:
    void validBoundedPackageRetainsCompactFieldsAndCounts();
    void regularAndIntensiveTimesRemainExplicitlySeparate();
    void missingTeacherAndEmptyOptionalFieldsAreExplicit();
    void duplicateAndUnknownReferencesAreRejected();
    void blankAndOversizedKeysAndTextAreRejected();
    void negativeOrderingAndTimeValuesAreRejected();
    void everyCollectionLimitRejectsOverflow();
    void lookupsReturnIndependentValueCopies();
    void recordsAndProjectionAreCopyableEqualAndReleasable();
    void contractHasNoExternalOwnersOrRawSourceAccessors();
    void reviewDecisionsPreserveMatchChoiceSemantics();
    void reviewDecisionMatrixRejectsIncompleteDuplicateAndInvalidChoices();
    void teacherMatchingUsesBothOrEitherNameAndRequiresOneName();
    void classMatchingRequiresCourseAndTeacherIdentityOrUnassignedFallback();
    void classAndTeacherMatchListsRetainDestinationOrder();
    void scheduleCandidateFactoryValidatesParsedDayAndClockFields();
    void scheduleOverlapPolicyUsesHalfOpenIntervalsAndStableCategoryOrder();
    void scheduleOverlapPolicyWrapsSundayOvernightIntoMonday();
    void scheduleOverlapPolicyTreatsEqualEndpointsAsTwentyFourHours();
    void existingNextContractsRemainUsable();
};

void NextApplicationClassTransferTests::validBoundedPackageRetainsCompactFieldsAndCounts()
{
    const auto result = ClassTransferProjection::create(validInput());

    QVERIFY(result);
    const auto& projection = result.value();
    QCOMPARE(projection.teacherCount(), std::size_t(2));
    QCOMPARE(projection.classCount(), std::size_t(2));
    QCOMPARE(projection.timeCount(), std::size_t(2));
    QCOMPARE(projection.teachers().size(), std::size_t(2));
    QCOMPARE(projection.classes().size(), std::size_t(2));
    QCOMPARE(projection.teachers().front().sourceKey, std::string("teacher-1"));
    QCOMPARE(projection.teachers().front().displayName, std::string("Alex Kim"));
    QCOMPARE(projection.classes().front().sourceKey, std::string("class-1"));
    QCOMPARE(projection.classes().front().name, std::string("Stored Class"));
    QCOMPARE(projection.classes().front().grade, std::string("E4"));
    QCOMPARE(projection.classes().front().notes, std::string("Class notes"));
    QCOMPARE(projection.classes().front().timeCount(), std::size_t(2));
    QVERIFY(projection.classes().front().hasTeacher());
    QVERIFY(!projection.classes().back().hasTeacher());
}

void NextApplicationClassTransferTests::regularAndIntensiveTimesRemainExplicitlySeparate()
{
    const auto result = ClassTransferProjection::create(validInput());

    QVERIFY(result);
    const auto& projection = result.value();
    const auto regular = projection.findTimes(
        "class-1",
        TransferTimeCategory::Regular
        );
    const auto intensive = projection.lookupTimes(
        "class-1",
        TransferTimeCategory::Intensive
        );

    QVERIFY(regular.has_value());
    QVERIFY(intensive.has_value());
    QCOMPARE(regular->size(), std::size_t(1));
    QCOMPARE(intensive->size(), std::size_t(1));
    QCOMPARE(regular->front().startTime, std::string("4:00 PM"));
    QCOMPARE(regular->front().endTime, std::string("4:50 PM"));
    QCOMPARE(intensive->front().startTime, std::string("10:00 AM"));
    QCOMPARE(intensive->front().endTime, std::string("10:55 AM"));
    QVERIFY(
        projection.findTimes("not-present", TransferTimeCategory::Regular)
            .has_value()
            == false
        );
}

void NextApplicationClassTransferTests::missingTeacherAndEmptyOptionalFieldsAreExplicit()
{
    auto input = validInput();
    input.teachers.front().summary.clear();
    input.teachers.front().notes.clear();
    auto& classWithoutTeacher = input.classes.back();
    classWithoutTeacher.summary.clear();
    classWithoutTeacher.teacherSourceKey.reset();
    classWithoutTeacher.grade.clear();
    classWithoutTeacher.level.clear();
    classWithoutTeacher.readingBook.clear();
    classWithoutTeacher.essayBook.clear();
    classWithoutTeacher.classColor.clear();
    classWithoutTeacher.fontColor.clear();
    classWithoutTeacher.notes.clear();
    classWithoutTeacher.timeFillerActivities.clear();

    const auto result = ClassTransferProjection::create(std::move(input));

    QVERIFY(result);
    const auto classCopy = result.value().findClass("class-2");
    QVERIFY(classCopy.has_value());
    QVERIFY(!classCopy->hasTeacher());
    QVERIFY(!classCopy->teacherSourceKey.has_value());
    QVERIFY(classCopy->summary.empty());
    QVERIFY(classCopy->regularTimes.empty());
    QVERIFY(classCopy->intensiveTimes.empty());
    QVERIFY(result.value().findTeacher("teacher-1")->summary.empty());
}

void NextApplicationClassTransferTests::duplicateAndUnknownReferencesAreRejected()
{
    auto duplicateTeachers = validInput();
    duplicateTeachers.teachers.push_back(
        transferTeacher("teacher-1", "Duplicate")
        );
    verifyInvalid(ClassTransferProjection::create(std::move(duplicateTeachers)));

    auto duplicateClasses = validInput();
    duplicateClasses.classes.push_back(
        transferClass("class-1", std::nullopt)
        );
    verifyInvalid(ClassTransferProjection::create(std::move(duplicateClasses)));

    auto unknownTeacher = validInput();
    unknownTeacher.classes.front().teacherSourceKey = "teacher-404";
    verifyInvalid(ClassTransferProjection::create(std::move(unknownTeacher)));

    auto blankTeacherReference = validInput();
    blankTeacherReference.classes.front().teacherSourceKey = " \t";
    verifyInvalid(
        ClassTransferProjection::create(std::move(blankTeacherReference))
        );
}

void NextApplicationClassTransferTests::blankAndOversizedKeysAndTextAreRejected()
{
    auto blankTeacherKey = validInput();
    blankTeacherKey.teachers.front().sourceKey = " \t";
    verifyInvalid(ClassTransferProjection::create(std::move(blankTeacherKey)));

    auto blankTeacherName = validInput();
    blankTeacherName.teachers.front().displayName.clear();
    verifyInvalid(ClassTransferProjection::create(std::move(blankTeacherName)));

    auto blankClassKey = validInput();
    blankClassKey.classes.front().sourceKey = "\n";
    verifyInvalid(ClassTransferProjection::create(std::move(blankClassKey)));

    auto blankClassName = validInput();
    blankClassName.classes.front().name = " ";
    verifyInvalid(ClassTransferProjection::create(std::move(blankClassName)));

    auto blankTimeDay = validInput();
    blankTimeDay.classes.front().regularTimes.front().day = "\t";
    verifyInvalid(ClassTransferProjection::create(std::move(blankTimeDay)));

    auto blankTimeStart = validInput();
    blankTimeStart.classes.front().regularTimes.front().startTime.clear();
    verifyInvalid(ClassTransferProjection::create(std::move(blankTimeStart)));

    auto blankTimeEnd = validInput();
    blankTimeEnd.classes.front().regularTimes.front().endTime = " ";
    verifyInvalid(ClassTransferProjection::create(std::move(blankTimeEnd)));

    auto oversizedTeacherKey = validInput();
    oversizedTeacherKey.teachers.front().sourceKey = std::string(
        kClassTransferMaxSourceKeyLength + 1,
        't'
        );
    verifyInvalid(
        ClassTransferProjection::create(std::move(oversizedTeacherKey))
        );

    auto oversizedTeacherName = validInput();
    oversizedTeacherName.teachers.front().displayName = std::string(
        kClassTransferMaxTeacherDisplayNameLength + 1,
        'n'
        );
    verifyInvalid(
        ClassTransferProjection::create(std::move(oversizedTeacherName))
        );

    auto oversizedTeacherSummary = validInput();
    oversizedTeacherSummary.teachers.front().summary = std::string(
        kClassTransferMaxTeacherSummaryLength + 1,
        's'
        );
    verifyInvalid(
        ClassTransferProjection::create(std::move(oversizedTeacherSummary))
        );

    auto oversizedTeacherNotes = validInput();
    oversizedTeacherNotes.teachers.front().notes = std::string(
        kClassTransferMaxTeacherNotesLength + 1,
        'n'
        );
    verifyInvalid(
        ClassTransferProjection::create(std::move(oversizedTeacherNotes))
        );

    auto oversizedClassKey = validInput();
    oversizedClassKey.classes.front().sourceKey = std::string(
        kClassTransferMaxSourceKeyLength + 1,
        'c'
        );
    verifyInvalid(ClassTransferProjection::create(std::move(oversizedClassKey)));

    auto oversizedClassName = validInput();
    oversizedClassName.classes.front().name = std::string(
        kClassTransferMaxClassNameLength + 1,
        'c'
        );
    verifyInvalid(
        ClassTransferProjection::create(std::move(oversizedClassName))
        );

    auto oversizedClassSummary = validInput();
    oversizedClassSummary.classes.front().summary = std::string(
        kClassTransferMaxClassSummaryLength + 1,
        's'
        );
    verifyInvalid(
        ClassTransferProjection::create(std::move(oversizedClassSummary))
        );

    auto oversizedGrade = validInput();
    oversizedGrade.classes.front().grade = std::string(
        kClassTransferMaxClassGradeLength + 1,
        'g'
        );
    verifyInvalid(ClassTransferProjection::create(std::move(oversizedGrade)));

    auto oversizedLevel = validInput();
    oversizedLevel.classes.front().level = std::string(
        kClassTransferMaxClassLevelLength + 1,
        'l'
        );
    verifyInvalid(ClassTransferProjection::create(std::move(oversizedLevel)));

    auto oversizedBook = validInput();
    oversizedBook.classes.front().readingBook = std::string(
        kClassTransferMaxClassBookLength + 1,
        'b'
        );
    verifyInvalid(ClassTransferProjection::create(std::move(oversizedBook)));

    auto oversizedColor = validInput();
    oversizedColor.classes.front().fontColor = std::string(
        kClassTransferMaxClassColorLength + 1,
        'c'
        );
    verifyInvalid(ClassTransferProjection::create(std::move(oversizedColor)));

    auto oversizedNotes = validInput();
    oversizedNotes.classes.front().notes = std::string(
        kClassTransferMaxClassNotesLength + 1,
        'n'
        );
    verifyInvalid(ClassTransferProjection::create(std::move(oversizedNotes)));

    auto oversizedFiller = validInput();
    oversizedFiller.classes.front().timeFillerActivities = std::string(
        kClassTransferMaxTimeFillerActivitiesLength + 1,
        'f'
        );
    verifyInvalid(ClassTransferProjection::create(std::move(oversizedFiller)));

    auto oversizedDay = validInput();
    oversizedDay.classes.front().regularTimes.front().day = std::string(
        kClassTransferMaxTimeDayLength + 1,
        'd'
        );
    verifyInvalid(ClassTransferProjection::create(std::move(oversizedDay)));

    auto oversizedStart = validInput();
    oversizedStart.classes.front().regularTimes.front().startTime = std::string(
        kClassTransferMaxTimeValueLength + 1,
        's'
        );
    verifyInvalid(ClassTransferProjection::create(std::move(oversizedStart)));

    auto oversizedEnd = validInput();
    oversizedEnd.classes.front().regularTimes.front().endTime = std::string(
        kClassTransferMaxTimeValueLength + 1,
        'e'
        );
    verifyInvalid(ClassTransferProjection::create(std::move(oversizedEnd)));
}

void NextApplicationClassTransferTests::negativeOrderingAndTimeValuesAreRejected()
{
    auto negativeOrder = validInput();
    negativeOrder.classes.front().order = -1;
    verifyInvalid(ClassTransferProjection::create(std::move(negativeOrder)));

    auto blankTimeValidation = ClassTransferProjection::validate(
        TransferClassTime{"Monday", "", "4:50 PM"}
        );
    verifyInvalidValidation(blankTimeValidation);

    auto validTimeValidation = ClassTransferProjection::validate(
        transferTime()
        );
    QVERIFY(validTimeValidation);
}

void NextApplicationClassTransferTests::everyCollectionLimitRejectsOverflow()
{
    auto tooManyTeachers = validInput();
    tooManyTeachers.teachers.assign(
        kClassTransferMaxTeacherEntries + 1,
        transferTeacher("teacher-overflow")
        );
    verifyInvalid(ClassTransferProjection::create(std::move(tooManyTeachers)));

    auto tooManyClasses = validInput();
    tooManyClasses.classes.assign(
        kClassTransferMaxClassEntries + 1,
        transferClass("class-overflow", std::nullopt)
        );
    verifyInvalid(ClassTransferProjection::create(std::move(tooManyClasses)));

    auto tooManyRegularTimes = validInput();
    tooManyRegularTimes.classes.front().regularTimes.assign(
        kClassTransferMaxRegularTimeEntriesPerClass + 1,
        transferTime()
        );
    verifyInvalid(
        ClassTransferProjection::create(std::move(tooManyRegularTimes))
        );

    auto tooManyIntensiveTimes = validInput();
    tooManyIntensiveTimes.classes.front().intensiveTimes.assign(
        kClassTransferMaxIntensiveTimeEntriesPerClass + 1,
        transferTime("Tuesday", "5:00 PM", "5:50 PM")
        );
    verifyInvalid(
        ClassTransferProjection::create(std::move(tooManyIntensiveTimes))
        );

    ClassTransferProjectionInput tooManyPackageTimes;
    const std::size_t classCount =
        kClassTransferMaxPackageTimeEntries
        / kClassTransferMaxRegularTimeEntriesPerClass
        + 1;
    tooManyPackageTimes.classes.reserve(classCount);
    for (std::size_t index = 0; index < classCount; ++index)
    {
        auto item = transferClass(
            "class-" + std::to_string(index + 1),
            std::nullopt,
            static_cast<std::int32_t>(index)
            );
        item.intensiveTimes.clear();
        item.regularTimes.assign(
            kClassTransferMaxRegularTimeEntriesPerClass,
            transferTime()
            );
        tooManyPackageTimes.classes.push_back(std::move(item));
    }
    verifyInvalid(
        ClassTransferProjection::create(std::move(tooManyPackageTimes))
        );
}

void NextApplicationClassTransferTests::lookupsReturnIndependentValueCopies()
{
    const auto result = ClassTransferProjection::create(validInput());
    QVERIFY(result);
    const ClassTransferProjection projection = result.value();

    auto teacherCopy = projection.findTeacher("teacher-1");
    QVERIFY(teacherCopy.has_value());
    teacherCopy->displayName = "Changed outside projection";
    teacherCopy->notes = "Changed notes";

    auto classCopy = projection.lookupClass("class-1");
    QVERIFY(classCopy.has_value());
    classCopy->name = "Changed class";
    classCopy->regularTimes.front().day = "Changed day";

    auto timesCopy = projection.findTimes(
        "class-1",
        TransferTimeCategory::Regular
        );
    QVERIFY(timesCopy.has_value());
    timesCopy->front().startTime = "Changed time";

    const auto storedTeacher = projection.findTeacher("teacher-1");
    const auto storedClass = projection.findClass("class-1");
    const auto storedTimes = projection.findTimes(
        "class-1",
        TransferTimeCategory::Regular
        );
    QVERIFY(storedTeacher.has_value());
    QVERIFY(storedClass.has_value());
    QVERIFY(storedTimes.has_value());
    QCOMPARE(storedTeacher->displayName, std::string("Alex Kim"));
    QCOMPARE(storedTeacher->notes, std::string("Notes 1"));
    QCOMPARE(storedClass->name, std::string("Stored Class"));
    QCOMPARE(storedClass->regularTimes.front().day, std::string("Monday"));
    QCOMPARE(storedTimes->front().startTime, std::string("4:00 PM"));
    QVERIFY(!projection.findTeacher("not-present").has_value());
    QVERIFY(!projection.findClass("not-present").has_value());
}

void NextApplicationClassTransferTests::recordsAndProjectionAreCopyableEqualAndReleasable()
{
    static_assert(std::is_copy_constructible_v<TransferTeacher>);
    static_assert(std::is_copy_assignable_v<TransferTeacher>);
    static_assert(std::is_copy_constructible_v<TransferClassTime>);
    static_assert(std::is_copy_assignable_v<TransferClassTime>);
    static_assert(std::is_copy_constructible_v<TransferClass>);
    static_assert(std::is_copy_assignable_v<TransferClass>);
    static_assert(std::is_copy_constructible_v<ClassTransferStagingPackage>);
    static_assert(std::is_copy_assignable_v<ClassTransferStagingPackage>);
    static_assert(std::is_copy_constructible_v<ClassTransferProjection>);
    static_assert(std::is_copy_assignable_v<ClassTransferProjection>);

    const auto input = validInput();
    const auto inputCopy = input;
    QVERIFY(inputCopy == input);

    const auto result = ClassTransferProjection::create(input);
    QVERIFY(result);
    const ClassTransferProjection original = result.value();
    const ClassTransferProjection copy = original;
    QVERIFY(copy == original);

    ClassTransferProjection assigned;
    assigned = original;
    QVERIFY(assigned == original);

    const auto moved = std::move(assigned);
    QVERIFY(moved == original);
    QVERIFY(original == copy);
}

void NextApplicationClassTransferTests::contractHasNoExternalOwnersOrRawSourceAccessors()
{
    static_assert(
        std::is_same_v<decltype(TransferTeacher::sourceKey), std::string>
        );
    static_assert(
        std::is_same_v<
            decltype(TransferClass::teacherSourceKey),
            std::optional<std::string>
            >
        );
    static_assert(
        std::is_same_v<
            decltype(TransferClass::regularTimes),
            std::vector<TransferClassTime>
            >
        );
    static_assert(
        std::is_same_v<
            decltype(std::declval<const ClassTransferProjection>().findTeacher(
                std::string_view{}
                )),
            std::optional<TransferTeacher>
            >
        );
    static_assert(
        std::is_same_v<
            decltype(std::declval<const ClassTransferProjection>().findClass(
                std::string_view{}
                )),
            std::optional<TransferClass>
            >
        );
    static_assert(
        std::is_same_v<
            decltype(std::declval<const ClassTransferProjection>().teachers()),
            const std::vector<TransferTeacher>&
            >
        );
    static_assert(
        std::is_same_v<
            decltype(std::declval<const ClassTransferProjection>().classes()),
            const std::vector<TransferClass>&
            >
        );
    static_assert(
        !std::is_pointer_v<
            decltype(std::declval<const ClassTransferProjection>().findTeacher(
                std::string_view{}
                ))
            >
        );
    static_assert(!HasRawSourceAccessor<ClassTransferProjection>);
    static_assert(!HasRawSourceAccessor<ClassTransferStagingPackage>);

    QVERIFY(true);
}

void NextApplicationClassTransferTests::reviewDecisionsPreserveMatchChoiceSemantics()
{
    static_assert(std::is_same_v<
        decltype(std::declval<ClassTransferReviewClassCandidate>()
                     .matchingClassIds),
        std::vector<ClassId>>);
    static_assert(std::is_same_v<
        decltype(std::declval<ClassTransferReviewTeacherCandidate>()
                     .matchingTeacherIds),
        std::vector<TeacherId>>);
    static_assert(std::is_same_v<
        decltype(std::declval<ClassTransferReviewClassResolution>()
                     .targetClassId),
        std::optional<ClassId>>);
    static_assert(std::is_same_v<
        decltype(std::declval<ClassTransferReviewTeacherResolution>()
                     .targetTeacherId),
        std::optional<TeacherId>>);
    static_assert(std::is_same_v<
        decltype(std::declval<ClassTransferReviewDecisionIssue>()
                     .targetClassId),
        std::optional<ClassId>>);
    static_assert(std::is_same_v<
        decltype(std::declval<ClassTransferReviewDecisionIssue>()
                     .targetTeacherId),
        std::optional<TeacherId>>);
    static_assert(!std::is_convertible_v<int, ClassId>);
    static_assert(!std::is_convertible_v<int, TeacherId>);

    const auto request = validReviewRequest();
    const auto result = validateClassTransferReviewDecisions(request);
    QVERIFY(result.accepted());
    QVERIFY(result.issues.empty());

    auto uniqueTeacherCreate = request;
    uniqueTeacherCreate.teacherResolutions[2].action =
        ClassTransferReviewTeacherAction::Create;
    uniqueTeacherCreate.teacherResolutions[2].targetTeacherId.reset();
    const auto uniqueResult =
        validateClassTransferReviewDecisions(uniqueTeacherCreate);
    QVERIFY(!uniqueResult.accepted());
    QVERIFY(hasReviewIssue(
        uniqueResult,
        ClassTransferReviewDecisionIssueCode::UniqueTeacherCannotBeCreated
        ));

    auto ambiguousTeacherReuse = request;
    ambiguousTeacherReuse.teacherResolutions[0].action =
        ClassTransferReviewTeacherAction::KeepExisting;
    ambiguousTeacherReuse.teacherResolutions[0].targetTeacherId =
        reviewTeacherId(12);
    QVERIFY(validateClassTransferReviewDecisions(ambiguousTeacherReuse).accepted());

    auto ambiguousTeacherReplacement = request;
    ambiguousTeacherReplacement.teacherResolutions[0].action =
        ClassTransferReviewTeacherAction::ReplaceExisting;
    ambiguousTeacherReplacement.teacherResolutions[0].targetTeacherId =
        reviewTeacherId(11);
    QVERIFY(validateClassTransferReviewDecisions(
        ambiguousTeacherReplacement).accepted());
}

void NextApplicationClassTransferTests::reviewDecisionMatrixRejectsIncompleteDuplicateAndInvalidChoices()
{
    using ClassAction = ClassTransferReviewClassAction;
    using TeacherAction = ClassTransferReviewTeacherAction;
    using IssueCode = ClassTransferReviewDecisionIssueCode;

    auto request = validReviewRequest();
    request.classResolutions.erase(request.classResolutions.begin());
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::MissingClassResolution
        ));

    request = validReviewRequest();
    request.teacherResolutions.pop_back();
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::MissingTeacherResolution
        ));

    request = validReviewRequest();
    request.classResolutions.push_back(request.classResolutions.front());
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::DuplicateClassResolution
        ));

    request = validReviewRequest();
    request.teacherResolutions.push_back(request.teacherResolutions.front());
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::DuplicateTeacherResolution
        ));

    request = validReviewRequest();
    request.classResolutions[0].action = ClassAction::Invalid;
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::InvalidClassAction
        ));

    request = validReviewRequest();
    request.classResolutions[0].targetClassId.reset();
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::ReplaceClassMissingTarget
        ));

    request = validReviewRequest();
    request.classResolutions[0].targetClassId = reviewClassId(99);
    const auto classTargetResult =
        validateClassTransferReviewDecisions(request);
    QVERIFY(hasReviewIssue(
        classTargetResult,
        IssueCode::ClassTargetNotInMatchSet
        ));
    const auto classTargetIssue = std::find_if(
        classTargetResult.issues.cbegin(),
        classTargetResult.issues.cend(),
        [](const ClassTransferReviewDecisionIssue& issue)
        {
            return issue.code == IssueCode::ClassTargetNotInMatchSet;
        });
    QVERIFY(classTargetIssue != classTargetResult.issues.cend());
    QVERIFY(classTargetIssue->targetClassId.has_value());
    QCOMPARE(classTargetIssue->targetClassId->value(), std::string("99"));
    QVERIFY(!classTargetIssue->targetTeacherId.has_value());

    request = validReviewRequest();
    request.classResolutions[1].targetClassId = reviewClassId(41);
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::NonReplaceClassHasTarget
        ));

    request = validReviewRequest();
    request.classResolutions[1] = {
        1, ClassAction::Replace, reviewClassId(41)};
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::DuplicateClassReplacementTarget
        ));

    request = validReviewRequest();
    request.teacherResolutions[0].action = TeacherAction::Invalid;
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::InvalidTeacherAction
        ));

    request = validReviewRequest();
    request.teacherResolutions[1].targetTeacherId.reset();
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::TeacherActionMissingTarget
        ));

    request = validReviewRequest();
    request.teacherResolutions[1].targetTeacherId = reviewTeacherId(99);
    const auto teacherTargetResult =
        validateClassTransferReviewDecisions(request);
    QVERIFY(hasReviewIssue(
        teacherTargetResult,
        IssueCode::TeacherTargetNotInMatchSet
        ));
    const auto teacherTargetIssue = std::find_if(
        teacherTargetResult.issues.cbegin(),
        teacherTargetResult.issues.cend(),
        [](const ClassTransferReviewDecisionIssue& issue)
        {
            return issue.code == IssueCode::TeacherTargetNotInMatchSet;
        });
    QVERIFY(teacherTargetIssue != teacherTargetResult.issues.cend());
    QVERIFY(teacherTargetIssue->targetTeacherId.has_value());
    QCOMPARE(teacherTargetIssue->targetTeacherId->value(), std::string("99"));
    QVERIFY(!teacherTargetIssue->targetClassId.has_value());

    request = validReviewRequest();
    request.teacherResolutions[0] = {
        "teacher-ambiguous", TeacherAction::Create, reviewTeacherId(11)};
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::CreateTeacherHasTarget
        ));

    request = validReviewRequest();
    request.teacherResolutions[0] = {
        "teacher-ambiguous", TeacherAction::ReplaceExisting,
        reviewTeacherId(12)};
    request.teacherResolutions[1] = {
        "teacher-two", TeacherAction::ReplaceExisting, reviewTeacherId(12)};
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::DuplicateTeacherReplacementTarget
        ));
}

void NextApplicationClassTransferTests::teacherMatchingUsesBothOrEitherNameAndRequiresOneName()
{
    const ClassTransferMatchingTeacherNames bothNames{"alex kim", "김 알렉스"};
    QVERIFY(classTransferTeacherNamesMatch(
        bothNames, {"alex kim", "김 알렉스"}));
    QVERIFY(!classTransferTeacherNamesMatch(
        bothNames, {"alex kim", "김 민수"}));

    const ClassTransferMatchingTeacherNames englishOnly{"alex kim", {}};
    QVERIFY(classTransferTeacherNamesMatch(
        englishOnly, {"alex kim", "김 민수"}));
    QVERIFY(!classTransferTeacherNamesMatch(
        englishOnly, {"alex lee", "김 알렉스"}));

    const ClassTransferMatchingTeacherNames koreanOnly{{}, "김 알렉스"};
    QVERIFY(classTransferTeacherNamesMatch(
        koreanOnly, {"alex lee", "김 알렉스"}));
    QVERIFY(!classTransferTeacherNamesMatch(
        koreanOnly, {"alex kim", "김 민수"}));
    QVERIFY(!classTransferTeacherNamesMatch(
        {{}, {}}, {"alex kim", "김 알렉스"}));
}

void NextApplicationClassTransferTests::classMatchingRequiresCourseAndTeacherIdentityOrUnassignedFallback()
{
    const auto sourceNames = ClassTransferMatchingTeacherNames{
        "alex kim", "김 알렉스"};
    const ClassTransferMatchingRequest request{
        .sourceTeachers = {
            {"teacher-1", sourceNames}
        },
        .destinationTeachers = {},
        .sourceClasses = {
            {0, "teacher-1", "e4", "perseus"},
            {1, "teacher-1", "", "perseus"},
            {2, "teacher-1", "e4", ""},
            {3, "missing-teacher", "e4", "perseus"}
        },
        .destinationClasses = {
            {
                matchingClassId("course-and-teacher-match"),
                "e4",
                "perseus",
                ClassTransferMatchingDestinationTeacher{
                    matchingTeacherId("teacher-id-1"), sourceNames}
            },
            {
                matchingClassId("teacher-identity-mismatch"),
                "e4",
                "perseus",
                ClassTransferMatchingDestinationTeacher{
                    matchingTeacherId("teacher-id-2"),
                    {"alex kim", "김 민수"}}
            },
            {
                matchingClassId("assigned-placeholder"),
                "e4",
                "perseus",
                ClassTransferMatchingDestinationTeacher{
                    matchingTeacherId("teacher-id-unloaded"), {{}, {}}}
            },
            {
                matchingClassId("grade-mismatch"),
                "e5",
                "perseus",
                ClassTransferMatchingDestinationTeacher{
                    matchingTeacherId("teacher-id-1"), sourceNames}
            },
            {
                matchingClassId("level-mismatch"),
                "e4",
                "apollo",
                ClassTransferMatchingDestinationTeacher{
                    matchingTeacherId("teacher-id-1"), sourceNames}
            },
            {
                matchingClassId("unassigned-match"),
                "e4",
                "perseus",
                std::nullopt
            }
        }
    };

    const auto result = matchClassTransferCandidates(request);
    QCOMPARE(result.classes.size(), std::size_t(4));
    QCOMPARE(result.classes[0].matchingClassIds.size(), std::size_t(1));
    QCOMPARE(
        result.classes[0].matchingClassIds[0].value(),
        std::string("course-and-teacher-match")
        );
    QVERIFY(std::none_of(
        result.classes[0].matchingClassIds.cbegin(),
        result.classes[0].matchingClassIds.cend(),
        [](const ClassId& id)
        {
            return id.value() == "assigned-placeholder";
        }
        ));
    QVERIFY(result.classes[1].matchingClassIds.empty());
    QVERIFY(result.classes[2].matchingClassIds.empty());
    QCOMPARE(result.classes[3].matchingClassIds.size(), std::size_t(1));
    QCOMPARE(
        result.classes[3].matchingClassIds[0].value(),
        std::string("unassigned-match")
        );
}

void NextApplicationClassTransferTests::classAndTeacherMatchListsRetainDestinationOrder()
{
    const ClassTransferMatchingTeacherNames names{"alex kim", "김 알렉스"};
    const ClassTransferMatchingRequest request{
        .sourceTeachers = {
            {"teacher-1", names}
        },
        .destinationTeachers = {
            {matchingTeacherId("teacher-later"), names},
            {matchingTeacherId("teacher-earlier"), names}
        },
        .sourceClasses = {
            {0, "teacher-1", "e4", "perseus"}
        },
        .destinationClasses = {
            {
                matchingClassId("class-later"),
                "e4",
                "perseus",
                ClassTransferMatchingDestinationTeacher{
                    matchingTeacherId("teacher-later"), names}
            },
            {
                matchingClassId("class-earlier"),
                "e4",
                "perseus",
                ClassTransferMatchingDestinationTeacher{
                    matchingTeacherId("teacher-earlier"), names}
            }
        }
    };

    const auto result = matchClassTransferCandidates(request);
    QCOMPARE(result.teachers.size(), std::size_t(1));
    QCOMPARE(result.teachers[0].matchingTeacherIds.size(), std::size_t(2));
    QCOMPARE(
        result.teachers[0].matchingTeacherIds[0].value(),
        std::string("teacher-later")
        );
    QCOMPARE(
        result.teachers[0].matchingTeacherIds[1].value(),
        std::string("teacher-earlier")
        );
    QCOMPARE(result.classes.size(), std::size_t(1));
    QCOMPARE(result.classes[0].matchingClassIds.size(), std::size_t(2));
    QCOMPARE(
        result.classes[0].matchingClassIds[0].value(),
        std::string("class-later")
        );
    QCOMPARE(
        result.classes[0].matchingClassIds[1].value(),
        std::string("class-earlier")
    );
}

void NextApplicationClassTransferTests::
    scheduleCandidateFactoryValidatesParsedDayAndClockFields()
{
    const auto invalidCategory = ClassTransferScheduleCandidate::create(
        static_cast<TransferTimeCategory>(-1), 0, 0, 0);
    const auto invalidNegativeDay = ClassTransferScheduleCandidate::create(
        TransferTimeCategory::Regular, -1, 0, 0);
    const auto invalidDayAfterSunday = ClassTransferScheduleCandidate::create(
        TransferTimeCategory::Regular, 7, 0, 0);
    const auto invalidNegativeStart = ClassTransferScheduleCandidate::create(
        TransferTimeCategory::Regular, 0, -1, 0);
    const auto invalidStartAfterDay = ClassTransferScheduleCandidate::create(
        TransferTimeCategory::Regular,
        0,
        kClassTransferMinutesPerDay,
        0
        );
    const auto invalidNegativeEnd = ClassTransferScheduleCandidate::create(
        TransferTimeCategory::Regular, 0, 0, -1);
    const auto invalidEndAfterDay = ClassTransferScheduleCandidate::create(
        TransferTimeCategory::Regular,
        0,
        0,
        kClassTransferMinutesPerDay
        );

    for (const auto* invalid : {
             &invalidCategory,
             &invalidNegativeDay,
             &invalidDayAfterSunday,
             &invalidNegativeStart,
             &invalidStartAfterDay,
             &invalidNegativeEnd,
             &invalidEndAfterDay
         })
    {
        QVERIFY(!*invalid);
        QCOMPARE((*invalid).error().code, ErrorCode::InvalidInput);
    }

    const auto startOfWeek = ClassTransferScheduleCandidate::create(
        TransferTimeCategory::Regular, 0, 0, 1);
    QVERIFY(startOfWeek);
    QCOMPARE(startOfWeek.value().category(), TransferTimeCategory::Regular);
    QCOMPARE(startOfWeek.value().startMinuteOfWeek(), std::int64_t(0));
    QCOMPARE(startOfWeek.value().endMinuteOfWeek(), std::int64_t(1));

    const auto equalEndpoints = ClassTransferScheduleCandidate::create(
        TransferTimeCategory::Intensive, 0, 1439, 1439);
    QVERIFY(equalEndpoints);
    QCOMPARE(equalEndpoints.value().startMinuteOfWeek(), std::int64_t(1439));
    QCOMPARE(equalEndpoints.value().endMinuteOfWeek(), std::int64_t(2879));

    const auto sundayOvernight = ClassTransferScheduleCandidate::create(
        TransferTimeCategory::Regular, 6, 23 * 60 + 59, 60);
    QVERIFY(sundayOvernight);
    QCOMPARE(
        sundayOvernight.value().startMinuteOfWeek(),
        kClassTransferMinutesPerWeek - 1
        );
    QCOMPARE(
        sundayOvernight.value().endMinuteOfWeek(),
        kClassTransferMinutesPerWeek + 60
        );
}

void NextApplicationClassTransferTests::
    scheduleOverlapPolicyUsesHalfOpenIntervalsAndStableCategoryOrder()
{
    constexpr std::int64_t mondayNine = 9 * 60;
    constexpr std::int64_t mondayTen = 10 * 60;
    constexpr std::int64_t mondayNineThirty = 9 * 60 + 30;
    constexpr std::int64_t mondayTenThirty = 10 * 60 + 30;
    constexpr std::int64_t mondayEleven = 11 * 60;

    const std::vector<ClassTransferScheduleCandidate> incoming{
        transferSchedule(TransferTimeCategory::Regular, mondayNine, mondayTen),
        transferSchedule(TransferTimeCategory::Regular, mondayTen, mondayEleven),
        transferSchedule(
            TransferTimeCategory::Regular,
            mondayNineThirty,
            mondayTenThirty
            ),
        transferSchedule(TransferTimeCategory::Intensive, mondayNine, mondayTen)
    };
    const std::vector<ClassTransferScheduleCandidate> existing{
        transferSchedule(
            TransferTimeCategory::Regular,
            mondayNine + 45,
            mondayTen + 15
            ),
        transferSchedule(
            TransferTimeCategory::Intensive,
            mondayNine,
            mondayTen
            )
    };

    const auto conflicts = findClassTransferScheduleConflicts(
        incoming,
        existing
        );

    // Monday 9–10 and 10–11 only touch. Conflicts are ordered by incoming
    // row, later incoming first, then existing; categories stay isolated.
    const std::vector<ClassTransferScheduleConflict> expected{
        {0, 2, true},
        {0, 0, false},
        {1, 2, true},
        {1, 0, false},
        {2, 0, false},
        {3, 1, false}
    };
    QCOMPARE(conflicts.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        QCOMPARE(conflicts[index].incomingIndex, expected[index].incomingIndex);
        QCOMPARE(conflicts[index].otherIndex, expected[index].otherIndex);
        QCOMPARE(
            conflicts[index].otherIsIncoming,
            expected[index].otherIsIncoming
            );
    }
}

void NextApplicationClassTransferTests::
    scheduleOverlapPolicyWrapsSundayOvernightIntoMonday()
{
    constexpr std::int64_t sundayElevenPm =
        6 * kClassTransferMinutesPerDay + 23 * 60;
    constexpr std::int64_t mondayOneAm =
        7 * kClassTransferMinutesPerDay + 60;
    constexpr std::int64_t mondayTwelveThirtyAm = 30;
    constexpr std::int64_t mondayOneThirtyAm = 90;
    constexpr std::int64_t mondayTwoAm = 120;

    const std::vector<ClassTransferScheduleCandidate> incoming{
        transferSchedule(
            TransferTimeCategory::Regular,
            sundayElevenPm,
            mondayOneAm
            ),
        transferSchedule(
            TransferTimeCategory::Regular,
            mondayTwelveThirtyAm,
            mondayOneThirtyAm
            )
    };
    const std::vector<ClassTransferScheduleCandidate> existing{
        transferSchedule(
            TransferTimeCategory::Regular,
            60,
            mondayTwoAm
            )
    };

    const auto conflicts = findClassTransferScheduleConflicts(
        incoming,
        existing
        );
    const std::vector<ClassTransferScheduleConflict> expected{
        {0, 1, true},
        {1, 0, false}
    };
    QCOMPARE(conflicts.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        QCOMPARE(conflicts[index].incomingIndex, expected[index].incomingIndex);
        QCOMPARE(conflicts[index].otherIndex, expected[index].otherIndex);
        QCOMPARE(
            conflicts[index].otherIsIncoming,
            expected[index].otherIsIncoming
            );
    }
}

void NextApplicationClassTransferTests::
    scheduleOverlapPolicyTreatsEqualEndpointsAsTwentyFourHours()
{
    // The repository parser turns Monday 4:00 PM–4:00 PM into this 24-hour
    // interval by advancing the end by one day when end <= start.
    const ClassTransferScheduleCandidate fullDay = transferSchedule(
        TransferTimeCategory::Regular,
        16 * 60,
        kClassTransferMinutesPerDay + 16 * 60
        );
    const ClassTransferScheduleCandidate crossingEnd = transferSchedule(
        TransferTimeCategory::Regular,
        kClassTransferMinutesPerDay + 15 * 60 + 30,
        kClassTransferMinutesPerDay + 16 * 60 + 30
        );
    const ClassTransferScheduleCandidate touchingEnd = transferSchedule(
        TransferTimeCategory::Regular,
        kClassTransferMinutesPerDay + 16 * 60,
        kClassTransferMinutesPerDay + 17 * 60
        );

    QVERIFY(classTransferScheduleIntervalsOverlap(fullDay, crossingEnd));
    QVERIFY(!classTransferScheduleIntervalsOverlap(fullDay, touchingEnd));
    QVERIFY(findClassTransferScheduleConflicts(
        {fullDay},
        {touchingEnd}
        ).empty());
}

void NextApplicationClassTransferTests::existingNextContractsRemainUsable()
{
    static_assert(!std::is_same_v<TeacherId, ClassId>);
    static_assert(!std::is_convertible_v<TeacherId, ClassId>);
    static_assert(!std::is_convertible_v<ClassId, TeacherId>);

    ClassSummaryProjectionInput summaryInput;
    QVERIFY(ClassSummaryProjection::validate(summaryInput));
    const auto summary = ClassSummaryProjection::create(std::move(summaryInput));
    QVERIFY(summary);

    ScheduleViewProjectionInput scheduleInput;
    QVERIFY(ScheduleViewProjection::validate(scheduleInput));
    const auto schedule = ScheduleViewProjection::create(std::move(scheduleInput));
    QVERIFY(schedule);
}

QTEST_APPLESS_MAIN(NextApplicationClassTransferTests)

#include "next_application_class_transfer_tests.moc"
