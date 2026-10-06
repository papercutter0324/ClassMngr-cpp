#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "features/schedule/ui/schedule_editor_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/widgets/clickable_color_preview.h"

#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlError>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QtTest/QtTest>

#include <cstdio>
#include <optional>

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("schedule-editor-dialog-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

struct SeededClass final
{
    int classId = -1;
    int teacherId = -1;
    ClassInfo expected;
};

int createTeacher(ApplicationServices& services, QString* error)
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD\uB2D8");
    teacher.teacherEn = QStringLiteral("Alex Kim");
    teacher.preferredRomanization = QStringLiteral("Alex Kim");
    teacher.preferredName = QStringLiteral("Alex Kim");
    teacher.roomNumber = QStringLiteral("Room 504");
    teacher.wifiName = QStringLiteral("Schedule Parity WiFi");
    teacher.wifiPassword = QStringLiteral("schedule-parity-password");
    teacher.internetType = QStringLiteral("Both");
    teacher.zoomId = QStringLiteral("123 456 7890");
    teacher.zoomPassword = QStringLiteral("schedule-parity-zoom");
    teacher.projectionType = QStringLiteral("Any");
    teacher.notes = QStringLiteral("Teacher profile remains untouched");

    const auto created = services.teacherService()->create(teacher);
    if (!created)
    {
        if (error)
        {
            *error = created.error();
        }
        return -1;
    }
    return *created;
}

std::optional<SeededClass> seedClass(
    ApplicationServices& services,
    QString* error
    )
{
    const int teacherId = createTeacher(services, error);
    const auto created = services.classService()->create(
        QStringLiteral("Schedule Editor Dialog Parity")
        );
    if (teacherId <= 0)
    {
        return std::nullopt;
    }
    if (!created)
    {
        if (error)
        {
            *error = created.error();
        }
        return std::nullopt;
    }

    auto loaded = services.classService()->classInfo(*created);
    if (!loaded)
    {
        if (error)
        {
            *error = loaded.error();
        }
        return std::nullopt;
    }

    ClassInfo info = *loaded;
    info.teacherId = teacherId;
    info.classGrade = QStringLiteral("E4");
    info.classLevel = QStringLiteral("Theseus");
    info.readingBook = QStringLiteral("Reading Explorer 1");
    info.essayBook = QStringLiteral("4A");
    info.classColor = QStringLiteral("#AABBCC");
    info.fontColor = QStringLiteral("#112233");
    info.notes = QStringLiteral("Keep hidden class notes");
    info.timeFillerActivities = QStringLiteral("Keep hidden filler activities");
    info.classTimes = {
        {
            QStringLiteral("Friday"),
            QStringLiteral("3:00 PM"),
            QStringLiteral("3:55 PM")
        }
    };
    info.intensiveTimes = {
        {
            QStringLiteral("Wednesday"),
            QStringLiteral("1:00 PM"),
            QStringLiteral("1:55 PM")
        }
    };

    const auto saved = services.classService()->saveClassInfo(info);
    if (!saved)
    {
        if (error)
        {
            *error = saved.error();
        }
        return std::nullopt;
    }

    loaded = services.classService()->classInfo(*created);
    if (!loaded)
    {
        if (error)
        {
            *error = loaded.error();
        }
        return std::nullopt;
    }

    return SeededClass{
        .classId = *created,
        .teacherId = teacherId,
        .expected = *loaded
    };
}

bool sameSchedule(
    const QList<ClassTime>& left,
    const QList<ClassTime>& right
    )
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

QList<QComboBox*> dialogCombos(ScheduleEditorDialog& dialog)
{
    return dialog.findChildren<QComboBox*>();
}

bool clickNamedButton(QWidget& widget, const QString& label)
{
    for (QPushButton* button : widget.findChildren<QPushButton*>())
    {
        if (button->text() == label)
        {
            button->click();
            return true;
        }
    }
    return false;
}

bool displaysSeededFields(
    ScheduleEditorDialog& dialog,
    const ClassInfo& expected
    )
{
    const QList<QComboBox*> combos = dialogCombos(dialog);
    const QList<QLineEdit*> edits = dialog.findChildren<QLineEdit*>();
    const QList<ClickableColorPreview*> previews =
        dialog.findChildren<ClickableColorPreview*>();
    return combos.size() == 2
        && edits.size() == 2
        && previews.size() == 2
        && combos.at(0)->currentText() == expected.classGrade
        && combos.at(1)->currentText() == expected.classLevel
        && edits.at(0)->text() == expected.teacherKr
        && edits.at(1)->text() == expected.roomNumber
        && previews.at(0)->styleSheet().contains(expected.classColor)
        && previews.at(1)->styleSheet().contains(expected.fontColor);
}

struct WarningObservation final
{
    bool accepted = false;
    QString title;
    QString text;
};

void acceptNextWarning(WarningObservation* observation)
{
    QTimer::singleShot(0, [observation]
    {
        const auto prompt = DialogServices::promptTestDriver().activePrompt();
        if (!prompt)
        {
            return;
        }
        observation->title = prompt->title;
        observation->text = prompt->text;
        observation->accepted =
            DialogServices::promptTestDriver().accept(prompt->id);
    });
}

