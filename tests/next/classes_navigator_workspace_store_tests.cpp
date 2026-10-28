#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "domain/models/class_info.h"
#include "next/application/classes_list_read_snapshot.h"
#include "next/application/classes_navigation_snapshot.h"
#include "next/platform/workspace_store_class_editor_actions.h"
#include "next/platform/workspace_store_classes_navigator_read_actions.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>
#include <limits>
#include <string>

using namespace ClassMngr::Next;

namespace
{

QString fixturePath(QTemporaryDir& directory, const QString& filename)
{
    const QString destination = directory.filePath(filename);
    QFile::remove(destination);
    return QFile::copy(
        QStringLiteral(CLASSMNGR_CLASSES_NAVIGATOR_FIXTURE), destination)
        ? destination
        : QString{};
}

QString supplementaryCharacters(const int count)
{
    QString text;
    text.reserve(count * 2);
    for (int index = 0; index < count; ++index)
    {
        text.append(QChar(0xD83D));
        text.append(QChar(0xDE00));
    }
    return text;
}

Application::ClassesNavigationSnapshotQuery queryFor(
    const Application::ClassesListSnapshot& classes
    )
{
    Application::ClassesNavigationSnapshotQuery query;
    query.classes.reserve(classes.classes.size());
    for (const auto& classroom : classes.classes)
    {
        query.classes.push_back({
            .classId = classroom.classId,
            .className = classroom.className
        });
    }
    return query;
}

QList<ClassNavigationBoundedReadRequestEntry> repositoryRequestFor(
    const QList<Classroom>& classes
    )
{
    QList<ClassNavigationBoundedReadRequestEntry> request;
    request.reserve(classes.size());
    for (const Classroom& classroom : classes)
    {
        request.append({
            .classId = classroom.id,
            .className = classroom.name
        });
    }
    return request;
}

ClassNavigationBoundedReadLimits repositoryReadLimits()
{
    return {
        .maximumClassCount = static_cast<int>(
            Application::kClassesNavigationMaximumClasses),
        .maximumTextLength = static_cast<int>(
            Application::kClassesNavigationMaximumTextLength),
        .maximumScheduleRowsPerSourceAndClass = static_cast<int>(
            Application::kClassesNavigationMaximumScheduleRows),
        .maximumScheduleTokenLength = static_cast<int>(
            Application::kClassesNavigationMaximumScheduleTokenLength)
    };
}

} // namespace

class ClassesNavigatorWorkspaceStoreTests final : public QObject
{
    Q_OBJECT

private slots:
    void readsBoundedListAndOrderedNavigationSummaries();
    void rejectsChangedIdentityAndOverLimitRequests();
    void rejectsMoreThanFourThousandNinetySixClasses();
    void rejectsMoreThanFourteenScheduleRowsPerSourceAndClass();
    void rejectsOverlongClassNamesAndScheduleTokens();
    void readsAndPersistsNavigatorPreferences();
    void repositoryClassListPreservesOrderAndExcludesTestingClasses();
    void repositoryClassListAcceptsExactCountBoundAndRejectsOneOver();
    void repositoryTextFieldsAcceptExactBoundAndRejectOneOver();
    void repositoryNavigationAcceptsExactScheduleAndTokenBounds();
    void repositoryNavigationRejectsScheduleOverflowAndIdentityChanges();
    void repositoryNavigationKeepsSchedulesUnderClassInfoParents();
    void repositoryReadFailuresRemainTyped();
    void classEditorPathActionsPersistDetailsAndCoTeacherTransactionally();
    void classEditorDetailsEnforceFourteenRowsPerScheduleSource();
    void classCoTeacherReadsEnforceRowAndTextBounds();
    void classNotesPathActionsBoundTextAndPreserveDetailsTransactionally();
};

