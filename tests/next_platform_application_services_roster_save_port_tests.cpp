#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/roster_repository.h"
#include "next/application/roster_save_use_case.h"
#include "domain/validation/roster_validator.h"
#include "features/roster/ui/roster_save_validation_adapter.h"
#include <random>
#include "next/application/roster_row_transfer_use_case.h"
#include "next/platform/application_services_roster_save_port.h"
#include "next/platform/application_services_roster_read_port.h"
#include "next/platform/application_services_roster_row_transfer_save_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <vector>
#include <initializer_list>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("roster-save-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

std::vector<Application::RosterSnapshotColumnWidth> snapshotWidths(
    const std::initializer_list<int> values
    )
{
    std::vector<Application::RosterSnapshotColumnWidth> result;
    result.reserve(values.size());
    for (const int value : values)
        result.emplace_back(value);
    return result;
}

const std::vector<std::u16string> transferBaseColumns{
    u"English", u"Korean", u"Winter", u"Speech Contest", u"Summer", u"Fall"
};

Application::RosterSnapshot transferSnapshot(
    const std::vector<std::u16string>& firstRow,
    const std::vector<std::u16string>& secondRow = {})
{
    Application::RosterSnapshot snapshot;
    snapshot.columns = transferBaseColumns;
    snapshot.columnWidths = snapshotWidths({101, 102, 103, 104, 105, 106});
    snapshot.rows.resize(25, std::vector<std::u16string>(transferBaseColumns.size()));
    snapshot.rows[0] = firstRow;
    if (!secondRow.empty())
        snapshot.rows[1] = secondRow;
    return snapshot;
}

Roster legacyTransferRoster(const Application::RosterSnapshot& snapshot)
{
    Roster roster;
    for (const auto& column : snapshot.columns)
        roster.columns.append(QString::fromStdU16String(column));
    for (const Application::RosterSnapshotColumnWidth width : snapshot.columnWidths)
        roster.columnWidths.append(width.rawValue());
    for (const auto& sourceRow : snapshot.rows)
    {
        QStringList row;
        for (const auto& cell : sourceRow)
            row.append(QString::fromStdU16String(cell));
        roster.rows.append(std::move(row));
    }
    return roster;
}

struct AcceptRosterTransferConfirmation final
    : Application::RosterRowTransferConfirmationPort
{
    bool confirmTargetQuestionableKoreanNameLengths(
        const Application::RosterSnapshot&,
        const std::vector<Application::RosterSaveIssue>&
        ) const override
    {
        return true;
    }
};

Application::RosterSaveRequest request(
    const int classId,
    const bool allowQuestionableKoreanNameLengths = false
    )
{
    Application::RosterSnapshot snapshot;
    snapshot.columns = {
        u"English",
        u"Korean",
        u"Winter",
        u"Speech Contest",
        u"Summer",
        u"Fall",
        u"備考"
    };
    snapshot.columnWidths = snapshotWidths({172, 123, 131, 132, 133, 134, 207});
    snapshot.rows.resize(25);
    for (int row = 0; row < 25; ++row)
    {
        std::u16string english = u"Student ";
        english.push_back(static_cast<char16_t>(u'A' + row));

        snapshot.rows[static_cast<std::size_t>(row)] = {
            std::move(english),
            u"김민지",
            row % 2 == 0 ? u"A+" : u"B",
            u"B+",
            u"A",
            u"B",
            row == 0 ? u"수업 \U0001F4DA" : u""
        };
    }

    const auto classIdentifier = Domain::ClassId::fromString(
        std::to_string(classId)
        );
    if (!classIdentifier)
    {
        qFatal("Test class ID must have a typed representation.");
    }

    return {
        .classId = *classIdentifier,
        .roster = std::move(snapshot),
        .allowQuestionableKoreanNameLengths = allowQuestionableKoreanNameLengths
    };
}

}

class NextPlatformApplicationServicesRosterSavePortTests final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsNonCanonicalIdsBeforeSessionAccess();
    void preparationMatchesLegacyNormalizationAndOrderedMetadata();
    void invalidRosterPrecedesUnavailableSessionAndWritesNothing();
    void rawBomSnapshotPersistsDecodedTextExactlyOnce();
    void savesCompleteRosterSnapshotToOpenSession();
    void missingParentClassReturnsNotFoundWithoutWrites();
    void forwardsQuestionableKoreanNameDecision();
    void repositoryFailureMapsTechnicalAndRollsBack();
    void closedSessionReturnsFailureWithoutDataServiceFallback();
    void rowTransferClosedSessionReturnsRecoverableUnavailableFailure();
    void rowTransferPersistsPreparedRostersAndConfirmedKoreanLength();
    void rowTransferSecondRosterFailureRollsBackBothWrites();
};

