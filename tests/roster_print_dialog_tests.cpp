#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"
#include "features/roster/ui/roster_print_dialog.h"

#include <QApplication>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <QUuid>

namespace
{

QString displayName(
    const QString& grade,
    const QString& level,
    const QString& teacher,
    const QString& schedule = QString()
    )
{
    QString result = grade + QLatin1Char(' ') + level
        + QLatin1Char(' ') + QChar(0x2022) + QLatin1Char(' ') + teacher;
    if (!schedule.isEmpty())
    {
        result += QLatin1Char(' ') + QChar(0x2022) + QLatin1Char(' ')
            + schedule;
    }
    return result;
}

QString defaultClassDisplayName()
{
    return QStringLiteral("Unknown Class ") + QChar(0x2022)
        + QStringLiteral(" No Teacher");
}

class RosterPrintDialogFixture final
{
public:
    QTemporaryDir directory;
    ApplicationServices services;

    bool initialize(QString* error)
    {
        if (!directory.isValid())
        {
            *error = QStringLiteral("Temporary directory is invalid.");
            return false;
        }

        const QString path = directory.filePath(
            QStringLiteral("roster-print-dialog-%1.tps").arg(
                QUuid::createUuid().toString(QUuid::WithoutBraces)
                )
            );
        const Status opened = services.openDatabase(path);
        if (!opened)
        {
            *error = opened.error();
            return false;
        }

        return true;
    }

    bool createTeacher(int* teacherId, QString* error)
    {
        Teacher teacher;
        teacher.teacherEn = QStringLiteral("Morgan");
        teacher.preferredName = QStringLiteral("Morgan");
        const auto created = services.teacherService()->save(teacher);
        if (!created)
        {
            *error = created.error();
            return false;
        }
        *teacherId = *created;
        return true;
    }

    bool createClass(
        const QString& name,
        const QString& grade,
        const QString& level,
        int teacherId,
        const QList<ClassTime>& classTimes,
        int* classId,
        QString* error
        )
    {
        const auto created = services.classService()->create(name);
        if (!created)
        {
            *error = created.error();
            return false;
        }

        ClassInfo info;
        info.classId = *created;
        info.teacherId = teacherId;
        info.classGrade = grade;
        info.classLevel = level;
        info.classTimes = classTimes;
        const Status saved = services.classService()->saveClassInfo(info);
        if (!saved)
        {
            *error = saved.error();
            return false;
        }

        *classId = *created;
        return true;
    }


};

QListWidget* classListFor(RosterPrintDialog& dialog)
{
    const QList<QListWidget*> lists = dialog.findChildren<QListWidget*>();
    return lists.size() == 1 ? lists.constFirst() : nullptr;
}


}

class RosterPrintDialogTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void selectedClassListPreservesOrderIdsAndCheckState();
    void teacherReadFailureRetainsClassFieldsAndDefaultTeacherFormatting();
    void classFieldsReadFailureUsesDefaultClassFormatting();
};

void RosterPrintDialogTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void RosterPrintDialogTests::selectedClassListPreservesOrderIdsAndCheckState()
{
    RosterPrintDialogFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int teacherId = 0;
    QVERIFY2(fixture.createTeacher(&teacherId, &error), qPrintable(error));
    int zuluId = 0;
    int alphaId = 0;
    int mikeId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Zulu"),
                 QStringLiteral("E5"),
                 QStringLiteral("Apollo"),
                 -1,
                 {},
                 &zuluId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Alpha"),
                 QStringLiteral("E6"),
                 QStringLiteral("Helios"),
                 -1,
                 {},
                 &alphaId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Mike"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 teacherId,
                 {{
                     QStringLiteral("Monday"),
                     QStringLiteral("4:00 PM"),
                     QStringLiteral("4:50 PM")
                 }},
                 &mikeId,
                 &error
                 ), qPrintable(error));

    RosterPrintDialog dialog(
        &fixture.services,
        mikeId,
        RosterTemplatePrintService::Scope::SelectedClasses,
        RosterPrintDialog::Action::Print
        );
    QListWidget* const list = classListFor(dialog);
    QVERIFY(list);
    QCOMPARE(list->count(), 3);

    const QList<int> expectedIds{alphaId, mikeId, zuluId};
    const QStringList expectedLabels{
        displayName(
            QStringLiteral("E6"),
            QStringLiteral("Helios"),
            QStringLiteral("No Teacher")
            ),
        displayName(
            QStringLiteral("E4"),
            QStringLiteral("Theseus"),
            QStringLiteral("Morgan"),
            QStringLiteral("Mon (4:00)")
            ),
        displayName(
            QStringLiteral("E5"),
            QStringLiteral("Apollo"),
            QStringLiteral("No Teacher")
            )
    };
    for (int index = 0; index < list->count(); ++index)
    {
        const QListWidgetItem* const item = list->item(index);
        QVERIFY(item);
        QCOMPARE(item->data(Qt::UserRole).toInt(), expectedIds.at(index));
        QCOMPARE(item->text(), expectedLabels.at(index));
        QCOMPARE(
            item->checkState(),
            expectedIds.at(index) == mikeId ? Qt::Checked : Qt::Unchecked
            );
    }
    QCOMPARE(dialog.selectedClassIds(), QList<int>({mikeId}));
}