void ClassesNavigatorWorkspaceStoreTests::
classNotesPathActionsBoundTextAndPreserveDetailsTransactionally()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("class-notes.tps"));
    int numericClassId = 0;
    qint64 teacherId = 0;
    const QString exactNotes(10'000, QLatin1Char('N'));
    const QString exactActivities(10'000, QLatin1Char('A'));
    {
        WorkspaceStore store;
        QVERIFY2(store.open(path), "temporary workspace should be created");
        const auto created = store.classRepository()->createClass(
            QStringLiteral("Notes persistence"));
        QVERIFY(created);
        numericClassId = created.value();
        QSqlQuery teacher(store.database());
        QVERIFY(teacher.exec(QStringLiteral(
            "INSERT INTO teachers(teacher_kr, teacher_en, preferred_name) "
            "VALUES('담당 교사', 'Main Teacher', 'Preferred Teacher')")));
        teacherId = teacher.lastInsertId().toLongLong();
        QVERIFY(teacherId > 0);

        ClassInfo info;
        info.classId = numericClassId;
        info.classGrade = QStringLiteral("E4");
        info.classLevel = QStringLiteral("Theseus");
        info.readingBook = QStringLiteral("Reading Explorer 1");
        info.essayBook = QStringLiteral("4A");
        info.classColor = QStringLiteral("#ABCDEF");
        info.fontColor = QStringLiteral("#000000");
        info.notes = exactNotes;
        info.timeFillerActivities = exactActivities;
        info.classTimes = {{QStringLiteral("Monday"), QStringLiteral("9:00 AM"),
            QStringLiteral("9:50 AM")}};
        info.intensiveTimes = {{QStringLiteral("Tuesday"), QStringLiteral("10:00 AM"),
            QStringLiteral("10:50 AM")}};
        QVERIFY(store.classInfoRepository()->saveClassDetailsDetailed(
            info, teacherId).has_value());
    }

    const auto classId = Domain::ClassId::fromString(std::to_string(numericClassId));
    QVERIFY(classId.has_value());
    const auto exactRead = Platform::readClassNotesAtPath(path, *classId);
    QVERIFY(exactRead);
    QVERIFY(exactRead.value().classFields);
    QCOMPARE(exactRead.value().classFields.value().notes.size(), std::size_t(10'000));
    QCOMPARE(exactRead.value().classFields.value().timeFillerActivities.size(),
        std::size_t(10'000));
    QCOMPARE(exactRead.value().classFields.value().regularSchedule.size(), std::size_t(1));
    QVERIFY(exactRead.value().teacherDisplayName);
    QCOMPARE(QString::fromStdU16String(
        exactRead.value().teacherDisplayName.value()), QStringLiteral("Preferred Teacher"));

    const QString exactSupplementary = supplementaryCharacters(5'000);
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery update(store.database());
        QVERIFY(update.prepare(QStringLiteral(
            "UPDATE class_info SET notes = ?, time_filler_activities = ? "
            "WHERE class_id = ?")));
        update.addBindValue(exactSupplementary);
        update.addBindValue(exactActivities);
        update.addBindValue(numericClassId);
        QVERIFY2(update.exec(), qPrintable(update.lastError().text()));
    }
    const auto exactSupplementaryNotes = Platform::readClassNotesAtPath(path, *classId);
    QVERIFY(exactSupplementaryNotes);
    QVERIFY(exactSupplementaryNotes.value().classFields);
    QCOMPARE(exactSupplementaryNotes.value().classFields.value().notes.size(),
        std::size_t(10'000));
    QCOMPARE(QString::fromStdU16String(
        exactSupplementaryNotes.value().classFields.value().notes), exactSupplementary);

    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery update(store.database());
        QVERIFY(update.prepare(QStringLiteral(
            "UPDATE class_info SET notes = ?, time_filler_activities = ? "
            "WHERE class_id = ?")));
        update.addBindValue(exactNotes);
        update.addBindValue(exactSupplementary);
        update.addBindValue(numericClassId);
        QVERIFY2(update.exec(), qPrintable(update.lastError().text()));
    }
    const auto exactSupplementaryActivities =
        Platform::readClassNotesAtPath(path, *classId);
    QVERIFY(exactSupplementaryActivities);
    QVERIFY(exactSupplementaryActivities.value().classFields);
    QCOMPARE(exactSupplementaryActivities.value().classFields.value()
        .timeFillerActivities.size(), std::size_t(10'000));
    QCOMPARE(QString::fromStdU16String(exactSupplementaryActivities.value()
        .classFields.value().timeFillerActivities), exactSupplementary);
    QCOMPARE(exactSupplementary.size(), 10'000);

    const Application::ClassNotesSaveRequest exactSaveRequest{
        .classId = *classId,
        .notes = exactNotes.toStdU16String(),
        .timeFillerActivities = exactActivities.toStdU16String()
    };
    QVERIFY(Platform::saveClassNotesAtPath(path, exactSaveRequest));

    const Application::ClassNotesSaveRequest trimmedRequest{
        .classId = *classId,
        .notes = u"  trimmed note  ",
        .timeFillerActivities = u"  trimmed activities \n"
    };
    QVERIFY(Platform::saveClassNotesAtPath(path, trimmedRequest));
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery saved(store.database());
        QVERIFY(saved.prepare(QStringLiteral(
            "SELECT teacher_id, class_grade, class_level, reading_book, essay_book, "
            "class_color, font_color, notes, time_filler_activities "
            "FROM class_info WHERE class_id = ?")));
        saved.addBindValue(numericClassId);
        QVERIFY(saved.exec());
        QVERIFY(saved.next());
        QCOMPARE(saved.value(0).toLongLong(), teacherId);
        QCOMPARE(saved.value(1).toString(), QStringLiteral("E4"));
        QCOMPARE(saved.value(2).toString(), QStringLiteral("Theseus"));
        QCOMPARE(saved.value(3).toString(), QStringLiteral("Reading Explorer 1"));
        QCOMPARE(saved.value(4).toString(), QStringLiteral("4A"));
        QCOMPARE(saved.value(5).toString(), QStringLiteral("#ABCDEF"));
        QCOMPARE(saved.value(6).toString(), QStringLiteral("#000000"));
        QCOMPARE(saved.value(7).toString(), QStringLiteral("trimmed note"));
        QCOMPARE(saved.value(8).toString(), QStringLiteral("trimmed activities"));
    }

    Application::ClassNotesSaveRequest tooLongNotes{
        .classId = *classId,
        .notes = std::u16string(10'001, u'N'),
        .timeFillerActivities = u"unchanged"
    };
    const auto notesSaveOverflow = Platform::saveClassNotesAtPath(path, tooLongNotes);
    QVERIFY(!notesSaveOverflow);
    QCOMPARE(notesSaveOverflow.error().code, Domain::ErrorCode::Validation);
    Application::ClassNotesSaveRequest tooLongActivities{
        .classId = *classId,
        .notes = u"unchanged",
        .timeFillerActivities = std::u16string(10'001, u'A')
    };
    const auto activitiesSaveOverflow = Platform::saveClassNotesAtPath(
        path, tooLongActivities);
    QVERIFY(!activitiesSaveOverflow);
    QCOMPARE(activitiesSaveOverflow.error().code, Domain::ErrorCode::Validation);

    const QString overSupplementary = supplementaryCharacters(5'001);
    QCOMPARE(overSupplementary.size(), 10'002);
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery update(store.database());
        QVERIFY(update.prepare(QStringLiteral(
            "UPDATE class_info SET notes = ?, time_filler_activities = ? "
            "WHERE class_id = ?")));
        update.addBindValue(overSupplementary);
        update.addBindValue(exactActivities);
        update.addBindValue(numericClassId);
        QVERIFY2(update.exec(), qPrintable(update.lastError().text()));
    }
    const auto overSupplementaryNotes = Platform::readClassNotesAtPath(path, *classId);
    QVERIFY(!overSupplementaryNotes);
    QCOMPARE(overSupplementaryNotes.error().code, Domain::ErrorCode::Validation);
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery update(store.database());
        QVERIFY(update.prepare(QStringLiteral(
            "UPDATE class_info SET notes = ?, time_filler_activities = ? "
            "WHERE class_id = ?")));
        update.addBindValue(exactNotes);
        update.addBindValue(overSupplementary);
        update.addBindValue(numericClassId);
        QVERIFY2(update.exec(), qPrintable(update.lastError().text()));
    }
    const auto overSupplementaryActivities =
        Platform::readClassNotesAtPath(path, *classId);
    QVERIFY(!overSupplementaryActivities);
    QCOMPARE(overSupplementaryActivities.error().code, Domain::ErrorCode::Validation);

    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery update(store.database());
        QVERIFY(update.prepare(QStringLiteral(
            "UPDATE class_info SET notes = ? WHERE class_id = ?")));
        update.addBindValue(QString(10'001, QLatin1Char('X')));
        update.addBindValue(numericClassId);
        QVERIFY2(update.exec(), qPrintable(update.lastError().text()));
    }
    const auto notesReadOverflow = Platform::readClassNotesAtPath(path, *classId);
    QVERIFY(!notesReadOverflow);
    QCOMPARE(notesReadOverflow.error().code, Domain::ErrorCode::Validation);
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery update(store.database());
        QVERIFY(update.prepare(QStringLiteral(
            "UPDATE class_info SET notes = ?, time_filler_activities = ? "
            "WHERE class_id = ?")));
        update.addBindValue(QStringLiteral("notes before trigger"));
        update.addBindValue(QString(10'001, QLatin1Char('X')));
        update.addBindValue(numericClassId);
        QVERIFY2(update.exec(), qPrintable(update.lastError().text()));
    }
    const auto activitiesReadOverflow = Platform::readClassNotesAtPath(path, *classId);
    QVERIFY(!activitiesReadOverflow);
    QCOMPARE(activitiesReadOverflow.error().code, Domain::ErrorCode::Validation);

    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery restore(store.database());
        QVERIFY(restore.prepare(QStringLiteral(
            "UPDATE class_info SET notes = ?, time_filler_activities = ? "
            "WHERE class_id = ?")));
        restore.addBindValue(QStringLiteral("before trigger"));
        restore.addBindValue(QStringLiteral("before trigger activities"));
        restore.addBindValue(numericClassId);
        QVERIFY2(restore.exec(), qPrintable(restore.lastError().text()));
        QSqlQuery trigger(store.database());
        QVERIFY2(trigger.exec(QStringLiteral(
            "CREATE TRIGGER reject_class_notes BEFORE UPDATE OF notes ON class_info "
            "WHEN NEW.class_id = %1 BEGIN SELECT RAISE(ABORT, 'forced notes failure'); END"
            ).arg(numericClassId)), qPrintable(trigger.lastError().text()));
    }

    const Application::ClassNotesSaveRequest triggerRequest{
        .classId = *classId,
        .notes = u"retry value",
        .timeFillerActivities = u"retry activities"
    };
    const auto failed = Platform::saveClassNotesAtPath(path, triggerRequest);
    QVERIFY(!failed);
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery afterFailure(store.database());
        QVERIFY(afterFailure.prepare(QStringLiteral(
            "SELECT teacher_id, class_grade, notes, time_filler_activities "
            "FROM class_info WHERE class_id = ?")));
        afterFailure.addBindValue(numericClassId);
        QVERIFY(afterFailure.exec());
        QVERIFY(afterFailure.next());
        QCOMPARE(afterFailure.value(0).toLongLong(), teacherId);
        QCOMPARE(afterFailure.value(1).toString(), QStringLiteral("E4"));
        QCOMPARE(afterFailure.value(2).toString(), QStringLiteral("before trigger"));
        QCOMPARE(afterFailure.value(3).toString(),
            QStringLiteral("before trigger activities"));
        QSqlQuery drop(store.database());
        QVERIFY2(drop.exec(QStringLiteral("DROP TRIGGER reject_class_notes")),
            qPrintable(drop.lastError().text()));
    }
    QVERIFY(Platform::saveClassNotesAtPath(path, triggerRequest));

    Application::ClassDetailsValidationInput detailsInput;
    detailsInput.classId = *classId;
    detailsInput.classGrade = u"E5";
    detailsInput.classLevel = u"Artemis";
    detailsInput.readingBook = u"Reading Explorer 2";
    detailsInput.essayBook = u"5A";
    detailsInput.classColor = u"#123456";
    detailsInput.fontColor = u"#FFFFFF";
    detailsInput.regularTimes = {{u"Wednesday", u"4:00 PM", u"4:50 PM"}};
    detailsInput.intensiveTimes = {{u"Thursday", u"5:00 PM", u"5:50 PM"}};
    const auto detailsSaved = Platform::saveClassDetailsAtPath(path, {
        .input = detailsInput,
        .saveRegularTimes = true,
        .saveIntensiveTimes = true
    });
    QVERIFY(std::holds_alternative<Application::ClassDetailsSaveWorkflowSuccess>(
        detailsSaved));
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        const auto record = store.classInfoRepository()->loadClassDetailsSaveInfo(
            numericClassId);
        QVERIFY(record);
        QCOMPARE(record->teacherId, teacherId);
        QCOMPARE(record->info.notes, QStringLiteral("retry value"));
        QCOMPARE(record->info.timeFillerActivities,
            QStringLiteral("retry activities"));
        QCOMPARE(record->info.classGrade, QStringLiteral("E5"));
    }
}

void ClassesNavigatorWorkspaceStoreTests::
classEditorDetailsEnforceFourteenRowsPerScheduleSource()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("editor-schedule-bound.tps"));
    int numericClassId = 0;
    {
        WorkspaceStore store;
        QVERIFY2(store.open(path), "temporary workspace should be created");
        const auto created = store.classRepository()->createClass(
            QStringLiteral("Bounded details"));
        QVERIFY(created);
        numericClassId = created.value();
        ClassInfo details;
        details.classId = numericClassId;
        details.classGrade = QStringLiteral("E4");
        details.classLevel = QStringLiteral("Theseus");
        details.readingBook = QStringLiteral("Reading Explorer 1");
        details.essayBook = QStringLiteral("4A");
        details.classColor = QStringLiteral("#ABCDEF");
        details.fontColor = QStringLiteral("#000000");
        for (int row = 0; row < 14; ++row)
        {
            const QString start = QStringLiteral("%1:00 AM").arg(row + 1);
            const QString end = QStringLiteral("%1:50 AM").arg(row + 1);
            details.classTimes.push_back({row == 0
                    ? QString(64, QLatin1Char('R')) : QStringLiteral("Monday"), start, end});
            details.intensiveTimes.push_back({row == 0
                    ? QString(64, QLatin1Char('I')) : QStringLiteral("Tuesday"), start, end});
        }
        const RepositoryStatus saved = store.classInfoRepository()
            ->saveClassDetailsDetailed(details, -1);
        QVERIFY(saved.has_value());
    }

    const auto classId = Domain::ClassId::fromString(std::to_string(numericClassId));
    QVERIFY(classId.has_value());
    const auto exact = Platform::readClassDetailsAtPath(path, *classId);
    QVERIFY(exact);
    QVERIFY(exact.value().classFields);
    QCOMPARE(exact.value().classFields.value().regularSchedule.size(), std::size_t(14));
    QCOMPARE(exact.value().classFields.value().intensiveSchedule.size(), std::size_t(14));
    const auto exactCoTeacher = Platform::readClassCoTeacherAtPath(path, *classId);
    QVERIFY(exactCoTeacher);
    QVERIFY(exactCoTeacher.value().classFields);
    QCOMPARE(exactCoTeacher.value().classFields.value().regularSchedule.size(),
        std::size_t(14));

    const auto overTokenSave = [&classId, &path](
        const bool regular
        ) {
        Application::ClassDetailsValidationInput input;
        input.classId = *classId;
        const Application::ClassDetailsScheduleValidationRow row{
            std::u16string(65, u'X'), u"9:00 AM", u"9:50 AM"};
        (regular ? input.regularTimes : input.intensiveTimes).push_back(row);
        const auto result = Platform::saveClassDetailsAtPath(path, {
            .input = std::move(input),
            .saveRegularTimes = true,
            .saveIntensiveTimes = true
        });
        const auto* const failure =
            std::get_if<Application::ClassDetailsSaveWorkflowFailure>(&result);
        return failure && failure->stage
            == Application::ClassDetailsSaveWorkflowStage::BoundedInput;
    };
    QVERIFY(overTokenSave(true));
    QVERIFY(overTokenSave(false));

    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery updateRegular(store.database());
        QVERIFY(updateRegular.prepare(QStringLiteral(
            "UPDATE class_times SET day = ? WHERE id = "
            "(SELECT id FROM class_times WHERE class_id = ? ORDER BY id LIMIT 1)")));
        updateRegular.addBindValue(QString(65, QLatin1Char('R')));
        updateRegular.addBindValue(numericClassId);
        QVERIFY2(updateRegular.exec(), qPrintable(updateRegular.lastError().text()));
    }
    const auto regularTokenOverflow = Platform::readClassDetailsAtPath(path, *classId);
    QVERIFY(!regularTokenOverflow);
    QCOMPARE(regularTokenOverflow.error().code, Domain::ErrorCode::Validation);
    const auto regularCoTeacherTokenOverflow =
        Platform::readClassCoTeacherAtPath(path, *classId);
    QVERIFY(!regularCoTeacherTokenOverflow);
    QCOMPARE(regularCoTeacherTokenOverflow.error().code, Domain::ErrorCode::Validation);
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery restoreRegular(store.database());
        QVERIFY(restoreRegular.prepare(QStringLiteral(
            "UPDATE class_times SET day = ? WHERE id = "
            "(SELECT id FROM class_times WHERE class_id = ? ORDER BY id LIMIT 1)")));
        restoreRegular.addBindValue(QString(64, QLatin1Char('R')));
        restoreRegular.addBindValue(numericClassId);
        QVERIFY2(restoreRegular.exec(), qPrintable(restoreRegular.lastError().text()));
        QSqlQuery updateIntensive(store.database());
        QVERIFY(updateIntensive.prepare(QStringLiteral(
            "UPDATE class_intensive_times SET day = ? WHERE id = "
            "(SELECT id FROM class_intensive_times WHERE class_id = ? ORDER BY id LIMIT 1)")));
        updateIntensive.addBindValue(QString(65, QLatin1Char('I')));
        updateIntensive.addBindValue(numericClassId);
        QVERIFY2(updateIntensive.exec(), qPrintable(updateIntensive.lastError().text()));
    }
    const auto intensiveTokenOverflow = Platform::readClassDetailsAtPath(path, *classId);
    QVERIFY(!intensiveTokenOverflow);
    QCOMPARE(intensiveTokenOverflow.error().code, Domain::ErrorCode::Validation);
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery restoreIntensive(store.database());
        QVERIFY(restoreIntensive.prepare(QStringLiteral(
            "UPDATE class_intensive_times SET day = ? WHERE id = "
            "(SELECT id FROM class_intensive_times WHERE class_id = ? ORDER BY id LIMIT 1)")));
        restoreIntensive.addBindValue(QString(64, QLatin1Char('I')));
        restoreIntensive.addBindValue(numericClassId);
        QVERIFY2(restoreIntensive.exec(), qPrintable(restoreIntensive.lastError().text()));
    }

    Application::ClassDetailsValidationInput tooMany;
    tooMany.classId = *classId;
    tooMany.regularTimes.resize(15);
    Application::ClassDetailsSaveWorkflowRequest request{
        .input = tooMany,
        .saveRegularTimes = true,
        .saveIntensiveTimes = true
    };
    auto overSave = Platform::saveClassDetailsAtPath(path, request);
    auto* boundedFailure = std::get_if<Application::ClassDetailsSaveWorkflowFailure>(
        &overSave);
    QVERIFY(boundedFailure);
    QCOMPARE(boundedFailure->stage,
        Application::ClassDetailsSaveWorkflowStage::BoundedInput);
    request.input.regularTimes.clear();
    request.input.intensiveTimes.resize(15);
    overSave = Platform::saveClassDetailsAtPath(path, request);
    boundedFailure = std::get_if<Application::ClassDetailsSaveWorkflowFailure>(
        &overSave);
    QVERIFY(boundedFailure);
    QCOMPARE(boundedFailure->stage,
        Application::ClassDetailsSaveWorkflowStage::BoundedInput);

    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery insertRegular(store.database());
        QVERIFY(insertRegular.prepare(QStringLiteral(
            "INSERT INTO class_times(class_id, day, start_time, end_time) "
            "VALUES(?, 'Wednesday', '6:00 AM', '6:50 AM')")));
        insertRegular.addBindValue(numericClassId);
        QVERIFY2(insertRegular.exec(), qPrintable(insertRegular.lastError().text()));
    }
    const auto regularOverflow = Platform::readClassDetailsAtPath(path, *classId);
    QVERIFY(!regularOverflow);
    QCOMPARE(regularOverflow.error().code, Domain::ErrorCode::Validation);
    const auto regularCoTeacherOverflow = Platform::readClassCoTeacherAtPath(path, *classId);
    QVERIFY(!regularCoTeacherOverflow);
    QCOMPARE(regularCoTeacherOverflow.error().code, Domain::ErrorCode::Validation);

    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery removeRegular(store.database());
        QVERIFY(removeRegular.prepare(QStringLiteral(
            "DELETE FROM class_times WHERE class_id = ? AND day = 'Wednesday'")));
        removeRegular.addBindValue(numericClassId);
        QVERIFY2(removeRegular.exec(), qPrintable(removeRegular.lastError().text()));
        QSqlQuery insertIntensive(store.database());
        QVERIFY(insertIntensive.prepare(QStringLiteral(
            "INSERT INTO class_intensive_times(class_id, day, start_time, end_time) "
            "VALUES(?, 'Wednesday', '6:00 AM', '6:50 AM')")));
        insertIntensive.addBindValue(numericClassId);
        QVERIFY2(insertIntensive.exec(), qPrintable(insertIntensive.lastError().text()));
    }
    const auto intensiveOverflow = Platform::readClassDetailsAtPath(path, *classId);
    QVERIFY(!intensiveOverflow);
    QCOMPARE(intensiveOverflow.error().code, Domain::ErrorCode::Validation);
}

void ClassesNavigatorWorkspaceStoreTests::
classCoTeacherReadsEnforceRowAndTextBounds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("teacher-bound.tps"));
    {
        WorkspaceStore store;
        QVERIFY2(store.open(path), "temporary workspace should be created");
        QSqlQuery insert(store.database());
        QVERIFY(insert.prepare(QStringLiteral(
            "INSERT INTO teachers(teacher_kr, teacher_en) VALUES('교사', ?)")));
        for (int index = 0; index < 4096; ++index)
        {
            insert.bindValue(0, QStringLiteral("Teacher %1").arg(index));
            QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
        }
    }
    const auto exactRows = Platform::readClassCoTeacherChoicesAtPath(path);
    QVERIFY(exactRows);
    QCOMPARE(exactRows.value().teachers.size(), std::size_t(4096));
    QVERIFY(std::all_of(exactRows.value().teachers.cbegin(),
        exactRows.value().teachers.cend(), [](const auto& teacher) {
            return teacher.roomNumber.empty() && teacher.wifiPassword.empty()
                && teacher.zoomPassword.empty();
        }));
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery insert(store.database());
        QVERIFY(insert.exec(QStringLiteral(
            "INSERT INTO teachers(teacher_kr, teacher_en) "
            "VALUES('교사', 'Teacher overflow')")));
    }
    const auto overRows = Platform::readClassCoTeacherChoicesAtPath(path);
    QVERIFY(!overRows);
    QCOMPARE(overRows.error().code, Domain::ErrorCode::Validation);

    qint64 selectedId = 0;
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery clear(store.database());
        QVERIFY(clear.exec(QStringLiteral("DELETE FROM teachers")));
        QSqlQuery insert(store.database());
        QVERIFY(insert.prepare(QStringLiteral(
            "INSERT INTO teachers(teacher_kr, teacher_en, room_number, wifi_name, "
            "wifi_password, internet_type, zoom_id, zoom_password, projection_type) "
            "VALUES('', ?, 'Room A', 'WiFi A', 'pass', 'WiFi', 'zoom', 'zoom-pass', 'HDMI')")));
        insert.addBindValue(QString(static_cast<qsizetype>(4096), QLatin1Char('A')));
        QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
        QSqlQuery find(store.database());
        QVERIFY(find.exec(QStringLiteral("SELECT id FROM teachers LIMIT 1")));
        QVERIFY(find.next());
        bool idOk = false;
        selectedId = find.value(0).toLongLong(&idOk);
        QVERIFY(idOk && selectedId > 0);
    }
    const auto exactText = Platform::readClassCoTeacherChoicesAtPath(path);
    QVERIFY(exactText);
    QCOMPARE(exactText.value().teachers.size(), std::size_t(1));
    const auto teacherId = Domain::TeacherId::fromString(std::to_string(selectedId));
    QVERIFY(teacherId.has_value());
    const auto profile = Platform::readClassCoTeacherTeacherDetailsAtPath(
        path, *teacherId);
    QVERIFY(profile);
    QCOMPARE(profile.value().roomNumber, std::u16string(u"Room A"));
    QCOMPARE(profile.value().wifiPassword, std::u16string(u"pass"));
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery update(store.database());
        QVERIFY(update.prepare(QStringLiteral(
            "UPDATE teachers SET wifi_password = ? WHERE id = ?")));
        update.addBindValue(QString(static_cast<qsizetype>(16384), QLatin1Char('P')));
        update.addBindValue(selectedId);
        QVERIFY2(update.exec(), qPrintable(update.lastError().text()));
    }
    const auto exactProfileBound = Platform::readClassCoTeacherTeacherDetailsAtPath(
        path, *teacherId);
    QVERIFY(exactProfileBound);
    QCOMPARE(exactProfileBound.value().wifiPassword.size(), std::size_t(16384));
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery update(store.database());
        QVERIFY(update.prepare(QStringLiteral(
            "UPDATE teachers SET wifi_password = ? WHERE id = ?")));
        update.addBindValue(QString(static_cast<qsizetype>(16385), QLatin1Char('P')));
        update.addBindValue(selectedId);
        QVERIFY2(update.exec(), qPrintable(update.lastError().text()));
    }
    const auto oversizedProfile = Platform::readClassCoTeacherTeacherDetailsAtPath(
        path, *teacherId);
    QVERIFY(!oversizedProfile);
    QCOMPARE(oversizedProfile.error().code, Domain::ErrorCode::Validation);

    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery clear(store.database());
        QVERIFY(clear.exec(QStringLiteral("DELETE FROM teachers")));
        QSqlQuery insert(store.database());
        QVERIFY(insert.prepare(QStringLiteral(
            "INSERT INTO teachers(teacher_kr, teacher_en) VALUES('', ?)")));
        const QString exactAggregateName(2048, QLatin1Char('A'));
        for (int index = 0; index < 2048; ++index)
        {
            insert.bindValue(0, exactAggregateName);
            QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
        }
    }
    const auto exactAggregate = Platform::readClassCoTeacherChoicesAtPath(path);
    QVERIFY(exactAggregate);
    QCOMPARE(exactAggregate.value().teachers.size(), std::size_t(2048));
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery update(store.database());
        QVERIFY(update.prepare(QStringLiteral(
            "UPDATE teachers SET teacher_en = ? WHERE id = "
            "(SELECT id FROM teachers ORDER BY id LIMIT 1)")));
        update.addBindValue(QString(2049, QLatin1Char('A')));
        QVERIFY2(update.exec(), qPrintable(update.lastError().text()));
    }
    const auto overAggregate = Platform::readClassCoTeacherChoicesAtPath(path);
    QVERIFY(!overAggregate);
    QCOMPARE(overAggregate.error().code, Domain::ErrorCode::Validation);

    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery clear(store.database());
        QVERIFY(clear.exec(QStringLiteral("DELETE FROM teachers")));
        QSqlQuery insert(store.database());
        QVERIFY(insert.prepare(QStringLiteral(
            "INSERT INTO teachers(teacher_kr, teacher_en) VALUES('', ?)")));
        insert.addBindValue(QString(4096, QLatin1Char('A')));
        QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
        QSqlQuery find(store.database());
        QVERIFY(find.exec(QStringLiteral("SELECT id FROM teachers LIMIT 1")));
        QVERIFY(find.next());
        selectedId = find.value(0).toLongLong();
    }
    const auto exactField = Platform::readClassCoTeacherChoicesAtPath(path);
    QVERIFY(exactField);
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery update(store.database());
        QVERIFY(update.prepare(QStringLiteral("UPDATE teachers SET teacher_en = ?")));
        update.addBindValue(QString(4097, QLatin1Char('A')));
        QVERIFY2(update.exec(), qPrintable(update.lastError().text()));
    }
    const auto overField = Platform::readClassCoTeacherChoicesAtPath(path);
    QVERIFY(!overField);
    QCOMPARE(overField.error().code, Domain::ErrorCode::Validation);
}

