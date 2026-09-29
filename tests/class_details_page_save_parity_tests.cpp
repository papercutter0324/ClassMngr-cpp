#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"
#include "features/classes/ui/class_details_page.h"

#include <QComboBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-details-save-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createClass(ApplicationServices& services, const QString& name)
{
    return services.classService()->create(name).value_or(-1);
}

int createTeacher(ApplicationServices& services)
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD\uB2D8");
    teacher.teacherEn = QStringLiteral("Parity Teacher");
    teacher.preferredRomanization = QStringLiteral("Parity Teacher");
    teacher.preferredName = QStringLiteral("Parity Teacher");
    teacher.roomNumber = QStringLiteral("Room 504");
    teacher.wifiName = QStringLiteral("Parity WiFi");
    teacher.wifiPassword = QStringLiteral("parity-wifi-password");
    teacher.internetType = QStringLiteral("Both");
    teacher.zoomId = QStringLiteral("123 456 7890");
    teacher.zoomPassword = QStringLiteral("parity-zoom-password");
    teacher.projectionType = QStringLiteral("Any");
    teacher.notes = QStringLiteral("Teacher profile remains untouched");

    return services.teacherService()->create(teacher).value_or(-1);
}

void chooseNewDetails(ClassDetailsPage& page)
{
    auto* grade = page.findChild<QComboBox*>(QStringLiteral("classGradeCombo"));
    auto* level = page.findChild<QComboBox*>(QStringLiteral("classLevelCombo"));
    auto* reading = page.findChild<QComboBox*>(
        QStringLiteral("classReadingBookCombo")
        );
    auto* essay = page.findChild<QComboBox*>(
        QStringLiteral("classEssayBookCombo")
        );
    Q_ASSERT(grade && level && reading && essay);

    grade->setCurrentText(QStringLiteral("E4"));
    level->setCurrentText(QStringLiteral("Theseus"));
    reading->setCurrentText(QStringLiteral("Reading Explorer 1"));
    essay->setCurrentText(QStringLiteral("4A"));
}

}

class ClassDetailsPageSaveParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void successfulUiSaveMatchesSeededCommonInputState();
};