void NextPlatformApplicationServicesRosterSavePortTests::
rejectsNonCanonicalIdsBeforeSessionAccess()
{
    ApplicationServices services;
    Platform::ApplicationServicesRosterSavePort port(services);

    for (const std::string classId : {
             "0",
             "-2",
             "+42",
             "class-42",
             " 42",
             "42 ",
             "01",
             "00042",
             "999999999999999999999"
         })
    {
        const auto typedId = Domain::ClassId::fromString(classId);
        QVERIFY(typedId);

        Application::RosterSaveRequest saveRequest = request(1);
        saveRequest.classId = *typedId;
        const auto result = port.saveRoster({.classId = saveRequest.classId, .roster = saveRequest.roster});
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }
}

void NextPlatformApplicationServicesRosterSavePortTests::
missingParentClassReturnsNotFoundWithoutWrites()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Deleted Roster Parent Test")
        );
    QVERIFY(createdClass);

    {
        QSqlQuery removeClass(services.databaseSession()->database());
        removeClass.prepare(QStringLiteral("DELETE FROM classes WHERE id=?"));
        removeClass.addBindValue(*createdClass);
        QVERIFY2(removeClass.exec(), qPrintable(removeClass.lastError().text()));
        QCOMPARE(removeClass.numRowsAffected(), 1);
    }

    Platform::ApplicationServicesRosterSavePort port(services);
    const auto assertMissingParent = [&](const Application::RosterSaveRequest& value)
    {
        const auto result = Application::RosterSaveUseCase::execute(
            value,
            port,
            Platform::rosterSaveCaseInsensitiveEquals
            );
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
        QVERIFY(!result.error().recoverable);
        QVERIFY(!result.error().message.empty());

        QSqlQuery counts(services.databaseSession()->database());
        counts.prepare(QStringLiteral(
            "SELECT "
            "(SELECT COUNT(*) FROM roster_columns WHERE class_id=?), "
            "(SELECT COUNT(*) FROM roster_data WHERE class_id=?)"
            ));
        counts.addBindValue(*createdClass);
        counts.addBindValue(*createdClass);
        QVERIFY2(counts.exec(), qPrintable(counts.lastError().text()));
        QVERIFY(counts.next());
        QCOMPARE(counts.value(0).toInt(), 0);
        QCOMPARE(counts.value(1).toInt(), 0);
    };

    const Application::RosterSaveRequest populated = request(*createdClass);
    assertMissingParent(populated);

    Application::RosterSaveRequest zeroRows = request(*createdClass);
    zeroRows.roster.rows.clear();
    QCOMPARE(zeroRows.roster.columns.size(), std::size_t{7});
    QVERIFY(zeroRows.roster.rows.empty());
    assertMissingParent(zeroRows);
}

void NextPlatformApplicationServicesRosterSavePortTests::
savesCompleteRosterSnapshotToOpenSession()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Roster Save Test")
        );
    QVERIFY(createdClass);

    Application::RosterSaveRequest saveRequest = request(*createdClass);
    saveRequest.roster.columns[0] = u" English ";
    saveRequest.roster.columns[3] = u" Speech   Contest ";
    saveRequest.roster.rows[0][0] = u"  Student   A  ";
    Platform::ApplicationServicesRosterSavePort port(services);
    const auto saved =
        Application::RosterSaveUseCase::execute(saveRequest, port, Platform::rosterSaveCaseInsensitiveEquals);
    QVERIFY2(
        saved,
        qPrintable(saved ? QString() : QString::fromStdString(saved.error().message))
        );

    RosterRepository* const repository =
        services.databaseSession()->rosterRepository();
    QVERIFY(repository);
    const auto persisted = repository->loadRoster(*createdClass);
    QVERIFY(persisted);
    QCOMPARE(persisted->columns,
        QStringList({
            "English", "Korean", "Winter", "Speech Contest",
            "Summer", "Fall", "備考"
        }));
    QCOMPARE(persisted->columnWidths,
        QVector<int>({172, 123, 131, 132, 133, 134, 207}));
    QCOMPARE(persisted->rows.size(), 25);
    QCOMPARE(persisted->rows.at(0).at(0), QStringLiteral("Student A"));
    QCOMPARE(persisted->rows.at(0).at(6), QStringLiteral("수업 \U0001F4DA"));
    QCOMPARE(persisted->rows.at(24).at(0), QStringLiteral("Student Y"));
    QCOMPARE(persisted->rows.at(24).at(2), QStringLiteral("A+"));
}