void ClassesNavigatorWorkspaceStoreTests::
classEditorPathActionsPersistDetailsAndCoTeacherTransactionally()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("class-editor.tps"));
    constexpr qint64 initialTeacherId = 2'147'483'648LL;
    constexpr qint64 coTeacherId = 2'147'483'649LL;
    int numericClassId = 0;
    {
        WorkspaceStore store;
        QVERIFY2(store.open(path), "temporary workspace should be created");
        ClassRepository* const classes = store.classRepository();
        ClassInfoRepository* const classInfo = store.classInfoRepository();
        QVERIFY(classes);
        QVERIFY(classInfo);
        const auto created = classes->createClass(QStringLiteral("Editor acceptance"));
        QVERIFY(created);
        numericClassId = created.value();
        QVERIFY(numericClassId > 0);

        QSqlQuery teachers(store.database());
        QVERIFY(teachers.prepare(QStringLiteral(
            "INSERT INTO teachers (id, teacher_kr, teacher_en, "
            "preferred_romanization, preferred_name, room_number, birthday, "
            "phone_number, wifi_name, wifi_password, internet_type, zoom_id, "
            "zoom_password, projection_type, notes) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")));
        const auto insertTeacher = [&teachers](
            const qint64 id,
            const QString& korean,
            const QString& english,
            const QString& room
            )
        {
            teachers.bindValue(0, id);
            teachers.bindValue(1, korean);
            teachers.bindValue(2, english);
            teachers.bindValue(3, english);
            teachers.bindValue(4, english);
            teachers.bindValue(5, room);
            teachers.bindValue(6, QString{});
            teachers.bindValue(7, QString{});
            teachers.bindValue(8, QStringLiteral("classroom-wifi"));
            teachers.bindValue(9, QStringLiteral("wifi-secret"));
            teachers.bindValue(10, QStringLiteral("WiFi"));
            teachers.bindValue(11, QStringLiteral("zoom-%1").arg(id));
            teachers.bindValue(12, QStringLiteral("zoom-secret"));
            teachers.bindValue(13, QStringLiteral("HDMI"));
            teachers.bindValue(14, QString{});
            return teachers.exec();
        };
        QVERIFY2(insertTeacher(initialTeacherId, QStringLiteral("담당 교사"),
                    QStringLiteral("Main Teacher"), QStringLiteral("Room A")),
            qPrintable(teachers.lastError().text()));
        QVERIFY2(insertTeacher(coTeacherId, QStringLiteral("공동 교사"),
                    QStringLiteral("Co Teacher"), QStringLiteral("Room B")),
            qPrintable(teachers.lastError().text()));

        ClassInfo details;
        details.classId = numericClassId;
        details.classGrade = QStringLiteral("E4");
        details.classLevel = QStringLiteral("Theseus");
        details.readingBook = QStringLiteral("Reading Explorer 1");
        details.essayBook = QStringLiteral("4A");
        details.classColor = QStringLiteral("#ABCDEF");
        details.fontColor = QStringLiteral("#000000");
        details.notes = QStringLiteral("preserve notes");
        details.timeFillerActivities = QStringLiteral("preserve filler activities");
        details.classTimes = {{
            QStringLiteral("Monday"), QStringLiteral("8:00 AM"), QStringLiteral("8:50 AM")
        }};
        details.intensiveTimes = {{
            QStringLiteral("Tuesday"), QStringLiteral("9:00 AM"), QStringLiteral("9:50 AM")
        }};
        const RepositoryStatus seeded = classInfo->saveClassDetailsDetailed(
            details, initialTeacherId);
        QVERIFY(seeded.has_value());
        teachers.finish();
        store.close();
    }

    const auto classId = Domain::ClassId::fromString(std::to_string(numericClassId));
    QVERIFY(classId.has_value());
    const auto initialRead = Platform::readClassDetailsAtPath(path, *classId);
    QVERIFY(initialRead);
    QVERIFY(initialRead.value().classFields);
    QCOMPARE(initialRead.value().classFields.value().classGrade, std::string("E4"));
    QCOMPARE(initialRead.value().teacherDisplayName.value(), std::string("Main Teacher"));
    QCOMPARE(initialRead.value().studentCount.value().rawValue(), 0);

    const auto choices = Platform::readClassCoTeacherChoicesAtPath(path);
    if (!choices)
    {
        qWarning() << "co-teacher choices error:"
            << QString::fromStdString(choices.error().message);
    }
    QVERIFY(choices);
    QVERIFY(std::any_of(choices.value().teachers.cbegin(), choices.value().teachers.cend(),
        [](const auto& teacher) {
            return teacher.teacherId.value() == "2147483649"
                && teacher.teacherEn == u"Co Teacher"
                && teacher.roomNumber.empty() && teacher.wifiPassword.empty()
                && teacher.zoomPassword.empty();
        }));
    const auto initialTeacherDomainId = Domain::TeacherId::fromString("2147483648");
    QVERIFY(initialTeacherDomainId.has_value());
    const auto initialTeacherDetails = Platform::readClassCoTeacherTeacherDetailsAtPath(
        path, *initialTeacherDomainId);
    QVERIFY(initialTeacherDetails);
    QCOMPARE(initialTeacherDetails.value().roomNumber, std::u16string(u"Room A"));
    QCOMPARE(initialTeacherDetails.value().wifiPassword, std::u16string(u"wifi-secret"));

    Application::ClassDetailsValidationInput input;
    input.classId = *classId;
    input.classGrade = u"E5";
    input.classLevel = u"Artemis";
    input.readingBook = u"Reading Explorer 2";
    input.essayBook = u"5A";
    input.classColor = u"#123456";
    input.fontColor = u"#FFFFFF";
    input.regularTimes = {{u"Wednesday", u"4:00 PM", u"4:50 PM"}};
    input.intensiveTimes = {{u"Thursday", u"5:00 PM", u"5:50 PM"}};
    Application::ClassDetailsSaveWorkflowRequest request{
        .input = input,
        .saveRegularTimes = true,
        .saveIntensiveTimes = true
    };
    const auto detailsSaved = Platform::saveClassDetailsAtPath(path, request);
    QVERIFY(std::holds_alternative<Application::ClassDetailsSaveWorkflowSuccess>(
        detailsSaved));
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        ClassInfoRepository* const repository = store.classInfoRepository();
        QVERIFY(repository);
        const auto saved = repository->loadClassDetailsSaveInfo(numericClassId);
        QVERIFY(saved);
        QCOMPARE(saved->teacherId, initialTeacherId);
        QCOMPARE(saved->info.notes, QStringLiteral("preserve notes"));
        QCOMPARE(saved->info.timeFillerActivities,
            QStringLiteral("preserve filler activities"));
        QCOMPARE(saved->info.classGrade, QStringLiteral("E5"));
        QCOMPARE(saved->info.classColor, QStringLiteral("#123456"));
        QCOMPARE(saved->info.classTimes.size(), 1);
        QCOMPARE(saved->info.classTimes.front().day, QStringLiteral("Wednesday"));
        QCOMPARE(saved->info.intensiveTimes.size(), 1);
        QCOMPARE(saved->info.intensiveTimes.front().day, QStringLiteral("Thursday"));
    }

    int conflictClassId = 0;
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        const auto created = store.classRepository()->createClass(
            QStringLiteral("Schedule conflict"));
        QVERIFY(created);
        conflictClassId = created.value();
        ClassInfo conflictDetails;
        conflictDetails.classId = conflictClassId;
        conflictDetails.classGrade = QStringLiteral("E5");
        conflictDetails.classLevel = QStringLiteral("Artemis");
        conflictDetails.readingBook = QStringLiteral("Reading Explorer 2");
        conflictDetails.essayBook = QStringLiteral("5A");
        conflictDetails.classColor = QStringLiteral("#ABCDEF");
        conflictDetails.fontColor = QStringLiteral("#000000");
        const RepositoryStatus seededConflict =
            store.classInfoRepository()->saveClassDetailsDetailed(
                conflictDetails, -1);
        QVERIFY(seededConflict.has_value());
        QSqlQuery insertConflict(store.database());
        QVERIFY(insertConflict.prepare(QStringLiteral(
            "INSERT INTO class_times(class_id, day, start_time, end_time) "
            "VALUES(?, 'Wednesday', '4:00 PM', '4:50 PM')")));
        insertConflict.addBindValue(conflictClassId);
        QVERIFY2(insertConflict.exec(), qPrintable(insertConflict.lastError().text()));
    }
    Application::ClassDetailsSaveWorkflowRequest conflictingRequest = request;
    conflictingRequest.input.classColor = u"#112233";
    const auto scheduleConflict = Platform::saveClassDetailsAtPath(
        path, conflictingRequest);
    const auto* const conflict =
        std::get_if<Application::ClassDetailsSaveWorkflowConflict>(
            &scheduleConflict);
    QVERIFY(conflict);
    QVERIFY(!conflict->conflicts.empty());
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        const auto unchanged = store.classInfoRepository()->loadClassDetailsSaveInfo(
            numericClassId);
        QVERIFY(unchanged);
        QCOMPARE(unchanged->info.classColor, QStringLiteral("#123456"));
    }

    const auto assignment = Platform::assignClassCoTeacherAtPath(path, {
        .classId = *classId,
        .teacherId = Domain::TeacherId::fromString(std::to_string(coTeacherId))
    });
    QVERIFY(assignment);
    const auto coTeacherRead = Platform::readClassCoTeacherAtPath(path, *classId);
    QVERIFY(coTeacherRead);
    QVERIFY(coTeacherRead.value().classFields);
    QCOMPARE(coTeacherRead.value().classFields.value().selectedTeacherId->value(),
        std::string("2147483649"));
    QCOMPARE(QString::fromStdU16String(coTeacherRead.value().teacherDisplayName.value()),
        QStringLiteral("Co Teacher"));
    const auto afterAssignmentDetails = Platform::readClassDetailsAtPath(path, *classId);
    QVERIFY(afterAssignmentDetails);
    QCOMPARE(afterAssignmentDetails.value().teacherDisplayName.value(),
        std::string("Co Teacher"));

    // A schedule-row failure happens inside the repository transaction. The
    // field update and schedule replacement must roll back together, while a
    // later retry succeeds and keeps the latest Co-Teacher assignment.
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery trigger(store.database());
        QVERIFY2(trigger.exec(QStringLiteral(
            "CREATE TRIGGER reject_editor_schedule BEFORE INSERT ON class_times "
            "WHEN NEW.class_id = %1 BEGIN SELECT RAISE(ABORT, 'forced write failure'); END"
        ).arg(numericClassId)), qPrintable(trigger.lastError().text()));
    }
    input.classGrade = u"E4";
    input.classLevel = u"Theseus";
    input.readingBook = u"Reading Explorer 1";
    input.essayBook = u"4A";
    input.classColor = u"#654321";
    input.fontColor = u"#FFFFFF";
    input.regularTimes = {{u"Friday", u"6:00 PM", u"6:50 PM"}};
    request.input = input;
    const auto failedSave = Platform::saveClassDetailsAtPath(path, request);
    const auto* const workflowFailure =
        std::get_if<Application::ClassDetailsSaveWorkflowFailure>(&failedSave);
    QVERIFY(workflowFailure);
    QCOMPARE(workflowFailure->stage, Application::ClassDetailsSaveWorkflowStage::Save);
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        ClassInfoRepository* const repository = store.classInfoRepository();
        QVERIFY(repository);
        const auto afterFailure = repository->loadClassDetailsSaveInfo(numericClassId);
        QVERIFY(afterFailure);
        QCOMPARE(afterFailure->teacherId, coTeacherId);
        QCOMPARE(afterFailure->info.classGrade, QStringLiteral("E5"));
        QCOMPARE(afterFailure->info.classColor, QStringLiteral("#123456"));
        QCOMPARE(afterFailure->info.notes, QStringLiteral("preserve notes"));
        QCOMPARE(afterFailure->info.classTimes.front().day, QStringLiteral("Wednesday"));
        QSqlQuery dropTrigger(store.database());
        QVERIFY2(dropTrigger.exec(QStringLiteral("DROP TRIGGER reject_editor_schedule")),
            qPrintable(dropTrigger.lastError().text()));
    }
    const auto retry = Platform::saveClassDetailsAtPath(path, request);
    QVERIFY(std::holds_alternative<Application::ClassDetailsSaveWorkflowSuccess>(retry));
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        const auto saved = store.classInfoRepository()->loadClassDetailsSaveInfo(
            numericClassId);
        QVERIFY(saved);
        QCOMPARE(saved->teacherId, coTeacherId);
        QCOMPARE(saved->info.notes, QStringLiteral("preserve notes"));
        QCOMPARE(saved->info.classTimes.front().day, QStringLiteral("Friday"));
    }

    const auto listBeforeAdd = Platform::readClassesNavigatorListAtPath(path);
    QVERIFY(listBeforeAdd);
    const auto added = Platform::createClassAtPath(path);
    QVERIFY(added);
    const Domain::ClassId addedId = added.value();
    const auto listAfterAdd = Platform::readClassesNavigatorListAtPath(path);
    QVERIFY(listAfterAdd);
    QVERIFY(std::any_of(listAfterAdd.value().classes.cbegin(), listAfterAdd.value().classes.cend(),
        [&addedId](const auto& classroom) { return classroom.classId == addedId; }));
    QVERIFY(listAfterAdd.value().classes.size() > listBeforeAdd.value().classes.size());
    QVERIFY(Platform::deleteClassAtPath(path, {.classId = addedId}));
    const auto listAfterDelete = Platform::readClassesNavigatorListAtPath(path);
    QVERIFY(listAfterDelete);
    QCOMPARE(listAfterDelete.value().classes.size(), listBeforeAdd.value().classes.size());
    QVERIFY(std::none_of(listAfterDelete.value().classes.cbegin(), listAfterDelete.value().classes.cend(),
        [&addedId](const auto& classroom) { return classroom.classId == addedId; }));
}

