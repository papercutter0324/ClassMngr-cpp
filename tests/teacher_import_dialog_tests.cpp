#include "features/teacher/ui/teacher_import_dialog.h"
#include "fakes/fake_file_dialog_service.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QtTest>

class TeacherImportDialogTests : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void showsFilePromptAndInvalidStatus();
    void browseUsesTypedFileDialogRequest();
    void suppliedWorkbookBuildsDynamicSelectionUi();
};

void TeacherImportDialogTests::cleanup()
{
    DialogServices::setFileDialogServiceForTesting(nullptr);
}

void TeacherImportDialogTests::browseUsesTypedFileDialogRequest()
{
    FakeFileDialogService fileDialogs;
    DialogServices::setFileDialogServiceForTesting(&fileDialogs);
    TeacherImportDialog dialog;

    auto* browseButton = dialog.findChild<QPushButton*>(
        QStringLiteral("teacherImportBrowseButton")
        );
    QVERIFY(browseButton);
    browseButton->click();

    QCOMPARE(fileDialogs.openFileRequests.size(), 1);
    const OpenFileRequest& request = fileDialogs.openFileRequests.first();
    QCOMPARE(request.parent, &dialog);
    QCOMPARE(request.title, QStringLiteral("Select Teacher Import File"));
    QCOMPARE(request.purpose, FileDialogPurpose::ImportWorkbook);
    QCOMPARE(
        request.nameFilters,
        QStringList({QStringLiteral("Excel Workbooks (*.xlsx)")})
        );
}

