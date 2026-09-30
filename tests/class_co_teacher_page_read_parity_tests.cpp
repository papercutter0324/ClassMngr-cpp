#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"
#include "features/classes/ui/class_co_teacher_page.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/widgets/sections/teacher_info_section.h"

#include <QComboBox>
#include <QLineEdit>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("co-teacher-page-read-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createClass(ApplicationServices& services)
{
    return services.classService()->create(
        QStringLiteral("Co-Teacher Read Parity")
        ).value_or(-1);
}

int createTeacher(ApplicationServices& services, const QString& suffix)
{
    Teacher teacher;
    teacher.teacherEn = QStringLiteral("%1 Teacher").arg(suffix);
    teacher.preferredName = teacher.teacherEn;
    return services.teacherService()->create(teacher).value_or(-1);
}

int createDetailedTeacher(
    ApplicationServices& services,
    const QString& koreanName,
    const QString& englishName,
    const QString& room,
    const QString& network,
    const QString& networkPassword,
    const QString& projection,
    const QString& zoomId,
    const QString& zoomPassword
    )
{
    Teacher teacher;
    teacher.teacherKr = koreanName;
    teacher.teacherEn = englishName;
    teacher.preferredName = englishName;
    teacher.roomNumber = room;
    teacher.internetType = QStringLiteral("LAN");
    teacher.wifiName = network;
    teacher.wifiPassword = networkPassword;
    teacher.projectionType = projection;
    teacher.zoomId = zoomId;
    teacher.zoomPassword = zoomPassword;
    return services.teacherService()->create(teacher).value_or(-1);
}

QStringList visibleTeacherDetails(const TeacherInfoSection* section)
{
    QStringList details;
    for (const QLineEdit* field : section->findChildren<QLineEdit*>())
    {
        details.append(field->text());
    }
    return details;
}

ClassInfo readParityInfo(
    const int classId,
    const int teacherId,
    const bool changed
    )
{
    ClassInfo info;
    info.classId = classId;
    info.teacherId = teacherId;
    info.classGrade = changed
        ? QStringLiteral("E5")
        : QStringLiteral("E4");
    info.classLevel = changed
        ? QStringLiteral("Artemis")
        : QStringLiteral("Theseus");
    if (changed)
    {
        info.classTimes.append({
            QStringLiteral("Wednesday"),
            QStringLiteral("5:00 PM"),
            QStringLiteral("5:50 PM")
        });
    }
    else
    {
        info.classTimes.append({
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:50 PM")
        });
    }
    return info;
}

int chooseTeacher(TeacherInfoSection* section, const int teacherId)
{
    QComboBox* const selector = section->teacherSelector();
    if (!selector)
    {
        return -1;
    }
    const int index = selector->findData(teacherId);
    if (index > 0)
    {
        selector->setCurrentIndex(index);
    }
    return index;
}

}

class ClassCoTeacherPageReadParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void loadDiscardAndSaveRefreshVisibleSelectionAndTitle();
    void loadAndDiscardRefreshVisibleTeacherChoicesAndDetails();
};

void ClassCoTeacherPageReadParityTests::
loadDiscardAndSaveRefreshVisibleSelectionAndTitle()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    const int teacherOne = createTeacher(services, QStringLiteral("First"));
    const int teacherTwo = createTeacher(services, QStringLiteral("Second"));
    const int teacherThree = createTeacher(services, QStringLiteral("Third"));
    QVERIFY(classId > 0);
    QVERIFY(teacherOne > 0);
    QVERIFY(teacherTwo > 0);
    QVERIFY(teacherThree > 0);
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        readParityInfo(classId, teacherOne, false)
        ));

    // The same leading public page constructor is used in current and baseline.
    ClassCoTeacherPage page(&services);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(QStringLiteral("Co-Teacher Read Parity"), classId));
    auto* section = page.findChild<TeacherInfoSection*>();
    auto* header = page.findChild<PageHeader*>();
    QVERIFY(section);
    QVERIFY(header);
    QCOMPARE(section->teacherId(), teacherOne);
    QCOMPARE(
        header->subtitle(),
        QStringLiteral("E4 Theseus \u2022 First Teacher \u2022 Mon (4:00)")
        );

    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        readParityInfo(classId, teacherTwo, true)
        ));
    QVERIFY(chooseTeacher(section, teacherThree) > 0);
    QVERIFY(page.hasUnsavedChanges());
    page.discardChanges();

    QCOMPARE(section->teacherId(), teacherTwo);
    QCOMPARE(
        header->subtitle(),
        QStringLiteral("E5 Artemis \u2022 Second Teacher \u2022 Wed (5:00)")
        );
    QVERIFY(!page.hasUnsavedChanges());

    QVERIFY(chooseTeacher(section, teacherThree) > 0);
    QVERIFY(page.hasUnsavedChanges());
    QVERIFY(page.saveChanges());
    QCOMPARE(section->teacherId(), teacherThree);
    QCOMPARE(
        header->subtitle(),
        QStringLiteral("E5 Artemis \u2022 Third Teacher \u2022 Wed (5:00)")
        );
    QVERIFY(!page.hasUnsavedChanges());
    const auto persisted = services.classService()->classInfo(classId);
    QVERIFY(persisted);
    QCOMPARE(persisted->teacherId, teacherThree);
}

