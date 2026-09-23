#include "features/sub_prep/ui/sub_prep_class_information_list_model.h"

#include <QtTest>

#include <string>

namespace
{
using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;

Domain::ClassId classId(
    int id
    )
{
    return *Domain::ClassId::fromString(std::to_string(id));
}

Domain::TeacherId teacherId(
    int id
    )
{
    return *Domain::TeacherId::fromString(std::to_string(id));
}

ClassSummary summary(
    int id,
    int teacher,
    const char* grade,
    const char* level,
    const char* meeting,
    int order
    )
{
    return {
        .id = classId(id),
        .teacherId = teacherId(teacher),
        .grade = grade,
        .level = level,
        .displayLabel = std::string(grade) + " " + level,
        .meetingText = meeting,
        .studentCount = 12,
        .order = order
    };
}

ClassSummaryProjection projection(
    std::vector<ClassSummary> classes,
    std::vector<TeacherSummary> teachers
    )
{
    auto result = ClassSummaryProjection::create({
        .teachers = std::move(teachers),
        .classes = std::move(classes),
        .selectedDetails = std::nullopt
    });
    Q_ASSERT(result);
    return std::move(result.value());
}
}

class SubPrepClassInformationListModelTests final : public QObject
{
    Q_OBJECT

private slots:
    void startsEmpty();
    void filtersAndOrdersRowsByGradeAndConfiguredLevel();
    void addsTeacherNamesToDuplicateNavigationLabels();
};

void SubPrepClassInformationListModelTests::startsEmpty()
{
    SubPrepClassInformationListModel model;

    QCOMPARE(model.rowCount(), 0);
    QVERIFY(model.grades().isEmpty());
    QVERIFY(model.currentGrade().isEmpty());
    QVERIFY(!model.classIdAt(0).has_value());
}

void SubPrepClassInformationListModelTests::
filtersAndOrdersRowsByGradeAndConfiguredLevel()
{
    SubPrepClassInformationListModel model;
    auto source = projection(
        {
            summary(42, 7, "E4", "Hercules", "Mon 9am", 0),
            summary(43, 8, "E5", "Athena", "Tues 10am", 1),
            summary(44, 7, "E4", "Theseus", "Wed 8am", 2)
        },
        {
            {
                .id = teacherId(7),
                .displayName = "Susan",
                .facilities = {},
                .notes = {}
            },
            {
                .id = teacherId(8),
                .displayName = "Athena Teacher",
                .facilities = {},
                .notes = {}
            }
        }
        );

    model.setProjection(std::move(source));

    QCOMPARE(model.grades(), QStringList({"E4", "E5"}));
    QCOMPARE(model.currentGrade(), QStringLiteral("E4"));
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.classIdAt(0)->value(), std::string("44"));
    QCOMPARE(model.classIdAt(1)->value(), std::string("42"));
    QCOMPARE(
        model.data(model.index(0, 0), Qt::DisplayRole).toString(),
        QStringLiteral("Theseus • Wed 8am")
        );
    QCOMPARE(
        model.data(model.index(0, 0),
                   SubPrepClassInformationListModel::GradeRole).toString(),
        QStringLiteral("E4")
        );
    QCOMPARE(
        model.data(model.index(0, 0),
                   SubPrepClassInformationListModel::StudentCountRole).toULongLong(),
        qulonglong(12)
        );

    model.setCurrentGrade(QStringLiteral("E5"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.classIdAt(0)->value(), std::string("43"));
    QCOMPARE(model.rowForClassId(classId(43)), 0);
    QCOMPARE(model.rowForClassId(classId(42)), -1);
}

void SubPrepClassInformationListModelTests::
addsTeacherNamesToDuplicateNavigationLabels()
{
    SubPrepClassInformationListModel model;
    model.setProjection(projection(
        {
            summary(42, 7, "E4", "Hercules", "Mon 9am", 0),
            summary(43, 8, "E4", "Hercules", "Mon 9am", 1)
        },
        {
            {
                .id = teacherId(7),
                .displayName = "Susan",
                .facilities = {},
                .notes = {}
            },
            {
                .id = teacherId(8),
                .displayName = "Athena Teacher",
                .facilities = {},
                .notes = {}
            }
        }
        ));

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(
        model.data(model.index(0, 0), Qt::DisplayRole).toString(),
        QStringLiteral("Hercules • Mon 9am — Susan")
        );
    QCOMPARE(
        model.data(model.index(1, 0), Qt::DisplayRole).toString(),
        QStringLiteral("Hercules • Mon 9am — Athena Teacher")
        );
}

QTEST_APPLESS_MAIN(SubPrepClassInformationListModelTests)

#include "sub_prep_class_information_list_model_tests.moc"
