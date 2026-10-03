#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/roster.h"
#include "domain/models/testing_class.h"
#include "domain/models/teacher.h"
#include "features/roster/ui/roster_print_dialog.h"
#include "fakes/fake_user_prompt_service.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPointer>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
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

bool appendRawRosterColumns(
    ApplicationServices& services,
    const int classId,
    const QStringList& columns,
    QString* error
    )
{
    QSqlQuery insert(services.databaseSession()->database());
    if (!insert.prepare(QStringLiteral(
            "INSERT INTO roster_columns (class_id, name, position, width) "
            "VALUES (?, ?, ?, ?)"
            )))
    {
        *error = insert.lastError().text();
        return false;
    }

    int position = Roster::BaseColumns.size();
    for (const QString& column : columns)
    {
        insert.bindValue(0, classId);
        insert.bindValue(1, column);
        insert.bindValue(2, position);
        insert.bindValue(3, 120);
        if (!insert.exec())
        {
            *error = insert.lastError().text();
            return false;
        }
        ++position;
    }
    return true;
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
    void classListReadFailureShowsWarningAndLeavesListEnabledAndEmpty();
    void teacherReadFailureRetainsClassFieldsAndDefaultTeacherFormatting();
    void classFieldsReadFailureUsesDefaultClassFormatting();
    void currentClassOnlyUsesTestingClassRecord();
    void currentClassOnlyDetailsReadFailureIsSilentAndLeavesListEmpty();
    void extraInfoColumnsComeFromSelectedClassRostersAndKeepChecksOnRefresh();
    void extraInfoColumnUnionKeepsScopeOrderAndFiltersNames();
    void emptyRosterDoesNotDiscardOtherClassesExtraColumns();
    void failedRosterReadProvidesNoExtraInfoColumns();
    void classListReadFailurePreservesRenderedExtraInfoControls();
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

    ClassInfoRepository* const classRepository =
        fixture.services.databaseSession()->classInfoRepository();
    TeacherRepository* const teacherRepository =
        fixture.services.databaseSession()->teacherRepository();
    QVERIFY(classRepository);
    QVERIFY(teacherRepository);
    const ClassSubtitleBatchReadMetrics classMetricsBefore =
        classRepository->classSubtitleBatchReadMetrics();
    const TeacherDisplayNameBatchReadMetrics teacherMetricsBefore =
        teacherRepository->teacherDisplayNameBatchReadMetrics();

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

    const ClassSubtitleBatchReadMetrics classMetricsAfter =
        classRepository->classSubtitleBatchReadMetrics();
    const TeacherDisplayNameBatchReadMetrics teacherMetricsAfter =
        teacherRepository->teacherDisplayNameBatchReadMetrics();
    QCOMPARE(classMetricsAfter.callCount - classMetricsBefore.callCount, 1);
    QCOMPARE(classMetricsAfter.requestedClassCount
                 - classMetricsBefore.requestedClassCount,
             expectedIds.size());
    QCOMPARE(classMetricsAfter.metadataStatementCount
                 - classMetricsBefore.metadataStatementCount,
             1);
    QCOMPARE(classMetricsAfter.regularScheduleStatementCount
                 - classMetricsBefore.regularScheduleStatementCount,
             1);
    QCOMPARE(teacherMetricsAfter.callCount - teacherMetricsBefore.callCount, 1);
    QCOMPARE(teacherMetricsAfter.statementCount
                 - teacherMetricsBefore.statementCount,
             1);
}

void RosterPrintDialogTests::
classListReadFailureShowsWarningAndLeavesListEnabledAndEmpty()
{
    RosterPrintDialogFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int currentId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Current"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 -1,
                 {},
                 &currentId,
                 &error
                 ), qPrintable(error));

    QSqlQuery dropClasses(fixture.services.databaseSession()->database());
    QVERIFY2(dropClasses.exec(QStringLiteral("DROP TABLE classes")),
             qPrintable(dropClasses.lastError().text()));

    QString warningTitle;
    QString warningText;
    QString warningDetails;
    bool warningCaptured = false;
    QTimer::singleShot(
        0,
        [&]()
        {
            auto* warning = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget());
            if (!warning)
            {
                return;
            }

            warningTitle = warning->windowTitle();
            warningText = warning->text();
            warningDetails = warning->detailedText();
            warningCaptured = true;
            warning->accept();
        }
        );

    RosterPrintDialog dialog(
        &fixture.services,
        currentId,
        RosterTemplatePrintService::Scope::SelectedClasses,
        RosterPrintDialog::Action::Print
        );

    QVERIFY(warningCaptured);
    QCOMPARE(warningTitle, QStringLiteral("Print Rosters"));
    QCOMPARE(warningText, QStringLiteral("Classes could not be loaded."));
    QVERIFY2(!warningDetails.isEmpty(),
             "The class-list error details were not shown.");
    QVERIFY(warningDetails.contains(QStringLiteral("classes"),
                                    Qt::CaseInsensitive));

    QListWidget* const list = classListFor(dialog);
    QVERIFY(list);
    QCOMPARE(list->count(), 0);
    QVERIFY(list->isEnabled());
    QCOMPARE(dialog.selectedClassIds(), QList<int>());
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

