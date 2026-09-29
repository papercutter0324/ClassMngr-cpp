#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "domain/models/class_info.h"
#include "features/schedule/ui/schedule_editor_dialog.h"
#include "next/application/class_details_save_use_case.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QComboBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QtTest/QtTest>

#include <optional>
#include <string>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("schedule-editor-dialog-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createClassWithDetails(ApplicationServices& services)
{
    const auto created = services.classService()->create(
        QStringLiteral("Schedule Editor Dialog Test")
        );
    if (!created)
    {
        return -1;
    }

    auto info = services.classService()->classInfo(*created);
    if (!info)
    {
        return -1;
    }

    info->classGrade = QStringLiteral("E4");
    info->classLevel = QStringLiteral("Theseus");
    info->readingBook = QStringLiteral("Reading Explorer 1");
    info->essayBook = QStringLiteral("4A");
    info->classColor = QStringLiteral("#123456");
    info->fontColor = QStringLiteral("#654321");
    if (!services.classService()->saveClassInfo(*info))
    {
        return -1;
    }

    return *created;
}

class RecordingSavePort final
    : public Application::ClassDetailsSavePort
{
public:
    [[nodiscard]] Domain::Result<void> saveClassDetails(
        const Application::ClassDetailsSaveRequest& value
        ) const override
    {
        ++callCount;
        lastRequest = value;
        return result;
    }

    mutable int callCount = 0;
    mutable std::optional<Application::ClassDetailsSaveRequest> lastRequest;
    Domain::Result<void> result = Domain::Result<void>::success();
};

QList<QComboBox*> dialogCombos(ScheduleEditorDialog& dialog)
{
    return dialog.findChildren<QComboBox*>();
}

bool clickSave(ScheduleEditorDialog& dialog)
{
    for (QPushButton* button : dialog.findChildren<QPushButton*>())
    {
        if (button->text() == QStringLiteral("Save"))
        {
            button->click();
            return true;
        }
    }
    return false;
}

void verifySchedulesOmitted(
    const Application::ClassDetailsSaveRequest& request
    )
{
    QVERIFY(!request.regularTimes.has_value());
    QVERIFY(!request.intensiveTimes.has_value());
}

void verifyUnchangedDetailsRequest(
    const Application::ClassDetailsSaveRequest& request,
    const int classId
    )
{
    QCOMPARE(request.classId.value(), std::to_string(classId));
    QCOMPARE(request.classGrade, std::u16string(u"E4"));
    QCOMPARE(request.classLevel, std::u16string(u"Theseus"));
    QCOMPARE(request.readingBook, std::u16string(u"Reading Explorer 1"));
    QCOMPARE(request.essayBook, std::u16string(u"4A"));
    QCOMPARE(request.classColor, std::u16string(u"#123456"));
    QCOMPARE(request.fontColor, std::u16string(u"#654321"));
    verifySchedulesOmitted(request);
}

}

class ScheduleEditorDialogTests final : public QObject
{
    Q_OBJECT

private slots:
    void unchangedDetailsAreForwardedAndSuccessfulSaveAccepts();
    void gradeChangeClearsBothBooksAndOmitsSchedules();
    void levelChangeClearsBothBooksAndOmitsSchedules();
    void notFoundKeepsDialogOpenWithoutWarning();
    void otherFailureShowsExistingWarningAndKeepsDialogOpen();
};

void ScheduleEditorDialogTests::
unchangedDetailsAreForwardedAndSuccessfulSaveAccepts()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClassWithDetails(services);
    QVERIFY(classId > 0);

    RecordingSavePort savePort;
    ScheduleEditorDialog dialog(&services, classId, nullptr, &savePort);
    const QList<QComboBox*> combos = dialogCombos(dialog);
    QCOMPARE(combos.size(), 2);
    QCOMPARE(combos.at(0)->currentText(), QStringLiteral("E4"));
    QCOMPARE(combos.at(1)->currentText(), QStringLiteral("Theseus"));

    QSignalSpy savedSpy(&dialog, &ScheduleEditorDialog::saved);
    QVERIFY(savedSpy.isValid());
    dialog.show();
    QTRY_VERIFY(dialog.isVisible());
    QVERIFY(clickSave(dialog));

    QCOMPARE(savePort.callCount, 1);
    QVERIFY(savePort.lastRequest.has_value());
    verifyUnchangedDetailsRequest(*savePort.lastRequest, classId);
    QCOMPARE(savedSpy.size(), 1);
    QCOMPARE(savedSpy.at(0).at(0).toInt(), classId);
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
    QVERIFY(!dialog.isVisible());
}

