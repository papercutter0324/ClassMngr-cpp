#include "data/data_service.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/utils/file_name_utils.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "features/classes/services/class_transfer_json_codec.h"
#include "features/classes/ui/class_export_dialog.h"
#include "features/classes/ui/class_import_dialog.h"

#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QLabel>
#include <QJsonDocument>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTemporaryDir>
#include <QTimer>
#include <QMessageBox>
#include <QtTest>

#include <algorithm>
#include <memory>
#include <optional>

namespace
{
Teacher completeTeacher(
    const QString& englishName = QStringLiteral("Alex Kim")
    )
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("김알렉스");
    teacher.teacherEn = englishName;
    teacher.preferredRomanization = QStringLiteral("Gim Allekseu");
    teacher.preferredName = QStringLiteral("Gim Allekseu");
    teacher.roomNumber = QStringLiteral("504");
    teacher.birthday = QStringLiteral("02-29");
    teacher.phoneNumber = QStringLiteral("010-1234-5678");
    teacher.wifiName = QStringLiteral("Campus WiFi");
    teacher.wifiPassword = QStringLiteral("wifi-password");
    teacher.internetType = QStringLiteral("Both");
    teacher.zoomId = QStringLiteral("123 456 7890");
    teacher.zoomPassword = QStringLiteral("zoom-password");
    teacher.projectionType = QStringLiteral("Any");
    teacher.notes = QStringLiteral("Teacher notes\nwith a second line.");
    return teacher;
}

int createdTeacherId(DataService& service, const Teacher& teacher)
{
    return service.createTeacher(teacher).value_or(-1);
}

int createdClassId(DataService& service, const QString& name)
{
    return service.createClass(name).value_or(-1);
}

std::unique_ptr<ApplicationServices> openApplicationServicesForCurrentDatabase(
    DataService& dataService,
    QString* error
    )
{
    auto applicationServices = std::make_unique<ApplicationServices>();
    const Status opened = applicationServices->openDatabase(
        dataService.currentDatabasePath());
    if (!opened)
    {
        if (error)
        {
            *error = opened.error();
        }
        return {};
    }

    return applicationServices;
}

ClassInfo completeClassInfo(
    int classId,
    int teacherId,
    const QString& grade,
    const QString& level,
    const QString& day,
    const QString& startTime = QStringLiteral("4:00 PM"),
    const QString& endTime = QStringLiteral("4:50 PM")
    )
{
    ClassInfo info;
    info.classId = classId;
    info.teacherId = teacherId;
    info.classGrade = grade;
    info.classLevel = level;
    info.readingBook = QStringLiteral("Reading Explorer 3");
    info.essayBook = QStringLiteral("4C");
    info.classColor = QStringLiteral("#123456");
    info.fontColor = QStringLiteral("#FEDCBA");
    info.notes = QStringLiteral("수업 노트\nSecond line");
    info.timeFillerActivities = QStringLiteral("Word chain");
    info.classTimes.append({day, startTime, endTime});
    info.intensiveTimes.append({
        day,
        QStringLiteral("10:00 AM"),
        QStringLiteral("10:55 AM")
    });
    return info;
}

Roster completeRoster(
    const QString& studentName
    )
{
    Roster roster;
    roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean"),
        QStringLiteral("Memo")
    };
    roster.columnWidths = {180, 190, 240};
    roster.rows.append({
        studentName,
        QStringLiteral("학생"),
        QStringLiteral("Needs extra practice")
    });
    roster.rows.append({QString(), QString(), QString()});
    return roster;
}

SpeakingEvalRows completeEvaluation(
    const QString& studentName
    )
{
    SpeakingEvalRows rows = SpeakingEval::emptyRows();
    rows[0][SpeakingEval::toInt(
        SpeakingEvalColumn::EnglishName)] = studentName;
    rows[0][SpeakingEval::toInt(
        SpeakingEvalColumn::KoreanName)] = QStringLiteral("학생");
    rows[0][SpeakingEval::toInt(
        SpeakingEvalColumn::Grammar)] = QStringLiteral("A");
    rows[0][SpeakingEval::toInt(
        SpeakingEvalColumn::Comments)] = QStringLiteral("Excellent progress.");
    return rows;
}

SpeakingEvalRows expectedFixtureEvaluationRows()
{
    SpeakingEvalRows rows = SpeakingEval::emptyRows();
    rows[0] = {
        QString(),
        QStringLiteral("Avery"),
        QStringLiteral("Fixture student"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("B"),
        QStringLiteral("A"),
        QStringLiteral("C"),
        QStringLiteral("B+"),
        QStringLiteral("Fixture evaluation comment"),
        QStringLiteral("Fixture evaluation note")
    };
    return rows;
}

int addCompleteClass(
    DataService& service,
    int teacherId,
    const QString& storedName,
    const QString& grade,
    const QString& level,
    const QString& day,
    const QString& studentName,
    const QString& evaluationName = QStringLiteral("Custom Evaluation"),
    const QString& startTime = QStringLiteral("4:00 PM"),
    const QString& endTime = QStringLiteral("4:50 PM")
    )
{
    const int classId = createdClassId(service, storedName);
    const ClassInfo info = completeClassInfo(
        classId, teacherId, grade, level, day, startTime, endTime);
    if (!service.saveClassInfo(info))
    {
        return -1;
    }

    if (!service.saveRoster(classId, completeRoster(studentName)))
    {
        return -1;
    }

    if (!service.saveSpeakingEval(
            classId,
            evaluationName,
            completeEvaluation(studentName)))
    {
        return -1;
    }

    return classId;
}

ClassImportPlan createAllPlan(
    const ClassTransferPackage& package
    )
{
    ClassImportPlan plan;

    for (int index = 0; index < package.classes.size(); ++index)
    {
        plan.classes.append({index, ClassImportAction::Create, -1});
    }

    for (const ClassTransferTeacher& teacher : package.teachers)
    {
        plan.teachers.append({
            teacher.key, TeacherImportAction::Create, -1});
    }

    return plan;
}

QString loadReviewFontFamily()
{
    const QString fontPath =
        QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
            QStringLiteral("resources/assets/fonts/Inter.ttc")
            );
    const int fontId = QFontDatabase::addApplicationFont(fontPath);
    if (fontId < 0)
    {
        return {};
    }

    const QStringList families =
        QFontDatabase::applicationFontFamilies(fontId);
    return families.isEmpty() ? QString() : families.first();
}

std::optional<QByteArray> persistedDatabaseSnapshot(
    const QSqlDatabase& database,
    QString* error
    )
{
    QSqlQuery tableQuery(database);
    if (!tableQuery.exec(QStringLiteral(
            "SELECT name FROM sqlite_master "
            "WHERE type = 'table' "
            "AND (name NOT LIKE 'sqlite_%' OR name = 'sqlite_sequence') "
            "ORDER BY name")))
    {
        *error = tableQuery.lastError().text();
        return std::nullopt;
    }

    QJsonObject tables;
    while (tableQuery.next())
    {
        const QString tableName = tableQuery.value(0).toString();
        QString quotedTableName = tableName;
        quotedTableName.replace(QStringLiteral("\""), QStringLiteral("\"\""));

        QSqlQuery rowQuery(database);
        if (!rowQuery.exec(
                QStringLiteral("SELECT * FROM \"%1\"")
                    .arg(quotedTableName)))
        {
            *error = rowQuery.lastError().text();
            return std::nullopt;
        }

        QList<QByteArray> serializedRows;
        while (rowQuery.next())
        {
            QJsonArray row;
            for (int column = 0; column < rowQuery.record().count(); ++column)
            {
                const QVariant value = rowQuery.value(column);
                QJsonObject cell;
                cell.insert(
                    QStringLiteral("type"),
                    QString::fromLatin1(value.metaType().name()));
                if (value.isNull())
                {
                    cell.insert(QStringLiteral("value"), QJsonValue::Null);
                }
                else if (value.metaType().id() == QMetaType::QByteArray)
                {
                    cell.insert(
                        QStringLiteral("value"),
                        QStringLiteral("base64:%1").arg(
                            QString::fromLatin1(value.toByteArray().toBase64())));
                }
                else
                {
                    cell.insert(QStringLiteral("value"), value.toString());
                }
                row.append(cell);
            }
            serializedRows.append(
                QJsonDocument(row).toJson(QJsonDocument::Compact));
        }

        std::sort(serializedRows.begin(), serializedRows.end());
        QJsonArray rows;
        for (const QByteArray& serializedRow : serializedRows)
        {
            rows.append(QJsonDocument::fromJson(serializedRow).array());
        }
        tables.insert(tableName, rows);
    }

    return QJsonDocument(tables).toJson(QJsonDocument::Compact);
}

std::optional<qlonglong> sqliteTotalChanges(
    const QSqlDatabase& database,
    QString* error
    )
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT total_changes()"))
        || !query.next())
    {
        *error = query.lastError().text();
        return std::nullopt;
    }
    return query.value(0).toLongLong();
}
}

class ClassTransferTests : public QObject
{
    Q_OBJECT

private slots:
    void jsonRoundTripPreservesCompletePackage();
    void exportPreservesSelectedClassAndSparseEvaluationRowOrder();
    void exportSkipsRowQueryWhenNoEvaluationsExist();
    void exportFailsWhenEvaluationRowsCannotBeRead();
    void importsCompleteClassesAndDeduplicatesTeacher();
    void previewMatchesCourseAndTeacherIgnoringSchedule();
    void previewPreservesQtNameAndCourseNormalization();
    void replacementRetainsIdAndClearsOldChildren();
    void teacherReplacementImportsCompleteSnapshot();
    void scheduleConflictLeavesDestinationUnchanged();
    void schedulePreflightParsesSundayOvernightAndEqualEndpoints();
    void importedClassesConflictAtomically();
    void databaseFailureRollsBackAllWrites();
    void incompleteCourseSignatureDoesNotMatch();
    void codecRejectsMalformedAndUnsupportedPackages();
    void requiredSuccessFixtureTraversesReviewAndPersistsResults();
    void successFixtureReplacesMatchingTeacherThroughReview();
    void successFixtureReplacesMatchingDestinationAndChildren();
    void successFixtureClassReplacementMatchesCommonInputState();
    void importDialogBatchesDistinctDestinationSubtitleReads();
    void malformedNonpositiveReviewTargetsAreRejectedAtLegacyBoundary();
    void permanentConflictFixturePresentsReviewAndRejectsScheduleCollision();
    void conflictFixtureMatchesCommonInputBaselineAndRejectsWithoutWrites();
    void dialogRejectsDuplicateReplacementTargets();
    void applyRejectsReplacementOutsideCurrentPreviewMatches();
    void exportDialogStartsClearAndSortsClassesAlphabetically();
    void exportDialogSkipsSubtitleBatchReadForEmptyClassList();
    void exportDialogShowsWarningAndStaysEmptyWhenClassListCannotLoad();
    void exportDialogUsesDefaultFormattingWhenClassFieldsCannotLoad();
    void exportDialogKeepsClassFieldsWhenTeacherCannotLoad();
    void filesystemSafeJsonFileName();
    void importDialogRequiresAmbiguousTeacherResolution();
    void importDialogBatchesDistinctMatchedTeacherDisplayNameReads();
    void importDialogFallsBackToIndividualTeacherProfilesAfterBatchFailure();
    void importDialogUsesDefaultFormattingWhenClassFieldsCannotLoad();
    void importDialogKeepsClassFieldsWhenTeacherCannotLoad();
    void importDialogUsesNewTeacherLabelWhenTeacherProfileCannotLoad();
};

void ClassTransferTests::jsonRoundTripPreservesCompletePackage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ClassTransferPackage package;
    package.exportedAtUtc = QDateTime::fromString(
        QStringLiteral("2026-07-20T12:34:56.789Z"), Qt::ISODateWithMs);
    package.teachers.append({QStringLiteral("teacher-1"), completeTeacher()});

    ClassTransferClass transferClass;
    transferClass.key = QStringLiteral("class-1");
    transferClass.name = QStringLiteral("Stored 이름");
    transferClass.teacherKey = QStringLiteral("teacher-1");
    transferClass.info = completeClassInfo(
        -1,
        -1,
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Monday")
        );
    transferClass.roster = completeRoster(QStringLiteral("Jamie"));
    transferClass.evaluations.append({
        QStringLiteral("Arbitrary Evaluation Name"),
        completeEvaluation(QStringLiteral("Jamie"))
    });
    package.classes.append(transferClass);

    const QString configuredFixturePath =
        qEnvironmentVariable(
            "CLASSMNGR_CLASS_TRANSFER_FIXTURE_OUTPUT_PATH"
            ).trimmed();
    const QString path =
        configuredFixturePath.isEmpty()
            ? directory.filePath(
                  QStringLiteral("roundtrip.classmngr-classes.json"))
            : configuredFixturePath;
    if (!configuredFixturePath.isEmpty())
    {
        QVERIFY2(
            QDir().mkpath(QFileInfo(path).absolutePath()),
            qPrintable(
                QStringLiteral("Could not create transfer fixture directory for %1")
                    .arg(path)
                )
            );
    }
    QVERIFY(ClassTransferJsonCodec::saveFile(path, package).has_value());

    const auto loaded = ClassTransferJsonCodec::loadFile(path);
    QVERIFY2(loaded.has_value(),
             loaded ? "" : qPrintable(loaded.error()));
    QCOMPARE(loaded->version, ClassTransferPackage::CurrentVersion);
    QCOMPARE(loaded->teachers.size(), 1);
    QCOMPARE(loaded->classes.size(), 1);
    QCOMPARE(loaded->teachers.first().teacher.wifiPassword,
             QStringLiteral("wifi-password"));
    QCOMPARE(loaded->teachers.first().teacher.preferredRomanization,
             QStringLiteral("Gim Allekseu"));
    QCOMPARE(loaded->teachers.first().teacher.preferredName,
             QStringLiteral("Gim Allekseu"));
    QCOMPARE(loaded->teachers.first().teacher.birthday,
             QStringLiteral("02-29"));
    QCOMPARE(loaded->teachers.first().teacher.phoneNumber,
             QStringLiteral("010-1234-5678"));
    QCOMPARE(loaded->teachers.first().teacher.zoomPassword,
             QStringLiteral("zoom-password"));
    QCOMPARE(loaded->teachers.first().teacher.notes,
             QStringLiteral("Teacher notes\nwith a second line."));
    QCOMPARE(loaded->classes.first().name, QStringLiteral("Stored 이름"));
    QCOMPARE(loaded->classes.first().info.notes,
             QStringLiteral("수업 노트\nSecond line"));
    QCOMPARE(loaded->classes.first().info.classTimes.size(), 1);
    QCOMPARE(loaded->classes.first().info.intensiveTimes.size(), 1);
    QCOMPARE(loaded->classes.first().roster.columnWidths,
             QVector<int>({180, 190, 240}));
    QCOMPARE(loaded->classes.first().evaluations.first().name,
             QStringLiteral("Arbitrary Evaluation Name"));
    QCOMPARE(
        loaded->classes.first().evaluations.first().rows[0][
            SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)],
        QStringLiteral("Jamie")
        );

    QJsonObject withUnknownField = ClassTransferJsonCodec::toJson(package);
    withUnknownField.insert(QStringLiteral("future_field"), 42);
    QVERIFY(ClassTransferJsonCodec::fromJson(withUnknownField).has_value());

    QJsonObject legacyJson = ClassTransferJsonCodec::toJson(package);
    QJsonArray legacyTeachers =
        legacyJson.value(QStringLiteral("teachers")).toArray();
    QJsonObject legacyTeacher = legacyTeachers.first().toObject();
    legacyTeacher.remove(QStringLiteral("preferred_romanization"));
    legacyTeacher.remove(QStringLiteral("preferred_name"));
    legacyTeacher.remove(QStringLiteral("birthday"));
    legacyTeacher.remove(QStringLiteral("phone_number"));
    legacyTeachers.replace(0, legacyTeacher);
    legacyJson.insert(QStringLiteral("teachers"), legacyTeachers);

    const auto legacyPackage =
        ClassTransferJsonCodec::fromJson(legacyJson);
    QVERIFY(legacyPackage.has_value());
    QVERIFY(
        legacyPackage->teachers.first().teacher
            .preferredRomanization.isEmpty()
        );
    QVERIFY(legacyPackage->teachers.first().teacher.preferredName.isEmpty());
    QVERIFY(legacyPackage->teachers.first().teacher.birthday.isEmpty());
    QVERIFY(legacyPackage->teachers.first().teacher.phoneNumber.isEmpty());
}