void RosterPrintDialogTests::currentClassOnlyUsesTestingClassRecord()
{
    RosterPrintDialogFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int regularClassId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Regular Class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 -1,
                 {},
                 &regularClassId,
                 &error
                 ), qPrintable(error));
    QVERIFY(regularClassId > 0);

    TestingClass testingClass;
    testingClass.name = QStringLiteral("Friday Testing Class");
    testingClass.grade = QStringLiteral("M1");
    testingClass.level = QStringLiteral("Major");
    testingClass.room = QStringLiteral("401");
    const auto created = fixture.services.scheduleService()->createTestingClass(
        testingClass
        );
    QVERIFY(created);

    ClassInfoRepository* const classRepository =
        fixture.services.databaseSession()->classInfoRepository();
    TeacherRepository* const teacherRepository =
        fixture.services.databaseSession()->teacherRepository();
    QVERIFY(classRepository);
    QVERIFY(teacherRepository);
    const ClassSubtitleBatchReadMetrics classMetricsBefore =
        classRepository->classSubtitleBatchReadMetrics();
    const TeacherDisplayNameBatchReadMetrics teacherMetricsBefore =
        teacherRepository->teacherDisplayNameBatchReadMetrics();

    RosterPrintDialog dialog(
        &fixture.services,
        *created,
        RosterTemplatePrintService::Scope::CurrentClass,
        RosterPrintDialog::Action::Print,
        nullptr,
        true
        );
    QListWidget* const list = classListFor(dialog);
    QVERIFY(list);
    QCOMPARE(list->count(), 1);
    const QListWidgetItem* const item = list->item(0);
    QVERIFY(item);
    QCOMPARE(item->data(Qt::UserRole).toInt(), *created);
    QCOMPARE(item->checkState(), Qt::Checked);
    QVERIFY(item->text().contains(testingClass.name));
    QVERIFY(item->text().contains(testingClass.grade));
    QVERIFY(item->text().contains(testingClass.level));
    QCOMPARE(dialog.selectedClassIds(), QList<int>({*created}));

    const ClassSubtitleBatchReadMetrics classMetricsAfter =
        classRepository->classSubtitleBatchReadMetrics();
    const TeacherDisplayNameBatchReadMetrics teacherMetricsAfter =
        teacherRepository->teacherDisplayNameBatchReadMetrics();
    QCOMPARE(classMetricsAfter.callCount, classMetricsBefore.callCount);
    QCOMPARE(classMetricsAfter.requestedClassCount,
             classMetricsBefore.requestedClassCount);
    QCOMPARE(classMetricsAfter.metadataStatementCount,
             classMetricsBefore.metadataStatementCount);
    QCOMPARE(classMetricsAfter.regularScheduleStatementCount,
             classMetricsBefore.regularScheduleStatementCount);
    QCOMPARE(teacherMetricsAfter.callCount, teacherMetricsBefore.callCount);
    QCOMPARE(teacherMetricsAfter.statementCount,
             teacherMetricsBefore.statementCount);
}