void ClassCoTeacherPageReadParityTests::
loadAndDiscardRefreshVisibleTeacherChoicesAndDetails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    const int teacherOne = createDetailedTeacher(
        services,
        QStringLiteral("\uAE40\uBBFC\uC900"),
        QStringLiteral("Zulu Teacher"),
        QStringLiteral("Room One"),
        QStringLiteral("Network One"),
        QStringLiteral("Password One"),
        QStringLiteral("HDMI"),
        QStringLiteral("Zoom One"),
        QStringLiteral("Zoom Password One")
        );
    const int teacherTwo = createDetailedTeacher(
        services,
        QStringLiteral("\uBC15\uC11C\uC5F0"),
        QStringLiteral("Alpha Teacher"),
        QStringLiteral("Room Two"),
        QStringLiteral("Network Two"),
        QStringLiteral("Password Two"),
        QStringLiteral("Zoom"),
        QStringLiteral("Zoom Two"),
        QStringLiteral("Zoom Password Two")
        );
    QVERIFY(classId > 0);
    QVERIFY(teacherOne > 0);
    QVERIFY(teacherTwo > 0);
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        readParityInfo(classId, teacherOne, false)
        ));

    // Use the same public leading constructor on current and baseline.
    ClassCoTeacherPage page(&services);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(QStringLiteral("Co-Teacher Read Parity"), classId));
    auto* section = page.findChild<TeacherInfoSection*>();
    auto* header = page.findChild<PageHeader*>();
    QVERIFY(section);
    QVERIFY(header);
    auto* englishSelector =
        section->findChild<QComboBox*>(QStringLiteral("teacherEnCombo"));
    QVERIFY(englishSelector);

    QCOMPARE(section->teacherSelector()->count(), 3);
    QCOMPARE(section->teacherSelector()->itemData(0).toInt(), -1);
    QCOMPARE(section->teacherSelector()->itemData(1).toInt(), teacherOne);
    QCOMPARE(section->teacherSelector()->itemData(2).toInt(), teacherTwo);
    QCOMPARE(section->teacherSelector()->currentData().toInt(), teacherOne);
    QCOMPARE(section->teacherSelector()->currentText(),
             QStringLiteral("\uAE40\uBBFC\uC900"));
    QCOMPARE(englishSelector->itemData(0).toInt(), -1);
    QCOMPARE(englishSelector->itemData(1).toInt(), teacherTwo);
    QCOMPARE(englishSelector->itemData(2).toInt(), teacherOne);
    QCOMPARE(englishSelector->currentData().toInt(), teacherOne);
    QCOMPARE(englishSelector->currentText(), QStringLiteral("Zulu Teacher"));
    const QStringList teacherOneDetails{
        QStringLiteral("Room One"),
        QStringLiteral("LAN"),
        QStringLiteral("Network One"),
        QStringLiteral("Password One"),
        QStringLiteral("HDMI"),
        QStringLiteral("Zoom One"),
        QStringLiteral("Zoom Password One")
    };
    QVERIFY(visibleTeacherDetails(section) == teacherOneDetails);
    QCOMPARE(
        header->subtitle(),
        QStringLiteral("E4 Theseus \u2022 Zulu Teacher \u2022 Mon (4:00)")
        );

    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        readParityInfo(classId, teacherTwo, true)
        ));
    page.discardChanges();

    QCOMPARE(section->teacherId(), teacherTwo);
    QCOMPARE(section->teacherSelector()->currentData().toInt(), teacherTwo);
    QCOMPARE(section->teacherSelector()->currentText(),
             QStringLiteral("\uBC15\uC11C\uC5F0"));
    QCOMPARE(section->teacherSelector()->itemData(0).toInt(), -1);
    QCOMPARE(section->teacherSelector()->itemData(1).toInt(), teacherOne);
    QCOMPARE(section->teacherSelector()->itemData(2).toInt(), teacherTwo);
    QCOMPARE(englishSelector->currentData().toInt(), teacherTwo);
    QCOMPARE(englishSelector->currentText(), QStringLiteral("Alpha Teacher"));
    QCOMPARE(englishSelector->itemData(0).toInt(), -1);
    QCOMPARE(englishSelector->itemData(1).toInt(), teacherTwo);
    QCOMPARE(englishSelector->itemData(2).toInt(), teacherOne);
    const QStringList teacherTwoDetails{
        QStringLiteral("Room Two"),
        QStringLiteral("LAN"),
        QStringLiteral("Network Two"),
        QStringLiteral("Password Two"),
        QStringLiteral("Zoom"),
        QStringLiteral("Zoom Two"),
        QStringLiteral("Zoom Password Two")
    };
    QVERIFY(visibleTeacherDetails(section) == teacherTwoDetails);
    QCOMPARE(
        header->subtitle(),
        QStringLiteral("E5 Artemis \u2022 Alpha Teacher \u2022 Wed (5:00)")
        );
    QVERIFY(!page.hasUnsavedChanges());
}

QTEST_MAIN(ClassCoTeacherPageReadParityTests)

#include "class_co_teacher_page_read_parity_tests.moc"
