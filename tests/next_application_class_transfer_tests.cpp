#include "next/application/class_summary_projection.h"
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

ClassTransferReviewDecisionRequest validReviewRequest()
{
    ClassTransferReviewDecisionRequest request;
    request.classes = {
        {0, {41, 42}},
        {1, {41, 43}},
        {2, {}}
    };
    request.teachers = {
        {"teacher-ambiguous", {11, 12}},
        {"teacher-two", {12, 13}},
        {"teacher-unique", {14}},
        {"teacher-new", {}}
    };
    request.classResolutions = {
        {0, ClassTransferReviewClassAction::Replace, 41},
        {1, ClassTransferReviewClassAction::Create, -1},
        {2, ClassTransferReviewClassAction::Skip, -1}
    };
    request.teacherResolutions = {
        {"teacher-ambiguous", ClassTransferReviewTeacherAction::Create, -1},
        {"teacher-two", ClassTransferReviewTeacherAction::KeepExisting, 13},
        {"teacher-unique", ClassTransferReviewTeacherAction::ReplaceExisting, 14},
        {"teacher-new", ClassTransferReviewTeacherAction::Create, -1}
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
    const auto request = validReviewRequest();
    const auto result = validateClassTransferReviewDecisions(request);
    QVERIFY(result.accepted());
    QVERIFY(result.issues.empty());

    auto uniqueTeacherCreate = request;
    uniqueTeacherCreate.teacherResolutions[2].action =
        ClassTransferReviewTeacherAction::Create;
    uniqueTeacherCreate.teacherResolutions[2].targetTeacherId = -1;
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
    ambiguousTeacherReuse.teacherResolutions[0].targetTeacherId = 12;
    QVERIFY(validateClassTransferReviewDecisions(ambiguousTeacherReuse).accepted());

    auto ambiguousTeacherReplacement = request;
    ambiguousTeacherReplacement.teacherResolutions[0].action =
        ClassTransferReviewTeacherAction::ReplaceExisting;
    ambiguousTeacherReplacement.teacherResolutions[0].targetTeacherId = 11;
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
    request.classResolutions[0].targetClassId = -1;
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::ReplaceClassMissingTarget
        ));

    request = validReviewRequest();
    request.classResolutions[0].targetClassId = 99;
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::ClassTargetNotInMatchSet
        ));

    request = validReviewRequest();
    request.classResolutions[1].targetClassId = 41;
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::NonReplaceClassHasTarget
        ));

    request = validReviewRequest();
    request.classResolutions[1] = {1, ClassAction::Replace, 41};
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
    request.teacherResolutions[1].targetTeacherId = -1;
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::TeacherActionMissingTarget
        ));

    request = validReviewRequest();
    request.teacherResolutions[1].targetTeacherId = 99;
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::TeacherTargetNotInMatchSet
        ));

    request = validReviewRequest();
    request.teacherResolutions[0] = {
        "teacher-ambiguous", TeacherAction::Create, 11};
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::CreateTeacherHasTarget
        ));

    request = validReviewRequest();
    request.teacherResolutions[0] = {
        "teacher-ambiguous", TeacherAction::ReplaceExisting, 12};
    request.teacherResolutions[1] = {
        "teacher-two", TeacherAction::ReplaceExisting, 12};
    QVERIFY(hasReviewIssue(
        validateClassTransferReviewDecisions(request),
        IssueCode::DuplicateTeacherReplacementTarget
        ));
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