void RosterPrintDialogTests::
currentClassOnlyDetailsReadFailureIsSilentAndLeavesListEmpty()
{
    RosterPrintDialogFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int regularClassId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Regular Class"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 -1,
                 {},
                 &regularClassId,
                 &error
                 ), qPrintable(error));
    QVERIFY(regularClassId > 0);

    TestingClass testingClass;
    testingClass.name = QStringLiteral("Testing Class With Missing Details");
    testingClass.grade = QStringLiteral("M1");
    testingClass.level = QStringLiteral("Major");
    testingClass.room = QStringLiteral("401");
    const auto created = fixture.services.scheduleService()->createTestingClass(
        testingClass
        );
    QVERIFY(created);

    QSqlQuery removeTestingDetails(
        fixture.services.databaseSession()->database()
        );
    removeTestingDetails.prepare(QStringLiteral(
        "DELETE FROM testing_classes WHERE class_id=?"
        ));
    removeTestingDetails.addBindValue(*created);
    QVERIFY2(removeTestingDetails.exec(),
             qPrintable(removeTestingDetails.lastError().text()));
    QCOMPARE(removeTestingDetails.numRowsAffected(), 1);

    QVERIFY(fixture.services.databaseSession()->isOpen());
    QVERIFY(fixture.services.classService()->isAvailable());
    QVERIFY(fixture.services.teacherService()->isAvailable());
    QVERIFY(fixture.services.scheduleService()->isAvailable());

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    RosterPrintDialog dialog(
        &fixture.services,
        *created,
        RosterTemplatePrintService::Scope::CurrentClass,
        RosterPrintDialog::Action::Print,
        nullptr,
        true
        );

    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);

    QListWidget* const list = classListFor(dialog);
    QVERIFY(list);
    QCOMPARE(list->count(), 0);
    QVERIFY(dialog.selectedClassIds().isEmpty());
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