void TeacherImportDialogTests::showsFilePromptAndInvalidStatus()
{
    TeacherImportDialog dialog;

    const auto* fileLabel = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportFilePathLabel"));
    QVERIFY(fileLabel);
    QCOMPARE(fileLabel->text(), QStringLiteral("File to import from:"));
    QVERIFY(fileLabel->alignment().testFlag(Qt::AlignTop));

    const auto* validationLabel = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportValidationStatus"));
    QVERIFY(validationLabel);
    QCOMPARE(validationLabel->text(), QStringLiteral("Choose a file to import from."));
    QVERIFY(validationLabel->alignment().testFlag(Qt::AlignTop));

    const auto* fileEdit = dialog.findChild<QLineEdit*>(
        QStringLiteral("teacherImportFilePath"));
    auto* browseButton = dialog.findChild<QPushButton*>(
        QStringLiteral("teacherImportBrowseButton"));
    auto* importButton = dialog.findChild<QPushButton*>(
        QStringLiteral("teacherImportAcceptButton"));
    auto* progress = dialog.findChild<QProgressBar*>(
        QStringLiteral("teacherImportProgressBar"));
    QVERIFY(fileEdit);
    QVERIFY(browseButton);
    QVERIFY(importButton);
    QVERIFY(progress);
    QCOMPARE(dialog.width(), 460);
    QCOMPARE(dialog.minimumWidth(), 460);
    QCOMPARE(dialog.maximumWidth(), 460);
    dialog.show();
    QCoreApplication::processEvents();
    QVERIFY(fileEdit->geometry().top() - fileLabel->geometry().bottom() <= 24);
    QVERIFY(validationLabel->geometry().top() - fileEdit->geometry().bottom() <= 24);

    dialog.setFilePath(QStringLiteral("/definitely/missing/teacher-list.xlsx"));
    QCOMPARE(validationLabel->text(), QStringLiteral("Loading workbook..."));
    QVERIFY(!progress->isHidden());
    QVERIFY(!browseButton->isEnabled());
    QVERIFY(!importButton->isEnabled());
    QTRY_COMPARE_WITH_TIMEOUT(
        validationLabel->text(),
        QStringLiteral("Status: Invalid File"),
        5000);
    QVERIFY(progress->isHidden());
    QVERIFY(browseButton->isEnabled());
}

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
    const auto* validationLabel = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportValidationStatus"));
    QVERIFY(validationLabel);
    QTRY_COMPARE_WITH_TIMEOUT(
        validationLabel->text(),
        QStringLiteral("Status: Valid File"),
        10000);
    QVERIFY(importButton->isEnabled());
    const auto* templateLabel = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportTemplateName"));
    const auto* automaticLabel = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportAutomaticCounts"));
    QVERIFY(templateLabel);
    QVERIFY(templateLabel->text().contains(QStringLiteral("Sectioned Teacher Contact List")));
    QVERIFY(templateLabel->text().contains(QStringLiteral("\nVersion: ")));
    QVERIFY(!templateLabel->text().contains(QStringLiteral("Data date")));
    QVERIFY(automaticLabel);
    QCOMPARE(
        automaticLabel->text(),
        QStringLiteral("Automatically Importing\n"
                       "    GS Team Member(s): 9\n"
                       "    Native English Teacher(s): 10"));
    const auto* koreanHeading = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportKoreanHeading"));
    QVERIFY(koreanHeading);
    QCOMPARE(koreanHeading->text(), QStringLiteral("Korean Teachers to Import:"));
    const auto* optionsHost = dialog.findChild<QWidget*>(
        QStringLiteral("teacherImportLevelOptionsHost"));
    QVERIFY(optionsHost);
    auto* allM1 = dialog.findChild<QRadioButton*>(QStringLiteral("teacherImportAll_M1"));
    QVERIFY(allM1);
    QVERIFY(dialog.findChild<QRadioButton*>(QStringLiteral("teacherImportAll_M2")));
    QVERIFY(dialog.findChild<QRadioButton*>(QStringLiteral("teacherImportAll_M3")));
    QVERIFY(dialog.findChild<QRadioButton*>(QStringLiteral("teacherImportAll_H1")));
    auto* allH2 = dialog.findChild<QRadioButton*>(QStringLiteral("teacherImportAll_H2"));
    QVERIFY(allH2);
    QVERIFY(!dialog.findChild<QRadioButton*>(QStringLiteral("teacherImportAll_Elem Only")));
    const auto* levelHeader = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportLevelHeader"));
    const auto* h2Label = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportLevel_H2"));
    QVERIFY(levelHeader);
    QVERIFY(h2Label);
    QVERIFY(levelHeader->indent() > 0);
    QCOMPARE(h2Label->indent(), levelHeader->indent());

    dialog.show();
    QCoreApplication::processEvents();
    QVERIFY(optionsHost->geometry().top() - koreanHeading->geometry().bottom() >= 8);

    auto* selectM1 = dialog.findChild<QRadioButton*>(
        QStringLiteral("teacherImportSelect_M1"));
    auto* noneM1 = dialog.findChild<QRadioButton*>(
        QStringLiteral("teacherImportNone_M1"));
    QVERIFY(selectM1);
    QVERIFY(noneM1);
    const auto* allHeader = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportAllHeader"));
    const auto* selectHeader = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportSelectHeader"));
    const auto* noneHeader = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportNoneHeader"));
    QVERIFY(allHeader);
    QVERIFY(selectHeader);
    QVERIFY(noneHeader);
    QCOMPARE(allM1->geometry().center().x(), allHeader->geometry().center().x());
    QCOMPARE(selectM1->geometry().center().x(), selectHeader->geometry().center().x());
    QCOMPARE(noneM1->geometry().center().x(), noneHeader->geometry().center().x());
    QCOMPARE(selectM1->geometry().center().x() - allM1->geometry().center().x(),
             noneM1->geometry().center().x() - selectM1->geometry().center().x());

    auto* scrollArea = dialog.findChild<QScrollArea*>(
        QStringLiteral("teacherImportCandidateScrollArea"));
    QVERIFY(scrollArea);
    QVERIFY(!scrollArea->isAncestorOf(allH2));

    selectM1->setChecked(true);
    const auto* nameHeader = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportCandidateNameHeader_M1"));
    const auto* roomHeader = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportCandidateRoomHeader_M1"));
    QVERIFY(nameHeader);
    QVERIFY(roomHeader);
    QCOMPARE(nameHeader->text(), QStringLiteral("Name"));
    QCOMPARE(roomHeader->text(), QStringLiteral("Room"));
    const auto* candidateLayout = qobject_cast<QGridLayout*>(
        roomHeader->parentWidget()->layout());
    QVERIFY(candidateLayout);
    QCOMPARE(candidateLayout->verticalSpacing(), 12);
    QCOMPARE(candidateLayout->columnMinimumWidth(1), 16);
    QCOMPARE(candidateLayout->columnMinimumWidth(3), 32);
    QVERIFY(candidateLayout->itemAtPosition(1, 2)->alignment().testFlag(Qt::AlignHCenter));
    QVERIFY(candidateLayout->itemAtPosition(1, 4)->alignment().testFlag(Qt::AlignHCenter));
    QCoreApplication::processEvents();
    QVERIFY(roomHeader->parentWidget()->contentsRect().right()
            - roomHeader->geometry().right() >= 32);
    const auto candidates = scrollArea->findChildren<QCheckBox*>();
    QVERIFY(!candidates.isEmpty());
    for (const QCheckBox* candidate : candidates)
    {
        QVERIFY(candidate->text().isEmpty());
    }
    const auto* firstCandidateName = scrollArea->findChild<QLabel*>(
        QStringLiteral("teacherImportCandidateName_0_0"));
    QVERIFY(firstCandidateName);
    QVERIFY(!firstCandidateName->text().isEmpty());
    allM1->setChecked(true);

    TeacherImportPlan plan = dialog.importPlan();
    QCOMPARE(plan.koreanTeachers.size(), 38);
    QCOMPARE(plan.nativeEnglishTeachers.size(), 10);
    QCOMPARE(plan.gsTeamMembers.size(), 9);

    noneM1->setChecked(true);
    plan = dialog.importPlan();
    QCOMPARE(plan.koreanTeachers.size(), 24);
    QCOMPARE(plan.nativeEnglishTeachers.size(), 10);
    QCOMPARE(plan.gsTeamMembers.size(), 9);

    dialog.setFilePath(QStringLiteral("/definitely/missing/teacher-list.xlsx"));
    QVERIFY(!importButton->isEnabled());
    QVERIFY(templateLabel->text().isEmpty());
    QCOMPARE(validationLabel->text(), QStringLiteral("Loading workbook..."));
    QTRY_COMPARE_WITH_TIMEOUT(
        validationLabel->text(),
        QStringLiteral("Status: Invalid File"),
        5000);
}

QTEST_MAIN(TeacherImportDialogTests)

#include "teacher_import_dialog_tests.moc"