void ClassTransferTests::
    exportPreservesSelectedClassAndSparseEvaluationRowOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());

    const int teacherId = createdTeacherId(service, completeTeacher());
    const int firstClassId = addCompleteClass(
        service,
        teacherId,
        QStringLiteral("First Class"),
        QStringLiteral("E4"),
        QStringLiteral("Orion"),
        QStringLiteral("Monday"),
        QStringLiteral("First Student"),
        QStringLiteral("First Evaluation")
        );
    const int secondClassId = addCompleteClass(
        service,
        teacherId,
        QStringLiteral("Second Class"),
        QStringLiteral("E5"),
        QStringLiteral("Pegasus"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Second Student"),
        QStringLiteral("Second Class Evaluation")
        );
    QVERIFY(firstClassId > 0);
    QVERIFY(secondClassId > 0);

    QSqlDatabase database = service.databaseSession()->database();
    auto evaluationIdFor = [&](int classId, const QString& name)
    {
        QSqlQuery query(database);
        query.prepare(R"(
            SELECT id
            FROM speaking_evaluations
            WHERE class_id=? AND evaluation_name=?
        )");
        query.addBindValue(classId);
        query.addBindValue(name);
        if (!query.exec() || !query.next())
        {
            return -1;
        }
        return query.value(0).toInt();
    };
    auto insertEvaluation = [&](int classId, const QString& name)
    {
        QSqlQuery query(database);
        query.prepare(R"(
            INSERT INTO speaking_evaluations (class_id, evaluation_name)
            VALUES (?, ?)
        )");
        query.addBindValue(classId);
        query.addBindValue(name);
        if (!query.exec())
        {
            return -1;
        }
        return query.lastInsertId().toInt();
    };
    auto clearEvaluationRows = [&](int evaluationId)
    {
        QSqlQuery query(database);
        query.prepare(
            "DELETE FROM speaking_eval_data WHERE evaluation_id=?");
        query.addBindValue(evaluationId);
        return query.exec();
    };
    auto insertEvaluationRow = [&](
        int evaluationId,
        int rowIndex,
        const QStringList& values,
        int nullColumn = -1
        )
    {
        QStringList columns{
            QStringLiteral("evaluation_id"), QStringLiteral("row_index")};
        QStringList placeholders{QStringLiteral("?"), QStringLiteral("?")};
        for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
        {
            columns.append(QStringLiteral("col_%1").arg(column));
            placeholders.append(
                column == nullColumn ? QStringLiteral("NULL")
                                     : QStringLiteral("?"));
        }

        QSqlQuery query(database);
        query.prepare(
            QStringLiteral("INSERT INTO speaking_eval_data (%1) VALUES (%2)")
                .arg(columns.join(QStringLiteral(", ")),
                     placeholders.join(QStringLiteral(", "))));
        query.addBindValue(evaluationId);
        query.addBindValue(rowIndex);
        for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
        {
            if (column != nullColumn)
            {
                query.addBindValue(values.value(column));
            }
        }
        return query.exec();
    };

    const int firstEvaluationId = evaluationIdFor(
        firstClassId, QStringLiteral("First Evaluation"));
    const int secondClassEvaluationId = evaluationIdFor(
        secondClassId, QStringLiteral("Second Class Evaluation"));
    const int secondEvaluationId = insertEvaluation(
        firstClassId, QStringLiteral("Second Evaluation"));
    QVERIFY(firstEvaluationId > 0);
    QVERIFY(secondClassEvaluationId > 0);
    QVERIFY(secondEvaluationId > firstEvaluationId);
    QVERIFY(clearEvaluationRows(firstEvaluationId));
    QVERIFY(clearEvaluationRows(secondClassEvaluationId));

    QStringList rowTwoValues;
    QStringList rowTwentyFourValues;
    for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
    {
        rowTwoValues.append(QStringLiteral("row2-col%1").arg(column));
        rowTwentyFourValues.append(
            QStringLiteral("row24-col%1").arg(column));
    }
    QVERIFY(insertEvaluationRow(firstEvaluationId, 2, rowTwoValues));
    QVERIFY(insertEvaluationRow(
        firstEvaluationId, 24, rowTwentyFourValues, 6));
    QSqlQuery ignoreCheckConstraints(database);
    QVERIFY(ignoreCheckConstraints.exec(
        QStringLiteral("PRAGMA ignore_check_constraints=ON")));
    QVERIFY(insertEvaluationRow(
        firstEvaluationId, -1,
        QStringList(SpeakingEval::ColumnCount, QStringLiteral("ignored"))));
    QVERIFY(ignoreCheckConstraints.exec(
        QStringLiteral("PRAGMA ignore_check_constraints=OFF")));
    QVERIFY(insertEvaluationRow(
        firstEvaluationId, SpeakingEval::RowCount,
        QStringList(SpeakingEval::ColumnCount, QStringLiteral("ignored"))));

    const auto package = service.buildClassTransferPackage(
        {secondClassId, firstClassId});
    QVERIFY2(package.has_value(),
             package ? "" : qPrintable(package.error()));
    QCOMPARE(package->classes.size(), 2);
    QCOMPARE(package->classes[0].name, QStringLiteral("Second Class"));
    QCOMPARE(package->classes[1].name, QStringLiteral("First Class"));

    const QList<ClassTransferEvaluation>& secondClassEvaluations =
        package->classes[0].evaluations;
    QCOMPARE(secondClassEvaluations.size(), 1);
    QCOMPARE(secondClassEvaluations[0].name,
             QStringLiteral("Second Class Evaluation"));
    QCOMPARE(secondClassEvaluations[0].rows.size(), SpeakingEval::RowCount);
    for (const QStringList& row : secondClassEvaluations[0].rows)
    {
        QCOMPARE(row.size(), SpeakingEval::ColumnCount);
        for (const QString& cell : row)
        {
            QVERIFY(cell.isEmpty());
        }
    }

    const QList<ClassTransferEvaluation>& firstClassEvaluations =
        package->classes[1].evaluations;
    QCOMPARE(firstClassEvaluations.size(), 2);
    QCOMPARE(firstClassEvaluations[0].name,
             QStringLiteral("First Evaluation"));
    QCOMPARE(firstClassEvaluations[1].name,
             QStringLiteral("Second Evaluation"));
    QCOMPARE(firstClassEvaluations[0].rows.size(), SpeakingEval::RowCount);
    for (const int rowIndex : {0, 1, 3, 4, 23})
    {
        QCOMPARE(
            firstClassEvaluations[0].rows[rowIndex],
            QStringList(SpeakingEval::ColumnCount, QString()));
    }
    QCOMPARE(firstClassEvaluations[0].rows[2], rowTwoValues);
    rowTwentyFourValues[6].clear();
    QCOMPARE(firstClassEvaluations[0].rows[24], rowTwentyFourValues);
    QCOMPARE(firstClassEvaluations[1].rows.size(), SpeakingEval::RowCount);
    for (const QStringList& row : firstClassEvaluations[1].rows)
    {
        QCOMPARE(row.size(), SpeakingEval::ColumnCount);
        for (const QString& cell : row)
        {
            QVERIFY(cell.isEmpty());
        }
    }
}

void ClassTransferTests::exportSkipsRowQueryWhenNoEvaluationsExist()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());

    const int teacherId = createdTeacherId(service, completeTeacher());
    const int classId = createdClassId(
        service, QStringLiteral("Empty Evaluation Export"));
    QVERIFY(classId > 0);
    QVERIFY(service.saveClassInfo(completeClassInfo(
        classId,
        teacherId,
        QStringLiteral("E4"),
        QStringLiteral("Orion"),
        QStringLiteral("Monday")
        )));
    QVERIFY(service.saveRoster(
        classId, completeRoster(QStringLiteral("Student"))));

    QSqlQuery dropRows(service.databaseSession()->database());
    QVERIFY2(dropRows.exec(QStringLiteral("DROP TABLE speaking_eval_data")),
             qPrintable(dropRows.lastError().text()));

    const auto package = service.buildClassTransferPackage({classId});
    QVERIFY2(package.has_value(),
             package ? "" : qPrintable(package.error()));
    QCOMPARE(package->classes.size(), 1);
    QCOMPARE(package->classes.first().name,
             QStringLiteral("Empty Evaluation Export"));
    QVERIFY(package->classes.first().evaluations.isEmpty());
}

void ClassTransferTests::exportFailsWhenEvaluationRowsCannotBeRead()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());

    const int teacherId = createdTeacherId(service, completeTeacher());
    const int classId = addCompleteClass(
        service,
        teacherId,
        QStringLiteral("Export Class"),
        QStringLiteral("E4"),
        QStringLiteral("Orion"),
        QStringLiteral("Monday"),
        QStringLiteral("Student"),
        QStringLiteral("Evaluation")
        );
    QVERIFY(classId > 0);

    QSqlQuery dropRows(service.databaseSession()->database());
    QVERIFY2(dropRows.exec(QStringLiteral("DROP TABLE speaking_eval_data")),
             qPrintable(dropRows.lastError().text()));

    const auto package = service.buildClassTransferPackage({classId});
    QVERIFY(!package.has_value());
    QVERIFY(package.error().contains(
        QStringLiteral("Unable to read speaking evaluation rows:")));
}

void ClassTransferTests::importsCompleteClassesAndDeduplicatesTeacher()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());

    const int teacherId = createdTeacherId(service, completeTeacher());
    const int firstClassId = addCompleteClass(
        service,
        teacherId,
        QStringLiteral("Source A"),
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Monday"),
        QStringLiteral("Jamie")
        );
    const int secondClassId = addCompleteClass(
        service,
        teacherId,
        QStringLiteral("Source B"),
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Morgan")
        );
    QVERIFY(firstClassId > 0);
    QVERIFY(secondClassId > 0);

    const auto package = service.buildClassTransferPackage(
        {firstClassId, secondClassId});
    QVERIFY2(package.has_value(),
             package ? "" : qPrintable(package.error()));
    QCOMPARE(package->teachers.size(), 1);
    QCOMPARE(package->classes.size(), 2);

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const auto summary = service.importClasses(
        *package, createAllPlan(*package));
    QVERIFY2(summary.has_value(),
             summary ? "" : qPrintable(summary.error()));
    QCOMPARE(summary->createdClassIds.size(), 2);
    QCOMPARE(summary->replacedClassIds.size(), 0);
    QCOMPARE(service.getAllTeachers().value_or(QList<Teacher>{}).size(), 1);

    const int importedFirstId = summary->createdClassIds.first();
    const int importedSecondId = summary->createdClassIds.last();
    QVERIFY(importedFirstId != firstClassId || importedSecondId != secondClassId
            || service.currentDatabasePath().endsWith(
                QStringLiteral("destination.db")));
    const Result<ClassInfo> importedFirst = service.loadClassInfo(importedFirstId);
    const Result<ClassInfo> importedSecond = service.loadClassInfo(importedSecondId);
    QVERIFY(importedFirst);
    QVERIFY(importedSecond);
    QCOMPARE(importedFirst->teacherId, importedSecond->teacherId);
    QCOMPARE(importedFirst->notes, QStringLiteral("수업 노트\nSecond line"));
    QCOMPARE(importedFirst->classColor, QStringLiteral("#123456"));
    QCOMPARE(service.getTeacher(importedFirst->teacherId)
                 .value_or(Teacher{}).wifiPassword,
             QStringLiteral("wifi-password"));
    QCOMPARE(service.getTeacher(importedFirst->teacherId)
                 .value_or(Teacher{}).birthday,
             QStringLiteral("02-29"));
    QCOMPARE(service.loadRoster(importedFirstId)->rows.first().first(),
             QStringLiteral("Jamie"));
    QCOMPARE(
        (*service.loadSpeakingEval(
            importedFirstId, QStringLiteral("Custom Evaluation")))[0][
                SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)],
        QStringLiteral("Jamie")
        );
}

void ClassTransferTests::previewMatchesCourseAndTeacherIgnoringSchedule()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = createdTeacherId(service, completeTeacher());
    const int sourceClass = addCompleteClass(
        service,
        sourceTeacher,
        QString(),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Monday"),
        QStringLiteral("Jamie")
        );
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const int destinationTeacher = createdTeacherId(service, completeTeacher());
    const int destinationClass = addCompleteClass(
        service,
        destinationTeacher,
        QString(),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Friday"),
        QStringLiteral("Old Student")
        );

    const auto preview = service.previewClassImport(*package);
    QVERIFY(preview.has_value());
    QCOMPARE(preview->teachers.size(), 1);
    QCOMPARE(preview->teachers.first().matchingTeacherIds,
             QList<int>({destinationTeacher}));
    QCOMPARE(preview->classes.size(), 1);
    QCOMPARE(preview->classes.first().matchingClassIds,
             QList<int>({destinationClass}));
}

void ClassTransferTests::replacementRetainsIdAndClearsOldChildren()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = createdTeacherId(service, completeTeacher());
    const int sourceClass = addCompleteClass(
        service,
        sourceTeacher,
        QStringLiteral("Imported Stored Name"),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Monday"),
        QStringLiteral("New Student"),
        QStringLiteral("Imported Evaluation")
        );
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    Teacher localTeacher = completeTeacher();
    localTeacher.wifiPassword = QStringLiteral("local-password");
    const int destinationTeacher = createdTeacherId(service, localTeacher);
    const int destinationClass = addCompleteClass(
        service,
        destinationTeacher,
        QStringLiteral("Old Stored Name"),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Monday"),
        QStringLiteral("Old Student"),
        QStringLiteral("Destination Only Evaluation")
        );

    ClassImportPlan plan;
    plan.classes.append({
        0, ClassImportAction::Replace, destinationClass});
    plan.teachers.append({
        package->teachers.first().key,
        TeacherImportAction::KeepExisting,
        destinationTeacher
    });

    const auto summary = service.importClasses(*package, plan);
    QVERIFY2(summary.has_value(),
             summary ? "" : qPrintable(summary.error()));
    QCOMPARE(summary->replacedClassIds,
             QList<int>({destinationClass}));
    QCOMPARE(service.getClasses().value_or(QList<Classroom>{}).size(), 1);
    QCOMPARE(service.getClassById(destinationClass)
                 .value_or(Classroom{}).name,
             QStringLiteral("Imported Stored Name"));
    QCOMPARE(service.loadClassInfo(destinationClass)->classTimes.first().day,
             QStringLiteral("Monday"));
    QCOMPARE(service.loadRoster(destinationClass)->rows.first().first(),
             QStringLiteral("New Student"));
    QVERIFY(service.loadSpeakingEval(
        destinationClass,
        QStringLiteral("Destination Only Evaluation"))->isEmpty());
    QVERIFY(!service.loadSpeakingEval(
        destinationClass,
        QStringLiteral("Imported Evaluation"))->isEmpty());
    QCOMPARE(service.getTeacher(destinationTeacher)
                 .value_or(Teacher{}).wifiPassword,
             QStringLiteral("local-password"));
}

void ClassTransferTests::teacherReplacementImportsCompleteSnapshot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = createdTeacherId(service, completeTeacher());
    const int sourceClass = addCompleteClass(
        service,
        sourceTeacher,
        QString(),
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QStringLiteral("Monday"),
        QStringLiteral("Jamie")
        );
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    Teacher localTeacher = completeTeacher();
    localTeacher.wifiPassword = QStringLiteral("outdated");
    localTeacher.notes = QStringLiteral("Outdated notes");
    const int destinationTeacher = createdTeacherId(service, localTeacher);
    const int destinationClass = addCompleteClass(
        service,
        destinationTeacher,
        QString(),
        QStringLiteral("E5"),
        QStringLiteral("Apollo"),
        QStringLiteral("Friday"),
        QStringLiteral("Old")
        );

    ClassImportPlan plan;
    plan.classes.append({
        0, ClassImportAction::Replace, destinationClass});
    plan.teachers.append({
        package->teachers.first().key,
        TeacherImportAction::ReplaceExisting,
        destinationTeacher
    });
    const auto summary = service.importClasses(*package, plan);
    QVERIFY2(summary.has_value(),
             summary ? "" : qPrintable(summary.error()));
    QCOMPARE(service.getTeacher(destinationTeacher)
                 .value_or(Teacher{}).wifiPassword,
             QStringLiteral("wifi-password"));
    QCOMPARE(
        service.getTeacher(destinationTeacher)
            .value_or(Teacher{}).preferredRomanization,
        QStringLiteral("Gim Allekseu")
        );
    QCOMPARE(service.getTeacher(destinationTeacher)
                 .value_or(Teacher{}).phoneNumber,
             QStringLiteral("010-1234-5678"));
    QCOMPARE(service.getTeacher(destinationTeacher)
                 .value_or(Teacher{}).notes,
             QStringLiteral("Teacher notes\nwith a second line."));
}

void ClassTransferTests::scheduleConflictLeavesDestinationUnchanged()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = createdTeacherId(
        service, completeTeacher(QStringLiteral("Source Teacher")));
    const int sourceClass = addCompleteClass(
        service,
        sourceTeacher,
        QString(),
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Monday"),
        QStringLiteral("Source Student")
        );
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const int destinationTeacher = createdTeacherId(
        service, completeTeacher(QStringLiteral("Destination Teacher")));
    const int destinationClass = addCompleteClass(
        service,
        destinationTeacher,
        QString(),
        QStringLiteral("E6"),
        QStringLiteral("Gaia"),
        QStringLiteral("Monday"),
        QStringLiteral("Destination Student")
        );
    QVERIFY(destinationClass > 0);
    const int teachersBefore = service.getAllTeachers()
        .value_or(QList<Teacher>{}).size();
    const int classesBefore = service.getClasses()
        .value_or(QList<Classroom>{}).size();

    const auto result = service.importClasses(
        *package, createAllPlan(*package));
    QVERIFY(!result.has_value());
    QVERIFY(result.error().contains(QStringLiteral("Schedule conflicts")));
    QCOMPARE(service.getAllTeachers().value_or(QList<Teacher>{}).size(),
             teachersBefore);
    QCOMPARE(service.getClasses().value_or(QList<Classroom>{}).size(),
             classesBefore);
    QCOMPARE(service.loadRoster(destinationClass)->rows.first().first(),
             QStringLiteral("Destination Student"));
}

