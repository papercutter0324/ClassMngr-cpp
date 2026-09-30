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

QTEST_MAIN(ClassCoTeacherPageReadParityTests)

#include "class_co_teacher_page_read_parity_tests.moc"
