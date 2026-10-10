#include "features/sub_prep/ui/sub_prep_class_information_list_model.h"
#include "ui/shared/widgets/navigation_pill_style.h"

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
    void appliesLegacyScheduleAndLabelTieBreakers();
    void addsTeacherNamesToDuplicateNavigationLabels();
    void pixelScrollingKeepsThePriorTabTailVisible();
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

    model.setProjection(projection(
        {
            summary(43, 8, "E5", "Athena", "Tues 10am", 0),
            summary(42, 7, "E4", "Hercules", "Mon 9am", 1)
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
    QCOMPARE(model.currentGrade(), QStringLiteral("E5"));
    QCOMPARE(model.classIdAt(0)->value(), std::string("43"));
}

void SubPrepClassInformationListModelTests::
appliesLegacyScheduleAndLabelTieBreakers()
{
    auto mondayEarlier = summary(
        90,
        7,
        "E4",
        "Hercules",
        "Tue 9am",
        3
        );
    mondayEarlier.navigationLabel = "Hercules • M/F 9:00";
    mondayEarlier.navigationFirstDayOrder = 0;
    mondayEarlier.navigationFirstTimeOrder = 9 * 60;

    auto mondayLabelZ = summary(
        30,
        7,
        "E4",
        "Hercules",
        "Mon 10am",
        0
        );
    mondayLabelZ.navigationLabel = "Hercules • Z 10:00";
    mondayLabelZ.navigationFirstDayOrder = 0;
    mondayLabelZ.navigationFirstTimeOrder = 10 * 60;

    auto mondayLabelA = summary(
        20,
        7,
        "E4",
        "Hercules",
        "Mon 10am",
        4
        );
    mondayLabelA.navigationLabel = "Hercules • A 10:00";
    mondayLabelA.navigationFirstDayOrder = 0;
    mondayLabelA.navigationFirstTimeOrder = 10 * 60;

    auto mondayLabelBLowerId = summary(
        25,
        7,
        "E4",
        "Hercules",
        "Mon 10am",
        1
        );
    mondayLabelBLowerId.navigationLabel = "Hercules • B 10:00";
    mondayLabelBLowerId.navigationFirstDayOrder = 0;
    mondayLabelBLowerId.navigationFirstTimeOrder = 10 * 60;

    auto tuesdayEarlier = summary(
        10,
        7,
        "E4",
        "Hercules",
        "Tue 8am",
        2
        );
    tuesdayEarlier.navigationLabel = "Hercules • T 8:00";
    tuesdayEarlier.navigationFirstDayOrder = 1;
    tuesdayEarlier.navigationFirstTimeOrder = 8 * 60;

    SubPrepClassInformationListModel model;
    model.setProjection(projection(
        {
            mondayLabelZ,
            tuesdayEarlier,
            mondayLabelBLowerId,
            mondayLabelA,
            mondayEarlier
        },
        {
            {
                .id = teacherId(7),
                .displayName = "Legacy Teacher",
                .facilities = {},
                .notes = {}
            }
        }
        ));

    QCOMPARE(model.rowCount(), 5);
    QCOMPARE(model.classIdAt(0)->value(), std::string("90"));
    QCOMPARE(model.classIdAt(1)->value(), std::string("20"));
    QCOMPARE(model.classIdAt(2)->value(), std::string("25"));
    QCOMPARE(model.classIdAt(3)->value(), std::string("30"));
    QCOMPARE(model.classIdAt(4)->value(), std::string("10"));
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
        QStringLiteral("Hercules • Mon 9am • Susan")
        );
    QCOMPARE(
        model.data(model.index(1, 0), Qt::DisplayRole).toString(),
        QStringLiteral("Hercules • Mon 9am • Athena Teacher")
        );
}

void SubPrepClassInformationListModelTests::
pixelScrollingKeepsThePriorTabTailVisible()
{
    auto prior = summary(101, 21, "E4", "Hercules", "Mon 9am", 0);
    prior.navigationLabel = "Prior class schedule";
    prior.navigationTeacherLabel = "Teacher 21";
    prior.navigationFirstDayOrder = 0;
    prior.navigationFirstTimeOrder = 9 * 60;

    auto priorDuplicate = summary(102, 22, "E4", "Hercules", "Mon 9am", 1);
    priorDuplicate.navigationLabel = "Prior class schedule";
    priorDuplicate.navigationTeacherLabel = "Teacher 22";
    priorDuplicate.navigationFirstDayOrder = 0;
    priorDuplicate.navigationFirstTimeOrder = 9 * 60;

    auto selected = summary(103, 23, "E4", "Hercules", "Mon 10am", 2);
    selected.navigationLabel = "Selected meeting schedule 09:00";
    selected.navigationFirstDayOrder = 0;
    selected.navigationFirstTimeOrder = 10 * 60;

    SubPrepClassInformationListModel model;
    model.setProjection(projection(
        {selected, priorDuplicate, prior},
        {
            {
                .id = teacherId(21),
                .displayName = "Teacher 21",
                .facilities = {},
                .notes = {}
            },
            {
                .id = teacherId(22),
                .displayName = "Teacher 22",
                .facilities = {},
                .notes = {}
            },
            {
                .id = teacherId(23),
                .displayName = "Teacher 23",
                .facilities = {},
                .notes = {}
            }
        }
        ));

    QCOMPARE(model.classIdAt(0)->value(), std::string("101"));
    QCOMPARE(model.classIdAt(1)->value(), std::string("102"));
    QCOMPARE(model.classIdAt(2)->value(), std::string("103"));
    QVERIFY(
        model.data(model.index(0, 0), Qt::DisplayRole)
            .toString()
            .endsWith(QStringLiteral("Teacher 21"))
        );

    SubPrepClassInformationTabSelector selector;
    selector.setModel(&model);
    selector.resize(320, selector.height());
    selector.setCurrentRow(0);
    QCOMPARE(selector.horizontalScrollOffset(), 0);
    QCOMPARE(selector.tabRectForRow(0).left(), 28);

    selector.setCurrentRow(2);

    const QRect priorRect = selector.tabRectForRow(1);
    const QRect selectedRect = selector.tabRectForRow(2);
    const int viewportLeft = 28;
    const int viewportRight = selector.width() - 28 - 1;
    QVERIFY(selector.horizontalScrollOffset() > 0);
    QCOMPARE(selectedRect.right(), viewportRight);
    QVERIFY(priorRect.left() < viewportLeft);
    QVERIFY(priorRect.right() >= viewportLeft);
    QVERIFY(priorRect.right() < viewportRight);
    QCOMPARE(
        selectedRect.left() - priorRect.right() - 1,
        NavigationPillStyle::Gap
        );
}

QTEST_MAIN(SubPrepClassInformationListModelTests)

#include "sub_prep_class_information_list_model_tests.moc"
