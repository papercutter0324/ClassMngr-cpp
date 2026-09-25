#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "domain/models/classroom.h"
#include "domain/models/roster.h"
#include "domain/models/speaking_evaluation.h"
#include "domain/validation/validation_result.h"
#include "ui/shared/pages/basepage.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QList>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>
#include <QtTest>
#include <QUuid>

// RosterModel always restores Roster::BaseColumns in setRoster(). Expose its
// private column list here only to drive the import slot's defensive warning
// branch with malformed in-memory state; persisted rosters cannot reach it.
#define private public
#include "features/roster/ui/roster_model.h"
#include "features/roster/ui/roster_editor_widget.h"
#undef private

#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"

namespace
{

struct EvaluationFixture
{
    QString name;
    QStringList componentGrades;
    QString expectedFinalGrade;
};

const QList<EvaluationFixture>& evaluations()
{
    static const QList<EvaluationFixture> values{
        {
            QStringLiteral("Winter"),
            {
                QStringLiteral("A+"),
                QStringLiteral("B+"),
                QStringLiteral("B+"),
                QStringLiteral("B"),
                QStringLiteral("B"),
                QStringLiteral("C")
            },
            QStringLiteral("B+")
        },
        {
            QStringLiteral("Speech Contest"),
            QStringList(6, QStringLiteral("B+")),
            QStringLiteral("B+")
        },
        {
            QStringLiteral("Summer"),
            QStringList(6, QStringLiteral("C")),
            QStringLiteral("C")
        },
        {
            QStringLiteral("Fall"),
            QStringList(6, QStringLiteral("A+")),
            QStringLiteral("A+")
        }
    };
    return values;
}

QStringList speakingEvaluationRow(
    const QString& english,
    const QString& korean,
    const QStringList& componentGrades
)
{
    QStringList row(SpeakingEval::ColumnCount, QString());
    row[SpeakingEval::toInt(SpeakingEvalColumn::Index)] = QStringLiteral("0");
    row[SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] = english;
    row[SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] = korean;

    for (int index = 0; index < componentGrades.size(); ++index)
    {
        row[SpeakingEval::toInt(SpeakingEvalColumn::Grammar) + index] =
            componentGrades.at(index);
    }

    return row;
}

int columnByName(
    const RosterModel* model,
    const QString& name
)
{
    if (!model)
    {
        return -1;
    }

    for (int column = 0; column < model->columnCount(); ++column)
    {
        if (model->columnName(column).compare(name, Qt::CaseInsensitive) == 0)
        {
            return column;
        }
    }

    return -1;
}

int columnByName(
    const Roster& roster,
    const QString& name
)
{
    for (int column = 0; column < roster.columns.size(); ++column)
    {
        if (roster.columns.at(column).compare(name, Qt::CaseInsensitive) == 0)
        {
            return column;
        }
    }

    return -1;
}

QString rosterCell(
    const Roster& roster,
    int row,
    const QString& columnName
)
{
    const int column = columnByName(roster, columnName);
    return row >= 0 && row < roster.rows.size() && column >= 0
        ? roster.rows.at(row).value(column)
        : QString();
}

class ScopedPromptService final
{
public:
    explicit ScopedPromptService(IUserPromptService* service)
    {
        DialogServices::setUserPromptServiceForTesting(service);
    }

    ~ScopedPromptService()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }
};

