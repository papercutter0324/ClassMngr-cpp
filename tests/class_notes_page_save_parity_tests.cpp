#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"
#include "features/classes/ui/class_notes_page.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QUuid>
#include <QtTest/QtTest>

namespace
{

struct ClassNotesWorkspace final
{
    QTemporaryDir directory;
    ApplicationServices services;
    QString className = QStringLiteral("Class Notes Save Parity");
    int classId = -1;
    int teacherId = -1;
};

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-notes-save-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

ClassInfo commonSeed(const int classId, const int teacherId)
{
    ClassInfo info;
    info.classId = classId;
    info.teacherId = teacherId;
    info.classGrade = QStringLiteral("E4");
    info.classLevel = QStringLiteral("Theseus");
    info.readingBook = QStringLiteral("Reading Explorer 1");
    info.essayBook = QStringLiteral("4A");
    info.classColor = QStringLiteral("#AABBCC");
    info.fontColor = QStringLiteral("#112233");
    info.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"),
        QStringLiteral("4:50 PM")
    }};
    info.intensiveTimes = {{
        QStringLiteral("Tuesday"),
        QStringLiteral("10:00 AM"),
        QStringLiteral("10:55 AM")
    }};
    info.notes = QStringLiteral("Original notes");
    info.timeFillerActivities = QStringLiteral("Original activities");
    return info;
}

bool initializeWorkspace(ClassNotesWorkspace& workspace, QString* error)
{
    if (!workspace.directory.isValid())
    {
        if (error)
        {
            *error = QStringLiteral("Temporary directory is unavailable.");
        }
        return false;
    }
    if (!workspace.services.openDatabase(databasePath(workspace.directory)))
    {
        if (error)
        {
            *error = QStringLiteral("Database could not be opened.");
        }
        return false;
    }

    const auto createdClass = workspace.services.classService()->create(
        workspace.className
        );
    if (!createdClass)
    {
        if (error)
        {
            *error = QStringLiteral("Class could not be created.");
        }
        return false;
    }
    workspace.classId = *createdClass;

    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD\uB2D8");
    teacher.teacherEn = QStringLiteral("Parity Teacher");
    teacher.preferredName = QStringLiteral("Parity Teacher");
    teacher.roomNumber = QStringLiteral("Room 504");
    teacher.wifiName = QStringLiteral("Parity WiFi");
    teacher.wifiPassword = QStringLiteral("parity-wifi-password");
    teacher.internetType = QStringLiteral("Both");
    teacher.zoomId = QStringLiteral("123 456 7890");
    teacher.zoomPassword = QStringLiteral("parity-zoom-password");
    teacher.projectionType = QStringLiteral("Any");
    const auto createdTeacher = workspace.services.teacherService()->create(
        teacher
        );
    if (!createdTeacher)
    {
        if (error)
        {
            *error = QStringLiteral("Teacher could not be created.");
        }
        return false;
    }
    workspace.teacherId = *createdTeacher;

    const Status saved = workspace.services.databaseSession()
        ->classInfoRepository()
        ->saveClassInfo(commonSeed(workspace.classId, workspace.teacherId));
    if (!saved)
    {
        if (error)
        {
            *error = saved.error();
        }
        return false;
    }
    return true;
}

bool sameSchedule(const QList<ClassTime>& left, const QList<ClassTime>& right)
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (qsizetype index = 0; index < left.size(); ++index)
    {
        if (left.at(index).day != right.at(index).day
            || left.at(index).startTime != right.at(index).startTime
            || left.at(index).endTime != right.at(index).endTime)
        {
            return false;
        }
    }
    return true;
}

bool sameClassInfo(const ClassInfo& left, const ClassInfo& right)
{
    return left.classId == right.classId
        && left.teacherId == right.teacherId
        && left.teacherKr == right.teacherKr
        && left.teacherEn == right.teacherEn
        && left.teacherPreferredName == right.teacherPreferredName
        && left.roomNumber == right.roomNumber
        && left.wifiName == right.wifiName
        && left.wifiPassword == right.wifiPassword
        && left.internetType == right.internetType
        && left.zoomId == right.zoomId
        && left.zoomPassword == right.zoomPassword
        && left.projectionType == right.projectionType
        && left.classGrade == right.classGrade
        && left.classLevel == right.classLevel
        && left.readingBook == right.readingBook
        && left.essayBook == right.essayBook
        && left.classColor == right.classColor
        && left.fontColor == right.fontColor
        && left.notes == right.notes
        && left.timeFillerActivities == right.timeFillerActivities
        && sameSchedule(left.classTimes, right.classTimes)
        && sameSchedule(left.intensiveTimes, right.intensiveTimes);
}

bool sameClassInfoExceptNotes(ClassInfo left, ClassInfo right)
{
    left.notes.clear();
    left.timeFillerActivities.clear();
    right.notes.clear();
    right.timeFillerActivities.clear();
    return sameClassInfo(left, right);
}

QList<QTextEdit*> noteEditors(ClassNotesPage& page)
{
    return page.findChildren<QTextEdit*>();
}

QPushButton* saveButton(ClassNotesPage& page)
{
    for (QPushButton* button : page.findChildren<QPushButton*>())
    {
        if (button->text().startsWith(QStringLiteral("Save Changes")))
        {
            return button;
        }
    }
    return nullptr;
}

void emitTranscript(const QJsonObject& transcript)
{
    qInfo().noquote()
        << QStringLiteral("F373_TRANSCRIPT ")
            + QString::fromUtf8(
                QJsonDocument(transcript).toJson(QJsonDocument::Compact)
                );
}

