#include "features/teacher/ui/teacher_import_dialog.h"
#include "data/database/database_schema_manager.h"
#include "data/repositories/teacher_import_repository.h"
#include "fakes/fake_file_dialog_service.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

class TeacherImportDialogTests : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void showsFilePromptAndInvalidStatus();
    void browseUsesTypedFileDialogRequest();
    void suppliedWorkbookBuildsDynamicSelectionUi();
    void checkedInWorkbookProductionDialogPlanAppliesToRepository();
};

namespace
{
QString teacherImportReviewFixturePath()
{
    return QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()).filePath(
        QStringLiteral("fixtures/teacher_import/sectioned_review.xlsx"));
}
}

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

void TeacherImportDialogTests::checkedInWorkbookProductionDialogPlanAppliesToRepository()
{
    TeacherImportDialog dialog;
    dialog.setFilePath(teacherImportReviewFixturePath());

    const auto* validationLabel = dialog.findChild<QLabel*>(
        QStringLiteral("teacherImportValidationStatus"));
    auto* importButton = dialog.findChild<QPushButton*>(
        QStringLiteral("teacherImportAcceptButton"));
    QVERIFY(validationLabel);
    QVERIFY(importButton);
    QTRY_COMPARE_WITH_TIMEOUT(
        validationLabel->text(), QStringLiteral("Status: Valid File"), 10000);

    auto* selectM1 = dialog.findChild<QRadioButton*>(
        QStringLiteral("teacherImportSelect_M1"));
    auto* noneM2 = dialog.findChild<QRadioButton*>(
        QStringLiteral("teacherImportNone_M2"));
    auto* allH1 = dialog.findChild<QRadioButton*>(
        QStringLiteral("teacherImportAll_H1"));
    auto* firstM1 = dialog.findChild<QCheckBox*>(
        QStringLiteral("teacherImportCandidate_0_0"));
    auto* secondM1 = dialog.findChild<QCheckBox*>(
        QStringLiteral("teacherImportCandidate_0_1"));
    QVERIFY(selectM1);
    QVERIFY(noneM2);
    QVERIFY(allH1);
    QVERIFY(firstM1);
    QVERIFY(secondM1);

    selectM1->setChecked(true);
    firstM1->setChecked(true);
    secondM1->setChecked(false);
    noneM2->setChecked(true);
    allH1->setChecked(true);
    QVERIFY(importButton->isEnabled());

    const TeacherImportPlan plan = dialog.importPlan();
    QVERIFY(plan.review.has_value());
    QCOMPARE(plan.review->candidateGroups.size(), 3);
    QCOMPARE(plan.review->groupSelections.size(), 3);
    QCOMPARE(plan.koreanTeachers.size(), 2);
    QCOMPARE(plan.nativeEnglishTeachers.size(), 1);
    QCOMPARE(plan.gsTeamMembers.size(), 1);
    QCOMPARE(plan.review->groupSelections.at(0).mode,
             TeacherImportSelectionMode::Selected);
    QCOMPARE(plan.review->groupSelections.at(0).selectedCandidateIndexes,
             QList<int>{0});
    QCOMPARE(plan.review->groupSelections.at(1).mode,
             TeacherImportSelectionMode::None);
    QCOMPARE(plan.review->groupSelections.at(2).mode,
             TeacherImportSelectionMode::All);

    QCOMPARE(plan.templateId, QStringLiteral("sectioned-contact-list-v1"));
    QCOMPARE(plan.sourceDate, QDate(2026, 9, 1));
    QCOMPARE(plan.koreanTeachers.at(0).teacherKr, QStringLiteral("홍길동"));
    QCOMPARE(plan.koreanTeachers.at(1).teacherKr, QStringLiteral("박민준"));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString connectionName =
        QStringLiteral("teacher-import-dialog-e2e-%1")
            .arg(QUuid::createUuid().toString());
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(
            temporaryDirectory.filePath(QStringLiteral("teacher-import.sqlite")));
        QVERIFY(database.open());
        QVERIFY(DatabaseSchemaManager::ensureSchema(database).has_value());

        QSqlQuery seed(database);
        QVERIFY2(seed.exec(QString::fromUtf8(R"(
            INSERT INTO teachers
                (id, teacher_kr, teacher_en, preferred_romanization,
                 preferred_name, room_number, birthday, phone_number,
                 wifi_name, wifi_password, internet_type, zoom_id,
                 zoom_password, projection_type, notes)
            VALUES
                (7001, '홍길동D', 'Manual English', 'Manual Romanization',
                 'Manual preferred', 'Old room', 'Old birthday',
                 '010-0000-0000', 'Manual WiFi', 'Manual WiFi Password',
                 'LAN', 'manual.zoom', 'manual.zoom.password',
                 'Any', 'Manual notes')
        )")), qPrintable(seed.lastError().text()));
        QVERIFY(seed.exec(R"(
            INSERT INTO native_english_teachers
                (name, position, phone_number, birthday, nationality, email)
            VALUES ('Alex', 'NET', '010-9999-8888', '02-01', 'Canadian', 'alex@example.com')
        )"));

        TeacherImportRepository repository(database);
        const auto imported = repository.importTeachers(plan);
        QVERIFY2(imported.has_value(),
                 imported.has_value() ? "" : qPrintable(imported.error()));
        QCOMPARE(imported->koreanTeachers.created, 1);
        QCOMPARE(imported->koreanTeachers.updated, 1);
        QCOMPARE(imported->koreanTeachers.unchanged, 0);
        QCOMPARE(imported->nativeEnglishTeachers.created, 0);
        QCOMPARE(imported->nativeEnglishTeachers.updated, 1);
        QCOMPARE(imported->nativeEnglishTeachers.unchanged, 0);
        QCOMPARE(imported->gsTeamMembers.created, 1);
        QCOMPARE(imported->gsTeamMembers.updated, 0);
        QCOMPARE(imported->gsTeamMembers.unchanged, 0);

        QSqlQuery persisted(database);
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT id, teacher_kr, room_number, birthday, phone_number, "
            "teacher_en, preferred_romanization, preferred_name, wifi_name, "
            "wifi_password, internet_type, zoom_id, zoom_password, "
            "projection_type, notes "
            "FROM teachers ORDER BY room_number")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 7001);
        QCOMPARE(persisted.value(1).toString(), QStringLiteral("홍길동"));
        QCOMPARE(persisted.value(2).toString(), QStringLiteral("413"));
        QCOMPARE(persisted.value(3).toString(), QStringLiteral("02-29"));
        QCOMPARE(persisted.value(4).toString(), QStringLiteral("010-1111-1111"));
        QCOMPARE(persisted.value(5).toString(), QStringLiteral("Manual English"));
        QCOMPARE(persisted.value(6).toString(), QStringLiteral("Manual Romanization"));
        QCOMPARE(persisted.value(7).toString(), QStringLiteral("Manual preferred"));
        QCOMPARE(persisted.value(8).toString(), QStringLiteral("Manual WiFi"));
        QCOMPARE(persisted.value(9).toString(), QStringLiteral("Manual WiFi Password"));
        QCOMPARE(persisted.value(10).toString(), QStringLiteral("LAN"));
        QCOMPARE(persisted.value(11).toString(), QStringLiteral("manual.zoom"));
        QCOMPARE(persisted.value(12).toString(), QStringLiteral("manual.zoom.password"));
        QCOMPARE(persisted.value(13).toString(), QStringLiteral("Any"));
        QCOMPARE(persisted.value(14).toString(), QStringLiteral("Manual notes"));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(1).toString(), QStringLiteral("박민준"));
        QCOMPARE(persisted.value(2).toString(), QStringLiteral("510"));
        QCOMPARE(persisted.value(3).toString(), QStringLiteral("05-09"));
        QCOMPARE(persisted.value(4).toString(), QStringLiteral("010-4444-4444"));
        QVERIFY(!persisted.next());

        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT COUNT(*) FROM teachers WHERE teacher_kr='홍길동'")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 1);

        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT COUNT(*) FROM teachers WHERE teacher_kr IN ('김하늘', '이서연')")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toInt(), 0);
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT position, phone_number, birthday, nationality, email "
            "FROM native_english_teachers WHERE name='Alex'")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toString(), QStringLiteral("Team Leader"));
        QCOMPARE(persisted.value(1).toString(), QStringLiteral("010-9999-8888"));
        QCOMPARE(persisted.value(2).toString(), QStringLiteral("03-07"));
        QCOMPARE(persisted.value(3).toString(), QStringLiteral("Canadian"));
        QCOMPARE(persisted.value(4).toString(), QStringLiteral("alex@example.com"));
        QVERIFY(persisted.exec(QStringLiteral(
            "SELECT name, position, phone_number, birthday FROM gs_team")));
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toString(), QStringLiteral("Taylor"));
        QCOMPARE(persisted.value(1).toString(), QStringLiteral("M2"));
        QCOMPARE(persisted.value(2).toString(), QStringLiteral("010-5555-5555"));
        QCOMPARE(persisted.value(3).toString(), QStringLiteral("06-10"));
        persisted.prepare(QStringLiteral("SELECT value FROM app_settings WHERE key=?"));
        persisted.addBindValue(QString::fromLatin1(
            TeacherImportRepository::LatestSourceDateSetting));
        QVERIFY(persisted.exec());
        QVERIFY(persisted.next());
        QCOMPARE(persisted.value(0).toString(), QStringLiteral("2026-09-01"));

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(TeacherImportDialogTests)

#include "teacher_import_dialog_tests.moc"