class RosterScoreImportFixture final
{
private:
    QTemporaryDir m_directory;

public:
    bool setStoredComponentScore(
        const QString& evaluationName,
        SpeakingEvalColumn component,
        const QString& score,
        QString* error
        ) const
    {
        const QString connectionName = QUuid::createUuid().toString();
        bool updated = false;
        QString failure;

        {
            QSqlDatabase database = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"),
                connectionName
                );
            database.setDatabaseName(
                m_directory.filePath(QStringLiteral("roster-score-import.db"))
                );
            if (!database.open())
            {
                failure = database.lastError().text();
            }
            else
            {
                const QString queryText =
                    QStringLiteral(
                        "UPDATE speaking_eval_data SET col_%1=? "
                        "WHERE row_index=0 AND evaluation_id=("
                        "SELECT id FROM speaking_evaluations "
                        "WHERE class_id=? AND evaluation_name=?)"
                        )
                        .arg(SpeakingEval::toInt(component));
                {
                    QSqlQuery query(database);
                    query.prepare(queryText);
                    query.addBindValue(score);
                    query.addBindValue(classId);
                    query.addBindValue(evaluationName);
                    updated = query.exec() && query.numRowsAffected() == 1;
                    if (!updated)
                    {
                        failure = query.lastError().text();
                        if (failure.isEmpty())
                        {
                            failure = QStringLiteral(
                                "The expected speaking evaluation score row was not updated."
                                );
                        }
                    }
                }
                database.close();
            }
        }

        QSqlDatabase::removeDatabase(connectionName);
        if (!updated && error)
        {
            *error = failure;
        }
        return updated;
    }

    bool initialize(QString* error)
    {
        if (!m_directory.isValid())
        {
            *error = QStringLiteral("Could not create a temporary directory.");
            return false;
        }

        const Status opened = m_services.openDatabase(
            m_directory.filePath(QStringLiteral("roster-score-import.db"))
        );
        if (!opened)
        {
            *error = opened.error();
            return false;
        }

        const Result<int> created = m_services.classService()->create(
            QStringLiteral("Roster score import fixture")
        );
        if (!created)
        {
            *error = created.error();
            return false;
        }
        classId = *created;

        Roster roster;
        roster.columns = Roster::BaseColumns;
        roster.columns.append({
            QStringLiteral("Notes"),
            QStringLiteral("Room")
        });
        roster.columnWidths = QVector<int>(roster.columns.size(), 120);
        roster.rows = {
            {
                QStringLiteral("Alex"), QStringLiteral("김민지"),
                QStringLiteral("C"), QStringLiteral("C"),
                QStringLiteral("C"), QStringLiteral("C"),
                QStringLiteral("Keep Alex's existing note"),
                QStringLiteral("401")
            },
            {
                QStringLiteral("Alex"), QStringLiteral("김하늘"),
                QStringLiteral("B"), QStringLiteral("B"),
                QStringLiteral("B"), QStringLiteral("B"),
                QStringLiteral("Same English name only"),
                QStringLiteral("402")
            },
            {
                QStringLiteral("Riley"), QStringLiteral("김민지"),
                QStringLiteral("A"), QStringLiteral("A"),
                QStringLiteral("A"), QStringLiteral("A"),
                QStringLiteral("Same Korean name only"),
                QStringLiteral("403")
            },
            {
                QStringLiteral("Jordan"), QStringLiteral("박지훈"),
                QStringLiteral("B+"), QStringLiteral("C"),
                QStringLiteral("A+"), QStringLiteral("B"),
                QStringLiteral("Unmatched student remains unchanged"),
                QStringLiteral("404")
            }
        };

        const Status rosterSaved = m_services.rosterService()->saveRoster(
            classId,
            roster
        );
        if (!rosterSaved)
        {
            *error = rosterSaved.error();
            return false;
        }

        const Result<Roster> savedRoster = m_services.rosterService()->roster(classId);
        if (!savedRoster)
        {
            *error = savedRoster.error();
            return false;
        }
        initialRoster = *savedRoster;

        // These are saved speaking-evaluation records consumed by the slot via
        // SpeakingEvaluationService; this fixture does not parse a workbook.
        for (const EvaluationFixture& evaluation : evaluations())
        {
            const Status saved = m_services.speakingEvaluationService()->saveEvaluation(
                classId,
                evaluation.name,
                SpeakingEvalRows{
                    speakingEvaluationRow(
                        QStringLiteral("Alex"),
                        QStringLiteral("김민지"),
                        evaluation.componentGrades
                    )
                }
            );
            if (!saved)
            {
                *error = saved.error();
                return false;
            }
        }

        return true;
    }

    ApplicationServices m_services;
    int classId = 0;
    Roster initialRoster;
};

} // namespace