void ClassesNavigatorWorkspaceStoreTests::
readsBoundedListAndOrderedNavigationSummaries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("navigator.tps"));
    QVERIFY(!path.isEmpty());

    const auto list = Platform::readClassesNavigatorListAtPath(path);
    QVERIFY(list);
    QCOMPARE(list.value().classes.size(), std::size_t(96));
    QVERIFY(std::is_sorted(list.value().classes.cbegin(), list.value().classes.cend(),
        [](const auto& left, const auto& right) {
            return QString::compare(
                QString::fromStdU16String(left.className),
                QString::fromStdU16String(right.className),
                Qt::CaseSensitive) < 0;
        }));

    const auto query = queryFor(list.value());
    const auto navigation =
        Platform::readClassesNavigatorNavigationAtPath(path, query);
    QVERIFY(navigation);
    QCOMPARE(navigation.value().classes.size(), query.classes.size());
    int totalScheduleRows = 0;
    for (std::size_t index = 0; index < query.classes.size(); ++index)
    {
        const auto& requested = query.classes[index];
        const auto& result = navigation.value().classes[index];
        QCOMPARE(result.classId, requested.classId);
        QCOMPARE(result.className, requested.className);
        QVERIFY(result.details);
        QVERIFY(result.details.value().regularSchedule.size()
            <= Application::kClassesNavigationMaximumScheduleRows);
        QVERIFY(result.details.value().intensiveSchedule.size()
            <= Application::kClassesNavigationMaximumScheduleRows);
        totalScheduleRows += static_cast<int>(
            result.details.value().regularSchedule.size()
            + result.details.value().intensiveSchedule.size());
    }
    QVERIFY(totalScheduleRows <= 2 * 96
        * static_cast<int>(Application::kClassesNavigationMaximumScheduleRows));
}