void ClassTransferTests::
    schedulePreflightParsesSundayOvernightAndEqualEndpoints()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = createdTeacherId(
        service, completeTeacher(QStringLiteral("Source Teacher")));
    const int sourceClass = addCompleteClass(
        service,
        sourceTeacher,
        QString(),
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Monday"),
        QStringLiteral("Source Student")
        );
    const auto sourcePackage = service.buildClassTransferPackage({sourceClass});
    QVERIFY(sourcePackage.has_value());
    ClassTransferPackage package = *sourcePackage;
    package.classes.first().info.classTimes.first() = {
        QStringLiteral("Sunday"),
        QStringLiteral(" 11:00 pm "),
        QStringLiteral("1:00 AM")
    };
    package.classes.first().info.intensiveTimes.first().day =
        QStringLiteral("Tuesday");

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const int destinationTeacher = createdTeacherId(
        service, completeTeacher(QStringLiteral("Destination Teacher")));
    const int destinationClass = addCompleteClass(
        service,
        destinationTeacher,
        QStringLiteral("Destination Class"),
        QStringLiteral("E6"),
        QStringLiteral("Gaia"),
        QStringLiteral("Monday"),
        QStringLiteral("Destination Student")
        );
    QVERIFY(destinationClass > 0);

    Result<ClassInfo> destinationInfo = service.loadClassInfo(destinationClass);
    QVERIFY(destinationInfo);
    destinationInfo->classTimes.first() = {
        QStringLiteral("Monday"),
        QStringLiteral("12:30 am"),
        QStringLiteral("1:30 AM")
    };
    destinationInfo->intensiveTimes.first().day = QStringLiteral("Wednesday");
    QVERIFY(service.saveClassInfo(*destinationInfo));

    const int teachersBefore = service.getAllTeachers()
        .value_or(QList<Teacher>{}).size();
    const int classesBefore = service.getClasses()
        .value_or(QList<Classroom>{}).size();
    const auto overnightResult = service.importClasses(
        package,
        createAllPlan(package)
        );
    QVERIFY(!overnightResult.has_value());
    QCOMPARE(
        overnightResult.error(),
        QStringLiteral(
            "Schedule conflicts prevent this import:\n\n"
            "Regular schedule: E4 Theseus \u2014 Sunday  11:00 pm \u20131:00 AM "
            "conflicts with E6 Gaia \u2014 Monday 12:30 am\u20131:30 AM"
            )
        );
    QCOMPARE(service.getAllTeachers().value_or(QList<Teacher>{}).size(),
             teachersBefore);
    QCOMPARE(service.getClasses().value_or(QList<Classroom>{}).size(),
             classesBefore);
    QCOMPARE(service.loadClassInfo(destinationClass)->classTimes.first().day,
             QStringLiteral("Monday"));
    QCOMPARE(service.loadClassInfo(destinationClass)->classTimes.first().startTime,
             QStringLiteral("12:30 am"));

    package.classes.first().info.classTimes.first() = {
        QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"),
        QStringLiteral("4:00 PM")
    };
    package.classes.first().info.intensiveTimes.first().day =
        QStringLiteral("Friday");
    destinationInfo = service.loadClassInfo(destinationClass);
    QVERIFY(destinationInfo);
    destinationInfo->classTimes.first() = {
        QStringLiteral("Tuesday"),
        QStringLiteral("3:30 PM"),
        QStringLiteral("4:30 PM")
    };
    QVERIFY(service.saveClassInfo(*destinationInfo));

    const auto equalEndpointResult = service.importClasses(
        package,
        createAllPlan(package)
        );
    QVERIFY(!equalEndpointResult.has_value());
    QCOMPARE(
        equalEndpointResult.error(),
        QStringLiteral(
            "Schedule conflicts prevent this import:\n\n"
            "Regular schedule: E4 Theseus \u2014 Monday 4:00 PM\u20134:00 PM "
            "conflicts with E6 Gaia \u2014 Tuesday 3:30 PM\u20134:30 PM"
            )
        );
    QCOMPARE(service.getAllTeachers().value_or(QList<Teacher>{}).size(),
             teachersBefore);
    QCOMPARE(service.getClasses().value_or(QList<Classroom>{}).size(),
             classesBefore);
    QCOMPARE(service.loadClassInfo(destinationClass)->classTimes.first().day,
             QStringLiteral("Tuesday"));
    QCOMPARE(service.loadClassInfo(destinationClass)->classTimes.first().startTime,
             QStringLiteral("3:30 PM"));

    package.classes.first().info.classTimes.first().day =
        QStringLiteral("Funday");
    const auto invalidScheduleResult = service.importClasses(
        package,
        createAllPlan(package)
        );
    QVERIFY(!invalidScheduleResult.has_value());
    QCOMPARE(
        invalidScheduleResult.error(),
        QStringLiteral(
            "E4 Theseus contains an invalid regular schedule entry: "
            "Funday 4:00 PM\u20134:00 PM"
            )
        );
}

void ClassTransferTests::importedClassesConflictAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = createdTeacherId(service, completeTeacher());
    const int sourceClass = addCompleteClass(
        service,
        sourceTeacher,
        QString(),
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Monday"),
        QStringLiteral("Source Student")
        );
    const auto originalPackage = service.buildClassTransferPackage({sourceClass});
    QVERIFY(originalPackage.has_value());

    ClassTransferPackage package = *originalPackage;
    ClassTransferClass conflictingClass = package.classes.first();
    conflictingClass.key = QStringLiteral("class-2");
    conflictingClass.info.classGrade = QStringLiteral("E5");
    conflictingClass.info.classLevel = QStringLiteral("Apollo");
    conflictingClass.info.intensiveTimes.first().day = QStringLiteral("Tuesday");
    package.classes.append(conflictingClass);

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const auto result = service.importClasses(
        package, createAllPlan(package));
    QVERIFY(!result.has_value());
    QVERIFY(result.error().contains(QStringLiteral("Schedule conflicts")));
    QVERIFY(service.getClasses().value_or(QList<Classroom>{}).isEmpty());
    QVERIFY(service.getAllTeachers().value_or(QList<Teacher>{}).isEmpty());
}

void ClassTransferTests::databaseFailureRollsBackAllWrites()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = createdTeacherId(service, completeTeacher());
    const int sourceClass = addCompleteClass(
        service,
        sourceTeacher,
        QString(),
        QStringLiteral("E4"),
        QStringLiteral("Theseus"),
        QStringLiteral("Monday"),
        QStringLiteral("Source Student")
        );
    const auto originalPackage = service.buildClassTransferPackage({sourceClass});
    QVERIFY(originalPackage.has_value());

    ClassTransferPackage package = *originalPackage;
    package.classes.first().evaluations.append(
        package.classes.first().evaluations.first());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const auto result = service.importClasses(
        package, createAllPlan(package));
    QVERIFY(!result.has_value());
    QVERIFY(service.getClasses().value_or(QList<Classroom>{}).isEmpty());
    QVERIFY(service.getAllTeachers().value_or(QList<Teacher>{}).isEmpty());
}

void ClassTransferTests::incompleteCourseSignatureDoesNotMatch()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int teacherId = createdTeacherId(service, completeTeacher());
    const int sourceClass = createdClassId(service, QString());
    ClassInfo sourceInfo;
    sourceInfo.classId = sourceClass;
    sourceInfo.teacherId = teacherId;
    sourceInfo.classGrade = QStringLiteral("E4");
    QVERIFY(service.saveClassInfo(sourceInfo));
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const int destinationTeacher = createdTeacherId(service, completeTeacher());
    const int destinationClass = createdClassId(service, QString());
    ClassInfo destinationInfo;
    destinationInfo.classId = destinationClass;
    destinationInfo.teacherId = destinationTeacher;
    destinationInfo.classGrade = QStringLiteral("E4");
    QVERIFY(service.saveClassInfo(destinationInfo));

    const auto preview = service.previewClassImport(*package);
    QVERIFY(preview.has_value());
    QVERIFY(preview->classes.first().matchingClassIds.isEmpty());
}

void ClassTransferTests::codecRejectsMalformedAndUnsupportedPackages()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString malformedPath = directory.filePath(
        QStringLiteral("malformed.json"));
    QFile malformedFile(malformedPath);
    QVERIFY(malformedFile.open(QIODevice::WriteOnly | QIODevice::Text));
    malformedFile.write("{not-json");
    malformedFile.close();
    QVERIFY(!ClassTransferJsonCodec::loadFile(malformedPath).has_value());

    ClassTransferPackage package;
    package.exportedAtUtc = QDateTime::currentDateTimeUtc();
    ClassTransferClass transferClass;
    transferClass.key = QStringLiteral("class-1");
    transferClass.info = completeClassInfo(
        -1, -1, QStringLiteral("E4"), QStringLiteral("Theseus"),
        QStringLiteral("Monday"));
    transferClass.roster.columns = {};
    transferClass.roster.columnWidths = {};
    package.classes.append(transferClass);

    QJsonObject json = ClassTransferJsonCodec::toJson(package);
    json.insert(QStringLiteral("version"), 99);
    QVERIFY(!ClassTransferJsonCodec::fromJson(json).has_value());

    json = ClassTransferJsonCodec::toJson(package);
    QJsonArray classes = json.value(QStringLiteral("classes")).toArray();
    QJsonObject firstClass = classes.first().toObject();
    firstClass.insert(QStringLiteral("teacher_ref"),
                      QStringLiteral("missing-teacher"));
    classes.replace(0, firstClass);
    json.insert(QStringLiteral("classes"), classes);
    QVERIFY(!ClassTransferJsonCodec::fromJson(json).has_value());
}

void ClassTransferTests::previewPreservesQtNameAndCourseNormalization()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());

    Teacher sourceTeacher = completeTeacher();
    sourceTeacher.teacherEn = QStringLiteral("  ALEX\tKIM  ");
    const int sourceTeacherId = createdTeacherId(service, sourceTeacher);
    const int sourceClass = addCompleteClass(
        service,
        sourceTeacherId,
        QStringLiteral("Source Class"),
        QStringLiteral(" E4\t"),
        QStringLiteral("  PERSEUS  "),
        QStringLiteral("Monday"),
        QStringLiteral("Incoming Student")
        );
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const int matchingTeacher = createdTeacherId(
        service,
        completeTeacher(QStringLiteral("alex kim"))
        );
    const int matchingClass = addCompleteClass(
        service,
        matchingTeacher,
        QStringLiteral("Matching Class"),
        QStringLiteral("e4"),
        QStringLiteral("perseus"),
        QStringLiteral("Friday"),
        QStringLiteral("Existing Student")
        );
    const int otherTeacher = createdTeacherId(
        service, completeTeacher(QStringLiteral("Other Teacher")));
    const int wrongTeacherClass = addCompleteClass(
        service,
        otherTeacher,
        QStringLiteral("Wrong Teacher Class"),
        QStringLiteral("e4"),
        QStringLiteral("perseus"),
        QStringLiteral("Saturday"),
        QStringLiteral("Other Student")
        );

    const auto preview = service.previewClassImport(*package);
    QVERIFY(preview.has_value());
    QCOMPARE(
        preview->teachers.first().matchingTeacherIds,
        QList<int>({matchingTeacher})
        );
    QCOMPARE(
        preview->classes.first().matchingClassIds,
        QList<int>({matchingClass})
        );
    QVERIFY(!preview->classes.first().matchingClassIds.contains(
        wrongTeacherClass));
}

void ClassTransferTests::
    permanentConflictFixturePresentsReviewAndRejectsScheduleCollision()
{
    const QString fixturePath =
        QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
            QStringLiteral("tests/fixtures/transfers/conflict_source.json")
            );
    const auto package = ClassTransferJsonCodec::loadFile(fixturePath);
    QVERIFY2(
        package.has_value(),
        package ? "" : qPrintable(package.error())
        );
    QCOMPARE(package->version, ClassTransferPackage::CurrentVersion);
    QCOMPARE(package->teachers.size(), 1);
    QCOMPARE(package->classes.size(), 1);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());

    const int destinationTeacher =
        createdTeacherId(service, completeTeacher());
    const int destinationClass = addCompleteClass(
        service,
        destinationTeacher,
        QStringLiteral("Destination Class"),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Monday"),
        QStringLiteral("Destination Student")
        );
    QVERIFY(destinationClass > 0);

    const auto preview = service.previewClassImport(*package);
    QVERIFY(preview.has_value());
    QCOMPARE(
        preview->teachers.first().matchingTeacherIds,
        QList<int>({destinationTeacher})
        );
    QCOMPARE(
        preview->classes.first().matchingClassIds,
        QList<int>({destinationClass})
        );

    QString applicationServicesError;
    const auto applicationServices =
        openApplicationServicesForCurrentDatabase(
            service, &applicationServicesError);
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    const QString reviewFontFamily = loadReviewFontFamily();
    if (!reviewFontFamily.isEmpty())
    {
        QApplication::setFont(QFont(reviewFontFamily));
    }
    ClassImportDialog dialog(
        applicationServices.get(), *package, *preview);
    auto* classChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("classImportChoice_0"));
    auto* teacherChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("teacherImportChoice_teacher-1"));
    auto* importButton = dialog.findChild<QPushButton*>(
        QStringLiteral("importClassesButton"));
    QVERIFY(classChoice);
    QVERIFY(teacherChoice);
    QVERIFY(importButton);
    QVERIFY(importButton->isEnabled());
    QCOMPARE(classChoice->count(), 3);
    QCOMPARE(teacherChoice->count(), 2);

    dialog.show();
    QApplication::processEvents();

    const QString screenshotPath =
        qEnvironmentVariable(
            "CLASSMNGR_CONFLICT_REVIEW_OUTPUT_PATH"
            ).trimmed();
    if (!screenshotPath.isEmpty())
    {
        QVERIFY2(
            QDir().mkpath(QFileInfo(screenshotPath).absolutePath()),
            qPrintable(
                QStringLiteral("Could not create conflict screenshot directory for %1")
                    .arg(screenshotPath)
                )
            );
        const QPixmap screenshot = dialog.grab();
        QVERIFY(!screenshot.isNull());
        QVERIFY2(
            screenshot.save(screenshotPath, "PNG"),
            qPrintable(QStringLiteral("Could not save conflict review screenshot: %1")
                           .arg(screenshotPath))
            );
    }

    const int teachersBefore = service.getAllTeachers()
        .value_or(QList<Teacher>{}).size();
    const int classesBefore = service.getClasses()
        .value_or(QList<Classroom>{}).size();
    const Teacher teacherBefore = service.getTeacher(destinationTeacher)
        .value_or(Teacher{});
    const Classroom classBefore = service.getClassById(destinationClass)
        .value_or(Classroom{});
    const Result<ClassInfo> classInfoBefore =
        service.loadClassInfo(destinationClass);
    QVERIFY(classInfoBefore);
    const Result<Roster> rosterBefore = service.loadRoster(destinationClass);
    QVERIFY(rosterBefore);
    const auto evaluationBefore = service.loadSpeakingEval(
        destinationClass, QStringLiteral("Custom Evaluation"));
    QVERIFY(evaluationBefore.has_value());
    const auto result = service.importClasses(
        *package, dialog.importPlan());
    QVERIFY(!result.has_value());
    const QString expectedConflict = QStringLiteral(
        "Schedule conflicts prevent this import:\n\n"
        "Regular schedule: E4 Perseus \u2014 Monday 4:00 PM\u20134:50 PM "
        "conflicts with E4 Perseus \u2014 Monday 4:00 PM\u20134:50 PM\n"
        "Intensive schedule: E4 Perseus \u2014 Monday 10:00 AM\u201310:55 AM "
        "conflicts with E4 Perseus \u2014 Monday 10:00 AM\u201310:55 AM"
        );
    QCOMPARE(result.error(), expectedConflict);
    QCOMPARE(service.getAllTeachers().value_or(QList<Teacher>{}).size(),
             teachersBefore);
    QCOMPARE(service.getClasses().value_or(QList<Classroom>{}).size(),
             classesBefore);
    QCOMPARE(service.getTeacher(destinationTeacher)->wifiPassword,
             teacherBefore.wifiPassword);
    QCOMPARE(service.getTeacher(destinationTeacher)->notes,
             teacherBefore.notes);
    QCOMPARE(service.getClassById(destinationClass)->name, classBefore.name);
    QCOMPARE(service.loadClassInfo(destinationClass)->classColor,
             classInfoBefore->classColor);
    QCOMPARE(service.loadClassInfo(destinationClass)->classTimes.size(),
             classInfoBefore->classTimes.size());
    QCOMPARE(service.loadClassInfo(destinationClass)->classTimes.first().day,
             classInfoBefore->classTimes.first().day);
    QCOMPARE(
        service.loadClassInfo(destinationClass)->classTimes.first().startTime,
        classInfoBefore->classTimes.first().startTime);
    QCOMPARE(
        service.loadClassInfo(destinationClass)->classTimes.first().endTime,
        classInfoBefore->classTimes.first().endTime);
    QCOMPARE(service.loadRoster(destinationClass)->rows.first().first(),
             rosterBefore->rows.first().first());

    int skipIndex = -1;
    for (int index = 0; index < classChoice->count(); ++index)
    {
        if (classChoice->itemData(index, Qt::UserRole).toInt()
                == static_cast<int>(ClassImportAction::Skip)
            && classChoice->itemData(index, Qt::UserRole + 1).toInt() == -1)
        {
            QVERIFY(skipIndex < 0);
            skipIndex = index;
        }
    }
    QVERIFY(skipIndex >= 0);
    classChoice->setCurrentIndex(skipIndex);

    int replaceTeacherIndex = -1;
    for (int index = 0; index < teacherChoice->count(); ++index)
    {
        if (teacherChoice->itemData(index, Qt::UserRole).toInt()
                == static_cast<int>(TeacherImportAction::ReplaceExisting)
            && teacherChoice->itemData(index, Qt::UserRole + 1).toInt()
                == destinationTeacher)
        {
            QVERIFY(replaceTeacherIndex < 0);
            replaceTeacherIndex = index;
        }
    }
    QVERIFY(replaceTeacherIndex >= 0);
    teacherChoice->setCurrentIndex(replaceTeacherIndex);
    QVERIFY(importButton->isEnabled());

    const ClassImportPlan skipPlan = dialog.importPlan();
    QCOMPARE(skipPlan.classes.size(), 1);
    QCOMPARE(skipPlan.classes.first().packageClassIndex, 0);
    QCOMPARE(skipPlan.classes.first().action, ClassImportAction::Skip);
    QCOMPARE(skipPlan.classes.first().targetClassId, -1);
    QCOMPARE(skipPlan.teachers.size(), 1);
    QCOMPARE(skipPlan.teachers.first().teacherKey,
             package->teachers.first().key);
    QCOMPARE(skipPlan.teachers.first().action,
             TeacherImportAction::ReplaceExisting);
    QCOMPARE(skipPlan.teachers.first().targetTeacherId, destinationTeacher);

    const auto skipped = service.importClasses(*package, skipPlan);
    QVERIFY2(skipped.has_value(),
             skipped ? "" : qPrintable(skipped.error()));
    QVERIFY(skipped->createdClassIds.isEmpty());
    QVERIFY(skipped->replacedClassIds.isEmpty());
    QCOMPARE(skipped->skippedClassCount, 1);
    QCOMPARE(service.getAllTeachers().value_or(QList<Teacher>{}).size(),
             teachersBefore);
    QCOMPARE(service.getClasses().value_or(QList<Classroom>{}).size(),
             classesBefore);

    const Teacher teacherAfter = service.getTeacher(destinationTeacher)
        .value_or(Teacher{});
    QCOMPARE(teacherAfter.id, teacherBefore.id);
    QCOMPARE(teacherAfter.teacherKr, teacherBefore.teacherKr);
    QCOMPARE(teacherAfter.teacherEn, teacherBefore.teacherEn);
    QCOMPARE(teacherAfter.preferredRomanization,
             teacherBefore.preferredRomanization);
    QCOMPARE(teacherAfter.preferredName, teacherBefore.preferredName);
    QCOMPARE(teacherAfter.roomNumber, teacherBefore.roomNumber);
    QCOMPARE(teacherAfter.birthday, teacherBefore.birthday);
    QCOMPARE(teacherAfter.phoneNumber, teacherBefore.phoneNumber);
    QCOMPARE(teacherAfter.wifiName, teacherBefore.wifiName);
    QCOMPARE(teacherAfter.wifiPassword, teacherBefore.wifiPassword);
    QCOMPARE(teacherAfter.internetType, teacherBefore.internetType);
    QCOMPARE(teacherAfter.zoomId, teacherBefore.zoomId);
    QCOMPARE(teacherAfter.zoomPassword, teacherBefore.zoomPassword);
    QCOMPARE(teacherAfter.projectionType, teacherBefore.projectionType);
    QCOMPARE(teacherAfter.notes, teacherBefore.notes);

    const Classroom classAfter = service.getClassById(destinationClass)
        .value_or(Classroom{});
    QCOMPARE(classAfter.id, classBefore.id);
    QCOMPARE(classAfter.name, classBefore.name);

    const Result<ClassInfo> classInfoAfter =
        service.loadClassInfo(destinationClass);
    QVERIFY(classInfoAfter);
    QCOMPARE(classInfoAfter->classId, classInfoBefore->classId);
    QCOMPARE(classInfoAfter->teacherId, classInfoBefore->teacherId);
    QCOMPARE(classInfoAfter->teacherKr, classInfoBefore->teacherKr);
    QCOMPARE(classInfoAfter->teacherEn, classInfoBefore->teacherEn);
    QCOMPARE(classInfoAfter->teacherPreferredName,
             classInfoBefore->teacherPreferredName);
    QCOMPARE(classInfoAfter->roomNumber, classInfoBefore->roomNumber);
    QCOMPARE(classInfoAfter->wifiName, classInfoBefore->wifiName);
    QCOMPARE(classInfoAfter->wifiPassword, classInfoBefore->wifiPassword);
    QCOMPARE(classInfoAfter->internetType, classInfoBefore->internetType);
    QCOMPARE(classInfoAfter->zoomId, classInfoBefore->zoomId);
    QCOMPARE(classInfoAfter->zoomPassword, classInfoBefore->zoomPassword);
    QCOMPARE(classInfoAfter->projectionType, classInfoBefore->projectionType);
    QCOMPARE(classInfoAfter->classGrade, classInfoBefore->classGrade);
    QCOMPARE(classInfoAfter->classLevel, classInfoBefore->classLevel);
    QCOMPARE(classInfoAfter->readingBook, classInfoBefore->readingBook);
    QCOMPARE(classInfoAfter->essayBook, classInfoBefore->essayBook);
    QCOMPARE(classInfoAfter->classColor, classInfoBefore->classColor);
    QCOMPARE(classInfoAfter->fontColor, classInfoBefore->fontColor);
    QCOMPARE(classInfoAfter->notes, classInfoBefore->notes);
    QCOMPARE(classInfoAfter->timeFillerActivities,
             classInfoBefore->timeFillerActivities);
    QCOMPARE(classInfoAfter->classTimes.size(),
             classInfoBefore->classTimes.size());
    for (int index = 0; index < classInfoBefore->classTimes.size(); ++index)
    {
        QCOMPARE(classInfoAfter->classTimes[index].day,
                 classInfoBefore->classTimes[index].day);
        QCOMPARE(classInfoAfter->classTimes[index].startTime,
                 classInfoBefore->classTimes[index].startTime);
        QCOMPARE(classInfoAfter->classTimes[index].endTime,
                 classInfoBefore->classTimes[index].endTime);
    }
    QCOMPARE(classInfoAfter->intensiveTimes.size(),
             classInfoBefore->intensiveTimes.size());
    for (int index = 0;
         index < classInfoBefore->intensiveTimes.size();
         ++index)
    {
        QCOMPARE(classInfoAfter->intensiveTimes[index].day,
                 classInfoBefore->intensiveTimes[index].day);
        QCOMPARE(classInfoAfter->intensiveTimes[index].startTime,
                 classInfoBefore->intensiveTimes[index].startTime);
        QCOMPARE(classInfoAfter->intensiveTimes[index].endTime,
                 classInfoBefore->intensiveTimes[index].endTime);
    }

    const Result<Roster> rosterAfter = service.loadRoster(destinationClass);
    QVERIFY(rosterAfter);
    QCOMPARE(rosterAfter->columns, rosterBefore->columns);
    QCOMPARE(rosterAfter->columnWidths, rosterBefore->columnWidths);
    QCOMPARE(rosterAfter->rows, rosterBefore->rows);

    const auto evaluationAfter = service.loadSpeakingEval(
        destinationClass, QStringLiteral("Custom Evaluation"));
    QVERIFY(evaluationAfter.has_value());
    QCOMPARE(evaluationAfter->size(), evaluationBefore->size());
    for (int index = 0; index < evaluationBefore->size(); ++index)
    {
        QCOMPARE(evaluationAfter->at(index), evaluationBefore->at(index));
    }
}

