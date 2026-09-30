#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "domain/models/teacher.h"
#include "features/teacher/ui/teacher_info_page.h"

#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QUuid>
#include <QtTest/QtTest>

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("teacher-info-persistence-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Teacher createTeacher(
    ApplicationServices& services,
    const QString& koreanName,
    const QString& englishName
    )
{
    Teacher teacher;
    teacher.teacherKr = koreanName;
    teacher.teacherEn = englishName;
    teacher.phoneNumber = QStringLiteral("010-2222-3333");
    teacher.roomNumber = QStringLiteral("Room 3");
    teacher.internetType = QStringLiteral("Both");
    teacher.wifiName = QStringLiteral("Original network");
    teacher.wifiPassword = QStringLiteral("Original password");
    teacher.projectionType = QStringLiteral("Zoom");
    teacher.zoomId = QStringLiteral("original.zoom");
    teacher.zoomPassword = QStringLiteral("Original zoom password");
    teacher.notes = QStringLiteral("Original notes");
    const auto saved = services.teacherService()->create(teacher);
    if (saved)
    {
        teacher.id = *saved;
        return teacher;
    }
    return {};
}

}

class TeacherInfoPagePersistenceParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void publicPageValidSaveAndInvalidWriteBehavior();
};

void TeacherInfoPagePersistenceParityTests::
publicPageValidSaveAndInvalidWriteBehavior()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const Teacher original = createTeacher(
        services,
        QStringLiteral("김선생"),
        QStringLiteral("Alex")
        );
    QVERIFY(original.id > 0);

    // Both baseline and current use the same public page constructor and form.
    TeacherInfoPage page(&services);
    page.setSaveMode(SaveMode::Manual);
    page.loadTeacher(original);
    auto* english = page.findChild<QLineEdit*>(
        QStringLiteral("teacherEnEdit"));
    auto* phone = page.findChild<QLineEdit*>(
        QStringLiteral("phoneNumberEdit"));
    auto* notes = page.findChild<QTextEdit*>(
        QStringLiteral("teacherNotesEdit"));
    QVERIFY(english);
    QVERIFY(phone);
    QVERIFY(notes);

    QSignalSpy saved(&page, &TeacherInfoPage::teacherSaved);
    english->setText(QStringLiteral("Alexandra"));
    phone->setText(QStringLiteral("01012345678"));
    notes->setPlainText(QStringLiteral("  Updated profile notes  "));
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(page.saveChanges());
    QVERIFY(!page.hasUnsavedChanges());
    QCOMPARE(saved.count(), 1);
    QCOMPARE(saved.constFirst().constFirst().toInt(), original.id);
    QCOMPARE(page.teacher().teacherEn, QStringLiteral("Alexandra"));
    QCOMPARE(page.teacher().phoneNumber, QStringLiteral("010-1234-5678"));
    QCOMPARE(page.teacher().notes, QStringLiteral("Updated profile notes"));

    const Result<Teacher> persisted =
        services.teacherService()->teacher(original.id);
    QVERIFY(persisted);
    QCOMPARE(persisted->teacherEn, QStringLiteral("Alexandra"));
    QCOMPARE(persisted->phoneNumber, QStringLiteral("010-1234-5678"));
    QCOMPARE(persisted->notes, QStringLiteral("Updated profile notes"));
    QCOMPARE(persisted->roomNumber, QStringLiteral("Room 3"));
    QCOMPARE(persisted->internetType, QStringLiteral("Both"));
    QCOMPARE(persisted->wifiName, QStringLiteral("Original network"));
    QCOMPARE(persisted->wifiPassword, QStringLiteral("Original password"));
    QCOMPARE(persisted->projectionType, QStringLiteral("Zoom"));
    QCOMPARE(persisted->zoomId, QStringLiteral("original.zoom"));
    QCOMPARE(persisted->zoomPassword, QStringLiteral("Original zoom password"));

    const Teacher beforeInvalid = createTeacher(
        services,
        QStringLiteral("김불변"),
        QStringLiteral("Unchanged Teacher")
        );
    QVERIFY(beforeInvalid.id > 0);

    TeacherInfoPage invalidPage(&services);
    invalidPage.setSaveMode(SaveMode::Manual);
    invalidPage.loadTeacher(beforeInvalid);
    auto* invalidKorean = invalidPage.findChild<QLineEdit*>(
        QStringLiteral("teacherKrEdit"));
    auto* invalidEnglish = invalidPage.findChild<QLineEdit*>(
        QStringLiteral("teacherEnEdit"));
    auto* englishMessage = invalidPage.findChild<QLabel*>(
        QStringLiteral("teacherEnValidationMessage"));
    QVERIFY(invalidKorean);
    QVERIFY(invalidEnglish);
    QVERIFY(englishMessage);

    QSignalSpy invalidSaved(&invalidPage, &TeacherInfoPage::teacherSaved);
    invalidKorean->clear();
    invalidEnglish->clear();
    QVERIFY(invalidPage.hasUnsavedChanges());
    QVERIFY(!invalidPage.saveChanges());
    QVERIFY(invalidPage.hasUnsavedChanges());
    QCOMPARE(invalidSaved.count(), 0);
    QCOMPARE(
        invalidEnglish->property("formValidationState").toString(),
        QStringLiteral("error")
        );
    QCOMPARE(englishMessage->text(), QStringLiteral("This field is required."));

    const Result<Teacher> unchanged =
        services.teacherService()->teacher(beforeInvalid.id);
    QVERIFY(unchanged);
    QCOMPARE(unchanged->teacherKr, QStringLiteral("김불변"));
    QCOMPARE(unchanged->teacherEn, QStringLiteral("Unchanged Teacher"));
    QCOMPARE(unchanged->notes, QStringLiteral("Original notes"));
    QCOMPARE(invalidPage.teacher().teacherKr, QStringLiteral("김불변"));
    QCOMPARE(invalidPage.teacher().teacherEn, QStringLiteral("Unchanged Teacher"));
}

QTEST_MAIN(TeacherInfoPagePersistenceParityTests)

#include "teacher_info_page_persistence_parity_tests.moc"