void NextPlatformApplicationServicesRosterSavePortTests::
forwardsQuestionableKoreanNameDecision()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Roster Save Validation Test")
        );
    QVERIFY(createdClass);

    Platform::ApplicationServicesRosterSavePort port(services);
    const auto originalSaved = Application::RosterSaveUseCase::execute(
        request(*createdClass),
        port, Platform::rosterSaveCaseInsensitiveEquals
        );
    QVERIFY2(
        originalSaved,
        qPrintable(
            originalSaved
                ? QString()
                : QString::fromStdString(originalSaved.error().message)
            )
        );

    Application::RosterSaveRequest saveRequest = request(*createdClass);
    saveRequest.roster.rows[0][1] = u"김";
    for (std::size_t row = 1; row < saveRequest.roster.rows.size(); ++row)
    {
        saveRequest.roster.rows[row] = {};
    }

    const auto rejected =
        Application::RosterSaveUseCase::execute(saveRequest, port, Platform::rosterSaveCaseInsensitiveEquals);
    QVERIFY(!rejected);
    QCOMPARE(rejected.error().code, Domain::ErrorCode::Validation);
    QVERIFY(!rejected.error().message.empty());

    RosterRepository* const repository =
        services.databaseSession()->rosterRepository();
    QVERIFY(repository);
    const auto beforeRejected = repository->loadRoster(*createdClass);
    QVERIFY(beforeRejected);
    const auto unchanged = repository->loadRoster(*createdClass);
    QVERIFY(unchanged);
    QCOMPARE(unchanged->columns, beforeRejected->columns);
    QCOMPARE(unchanged->columnWidths, beforeRejected->columnWidths);
    QCOMPARE(unchanged->rows, beforeRejected->rows);

    saveRequest.allowQuestionableKoreanNameLengths = true;
    const auto accepted =
        Application::RosterSaveUseCase::execute(saveRequest, port, Platform::rosterSaveCaseInsensitiveEquals);
    QVERIFY2(
        accepted,
        qPrintable(
            accepted ? QString() : QString::fromStdString(accepted.error().message)
            )
        );

    const auto persisted = repository->loadRoster(*createdClass);
    QVERIFY(persisted);
    QCOMPARE(persisted->rows.size(), 1);
    QCOMPARE(persisted->rows.at(0).at(1), QStringLiteral("김"));
}

void NextPlatformApplicationServicesRosterSavePortTests::
repositoryFailureMapsTechnicalAndRollsBack()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Roster Save Repository Failure Test")
        );
    QVERIFY(createdClass);

    Platform::ApplicationServicesRosterSavePort port(services);
    const Application::RosterSaveRequest original = request(*createdClass);
    const auto originalSaved =
        Application::RosterSaveUseCase::execute(original, port, Platform::rosterSaveCaseInsensitiveEquals);
    QVERIFY2(
        originalSaved,
        qPrintable(
            originalSaved
                ? QString()
                : QString::fromStdString(originalSaved.error().message)
            )
        );

    QSqlQuery trigger(services.databaseSession()->database());
    QVERIFY2(
        trigger.exec(QStringLiteral(
            "CREATE TRIGGER reject_roster_data_insert "
            "BEFORE INSERT ON roster_data "
            "WHEN NEW.value = 'Injected Failure' "
            "BEGIN "
            "SELECT RAISE(ABORT, 'injected roster failure'); "
            "END"
            )),
        qPrintable(trigger.lastError().text())
        );

    Application::RosterSaveRequest changed = original;
    changed.roster.rows[0][0] = u"Injected Failure";
    const auto failed =
        Application::RosterSaveUseCase::execute(changed, port, Platform::rosterSaveCaseInsensitiveEquals);
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!failed.error().message.empty());

    RosterRepository* const repository =
        services.databaseSession()->rosterRepository();
    QVERIFY(repository);
    const auto persisted = repository->loadRoster(*createdClass);
    QVERIFY(persisted);
    QCOMPARE(persisted->columns.size(), 7);
    QCOMPARE(persisted->columnWidths.size(), 7);
    QCOMPARE(persisted->rows.size(), 25);
    QCOMPARE(persisted->rows.at(0).at(0), QStringLiteral("Student A"));
    QCOMPARE(persisted->rows.at(24).at(0), QStringLiteral("Student Y"));
}