void RosterPrintDialogTests::
teacherReadFailureRetainsClassFieldsAndDefaultTeacherFormatting()
{
    RosterPrintDialogFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int teacherId = 0;
    QVERIFY2(fixture.createTeacher(&teacherId, &error), qPrintable(error));
    int currentId = 0;
    int otherId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Current"),
                 QStringLiteral("E4"),
                 QStringLiteral("Hercules"),
                 teacherId,
                 {},
                 &currentId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Other"),
                 QStringLiteral("E5"),
                 QStringLiteral("Apollo"),
                 teacherId,
                 {},
                 &otherId,
                 &error
                 ), qPrintable(error));

    QSqlQuery disableForeignKeys(fixture.services.databaseSession()->database());
    QVERIFY2(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")),
             qPrintable(disableForeignKeys.lastError().text()));
    QSqlQuery dropTeachers(fixture.services.databaseSession()->database());
    QVERIFY2(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")),
             qPrintable(dropTeachers.lastError().text()));

    RosterPrintDialog dialog(
        &fixture.services,
        currentId,
        RosterTemplatePrintService::Scope::SelectedClasses,
        RosterPrintDialog::Action::Print
        );
    QListWidget* const list = classListFor(dialog);
    QVERIFY(list);
    QCOMPARE(list->count(), 2);
    QCOMPARE(list->item(0)->data(Qt::UserRole).toInt(), currentId);
    QCOMPARE(list->item(0)->text(), displayName(
        QStringLiteral("E4"),
        QStringLiteral("Hercules"),
        QStringLiteral("No Teacher")
    ));
    QCOMPARE(list->item(0)->checkState(), Qt::Checked);
    QCOMPARE(list->item(1)->data(Qt::UserRole).toInt(), otherId);
    QCOMPARE(list->item(1)->text(), displayName(
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QStringLiteral("No Teacher")
    ));
    QCOMPARE(list->item(1)->checkState(), Qt::Unchecked);
}

void RosterPrintDialogTests::classFieldsReadFailureUsesDefaultClassFormatting()
{
    RosterPrintDialogFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int currentId = 0;
    int otherId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Current"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 -1,
                 {},
                 &currentId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Other"),
                 QStringLiteral("E5"),
                 QStringLiteral("Apollo"),
                 -1,
                 {},
                 &otherId,
                 &error
                 ), qPrintable(error));

    QSqlQuery dropTimes(fixture.services.databaseSession()->database());
    QVERIFY2(dropTimes.exec(QStringLiteral("DROP TABLE class_times")),
             qPrintable(dropTimes.lastError().text()));

    RosterPrintDialog dialog(
        &fixture.services,
        currentId,
        RosterTemplatePrintService::Scope::SelectedClasses,
        RosterPrintDialog::Action::Print
        );
    QListWidget* const list = classListFor(dialog);
    QVERIFY(list);
    QCOMPARE(list->count(), 2);
    QCOMPARE(list->item(0)->data(Qt::UserRole).toInt(), currentId);
    QCOMPARE(list->item(0)->text(), defaultClassDisplayName());
    QCOMPARE(list->item(0)->checkState(), Qt::Checked);
    QCOMPARE(list->item(1)->data(Qt::UserRole).toInt(), otherId);
    QCOMPARE(list->item(1)->text(), defaultClassDisplayName());
    QCOMPARE(list->item(1)->checkState(), Qt::Unchecked);
}

QTEST_MAIN(RosterPrintDialogTests)

#include "roster_print_dialog_tests.moc"