void ClassesNavigatorWorkspaceStoreTests::
rejectsChangedIdentityAndOverLimitRequests()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("identity.tps"));
    QVERIFY(!path.isEmpty());
    const auto list = Platform::readClassesNavigatorListAtPath(path);
    QVERIFY(list);

    auto reordered = queryFor(list.value());
    std::reverse(reordered.classes.begin(), reordered.classes.end());
    const auto changed =
        Platform::readClassesNavigatorNavigationAtPath(path, reordered);
    QVERIFY(!changed);
    QCOMPARE(changed.error().code, Domain::ErrorCode::Validation);

    Application::ClassesNavigationSnapshotQuery tooMany;
    tooMany.classes.reserve(Application::kClassesNavigationMaximumClasses + 1);
    for (std::size_t index = 0;
         index <= Application::kClassesNavigationMaximumClasses; ++index)
    {
        const auto id = Domain::ClassId::fromString(
            std::to_string(index + 1));
        QVERIFY(id.has_value());
        tooMany.classes.push_back({.classId = *id, .className = u"Class"});
    }
    const auto overLimit =
        Platform::readClassesNavigatorNavigationAtPath(path, tooMany);
    QVERIFY(!overLimit);
    QCOMPARE(overLimit.error().code, Domain::ErrorCode::Validation);
}