void NextPlatformApplicationServicesRosterSavePortTests::
closedSessionReturnsFailureWithoutDataServiceFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto createdClass = services.classService()->create(
        QStringLiteral("Closed Roster Save Test")
        );
    QVERIFY(createdClass);

    services.closeDatabase();
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(services.dataService());
    QVERIFY(!services.dataService()->isOpen());

    Platform::ApplicationServicesRosterSavePort port(services);
    const auto result = Application::RosterSaveUseCase::execute(
        request(*createdClass),
        port, Platform::rosterSaveCaseInsensitiveEquals
        );
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QCOMPARE(result.error().message, std::string("The roster service is unavailable."));
    QVERIFY(result.error().recoverable);
}

void NextPlatformApplicationServicesRosterSavePortTests::
rowTransferClosedSessionReturnsRecoverableUnavailableFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    services.closeDatabase();
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(services.dataService());
    QVERIFY(!services.dataService()->isOpen());

    const auto sourceId = Domain::ClassId::fromString("1");
    const auto targetId = Domain::ClassId::fromString("2");
    QVERIFY(sourceId);
    QVERIFY(targetId);
    Platform::ApplicationServicesRosterRowTransferSavePort port(&services);
    const auto result = port.saveTransfer({
        .sourceClassId = *sourceId,
        .targetClassId = *targetId,
        .sourceRoster = {},
        .targetRoster = {}
    });

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QCOMPARE(result.error().message, std::string("The roster service is unavailable."));
    QVERIFY(result.error().recoverable);
}

void NextPlatformApplicationServicesRosterSavePortTests::
rowTransferPersistsPreparedRostersAndConfirmedKoreanLength()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto sourceClass = services.classService()->create(
        QStringLiteral("Roster Transfer Source")
        );
    const auto targetClass = services.classService()->create(
        QStringLiteral("Roster Transfer Target")
        );
    QVERIFY(sourceClass);
    QVERIFY(targetClass);

    const auto sourceId = Domain::ClassId::fromString(std::to_string(*sourceClass));
    const auto targetId = Domain::ClassId::fromString(std::to_string(*targetClass));
    QVERIFY(sourceId);
    QVERIFY(targetId);

    const Application::RosterSnapshot source = transferSnapshot(
        {u"  Alice\t ", u" \uAE40 ", u"", u"", u"", u""},
        {u"  Bob\t Jones  ", u" \uBC15 \uC9C0\uBBFC ", u"", u"", u"", u""}
        );
    const Application::RosterSnapshot originalTarget = transferSnapshot(
        {u"Current", u"\uD64D\uAE38\uB3D9", u"", u"", u"", u""}
        );
    RosterRepository* const repository =
        services.databaseSession()->rosterRepository();
    QVERIFY(repository);
    QVERIFY(repository->saveRoster(*sourceClass, legacyTransferRoster(source)));
    QVERIFY(repository->saveRoster(*targetClass, legacyTransferRoster(originalTarget)));

    Platform::ApplicationServicesRosterReadPort readPort(services);
    Platform::ApplicationServicesRosterRowTransferSavePort savePort(&services);
    const AcceptRosterTransferConfirmation confirmation;
    const auto result = Application::RosterRowTransferUseCase::execute(
        {
            .sourceClassId = *sourceId,
            .targetClassId = *targetId,
            .sourceRoster = source,
            .sourceRow = Application::RosterRowTransferSourceIndex{0},
            .baseColumnNames = transferBaseColumns,
            .sourceQuestionableKoreanNameLengthsConfirmed = true
        },
        readPort,
        savePort,
        Platform::rosterSaveCaseInsensitiveEquals,
        &confirmation
        );
    const auto* transferred =
        std::get_if<Application::RosterRowTransferSuccess>(&result);
    QVERIFY(transferred);
    QCOMPARE(transferred->destinationRow.rawValue(), std::size_t{1});

    const auto savedSource = repository->loadRoster(*sourceClass);
    const auto savedTarget = repository->loadRoster(*targetClass);
    QVERIFY(savedSource);
    QVERIFY(savedTarget);
    QCOMPARE(savedSource->rows.size(), 1);
    QCOMPARE(savedSource->rows.at(0).at(0), QStringLiteral("Bob Jones"));
    QCOMPARE(savedSource->rows.at(0).at(1), QString::fromStdU16String(u"\uBC15\uC9C0\uBBFC"));
    QCOMPARE(savedTarget->rows.size(), 2);
    QCOMPARE(savedTarget->rows.at(1).at(0), QStringLiteral("Alice"));
    // A one-syllable Korean name is permitted by the UI only after its
    // questionable-length confirmation; the prepared text is persisted.
    QCOMPARE(savedTarget->rows.at(1).at(1), QString::fromStdU16String(u"\uAE40"));
}

