#include "core/settingsmanager.h"
#include "features/calendar/ui/calendar_settings_dialog.h"
#include "features/schedule/ui/schedule_import_dialog.h"
#include "features/schedule/ui/schedule_import_review_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_batch_export_dialog.h"
#include "ui/shared/dialogs/dialog_shell.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QVBoxLayout>

#include <type_traits>

static_assert(std::is_base_of_v<DialogShell, CalendarSettingsDialog>);
static_assert(std::is_base_of_v<DialogShell, ScheduleImportDialog>);
static_assert(std::is_base_of_v<DialogShell, ScheduleImportReviewDialog>);
static_assert(std::is_base_of_v<DialogShell, SpeakingEvalBatchExportDialog>);

class DialogShellTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void appliesSharedLayoutHeaderAndAccessibilityPolicy();
    void standardButtonsProvideDefaultAndEscapeBehavior();
    void persistsGeometryByStableDialogKey();

private:
    QTemporaryDir m_settingsRoot;
};

void DialogShellTests::initTestCase()
{
    QVERIFY(m_settingsRoot.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsRoot.path().toUtf8()
        );
    SettingsManager::instance().clear();
}

void DialogShellTests::appliesSharedLayoutHeaderAndAccessibilityPolicy()
{
    DialogShell dialog(QStringLiteral("test key"));
    dialog.setWindowTitle(QStringLiteral("Policy Test"));
    dialog.setHeader(
        QStringLiteral("Heading"),
        QStringLiteral("Supporting text")
        );
    dialog.show();

    QCOMPARE(dialog.dialogKey(), QStringLiteral("test_key"));
    QCOMPARE(dialog.objectName(), QStringLiteral("test_keyDialog"));
    QCOMPARE(dialog.accessibleName(), QStringLiteral("Policy Test"));
    QCOMPARE(dialog.contentLayout()->contentsMargins(), QMargins(18, 18, 18, 18));
    QCOMPARE(dialog.contentLayout()->spacing(), 10);

    auto* heading = dialog.findChild<QLabel*>(
        QStringLiteral("test_keyHeaderTitle")
        );
    auto* subtitle = dialog.findChild<QLabel*>(
        QStringLiteral("test_keyHeaderSubtitle")
        );
    QVERIFY(heading);
    QVERIFY(subtitle);
    QCOMPARE(heading->text(), QStringLiteral("Heading"));
    QCOMPARE(subtitle->text(), QStringLiteral("Supporting text"));
}

void DialogShellTests::standardButtonsProvideDefaultAndEscapeBehavior()
{
    DialogShell dialog(QStringLiteral("buttonPolicy"));
    auto* buttons = dialog.addButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel
        );
    auto* saveButton = buttons->button(QDialogButtonBox::Save);
    QVERIFY(saveButton);
    QVERIFY(saveButton->isDefault());
    QCOMPARE(
        buttons->objectName(),
        QStringLiteral("buttonPolicyButtonBox")
        );

    QSignalSpy rejectedSpy(&dialog, &QDialog::rejected);
    dialog.show();
    QTest::keyClick(&dialog, Qt::Key_Escape);
    QCOMPARE(rejectedSpy.count(), 1);
}

void DialogShellTests::persistsGeometryByStableDialogKey()
{
    QSize savedSize;
    {
        DialogShell dialog(QStringLiteral("geometryPolicy"));
        dialog.resize(512, 376);
        dialog.show();
        QCoreApplication::processEvents();
        savedSize = dialog.size();
        dialog.close();
    }

    QVERIFY(
        !SettingsManager::instance()
             .get(QStringLiteral("ui/dialogs/geometryPolicy/geometry"))
             .toByteArray()
             .isEmpty()
        );

    DialogShell restored(QStringLiteral("geometryPolicy"));
    restored.resize(300, 200);
    restored.show();
    QCoreApplication::processEvents();
    QCOMPARE(restored.size(), savedSize);
}

QTEST_MAIN(DialogShellTests)
#include "dialog_shell_tests.moc"