void ScheduleEditorDialogTests::
gradeChangeClearsBothBooksAndOmitsSchedules()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClassWithDetails(services);
    QVERIFY(classId > 0);

    RecordingSavePort savePort;
    ScheduleEditorDialog dialog(&services, classId, nullptr, &savePort);
    const QList<QComboBox*> combos = dialogCombos(dialog);
    QCOMPARE(combos.size(), 2);
    combos.at(0)->setCurrentText(QStringLiteral("E5"));
    QCOMPARE(combos.at(0)->currentText(), QStringLiteral("E5"));

    dialog.show();
    QTRY_VERIFY(dialog.isVisible());
    QVERIFY(clickSave(dialog));

    QCOMPARE(savePort.callCount, 1);
    QVERIFY(savePort.lastRequest.has_value());
    const Application::ClassDetailsSaveRequest& request =
        *savePort.lastRequest;
    QCOMPARE(request.classId.value(), std::to_string(classId));
    QCOMPARE(request.classGrade, std::u16string(u"E5"));
    QVERIFY(request.readingBook.empty());
    QVERIFY(request.essayBook.empty());
    verifySchedulesOmitted(request);
}

void ScheduleEditorDialogTests::
levelChangeClearsBothBooksAndOmitsSchedules()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClassWithDetails(services);
    QVERIFY(classId > 0);

    RecordingSavePort savePort;
    ScheduleEditorDialog dialog(&services, classId, nullptr, &savePort);
    const QList<QComboBox*> combos = dialogCombos(dialog);
    QCOMPARE(combos.size(), 2);
    const int changedLevel = combos.at(1)->findText(QStringLiteral("Perseus"));
    QVERIFY(changedLevel >= 0);
    combos.at(1)->setCurrentIndex(changedLevel);
    QCOMPARE(combos.at(0)->currentText(), QStringLiteral("E4"));
    QCOMPARE(combos.at(1)->currentText(), QStringLiteral("Perseus"));

    dialog.show();
    QTRY_VERIFY(dialog.isVisible());
    QVERIFY(clickSave(dialog));

    QCOMPARE(savePort.callCount, 1);
    QVERIFY(savePort.lastRequest.has_value());
    const Application::ClassDetailsSaveRequest& request =
        *savePort.lastRequest;
    QCOMPARE(request.classId.value(), std::to_string(classId));
    QCOMPARE(request.classGrade, std::u16string(u"E4"));
    QCOMPARE(request.classLevel, std::u16string(u"Perseus"));
    QVERIFY(request.readingBook.empty());
    QVERIFY(request.essayBook.empty());
    verifySchedulesOmitted(request);
}

void ScheduleEditorDialogTests::notFoundKeepsDialogOpenWithoutWarning()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClassWithDetails(services);
    QVERIFY(classId > 0);

    RecordingSavePort savePort;
    savePort.result = Domain::Result<void>::failure({
        .code = Domain::ErrorCode::NotFound,
        .message = "class service unavailable",
        .recoverable = true
    });
    ScheduleEditorDialog dialog(&services, classId, nullptr, &savePort);
    dialog.show();
    QTRY_VERIFY(dialog.isVisible());
    QVERIFY(clickSave(dialog));
    QApplication::processEvents();

    QCOMPARE(savePort.callCount, 1);
    QVERIFY(savePort.lastRequest.has_value());
    verifyUnchangedDetailsRequest(*savePort.lastRequest, classId);
    QVERIFY(dialog.isVisible());
    QVERIFY(!DialogServices::promptTestDriver().activePrompt().has_value());
}

void ScheduleEditorDialogTests::
otherFailureShowsExistingWarningAndKeepsDialogOpen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClassWithDetails(services);
    QVERIFY(classId > 0);

    RecordingSavePort savePort;
    savePort.result = Domain::Result<void>::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "storage write rejected",
        .recoverable = false
    });
    ScheduleEditorDialog dialog(&services, classId, nullptr, &savePort);
    dialog.show();
    QTRY_VERIFY(dialog.isVisible());

    QString warningTitle;
    QString warningText;
    bool warningAccepted = false;
    QTimer::singleShot(0, [&]
    {
        const auto prompt = DialogServices::promptTestDriver().activePrompt();
        if (!prompt)
        {
            return;
        }
        warningTitle = prompt->title;
        warningText = prompt->text;
        warningAccepted = DialogServices::promptTestDriver().accept(prompt->id);
    });
    QVERIFY(clickSave(dialog));

    QCOMPARE(savePort.callCount, 1);
    QVERIFY(savePort.lastRequest.has_value());
    verifyUnchangedDetailsRequest(*savePort.lastRequest, classId);
    QVERIFY(warningAccepted);
    QCOMPARE(warningTitle, QStringLiteral("Could Not Save"));
    QCOMPARE(warningText,
        QStringLiteral("The class information could not be saved."));
    QVERIFY(dialog.isVisible());
}

QTEST_MAIN(ScheduleEditorDialogTests)

#include "schedule_editor_dialog_tests.moc"