class RosterEditorWidgetImportTests final : public QObject
{
    Q_OBJECT

private slots:
    void importScoresPersistsGradesAndRepeatedImportIsIdempotent();
    void paddedAndIncompleteScoresKeepRepositoryImportPolicy();
    void partialNamePairIsNotMatchedOrModified();
    void missingNameColumnsWarnWithoutChangingOrPersistingData();
};

void RosterEditorWidgetImportTests
    ::importScoresPersistsGradesAndRepeatedImportIsIdempotent()
{
    RosterScoreImportFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    RosterEditorWidget editor(&fixture.m_services, true);
    editor.loadClass(
        Classroom(
            QStringLiteral("Roster score import fixture"),
            fixture.classId
        )
    );

    auto* model = editor.findChild<RosterModel*>();
    QVERIFY(model);
    QCOMPARE(model->rowCount(), 25);
    QCOMPARE(columnByName(model, QStringLiteral("Winter")), 2);
    QCOMPARE(columnByName(model, QStringLiteral("Speech Contest")), 3);
    QCOMPARE(columnByName(model, QStringLiteral("Summer")), 4);
    QCOMPARE(columnByName(model, QStringLiteral("Fall")), 5);

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "importScores",
        Qt::DirectConnection
    ));

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Information);
    QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Import Scores"));
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral("Scores imported successfully.")
    );
    QVERIFY(editor.hasUnsavedChanges());

    // The normal autosave debounce is asynchronous; fail if persistence does
    // not finish promptly rather than reading the in-memory table as success.
    QTRY_VERIFY_WITH_TIMEOUT(!editor.hasUnsavedChanges(), 5'000);

    const Result<Roster> persisted = fixture.m_services.rosterService()->roster(
        fixture.classId
    );
    QVERIFY(persisted);
    QCOMPARE(persisted->columns, fixture.initialRoster.columns);
    QCOMPARE(persisted->rows.size(), fixture.initialRoster.rows.size());

    Roster expected = fixture.initialRoster;
    for (const EvaluationFixture& evaluation : evaluations())
    {
        const int column = columnByName(expected, evaluation.name);
        QVERIFY(column >= 0);
        expected.rows[0][column] = evaluation.expectedFinalGrade;
    }

    // Winter's mixed values total 16 points across six components (~2.667).
    // The production service rounds at a fractional average of >= 0.4, so it
    // returns B+; no six integer scores can represent an exact 0.4 fraction.

    // Only the student with the full English/Korean name-pair key changes.
    // Same-English-only, same-Korean-only, and unmatched rows retain all data.
    for (int row = 0; row < expected.rows.size(); ++row)
    {
        QCOMPARE(persisted->rows.at(row), expected.rows.at(row));
    }

    // The editor supplies empty rows in memory; their empty name-pair key is
    // skipped by the real import slot and remains empty.
    for (const QString& nameColumn : {
             QStringLiteral("English"),
             QStringLiteral("Korean"),
             QStringLiteral("Winter"),
             QStringLiteral("Speech Contest"),
             QStringLiteral("Summer"),
             QStringLiteral("Fall"),
             QStringLiteral("Notes"),
             QStringLiteral("Room")
         })
    {
        const int column = columnByName(model, nameColumn);
        QVERIFY(column >= 0);
        QVERIFY(model->index(4, column).data(Qt::EditRole).toString().isEmpty());
    }

    prompts.messages.clear();
    QSignalSpy dirtyChanges(&editor, &RosterEditorWidget::unsavedChangesChanged);
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "importScores",
        Qt::DirectConnection
    ));
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Information);
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral("Scores are already up to date.")
    );
    QVERIFY(!editor.hasUnsavedChanges());
    QCOMPARE(dirtyChanges.count(), 0);

    const Result<Roster> afterRepeatedImport = fixture.m_services.rosterService()->roster(
        fixture.classId
    );
    QVERIFY(afterRepeatedImport);
    QCOMPARE(afterRepeatedImport->rows.size(), persisted->rows.size());
    for (int row = 0; row < persisted->rows.size(); ++row)
    {
        QCOMPARE(afterRepeatedImport->rows.at(row), persisted->rows.at(row));
    }
}

