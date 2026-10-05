#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/class_co_teacher_assignment_use_case.h"
#include "next/platform/application_services_class_co_teacher_assignment_port.h"

#include <QByteArray>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUuid>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory, const QString& scenario)
{
    return directory.filePath(
        QStringLiteral("f355-%1-%2.tps").arg(
            scenario,
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createClass(ApplicationServices& services, const QString& name)
{
    const auto created = services.classService()->create(name);
    return created ? *created : -1;
}

int createTeacher(ApplicationServices& services, const QString& name)
{
    Teacher teacher;
    teacher.teacherEn = name;
    teacher.preferredName = name;
    const auto created = services.teacherService()->save(teacher);
    return created ? *created : -1;
}

ClassInfo basicInfo(const int classId)
{
    ClassInfo info;
    info.classId = classId;
    info.classGrade.clear();
    info.classLevel.clear();
    info.readingBook.clear();
    info.essayBook.clear();
    return info;
}

QString errorCodeName(const Domain::ErrorCode code)
{
    switch (code)
    {
    case Domain::ErrorCode::InvalidInput: return QStringLiteral("InvalidInput");
    case Domain::ErrorCode::NotFound: return QStringLiteral("NotFound");
    case Domain::ErrorCode::Conflict: return QStringLiteral("Conflict");
    case Domain::ErrorCode::Validation: return QStringLiteral("Validation");
    case Domain::ErrorCode::Canceled: return QStringLiteral("Canceled");
    case Domain::ErrorCode::Technical: return QStringLiteral("Technical");
    }
    return QStringLiteral("Unknown");
}

QString asciiJsonString(const QString& value)
{
    QString result = QStringLiteral("\"");
    for (const QChar character : value)
    {
        const ushort codeUnit = character.unicode();
        if (character == QChar(u'\"'))
        {
            result += QStringLiteral("\\\"");
        }
        else if (character == QChar(u'\\'))
        {
            result += QStringLiteral("\\\\");
        }
        else if (character == QChar(u'\n'))
        {
            result += QStringLiteral("\\n");
        }
        else if (character == QChar(u'\r'))
        {
            result += QStringLiteral("\\r");
        }
        else if (character == QChar(u'\t'))
        {
            result += QStringLiteral("\\t");
        }
        else if (codeUnit < 0x20 || codeUnit > 0x7E)
        {
            result += QStringLiteral("\\u%1").arg(
                codeUnit,
                4,
                16,
                QChar(u'0')
                );
        }
        else
        {
            result += character;
        }
    }
    result += QChar(u'\"');
    return result;
}

void emitTranscript(
    const QString& scenario,
    const bool succeeded,
    const QString& code,
    const QString& message,
    const QString& observation
    )
{
    QTextStream output(stdout);
    output << "F355_TRANSCRIPT {\"scenario\":" << asciiJsonString(scenario)
           << ",\"success\":" << (succeeded ? "true" : "false")
           << ",\"error_code\":" << asciiJsonString(code)
           << ",\"message\":" << asciiJsonString(message)
           << ",\"observation\":" << asciiJsonString(observation)
           << "}\n";
    output.flush();
}

QString messageFor(const Domain::Result<void>& result)
{
    return result
        ? QString{}
        : QString::fromStdString(result.error().message);
}

QString codeFor(const Domain::Result<void>& result)
{
    return result ? QString{} : errorCodeName(result.error().code);
}

}

class ClassCoTeacherAssignmentPersistenceParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void emitsBaselineCompatiblePersistenceTranscripts();
};

void ClassCoTeacherAssignmentPersistenceParityTests::
emitsBaselineCompatiblePersistenceTranscripts()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    {
        ApplicationServices services;
        QVERIFY(services.openDatabase(databasePath(directory, "assign")));
        const int classId = createClass(
            services,
            QStringLiteral("F355 Canonical Assign")
            );
        const int teacherId = createTeacher(
            services,
            QStringLiteral("Assigned Teacher")
            );
        QVERIFY(classId > 0);
        QVERIFY(teacherId > 0);
        QVERIFY(services.classService()->saveClassInfo(basicInfo(classId)));

        Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
        const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
            classId,
            teacherId,
            port
            );
        QVERIFY(result);
        const auto saved = services.classService()->classInfo(classId);
        QVERIFY(saved);
        const bool teacherAssigned = saved->teacherId == teacherId;
        QVERIFY(teacherAssigned);
        emitTranscript(
            QStringLiteral("canonical_assign"),
            result.hasValue(),
            codeFor(result),
            messageFor(result),
            teacherAssigned
                ? QStringLiteral("teacher_assigned")
                : QStringLiteral("teacher_not_assigned")
            );
    }

    {
        ApplicationServices services;
        QVERIFY(services.openDatabase(databasePath(directory, "unassign")));
        const int classId = createClass(
            services,
            QStringLiteral("F355 Canonical Unassign")
            );
        const int teacherId = createTeacher(
            services,
            QStringLiteral("Previous Teacher")
            );
        QVERIFY(classId > 0);
        QVERIFY(teacherId > 0);
        ClassInfo info = basicInfo(classId);
        info.teacherId = teacherId;
        QVERIFY(services.classService()->saveClassInfo(info));

        Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
        const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
            classId,
            -1,
            port
            );
        QVERIFY(result);
        QSqlQuery query(services.databaseSession()->database());
        query.prepare(QStringLiteral(
            "SELECT teacher_id FROM class_info WHERE class_id=?"
            ));
        query.addBindValue(classId);
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        QVERIFY(query.next());
        const bool teacherIdIsNull = query.value(0).isNull();
        QVERIFY(teacherIdIsNull);
        emitTranscript(
            QStringLiteral("unassign"),
            result.hasValue(),
            codeFor(result),
            messageFor(result),
            teacherIdIsNull
                ? QStringLiteral("teacher_id_null")
                : QStringLiteral("teacher_id_nonnull")
            );
    }

    {
        ApplicationServices services;
        QVERIFY(services.openDatabase(databasePath(directory, "invalid_notes")));
        const int classId = createClass(
            services,
            QStringLiteral("F355 Invalid Notes")
            );
        const int teacherId = createTeacher(
            services,
            QStringLiteral("Assigned Teacher")
            );
        QVERIFY(classId > 0);
        QVERIFY(teacherId > 0);
        QVERIFY(services.classService()->saveClassInfo(basicInfo(classId)));
        QSqlQuery corrupt(services.databaseSession()->database());
        corrupt.prepare(QStringLiteral(
            "UPDATE class_info SET notes=? WHERE class_id=?"
            ));
        corrupt.addBindValue(QString(10001, QChar(u'x')));
        corrupt.addBindValue(classId);
        QVERIFY2(corrupt.exec(), qPrintable(corrupt.lastError().text()));

        Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
        const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
            classId,
            teacherId,
            port
            );
        QVERIFY(!result);
        QVERIFY(QString::fromStdString(result.error().message).contains(
            QStringLiteral("Class information validation failed")
            ));
        emitTranscript(
            QStringLiteral("invalid_notes"),
            result.hasValue(),
            codeFor(result),
            messageFor(result),
            QStringLiteral("assignment_rejected")
            );
    }

    {
        ApplicationServices services;
        QVERIFY(services.openDatabase(databasePath(directory, "conflicts")));
        const int sourceClassId = createClass(
            services,
            QStringLiteral("F355 Regular Conflict Source")
            );
        const int selectedClassId = createClass(
            services,
            QStringLiteral("F355 Intensive Conflict Selected")
            );
        const int teacherId = createTeacher(
            services,
            QStringLiteral("Assigned Teacher")
            );
        QVERIFY(sourceClassId > 0);
        QVERIFY(selectedClassId > 0);
        QVERIFY(teacherId > 0);

        ClassInfo source = basicInfo(sourceClassId);
        source.classTimes = {{
            QStringLiteral("Monday"),
            QStringLiteral("9:00 AM"),
            QStringLiteral("9:50 AM")
        }};
        source.intensiveTimes = {{
            QStringLiteral("Friday"),
            QStringLiteral("10:00 AM"),
            QStringLiteral("10:50 AM")
        }};
        QVERIFY(services.classService()->saveClassInfo(source));

        ClassInfo selected = basicInfo(selectedClassId);
        selected.classTimes = source.classTimes;
        selected.intensiveTimes = {{
            QStringLiteral("Friday"),
            QStringLiteral("10:10 AM"),
            QStringLiteral("10:40 AM")
        }};
        const Status selectedSaved = services.databaseSession()
            ->classInfoRepository()
            ->saveClassInfo(selected);
        QVERIFY(selectedSaved);

        Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
        const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
            selectedClassId,
            teacherId,
            port
            );
        QVERIFY(!result);
        QVERIFY(QString::fromStdString(result.error().message).contains(
            QStringLiteral("Monday 9:00 AM")
            ));
        emitTranscript(
            QStringLiteral("regular_before_intensive_conflict"),
            result.hasValue(),
            codeFor(result),
            messageFor(result),
            QStringLiteral("regular_conflict_selected")
            );
    }

    {
        ApplicationServices services;
        QVERIFY(services.openDatabase(databasePath(directory, "missing_info")));
        const int classId = createClass(
            services,
            QStringLiteral("F355 Missing Metadata")
            );
        const int teacherId = createTeacher(
            services,
            QStringLiteral("Assigned Teacher")
            );
        QVERIFY(classId > 0);
        QVERIFY(teacherId > 0);

        Platform::ApplicationServicesClassCoTeacherAssignmentPort port(services);
        const auto result = Application::ClassCoTeacherAssignmentUseCase::execute(
            classId,
            teacherId,
            port
            );
        QVERIFY(result);
        QSqlQuery query(services.databaseSession()->database());
        query.prepare(QStringLiteral(
            "SELECT teacher_id, class_grade, class_level, reading_book, "
            "essay_book, class_color, font_color, notes, "
            "time_filler_activities FROM class_info WHERE class_id=?"
            ));
        query.addBindValue(classId);
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        QVERIFY(query.next());
        const bool defaultMetadata = query.value(0).toInt() == teacherId
            && query.value(1).isNull()
            && query.value(2).isNull()
            && query.value(3).isNull()
            && query.value(4).isNull()
            && query.value(5).toString() == QStringLiteral("#FFFFFF")
            && query.value(6).toString() == QStringLiteral("#000000")
            && query.value(7).isNull()
            && query.value(8).isNull();
        QVERIFY(defaultMetadata);
        emitTranscript(
            QStringLiteral("missing_class_info_row"),
            result.hasValue(),
            codeFor(result),
            messageFor(result),
            defaultMetadata
                ? QStringLiteral("assigned_with_schema_defaults")
                : QStringLiteral("unexpected_metadata")
            );
    }
}

QTEST_MAIN(ClassCoTeacherAssignmentPersistenceParityTests)

#include "class_co_teacher_assignment_persistence_parity_tests.moc"