void ClassDetailsPageSaveParityTests::successfulUiSaveMatchesSeededCommonInputState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int teacherId = createTeacher(services);
    const int classId = createClass(
        services,
        QStringLiteral("Class Details Save Parity")
        );
    QVERIFY(teacherId > 0);
    QVERIFY(classId > 0);

    auto seededInfoResult = services.classService()->classInfo(classId);
    QVERIFY(seededInfoResult);
    ClassInfo seededInfo = *seededInfoResult;
    seededInfo.teacherId = teacherId;
    seededInfo.classGrade = QStringLiteral("E5");
    seededInfo.classLevel = QStringLiteral("Artemis");
    seededInfo.readingBook = QStringLiteral("Reading Explorer 2");
    seededInfo.essayBook = QStringLiteral("5A");
    seededInfo.classColor = QStringLiteral("#AABBCC");
    seededInfo.fontColor = QStringLiteral("#112233");
    seededInfo.notes = QStringLiteral("Keep these notes exactly.\nSecond line.");
    seededInfo.timeFillerActivities = QStringLiteral(
        "Quiet reading\nWord games"
        );
    seededInfo.classTimes = {
        {
            QStringLiteral("Friday"),
            QStringLiteral("3:00 PM"),
            QStringLiteral("3:55 PM")
        },
        {
            QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"),
            QStringLiteral("9:55 AM")
        }
    };
    seededInfo.intensiveTimes = {
        {
            QStringLiteral("Wednesday"),
            QStringLiteral("1:00 PM"),
            QStringLiteral("1:55 PM")
        },
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("10:00 AM"),
            QStringLiteral("10:55 AM")
        }
    };
    QVERIFY(services.classService()->saveClassInfo(seededInfo));

    ClassDetailsPage page(&services, false);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(
        Classroom(QStringLiteral("Class Details Save Parity"), classId)
        );

    QSignalSpy savedSpy(&page, &ClassDetailsPage::classInfoSaved);
    QVERIFY(savedSpy.isValid());
    chooseNewDetails(page);

    QVERIFY(page.hasUnsavedChanges());
    auto* saveButton = page.findChild<QPushButton*>(
        QStringLiteral("classInfoSaveButton")
        );
    QVERIFY(saveButton);
    QVERIFY(saveButton->isEnabled());
    saveButton->click();

    QCOMPARE(savedSpy.size(), 1);
    QCOMPARE(savedSpy.at(0).at(0).toInt(), classId);
    QVERIFY(!page.hasUnsavedChanges());

    const auto persistedResult = services.classService()->classInfo(classId);
    QVERIFY(persistedResult);
    const ClassInfo& persisted = *persistedResult;

    QCOMPARE(persisted.classId, classId);
    QCOMPARE(persisted.teacherId, teacherId);
    QCOMPARE(persisted.teacherKr, QStringLiteral("\uAE40\uC120\uC0DD\uB2D8"));
    QCOMPARE(persisted.teacherEn, QStringLiteral("Parity Teacher"));
    QCOMPARE(
        persisted.teacherPreferredName,
        QStringLiteral("Parity Teacher")
        );
    QCOMPARE(persisted.roomNumber, QStringLiteral("Room 504"));
    QCOMPARE(persisted.wifiName, QStringLiteral("Parity WiFi"));
    QCOMPARE(persisted.wifiPassword, QStringLiteral("parity-wifi-password"));
    QCOMPARE(persisted.internetType, QStringLiteral("Both"));
    QCOMPARE(persisted.zoomId, QStringLiteral("123 456 7890"));
    QCOMPARE(persisted.zoomPassword, QStringLiteral("parity-zoom-password"));
    QCOMPARE(persisted.projectionType, QStringLiteral("Any"));
    QCOMPARE(persisted.classGrade, QStringLiteral("E4"));
    QCOMPARE(persisted.classLevel, QStringLiteral("Theseus"));
    QCOMPARE(persisted.readingBook, QStringLiteral("Reading Explorer 1"));
    QCOMPARE(persisted.essayBook, QStringLiteral("4A"));
    QCOMPARE(persisted.classColor, QStringLiteral("#AABBCC"));
    QCOMPARE(persisted.fontColor, QStringLiteral("#112233"));
    QCOMPARE(
        persisted.notes,
        QStringLiteral("Keep these notes exactly.\nSecond line.")
        );
    QCOMPARE(
        persisted.timeFillerActivities,
        QStringLiteral("Quiet reading\nWord games")
        );

    QCOMPARE(persisted.classTimes.size(), 2);
    QCOMPARE(persisted.classTimes.at(0).day, QStringLiteral("Friday"));
    QCOMPARE(persisted.classTimes.at(0).startTime, QStringLiteral("3:00 PM"));
    QCOMPARE(persisted.classTimes.at(0).endTime, QStringLiteral("3:55 PM"));
    QCOMPARE(persisted.classTimes.at(1).day, QStringLiteral("Monday"));
    QCOMPARE(persisted.classTimes.at(1).startTime, QStringLiteral("9:00 AM"));
    QCOMPARE(persisted.classTimes.at(1).endTime, QStringLiteral("9:55 AM"));

    QCOMPARE(persisted.intensiveTimes.size(), 2);
    QCOMPARE(persisted.intensiveTimes.at(0).day, QStringLiteral("Wednesday"));
    QCOMPARE(
        persisted.intensiveTimes.at(0).startTime,
        QStringLiteral("1:00 PM")
        );
    QCOMPARE(
        persisted.intensiveTimes.at(0).endTime,
        QStringLiteral("1:55 PM")
        );
    QCOMPARE(persisted.intensiveTimes.at(1).day, QStringLiteral("Tuesday"));
    QCOMPARE(
        persisted.intensiveTimes.at(1).startTime,
        QStringLiteral("10:00 AM")
        );
    QCOMPARE(
        persisted.intensiveTimes.at(1).endTime,
        QStringLiteral("10:55 AM")
        );
}

QTEST_MAIN(ClassDetailsPageSaveParityTests)

#include "class_details_page_save_parity_tests.moc"