void ClassTransferTests::
    conflictFixtureMatchesCommonInputBaselineAndRejectsWithoutWrites()
{
    // The fixture postdates baseline 48fc5c5c; using its unchanged bytes in
    // both trees is common-input evidence, not historical input parity.
    const QString fixturePath =
        QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
            QStringLiteral("tests/fixtures/transfers/conflict_source.json")
            );
    QFile fixtureFile(fixturePath);
    QVERIFY(fixtureFile.open(QIODevice::ReadOnly));
    const QByteArray fixtureBytes = fixtureFile.readAll();
    QByteArray normalizedFixtureBytes = fixtureBytes;
    normalizedFixtureBytes.replace("\r\n", "\n");
    normalizedFixtureBytes.replace('\r', '\n');
    QCOMPARE(
        QCryptographicHash::hash(
            normalizedFixtureBytes, QCryptographicHash::Sha256).toHex(),
        QByteArray("14756a43ac06382b890bfcbb74b50cb327eaf75f5414cd5f4697f4a4785a2b0c")
        );
    const auto package = ClassTransferJsonCodec::fromJson(
        QJsonDocument::fromJson(fixtureBytes).object());
    QVERIFY2(package.has_value(), package ? "" : qPrintable(package.error()));
    QCOMPARE(package->version, ClassTransferPackage::CurrentVersion);
    QCOMPARE(package->teachers.size(), 1);
    QCOMPARE(package->classes.size(), 1);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());

    const int destinationTeacher =
        createdTeacherId(service, completeTeacher());
    QCOMPARE(destinationTeacher, 1);
    const int destinationClass = addCompleteClass(
        service,
        destinationTeacher,
        QStringLiteral("Destination Class"),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Monday"),
        QStringLiteral("Destination Student")
        );
    QCOMPARE(destinationClass, 1);

    const auto preview = service.previewClassImport(*package);
    QVERIFY2(preview.has_value(), preview ? "" : qPrintable(preview.error()));
    QCOMPARE(preview->teachers.size(), 1);
    QCOMPARE(preview->teachers.first().teacherKey,
             QStringLiteral("teacher-1"));
    QCOMPARE(preview->teachers.first().matchingTeacherIds,
             QList<int>({destinationTeacher}));
    QCOMPARE(preview->classes.size(), 1);
    QCOMPARE(preview->classes.first().packageClassIndex, 0);
    QCOMPARE(preview->classes.first().matchingClassIds,
             QList<int>({destinationClass}));

    QString applicationServicesError;
    const auto applicationServices =
        openApplicationServicesForCurrentDatabase(
            service, &applicationServicesError);
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    ClassImportDialog dialog(applicationServices.get(), *package, *preview);
    auto* classChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("classImportChoice_0"));
    auto* teacherChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("teacherImportChoice_teacher-1"));
    auto* importButton = dialog.findChild<QPushButton*>(
        QStringLiteral("importClassesButton"));
    QVERIFY(classChoice);
    QVERIFY(teacherChoice);
    QVERIFY(importButton);
    QVERIFY(importButton->isEnabled());

    QCOMPARE(classChoice->count(), 3);
    QCOMPARE(classChoice->itemData(0, Qt::UserRole).toInt(),
             static_cast<int>(ClassImportAction::Create));
    QCOMPARE(classChoice->itemData(0, Qt::UserRole + 1).toInt(), -1);
    QCOMPARE(classChoice->itemData(1, Qt::UserRole).toInt(),
             static_cast<int>(ClassImportAction::Replace));
    QCOMPARE(classChoice->itemData(1, Qt::UserRole + 1).toInt(),
             destinationClass);
    QCOMPARE(classChoice->itemData(2, Qt::UserRole).toInt(),
             static_cast<int>(ClassImportAction::Skip));
    QCOMPARE(classChoice->itemData(2, Qt::UserRole + 1).toInt(), -1);

    QCOMPARE(teacherChoice->count(), 2);
    QCOMPARE(teacherChoice->itemData(0, Qt::UserRole).toInt(),
             static_cast<int>(TeacherImportAction::KeepExisting));
    QCOMPARE(teacherChoice->itemData(0, Qt::UserRole + 1).toInt(),
             destinationTeacher);
    QCOMPARE(teacherChoice->itemData(1, Qt::UserRole).toInt(),
             static_cast<int>(TeacherImportAction::ReplaceExisting));
    QCOMPARE(teacherChoice->itemData(1, Qt::UserRole + 1).toInt(),
             destinationTeacher);

    const ClassImportPlan plan = dialog.importPlan();
    QCOMPARE(plan.classes.size(), 1);
    QCOMPARE(plan.classes.first().packageClassIndex, 0);
    QCOMPARE(plan.classes.first().action, ClassImportAction::Create);
    QCOMPARE(plan.classes.first().targetClassId, -1);
    QCOMPARE(plan.teachers.size(), 1);
    QCOMPARE(plan.teachers.first().teacherKey, QStringLiteral("teacher-1"));
    QCOMPARE(plan.teachers.first().action,
             TeacherImportAction::KeepExisting);
    QCOMPARE(plan.teachers.first().targetTeacherId, destinationTeacher);

    const QSqlDatabase database = service.databaseSession()->database();
    QString snapshotError;
    const auto snapshotBefore =
        persistedDatabaseSnapshot(database, &snapshotError);
    QVERIFY2(snapshotBefore.has_value(), qPrintable(snapshotError));
    const auto changesBefore = sqliteTotalChanges(database, &snapshotError);
    QVERIFY2(changesBefore.has_value(), qPrintable(snapshotError));

    QSqlQuery queryOnly(database);
    QVERIFY2(queryOnly.exec(QStringLiteral("PRAGMA query_only = ON")),
             qPrintable(queryOnly.lastError().text()));
    QSqlQuery queryOnlyState(database);
    QVERIFY2(queryOnlyState.exec(QStringLiteral("PRAGMA query_only")),
             qPrintable(queryOnlyState.lastError().text()));
    QVERIFY(queryOnlyState.next());
    QCOMPARE(queryOnlyState.value(0).toInt(), 1);

    const auto result = service.importClasses(*package, plan);
    QVERIFY(!result.has_value());
    QCOMPARE(
        result.error(),
        QStringLiteral(
            "Schedule conflicts prevent this import:\n\n"
            "Regular schedule: E4 Perseus \u2014 Monday 4:00 PM\u20134:50 PM "
            "conflicts with E4 Perseus \u2014 Monday 4:00 PM\u20134:50 PM\n"
            "Intensive schedule: E4 Perseus \u2014 Monday 10:00 AM\u201310:55 AM "
            "conflicts with E4 Perseus \u2014 Monday 10:00 AM\u201310:55 AM"));

    const auto snapshotAfter =
        persistedDatabaseSnapshot(database, &snapshotError);
    QVERIFY2(snapshotAfter.has_value(), qPrintable(snapshotError));
    QCOMPARE(*snapshotAfter, *snapshotBefore);
    const auto changesAfter = sqliteTotalChanges(database, &snapshotError);
    QVERIFY2(changesAfter.has_value(), qPrintable(snapshotError));
    QCOMPARE(*changesAfter - *changesBefore, qlonglong(0));
}

void ClassTransferTests::requiredSuccessFixtureTraversesReviewAndPersistsResults()
{
    const QString fixturePath =
        QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
            QStringLiteral("tests/fixtures/transfers/success_source.json")
            );
    const auto package = ClassTransferJsonCodec::loadFile(fixturePath);
    QVERIFY2(
        package.has_value(),
        package ? "" : qPrintable(package.error())
        );
    QCOMPARE(package->teachers.size(), 1);
    QCOMPARE(package->classes.size(), 1);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());

    const auto preview = service.previewClassImport(*package);
    QVERIFY2(preview.has_value(), preview ? "" : qPrintable(preview.error()));
    QCOMPARE(preview->teachers.size(), 1);
    QCOMPARE(preview->teachers.first().matchingTeacherIds, QList<int>{});
    QCOMPARE(preview->classes.size(), 1);
    QCOMPARE(preview->classes.first().packageClassIndex, 0);
    QCOMPARE(preview->classes.first().matchingClassIds, QList<int>{});

    QString applicationServicesError;
    const auto applicationServices =
        openApplicationServicesForCurrentDatabase(
            service, &applicationServicesError);
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    ClassImportDialog dialog(applicationServices.get(), *package, *preview);
    auto* classChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("classImportChoice_0"));
    auto* teacherChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("teacherImportChoice_teacher-success-1"));
    auto* importButton = dialog.findChild<QPushButton*>(
        QStringLiteral("importClassesButton"));
    QVERIFY(classChoice);
    QVERIFY(teacherChoice);
    QVERIFY(importButton);
    QVERIFY(importButton->isEnabled());
    QCOMPARE(dialog.importPlan().classes.first().action,
             ClassImportAction::Create);
    QCOMPARE(dialog.importPlan().teachers.first().action,
             TeacherImportAction::Create);

    const auto summary = service.importClasses(*package, dialog.importPlan());
    QVERIFY2(summary.has_value(), summary ? "" : qPrintable(summary.error()));
    QCOMPARE(summary->createdClassIds.size(), 1);
    QVERIFY(summary->replacedClassIds.isEmpty());
    QCOMPARE(summary->skippedClassCount, 0);
    QCOMPARE(service.getClasses().value_or(QList<Classroom>{}).size(), 1);
    QCOMPARE(service.getAllTeachers().value_or(QList<Teacher>{}).size(), 1);

    const int importedClassId = summary->createdClassIds.first();
    const Classroom importedClass = service.getClassById(importedClassId)
        .value_or(Classroom{});
    QCOMPARE(importedClass.name, QStringLiteral("Fixture Stored Class"));
    const Result<ClassInfo> importedInfo =
        service.loadClassInfo(importedClassId);
    QVERIFY(importedInfo);
    QCOMPARE(importedInfo->classGrade, QStringLiteral("E3"));
    QCOMPARE(importedInfo->classLevel, QStringLiteral("Orion"));
    QCOMPARE(importedInfo->classColor, QStringLiteral("#2468AC"));
    QCOMPARE(importedInfo->classTimes.size(), 1);
    QCOMPARE(importedInfo->classTimes.first().day, QStringLiteral("Tuesday"));
    QCOMPARE(importedInfo->classTimes.first().startTime,
             QStringLiteral("3:00 PM"));
    QCOMPARE(importedInfo->classTimes.first().endTime,
             QStringLiteral("3:50 PM"));

    const auto importedTeacher = service.getTeacher(importedInfo->teacherId);
    QVERIFY(importedTeacher.has_value());
    QCOMPARE(importedTeacher->teacherEn, QStringLiteral("Alex Kim"));
    QCOMPARE(importedTeacher->wifiPassword,
             QStringLiteral("fixture-password"));
    const Result<Roster> importedRoster = service.loadRoster(importedClassId);
    QVERIFY(importedRoster);
    QCOMPARE(importedRoster->columns,
             QStringList({"English", "Korean", "Memo"}));
    QCOMPARE(importedRoster->rows.size(), 1);
    QCOMPARE(importedRoster->rows.first(),
             QStringList({"Avery", "Fixture student", "Transferred row"}));
    const auto importedEvaluation = service.loadSpeakingEval(
        importedClassId, QStringLiteral("Fixture Evaluation"));
    QVERIFY(importedEvaluation.has_value());
    QCOMPARE(importedEvaluation->size(), SpeakingEval::RowCount);
    QCOMPARE(*importedEvaluation, expectedFixtureEvaluationRows());
}