void emitTranscript(const QString& scenario, const QString& observation)
{
    const QByteArray row = QStringLiteral("SCHEDULE_EDITOR_DIALOG|%1|%2\n")
        .arg(scenario, observation)
        .toUtf8();
    std::fwrite(row.constData(), 1, static_cast<std::size_t>(row.size()), stdout);
    std::fflush(stdout);
}

bool renameClassInfoTable(
    const QSqlDatabase& database,
    const QString& from,
    const QString& to,
    QString* error
    )
{
    QSqlQuery query(database);
    if (query.exec(QStringLiteral("ALTER TABLE %1 RENAME TO %2").arg(from, to)))
    {
        return true;
    }
    if (error)
    {
        *error = query.lastError().text();
    }
    return false;
}

}

class ScheduleEditorDialogParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void cancelDoesNotWrite();
    void unchangedSavePreservesVisibleAndHiddenDetails();
    void gradeChangeClearsBooksAndPreservesOtherDetails();
    void levelChangeClearsBooksAndPreservesOtherDetails();
    void technicalWriteFailureWarnsAndDoesNotWrite();
    void transientInitialReadFailureBlocksSave();
};

void ScheduleEditorDialogParityTests::cancelDoesNotWrite()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QString seedError;
    const auto seed = seedClass(services, &seedError);
    QVERIFY2(seed.has_value(), qPrintable(seedError));

    ScheduleEditorDialog dialog(&services, seed->classId);
    QVERIFY(displaysSeededFields(dialog, seed->expected));
    dialog.show();
    QTRY_VERIFY(dialog.isVisible());
    QVERIFY(clickNamedButton(dialog, QStringLiteral("Cancel")));

    const auto after = services.classService()->classInfo(seed->classId);
    QVERIFY(after.has_value());
    QVERIFY(sameClassInfo(*after, seed->expected));
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Rejected));
    QVERIFY(!dialog.isVisible());
    QVERIFY(!DialogServices::promptTestDriver().activePrompt().has_value());
    emitTranscript(
        QStringLiteral("cancel"),
        QStringLiteral("closed=1,warning=none,unchanged=1")
        );
}

void ScheduleEditorDialogParityTests::
unchangedSavePreservesVisibleAndHiddenDetails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QString seedError;
    const auto seed = seedClass(services, &seedError);
    QVERIFY2(seed.has_value(), qPrintable(seedError));

    ScheduleEditorDialog dialog(&services, seed->classId);
    QVERIFY(displaysSeededFields(dialog, seed->expected));
    QSignalSpy savedSpy(&dialog, &ScheduleEditorDialog::saved);
    QVERIFY(savedSpy.isValid());
    dialog.show();
    QTRY_VERIFY(dialog.isVisible());
    QVERIFY(clickNamedButton(dialog, QStringLiteral("Save")));

    const auto after = services.classService()->classInfo(seed->classId);
    QVERIFY(after.has_value());
    QVERIFY(sameClassInfo(*after, seed->expected));
    QCOMPARE(savedSpy.size(), 1);
    QCOMPARE(savedSpy.at(0).at(0).toInt(), seed->classId);
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    QVERIFY(!dialog.isVisible());
    emitTranscript(
        QStringLiteral("unchanged-save"),
        QStringLiteral(
            "grade=E4,level=Theseus,teacher=visible,room=visible,"
            "hidden=preserved,schedules=regular+intensive"
            )
        );
}

void ScheduleEditorDialogParityTests::
gradeChangeClearsBooksAndPreservesOtherDetails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QString seedError;
    const auto seed = seedClass(services, &seedError);
    QVERIFY2(seed.has_value(), qPrintable(seedError));

    ScheduleEditorDialog dialog(&services, seed->classId);
    const QList<QComboBox*> combos = dialogCombos(dialog);
    QCOMPARE(combos.size(), 2);
    combos.at(0)->setCurrentText(QStringLiteral("E5"));
    combos.at(1)->setCurrentText(QStringLiteral("Artemis"));
    QCOMPARE(combos.at(0)->currentText(), QStringLiteral("E5"));
    QCOMPARE(combos.at(1)->currentText(), QStringLiteral("Artemis"));
    dialog.show();
    QTRY_VERIFY(dialog.isVisible());
    QVERIFY(clickNamedButton(dialog, QStringLiteral("Save")));

    ClassInfo expected = seed->expected;
    expected.classGrade = QStringLiteral("E5");
    expected.classLevel = QStringLiteral("Artemis");
    expected.readingBook.clear();
    expected.essayBook.clear();
    const auto after = services.classService()->classInfo(seed->classId);
    QVERIFY(after.has_value());
    QVERIFY(sameClassInfo(*after, expected));
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    emitTranscript(
        QStringLiteral("grade-change"),
        QStringLiteral(
            "grade=E5,level=Artemis,books=cleared,"
            "other-details=preserved,schedules=regular+intensive"
            )
        );
}