void RosterEditorWidgetImportTests::partialNamePairIsNotMatchedOrModified()
{
    RosterScoreImportFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    RosterEditorWidget editor(&fixture.m_services, true);
    editor.loadClass(
        Classroom(
            QStringLiteral("Roster score import fixture"),
            fixture.classId
        )
    );

    auto* model = editor.findChild<RosterModel*>();
    QVERIFY(model);
    const int englishColumn = columnByName(model, QStringLiteral("English"));
    const int koreanColumn = columnByName(model, QStringLiteral("Korean"));
    QVERIFY(englishColumn >= 0);
    QVERIFY(koreanColumn >= 0);

    // The editor starts with empty rows. Give one only the English half of an
    // evaluation name pair and sentinel grades to prove it is not imported by
    // English name alone. This deliberately invalid row is in-memory test
    // setup only; it is discarded below rather than saved.
    QVERIFY(model->setData(
        model->index(4, englishColumn),
        QStringLiteral("Alex"),
        Qt::EditRole
    ));
    QVERIFY(model->data(model->index(4, koreanColumn), Qt::EditRole)
                .toString().isEmpty());
    for (const EvaluationFixture& evaluation : evaluations())
    {
        const int column = columnByName(model, evaluation.name);
        QVERIFY(column >= 0);
        QVERIFY(model->setData(
            model->index(4, column),
            QStringLiteral("C"),
            Qt::EditRole
        ));
    }

    prompts.messages.clear();
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "importScores",
        Qt::DirectConnection
    ));
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral("Scores imported successfully.")
    );

    QCOMPARE(
        model->data(model->index(4, englishColumn), Qt::EditRole).toString(),
        QStringLiteral("Alex")
    );
    QVERIFY(model->data(model->index(4, koreanColumn), Qt::EditRole)
                .toString().isEmpty());
    for (const EvaluationFixture& evaluation : evaluations())
    {
        const int column = columnByName(model, evaluation.name);
        QVERIFY(column >= 0);
        QCOMPARE(
            model->data(model->index(4, column), Qt::EditRole).toString(),
            QStringLiteral("C")
        );
    }

    editor.discardChanges();
    QVERIFY(!editor.hasUnsavedChanges());
    const Result<Roster> persisted = fixture.m_services.rosterService()->roster(
        fixture.classId
    );
    QVERIFY(persisted);
    QCOMPARE(persisted->columns, fixture.initialRoster.columns);
    QCOMPARE(persisted->rows.size(), fixture.initialRoster.rows.size());
    for (int row = 0; row < fixture.initialRoster.rows.size(); ++row)
    {
        QCOMPARE(persisted->rows.at(row), fixture.initialRoster.rows.at(row));
    }
}