void ClassesNavigatorWorkspaceStoreTests::
rejectsMoreThanFourThousandNinetySixClasses()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("class-limit.tps"));
    QVERIFY(!path.isEmpty());
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery insert(store.database());
        QVERIFY(insert.prepare(QStringLiteral(
            "INSERT INTO classes(name) VALUES(?)")));
        for (std::size_t index = 0;
             index < Application::kClassesNavigationMaximumClasses; ++index)
        {
            insert.bindValue(0, QStringLiteral("Overflow %1")
                .arg(static_cast<qulonglong>(index)));
            QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
        }
    }

    const auto overLimit = Platform::readClassesNavigatorListAtPath(path);
    QVERIFY(!overLimit);
    QCOMPARE(overLimit.error().code, Domain::ErrorCode::Validation);
}

void ClassesNavigatorWorkspaceStoreTests::
rejectsMoreThanFourteenScheduleRowsPerSourceAndClass()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("schedule-bound.tps"));
    QVERIFY(!path.isEmpty());
    int targetId = 0;
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery classQuery(store.database());
        QVERIFY(classQuery.exec(QStringLiteral(
            "SELECT id, name FROM classes ORDER BY name LIMIT 1")));
        QVERIFY(classQuery.next());
        bool idOk = false;
        targetId = classQuery.value(0).toInt(&idOk);
        QVERIFY(idOk && targetId > 0);
        QSqlQuery insert(store.database());
        QVERIFY(insert.prepare(QStringLiteral(
            "INSERT INTO class_times(class_id, day, start_time, end_time) "
            "VALUES(?, ?, ?, ?)")));
        for (int row = 0; row < 15; ++row)
        {
            insert.bindValue(0, targetId);
            insert.bindValue(1, QStringLiteral("Monday"));
            insert.bindValue(2, QStringLiteral("%1:00").arg(row + 1));
            insert.bindValue(3, QStringLiteral("%1:50").arg(row + 1));
            QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
        }
    }

    const auto list = Platform::readClassesNavigatorListAtPath(path);
    QVERIFY(list);
    const auto navigation =
        Platform::readClassesNavigatorNavigationAtPath(path, queryFor(list.value()));
    QVERIFY(!navigation);
    QCOMPARE(navigation.error().code, Domain::ErrorCode::Validation);
}

void ClassesNavigatorWorkspaceStoreTests::
rejectsOverlongClassNamesAndScheduleTokens()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString namePath = fixturePath(directory, QStringLiteral("name-bound.tps"));
    QVERIFY(!namePath.isEmpty());
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(namePath));
        QSqlQuery update(store.database());
        QVERIFY(update.prepare(QStringLiteral(
            "UPDATE classes SET name = ? WHERE id = "
            "(SELECT id FROM classes ORDER BY name LIMIT 1)")));
        update.addBindValue(QString(
            static_cast<qsizetype>(Application::kClassesNavigationMaximumTextLength + 1),
            QLatin1Char('x')));
        QVERIFY2(update.exec(), qPrintable(update.lastError().text()));
    }
    const auto longName = Platform::readClassesNavigatorListAtPath(namePath);
    QVERIFY(!longName);
    QCOMPARE(longName.error().code, Domain::ErrorCode::Validation);

    const QString tokenPath = fixturePath(directory, QStringLiteral("token-bound.tps"));
    QVERIFY(!tokenPath.isEmpty());
    int targetId = 0;
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(tokenPath));
        QSqlQuery classQuery(store.database());
        QVERIFY(classQuery.exec(QStringLiteral(
            "SELECT id FROM classes ORDER BY name LIMIT 1")));
        QVERIFY(classQuery.next());
        bool idOk = false;
        targetId = classQuery.value(0).toInt(&idOk);
        QVERIFY(idOk && targetId > 0);

        QSqlQuery removeSchedule(store.database());
        QVERIFY(removeSchedule.prepare(QStringLiteral(
            "DELETE FROM class_times WHERE class_id = ?")));
        removeSchedule.addBindValue(targetId);
        QVERIFY2(removeSchedule.exec(),
            qPrintable(removeSchedule.lastError().text()));

        QSqlQuery insert(store.database());
        QVERIFY(insert.prepare(QStringLiteral(
            "INSERT INTO class_times(class_id, day, start_time, end_time) "
            "VALUES(?, ?, ?, ?)")));
        insert.addBindValue(targetId);
        insert.addBindValue(QString(
            static_cast<qsizetype>(
                Application::kClassesNavigationMaximumScheduleTokenLength + 1),
            QLatin1Char('x')));
        insert.addBindValue(QStringLiteral("09:00"));
        insert.addBindValue(QStringLiteral("09:50"));
        QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    }
    const auto list = Platform::readClassesNavigatorListAtPath(tokenPath);
    QVERIFY(list);
    const auto navigation = Platform::readClassesNavigatorNavigationAtPath(
        tokenPath, queryFor(list.value()));
    QVERIFY(!navigation);
    QCOMPARE(navigation.error().code, Domain::ErrorCode::Validation);
}

void ClassesNavigatorWorkspaceStoreTests::
readsAndPersistsNavigatorPreferences()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("preferences.tps"));
    QVERIFY(!path.isEmpty());

    const auto defaults = Platform::readClassesNavigatorPreferencesAtPath(path);
    QVERIFY(defaults.scheduleDisplayMode.has_value());
    QCOMPARE(*defaults.scheduleDisplayMode, Application::ScheduleDisplayMode::Regular);
    QVERIFY(defaults.visibilityScope.has_value());
    QCOMPARE(*defaults.visibilityScope, Application::ClassVisibilityScope::ActiveSchedule);
    QVERIFY(defaults.dayFilterResetPolicy.has_value());
    QCOMPARE(*defaults.dayFilterResetPolicy,
        Application::ClassDayFilterResetPolicy::OnApplicationClose);
    QVERIFY(defaults.classSelectionResetPolicy.has_value());
    QCOMPARE(*defaults.classSelectionResetPolicy,
        Application::ClassSelectionResetPolicy::OnApplicationClose);

    QVERIFY(Platform::saveClassesNavigatorScheduleModeAtPath(
        path, Application::ScheduleDisplayMode::Intensive));
    QVERIFY(Platform::saveClassesNavigatorVisibilityAtPath(
        path, Application::ClassVisibilityScope::AllClasses));
    const auto saved = Platform::readClassesNavigatorPreferencesAtPath(path);
    QCOMPARE(*saved.scheduleDisplayMode, Application::ScheduleDisplayMode::Intensive);
    QCOMPARE(*saved.visibilityScope, Application::ClassVisibilityScope::AllClasses);
}