bool saveAndCaptureWarning(
    ClassNotesPage& page,
    bool* captured,
    QString* title,
    QString* message
    )
{
    QTimer::singleShot(0, [&] {
        const auto prompt = DialogServices::promptTestDriver().activePrompt();
        if (!prompt)
        {
            return;
        }
        if (captured)
        {
            *captured = true;
        }
        if (title)
        {
            *title = prompt->title;
        }
        if (message)
        {
            *message = prompt->text;
        }
        DialogServices::promptTestDriver().accept(prompt->id);
    });
    const bool saved = page.saveChanges();
    QCoreApplication::processEvents();
    return saved;
}

}

class ClassNotesPageSaveParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void successfulSaveTrimsBothFieldsAndPreservesClassInfo();
    void failedSaveWarnsAndRetainsUnchangedPersistence();
};

void ClassNotesPageSaveParityTests::
successfulSaveTrimsBothFieldsAndPreservesClassInfo()
{
    ClassNotesWorkspace workspace;
    QString setupError;
    QVERIFY2(initializeWorkspace(workspace, &setupError), qPrintable(setupError));

    const auto before = workspace.services.classService()->classInfo(
        workspace.classId
        );
    QVERIFY(before);

    ClassNotesPage page(&workspace.services);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(workspace.className, workspace.classId));
    const QList<QTextEdit*> editors = noteEditors(page);
    QCOMPARE(editors.size(), 2);
    QCOMPARE(editors.at(0)->toPlainText(), QStringLiteral("Original notes"));
    QCOMPARE(
        editors.at(1)->toPlainText(),
        QStringLiteral("Original activities")
        );

    editors.at(0)->setPlainText(
        QStringLiteral("  Saved notes  \n  Keep second line  ")
        );
    editors.at(1)->setPlainText(
        QStringLiteral("  Saved activities\nMore activities  ")
        );
    QVERIFY(page.hasUnsavedChanges());
    QPushButton* const button = saveButton(page);
    QVERIFY(button);
    QVERIFY(button->isEnabled());
    button->click();

    QVERIFY(!page.hasUnsavedChanges());
    const auto after = workspace.services.classService()->classInfo(
        workspace.classId
        );
    QVERIFY(after);
    QCOMPARE(
        after->notes,
        QStringLiteral("Saved notes  \n  Keep second line")
        );
    QCOMPARE(
        after->timeFillerActivities,
        QStringLiteral("Saved activities\nMore activities")
        );
    QVERIFY(sameClassInfoExceptNotes(*before, *after));

    emitTranscript({
        {QStringLiteral("case"), QStringLiteral("success")},
        {QStringLiteral("notes"), after->notes},
        {QStringLiteral("activities"), after->timeFillerActivities},
        {QStringLiteral("unrelated_class_info_preserved"), true},
        {QStringLiteral("page_clean"), true}
    });
}

void ClassNotesPageSaveParityTests::
failedSaveWarnsAndRetainsUnchangedPersistence()
{
    ClassNotesWorkspace workspace;
    QString setupError;
    QVERIFY2(initializeWorkspace(workspace, &setupError), qPrintable(setupError));

    const auto before = workspace.services.classService()->classInfo(
        workspace.classId
        );
    QVERIFY(before);

    ClassNotesPage page(&workspace.services);
    page.setSaveMode(SaveMode::Manual);
    page.loadClass(Classroom(workspace.className, workspace.classId));
    const QList<QTextEdit*> editors = noteEditors(page);
    QCOMPARE(editors.size(), 2);
    editors.at(0)->setPlainText(QStringLiteral("  Rejected notes  "));
    editors.at(1)->setPlainText(QStringLiteral("  Rejected activities  "));
    QVERIFY(page.hasUnsavedChanges());

    QSqlQuery installFailure(workspace.services.databaseSession()->database());
    QVERIFY2(
        installFailure.exec(QStringLiteral(
            "CREATE TRIGGER fail_f373_class_notes_save "
            "BEFORE UPDATE OF notes, time_filler_activities ON class_info "
            "WHEN NEW.notes = 'Rejected notes' "
            "BEGIN SELECT RAISE(ABORT, 'F373 deterministic write failure'); END"
            )),
        qPrintable(installFailure.lastError().text())
        );

    bool warningCaptured = false;
    QString warningTitle;
    QString warningMessage;
    QVERIFY(!saveAndCaptureWarning(
        page,
        &warningCaptured,
        &warningTitle,
        &warningMessage
        ));
    QVERIFY(warningCaptured);
    QCOMPARE(warningTitle, QStringLiteral("Save Class Notes"));
    QCOMPARE(
        warningMessage,
        QStringLiteral(
            "Saving class notes failed for class id %1: "
            "F373 deterministic write failure Unable to fetch row "
            "(database error 1811)"
            ).arg(workspace.classId)
        );
    QCOMPARE(editors.at(0)->toPlainText(), QStringLiteral("  Rejected notes  "));
    QCOMPARE(
        editors.at(1)->toPlainText(),
        QStringLiteral("  Rejected activities  ")
        );
    QVERIFY(page.hasUnsavedChanges());

    const auto after = workspace.services.classService()->classInfo(
        workspace.classId
        );
    QVERIFY(after);
    QVERIFY(sameClassInfo(*before, *after));

    emitTranscript({
        {QStringLiteral("case"), QStringLiteral("failure")},
        {QStringLiteral("warning_title"), warningTitle},
        {QStringLiteral("warning_message"), warningMessage},
        {QStringLiteral("editor_values_retained"), true},
        {QStringLiteral("page_dirty"), true},
        {QStringLiteral("persisted_state_unchanged"), true}
    });
}

QTEST_MAIN(ClassNotesPageSaveParityTests)

#include "class_notes_page_save_parity_tests.moc"