void ScheduleEditorDialogParityTests::
levelChangeClearsBooksAndPreservesOtherDetails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QString seedError;
    const auto seed = seedClass(services, &seedError);
    QVERIFY2(seed.has_value(), qPrintable(seedError));

    ScheduleEditorDialog dialog(&services, seed->classId);
    const QList<QComboBox*> combos = dialogCombos(dialog);
    QCOMPARE(combos.size(), 2);
    combos.at(1)->setCurrentText(QStringLiteral("Perseus"));
    QCOMPARE(combos.at(0)->currentText(), QStringLiteral("E4"));
    QCOMPARE(combos.at(1)->currentText(), QStringLiteral("Perseus"));
    dialog.show();
    QTRY_VERIFY(dialog.isVisible());
    QVERIFY(clickNamedButton(dialog, QStringLiteral("Save")));

    ClassInfo expected = seed->expected;
    expected.classLevel = QStringLiteral("Perseus");
    expected.readingBook.clear();
    expected.essayBook.clear();
    const auto after = services.classService()->classInfo(seed->classId);
    QVERIFY(after.has_value());
    QVERIFY(sameClassInfo(*after, expected));
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    emitTranscript(
        QStringLiteral("level-change"),
        QStringLiteral(
            "grade=E4,level=Perseus,books=cleared,"
            "other-details=preserved,schedules=regular+intensive"
            )
        );
}

void ScheduleEditorDialogParityTests::
technicalWriteFailureWarnsAndDoesNotWrite()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QString seedError;
    const auto seed = seedClass(services, &seedError);
    QVERIFY2(seed.has_value(), qPrintable(seedError));

    QSqlQuery trigger(services.databaseSession()->database());
    QVERIFY(trigger.exec(QStringLiteral(
        "CREATE TRIGGER f369_fail_class_info_update "
        "BEFORE UPDATE ON class_info BEGIN "
        "SELECT RAISE(ABORT, 'forced F369 class info write failure'); END"
        )));

    ScheduleEditorDialog dialog(&services, seed->classId);
    QVERIFY(displaysSeededFields(dialog, seed->expected));
    dialog.show();
    QTRY_VERIFY(dialog.isVisible());
    WarningObservation warning;
    acceptNextWarning(&warning);
    QVERIFY(clickNamedButton(dialog, QStringLiteral("Save")));

    const auto after = services.classService()->classInfo(seed->classId);
    QVERIFY(after.has_value());
    QVERIFY(sameClassInfo(*after, seed->expected));
    QVERIFY(warning.accepted);
    QCOMPARE(warning.title, QStringLiteral("Could Not Save"));
    QCOMPARE(warning.text,
        QStringLiteral("The class information could not be saved."));
    QVERIFY(dialog.isVisible());
    emitTranscript(
        QStringLiteral("technical-write-failure"),
        QStringLiteral(
            "warning=Could Not Save,open=1,unchanged=1"
            )
        );
}

void ScheduleEditorDialogParityTests::transientInitialReadFailureBlocksSave()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QString seedError;
    const auto seed = seedClass(services, &seedError);
    QVERIFY2(seed.has_value(), qPrintable(seedError));

    QString renameError;
    QVERIFY2(renameClassInfoTable(
        services.databaseSession()->database(),
        QStringLiteral("class_info"),
        QStringLiteral("class_info_transient"),
        &renameError
        ), qPrintable(renameError));
    ScheduleEditorDialog dialog(&services, seed->classId);
    QVERIFY2(renameClassInfoTable(
        services.databaseSession()->database(),
        QStringLiteral("class_info_transient"),
        QStringLiteral("class_info"),
        &renameError
        ), qPrintable(renameError));

    const QList<QComboBox*> combos = dialogCombos(dialog);
    QCOMPARE(combos.size(), 2);
    QVERIFY(combos.at(0)->currentText().isEmpty());
    QVERIFY(combos.at(1)->currentText().isEmpty());
    QVERIFY(!DialogServices::promptTestDriver().activePrompt().has_value());
    dialog.show();
    QTRY_VERIFY(dialog.isVisible());
    WarningObservation warning;
    acceptNextWarning(&warning);
    QVERIFY(clickNamedButton(dialog, QStringLiteral("Save")));

    const auto after = services.classService()->classInfo(seed->classId);
    QVERIFY(after.has_value());
    QVERIFY(sameClassInfo(*after, seed->expected));
    QVERIFY(warning.accepted);
    QCOMPARE(warning.title, QStringLiteral("Could Not Save"));
    QCOMPARE(warning.text,
        QStringLiteral("The class information could not be saved."));
    QVERIFY(dialog.isVisible());
    emitTranscript(
        QStringLiteral("transient-read-failure"),
        QStringLiteral(
            "warning=Could Not Save,open=1,unchanged=1"
            )
        );
}

QTEST_MAIN(ScheduleEditorDialogParityTests)

#include "schedule_editor_dialog_parity_tests.moc"