void ClassTransferTests::successFixtureReplacesMatchingTeacherThroughReview()
{
    const QString fixturePath =
        QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
            QStringLiteral("tests/fixtures/transfers/success_source.json")
            );
    const auto package = ClassTransferJsonCodec::loadFile(fixturePath);
    QVERIFY2(
        package.has_value(),
        package ? "" : qPrintable(package.error())
        );
    QCOMPARE(package->teachers.size(), 1);
    QCOMPARE(package->classes.size(), 1);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());

    const Teacher& fixtureTeacher = package->teachers.first().teacher;
    Teacher localTeacher = fixtureTeacher;
    localTeacher.preferredRomanization = QStringLiteral("Local romanization");
    localTeacher.preferredName = QStringLiteral("Local preferred name");
    localTeacher.roomNumber = QStringLiteral("Local room");
    localTeacher.birthday = QStringLiteral("01-01");
    localTeacher.phoneNumber = QStringLiteral("010-9876-5432");
    localTeacher.wifiName = QStringLiteral("Local WiFi");
    localTeacher.wifiPassword = QStringLiteral("local-only-password");
    localTeacher.internetType = QStringLiteral("LAN");
    localTeacher.zoomId = QStringLiteral("987 654 3210");
    localTeacher.zoomPassword = QStringLiteral("local Zoom password");
    localTeacher.projectionType = QStringLiteral("Zoom");
    localTeacher.notes = QStringLiteral("Local-only notes");
    QVERIFY(localTeacher.preferredRomanization
            != fixtureTeacher.preferredRomanization);
    QVERIFY(localTeacher.preferredName != fixtureTeacher.preferredName);
    QVERIFY(localTeacher.roomNumber != fixtureTeacher.roomNumber);
    QVERIFY(localTeacher.birthday != fixtureTeacher.birthday);
    QVERIFY(localTeacher.phoneNumber != fixtureTeacher.phoneNumber);
    QVERIFY(localTeacher.wifiName != fixtureTeacher.wifiName);
    QVERIFY(localTeacher.wifiPassword != fixtureTeacher.wifiPassword);
    QVERIFY(localTeacher.internetType != fixtureTeacher.internetType);
    QVERIFY(localTeacher.zoomId != fixtureTeacher.zoomId);
    QVERIFY(localTeacher.zoomPassword != fixtureTeacher.zoomPassword);
    QVERIFY(localTeacher.projectionType != fixtureTeacher.projectionType);
    QVERIFY(localTeacher.notes != fixtureTeacher.notes);
    const int destinationTeacher = createdTeacherId(service, localTeacher);
    QVERIFY(destinationTeacher > 0);

    const auto preview = service.previewClassImport(*package);
    QVERIFY2(preview.has_value(), preview ? "" : qPrintable(preview.error()));
    QCOMPARE(preview->teachers.size(), 1);
    QCOMPARE(
        preview->teachers.first().matchingTeacherIds,
        QList<int>({destinationTeacher})
        );
    QCOMPARE(preview->classes.size(), 1);
    QCOMPARE(preview->classes.first().packageClassIndex, 0);
    QCOMPARE(preview->classes.first().matchingClassIds, QList<int>{});

    QString applicationServicesError;
    const auto applicationServices =
        openApplicationServicesForCurrentDatabase(
            service, &applicationServicesError);
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    ClassImportDialog dialog(applicationServices.get(), *package, *preview);
    auto* classChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("classImportChoice_0"));
    auto* teacherChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("teacherImportChoice_teacher-success-1"));
    auto* importButton = dialog.findChild<QPushButton*>(
        QStringLiteral("importClassesButton"));
    QVERIFY(classChoice);
    QVERIFY(teacherChoice);
    QVERIFY(importButton);
    QCOMPARE(classChoice->itemData(0, Qt::UserRole).toInt(),
             static_cast<int>(ClassImportAction::Create));
    QCOMPARE(teacherChoice->count(), 2);

    int replaceIndex = -1;
    for (int index = 0; index < teacherChoice->count(); ++index)
    {
        if (teacherChoice->itemData(index, Qt::UserRole).toInt()
                == static_cast<int>(TeacherImportAction::ReplaceExisting)
            && teacherChoice->itemData(index, Qt::UserRole + 1).toInt()
                == destinationTeacher)
        {
            QVERIFY(replaceIndex < 0);
            replaceIndex = index;
        }
    }
    QVERIFY(replaceIndex >= 0);
    teacherChoice->setCurrentIndex(replaceIndex);
    QVERIFY(importButton->isEnabled());

    const ClassImportPlan plan = dialog.importPlan();
    QCOMPARE(plan.classes.size(), 1);
    QCOMPARE(plan.classes.first().action, ClassImportAction::Create);
    QCOMPARE(plan.teachers.size(), 1);
    QCOMPARE(plan.teachers.first().action,
             TeacherImportAction::ReplaceExisting);
    QCOMPARE(plan.teachers.first().targetTeacherId, destinationTeacher);

    const auto summary = service.importClasses(*package, plan);
    QVERIFY2(summary.has_value(), summary ? "" : qPrintable(summary.error()));
    QCOMPARE(summary->createdClassIds.size(), 1);
    QVERIFY(summary->replacedClassIds.isEmpty());
    QCOMPARE(summary->skippedClassCount, 0);
    QCOMPARE(service.getAllTeachers().value_or(QList<Teacher>{}).size(), 1);
    QCOMPARE(service.getClasses().value_or(QList<Classroom>{}).size(), 1);

    const auto importedTeacher = service.getTeacher(destinationTeacher);
    QVERIFY(importedTeacher.has_value());
    QCOMPARE(importedTeacher->id, destinationTeacher);
    QCOMPARE(importedTeacher->teacherEn,
             fixtureTeacher.teacherEn);
    QCOMPARE(importedTeacher->teacherKr,
             fixtureTeacher.teacherKr);
    QCOMPARE(importedTeacher->preferredRomanization,
             fixtureTeacher.preferredRomanization);
    QCOMPARE(importedTeacher->preferredName,
             fixtureTeacher.preferredName);
    QCOMPARE(importedTeacher->roomNumber, fixtureTeacher.roomNumber);
    QCOMPARE(importedTeacher->birthday, fixtureTeacher.birthday);
    QCOMPARE(importedTeacher->phoneNumber, fixtureTeacher.phoneNumber);
    QCOMPARE(importedTeacher->wifiName, fixtureTeacher.wifiName);
    QCOMPARE(importedTeacher->wifiPassword,
             fixtureTeacher.wifiPassword);
    QCOMPARE(importedTeacher->internetType, fixtureTeacher.internetType);
    QCOMPARE(importedTeacher->zoomId, fixtureTeacher.zoomId);
    QCOMPARE(importedTeacher->zoomPassword, fixtureTeacher.zoomPassword);
    QCOMPARE(importedTeacher->projectionType, fixtureTeacher.projectionType);
    QCOMPARE(importedTeacher->notes, fixtureTeacher.notes);

    const int importedClassId = summary->createdClassIds.first();
    const auto importedClass = service.getClassById(importedClassId);
    QVERIFY(importedClass.has_value());
    QCOMPARE(importedClass->name, QStringLiteral("Fixture Stored Class"));
    const Result<ClassInfo> importedInfo =
        service.loadClassInfo(importedClassId);
    QVERIFY(importedInfo);
    QCOMPARE(importedInfo->teacherId, destinationTeacher);
    QCOMPARE(importedInfo->classGrade, QStringLiteral("E3"));
    QCOMPARE(importedInfo->classLevel, QStringLiteral("Orion"));
}

void ClassTransferTests::successFixtureReplacesMatchingDestinationAndChildren()
{
    const QString fixturePath =
        QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
            QStringLiteral("tests/fixtures/transfers/success_source.json")
            );
    const auto package = ClassTransferJsonCodec::loadFile(fixturePath);
    QVERIFY2(
        package.has_value(),
        package ? "" : qPrintable(package.error())
        );
    QCOMPARE(package->teachers.size(), 1);
    QCOMPARE(package->classes.size(), 1);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());

    Teacher localTeacher = package->teachers.first().teacher;
    localTeacher.wifiPassword = QStringLiteral("local-only-password");
    const int destinationTeacher = createdTeacherId(service, localTeacher);
    QVERIFY(destinationTeacher > 0);
    const int destinationClass = addCompleteClass(
        service,
        destinationTeacher,
        QStringLiteral("Old Destination Class"),
        QStringLiteral("E3"),
        QStringLiteral("Orion"),
        QStringLiteral("Thursday"),
        QStringLiteral("Old destination student"),
        QStringLiteral("Destination Only Evaluation"),
        QStringLiteral("11:00 AM"),
        QStringLiteral("11:50 AM")
        );
    QVERIFY(destinationClass > 0);

    const auto preview = service.previewClassImport(*package);
    QVERIFY2(preview.has_value(), preview ? "" : qPrintable(preview.error()));
    QCOMPARE(preview->teachers.size(), 1);
    QCOMPARE(
        preview->teachers.first().matchingTeacherIds,
        QList<int>({destinationTeacher})
        );
    QCOMPARE(preview->classes.size(), 1);
    QCOMPARE(preview->classes.first().packageClassIndex, 0);
    QCOMPARE(
        preview->classes.first().matchingClassIds,
        QList<int>({destinationClass})
        );

    QString applicationServicesError;
    const auto applicationServices =
        openApplicationServicesForCurrentDatabase(
            service, &applicationServicesError);
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    ClassImportDialog dialog(applicationServices.get(), *package, *preview);
    auto* classChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("classImportChoice_0"));
    auto* importButton = dialog.findChild<QPushButton*>(
        QStringLiteral("importClassesButton"));
    auto* validationLabel = dialog.findChild<QLabel*>(
        QStringLiteral("importValidationLabel"));
    QVERIFY(classChoice);
    QVERIFY(importButton);
    QVERIFY(validationLabel);
    QVERIFY(importButton->isEnabled());
    QCOMPARE(classChoice->count(), 3);

    classChoice->setItemData(1, 0, Qt::UserRole + 1);
    classChoice->setCurrentIndex(1);
    QVERIFY(!importButton->isEnabled());
    QCOMPARE(
        validationLabel->text(),
        QStringLiteral(
            "A replacement class is not one of the inferred matches."));
    classChoice->setItemData(1, destinationClass, Qt::UserRole + 1);
    classChoice->setCurrentIndex(0);
    QVERIFY(importButton->isEnabled());

    classChoice->setItemData(0, 0, Qt::UserRole + 1);
    classChoice->setCurrentIndex(1);
    QVERIFY(importButton->isEnabled());
    classChoice->setCurrentIndex(0);
    QVERIFY(!importButton->isEnabled());
    QCOMPARE(
        validationLabel->text(),
        QStringLiteral(
            "Only replacement actions may specify a destination class."));
    classChoice->setItemData(0, -1, Qt::UserRole + 1);
    classChoice->setCurrentIndex(1);
    QVERIFY(importButton->isEnabled());

    classChoice->setItemData(1, -2, Qt::UserRole + 1);
    classChoice->setCurrentIndex(0);
    QVERIFY(importButton->isEnabled());
    classChoice->setCurrentIndex(1);
    QVERIFY(!importButton->isEnabled());
    QCOMPARE(
        validationLabel->text(),
        QStringLiteral(
            "A replacement class is not one of the inferred matches."));
    classChoice->setItemData(1, destinationClass, Qt::UserRole + 1);
    classChoice->setCurrentIndex(0);
    classChoice->setCurrentIndex(1);
    QVERIFY(importButton->isEnabled());

    auto* teacherChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("teacherImportChoice_teacher-success-1"));
    QVERIFY(teacherChoice);
    QCOMPARE(teacherChoice->count(), 2);
    for (const int targetIndex : {0, 1})
    {
        for (const int invalidTarget : {0, -2})
        {
            const int otherIndex = targetIndex == 0 ? 1 : 0;
            teacherChoice->setItemData(
                targetIndex, invalidTarget, Qt::UserRole + 1);
            teacherChoice->setCurrentIndex(otherIndex);
            QVERIFY(importButton->isEnabled());
            teacherChoice->setCurrentIndex(targetIndex);
            QVERIFY(!importButton->isEnabled());
            QCOMPARE(
                validationLabel->text(),
                QStringLiteral(
                    "A selected teacher is not one of the inferred matches."));

            teacherChoice->setItemData(
                targetIndex, destinationTeacher, Qt::UserRole + 1);
            teacherChoice->setCurrentIndex(otherIndex);
            QVERIFY(importButton->isEnabled());
            teacherChoice->setCurrentIndex(targetIndex);
            QVERIFY(importButton->isEnabled());
        }
    }
    teacherChoice->setCurrentIndex(0);
    QVERIFY(importButton->isEnabled());

    QCOMPARE(dialog.importPlan().classes.first().action,
             ClassImportAction::Replace);
    QCOMPARE(dialog.importPlan().classes.first().targetClassId,
             destinationClass);
    QCOMPARE(dialog.importPlan().teachers.first().action,
             TeacherImportAction::KeepExisting);
    QCOMPARE(dialog.importPlan().teachers.first().targetTeacherId,
             destinationTeacher);

    const auto summary = service.importClasses(*package, dialog.importPlan());
    QVERIFY2(summary.has_value(), summary ? "" : qPrintable(summary.error()));
    QVERIFY(summary->createdClassIds.isEmpty());
    QCOMPARE(summary->replacedClassIds, QList<int>({destinationClass}));
    QCOMPARE(summary->skippedClassCount, 0);
    QCOMPARE(service.getClasses().value_or(QList<Classroom>{}).size(), 1);
    QCOMPARE(service.getAllTeachers().value_or(QList<Teacher>{}).size(), 1);

    const Classroom replacedClass = service.getClassById(destinationClass)
        .value_or(Classroom{});
    QCOMPARE(replacedClass.id, destinationClass);
    QCOMPARE(replacedClass.name, QStringLiteral("Fixture Stored Class"));

    const Result<ClassInfo> replacedInfo =
        service.loadClassInfo(destinationClass);
    QVERIFY(replacedInfo);
    QCOMPARE(replacedInfo->teacherId, destinationTeacher);
    QCOMPARE(replacedInfo->classGrade, QStringLiteral("E3"));
    QCOMPARE(replacedInfo->classLevel, QStringLiteral("Orion"));
    QCOMPARE(replacedInfo->readingBook, QStringLiteral("Reading Explorer 2"));
    QCOMPARE(replacedInfo->essayBook, QStringLiteral("3C"));
    QCOMPARE(replacedInfo->classColor, QStringLiteral("#2468AC"));
    QCOMPARE(replacedInfo->fontColor, QStringLiteral("#FFFFFF"));
    QCOMPARE(replacedInfo->notes, QStringLiteral("Fixture class notes"));
    QCOMPARE(replacedInfo->timeFillerActivities, QStringLiteral("Word chain"));
    QCOMPARE(replacedInfo->classTimes.size(), 1);
    QCOMPARE(replacedInfo->classTimes.first().day, QStringLiteral("Tuesday"));
    QCOMPARE(replacedInfo->classTimes.first().startTime,
             QStringLiteral("3:00 PM"));
    QCOMPARE(replacedInfo->classTimes.first().endTime,
             QStringLiteral("3:50 PM"));
    QVERIFY(replacedInfo->intensiveTimes.isEmpty());

    const Result<Roster> replacedRoster = service.loadRoster(destinationClass);
    QVERIFY(replacedRoster);
    QCOMPARE(replacedRoster->columns,
             QStringList({"English", "Korean", "Memo"}));
    QCOMPARE(replacedRoster->columnWidths, QList<int>({180, 190, 240}));
    QCOMPARE(replacedRoster->rows.size(), 1);
    QCOMPARE(replacedRoster->rows.first(),
             QStringList({"Avery", "Fixture student", "Transferred row"}));
    const auto oldEvaluation = service.loadSpeakingEval(
        destinationClass,
        QStringLiteral("Destination Only Evaluation"));
    QVERIFY(oldEvaluation.has_value());
    QVERIFY(oldEvaluation->isEmpty());
    const auto importedEvaluation = service.loadSpeakingEval(
        destinationClass, QStringLiteral("Fixture Evaluation"));
    QVERIFY(importedEvaluation.has_value());
    QCOMPARE(importedEvaluation->size(), SpeakingEval::RowCount);
    QCOMPARE(*importedEvaluation, expectedFixtureEvaluationRows());

    const auto retainedTeacher = service.getTeacher(destinationTeacher);
    QVERIFY(retainedTeacher.has_value());
    QCOMPARE(retainedTeacher->wifiPassword,
             QStringLiteral("local-only-password"));
}

