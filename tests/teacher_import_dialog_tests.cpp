#include "features/teacher/ui/teacher_import_dialog.h"

#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QtTest>

class TeacherImportDialogTests : public QObject
{
    Q_OBJECT

private slots:
    void suppliedWorkbookBuildsDynamicSelectionUi();
};

void TeacherImportDialogTests::suppliedWorkbookBuildsDynamicSelectionUi()
{
    const QString path = qEnvironmentVariable("CLASSMNGR_TEACHER_IMPORT_SAMPLE");
    if (path.isEmpty())
    {
        QSKIP("Set CLASSMNGR_TEACHER_IMPORT_SAMPLE to validate an external workbook.");
    }

    TeacherImportDialog dialog;
    dialog.setFilePath(path);

    auto* importButton = dialog.findChild<QPushButton*>(
        QStringLiteral("teacherImportAcceptButton"));
    QVERIFY(importButton);
    QVERIFY(importButton->isEnabled());
    const auto* templateLabel = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportTemplateName"));
    const auto* automaticLabel = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportAutomaticCounts"));
    QVERIFY(templateLabel);
    QVERIFY(templateLabel->text().contains(QStringLiteral("Sectioned Teacher Contact List")));
    QVERIFY(automaticLabel);
    QVERIFY(automaticLabel->text().contains(QStringLiteral("10")));
    QVERIFY(automaticLabel->text().contains(QStringLiteral("9")));
    QVERIFY(dialog.findChild<QRadioButton*>(QStringLiteral("teacherImportAll_M1")));
    QVERIFY(dialog.findChild<QRadioButton*>(QStringLiteral("teacherImportAll_M2")));
    QVERIFY(dialog.findChild<QRadioButton*>(QStringLiteral("teacherImportAll_M3")));
    QVERIFY(dialog.findChild<QRadioButton*>(QStringLiteral("teacherImportAll_H1")));
    QVERIFY(dialog.findChild<QRadioButton*>(QStringLiteral("teacherImportAll_H2")));
    QVERIFY(!dialog.findChild<QRadioButton*>(QStringLiteral("teacherImportAll_Elem Only")));

    TeacherImportPlan plan = dialog.importPlan();
    QCOMPARE(plan.koreanTeachers.size(), 38);
    QCOMPARE(plan.nativeEnglishTeachers.size(), 10);
    QCOMPARE(plan.gsTeamMembers.size(), 9);

    auto* noneM1 = dialog.findChild<QRadioButton*>(
        QStringLiteral("teacherImportNone_M1"));
    QVERIFY(noneM1);
    noneM1->setChecked(true);
    plan = dialog.importPlan();
    QCOMPARE(plan.koreanTeachers.size(), 24);
    QCOMPARE(plan.nativeEnglishTeachers.size(), 10);
    QCOMPARE(plan.gsTeamMembers.size(), 9);

    dialog.setFilePath(QStringLiteral("/definitely/missing/teacher-list.xlsx"));
    QVERIFY(!importButton->isEnabled());
    QVERIFY(templateLabel->text().isEmpty());
    const auto* validationLabel = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportValidationStatus"));
    QVERIFY(validationLabel);
    QVERIFY(!validationLabel->text().isEmpty());
}

QTEST_MAIN(TeacherImportDialogTests)

#include "teacher_import_dialog_tests.moc"