void NextPlatformApplicationServicesRosterSavePortTests::
rowTransferSecondRosterFailureRollsBackBothWrites()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto sourceClass = services.classService()->create(
        QStringLiteral("Roster Transfer Rollback Source")
        );
    const auto targetClass = services.classService()->create(
        QStringLiteral("Roster Transfer Rollback Target")
        );
    QVERIFY(sourceClass);
    QVERIFY(targetClass);

    const auto sourceId = Domain::ClassId::fromString(std::to_string(*sourceClass));
    const auto targetId = Domain::ClassId::fromString(std::to_string(*targetClass));
    QVERIFY(sourceId);
    QVERIFY(targetId);
    const Application::RosterSnapshot originalSource = transferSnapshot(
        {u"Original Source", u"\uAE40\uBBFC\uC9C0", u"", u"", u"", u""}
        );
    const Application::RosterSnapshot originalTarget = transferSnapshot(
        {u"Original Target", u"\uBC15\uC9C0\uBBFC", u"", u"", u"", u""}
        );
    RosterRepository* const repository =
        services.databaseSession()->rosterRepository();
    QVERIFY(repository);
    QVERIFY(repository->saveRoster(*sourceClass, legacyTransferRoster(originalSource)));
    QVERIFY(repository->saveRoster(*targetClass, legacyTransferRoster(originalTarget)));
    const auto originalSourceStored = repository->loadRoster(*sourceClass);
    const auto originalTargetStored = repository->loadRoster(*targetClass);
    QVERIFY(originalSourceStored);
    QVERIFY(originalTargetStored);

    QSqlQuery trigger(services.databaseSession()->database());
    const QString createTrigger = QStringLiteral(
        "CREATE TRIGGER reject_second_transfer_roster_column "
        "BEFORE INSERT ON roster_columns WHEN NEW.class_id=%1 "
        "BEGIN SELECT RAISE(ABORT, 'injected second roster failure'); END"
        ).arg(*targetClass);
    QVERIFY2(trigger.exec(createTrigger), qPrintable(trigger.lastError().text()));

    Platform::ApplicationServicesRosterRowTransferSavePort port(&services);
    const auto failed = port.saveTransfer({
        .sourceClassId = *sourceId,
        .targetClassId = *targetId,
        .sourceRoster = transferSnapshot(
            {u"Changed Source", u"\uAE40\uBBFC\uC9C0", u"", u"", u"", u""}),
        .targetRoster = transferSnapshot(
            {u"Changed Target", u"\uC774\uC601\uC218", u"", u"", u"", u""})
    });
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!failed.error().message.empty());

    const auto persistedSource = repository->loadRoster(*sourceClass);
    const auto persistedTarget = repository->loadRoster(*targetClass);
    QVERIFY(persistedSource);
    QVERIFY(persistedTarget);
    QCOMPARE(persistedSource->columns, originalSourceStored->columns);
    QCOMPARE(persistedSource->columnWidths, originalSourceStored->columnWidths);
    QCOMPARE(persistedSource->rows, originalSourceStored->rows);
    QCOMPARE(persistedTarget->columns, originalTargetStored->columns);
    QCOMPARE(persistedTarget->columnWidths, originalTargetStored->columnWidths);
    QCOMPARE(persistedTarget->rows, originalTargetStored->rows);
}


