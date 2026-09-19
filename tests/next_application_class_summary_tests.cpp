#include "next/application/class_summary_projection.h"

#include <QtTest/QtTest>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

namespace
{

TeacherId teacherId(
    std::string value
    )
{
    return *TeacherId::fromString(value);
}

ClassId classId(
    std::string value
    )
{
    return *ClassId::fromString(value);
}

TeacherSummary teacherSummary(
    std::string id,
    std::string displayName = "Teacher Name",
    std::string facilities = "Room 1",
    std::string notes = "Teacher notes"
    )
{
    return TeacherSummary{
        teacherId(std::move(id)),
        std::move(displayName),
        std::move(facilities),
        std::move(notes)
    };
}

ClassSummary classSummary(
    std::string id,
    std::optional<TeacherId> teacher,
    std::size_t studentCount = 24,
    std::int32_t order = 0
    )
{
    return ClassSummary{
        classId(std::move(id)),
        std::move(teacher),
        "Grade 1",
        "Level A",
        "Class label",
        "Mon 10:00",
        studentCount,
        order
    };
}

ClassSummaryProjectionInput validInput()
{
    ClassSummaryProjectionInput input;
    input.teachers = {
        teacherSummary("teacher-1", "Teacher One", "Room 1", "Notes One"),
        teacherSummary("teacher-2", "Teacher Two", "Room 2", "Notes Two")
    };
    input.classes = {
        classSummary("class-1", teacherId("teacher-1"), 24),
        classSummary("class-2", std::nullopt, 0)
    };
    input.classes[0].grade = "Grade 3";
    input.classes[0].level = "Level B";
    input.classes[0].displayLabel = "Teacher One / Grade 3";
    input.classes[0].meetingText = "Tue 14:30";
    input.selectedDetails = SelectedClassDetails{
        classId("class-1"),
        teacherId("teacher-1"),
        "Selected class notes",
        "Teacher One",
        "Room 1",
        "Selected teacher notes"
    };
    return input;
}

void verifyInvalid(
    const Domain::Result<ClassSummaryProjection>& result
    )
{
    QVERIFY(!result);
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, ErrorCode::InvalidInput);
}

}

class NextApplicationClassSummaryTests final : public QObject
{
    Q_OBJECT

private slots:
    void valid96ClassScaleProjectionIsBoundedAndRetainsMetadata();
    void teacherAndClassIdentifiersRemainDistinctTypes();
    void lookupsReturnIndependentValueCopies();
    void missingTeacherUsesExplicitOptionalFallback();
    void selectedDetailsAbsenceAndLookupMissesReturnNullopt();
    void selectedDetailsRequireExistingClassAndMatchingTeacher();
    void duplicateIdentifiersAreRejected();
    void blankAndOversizedFieldsAreRejected();
    void collectionAndStudentCountBoundsAreRejected();
    void projectionsAreCopyableEqualAndIndependentlyReleasable();
    void contractSurfaceHasTypedValuesAndNoPointerLookups();
};

