#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/roster.h"
#include "domain/models/teacher.h"
#include "features/roster/ui/roster_print_dialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
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

    bool saveRoster(
        int classId,
        const QStringList& extraColumns,
        QString* error
        )
    {
        Roster roster;
        roster.columns = Roster::BaseColumns;
        roster.columns.append(extraColumns);
        for (int index = 0; index < roster.columns.size(); ++index)
        {
            roster.columnWidths.append(120);
        }

        const Status saved = services.rosterService()->saveRoster(
            classId, roster);
        if (!saved)
        {
            *error = saved.error();
            return false;
        }
        return true;
    }
};

QListWidget* classListFor(RosterPrintDialog& dialog)
{
    const QList<QListWidget*> lists = dialog.findChildren<QListWidget*>();
    return lists.size() == 1 ? lists.constFirst() : nullptr;
}

QStringList extraColumnLabelsFor(RosterPrintDialog& dialog)
{
    QStringList labels;
    for (const QCheckBox* checkBox : dialog.findChildren<QCheckBox*>())
    {
        labels.append(checkBox->text());
    }
    return labels;
}

QCheckBox* extraColumnCheckBoxFor(
    RosterPrintDialog& dialog,
    const QString& label
    )
{
    for (QCheckBox* checkBox : dialog.findChildren<QCheckBox*>())
    {
        if (checkBox->text() == label)
        {
            return checkBox;
        }
    }
    return nullptr;
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
    void extraInfoColumnsComeFromSelectedClassRostersAndKeepChecksOnRefresh();
    void failedRosterReadProvidesNoExtraInfoColumns();
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

void RosterPrintDialogTests::
extraInfoColumnsComeFromSelectedClassRostersAndKeepChecksOnRefresh()
{
    RosterPrintDialogFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int teacherId = 0;
    QVERIFY2(fixture.createTeacher(&teacherId, &error), qPrintable(error));
    int alphaId = 0;
    int betaId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Alpha"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 teacherId,
                 {},
                 &alphaId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Beta"),
                 QStringLiteral("E5"),
                 QStringLiteral("Apollo"),
                 teacherId,
                 {},
                 &betaId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.saveRoster(
                 alphaId,
                 {QStringLiteral("Alpha Notes"), QStringLiteral("Shared Notes")},
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.saveRoster(
                 betaId,
                 {QStringLiteral("Beta Notes"), QStringLiteral("Shared Notes")},
                 &error
                 ), qPrintable(error));

    RosterPrintDialog dialog(
        &fixture.services,
        alphaId,
        RosterTemplatePrintService::Scope::SelectedClasses,
        RosterPrintDialog::Action::Print
        );
    QListWidget* const classList = classListFor(dialog);
    QVERIFY(classList);
    QCOMPARE(classList->count(), 2);
    QCOMPARE(classList->item(0)->data(Qt::UserRole).toInt(), alphaId);
    QCOMPARE(classList->item(0)->checkState(), Qt::Checked);
    QCOMPARE(classList->item(1)->data(Qt::UserRole).toInt(), betaId);
    QCOMPARE(classList->item(1)->checkState(), Qt::Unchecked);
    classList->item(1)->setCheckState(Qt::Checked);
    QCOMPARE(dialog.selectedClassIds(), QList<int>({alphaId, betaId}));

    auto* const templateCombo = dialog.findChild<QComboBox*>(
        QStringLiteral("templateCombo"));
    QVERIFY(templateCombo);
    const int extraInfoIndex = templateCombo->findData(
        static_cast<int>(
            RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo));
    QVERIFY(extraInfoIndex >= 0);
    templateCombo->setCurrentIndex(extraInfoIndex);

    QCOMPARE(
        extraColumnLabelsFor(dialog),
        (QStringList{
            QStringLiteral("Alpha Notes"),
            QStringLiteral("Shared Notes"),
            QStringLiteral("Beta Notes")
        })
        );
    QCheckBox* sharedNotes = extraColumnCheckBoxFor(
        dialog, QStringLiteral("Shared Notes"));
    QVERIFY(sharedNotes);
    sharedNotes->setChecked(true);
    QCOMPARE(dialog.selectedExtraColumns(),
             QStringList({QStringLiteral("Shared Notes")}));

    classList->item(1)->setCheckState(Qt::Unchecked);
    QCOMPARE(dialog.selectedClassIds(), QList<int>({alphaId}));
    QCOMPARE(
        extraColumnLabelsFor(dialog),
        (QStringList{
            QStringLiteral("Alpha Notes"),
            QStringLiteral("Shared Notes")
        })
        );
    sharedNotes = extraColumnCheckBoxFor(
        dialog, QStringLiteral("Shared Notes"));
    QVERIFY(sharedNotes);
    QVERIFY(sharedNotes->isChecked());
    QCOMPARE(dialog.selectedExtraColumns(),
             QStringList({QStringLiteral("Shared Notes")}));

    classList->item(1)->setCheckState(Qt::Checked);
    QCOMPARE(dialog.selectedClassIds(), QList<int>({alphaId, betaId}));
    QCOMPARE(
        extraColumnLabelsFor(dialog),
        (QStringList{
            QStringLiteral("Alpha Notes"),
            QStringLiteral("Shared Notes"),
            QStringLiteral("Beta Notes")
        })
        );
    sharedNotes = extraColumnCheckBoxFor(
        dialog, QStringLiteral("Shared Notes"));
    QVERIFY(sharedNotes);
    QVERIFY(sharedNotes->isChecked());
    QCOMPARE(dialog.selectedExtraColumns(),
             QStringList({QStringLiteral("Shared Notes")}));
}

void RosterPrintDialogTests::failedRosterReadProvidesNoExtraInfoColumns()
{
    RosterPrintDialogFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int classId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Roster With Unavailable Data"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 -1,
                 {},
                 &classId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.saveRoster(
                 classId,
                 {QStringLiteral("Private Notes")},
                 &error
                 ), qPrintable(error));

    QSqlQuery dropRosterColumns(
        fixture.services.databaseSession()->database());
    QVERIFY2(dropRosterColumns.exec(
                 QStringLiteral("DROP TABLE roster_columns")),
             qPrintable(dropRosterColumns.lastError().text()));

    RosterPrintDialog dialog(
        &fixture.services,
        classId,
        RosterTemplatePrintService::Scope::CurrentClass,
        RosterPrintDialog::Action::Print
        );
    auto* const templateCombo = dialog.findChild<QComboBox*>(
        QStringLiteral("templateCombo"));
    QVERIFY(templateCombo);
    const int extraInfoIndex = templateCombo->findData(
        static_cast<int>(
            RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo));
    QVERIFY(extraInfoIndex >= 0);
    templateCombo->setCurrentIndex(extraInfoIndex);

    QVERIFY(extraColumnLabelsFor(dialog).isEmpty());
    QVERIFY(dialog.selectedExtraColumns().isEmpty());
}

QTEST_MAIN(RosterPrintDialogTests)

#include "roster_print_dialog_tests.moc"