void NextPlatformApplicationServicesRosterSavePortTests::
preparationMatchesLegacyNormalizationAndOrderedMetadata()
{
    const auto compare = [](const Application::RosterSnapshot& source, const bool allow)
    {
        const auto prepared = Application::prepareRosterSave(source, allow,
            Platform::rosterSaveCaseInsensitiveEquals);
        Roster legacy;
        for (const auto& column : source.columns)
            legacy.columns.append(QString::fromStdU16String(column));
        for (const Application::RosterSnapshotColumnWidth width : source.columnWidths)
            legacy.columnWidths.append(width.rawValue());
        for (const auto& row : source.rows)
        {
            QStringList cells;
            for (const auto& cell : row) cells.append(QString::fromStdU16String(cell));
            legacy.rows.append(cells);
        }
        const Roster normalized = RosterValidator::normalized(legacy);
        QCOMPARE(prepared.roster.columns.size(), static_cast<std::size_t>(normalized.columns.size()));
        for (std::size_t column = 0; column < prepared.roster.columns.size(); ++column)
            QCOMPARE(RosterSaveValidationAdapter::text(prepared.roster.columns[column]), normalized.columns[column]);
        QCOMPARE(prepared.roster.rows.size(), static_cast<std::size_t>(normalized.rows.size()));
        for (std::size_t row = 0; row < prepared.roster.rows.size(); ++row)
        {
            QCOMPARE(prepared.roster.rows[row].size(), static_cast<std::size_t>(normalized.rows[row].size()));
            for (std::size_t column = 0; column < prepared.roster.rows[row].size(); ++column)
                QCOMPARE(RosterSaveValidationAdapter::text(prepared.roster.rows[row][column]), normalized.rows[row][column]);
        }
        QVERIFY(prepared.roster.columnWidths == source.columnWidths);
        const auto actual = RosterSaveValidationAdapter::validation(prepared);
        const auto expected = RosterValidator::validate(normalized, allow);
        QCOMPARE(actual.issues().size(), expected.issues().size());
        for (qsizetype index = 0; index < expected.issues().size(); ++index)
        {
            const auto& a = actual.issues()[index];
            const auto& e = expected.issues()[index];
            QCOMPARE(a.code, e.code);
            QCOMPARE(a.field, e.field);
            QCOMPARE(a.row, e.row);
            QCOMPARE(a.column, e.column);
            QCOMPARE(a.severity, e.severity);
            QCOMPARE(a.arguments, e.arguments);
            for (auto it = e.arguments.begin(); it != e.arguments.end(); ++it)
                QCOMPARE(a.arguments.value(it.key()).metaType(), it.value().metaType());
        }
        // Compare the saved error string, including non-BMP and unpaired UTF-16.
        if (expected.hasErrors())
        {
            QStringList errors;
            for (const auto& issue : expected.errors())
            {
                QString detail = issue.field.isEmpty() ? issue.code : issue.field + ": " + issue.code;
                if (issue.row >= 0 && !issue.field.contains(u'['))
                    detail.prepend(QStringLiteral("row %1, ").arg(issue.row + 1));
                errors.append(detail);
            }
            Platform::ApplicationServicesRosterSavePort closed(nullptr);
            const auto saved = Application::RosterSaveUseCase::execute(
                {.classId = *Domain::ClassId::fromString("42"), .roster = source,
                 .allowQuestionableKoreanNameLengths = allow}, closed,
                Platform::rosterSaveCaseInsensitiveEquals);
            QVERIFY(!saved);
            QCOMPARE(saved.error().message,
                (QStringLiteral("Roster validation failed: ") + errors.join("; ")).toUtf8().toStdString());
        }
    };
    auto base = request(42).roster;
    base.rows.clear();
    base.columnWidths = snapshotWidths({-5, 0, 1});
    const std::vector<std::u16string> names{
        u"", u"  ", u"aLICE", u"a . b", u"a-b", u"a--b", u"A..b", u"A . B .",
        u"Mary - JANE", u"alice2", u"O'Brien", u"Zo\u00eb", u"A\U0001f4da", u"\u0000",
        std::u16string(21, u'a'), std::u16string(10001, u'a'),
        u"\uAE40", u"\uAE40\uBBFC", u"\uAE40\uBBFC\uC9C0", u"\uAE40\uBBFC\uC9C0\uC218",
        u"\uAE40\uBBFC\uC9C0\uC218\uC218", u" \uAE40 \uBBFC \uC9C0 (b) ",
        u"\uAE40(B)", u"\uAE40(bbb)", u"(a)", u" (a)", u"\u3131", u"\uAE402",
        std::u16string{char16_t(0xd800)}, std::u16string{char16_t(0xdc00)},
        u"\ufeffAlice", u"\ufeff\ufeffAlice", std::u16string{char16_t(0xfffe),char16_t(0x4100)},
        std::u16string{u'A',char16_t(0xd800),u'B'}, std::u16string{u'A',0,u'B'}
    };
    for (const auto& name : names)
    {
        auto value = base;
        value.rows = {{name, name, u"", u"", u"", u"", u"  A\t B "}};
        compare(value, false);
        compare(value, true);
    }
    for (const char16_t whitespace : {char16_t(9), char16_t(10), char16_t(11), char16_t(12),
             char16_t(13), char16_t(32), char16_t(0x85), char16_t(0xa0), char16_t(0x1680),
             char16_t(0x2000), char16_t(0x200a), char16_t(0x2028), char16_t(0x2029),
             char16_t(0x202f), char16_t(0x205f), char16_t(0x3000), char16_t(0x200b)})
    {
        auto value = base;
        value.columns[0] = std::u16string(1, whitespace) + u"ENGLISH" + whitespace;
        value.rows = {{u"Alice", u"\uAE40" + std::u16string(1, whitespace) + u"\uBBFC\uC9C0",
                       u" a" + std::u16string(1, whitespace) + u"b "}};
        compare(value, false);
    }
    auto structure = base;
    structure.columns = {u"English", u"Korean", u" Autumn ", u"Fall", u"", std::u16string(65, u'x'),
                         u"\u00c4", u"\u00e4", u"\u03a3", u"\u03c2", u"\U00010400", u"\U00010428",
                         u"\U0001f4da", std::u16string{char16_t(0xd800)}};
    structure.columnWidths.resize(15);
    structure.rows.resize(26);
    structure.rows[0].resize(16);
    structure.rows[0][12] = std::u16string(10001, u'X');
    structure.rows[0][13] = std::u16string(10001, u'X');
    structure.rows[0][15] = std::u16string(10001, u'X');
    structure.rows[1] = {u"alice", u"\uAE40\uBBFC\uC9C0"};
    structure.rows[2] = {u"bob", u"\uAE40\uBBFC\uC9C0"};
    structure.rows[3] = structure.rows[2];
    structure.rows[4] = structure.rows[1];
    compare(structure, false);
    compare(structure, true);
    compare({}, false);
    // Deterministic malformed/valid combinations exercise normalization regexes.
    std::mt19937 random(345);
    const std::u16string alphabet = u"aAbB .-2()\t\n\uAE40\uBBFC\uC9C0\u00a0\u0085";
    for (int index = 0; index < 400; ++index)
    {
        std::u16string name;
        for (int size = static_cast<int>(random() % 15); size > 0; --size)
            name.push_back(alphabet[random() % alphabet.size()]);
        auto value = base;
        value.rows = {{name, name}};
        compare(value, (index % 2) == 0);
    }
}