void NextApplicationClassSummaryTests::valid96ClassScaleProjectionIsBoundedAndRetainsMetadata()
{
    ClassSummaryProjectionInput input;
    input.teachers = {
        teacherSummary("teacher-1", "Teacher One", "Room 1", "Notes One"),
        teacherSummary("teacher-2", "Teacher Two", "Room 2", "Notes Two"),
        teacherSummary("teacher-3", "Teacher Three", "Room 3", "Notes Three")
    };
    input.classes.reserve(96);
    for (std::size_t index = 0; index < 96; ++index)
    {
        std::optional<TeacherId> teacher;
        if (index % 3 != 0)
        {
            teacher = teacherId(
                "teacher-" + std::to_string((index % 3) + 1)
                );
        }

        auto summary = classSummary(
            "class-" + std::to_string(index + 1),
            std::move(teacher),
            index,
            static_cast<std::int32_t>(index)
            );
        summary.grade = "Grade " + std::to_string((index % 6) + 1);
        summary.level = "Level " + std::to_string((index % 4) + 1);
        summary.displayLabel = "Visible class " + std::to_string(index + 1);
        summary.meetingText = "Day " + std::to_string((index % 5) + 1);
        input.classes.push_back(std::move(summary));
    }
    input.selectedDetails = SelectedClassDetails{
        classId("class-42"),
        teacherId("teacher-3"),
        "Selected class 42 notes",
        "Teacher Three",
        "Room 3",
        "Teacher Three detail notes"
    };

    const auto result = ClassSummaryProjection::create(std::move(input));

    QVERIFY(result);
    const auto& projection = result.value();
    QCOMPARE(projection.classes().size(), std::size_t(96));
    QCOMPARE(projection.teacherIndex().summaries().size(), std::size_t(3));
    QCOMPARE(
        projection.classes().front().id.value(),
        std::string("class-1")
        );
    QCOMPARE(
        projection.classes().front().displayLabel,
        std::string("Visible class 1")
        );
    QCOMPARE(projection.classes().front().studentCount, std::size_t(0));
    QCOMPARE(projection.classes().front().order, std::int32_t(0));
    QVERIFY(!projection.classes().front().hasTeacher());
    const auto class42 = projection.findClass(classId("class-42"));
    QVERIFY(class42.has_value());
    QCOMPARE(class42->order, std::int32_t(41));
    QVERIFY(projection.findTeacher(teacherId("teacher-2")).has_value());
    QVERIFY(projection.selectedDetails().has_value());
    QCOMPARE(
        projection.selectedDetails()->classId.value(),
        std::string("class-42")
        );
}

void NextApplicationClassSummaryTests::teacherAndClassIdentifiersRemainDistinctTypes()
{
    static_assert(!std::is_same_v<TeacherId, ClassId>);
    static_assert(!std::is_convertible_v<TeacherId, ClassId>);
    static_assert(!std::is_convertible_v<ClassId, TeacherId>);
    static_assert(!std::is_constructible_v<ClassId, TeacherId>);
    static_assert(!std::is_constructible_v<TeacherId, ClassId>);
    static_assert(
        !std::is_constructible_v<
            ClassSummary,
            TeacherId,
            std::optional<TeacherId>,
            std::string,
            std::string,
            std::string,
            std::string,
            std::size_t
            >
        );

    QVERIFY(true);
}

void NextApplicationClassSummaryTests::lookupsReturnIndependentValueCopies()
{
    const auto result = ClassSummaryProjection::create(validInput());
    QVERIFY(result);
    const ClassSummaryProjection projection = result.value();

    auto classCopy = projection.findClass(classId("class-1"));
    QVERIFY(classCopy.has_value());
    classCopy->displayLabel = "Changed outside projection";
    classCopy->studentCount = 1;

    auto teacherCopy = projection.findTeacher(teacherId("teacher-1"));
    QVERIFY(teacherCopy.has_value());
    teacherCopy->displayName = "Changed teacher";
    teacherCopy->notes = "Changed notes";

    const auto storedClass = projection.findClass(classId("class-1"));
    const auto storedTeacher = projection.findTeacher(teacherId("teacher-1"));
    QVERIFY(storedClass.has_value());
    QVERIFY(storedTeacher.has_value());
    QCOMPARE(storedClass->displayLabel, std::string("Teacher One / Grade 3"));
    QCOMPARE(storedClass->studentCount, std::size_t(24));
    QCOMPARE(storedTeacher->displayName, std::string("Teacher One"));
    QCOMPARE(storedTeacher->notes, std::string("Notes One"));
    QVERIFY(!projection.findClass(classId("unknown-class")).has_value());
    QVERIFY(!projection.findTeacher(teacherId("unknown-teacher")).has_value());
}

void NextApplicationClassSummaryTests::missingTeacherUsesExplicitOptionalFallback()
{
    auto input = validInput();
    input.classes[0].teacherId.reset();
    input.selectedDetails->classId = input.classes[0].id;
    input.selectedDetails->teacherId.reset();
    input.selectedDetails->teacherDisplayName.clear();
    input.selectedDetails->teacherFacilities.clear();
    input.selectedDetails->teacherNotes.clear();

    const auto result = ClassSummaryProjection::create(std::move(input));

    QVERIFY(result);
    const auto classCopy = result.value().findClass(classId("class-1"));
    QVERIFY(classCopy.has_value());
    QVERIFY(!classCopy->teacherId.has_value());
    QVERIFY(!classCopy->hasTeacher());
    QVERIFY(result.value().selectedClassDetails().has_value());
    QVERIFY(!result.value().selectedClassDetails()->teacherId.has_value());
    QVERIFY(!result.value().selectedClassDetails()->hasTeacher());
}