void RosterEditorWidgetImportTests::
    paddedAndIncompleteScoresKeepRepositoryImportPolicy()
{
    RosterScoreImportFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    // Bypass the speaking-evaluation editor's normalizer so the repository
    // receives a persisted padded label and must retain its own trim policy.
    QVERIFY2(
        fixture.setStoredComponentScore(
            QStringLiteral("Winter"),
            SpeakingEvalColumn::OverallEffort,
            QStringLiteral(" C "),
            &error
            ),
        qPrintable(error)
        );
    const Result<SpeakingEvalRows> paddedWinter =
        fixture.m_services.speakingEvaluationService()->evaluation(
            fixture.classId,
            QStringLiteral("Winter")
            );
    QVERIFY(paddedWinter);
    QCOMPARE(
        paddedWinter->constFirst().at(
            SpeakingEval::toInt(SpeakingEvalColumn::OverallEffort)
            ),
        QStringLiteral(" C ")
        );

    // An absent final component remains valid saved data and imports as N/A.
    QVERIFY2(
        fixture.setStoredComponentScore(
            QStringLiteral("Summer"),
            SpeakingEvalColumn::OverallEffort,
            QString(),
            &error
            ),
        qPrintable(error)
        );
    const Result<SpeakingEvalRows> incompleteSummer =
        fixture.m_services.speakingEvaluationService()->evaluation(
            fixture.classId,
            QStringLiteral("Summer")
            );
    QVERIFY(incompleteSummer);
    QVERIFY(
        incompleteSummer->constFirst().at(
            SpeakingEval::toInt(SpeakingEvalColumn::OverallEffort)
            ).isEmpty()
        );

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    RosterEditorWidget editor(&fixture.m_services, true);
    editor.loadClass(
        Classroom(
            QStringLiteral("Roster score import fixture"),
            fixture.classId
            )
        );

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "importScores",
        Qt::DirectConnection
        ));
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral("Scores imported successfully.")
        );
    QTRY_VERIFY_WITH_TIMEOUT(!editor.hasUnsavedChanges(), 5'000);

    const Result<Roster> persisted =
        fixture.m_services.rosterService()->roster(fixture.classId);
    QVERIFY(persisted);
    QCOMPARE(rosterCell(*persisted, 0, QStringLiteral("Winter")),
        QStringLiteral("B+"));
    QCOMPARE(rosterCell(*persisted, 0, QStringLiteral("Summer")),
        QStringLiteral("N/A"));
    for (int row = 1; row < fixture.initialRoster.rows.size(); ++row)
    {
        QCOMPARE(persisted->rows.at(row), fixture.initialRoster.rows.at(row));
    }
}

void RosterEditorWidgetImportTests
    ::missingNameColumnsWarnWithoutChangingOrPersistingData()
{
    RosterScoreImportFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    for (const QString& missingColumn : {
             QStringLiteral("English"),
             QStringLiteral("Korean")
         })
    {
        RosterEditorWidget editor(&fixture.m_services, true);
        editor.loadClass(
            Classroom(
                QStringLiteral("Roster score import fixture"),
                fixture.classId
            )
        );

        const int column = columnByName(editor.m_model, missingColumn);
        QVERIFY(column >= 0);
        // Bypass the model's required-column invariant solely to exercise the
        // production slot's warning guard for each missing-name-column case.
        editor.m_model->m_columns.removeAt(column);

        prompts.messages.clear();
        QVERIFY(QMetaObject::invokeMethod(
            &editor,
            "importScores",
            Qt::DirectConnection
        ));

        QCOMPARE(prompts.messages.size(), 1);
        QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
        QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Import Scores"));
        QCOMPARE(
            prompts.messages.constFirst().message,
            QStringLiteral("Roster must contain 'English' and 'Korean' columns.")
        );
        QVERIFY(!editor.hasUnsavedChanges());
        QVERIFY(!editor.m_model->isDirty());

        const Result<Roster> persisted = fixture.m_services.rosterService()->roster(
            fixture.classId
        );
        QVERIFY(persisted);
        QCOMPARE(persisted->columns, fixture.initialRoster.columns);
        QCOMPARE(persisted->rows.size(), fixture.initialRoster.rows.size());
        for (int row = 0; row < fixture.initialRoster.rows.size(); ++row)
        {
            QCOMPARE(persisted->rows.at(row), fixture.initialRoster.rows.at(row));
        }
    }
}

QTEST_MAIN(RosterEditorWidgetImportTests)

#include "roster_editor_widget_import_tests.moc"
