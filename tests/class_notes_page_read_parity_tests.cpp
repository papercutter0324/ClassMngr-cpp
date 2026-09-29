#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"
#include "features/classes/ui/class_notes_page.h"
#include "ui/shared/pages/page_header.h"

#include <QTemporaryDir>
#include <QTextEdit>
#include <QUuid>
#include <QtTest/QtTest>

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-notes-read-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createClass(ApplicationServices& services)
{
    return services.classService()->create(
        QStringLiteral("Class Notes Read Parity")
        ).value_or(-1);
}

int createTeacher(ApplicationServices& services)
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD\uB2D8");
    teacher.teacherEn = QStringLiteral("Parity Teacher");
    teacher.preferredName = QStringLiteral("Parity Teacher");
    return services.teacherService()->create(teacher).value_or(-1);
}

ClassInfo parityInfo(const int classId, const int teacherId, const bool reloaded)
{
    ClassInfo info;
    info.classId = classId;
    info.teacherId = teacherId;
    info.classGrade = reloaded
        ? QStringLiteral("G2")
        : QStringLiteral("E4");
    info.classLevel = reloaded
        ? QStringLiteral("After Reload")
        : QStringLiteral("Before Reload");
    if (reloaded)
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
    info.notes = reloaded
        ? QStringLiteral("  Reloaded notes  ")
        : QStringLiteral("  Initial notes  ");
    info.timeFillerActivities = reloaded
        ? QStringLiteral("  Reloaded activities  ")
        : QStringLiteral("  Initial activities  ");
    return info;
}

}

class ClassNotesPageReadParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void initialTextSubtitleAndDiscardReloadMatchCommonInput();
};

void ClassNotesPageReadParityTests::
initialTextSubtitleAndDiscardReloadMatchCommonInput()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services);
    QVERIFY(classId > 0);
    const int teacherId = createTeacher(services);
    QVERIFY(teacherId > 0);

    ClassInfo initial = parityInfo(classId, teacherId, false);
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(initial));

    // Use the same leading public constructor on the baseline and current page.
    ClassNotesPage page(&services);
    page.loadClass(Classroom(QStringLiteral("Class Notes Read Parity"), classId));

    auto* header = page.findChild<PageHeader*>();
    QVERIFY(header);
    QCOMPARE(
        header->subtitle(),
        QStringLiteral("E4 Before Reload \u2022 Parity Teacher \u2022 Mon (4:00)")
        );
    const QList<QTextEdit*> editors = page.findChildren<QTextEdit*>();
    QCOMPARE(editors.size(), 2);
    QCOMPARE(editors[0]->toPlainText(), QStringLiteral("Initial notes"));
    QCOMPARE(editors[1]->toPlainText(), QStringLiteral("Initial activities"));

    editors[0]->setPlainText(QStringLiteral("Discard these notes"));
    editors[1]->setPlainText(QStringLiteral("Discard these activities"));
    QVERIFY(page.hasUnsavedChanges());

    const ClassInfo reloaded = parityInfo(classId, teacherId, true);
    QVERIFY(services.databaseSession()->classInfoRepository()->saveClassInfo(
        reloaded
        ));
    page.discardChanges();

    QCOMPARE(
        header->subtitle(),
        QStringLiteral("G2 After Reload \u2022 Parity Teacher \u2022 Wed (5:00)")
        );
    QCOMPARE(editors[0]->toPlainText(), QStringLiteral("Reloaded notes"));
    QCOMPARE(editors[1]->toPlainText(), QStringLiteral("Reloaded activities"));
    QVERIFY(!page.hasUnsavedChanges());
}

QTEST_MAIN(ClassNotesPageReadParityTests)

#include "class_notes_page_read_parity_tests.moc"