void NextPlatformApplicationServicesRosterSavePortTests::
invalidRosterPrecedesUnavailableSessionAndWritesNothing()
{
    Platform::ApplicationServicesRosterSavePort unavailable(nullptr);
    auto value = request(42);
    value.roster.rows[0][0] = u"Invalid2";
    const auto rejected = Application::RosterSaveUseCase::execute(value, unavailable,
        Platform::rosterSaveCaseInsensitiveEquals);
    QVERIFY(!rejected);
    QCOMPARE(rejected.error().code, Domain::ErrorCode::Validation);
    QVERIFY(rejected.error().message.starts_with("Roster validation failed: "));
    QTemporaryDir directory;
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto created = services.classService()->create(QStringLiteral("No Writes"));
    QVERIFY(created);
    value = request(*created);
    value.roster.rows[0][0] = u"Invalid2";
    QSqlQuery fail(services.databaseSession()->database());
    QVERIFY(fail.exec(QStringLiteral("CREATE TRIGGER reject_any_roster_column BEFORE INSERT ON roster_columns "
        "BEGIN SELECT RAISE(ABORT, 'repository was called'); END")));
    Platform::ApplicationServicesRosterSavePort open(services);
    const auto result = Application::RosterSaveUseCase::execute(value, open,
        Platform::rosterSaveCaseInsensitiveEquals);
    QVERIFY(!result);
    QVERIFY(result.error().message.starts_with("Roster validation failed: "));
    QSqlQuery count(services.databaseSession()->database());
    QVERIFY(count.exec(QStringLiteral("SELECT (SELECT COUNT(*) FROM roster_columns), "
        "(SELECT COUNT(*) FROM roster_data)")));
    QVERIFY(count.next());
    QCOMPARE(count.value(0).toInt(), 0);
    QCOMPARE(count.value(1).toInt(), 0);
}