void NextApplicationClassSummaryTests::selectedDetailsAbsenceAndLookupMissesReturnNullopt()
{
    auto withoutSelectedDetails = validInput();
    withoutSelectedDetails.selectedDetails.reset();

    const auto absentResult = ClassSummaryProjection::create(
        std::move(withoutSelectedDetails)
        );
    QVERIFY(absentResult);
    const auto& absentProjection = absentResult.value();
    QVERIFY(!absentProjection.selectedDetails().has_value());
    QVERIFY(!absentProjection.selectedClassDetails().has_value());
    QVERIFY(!absentProjection.findSelectedDetails(classId("class-1"))
                 .has_value());
    QVERIFY(!absentProjection.findSelectedDetails(classId("not-visible"))
                 .has_value());

    const auto presentResult = ClassSummaryProjection::create(validInput());
    QVERIFY(presentResult);
    const auto& presentProjection = presentResult.value();
    QVERIFY(
        presentProjection.findSelectedDetails(classId("class-1"))
            .has_value()
        );
    QVERIFY(!presentProjection.findSelectedDetails(classId("class-2"))
                 .has_value());
}

void NextApplicationClassSummaryTests::selectedDetailsRequireExistingClassAndMatchingTeacher()
{
    auto unknownClass = validInput();
    unknownClass.selectedDetails->classId = classId("not-visible");
    verifyInvalid(ClassSummaryProjection::create(std::move(unknownClass)));

    auto mismatchedTeacher = validInput();
    mismatchedTeacher.selectedDetails->teacherId = teacherId("teacher-2");
    verifyInvalid(
        ClassSummaryProjection::create(std::move(mismatchedTeacher))
        );

    auto classWithUnknownTeacher = validInput();
    classWithUnknownTeacher.classes[0].teacherId = teacherId("teacher-404");
    classWithUnknownTeacher.selectedDetails->teacherId = teacherId(
        "teacher-404"
        );
    verifyInvalid(
        ClassSummaryProjection::create(std::move(classWithUnknownTeacher))
        );
}

void NextApplicationClassSummaryTests::duplicateIdentifiersAreRejected()
{
    auto duplicateTeachers = validInput();
    duplicateTeachers.teachers.push_back(
        teacherSummary("teacher-1", "Duplicate teacher")
        );
    verifyInvalid(
        ClassSummaryProjection::create(std::move(duplicateTeachers))
        );

    auto duplicateClasses = validInput();
    duplicateClasses.classes.push_back(
        classSummary("class-1", teacherId("teacher-1"))
        );
    verifyInvalid(ClassSummaryProjection::create(std::move(duplicateClasses)));
}

void NextApplicationClassSummaryTests::blankAndOversizedFieldsAreRejected()
{
    auto blankTeacherName = validInput();
    blankTeacherName.teachers.front().displayName = " \t";
    verifyInvalid(
        ClassSummaryProjection::create(std::move(blankTeacherName))
        );

    auto blankClassText = validInput();
    blankClassText.classes.front().grade = "\t";
    verifyInvalid(
        ClassSummaryProjection::create(std::move(blankClassText))
        );

    auto blankIdentifier = validInput();
    blankIdentifier.teachers.front().id = teacherId(" ");
    verifyInvalid(
        ClassSummaryProjection::create(std::move(blankIdentifier))
        );

    auto oversizedIdentifier = validInput();
    oversizedIdentifier.classes.front().id = classId(
        std::string(kSummaryMaxIdentifierLength + 1, 'c')
        );
    verifyInvalid(
        ClassSummaryProjection::create(std::move(oversizedIdentifier))
        );

    auto oversizedTeacherFields = validInput();
    oversizedTeacherFields.teachers.front().facilities = std::string(
        kTeacherSummaryMaxFacilitiesLength + 1,
        'f'
        );
    verifyInvalid(
        ClassSummaryProjection::create(std::move(oversizedTeacherFields))
        );

    auto oversizedClassFields = validInput();
    oversizedClassFields.classes.front().meetingText = std::string(
        kClassSummaryMaxMeetingTextLength + 1,
        'm'
        );
    verifyInvalid(
        ClassSummaryProjection::create(std::move(oversizedClassFields))
        );

    auto oversizedDetails = validInput();
    oversizedDetails.selectedDetails->classNotes = std::string(
        kSelectedClassDetailsMaxClassNotesLength + 1,
        'n'
        );
    verifyInvalid(
        ClassSummaryProjection::create(std::move(oversizedDetails))
        );
}

