#include "ui/shared/widgets/sections/teacher_info_section.h"

#include <QComboBox>
#include <QtTest>

class TeacherInfoSectionTests : public QObject
{
    Q_OBJECT

private slots:
    void teacherCombosUseLocaleSpecificSortingAndSyncSelection();
};

void TeacherInfoSectionTests
    ::teacherCombosUseLocaleSpecificSortingAndSyncSelection()
{
    QList<Teacher> teachers;

    Teacher bob;
    bob.id = 1;
    bob.teacherKr = QStringLiteral("다솜");
    bob.teacherEn = QStringLiteral("Bob");
    teachers.append(bob);

    Teacher charlie;
    charlie.id = 2;
    charlie.teacherKr = QStringLiteral("가영");
    charlie.teacherEn = QStringLiteral("Charlie");
    teachers.append(charlie);

    Teacher alice;
    alice.id = 3;
    alice.teacherKr = QStringLiteral("나래");
    alice.teacherEn = QStringLiteral("Alice");
    teachers.append(alice);

    TeacherInfoSection section;
    section.setTeachers(
        teachers
        );

    auto* koreanCombo =
        section.findChild<QComboBox*>(
            QStringLiteral("teacherKrCombo")
            );
    auto* englishCombo =
        section.findChild<QComboBox*>(
            QStringLiteral("teacherEnCombo")
            );

    QVERIFY(koreanCombo);
    QVERIFY(englishCombo);

    QCOMPARE(koreanCombo->count(), 4);
    QCOMPARE(englishCombo->count(), 4);

    QCOMPARE(koreanCombo->itemText(1), QStringLiteral("가영"));
    QCOMPARE(koreanCombo->itemData(1).toInt(), 2);
    QCOMPARE(koreanCombo->itemText(2), QStringLiteral("나래"));
    QCOMPARE(koreanCombo->itemData(2).toInt(), 3);
    QCOMPARE(koreanCombo->itemText(3), QStringLiteral("다솜"));
    QCOMPARE(koreanCombo->itemData(3).toInt(), 1);

    QCOMPARE(englishCombo->itemText(1), QStringLiteral("Alice"));
    QCOMPARE(englishCombo->itemData(1).toInt(), 3);
    QCOMPARE(englishCombo->itemText(2), QStringLiteral("Bob"));
    QCOMPARE(englishCombo->itemData(2).toInt(), 1);
    QCOMPARE(englishCombo->itemText(3), QStringLiteral("Charlie"));
    QCOMPARE(englishCombo->itemData(3).toInt(), 2);

    englishCombo->setCurrentIndex(
        englishCombo->findData(1)
        );

    QCOMPARE(section.teacherId(), 1);
    QCOMPARE(koreanCombo->currentData().toInt(), 1);
    QCOMPARE(koreanCombo->currentText(), QStringLiteral("다솜"));

    koreanCombo->setCurrentIndex(
        koreanCombo->findData(2)
        );

    QCOMPARE(section.teacherId(), 2);
    QCOMPARE(englishCombo->currentData().toInt(), 2);
    QCOMPARE(englishCombo->currentText(), QStringLiteral("Charlie"));
}

QTEST_MAIN(TeacherInfoSectionTests)

#include "teacher_info_section_tests.moc"