void ClassTransferTests::successFixtureClassReplacementMatchesCommonInputState()
{
    const QString fixturePath =
        QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
            QStringLiteral("tests/fixtures/transfers/success_source.json")
            );
    QFile fixtureFile(fixturePath);
    QVERIFY(fixtureFile.open(QIODevice::ReadOnly));
    const QByteArray fixtureBytes = fixtureFile.readAll();
    QByteArray normalizedFixtureBytes = fixtureBytes;
    normalizedFixtureBytes.replace("\r\n", "\n");
    normalizedFixtureBytes.replace('\r', '\n');
    QCOMPARE(
        QCryptographicHash::hash(
            normalizedFixtureBytes, QCryptographicHash::Sha256).toHex(),
        QByteArray("df18ca11d3052499d3ddc02ca18cd92e51d436c659dc756a69063cec9c29aadd")
        );
    const auto package = ClassTransferJsonCodec::fromJson(
        QJsonDocument::fromJson(fixtureBytes).object());
    QVERIFY2(package.has_value(), package ? "" : qPrintable(package.error()));
    QCOMPARE(package->version, ClassTransferPackage::CurrentVersion);
    QCOMPARE(package->teachers.size(), 1);
    QCOMPARE(package->classes.size(), 1);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());

    Teacher localTeacher = package->teachers.first().teacher;
    localTeacher.wifiPassword = QStringLiteral("local-only-password");
    const int destinationTeacher = createdTeacherId(service, localTeacher);
    QCOMPARE(destinationTeacher, 1);
    const int destinationClass = addCompleteClass(
        service,
        destinationTeacher,
        QStringLiteral("Old Destination Class"),
        QStringLiteral("E3"),
        QStringLiteral("Orion"),
        QStringLiteral("Thursday"),
        QStringLiteral("Old destination student"),
        QStringLiteral("Destination Only Evaluation"),
        QStringLiteral("11:00 AM"),
        QStringLiteral("11:50 AM")
        );
    QCOMPARE(destinationClass, 1);

    const auto preview = service.previewClassImport(*package);
    QVERIFY2(preview.has_value(), preview ? "" : qPrintable(preview.error()));
    QCOMPARE(preview->teachers.size(), 1);
    QCOMPARE(preview->teachers.first().teacherKey,
             QStringLiteral("teacher-success-1"));
    QCOMPARE(preview->teachers.first().matchingTeacherIds,
             QList<int>({destinationTeacher}));
    QCOMPARE(preview->classes.size(), 1);
    QCOMPARE(preview->classes.first().packageClassIndex, 0);
    QCOMPARE(preview->classes.first().matchingClassIds,
             QList<int>({destinationClass}));

    QString applicationServicesError;
    const auto applicationServices =
        openApplicationServicesForCurrentDatabase(
            service, &applicationServicesError);
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    ClassImportDialog dialog(applicationServices.get(), *package, *preview);
    auto* classChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("classImportChoice_0"));
    auto* teacherChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("teacherImportChoice_teacher-success-1"));
    auto* importButton = dialog.findChild<QPushButton*>(
        QStringLiteral("importClassesButton"));
    QVERIFY(classChoice);
    QVERIFY(teacherChoice);
    QVERIFY(importButton);
    QCOMPARE(classChoice->count(), 3);
    QCOMPARE(classChoice->itemText(1),
             QStringLiteral("Replace: E3 Orion • Alex • Thurs (11:00)"));
    QCOMPARE(classChoice->itemData(0, Qt::UserRole).toInt(),
             static_cast<int>(ClassImportAction::Create));
    QCOMPARE(classChoice->itemData(1, Qt::UserRole).toInt(),
             static_cast<int>(ClassImportAction::Replace));
    QCOMPARE(classChoice->itemData(1, Qt::UserRole + 1).toInt(),
             destinationClass);
    QCOMPARE(classChoice->itemData(2, Qt::UserRole).toInt(),
             static_cast<int>(ClassImportAction::Skip));
    QCOMPARE(teacherChoice->count(), 2);
    QCOMPARE(teacherChoice->itemText(0),
             QStringLiteral("Keep local: Alex"));
    QCOMPARE(teacherChoice->itemText(1),
             QStringLiteral("Replace local: Alex"));
    QCOMPARE(teacherChoice->itemData(0, Qt::UserRole).toInt(),
             static_cast<int>(TeacherImportAction::KeepExisting));
    QCOMPARE(teacherChoice->itemData(0, Qt::UserRole + 1).toInt(),
             destinationTeacher);
    QCOMPARE(teacherChoice->itemData(1, Qt::UserRole).toInt(),
             static_cast<int>(TeacherImportAction::ReplaceExisting));
    QCOMPARE(teacherChoice->itemData(1, Qt::UserRole + 1).toInt(),
             destinationTeacher);

    classChoice->setCurrentIndex(1);
    QVERIFY(importButton->isEnabled());
    const ClassImportPlan plan = dialog.importPlan();
    QCOMPARE(plan.classes.size(), 1);
    QCOMPARE(plan.classes.first().packageClassIndex, 0);
    QCOMPARE(plan.classes.first().action, ClassImportAction::Replace);
    QCOMPARE(plan.classes.first().targetClassId, destinationClass);
    QCOMPARE(plan.teachers.size(), 1);
    QCOMPARE(plan.teachers.first().teacherKey,
             QStringLiteral("teacher-success-1"));
    QCOMPARE(plan.teachers.first().action, TeacherImportAction::KeepExisting);
    QCOMPARE(plan.teachers.first().targetTeacherId, destinationTeacher);

    const auto summary = service.importClasses(*package, plan);
    QVERIFY2(summary.has_value(), summary ? "" : qPrintable(summary.error()));
    QVERIFY(summary->createdClassIds.isEmpty());
    QCOMPARE(summary->replacedClassIds, QList<int>({destinationClass}));
    QCOMPARE(summary->skippedClassCount, 0);
    const Result<QList<Classroom>> classesAfter = service.getClasses();
    QVERIFY(classesAfter);
    QCOMPARE(classesAfter->size(), 1);
    const Result<Classroom> replacedClass =
        service.getClassById(destinationClass);
    QVERIFY(replacedClass);
    QCOMPARE(replacedClass->id, destinationClass);
    QCOMPARE(replacedClass->name, QStringLiteral("Fixture Stored Class"));

    const Result<ClassInfo> replacedInfo =
        service.loadClassInfo(destinationClass);
    QVERIFY(replacedInfo);
    QCOMPARE(replacedInfo->classId, destinationClass);
    QCOMPARE(replacedInfo->teacherId, destinationTeacher);
    QCOMPARE(replacedInfo->classGrade, QStringLiteral("E3"));
    QCOMPARE(replacedInfo->classLevel, QStringLiteral("Orion"));
    QCOMPARE(replacedInfo->readingBook, QStringLiteral("Reading Explorer 2"));
    QCOMPARE(replacedInfo->essayBook, QStringLiteral("3C"));
    QCOMPARE(replacedInfo->classColor, QStringLiteral("#2468AC"));
    QCOMPARE(replacedInfo->fontColor, QStringLiteral("#FFFFFF"));
    QCOMPARE(replacedInfo->notes, QStringLiteral("Fixture class notes"));
    QCOMPARE(replacedInfo->timeFillerActivities, QStringLiteral("Word chain"));
    QCOMPARE(replacedInfo->classTimes.size(), 1);
    QCOMPARE(replacedInfo->classTimes.first().day, QStringLiteral("Tuesday"));
    QCOMPARE(replacedInfo->classTimes.first().startTime,
             QStringLiteral("3:00 PM"));
    QCOMPARE(replacedInfo->classTimes.first().endTime,
             QStringLiteral("3:50 PM"));
    QVERIFY(replacedInfo->intensiveTimes.isEmpty());

    const Result<Roster> replacedRoster = service.loadRoster(destinationClass);
    QVERIFY(replacedRoster);
    QCOMPARE(replacedRoster->columns,
             QStringList({"English", "Korean", "Memo"}));
    QCOMPARE(replacedRoster->columnWidths, QList<int>({180, 190, 240}));
    QCOMPARE(replacedRoster->rows,
             QList<QStringList>({
                 {"Avery", "Fixture student", "Transferred row"}
             }));
    const Result<SpeakingEvalRows> replacedEvaluation =
        service.loadSpeakingEval(
            destinationClass, QStringLiteral("Fixture Evaluation"));
    QVERIFY(replacedEvaluation);
    SpeakingEvalRows expectedEvaluation = SpeakingEval::emptyRows();
    expectedEvaluation[0] = {
        QString(),
        QStringLiteral("Avery"),
        QStringLiteral("Fixture student"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("B"),
        QStringLiteral("A"),
        QStringLiteral("C"),
        QStringLiteral("B+"),
        QStringLiteral("Fixture evaluation comment"),
        QStringLiteral("Fixture evaluation note")
    };
    QCOMPARE(*replacedEvaluation, expectedEvaluation);
    const Result<SpeakingEvalRows> oldEvaluation = service.loadSpeakingEval(
        destinationClass, QStringLiteral("Destination Only Evaluation"));
    QVERIFY(oldEvaluation);
    QVERIFY(oldEvaluation->isEmpty());

    const Result<Teacher> retainedTeacher =
        service.getTeacher(destinationTeacher);
    QVERIFY(retainedTeacher);
    QCOMPARE(retainedTeacher->id, destinationTeacher);
    QCOMPARE(retainedTeacher->wifiPassword,
             QStringLiteral("local-only-password"));

    QString snapshotError;
    const auto snapshot = persistedDatabaseSnapshot(
        service.databaseSession()->database(), &snapshotError);
    QVERIFY2(snapshot.has_value(), qPrintable(snapshotError));
    const QByteArray snapshotHash = QCryptographicHash::hash(
        *snapshot, QCryptographicHash::Sha256).toHex();
    QVERIFY2(snapshotHash == QByteArrayLiteral("ae65cb0a14393a9da0c9a546320f233531e324a4ef0bbfeee7f1a8306701ee6b"),
             snapshotHash.constData());
}

void ClassTransferTests::importDialogBatchesDistinctDestinationSubtitleReads()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());

    Teacher displayTeacher = completeTeacher();
    displayTeacher.preferredRomanization = QStringLiteral("Alex");
    displayTeacher.preferredName = QStringLiteral("Alex");
    const int destinationTeacher = createdTeacherId(service, displayTeacher);
    const int destinationClassA = addCompleteClass(
        service,
        destinationTeacher,
        QStringLiteral("Local A"),
        QStringLiteral("E3"),
        QStringLiteral("Orion"),
        QStringLiteral("Thursday"),
        QStringLiteral("Student A"),
        QStringLiteral("Evaluation A"),
        QStringLiteral("11:00 AM"),
        QStringLiteral("11:50 AM")
        );
    const int destinationClassB = addCompleteClass(
        service,
        destinationTeacher,
        QStringLiteral("Local B"),
        QStringLiteral("E4"),
        QStringLiteral("Hydra"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Student B"),
        QStringLiteral("Evaluation B"),
        QStringLiteral("2:00 PM"),
        QStringLiteral("2:50 PM")
        );
    const int destinationClassC = addCompleteClass(
        service,
        destinationTeacher,
        QStringLiteral("Local C"),
        QStringLiteral("M1"),
        QStringLiteral("Pegasus"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Student C"),
        QStringLiteral("Evaluation C"),
        QStringLiteral("3:00 PM"),
        QStringLiteral("3:50 PM")
        );
    QVERIFY(destinationClassA > 0);
    QVERIFY(destinationClassB > 0);
    QVERIFY(destinationClassC > 0);

    QString applicationServicesError;
    const auto applicationServices = openApplicationServicesForCurrentDatabase(
        service,
        &applicationServicesError
        );
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    DatabaseSession* const session = applicationServices->databaseSession();
    QVERIFY(session);
    ClassInfoRepository* const classRepository = session->classInfoRepository();
    TeacherRepository* const teacherRepository = session->teacherRepository();
    QVERIFY(classRepository);
    QVERIFY(teacherRepository);

    ClassTransferPackage package;
    ClassTransferClass incomingA;
    incomingA.key = QStringLiteral("incoming-a");
    incomingA.name = QStringLiteral("Incoming A");
    incomingA.info = completeClassInfo(
        -1, -1, QStringLiteral("E3"), QStringLiteral("Orion"),
        QStringLiteral("Thursday")
        );
    package.classes.append(incomingA);
    ClassTransferClass incomingB;
    incomingB.key = QStringLiteral("incoming-b");
    incomingB.name = QStringLiteral("Incoming B");
    incomingB.info = completeClassInfo(
        -1, -1, QStringLiteral("E4"), QStringLiteral("Hydra"),
        QStringLiteral("Wednesday")
        );
    package.classes.append(incomingB);

    ClassImportPreview preview;
    preview.classes.append(ClassImportClassPreview{
        0,
        {destinationClassB, destinationClassA, destinationClassB}
    });
    preview.classes.append(ClassImportClassPreview{
        1,
        {destinationClassA, destinationClassB}
    });
    preview.classes.append(ClassImportClassPreview{
        99,
        {destinationClassC}
    });

    const ClassSubtitleBatchReadMetrics classMetricsBefore =
        classRepository->classSubtitleBatchReadMetrics();
    const TeacherDisplayNameBatchReadMetrics teacherMetricsBefore =
        teacherRepository->teacherDisplayNameBatchReadMetrics();
    ClassImportDialog dialog(applicationServices.get(), package, preview);

    auto* classChoiceA = dialog.findChild<QComboBox*>(
        QStringLiteral("classImportChoice_0"));
    auto* classChoiceB = dialog.findChild<QComboBox*>(
        QStringLiteral("classImportChoice_1"));
    QVERIFY(classChoiceA);
    QVERIFY(classChoiceB);
    QCOMPARE(classChoiceA->count(), 5);
    QCOMPARE(classChoiceA->itemText(1),
             QStringLiteral("Replace: E4 Hydra • Alex • Wed (2:00)"));
    QCOMPARE(classChoiceA->itemText(2),
             QStringLiteral("Replace: E3 Orion • Alex • Thurs (11:00)"));
    QCOMPARE(classChoiceA->itemText(3),
             QStringLiteral("Replace: E4 Hydra • Alex • Wed (2:00)"));
    QCOMPARE(classChoiceA->itemData(1, Qt::UserRole + 1).toInt(),
             destinationClassB);
    QCOMPARE(classChoiceA->itemData(2, Qt::UserRole + 1).toInt(),
             destinationClassA);
    QCOMPARE(classChoiceA->itemData(3, Qt::UserRole + 1).toInt(),
             destinationClassB);
    QCOMPARE(classChoiceB->count(), 4);
    QCOMPARE(classChoiceB->itemText(1),
             QStringLiteral("Replace: E3 Orion • Alex • Thurs (11:00)"));
    QCOMPARE(classChoiceB->itemText(2),
             QStringLiteral("Replace: E4 Hydra • Alex • Wed (2:00)"));
    QCOMPARE(classChoiceB->itemData(1, Qt::UserRole + 1).toInt(),
             destinationClassA);
    QCOMPARE(classChoiceB->itemData(2, Qt::UserRole + 1).toInt(),
             destinationClassB);

    const ClassSubtitleBatchReadMetrics classMetricsAfterBatch =
        classRepository->classSubtitleBatchReadMetrics();
    const TeacherDisplayNameBatchReadMetrics teacherMetricsAfterBatch =
        teacherRepository->teacherDisplayNameBatchReadMetrics();
    QCOMPARE(classMetricsAfterBatch.callCount - classMetricsBefore.callCount, 1);
    QCOMPARE(classMetricsAfterBatch.requestedClassCount
                 - classMetricsBefore.requestedClassCount,
             2);
    QCOMPARE(classMetricsAfterBatch.metadataStatementCount
                 - classMetricsBefore.metadataStatementCount,
             1);
    QCOMPARE(classMetricsAfterBatch.regularScheduleStatementCount
                 - classMetricsBefore.regularScheduleStatementCount,
             1);
    QCOMPARE(teacherMetricsAfterBatch.callCount - teacherMetricsBefore.callCount,
             1);
    QCOMPARE(teacherMetricsAfterBatch.statementCount
                 - teacherMetricsBefore.statementCount,
             1);

    ClassImportPreview noMatchesPreview;
    noMatchesPreview.classes.append(
        ClassImportClassPreview{0, QList<int>{}}
        );
    noMatchesPreview.classes.append(
        ClassImportClassPreview{99, {destinationClassC}}
        );
    ClassImportDialog noMatchesDialog(
        applicationServices.get(),
        package,
        noMatchesPreview
        );
    QVERIFY(noMatchesDialog.findChild<QComboBox*>(
        QStringLiteral("classImportChoice_0")));
    QVERIFY(!noMatchesDialog.findChild<QComboBox*>(
        QStringLiteral("classImportChoice_99")));

    const ClassSubtitleBatchReadMetrics classMetricsAfterNoMatches =
        classRepository->classSubtitleBatchReadMetrics();
    const TeacherDisplayNameBatchReadMetrics teacherMetricsAfterNoMatches =
        teacherRepository->teacherDisplayNameBatchReadMetrics();
    QCOMPARE(classMetricsAfterNoMatches.callCount,
             classMetricsAfterBatch.callCount);
    QCOMPARE(classMetricsAfterNoMatches.requestedClassCount,
             classMetricsAfterBatch.requestedClassCount);
    QCOMPARE(classMetricsAfterNoMatches.metadataStatementCount,
             classMetricsAfterBatch.metadataStatementCount);
    QCOMPARE(classMetricsAfterNoMatches.regularScheduleStatementCount,
             classMetricsAfterBatch.regularScheduleStatementCount);
    QCOMPARE(teacherMetricsAfterNoMatches.callCount,
             teacherMetricsAfterBatch.callCount);
    QCOMPARE(teacherMetricsAfterNoMatches.statementCount,
             teacherMetricsAfterBatch.statementCount);
}

void ClassTransferTests::malformedNonpositiveReviewTargetsAreRejectedAtLegacyBoundary()
{
    const QString fixturePath =
        QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
            QStringLiteral("tests/fixtures/transfers/success_source.json")
            );
    const auto package = ClassTransferJsonCodec::loadFile(fixturePath);
    QVERIFY2(
        package.has_value(),
        package ? "" : qPrintable(package.error())
        );

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());

    for (const int invalidTarget : {0, -2})
    {
        ClassImportPlan classTargetPlan = createAllPlan(*package);
        classTargetPlan.classes.first().targetClassId = invalidTarget;
        const auto invalidClassTarget = service.importClasses(
            *package, classTargetPlan);
        QVERIFY(!invalidClassTarget.has_value());
        QVERIFY(invalidClassTarget.error().contains(
            QStringLiteral(
                "Only replacement actions may specify a destination class.")));

        ClassImportPlan replaceClassPlan = createAllPlan(*package);
        replaceClassPlan.classes.first().action = ClassImportAction::Replace;
        replaceClassPlan.classes.first().targetClassId = invalidTarget;
        const auto invalidReplacementTarget = service.importClasses(
            *package, replaceClassPlan);
        QVERIFY(!invalidReplacementTarget.has_value());
        QVERIFY(invalidReplacementTarget.error().contains(
            QStringLiteral(
                "A replacement class is not one of the inferred matches.")));

        ClassImportPlan teacherTargetPlan = createAllPlan(*package);
        teacherTargetPlan.teachers.first().targetTeacherId = invalidTarget;
        const auto invalidTeacherTarget = service.importClasses(
            *package, teacherTargetPlan);
        QVERIFY(!invalidTeacherTarget.has_value());
        QVERIFY(invalidTeacherTarget.error().contains(
            QStringLiteral(
                "An unambiguous teacher match must reuse the local teacher.")));

        ClassImportPlan keepTeacherPlan = createAllPlan(*package);
        keepTeacherPlan.teachers.first().action =
            TeacherImportAction::KeepExisting;
        keepTeacherPlan.teachers.first().targetTeacherId = invalidTarget;
        const auto invalidKeptTeacherTarget = service.importClasses(
            *package, keepTeacherPlan);
        QVERIFY(!invalidKeptTeacherTarget.has_value());
        QVERIFY(invalidKeptTeacherTarget.error().contains(
            QStringLiteral(
                "A selected teacher is not one of the inferred matches.")));
    }

    ClassImportPlan invalidClassActionPlan = createAllPlan(*package);
    invalidClassActionPlan.classes.first().action =
        static_cast<ClassImportAction>(99);
    invalidClassActionPlan.classes.first().targetClassId = 0;
    const auto invalidClassAction = service.importClasses(
        *package, invalidClassActionPlan);
    QVERIFY(!invalidClassAction.has_value());
    QVERIFY(invalidClassAction.error().contains(
        QStringLiteral(
            "The class import plan contains an invalid or duplicate class entry.")));

    ClassImportPlan invalidTeacherActionPlan = createAllPlan(*package);
    invalidTeacherActionPlan.teachers.first().action =
        static_cast<TeacherImportAction>(99);
    invalidTeacherActionPlan.teachers.first().targetTeacherId = 0;
    const auto invalidTeacherAction = service.importClasses(
        *package, invalidTeacherActionPlan);
    QVERIFY(!invalidTeacherAction.has_value());
    QVERIFY(invalidTeacherAction.error().contains(
        QStringLiteral(
            "The teacher import plan contains an invalid or duplicate teacher entry.")));

    QCOMPARE(service.getClasses().value_or(QList<Classroom>{}).size(), 0);
    QCOMPARE(service.getAllTeachers().value_or(QList<Teacher>{}).size(), 0);
}