void NextApplicationClassSummaryTests::collectionAndStudentCountBoundsAreRejected()
{
    auto tooManyTeachers = validInput();
    tooManyTeachers.teachers.assign(
        kTeacherSummaryMaxEntries + 1,
        teacherSummary("teacher-overflow")
        );
    verifyInvalid(
        ClassSummaryProjection::create(std::move(tooManyTeachers))
        );

    auto tooManyClasses = validInput();
    tooManyClasses.classes.assign(
        kClassSummaryMaxEntries + 1,
        classSummary("class-overflow", std::nullopt)
        );
    verifyInvalid(
        ClassSummaryProjection::create(std::move(tooManyClasses))
        );

    auto tooManyStudents = validInput();
    tooManyStudents.classes.front().studentCount =
        kClassSummaryMaxStudentCount + 1;
    verifyInvalid(
        ClassSummaryProjection::create(std::move(tooManyStudents))
        );

    auto negativeOrder = validInput();
    negativeOrder.classes.front().order = -1;
    verifyInvalid(ClassSummaryProjection::create(std::move(negativeOrder)));
}

void NextApplicationClassSummaryTests::projectionsAreCopyableEqualAndIndependentlyReleasable()
{
    static_assert(std::is_copy_constructible_v<TeacherSummary>);
    static_assert(std::is_copy_assignable_v<TeacherSummary>);
    static_assert(std::is_copy_constructible_v<ClassSummary>);
    static_assert(std::is_copy_assignable_v<ClassSummary>);
    static_assert(std::is_copy_constructible_v<SelectedClassDetails>);
    static_assert(std::is_copy_assignable_v<SelectedClassDetails>);
    static_assert(std::is_copy_constructible_v<TeacherSummaryIndex>);
    static_assert(std::is_copy_assignable_v<TeacherSummaryIndex>);
    static_assert(std::is_copy_constructible_v<ClassSummaryProjection>);
    static_assert(std::is_copy_assignable_v<ClassSummaryProjection>);

    const auto result = ClassSummaryProjection::create(validInput());
    QVERIFY(result);
    const ClassSummaryProjection original = result.value();
    const ClassSummaryProjection copy = original;
    QVERIFY(copy == original);

    ClassSummaryProjection assigned;
    assigned = original;
    QVERIFY(assigned == original);

    const auto released = std::move(assigned);
    QVERIFY(released == original);
    QVERIFY(original == copy);
}

void NextApplicationClassSummaryTests::contractSurfaceHasTypedValuesAndNoPointerLookups()
{
    static_assert(std::is_same_v<decltype(TeacherSummary::id), TeacherId>);
    static_assert(std::is_same_v<decltype(ClassSummary::id), ClassId>);
    static_assert(std::is_same_v<decltype(ClassSummary::order), std::int32_t>);
    static_assert(
        std::is_same_v<
            decltype(std::declval<const TeacherSummaryIndex>().find(
                std::declval<const TeacherId&>()
                )),
            std::optional<TeacherSummary>
            >
        );
    static_assert(
        std::is_same_v<
            decltype(std::declval<const ClassSummaryProjection>().findClass(
                std::declval<const ClassId&>()
                )),
            std::optional<ClassSummary>
            >
        );
    static_assert(
        std::is_same_v<
            decltype(std::declval<const ClassSummaryProjection>().findTeacher(
                std::declval<const TeacherId&>()
                )),
            std::optional<TeacherSummary>
            >
        );
    static_assert(
        !std::is_pointer_v<
            decltype(std::declval<const ClassSummaryProjection>().selectedDetails())
            >
        );

    QVERIFY(true);
}

QTEST_APPLESS_MAIN(NextApplicationClassSummaryTests)

#include "next_application_class_summary_tests.moc"