void ClassesNavigatorWorkspaceStoreTests::
repositoryClassListPreservesOrderAndExcludesTestingClasses()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("repository-order.tps"));
    QVERIFY(!path.isEmpty());

    WorkspaceStore store;
    QVERIFY(store.openExisting(path));
    QSqlQuery insert(store.database());
    QVERIFY(insert.prepare(QStringLiteral("INSERT INTO classes(name) VALUES(?)")));
    insert.bindValue(0, QStringLiteral("Navigator duplicate"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    const int firstDuplicateId = insert.lastInsertId().toInt();
    insert.bindValue(0, QStringLiteral("Navigator duplicate"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    const int secondDuplicateId = insert.lastInsertId().toInt();
    insert.bindValue(0, QStringLiteral("Navigator duplicate"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    const int thirdDuplicateId = insert.lastInsertId().toInt();

    QSqlQuery markTesting(store.database());
    QVERIFY(markTesting.prepare(QStringLiteral(
        "INSERT INTO testing_classes(class_id, room) VALUES(?, ?)")));
    markTesting.addBindValue(thirdDuplicateId);
    markTesting.addBindValue(QStringLiteral("Test room"));
    QVERIFY2(markTesting.exec(), qPrintable(markTesting.lastError().text()));

    ClassRepository* const repository = store.classRepository();
    QVERIFY(repository);
    const auto classes = repository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(classes);

    const auto firstPosition = std::find_if(
        classes->cbegin(), classes->cend(), [firstDuplicateId](const Classroom& item) {
            return item.id == firstDuplicateId;
        });
    const auto secondPosition = std::find_if(
        classes->cbegin(), classes->cend(), [secondDuplicateId](const Classroom& item) {
            return item.id == secondDuplicateId;
        });
    QVERIFY(firstPosition != classes->cend());
    QVERIFY(secondPosition != classes->cend());
    QVERIFY(firstPosition < secondPosition);
    QCOMPARE(secondPosition->name, QStringLiteral("Navigator duplicate"));

    QSqlQuery actualIds(store.database());
    QVERIFY(actualIds.exec(QStringLiteral(
        "SELECT id FROM classes WHERE name = 'Navigator duplicate' ORDER BY id")));
    QVERIFY(actualIds.next());
    QCOMPARE(firstPosition->id, actualIds.value(0).toInt());
    QVERIFY(actualIds.next());
    QCOMPARE(secondPosition->id, actualIds.value(0).toInt());
    const auto testingPosition = std::find_if(
        classes->cbegin(), classes->cend(), [thirdDuplicateId](const Classroom& item) {
            return item.id == thirdDuplicateId;
        });
    QVERIFY(testingPosition == classes->cend());
}

void ClassesNavigatorWorkspaceStoreTests::
repositoryClassListAcceptsExactCountBoundAndRejectsOneOver()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("repository-count.tps"));
    QVERIFY(!path.isEmpty());

    WorkspaceStore store;
    QVERIFY(store.openExisting(path));
    ClassRepository* const repository = store.classRepository();
    QVERIFY(repository);
    const auto initial = repository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(initial);
    QCOMPARE(initial->size(), qsizetype(96));

    QSqlQuery insert(store.database());
    QVERIFY(insert.prepare(QStringLiteral("INSERT INTO classes(name) VALUES(?)")));
    for (int index = 0; index < 4000; ++index)
    {
        insert.bindValue(0, QStringLiteral("Repository bound %1").arg(index));
        QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    }
    const auto exact = repository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(exact);
    QCOMPARE(exact->size(), qsizetype(4096));
    ClassInfoRepository* const infoRepository = store.classInfoRepository();
    QVERIFY(infoRepository);
    const auto exactNavigation =
        infoRepository->loadBoundedClassesNavigationRecords(
            repositoryRequestFor(*exact), repositoryReadLimits());
    QVERIFY(exactNavigation);
    QCOMPARE(exactNavigation->size(), qsizetype(4096));

    insert.bindValue(0, QStringLiteral("Repository bound overflow"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    const auto over = repository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(!over);
    QCOMPARE(over.error().code, RepositoryErrorCode::Validation);
    const auto navigationOver =
        infoRepository->loadBoundedClassesNavigationRecords(
            repositoryRequestFor(*exact), repositoryReadLimits());
    QVERIFY(!navigationOver);
    QCOMPARE(navigationOver.error().code, RepositoryErrorCode::Validation);
}

void ClassesNavigatorWorkspaceStoreTests::
repositoryTextFieldsAcceptExactBoundAndRejectOneOver()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("repository-text.tps"));
    QVERIFY(!path.isEmpty());

    WorkspaceStore store;
    QVERIFY(store.openExisting(path));
    QSqlQuery targetQuery(store.database());
    QVERIFY(targetQuery.exec(QStringLiteral(
        "SELECT c.id FROM classes c INNER JOIN class_info ci "
        "ON ci.class_id = c.id ORDER BY c.id LIMIT 1")));
    QVERIFY(targetQuery.next());
    const int targetId = targetQuery.value(0).toInt();
    QVERIFY(targetId > 0);

    const QString exactText(
        static_cast<qsizetype>(Application::kClassesNavigationMaximumTextLength),
        QLatin1Char('x'));
    const QString overText(
        static_cast<qsizetype>(Application::kClassesNavigationMaximumTextLength + 1),
        QLatin1Char('x'));
    QSqlQuery updateName(store.database());
    QVERIFY(updateName.prepare(QStringLiteral(
        "UPDATE classes SET name = ? WHERE id = ?")));
    updateName.addBindValue(exactText);
    updateName.addBindValue(targetId);
    QVERIFY2(updateName.exec(), qPrintable(updateName.lastError().text()));

    ClassRepository* const classRepository = store.classRepository();
    QVERIFY(classRepository);
    auto classes = classRepository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(classes);
    const auto target = std::find_if(
        classes->cbegin(), classes->cend(), [targetId](const Classroom& item) {
            return item.id == targetId;
        });
    QVERIFY(target != classes->cend());
    QCOMPARE(target->name.size(),
        static_cast<qsizetype>(Application::kClassesNavigationMaximumTextLength));

    QSqlQuery updateGrade(store.database());
    QVERIFY(updateGrade.prepare(QStringLiteral(
        "UPDATE class_info SET class_grade = ? WHERE class_id = ?")));
    updateGrade.addBindValue(exactText);
    updateGrade.addBindValue(targetId);
    QVERIFY2(updateGrade.exec(), qPrintable(updateGrade.lastError().text()));
    ClassInfoRepository* const infoRepository = store.classInfoRepository();
    QVERIFY(infoRepository);
    auto request = repositoryRequestFor(*classes);
    auto summaries = infoRepository->loadBoundedClassesNavigationRecords(
        request, repositoryReadLimits());
    QVERIFY(summaries);
    const auto summary = std::find_if(
        summaries->cbegin(), summaries->cend(), [targetId](const auto& item) {
            return item.details.classId == targetId;
        });
    QVERIFY(summary != summaries->cend());
    QCOMPARE(summary->details.grade.size(), exactText.size());

    updateGrade.bindValue(0, overText);
    updateGrade.bindValue(1, targetId);
    QVERIFY2(updateGrade.exec(), qPrintable(updateGrade.lastError().text()));
    summaries = infoRepository->loadBoundedClassesNavigationRecords(
        request, repositoryReadLimits());
    QVERIFY(!summaries);
    QCOMPARE(summaries.error().code, RepositoryErrorCode::Validation);

    updateName.bindValue(0, overText);
    updateName.bindValue(1, targetId);
    QVERIFY2(updateName.exec(), qPrintable(updateName.lastError().text()));
    classes = classRepository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(!classes);
    QCOMPARE(classes.error().code, RepositoryErrorCode::Validation);
}

void ClassesNavigatorWorkspaceStoreTests::
repositoryNavigationAcceptsExactScheduleAndTokenBounds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("repository-schedule-exact.tps"));
    QVERIFY(!path.isEmpty());

    WorkspaceStore store;
    QVERIFY(store.openExisting(path));
    ClassRepository* const classRepository = store.classRepository();
    ClassInfoRepository* const infoRepository = store.classInfoRepository();
    QVERIFY(classRepository);
    QVERIFY(infoRepository);
    const auto classes = classRepository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(classes);
    QVERIFY(!classes->isEmpty());

    QSqlQuery targetQuery(store.database());
    QVERIFY(targetQuery.exec(QStringLiteral(
        "SELECT class_id FROM class_info ORDER BY class_id LIMIT 1")));
    QVERIFY(targetQuery.next());
    const int targetId = targetQuery.value(0).toInt();
    QVERIFY(targetId > 0);
    QSqlQuery removeRegular(store.database());
    QVERIFY(removeRegular.prepare(QStringLiteral(
        "DELETE FROM class_times WHERE class_id = ?")));
    removeRegular.addBindValue(targetId);
    QVERIFY2(removeRegular.exec(), qPrintable(removeRegular.lastError().text()));
    QSqlQuery removeIntensive(store.database());
    QVERIFY(removeIntensive.prepare(QStringLiteral(
        "DELETE FROM class_intensive_times WHERE class_id = ?")));
    removeIntensive.addBindValue(targetId);
    QVERIFY2(removeIntensive.exec(), qPrintable(removeIntensive.lastError().text()));

    const QString exactToken(
        static_cast<qsizetype>(
            Application::kClassesNavigationMaximumScheduleTokenLength),
        QLatin1Char('d'));
    for (const QString table : {
             QStringLiteral("class_times"),
             QStringLiteral("class_intensive_times")})
    {
        QSqlQuery insert(store.database());
        QVERIFY(insert.prepare(QStringLiteral(
            "INSERT INTO %1(class_id, day, start_time, end_time) "
            "VALUES(?, ?, ?, ?)" ).arg(table)));
        for (int row = 0; row < 14; ++row)
        {
            insert.bindValue(0, targetId);
            insert.bindValue(1, exactToken);
            insert.bindValue(2, QStringLiteral("09:%1").arg(row, 2, 10, QLatin1Char('0')));
            insert.bindValue(3, QStringLiteral("10:%1").arg(row, 2, 10, QLatin1Char('0')));
            QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
        }
    }

    const auto summaries = infoRepository->loadBoundedClassesNavigationRecords(
        repositoryRequestFor(*classes), repositoryReadLimits());
    QVERIFY(summaries);
    const auto summary = std::find_if(
        summaries->cbegin(), summaries->cend(), [targetId](const auto& item) {
            return item.details.classId == targetId;
        });
    QVERIFY(summary != summaries->cend());
    QCOMPARE(summary->details.regularTimes.size(), qsizetype(14));
    QCOMPARE(summary->details.intensiveTimes.size(), qsizetype(14));
    QCOMPARE(summary->details.regularTimes.front().day.size(), exactToken.size());
}

void ClassesNavigatorWorkspaceStoreTests::
repositoryNavigationRejectsScheduleOverflowAndIdentityChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("repository-schedule-overflow.tps"));
    QVERIFY(!path.isEmpty());

    WorkspaceStore store;
    QVERIFY(store.openExisting(path));
    ClassRepository* const classRepository = store.classRepository();
    ClassInfoRepository* const infoRepository = store.classInfoRepository();
    QVERIFY(classRepository);
    QVERIFY(infoRepository);
    const auto classes = classRepository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(classes);
    const auto request = repositoryRequestFor(*classes);
    auto reversed = request;
    std::reverse(reversed.begin(), reversed.end());
    auto changed = infoRepository->loadBoundedClassesNavigationRecords(
        reversed, repositoryReadLimits());
    QVERIFY(!changed);
    QCOMPARE(changed.error().code, RepositoryErrorCode::Validation);

    QList<ClassNavigationBoundedReadRequestEntry> tooMany;
    tooMany.reserve(4097);
    for (int index = 1; index <= 4097; ++index)
    {
        tooMany.append({
            .classId = index,
            .className = QStringLiteral("Class %1").arg(index)
        });
    }
    changed = infoRepository->loadBoundedClassesNavigationRecords(
        tooMany, repositoryReadLimits());
    QVERIFY(!changed);
    QCOMPARE(changed.error().code, RepositoryErrorCode::Validation);

    auto invalidId = request;
    invalidId.front().classId = 0;
    changed = infoRepository->loadBoundedClassesNavigationRecords(
        invalidId, repositoryReadLimits());
    QVERIFY(!changed);
    QCOMPARE(changed.error().code, RepositoryErrorCode::InvalidInput);

    const int targetId = classes->front().id;
    QSqlQuery remove(store.database());
    QVERIFY(remove.prepare(QStringLiteral(
        "DELETE FROM class_times WHERE class_id = ?")));
    remove.addBindValue(targetId);
    QVERIFY2(remove.exec(), qPrintable(remove.lastError().text()));
    QSqlQuery insert(store.database());
    QVERIFY(insert.prepare(QStringLiteral(
        "INSERT INTO class_times(class_id, day, start_time, end_time) "
        "VALUES(?, ?, ?, ?)")));
    for (int row = 0; row < 15; ++row)
    {
        insert.bindValue(0, targetId);
        insert.bindValue(1, QStringLiteral("Monday"));
        insert.bindValue(2, QStringLiteral("09:%1").arg(row, 2, 10, QLatin1Char('0')));
        insert.bindValue(3, QStringLiteral("10:%1").arg(row, 2, 10, QLatin1Char('0')));
        QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    }
    changed = infoRepository->loadBoundedClassesNavigationRecords(
        request, repositoryReadLimits());
    QVERIFY(!changed);
    QCOMPARE(changed.error().code, RepositoryErrorCode::Validation);

    QSqlQuery removeOverflow(store.database());
    QVERIFY(removeOverflow.prepare(QStringLiteral(
        "DELETE FROM class_times WHERE class_id = ?")));
    removeOverflow.addBindValue(targetId);
    QVERIFY2(removeOverflow.exec(), qPrintable(removeOverflow.lastError().text()));
    QSqlQuery insertLongToken(store.database());
    QVERIFY(insertLongToken.prepare(QStringLiteral(
        "INSERT INTO class_times(class_id, day, start_time, end_time) "
        "VALUES(?, ?, ?, ?)")));
    insertLongToken.addBindValue(targetId);
    insertLongToken.addBindValue(QString(
        static_cast<qsizetype>(
            Application::kClassesNavigationMaximumScheduleTokenLength + 1),
        QLatin1Char('d')));
    insertLongToken.addBindValue(QStringLiteral("09:00"));
    insertLongToken.addBindValue(QStringLiteral("09:50"));
    QVERIFY2(insertLongToken.exec(), qPrintable(insertLongToken.lastError().text()));
    changed = infoRepository->loadBoundedClassesNavigationRecords(
        request, repositoryReadLimits());
    QVERIFY(!changed);
    QCOMPARE(changed.error().code, RepositoryErrorCode::Validation);
}