void ClassTransferTests::exportDialogStartsClearAndSortsClassesAlphabetically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("dialog.db"))).has_value());
    const int teacherId = createdTeacherId(service, completeTeacher());
    QVERIFY(teacherId > 0);
    const int zuluClass = createdClassId(service, QStringLiteral("Zulu"));
    const int alphaClass = createdClassId(service, QStringLiteral("Alpha"));
    const int mikeClass = createdClassId(service, QStringLiteral("Mike"));

    QVERIFY(service.saveClassInfo(completeClassInfo(
        zuluClass, teacherId, QStringLiteral("E4"), QStringLiteral("Zulu"),
        QStringLiteral("Wednesday"))));
    QVERIFY(service.saveClassInfo(completeClassInfo(
        alphaClass, teacherId, QStringLiteral("E4"), QStringLiteral("Alpha"),
        QStringLiteral("Monday"))));
    QVERIFY(service.saveClassInfo(completeClassInfo(
        mikeClass, teacherId, QStringLiteral("E4"), QStringLiteral("Mike"),
        QStringLiteral("Tuesday"))));

    QString applicationServicesError;
    const auto applicationServices =
        openApplicationServicesForCurrentDatabase(
            service, &applicationServicesError);
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    DatabaseSession* const session = applicationServices->databaseSession();
    QVERIFY(session);
    ClassInfoRepository* const classRepository = session->classInfoRepository();
    TeacherRepository* const teacherRepository = session->teacherRepository();
    QVERIFY(classRepository);
    QVERIFY(teacherRepository);
    const ClassSubtitleBatchReadMetrics classMetricsBefore =
        classRepository->classSubtitleBatchReadMetrics();
    const TeacherDisplayNameBatchReadMetrics teacherMetricsBefore =
        teacherRepository->teacherDisplayNameBatchReadMetrics();

    ClassExportDialog dialog(applicationServices.get());
    QCOMPARE(dialog.selectedClassIds(), QList<int>());

    auto* classList = dialog.findChild<QListWidget*>(
        QStringLiteral("classExportList"));
    QVERIFY(classList);
    QCOMPARE(classList->count(), 3);
    QCOMPARE(classList->item(0)->text(),
             QStringLiteral("E4 Alpha • Gim Allekseu • Mon (4:00)"));
    QCOMPARE(classList->item(1)->text(),
             QStringLiteral("E4 Mike • Gim Allekseu • Tues (4:00)"));
    QCOMPARE(classList->item(2)->text(),
             QStringLiteral("E4 Zulu • Gim Allekseu • Wed (4:00)"));
    QCOMPARE(classList->item(0)->data(Qt::UserRole).toInt(), alphaClass);
    QCOMPARE(classList->item(1)->data(Qt::UserRole).toInt(), mikeClass);
    QCOMPARE(classList->item(2)->data(Qt::UserRole).toInt(), zuluClass);

    const ClassSubtitleBatchReadMetrics classMetricsAfter =
        classRepository->classSubtitleBatchReadMetrics();
    const TeacherDisplayNameBatchReadMetrics teacherMetricsAfter =
        teacherRepository->teacherDisplayNameBatchReadMetrics();
    QCOMPARE(classMetricsAfter.callCount - classMetricsBefore.callCount, 1);
    QCOMPARE(classMetricsAfter.requestedClassCount
                 - classMetricsBefore.requestedClassCount,
             3);
    QCOMPARE(classMetricsAfter.metadataStatementCount
                 - classMetricsBefore.metadataStatementCount,
             1);
    QCOMPARE(classMetricsAfter.regularScheduleStatementCount
                 - classMetricsBefore.regularScheduleStatementCount,
             1);
    QCOMPARE(teacherMetricsAfter.callCount - teacherMetricsBefore.callCount,
             1);
    QCOMPARE(teacherMetricsAfter.statementCount
                 - teacherMetricsBefore.statementCount,
             1);

    for (int index = 0; index < classList->count(); ++index)
    {
        QCOMPARE(classList->item(index)->checkState(), Qt::Unchecked);
    }

    auto* exportButton = dialog.findChild<QPushButton*>(
        QStringLiteral("exportClassesButton"));
    QVERIFY(exportButton);
    QVERIFY(!exportButton->isEnabled());

    classList->item(1)->setCheckState(Qt::Checked);
    QCOMPARE(dialog.selectedClassIds(), QList<int>({mikeClass}));
    QVERIFY(exportButton->isEnabled());
}

void ClassTransferTests::
exportDialogSkipsSubtitleBatchReadForEmptyClassList()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("dialog.db"))).has_value());

    QString applicationServicesError;
    const auto applicationServices =
        openApplicationServicesForCurrentDatabase(
            service, &applicationServicesError);
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    DatabaseSession* const session = applicationServices->databaseSession();
    QVERIFY(session);
    ClassInfoRepository* const classRepository = session->classInfoRepository();
    TeacherRepository* const teacherRepository = session->teacherRepository();
    QVERIFY(classRepository);
    QVERIFY(teacherRepository);
    const ClassSubtitleBatchReadMetrics classMetricsBefore =
        classRepository->classSubtitleBatchReadMetrics();
    const TeacherDisplayNameBatchReadMetrics teacherMetricsBefore =
        teacherRepository->teacherDisplayNameBatchReadMetrics();

    ClassExportDialog dialog(applicationServices.get());

    auto* classList = dialog.findChild<QListWidget*>(
        QStringLiteral("classExportList"));
    QVERIFY(classList);
    QCOMPARE(classList->count(), 0);
    QCOMPARE(dialog.selectedClassIds(), QList<int>());

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

void ClassTransferTests::
exportDialogShowsWarningAndStaysEmptyWhenClassListCannotLoad()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("dialog.db"))).has_value());

    QString applicationServicesError;
    const auto applicationServices =
        openApplicationServicesForCurrentDatabase(
            service, &applicationServicesError);
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    QSqlQuery dropClasses(applicationServices->databaseSession()->database());
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

    ClassExportDialog dialog(applicationServices.get());

    QVERIFY(warningCaptured);
    QCOMPARE(warningTitle, QStringLiteral("Export Classes"));
    QCOMPARE(warningText, QStringLiteral("Classes could not be loaded."));
    QVERIFY2(!warningDetails.isEmpty(), "The database error details were not shown.");
    QVERIFY(warningDetails.contains(QStringLiteral("classes"),
                                    Qt::CaseInsensitive));

    auto* classList = dialog.findChild<QListWidget*>(
        QStringLiteral("classExportList"));
    QVERIFY(classList);
    QCOMPARE(classList->count(), 0);
    QVERIFY(classList->isEnabled());
    QCOMPARE(dialog.selectedClassIds(), QList<int>());

    auto* exportButton = dialog.findChild<QPushButton*>(
        QStringLiteral("exportClassesButton"));
    QVERIFY(exportButton);
    QVERIFY(!exportButton->isEnabled());
}

void ClassTransferTests::exportDialogUsesDefaultFormattingWhenClassFieldsCannotLoad()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("dialog.db"))).has_value());
    const int teacherId = createdTeacherId(service, completeTeacher());
    QVERIFY(teacherId > 0);
    const int classId = createdClassId(service, QStringLiteral("Stored class"));
    QVERIFY(classId > 0);
    QVERIFY(service.saveClassInfo(completeClassInfo(
        classId, teacherId, QStringLiteral("E4"), QStringLiteral("Orion"),
        QStringLiteral("Monday"))));

    QString applicationServicesError;
    const auto applicationServices =
        openApplicationServicesForCurrentDatabase(
            service, &applicationServicesError);
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    QSqlQuery dropClassTimes(applicationServices->databaseSession()->database());
    QVERIFY2(dropClassTimes.exec(QStringLiteral("DROP TABLE class_times")),
             qPrintable(dropClassTimes.lastError().text()));

    ClassExportDialog dialog(applicationServices.get());
    auto* classList = dialog.findChild<QListWidget*>(
        QStringLiteral("classExportList"));
    QVERIFY(classList);
    QCOMPARE(classList->count(), 1);
    QCOMPARE(classList->item(0)->text(),
             QStringLiteral("Unknown Class • No Teacher"));
    QCOMPARE(classList->item(0)->data(Qt::UserRole).toInt(), classId);
    QCOMPARE(classList->item(0)->checkState(), Qt::Unchecked);
}

void ClassTransferTests::exportDialogKeepsClassFieldsWhenTeacherCannotLoad()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("dialog.db"))).has_value());
    const int teacherId = createdTeacherId(service, completeTeacher());
    QVERIFY(teacherId > 0);
    const int classId = createdClassId(service, QStringLiteral("Stored class"));
    QVERIFY(classId > 0);
    QVERIFY(service.saveClassInfo(completeClassInfo(
        classId, teacherId, QStringLiteral("E4"), QStringLiteral("Orion"),
        QStringLiteral("Monday"))));

    QString applicationServicesError;
    const auto applicationServices =
        openApplicationServicesForCurrentDatabase(
            service, &applicationServicesError);
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    QSqlQuery dropTeachers(applicationServices->databaseSession()->database());
    QVERIFY2(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")),
             qPrintable(dropTeachers.lastError().text()));

    ClassExportDialog dialog(applicationServices.get());
    auto* classList = dialog.findChild<QListWidget*>(
        QStringLiteral("classExportList"));
    QVERIFY(classList);
    QCOMPARE(classList->count(), 1);
    QCOMPARE(classList->item(0)->text(),
             QStringLiteral("E4 Orion • No Teacher • Mon (4:00)"));
    QCOMPARE(classList->item(0)->data(Qt::UserRole).toInt(), classId);
    QCOMPARE(classList->item(0)->checkState(), Qt::Unchecked);
}

void ClassTransferTests::filesystemSafeJsonFileName()
{
    QCOMPARE(
        FileNameUtils::filesystemSafeJsonFileName(
            QStringLiteral("Class: A/B?.json"), QStringLiteral("Class")),
        QStringLiteral("Class_ A_B_.json")
        );
    QCOMPARE(
        FileNameUtils::filesystemSafeJsonFileName(
            QStringLiteral("CON"), QStringLiteral("Class")),
        QStringLiteral("_CON.json")
        );
    QCOMPARE(
        FileNameUtils::filesystemSafeJsonFileName(
            QStringLiteral("LPT9.json"), QStringLiteral("Class")),
        QStringLiteral("_LPT9.json")
        );
    QCOMPARE(
        FileNameUtils::filesystemSafeJsonFileName(
            QStringLiteral("  ...  "), QStringLiteral("Fallback")),
        QStringLiteral("Fallback.json")
        );
    QCOMPARE(
        FileNameUtils::filesystemSafeJsonFileName(
            QStringLiteral("Lesson.JSON"), QStringLiteral("Class")),
        QStringLiteral("Lesson.json")
        );
    QCOMPARE(
        FileNameUtils::filesystemSafeJsonFileName(
            QStringLiteral("A%1B").arg(QChar(1)), QStringLiteral("Class")),
        QStringLiteral("A_B.json")
        );

    const QString longName(300, u'a');
    const QString safeLongName = FileNameUtils::filesystemSafeJsonFileName(
        longName, QStringLiteral("Class"));
    QVERIFY(safeLongName.endsWith(QStringLiteral(".json")));
    QVERIFY(safeLongName.toUtf8().size() <= 245);
}

void ClassTransferTests::importDialogRequiresAmbiguousTeacherResolution()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = createdTeacherId(service, completeTeacher());
    const int sourceClass = createdClassId(service, QString());
    ClassInfo sourceInfo;
    sourceInfo.classId = sourceClass;
    sourceInfo.teacherId = sourceTeacher;
    sourceInfo.classGrade = QStringLiteral("E4");
    sourceInfo.classLevel = QStringLiteral("Theseus");
    QVERIFY(service.saveClassInfo(sourceInfo));
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    QVERIFY(service.createTeacher(completeTeacher()).has_value());
    QVERIFY(service.createTeacher(completeTeacher()).has_value());
    const auto preview = service.previewClassImport(*package);
    QVERIFY(preview.has_value());
    QCOMPARE(preview->teachers.first().matchingTeacherIds.size(), 2);
    QCOMPARE(preview->teachers.first().matchingTeacherIds,
             QList<int>({1, 2}));

    QString applicationServicesError;
    const auto applicationServices =
        openApplicationServicesForCurrentDatabase(
            service, &applicationServicesError);
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    ClassImportDialog dialog(
        applicationServices.get(), *package, *preview);
    auto* importButton = dialog.findChild<QPushButton*>(
        QStringLiteral("importClassesButton"));
    auto* teacherChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("teacherImportChoice_")
            + package->teachers.first().key);
    QVERIFY(importButton);
    QVERIFY(teacherChoice);
    QCOMPARE(teacherChoice->count(), 6);
    QCOMPARE(teacherChoice->itemText(0),
             QStringLiteral("Choose a teacher resolution…"));
    QCOMPARE(teacherChoice->itemText(1),
             QStringLiteral("Create new teacher"));
    QCOMPARE(teacherChoice->itemText(2),
             QStringLiteral("Keep local: Gim Allekseu"));
    QCOMPARE(teacherChoice->itemText(3),
             QStringLiteral("Replace local: Gim Allekseu"));
    QCOMPARE(teacherChoice->itemText(4),
             QStringLiteral("Keep local: Gim Allekseu"));
    QCOMPARE(teacherChoice->itemText(5),
             QStringLiteral("Replace local: Gim Allekseu"));
    QCOMPARE(teacherChoice->itemData(0, Qt::UserRole).toInt(), -1);
    QCOMPARE(teacherChoice->itemData(0, Qt::UserRole + 1).toInt(), -1);
    QCOMPARE(teacherChoice->itemData(1, Qt::UserRole).toInt(),
             static_cast<int>(TeacherImportAction::Create));
    QCOMPARE(teacherChoice->itemData(1, Qt::UserRole + 1).toInt(), -1);
    QCOMPARE(teacherChoice->itemData(2, Qt::UserRole).toInt(),
             static_cast<int>(TeacherImportAction::KeepExisting));
    QCOMPARE(teacherChoice->itemData(2, Qt::UserRole + 1).toInt(), 1);
    QCOMPARE(teacherChoice->itemData(3, Qt::UserRole).toInt(),
             static_cast<int>(TeacherImportAction::ReplaceExisting));
    QCOMPARE(teacherChoice->itemData(3, Qt::UserRole + 1).toInt(), 1);
    QCOMPARE(teacherChoice->itemData(4, Qt::UserRole).toInt(),
             static_cast<int>(TeacherImportAction::KeepExisting));
    QCOMPARE(teacherChoice->itemData(4, Qt::UserRole + 1).toInt(), 2);
    QCOMPARE(teacherChoice->itemData(5, Qt::UserRole).toInt(),
             static_cast<int>(TeacherImportAction::ReplaceExisting));
    QCOMPARE(teacherChoice->itemData(5, Qt::UserRole + 1).toInt(), 2);
    QVERIFY(!importButton->isEnabled());

    teacherChoice->setCurrentIndex(1);
    QVERIFY(importButton->isEnabled());
    QCOMPARE(dialog.importPlan().teachers.first().action,
             TeacherImportAction::Create);
}

void ClassTransferTests::
importDialogBatchesDistinctMatchedTeacherDisplayNameReads()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());

    Teacher preferredTeacher = completeTeacher(QStringLiteral("English One"));
    preferredTeacher.preferredRomanization = QStringLiteral("Romanized One");
    preferredTeacher.preferredName = QStringLiteral("Preferred One");
    const int preferredId = createdTeacherId(service, preferredTeacher);

    Teacher englishTeacher = completeTeacher(QStringLiteral("English Two"));
    englishTeacher.preferredRomanization = QStringLiteral("Romanized Two");
    englishTeacher.preferredName.clear();
    const int englishId = createdTeacherId(service, englishTeacher);

    Teacher romanizedTeacher = completeTeacher(QStringLiteral("English Three"));
    romanizedTeacher.preferredRomanization = QStringLiteral("Romanized Three");
    romanizedTeacher.preferredName.clear();
    const int romanizedId = createdTeacherId(service, romanizedTeacher);

    Teacher koreanTeacher = completeTeacher(QStringLiteral("English Four"));
    koreanTeacher.preferredRomanization.clear();
    koreanTeacher.preferredName.clear();
    const int koreanId = createdTeacherId(service, koreanTeacher);
    QVERIFY(preferredId > 0);
    QVERIFY(englishId > 0);
    QVERIFY(romanizedId > 0);
    QVERIFY(koreanId > 0);

    QString applicationServicesError;
    const auto applicationServices = openApplicationServicesForCurrentDatabase(
        service,
        &applicationServicesError
        );
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    DatabaseSession* const session = applicationServices->databaseSession();
    QVERIFY(session);
    TeacherRepository* const teacherRepository = session->teacherRepository();
    QVERIFY(teacherRepository);

    const auto clearEnglishName =
        [teacherRepository](const int teacherId)
    {
        const Result<Teacher> loaded = teacherRepository->getTeacher(teacherId);
        if (!loaded)
        {
            return false;
        }
        Teacher updated = loaded.value();
        updated.teacherEn.clear();
        return teacherRepository->updateTeacher(updated).has_value();
    };
    const auto clearNamesExceptKorean =
        [teacherRepository](const int teacherId)
    {
        const Result<Teacher> loaded = teacherRepository->getTeacher(teacherId);
        if (!loaded)
        {
            return false;
        }
        Teacher updated = loaded.value();
        updated.teacherEn.clear();
        updated.preferredRomanization.clear();
        updated.preferredName.clear();
        updated.teacherKr = QStringLiteral("Korean Four");
        return teacherRepository->updateTeacher(updated).has_value();
    };
    QVERIFY(clearEnglishName(romanizedId));
    QVERIFY(clearNamesExceptKorean(koreanId));

    ClassTransferPackage package;
    package.teachers.append({
        QStringLiteral("incoming-one"),
        completeTeacher()
    });
    package.teachers.append({
        QStringLiteral("incoming-two"),
        completeTeacher()
    });
    package.teachers.append({
        QStringLiteral("incoming-empty"),
        completeTeacher()
    });

    ClassImportPreview preview;
    preview.teachers.append({
        QStringLiteral("incoming-one"),
        QList<int>{preferredId, preferredId, englishId}
    });
    preview.teachers.append({
        QStringLiteral("missing-package-teacher"),
        QList<int>{romanizedId}
    });
    preview.teachers.append({
        QStringLiteral("incoming-two"),
        QList<int>{englishId, romanizedId, koreanId, 999999, preferredId}
    });
    preview.teachers.append({
        QStringLiteral("incoming-empty"),
        QList<int>{}
    });

    const TeacherDisplayNameBatchReadMetrics before =
        teacherRepository->teacherDisplayNameBatchReadMetrics();
    ClassImportDialog dialog(applicationServices.get(), package, preview);

    auto* firstChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("teacherImportChoice_incoming-one"));
    auto* secondChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("teacherImportChoice_incoming-two"));
    auto* emptyChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("teacherImportChoice_incoming-empty"));
    QVERIFY(firstChoice);
    QVERIFY(secondChoice);
    QVERIFY(emptyChoice);
    QVERIFY(!dialog.findChild<QComboBox*>(
        QStringLiteral("teacherImportChoice_missing-package-teacher")));

    QCOMPARE(firstChoice->count(), 8);
    QCOMPARE(firstChoice->itemText(0),
             QStringLiteral("Choose a teacher resolution…"));
    QCOMPARE(firstChoice->itemText(1), QStringLiteral("Create new teacher"));
    QCOMPARE(firstChoice->itemText(2),
             QStringLiteral("Keep local: Preferred One"));
    QCOMPARE(firstChoice->itemText(3),
             QStringLiteral("Replace local: Preferred One"));
    QCOMPARE(firstChoice->itemText(4),
             QStringLiteral("Keep local: Preferred One"));
    QCOMPARE(firstChoice->itemText(5),
             QStringLiteral("Replace local: Preferred One"));
    QCOMPARE(firstChoice->itemText(6),
             QStringLiteral("Keep local: English Two"));
    QCOMPARE(firstChoice->itemText(7),
             QStringLiteral("Replace local: English Two"));
    QCOMPARE(firstChoice->itemData(2, Qt::UserRole).toInt(),
             static_cast<int>(TeacherImportAction::KeepExisting));
    QCOMPARE(firstChoice->itemData(2, Qt::UserRole + 1).toInt(), preferredId);
    QCOMPARE(firstChoice->itemData(4, Qt::UserRole + 1).toInt(), preferredId);
    QCOMPARE(firstChoice->itemData(6, Qt::UserRole + 1).toInt(), englishId);

    QCOMPARE(secondChoice->count(), 12);
    QCOMPARE(secondChoice->itemText(2),
             QStringLiteral("Keep local: English Two"));
    QCOMPARE(secondChoice->itemText(3),
             QStringLiteral("Replace local: English Two"));
    QCOMPARE(secondChoice->itemText(4),
             QStringLiteral("Keep local: Romanized Three"));
    QCOMPARE(secondChoice->itemText(5),
             QStringLiteral("Replace local: Romanized Three"));
    QCOMPARE(secondChoice->itemText(6),
             QStringLiteral("Keep local: Korean Four"));
    QCOMPARE(secondChoice->itemText(7),
             QStringLiteral("Replace local: Korean Four"));
    QCOMPARE(secondChoice->itemText(8),
             QStringLiteral("Keep local: New Teacher"));
    QCOMPARE(secondChoice->itemText(9),
             QStringLiteral("Replace local: New Teacher"));
    QCOMPARE(secondChoice->itemText(10),
             QStringLiteral("Keep local: Preferred One"));
    QCOMPARE(secondChoice->itemText(11),
             QStringLiteral("Replace local: Preferred One"));
    QCOMPARE(secondChoice->itemData(2, Qt::UserRole + 1).toInt(), englishId);
    QCOMPARE(secondChoice->itemData(4, Qt::UserRole + 1).toInt(), romanizedId);
    QCOMPARE(secondChoice->itemData(6, Qt::UserRole + 1).toInt(), koreanId);
    QCOMPARE(secondChoice->itemData(8, Qt::UserRole + 1).toInt(), 999999);
    QCOMPARE(secondChoice->itemData(10, Qt::UserRole + 1).toInt(), preferredId);
    QCOMPARE(emptyChoice->count(), 1);
    QCOMPARE(emptyChoice->itemText(0), QStringLiteral("Create new teacher"));

    const TeacherDisplayNameBatchReadMetrics after =
        teacherRepository->teacherDisplayNameBatchReadMetrics();
    QCOMPARE(after.callCount - before.callCount, 1);
    QCOMPARE(after.requestedTeacherCount - before.requestedTeacherCount, 5);
    QCOMPARE(after.statementCount - before.statementCount, 1);

    ClassImportPreview noReadPreview;
    noReadPreview.teachers.append({
        QStringLiteral("incoming-one"),
        QList<int>{}
    });
    noReadPreview.teachers.append({
        QStringLiteral("missing-package-teacher"),
        QList<int>{preferredId}
    });
    ClassImportDialog noReadDialog(
        applicationServices.get(), package, noReadPreview);
    QVERIFY(noReadDialog.findChild<QComboBox*>(
        QStringLiteral("teacherImportChoice_incoming-one")));
    QVERIFY(!noReadDialog.findChild<QComboBox*>(
        QStringLiteral("teacherImportChoice_missing-package-teacher")));
    const TeacherDisplayNameBatchReadMetrics afterNoRead =
        teacherRepository->teacherDisplayNameBatchReadMetrics();
    QCOMPARE(afterNoRead.callCount, after.callCount);
    QCOMPARE(afterNoRead.requestedTeacherCount, after.requestedTeacherCount);
    QCOMPARE(afterNoRead.statementCount, after.statementCount);
}

