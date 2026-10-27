#include "data/database/workspace_store.h"
#include "next/application/teacher_directory_read_query.h"
#include "next/platform/workspace_store_gateway.h"

#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

[[nodiscard]] QString fixturePath(const QString& fixtureName)
{
    return QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
        QStringLiteral("tests/fixtures/workspaces/") + fixtureName
        );
}

[[nodiscard]] bool executeSql(const QString& path, const QString& statement)
{
    const QString connectionName = QStringLiteral("teacher-directory-sql-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool executed = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(path);
        if (database.open())
        {
            QSqlQuery query(database);
            executed = query.exec(statement);
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return executed;
}

[[nodiscard]] bool executeFixture(
    const QString& profilePath,
    QString* errorMessage
    )
{
    QFile fixture(fixturePath(QStringLiteral("representative_startup.sql")));
    if (!fixture.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage = fixture.errorString();
        }
        return false;
    }

    const QStringList statements = QString::fromUtf8(fixture.readAll())
        .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    const QString connectionName = QStringLiteral("teacher-directory-fixture-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool executed = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(profilePath);
        if (!database.open())
        {
            if (errorMessage)
            {
                *errorMessage = database.lastError().text();
            }
        }
        else
        {
            executed = true;
            QSqlQuery query(database);
            for (const QString& rawStatement : statements)
            {
                const QString statement = rawStatement.trimmed();
                if (statement.isEmpty())
                {
                    continue;
                }
                if (!query.exec(statement))
                {
                    if (errorMessage)
                    {
                        *errorMessage = QStringLiteral("%1: %2")
                            .arg(statement.simplified().left(120),
                                 query.lastError().text());
                    }
                    executed = false;
                    break;
                }
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return executed;
}

[[nodiscard]] bool createFixtureWorkspace(
    const QString& profilePath,
    QString* errorMessage
    )
{
    {
        WorkspaceStore store;
        const RepositoryStatus opened = store.open(profilePath);
        if (!opened)
        {
            if (errorMessage)
            {
                *errorMessage = opened.error().diagnostic;
            }
            return false;
        }
        store.close();
    }
    return executeFixture(profilePath, errorMessage);
}

[[nodiscard]] Application::TeacherCreateRequest validTeacher(
    const QString& korean,
    const QString& english
    )
{
    return {
        .fields = {
            .teacherKr = korean.toStdU16String(),
            .teacherEn = english.toStdU16String()
        }
    };
}

[[nodiscard]] std::optional<qint64> teacherIdFor(
    const Application::TeacherDirectoryReadSnapshot& snapshot,
    const std::string& id
    )
{
    const auto found = std::find_if(
        snapshot.teachers.begin(),
        snapshot.teachers.end(),
        [&id](const Application::TeacherDirectoryEntry& teacher) {
            return teacher.teacherId.value() == id;
        }
        );
    return found == snapshot.teachers.end()
        ? std::nullopt
        : Application::parseCanonicalPositiveDecimalId64(
              found->teacherId.value()
              );
}

} // namespace

class WorkspaceStoreTeacherDirectoryTests final : public QObject
{
    Q_OBJECT

private slots:
    void emptyWorkspaceCreateValidationFailureAndWideGeneratedId();
    void directoryOrderingFallbackAndDeleteTransactionStayWide();
};

void WorkspaceStoreTeacherDirectoryTests::
emptyWorkspaceCreateValidationFailureAndWideGeneratedId()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString profilePath = temporaryDirectory.filePath(
        QStringLiteral("empty-teacher-directory.tps")
        );

    Platform::WorkspaceStoreGateway gateway;
    const auto opened = gateway.createWorkspace({
        .location = Application::WorkspaceLocation(
            profilePath.toUtf8().toStdString()
            )
    });
    QVERIFY(opened);

    auto directory = gateway.readTeacherDirectory();
    QVERIFY(directory);
    QVERIFY(directory.value().teachers.empty());

    const Application::TeacherCreateResult invalid = gateway.createTeacher({});
    QVERIFY(!invalid);
    directory = gateway.readTeacherDirectory();
    QVERIFY(directory);
    QVERIFY(directory.value().teachers.empty());

    const Application::TeacherCreateResult first = gateway.createTeacher(
        validTeacher(QStringLiteral("김다은"), QStringLiteral("Dana Kim"))
        );
    QVERIFY(first);
    QCOMPARE(first.value().value(), std::string("1"));
    directory = gateway.readTeacherDirectory();
    QVERIFY(directory);
    QCOMPARE(directory.value().teachers.size(), std::size_t(1));
    QCOMPARE(directory.value().teachers.front().teacherId, first.value());

    QVERIFY(executeSql(
        profilePath,
        QStringLiteral(
            "CREATE TRIGGER reject_teacher_create BEFORE INSERT ON teachers "
            "BEGIN SELECT RAISE(ABORT, 'injected create failure'); END"
            )
        ));
    const Application::TeacherCreateResult failed = gateway.createTeacher(
        validTeacher(QStringLiteral("이준"), QStringLiteral("Jordan Lee"))
        );
    QVERIFY(!failed);
    directory = gateway.readTeacherDirectory();
    QVERIFY(directory);
    QCOMPARE(directory.value().teachers.size(), std::size_t(1));
    QVERIFY(executeSql(
        profilePath,
        QStringLiteral("DROP TRIGGER reject_teacher_create")
        ));

    QVERIFY(executeSql(
        profilePath,
        QStringLiteral(
            "UPDATE sqlite_sequence SET seq=2147483649 WHERE name='teachers'"
            )
        ));
    const Application::TeacherCreateResult wide = gateway.createTeacher(
        validTeacher(QStringLiteral("홍길동"), QStringLiteral("Hong Gildong"))
        );
    QVERIFY(wide);
    QCOMPARE(wide.value().value(), std::string("2147483650"));
    const Application::TeacherProfileReadResult profile =
        gateway.readTeacherProfile(wide.value());
    QVERIFY(profile);
    QCOMPARE(profile.value().teacherId, wide.value());
    QCOMPARE(profile.value().fields.teacherEn, u"Hong Gildong");
}

void WorkspaceStoreTeacherDirectoryTests::
directoryOrderingFallbackAndDeleteTransactionStayWide()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString profilePath = temporaryDirectory.filePath(
        QStringLiteral("ordered-teacher-directory.tps")
        );
    QString fixtureError;
    QVERIFY2(createFixtureWorkspace(profilePath, &fixtureError),
        qPrintable(fixtureError));

    Platform::WorkspaceStoreGateway gateway;
    const auto opened = gateway.openWorkspace({
        .location = Application::WorkspaceLocation(
            profilePath.toUtf8().toStdString()
            )
    });
    QVERIFY(opened);

    QVERIFY(executeSql(
        profilePath,
        QStringLiteral(
            "UPDATE teachers SET teacher_en='Beta', teacher_kr='나', "
            "preferred_romanization='', preferred_name='Bee' WHERE id=1"
            )
        ));
    QVERIFY(executeSql(
        profilePath,
        QStringLiteral(
            "UPDATE teachers SET teacher_en='Alpha', teacher_kr='다', "
            "preferred_romanization='', preferred_name='' WHERE id=2"
            )
        ));
    QVERIFY(executeSql(
        profilePath,
        QStringLiteral(
            "UPDATE teachers SET teacher_en='', teacher_kr='다', "
            "preferred_romanization='Roman', preferred_name='' WHERE id=3"
            )
        ));
    QVERIFY(executeSql(
        profilePath,
        QStringLiteral(
            "UPDATE teachers SET teacher_en='', teacher_kr='가', "
            "preferred_romanization='', preferred_name='' WHERE id=4"
            )
        ));
    QVERIFY(executeSql(
        profilePath,
        QStringLiteral(
            "INSERT INTO teachers (id, teacher_kr, teacher_en, "
            "preferred_romanization, preferred_name, room_number, birthday, "
            "phone_number, wifi_name, wifi_password, internet_type, zoom_id, "
            "zoom_password, projection_type, notes) VALUES "
            "(2147483650, '나', '', '', '', '', '', '', '', '', 'WiFi', '', '', 'HDMI', ''), "
            "(5, '다', '', 'Roman 5', '', '', '', '', '', '', 'WiFi', '', '', 'HDMI', '')"
            )
        ));

    auto directory = gateway.readTeacherDirectory();
    QVERIFY(directory);
    const std::vector<std::string> expectedOrder{
        "2", "1", "4", "2147483650", "3", "5"
    };
    std::vector<std::string> actualOrder;
    for (const Application::TeacherDirectoryEntry& teacher :
         directory.value().teachers)
    {
        actualOrder.push_back(teacher.teacherId.value());
    }
    QCOMPARE(actualOrder, expectedOrder);
    QCOMPARE(directory.value().teachers[0].preferredName, std::u16string{});
    QCOMPARE(directory.value().teachers[1].preferredName, std::u16string(u"Bee"));
    QCOMPARE(directory.value().teachers[2].teacherKr, std::u16string(u"가"));
    QCOMPARE(directory.value().teachers[4].preferredRomanization,
        std::u16string(u"Roman"));
    QCOMPARE(teacherIdFor(directory.value(), "2147483650"),
        std::optional<qint64>(2147483650LL));

    QVERIFY(executeSql(
        profilePath,
        QStringLiteral(
            "CREATE TRIGGER reject_teacher_delete BEFORE DELETE ON teachers "
            "WHEN OLD.id=1 BEGIN SELECT RAISE(ABORT, 'injected delete failure'); END"
            )
        ));
    const Application::TeacherDeleteResult failed = gateway.deleteTeacher({
        .teacherId = *Domain::TeacherId::fromString("1")
    });
    QVERIFY(!failed);

    const QString countConnectionName = QStringLiteral("teacher-directory-count-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            countConnectionName
            );
        database.setDatabaseName(profilePath);
        QVERIFY(database.open());
        QSqlQuery references(database);
        QVERIFY(references.exec(QStringLiteral(
            "SELECT COUNT(*) FROM class_info WHERE teacher_id=1"
            )));
        QVERIFY(references.next());
        QCOMPARE(references.value(0).toInt(), 2);
        QSqlQuery classes(database);
        QVERIFY(classes.exec(QStringLiteral("SELECT COUNT(*) FROM classes")));
        QVERIFY(classes.next());
        QCOMPARE(classes.value(0).toInt(), 8);
        database.close();
    }
    QSqlDatabase::removeDatabase(countConnectionName);

    QVERIFY(executeSql(
        profilePath,
        QStringLiteral("DROP TRIGGER reject_teacher_delete")
        ));
    const Application::TeacherDeleteResult removed = gateway.deleteTeacher({
        .teacherId = *Domain::TeacherId::fromString("1")
    });
    QVERIFY(removed);
    directory = gateway.readTeacherDirectory();
    QVERIFY(directory);
    QCOMPARE(directory.value().teachers.size(), std::size_t(5));
    QVERIFY(!teacherIdFor(directory.value(), "1"));

    const QString verifyConnectionName = QStringLiteral("teacher-directory-verify-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            verifyConnectionName
            );
        database.setDatabaseName(profilePath);
        QVERIFY(database.open());
        QSqlQuery classes(database);
        QVERIFY(classes.exec(QStringLiteral("SELECT COUNT(*) FROM classes")));
        QVERIFY(classes.next());
        QCOMPARE(classes.value(0).toInt(), 8);
        QSqlQuery references(database);
        QVERIFY(references.exec(QStringLiteral(
            "SELECT COUNT(*) FROM class_info WHERE teacher_id IS NULL"
            )));
        QVERIFY(references.next());
        QCOMPARE(references.value(0).toInt(), 2);
        database.close();
    }
    QSqlDatabase::removeDatabase(verifyConnectionName);
}

QTEST_GUILESS_MAIN(WorkspaceStoreTeacherDirectoryTests)

#include "workspace_store_teacher_directory_tests.moc"