void NextPlatformApplicationServicesRosterSavePortTests::rawBomSnapshotPersistsDecodedTextExactlyOnce()
{
    QTemporaryDir directory;
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto created = services.classService()->create(QStringLiteral("BOM storage"));
    const auto legacyClass = services.classService()->create(QStringLiteral("Legacy BOM storage"));
    const auto directClass = services.classService()->create(QStringLiteral("Prepared BOM storage"));
    QVERIFY(created);
    QVERIFY(legacyClass);
    QVERIFY(directClass);
    auto value = request(*created);
    value.roster.columns[0] = u"\ufeffEnglish";
    value.roster.rows[0][0] = u"\ufeffAlice";
    value.roster.rows[0][6] = u"\ufeff\ufeffNotes";
    value.roster.rows[1][6] = std::u16string{char16_t(0xfffe), char16_t(0x4200)};
    value.roster.rows[2][6] = u"\ufeff\ufeff\ufeffTriple";
    value.roster.rows[3][6] = u"\ufeffLeading";
    const auto original = value;
    const auto prepared = Application::prepareRosterSave(value.roster, false,
        Platform::rosterSaveCaseInsensitiveEquals);
    QVERIFY(!prepared.hasErrors());
    QCOMPARE(prepared.roster.rows[0][6], std::u16string(u"\ufeffNotes"));
    QCOMPARE(prepared.roster.rows[1][6], std::u16string(u"B"));
    QCOMPARE(prepared.roster.rows[2][6], std::u16string(u"\ufeff\ufeffTriple"));
    QCOMPARE(prepared.roster.rows[3][6], std::u16string(u"Leading"));
    Platform::ApplicationServicesRosterSavePort port(services);
    QVERIFY(Application::RosterSaveUseCase::execute(value, port, Platform::rosterSaveCaseInsensitiveEquals));
    QVERIFY(value == original);
    // A direct port call accepts logical prepared text and must not decode it again.
    QVERIFY(port.saveRoster({.classId = *Domain::ClassId::fromString(std::to_string(*directClass)),
        .roster = prepared.roster}));
    Roster legacy;
    for (const auto& column : value.roster.columns)
        legacy.columns.append(QString::fromStdU16String(column));
    for (const Application::RosterSnapshotColumnWidth width : value.roster.columnWidths)
        legacy.columnWidths.append(width.rawValue());
    for (const auto& row : value.roster.rows)
    {
        QStringList cells;
        for (const auto& cell : row) cells.append(QString::fromStdU16String(cell));
        legacy.rows.append(cells);
    }
    legacy = RosterValidator::normalized(legacy);
    QVERIFY(!RosterValidator::validate(legacy).hasErrors());
    auto* const repository = services.databaseSession()->rosterRepository();
    QVERIFY(repository->saveRoster(*legacyClass, legacy));
    // SQLite's UTF-16 binding consumes one leading BOM. Compare actual bytes
    // with the unchanged legacy save rather than relying on load conversion.
    const auto storedBytes = [&](const int id, const int row)
    {
        QSqlQuery bytes(services.databaseSession()->database());
        bytes.prepare(QStringLiteral("SELECT hex(CAST(value AS BLOB)) FROM roster_data WHERE class_id=? AND row_index=? AND col_index=6"));
        bytes.addBindValue(id);
        bytes.addBindValue(row);
        if (!bytes.exec() || !bytes.next())
            return QStringLiteral("query failed");
        return bytes.value(0).toString();
    };
    const QStringList expected{QStringLiteral("4E6F746573"), QStringLiteral("42"),
        QStringLiteral("EFBBBF547269706C65"), QStringLiteral("4C656164696E67")};
    for (int row = 0; row < 4; ++row)
    {
        QCOMPARE(storedBytes(*legacyClass, row), expected[row]);
        QCOMPARE(storedBytes(*created, row), storedBytes(*legacyClass, row));
        QCOMPARE(storedBytes(*directClass, row), storedBytes(*legacyClass, row));
    }
    const auto stored = repository->loadRoster(*created);
    const auto legacyStored = repository->loadRoster(*legacyClass);
    QVERIFY(stored);
    QVERIFY(legacyStored);
    QCOMPARE(stored->columns, legacyStored->columns);
    QCOMPARE(stored->columnWidths, legacyStored->columnWidths);
    QCOMPARE(stored->rows, legacyStored->rows);
}


QTEST_MAIN(NextPlatformApplicationServicesRosterSavePortTests)

#include "next_platform_application_services_roster_save_port_tests.moc"