void ClassTransferTests::
importDialogFallsBackToIndividualTeacherProfilesAfterBatchFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());

    Teacher romanizedTeacher = completeTeacher(QStringLiteral(""));
    romanizedTeacher.preferredRomanization = QStringLiteral("Romanized One");
    romanizedTeacher.preferredName.clear();
    const int romanizedId = createdTeacherId(service, romanizedTeacher);
    Teacher englishTeacher = completeTeacher(QStringLiteral("English Two"));
    englishTeacher.preferredRomanization = QStringLiteral("Romanized Two");
    englishTeacher.preferredName.clear();
    const int englishId = createdTeacherId(service, englishTeacher);
    QVERIFY(romanizedId > 0);
    QVERIFY(englishId > 0);

    QString applicationServicesError;
    const auto applicationServices = openApplicationServicesForCurrentDatabase(
        service,
        &applicationServicesError
        );
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    DatabaseSession* const session = applicationServices->databaseSession();
    QVERIFY(session);
    TeacherRepository* const teacherRepository = session->teacherRepository();
    QVERIFY(teacherRepository);
    const TeacherDisplayNameBatchReadMetrics before =
        teacherRepository->teacherDisplayNameBatchReadMetrics();

    QSqlQuery renameColumn(session->database());
    QVERIFY2(renameColumn.exec(QStringLiteral(
        "ALTER TABLE teachers RENAME COLUMN preferred_name TO old_preferred_name"
        )), qPrintable(renameColumn.lastError().text()));

    ClassTransferPackage package;
    package.teachers.append({
        QStringLiteral("incoming"),
        completeTeacher()
    });
    ClassImportPreview preview;
    preview.teachers.append({
        QStringLiteral("incoming"),
        QList<int>{romanizedId, englishId}
    });

    ClassImportDialog dialog(applicationServices.get(), package, preview);
    auto* teacherChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("teacherImportChoice_incoming"));
    QVERIFY(teacherChoice);
    QCOMPARE(teacherChoice->count(), 6);
    QCOMPARE(teacherChoice->itemText(2),
             QStringLiteral("Keep local: Romanized One"));
    QCOMPARE(teacherChoice->itemText(3),
             QStringLiteral("Replace local: Romanized One"));
    QCOMPARE(teacherChoice->itemText(4),
             QStringLiteral("Keep local: English Two"));
    QCOMPARE(teacherChoice->itemText(5),
             QStringLiteral("Replace local: English Two"));

    const TeacherDisplayNameBatchReadMetrics after =
        teacherRepository->teacherDisplayNameBatchReadMetrics();
    QCOMPARE(after.callCount - before.callCount, 1);
    QCOMPARE(after.requestedTeacherCount - before.requestedTeacherCount, 2);
    QCOMPARE(after.statementCount - before.statementCount, 1);
}

void ClassTransferTests::importDialogUsesDefaultFormattingWhenClassFieldsCannotLoad()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = createdTeacherId(service, completeTeacher());
    const int sourceClass = addCompleteClass(
        service, sourceTeacher, QStringLiteral("Incoming"),
        QStringLiteral("E4"), QStringLiteral("Orion"),
        QStringLiteral("Monday"), QStringLiteral("Incoming student"));
    QVERIFY(sourceClass > 0);
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const int destinationTeacher =
        createdTeacherId(service, completeTeacher());
    const int destinationClass = addCompleteClass(
        service, destinationTeacher, QStringLiteral("Destination"),
        QStringLiteral("E4"), QStringLiteral("Orion"),
        QStringLiteral("Monday"), QStringLiteral("Destination student"));
    QVERIFY(destinationClass > 0);
    const auto preview = service.previewClassImport(*package);
    QVERIFY2(preview, preview ? "" : qPrintable(preview.error()));
    QCOMPARE(preview->classes.first().matchingClassIds,
             QList<int>({destinationClass}));

    QString applicationServicesError;
    const auto applicationServices =
        openApplicationServicesForCurrentDatabase(
            service, &applicationServicesError);
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    QSqlQuery dropClassTimes(applicationServices->databaseSession()->database());
    QVERIFY2(dropClassTimes.exec(QStringLiteral("DROP TABLE class_times")),
             qPrintable(dropClassTimes.lastError().text()));

    ClassImportDialog dialog(applicationServices.get(), *package, *preview);
    auto* classChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("classImportChoice_0"));
    QVERIFY(classChoice);
    QCOMPARE(classChoice->count(), 3);
    QCOMPARE(classChoice->itemText(1),
             QStringLiteral("Replace: Unknown Class • No Teacher"));
    QCOMPARE(classChoice->itemData(1, Qt::UserRole + 1).toInt(),
             destinationClass);
}

void ClassTransferTests::importDialogKeepsClassFieldsWhenTeacherCannotLoad()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = createdTeacherId(service, completeTeacher());
    const int sourceClass = addCompleteClass(
        service, sourceTeacher, QStringLiteral("Incoming"),
        QStringLiteral("E4"), QStringLiteral("Orion"),
        QStringLiteral("Monday"), QStringLiteral("Incoming student"));
    QVERIFY(sourceClass > 0);
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const int destinationTeacher =
        createdTeacherId(service, completeTeacher());
    const int destinationClass = addCompleteClass(
        service, destinationTeacher, QStringLiteral("Destination"),
        QStringLiteral("E4"), QStringLiteral("Orion"),
        QStringLiteral("Monday"), QStringLiteral("Destination student"));
    QVERIFY(destinationClass > 0);
    const auto preview = service.previewClassImport(*package);
    QVERIFY2(preview, preview ? "" : qPrintable(preview.error()));
    QCOMPARE(preview->classes.first().matchingClassIds,
             QList<int>({destinationClass}));

    QString applicationServicesError;
    const auto applicationServices =
        openApplicationServicesForCurrentDatabase(
            service, &applicationServicesError);
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    QSqlQuery dropTeachers(applicationServices->databaseSession()->database());
    QVERIFY2(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")),
             qPrintable(dropTeachers.lastError().text()));

    ClassImportDialog dialog(applicationServices.get(), *package, *preview);
    auto* classChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("classImportChoice_0"));
    QVERIFY(classChoice);
    QCOMPARE(classChoice->count(), 3);
    QCOMPARE(classChoice->itemText(1),
             QStringLiteral("Replace: E4 Orion • No Teacher • Mon (4:00)"));
    QCOMPARE(classChoice->itemData(1, Qt::UserRole + 1).toInt(),
             destinationClass);
}

void ClassTransferTests::importDialogUsesNewTeacherLabelWhenTeacherProfileCannotLoad()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = createdTeacherId(service, completeTeacher());
    const int sourceClass = addCompleteClass(
        service, sourceTeacher, QStringLiteral("Incoming"),
        QStringLiteral("E4"), QStringLiteral("Orion"),
        QStringLiteral("Monday"), QStringLiteral("Incoming student"));
    QVERIFY(sourceClass > 0);
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const int destinationTeacher =
        createdTeacherId(service, completeTeacher());
    const int destinationClass = addCompleteClass(
        service, destinationTeacher, QStringLiteral("Destination"),
        QStringLiteral("E4"), QStringLiteral("Orion"),
        QStringLiteral("Monday"), QStringLiteral("Destination student"));
    QVERIFY(destinationClass > 0);
    auto preview = service.previewClassImport(*package);
    QVERIFY2(preview, preview ? "" : qPrintable(preview.error()));
    QCOMPARE(preview->teachers.first().matchingTeacherIds,
             QList<int>({destinationTeacher}));
    preview->classes.clear();

    QString applicationServicesError;
    const auto applicationServices =
        openApplicationServicesForCurrentDatabase(
            service, &applicationServicesError);
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    TeacherRepository* const teacherRepository =
        applicationServices->databaseSession()->teacherRepository();
    QVERIFY(teacherRepository);
    const TeacherDisplayNameBatchReadMetrics before =
        teacherRepository->teacherDisplayNameBatchReadMetrics();
    QSqlQuery dropTeachers(applicationServices->databaseSession()->database());
    QVERIFY2(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")),
             qPrintable(dropTeachers.lastError().text()));

    ClassImportDialog dialog(applicationServices.get(), *package, *preview);
    auto* teacherChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("teacherImportChoice_")
            + package->teachers.first().key);
    QVERIFY(teacherChoice);
    QCOMPARE(teacherChoice->count(), 2);
    QCOMPARE(teacherChoice->itemText(0),
             QStringLiteral("Keep local: New Teacher"));
    QCOMPARE(teacherChoice->itemText(1),
             QStringLiteral("Replace local: New Teacher"));
    QCOMPARE(teacherChoice->itemData(0, Qt::UserRole).toInt(),
             static_cast<int>(TeacherImportAction::KeepExisting));
    QCOMPARE(teacherChoice->itemData(0, Qt::UserRole + 1).toInt(),
             destinationTeacher);
    QCOMPARE(teacherChoice->itemData(1, Qt::UserRole).toInt(),
             static_cast<int>(TeacherImportAction::ReplaceExisting));
    QCOMPARE(teacherChoice->itemData(1, Qt::UserRole + 1).toInt(),
             destinationTeacher);

    const TeacherDisplayNameBatchReadMetrics after =
        teacherRepository->teacherDisplayNameBatchReadMetrics();
    QCOMPARE(after.callCount - before.callCount, 1);
    QCOMPARE(after.requestedTeacherCount - before.requestedTeacherCount, 1);
    QCOMPARE(after.statementCount - before.statementCount, 1);
}

void ClassTransferTests::dialogRejectsDuplicateReplacementTargets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = createdTeacherId(service, completeTeacher());
    const int firstSourceClass = addCompleteClass(
        service,
        sourceTeacher,
        QStringLiteral("Source One"),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Monday"),
        QStringLiteral("First Student")
        );
    const int secondSourceClass = addCompleteClass(
        service,
        sourceTeacher,
        QStringLiteral("Source Two"),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Second Student")
        );
    QVERIFY(firstSourceClass > 0);
    QVERIFY(secondSourceClass > 0);
    const auto package = service.buildClassTransferPackage(
        {firstSourceClass, secondSourceClass});
    QVERIFY(package.has_value());
    QCOMPARE(package->classes.size(), 2);

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const int destinationTeacher = createdTeacherId(
        service, completeTeacher());
    const int destinationClass = addCompleteClass(
        service,
        destinationTeacher,
        QStringLiteral("Destination"),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Friday"),
        QStringLiteral("Existing Student")
        );
    const auto preview = service.previewClassImport(*package);
    QVERIFY(preview.has_value());
    QCOMPARE(preview->classes.size(), 2);
    QCOMPARE(preview->classes[0].matchingClassIds,
             QList<int>({destinationClass}));
    QCOMPARE(preview->classes[1].matchingClassIds,
             QList<int>({destinationClass}));

    QString applicationServicesError;
    const auto applicationServices =
        openApplicationServicesForCurrentDatabase(
            service, &applicationServicesError);
    QVERIFY2(applicationServices, qPrintable(applicationServicesError));
    ClassImportDialog dialog(applicationServices.get(), *package, *preview);
    auto* firstChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("classImportChoice_0"));
    auto* secondChoice = dialog.findChild<QComboBox*>(
        QStringLiteral("classImportChoice_1"));
    auto* importButton = dialog.findChild<QPushButton*>(
        QStringLiteral("importClassesButton"));
    auto* validationLabel = dialog.findChild<QLabel*>(
        QStringLiteral("importValidationLabel"));
    QVERIFY(firstChoice);
    QVERIFY(secondChoice);
    QVERIFY(importButton);
    QVERIFY(validationLabel);
    firstChoice->setCurrentIndex(1);
    QVERIFY(importButton->isEnabled());
    secondChoice->setCurrentIndex(1);
    QVERIFY(!importButton->isEnabled());
    QCOMPARE(
        validationLabel->text(),
        QStringLiteral(
            "Two package classes cannot replace the same destination class.")
        );
}

void ClassTransferTests::applyRejectsReplacementOutsideCurrentPreviewMatches()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DataService service;
    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("source.db"))).has_value());
    const int sourceTeacher = createdTeacherId(service, completeTeacher());
    const int sourceClass = addCompleteClass(
        service,
        sourceTeacher,
        QStringLiteral("Source Class"),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Monday"),
        QStringLiteral("Incoming Student")
        );
    const auto package = service.buildClassTransferPackage({sourceClass});
    QVERIFY(package.has_value());

    QVERIFY(service.openDatabase(
        directory.filePath(QStringLiteral("destination.db"))).has_value());
    const int destinationTeacher = createdTeacherId(
        service, completeTeacher());
    const int destinationClass = addCompleteClass(
        service,
        destinationTeacher,
        QStringLiteral("Original Destination"),
        QStringLiteral("E4"),
        QStringLiteral("Perseus"),
        QStringLiteral("Friday"),
        QStringLiteral("Existing Student")
        );
    const auto preview = service.previewClassImport(*package);
    QVERIFY(preview.has_value());
    QCOMPARE(preview->classes.first().matchingClassIds,
             QList<int>({destinationClass}));

    ClassImportPlan plan;
    plan.classes.append({0, ClassImportAction::Replace, destinationClass});
    plan.teachers.append({
        package->teachers.first().key,
        TeacherImportAction::KeepExisting,
        destinationTeacher
    });

    const Result<ClassInfo> initialInfo =
        service.loadClassInfo(destinationClass);
    QVERIFY(initialInfo);
    ClassInfo staleInfo = *initialInfo;
    staleInfo.classGrade = QStringLiteral("E5");
    QVERIFY(service.saveClassInfo(staleInfo));
    const auto teacherBefore = service.getTeacher(destinationTeacher);
    QVERIFY(teacherBefore.has_value());
    const auto classBefore = service.getClassById(destinationClass);
    QVERIFY(classBefore.has_value());
    const Result<ClassInfo> infoBefore = service.loadClassInfo(destinationClass);
    QVERIFY(infoBefore);
    const Result<Roster> rosterBefore = service.loadRoster(destinationClass);
    QVERIFY(rosterBefore);

    const auto result = service.importClasses(*package, plan);
    QVERIFY(!result.has_value());
    QVERIFY(result.error().contains(
        QStringLiteral("replacement class is not one of the inferred matches")));
    QCOMPARE(service.getAllTeachers().value_or(QList<Teacher>{}).size(), 1);
    QCOMPARE(service.getClasses().value_or(QList<Classroom>{}).size(), 1);
    QCOMPARE(service.getTeacher(destinationTeacher)->wifiPassword,
             teacherBefore->wifiPassword);
    QCOMPARE(service.getClassById(destinationClass)->name, classBefore->name);
    QCOMPARE(service.loadClassInfo(destinationClass)->classGrade,
             infoBefore->classGrade);
    QCOMPARE(service.loadClassInfo(destinationClass)->classColor,
             infoBefore->classColor);
    QCOMPARE(service.loadRoster(destinationClass)->rows.first().first(),
             rosterBefore->rows.first().first());
}

QTEST_MAIN(ClassTransferTests)

#include "class_transfer_tests.moc"