void RosterPrintDialogTests::
extraInfoColumnUnionKeepsScopeOrderAndFiltersNames()
{
    RosterPrintDialogFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int alphaId = 0;
    int betaId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Alpha"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 -1,
                 {},
                 &alphaId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Beta"),
                 QStringLiteral("E5"),
                 QStringLiteral("Apollo"),
                 -1,
                 {},
                 &betaId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.saveRoster(alphaId, {}, &error), qPrintable(error));
    QVERIFY2(fixture.saveRoster(betaId, {}, &error), qPrintable(error));
    QVERIFY2(appendRawRosterColumns(
                 fixture.services,
                 alphaId,
                 {
                     QStringLiteral("  Alpha Notes  "),
                     QStringLiteral(" SHARED "),
                     QStringLiteral("shared"),
                     QStringLiteral(" English "),
                     QStringLiteral("Autumn"),
                     QStringLiteral("   "),
                     QStringLiteral("alpha tail")
                 },
                 &error
                 ), qPrintable(error));
    QVERIFY2(appendRawRosterColumns(
                 fixture.services,
                 betaId,
                 {
                     QStringLiteral("Beta Notes"),
                     QStringLiteral("shared"),
                     QStringLiteral("BETA notes"),
                     QStringLiteral("Second"),
                     QStringLiteral(" Korean "),
                     QStringLiteral("Autumn"),
                     QStringLiteral("Beta Tail")
                 },
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
    QCOMPARE(classList->item(1)->data(Qt::UserRole).toInt(), betaId);

    auto* const templateCombo = dialog.findChild<QComboBox*>(
        QStringLiteral("templateCombo"));
    QVERIFY(templateCombo);
    const int extraInfoIndex = templateCombo->findData(
        static_cast<int>(
            RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo));
    QVERIFY(extraInfoIndex >= 0);
    templateCombo->setCurrentIndex(extraInfoIndex);
    classList->item(1)->setCheckState(Qt::Checked);

    const QStringList expected{
        QStringLiteral("Alpha Notes"),
        QStringLiteral("SHARED"),
        QStringLiteral("alpha tail"),
        QStringLiteral("Beta Notes"),
        QStringLiteral("Second"),
        QStringLiteral("Beta Tail")
    };
    QCOMPARE(extraColumnLabelsFor(dialog), expected);

    QCheckBox* shared = extraColumnCheckBoxFor(
        dialog,
        QStringLiteral("SHARED")
        );
    QVERIFY(shared);
    shared->setChecked(true);
    QCOMPARE(dialog.selectedExtraColumns(), QStringList({QStringLiteral("SHARED")}));

    classList->item(1)->setCheckState(Qt::Unchecked);
    QCOMPARE(
        extraColumnLabelsFor(dialog),
        (QStringList{
            QStringLiteral("Alpha Notes"),
            QStringLiteral("SHARED"),
            QStringLiteral("alpha tail")
        })
        );
    shared = extraColumnCheckBoxFor(dialog, QStringLiteral("SHARED"));
    QVERIFY(shared);
    QVERIFY(shared->isChecked());
    QCOMPARE(dialog.selectedExtraColumns(), QStringList({QStringLiteral("SHARED")}));

    classList->item(1)->setCheckState(Qt::Checked);
    QCOMPARE(extraColumnLabelsFor(dialog), expected);
    shared = extraColumnCheckBoxFor(dialog, QStringLiteral("SHARED"));
    QVERIFY(shared);
    QVERIFY(shared->isChecked());
}

void RosterPrintDialogTests::
emptyRosterDoesNotDiscardOtherClassesExtraColumns()
{
    RosterPrintDialogFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int alphaId = 0;
    int betaId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Alpha Empty"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 -1,
                 {},
                 &alphaId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Beta With Roster"),
                 QStringLiteral("E5"),
                 QStringLiteral("Apollo"),
                 -1,
                 {},
                 &betaId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.saveRoster(
                 betaId,
                 {QStringLiteral("Beta Notes")},
                 &error
                 ), qPrintable(error));

    RosterPrintDialog dialog(
        &fixture.services,
        alphaId,
        RosterTemplatePrintService::Scope::AllClasses,
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

    QCOMPARE(extraColumnLabelsFor(dialog), QStringList({
        QStringLiteral("Beta Notes")
    }));
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
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    templateCombo->setCurrentIndex(extraInfoIndex);

    QVERIFY(extraColumnLabelsFor(dialog).isEmpty());
    QVERIFY(dialog.selectedExtraColumns().isEmpty());
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

void RosterPrintDialogTests::
classListReadFailurePreservesRenderedExtraInfoControls()
{
    RosterPrintDialogFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int classId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Class with extra info"),
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

    RosterPrintDialog dialog(
        &fixture.services,
        classId,
        RosterTemplatePrintService::Scope::SelectedClasses,
        RosterPrintDialog::Action::Print
        );
    auto* const templateCombo = dialog.findChild<QComboBox*>(
        QStringLiteral("templateCombo")
        );
    QVERIFY(templateCombo);
    const int extraInfoIndex = templateCombo->findData(
        static_cast<int>(
            RosterTemplatePrintService::TemplateId::PerClassWithExtraInfo
            )
        );
    QVERIFY(extraInfoIndex >= 0);
    templateCombo->setCurrentIndex(extraInfoIndex);

    const QStringList labelsBefore = extraColumnLabelsFor(dialog);
    QCOMPARE(labelsBefore, QStringList({QStringLiteral("Private Notes")}));
    QCheckBox* const privateNotes = extraColumnCheckBoxFor(
        dialog,
        QStringLiteral("Private Notes")
        );
    QVERIFY(privateNotes);
    QPointer<QCheckBox> privateNotesControl = privateNotes;
    privateNotes->setChecked(true);
    QVERIFY(privateNotes->isChecked());
    const QStringList selectedBefore = dialog.selectedExtraColumns();
    QCOMPARE(selectedBefore, QStringList({QStringLiteral("Private Notes")}));

    QVERIFY(fixture.services.databaseSession()->isOpen());
    QVERIFY(fixture.services.classService()->isAvailable());
    QVERIFY(fixture.services.rosterService()->isAvailable());
    QSqlQuery dropClasses(fixture.services.databaseSession()->database());
    QVERIFY2(dropClasses.exec(QStringLiteral("DROP TABLE classes")),
             qPrintable(dropClasses.lastError().text()));
    QVERIFY(fixture.services.databaseSession()->isOpen());
    QVERIFY(fixture.services.classService()->isAvailable());
    QVERIFY(fixture.services.rosterService()->isAvailable());

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QVERIFY(QMetaObject::invokeMethod(
        &dialog,
        "updateExtraInfoColumns",
        Qt::DirectConnection
        ));

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.title, QStringLiteral("Print Rosters"));
    QCOMPARE(warning.message, QStringLiteral("Classes could not be loaded."));
    QVERIFY(!warning.details.trimmed().isEmpty());
    QVERIFY(warning.details.contains(QStringLiteral("classes"),
                                    Qt::CaseInsensitive));
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);

    QCOMPARE(extraColumnLabelsFor(dialog), labelsBefore);
    QVERIFY(!privateNotesControl.isNull());
    QVERIFY(extraColumnCheckBoxFor(
        dialog,
        QStringLiteral("Private Notes")
        ) == privateNotesControl.data());
    QVERIFY(privateNotesControl->isChecked());
    QCOMPARE(dialog.selectedExtraColumns(), selectedBefore);
}

QTEST_MAIN(RosterPrintDialogTests)

#include "roster_print_dialog_tests.moc"
