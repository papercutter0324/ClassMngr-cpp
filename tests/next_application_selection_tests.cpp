#include "next/application/selection_state.h"

#include <QtTest/QtTest>

#include <type_traits>
#include <variant>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Domain;

class NextApplicationSelectionTests final : public QObject
{
    Q_OBJECT

private slots:
    void initialStateHasNoSelection();
    void selectingEachTypedCategoryExposesStableKindAndAccessor();
    void replacingSelectionReplacesThePreviousCategory();
    void clearingSelectionRestoresNoSelection();
    void snapshotsAreCopyableValuesWithEquality();
    void typedCategoriesRemainDistinctAtCompileTime();
};

void NextApplicationSelectionTests::initialStateHasNoSelection()
{
    SelectionState state;

    const SelectionStateSnapshot snapshot = state.snapshot();

    QCOMPARE(snapshot.kind(), SelectionKind::None);
    QVERIFY(!snapshot.hasSelection());
    QVERIFY(std::holds_alternative<std::monostate>(snapshot.value()));
    QVERIFY(snapshot.teacherId() == nullptr);
    QVERIFY(snapshot.classId() == nullptr);
    QVERIFY(snapshot.campusId() == nullptr);
    QVERIFY(snapshot.calendarEventId() == nullptr);
}

void NextApplicationSelectionTests::selectingEachTypedCategoryExposesStableKindAndAccessor()
{
    SelectionState state;

    state.setSelection(*TeacherId::fromString("teacher-1"));
    const SelectionStateSnapshot teacherSnapshot = state.snapshot();
    QCOMPARE(teacherSnapshot.kind(), SelectionKind::Teacher);
    QVERIFY(teacherSnapshot.hasSelection());
    QVERIFY(teacherSnapshot.teacherId() != nullptr);
    QVERIFY(teacherSnapshot.teacherId()->value() == "teacher-1");

    state.setSelection(*ClassId::fromString("class-1"));
    const SelectionStateSnapshot classSnapshot = state.snapshot();
    QCOMPARE(classSnapshot.kind(), SelectionKind::Class);
    QVERIFY(classSnapshot.classId() != nullptr);
    QVERIFY(classSnapshot.classId()->value() == "class-1");

    state.setSelection(*CampusId::fromString("campus-1"));
    const SelectionStateSnapshot campusSnapshot = state.snapshot();
    QCOMPARE(campusSnapshot.kind(), SelectionKind::Campus);
    QVERIFY(campusSnapshot.campusId() != nullptr);
    QVERIFY(campusSnapshot.campusId()->value() == "campus-1");

    state.setSelection(*CalendarEventId::fromString("event-1"));
    const SelectionStateSnapshot eventSnapshot = state.snapshot();
    QCOMPARE(eventSnapshot.kind(), SelectionKind::CalendarEvent);
    QVERIFY(eventSnapshot.calendarEventId() != nullptr);
    QVERIFY(eventSnapshot.calendarEventId()->value() == "event-1");
}

void NextApplicationSelectionTests::replacingSelectionReplacesThePreviousCategory()
{
    SelectionState state;

    state.setSelection(*TeacherId::fromString("teacher-1"));
    const SelectionStateSnapshot firstSnapshot = state.snapshot();

    state.setSelection(*ClassId::fromString("class-1"));
    const SelectionStateSnapshot replacementSnapshot = state.snapshot();

    QCOMPARE(replacementSnapshot.kind(), SelectionKind::Class);
    QVERIFY(replacementSnapshot.classId() != nullptr);
    QVERIFY(replacementSnapshot.classId()->value() == "class-1");
    QVERIFY(replacementSnapshot.teacherId() == nullptr);
    QVERIFY(firstSnapshot.kind() == SelectionKind::Teacher);
    QVERIFY(firstSnapshot.teacherId() != nullptr);
    QVERIFY(firstSnapshot.teacherId()->value() == "teacher-1");
}

void NextApplicationSelectionTests::clearingSelectionRestoresNoSelection()
{
    SelectionState state;
    state.setSelection(*CampusId::fromString("campus-1"));

    state.clear();
    const SelectionStateSnapshot snapshot = state.snapshot();

    QCOMPARE(snapshot.kind(), SelectionKind::None);
    QVERIFY(!snapshot.hasSelection());
    QVERIFY(std::holds_alternative<std::monostate>(snapshot.value()));
}

void NextApplicationSelectionTests::snapshotsAreCopyableValuesWithEquality()
{
    static_assert(std::is_copy_constructible_v<SelectionStateSnapshot>);
    static_assert(std::is_copy_assignable_v<SelectionStateSnapshot>);

    SelectionState state;
    state.setSelection(*CalendarEventId::fromString("event-1"));

    const SelectionStateSnapshot original = state.snapshot();
    const SelectionStateSnapshot copy = original;
    QVERIFY(copy == original);

    state.clear();
    QVERIFY(original == copy);
    QVERIFY(!(state.snapshot() == original));
    QVERIFY(original.calendarEventId() != nullptr);
    QVERIFY(original.calendarEventId()->value() == "event-1");
}

void NextApplicationSelectionTests::typedCategoriesRemainDistinctAtCompileTime()
{
    static_assert(!std::is_same_v<TeacherId, ClassId>);
    static_assert(!std::is_same_v<ClassId, CampusId>);
    static_assert(!std::is_same_v<CampusId, CalendarEventId>);
    static_assert(!std::is_convertible_v<TeacherId, ClassId>);
    static_assert(!std::is_constructible_v<ClassId, TeacherId>);
    static_assert(
        std::is_same_v<
            std::variant_alternative_t<1, SelectionValue>,
            TeacherId
            >
        );

    QVERIFY(true);
}

QTEST_APPLESS_MAIN(NextApplicationSelectionTests)

#include "next_application_selection_tests.moc"