void ClassesNavigatorWorkspaceStoreTests::
repositoryNavigationKeepsSchedulesUnderClassInfoParents()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = fixturePath(directory, QStringLiteral("repository-parent.tps"));
    QVERIFY(!path.isEmpty());

    WorkspaceStore store;
    QVERIFY(store.openExisting(path));
    QSqlQuery addClass(store.database());
    QVERIFY(addClass.prepare(QStringLiteral("INSERT INTO classes(name) VALUES(?)")));
    addClass.addBindValue(QStringLiteral("Class without information"));
    QVERIFY2(addClass.exec(), qPrintable(addClass.lastError().text()));
    const int classId = addClass.lastInsertId().toInt();
    QVERIFY(classId > 0);

    QSqlQuery addSchedule(store.database());
    QVERIFY(addSchedule.prepare(QStringLiteral(
        "INSERT INTO class_times(class_id, day, start_time, end_time) "
        "VALUES(?, ?, ?, ?)")));
    addSchedule.addBindValue(classId);
    addSchedule.addBindValue(QStringLiteral("Monday"));
    addSchedule.addBindValue(QStringLiteral("09:00"));
    addSchedule.addBindValue(QStringLiteral("09:50"));
    QVERIFY2(addSchedule.exec(), qPrintable(addSchedule.lastError().text()));

    ClassRepository* const classRepository = store.classRepository();
    ClassInfoRepository* const infoRepository = store.classInfoRepository();
    QVERIFY(classRepository);
    QVERIFY(infoRepository);
    const auto classes = classRepository->getBoundedClassesForNavigation(4096, 4096);
    QVERIFY(classes);
    const auto summaries = infoRepository->loadBoundedClassesNavigationRecords(
        repositoryRequestFor(*classes), repositoryReadLimits());
    QVERIFY(summaries);
    const auto summary = std::find_if(
        summaries->cbegin(), summaries->cend(), [classId](const auto& item) {
            return item.details.classId == classId;
        });
    QVERIFY(summary != summaries->cend());
    QVERIFY(!summary->details.hasClassInfo);
    QVERIFY(summary->details.regularTimes.isEmpty());
    QVERIFY(summary->details.intensiveTimes.isEmpty());
}

void ClassesNavigatorWorkspaceStoreTests::repositoryReadFailuresRemainTyped()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString listPath = fixturePath(directory, QStringLiteral("repository-list-failure.tps"));
    QVERIFY(!listPath.isEmpty());
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(listPath));
        QSqlQuery drop(store.database());
        QVERIFY2(drop.exec(QStringLiteral("DROP TABLE testing_classes")),
            qPrintable(drop.lastError().text()));
        ClassRepository* const repository = store.classRepository();
        QVERIFY(repository);
        const auto result = repository->getBoundedClassesForNavigation(4096, 4096);
        QVERIFY(!result);
        QCOMPARE(result.error().code, RepositoryErrorCode::Technical);
    }

    const QString navigationPath = fixturePath(
        directory, QStringLiteral("repository-navigation-failure.tps"));
    QVERIFY(!navigationPath.isEmpty());
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(navigationPath));
        ClassRepository* const classRepository = store.classRepository();
        ClassInfoRepository* const infoRepository = store.classInfoRepository();
        QVERIFY(classRepository);
        QVERIFY(infoRepository);
        const auto classes = classRepository->getBoundedClassesForNavigation(
            4096, 4096);
        QVERIFY(classes);
        QSqlQuery drop(store.database());
        QVERIFY2(drop.exec(QStringLiteral("DROP TABLE class_times")),
            qPrintable(drop.lastError().text()));
        const auto result = infoRepository->loadBoundedClassesNavigationRecords(
            repositoryRequestFor(*classes), repositoryReadLimits());
        QVERIFY(!result);
        QCOMPARE(result.error().code, RepositoryErrorCode::Technical);
    }
}

QTEST_GUILESS_MAIN(ClassesNavigatorWorkspaceStoreTests)
#include "classes_navigator_workspace_store_tests.moc"
