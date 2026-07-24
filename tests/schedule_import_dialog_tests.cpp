#include "core/application_services.h"
#include "core/utils/colorutils.h"
#include "data/data_service.h"
#include "features/schedule/ui/schedule_import_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QtTest>

namespace
{
bool advanceToUserPage(
    ScheduleImportDialog* dialog
    )
{
    auto* normal =
        dialog->findChild<QRadioButton*>(
            QStringLiteral("scheduleImportNormalRadio")
            );
    auto* next =
        dialog->findChild<QPushButton*>(
            QStringLiteral("scheduleImportNextButton")
            );
    auto* pages =
        dialog->findChild<QStackedWidget*>();
    auto* sheets =
        dialog->findChild<QComboBox*>(
            QStringLiteral("scheduleImportSheetCombo")
            );
    if (!normal || !next || !pages || !sheets)
    {
        return false;
    }

    normal->setChecked(true);
    next->click();
    if (pages->currentIndex() == 1)
    {
        return true;
    }

    for (int index = 0; index < sheets->count(); ++index)
    {
        if (sheets->itemData(index).toInt() < 0)
        {
            continue;
        }
        sheets->setCurrentIndex(index);
        next->click();
        if (pages->currentIndex() == 1)
        {
            return true;
        }
    }
    return false;
}
}

class ScheduleImportDialogTests : public QObject
{
    Q_OBJECT

private slots:
    void selectsHighestContrastFontColor();
    void requiresFileAndScheduleKind();
    void mismatchedProfileRequiresConfirmation();
    void suppliedWorkbookBuildsStagedReview();
};

void ScheduleImportDialogTests::selectsHighestContrastFontColor()
{
    QCOMPARE(
        ColorUtils::getContrastingFontColor(
            QColor(QStringLiteral("#10DDDD"))
            ),
        QStringLiteral("#000000")
        );
    QCOMPARE(
        ColorUtils::getContrastingFontColor(
            QColor(QStringLiteral("#E36363"))
            ),
        QStringLiteral("#000000")
        );
    QCOMPARE(
        ColorUtils::getContrastingFontColor(
            QColor(QStringLiteral("#B52A2A"))
            ),
        QStringLiteral("#FFFFFF")
        );
    QCOMPARE(
        ColorUtils::getContrastingFontColor(
            QColor(QStringLiteral("#E4DFC6"))
            ),
        QStringLiteral("#000000")
        );
}

void ScheduleImportDialogTests::requiresFileAndScheduleKind()
{
    ApplicationServices services;
    ScheduleImportDialog dialog(&services);
    auto* next =
        dialog.findChild<QPushButton*>(
            QStringLiteral("scheduleImportNextButton")
            );
    auto* normal =
        dialog.findChild<QRadioButton*>(
            QStringLiteral("scheduleImportNormalRadio")
            );
    auto* status =
        dialog.findChild<QLabel*>(
            QStringLiteral("scheduleImportSourceStatus")
            );
    QVERIFY(next);
    QVERIFY(normal);
    QVERIFY(status);
    QVERIFY(!next->isEnabled());

    dialog.setFilePath(
        QStringLiteral("not-an-xlsx-file.txt")
        );
    QVERIFY(!next->isEnabled());
    normal->setChecked(true);
    QVERIFY(next->isEnabled());
    next->click();
    QVERIFY(
        status->text().contains(
            QStringLiteral(".xlsx")
            )
        );
}

void ScheduleImportDialogTests
    ::mismatchedProfileRequiresConfirmation()
{
    const QString path =
        qEnvironmentVariable(
            "CLASSMNGR_SCHEDULE_IMPORT_SAMPLE"
            );
    if (path.isEmpty())
    {
        QSKIP(
            "Set CLASSMNGR_SCHEDULE_IMPORT_SAMPLE to validate profile-name confirmation."
            );
    }

    ApplicationServices services;
    services.dataService()->saveSetting(
        QStringLiteral("myInfo/name"),
        QStringLiteral("A Name That Is Not In The Workbook")
        );
    ScheduleImportDialog dialog(&services);
    dialog.setFilePath(path);
    QVERIFY(advanceToUserPage(&dialog));

    auto* users =
        dialog.findChild<QComboBox*>(
            QStringLiteral("scheduleImportUserCombo")
            );
    auto* confirmation =
        dialog.findChild<QCheckBox*>(
            QStringLiteral("scheduleImportNameConfirmation")
            );
    auto* next =
        dialog.findChild<QPushButton*>(
            QStringLiteral("scheduleImportNextButton")
            );
    QVERIFY(users);
    QVERIFY(confirmation);
    QVERIFY(next);

    for (int index = 0; index < users->count(); ++index)
    {
        if (users->itemData(index).toInt() >= 0)
        {
            users->setCurrentIndex(index);
            break;
        }
    }
    QVERIFY(!confirmation->isHidden());
    QVERIFY(!next->isEnabled());
    confirmation->setChecked(true);
    QVERIFY(next->isEnabled());
}

void ScheduleImportDialogTests
    ::suppliedWorkbookBuildsStagedReview()
{
    const QString path =
        qEnvironmentVariable(
            "CLASSMNGR_SCHEDULE_IMPORT_SAMPLE"
            );
    if (path.isEmpty())
    {
        QSKIP(
            "Set CLASSMNGR_SCHEDULE_IMPORT_SAMPLE to validate the staged dialog with an external workbook."
            );
    }

    ApplicationServices services;
    services.dataService()->saveSetting(
        QStringLiteral("myInfo/name"),
        QString()
        );
    ScheduleImportDialog dialog(&services);
    dialog.setFilePath(path);
    auto* next =
        dialog.findChild<QPushButton*>(
            QStringLiteral("scheduleImportNextButton")
            );
    auto* pages =
        dialog.findChild<QStackedWidget*>();
    QVERIFY(next);
    QVERIFY(pages);
    QVERIFY(advanceToUserPage(&dialog));
    QCOMPARE(pages->currentIndex(), 1);

    auto* users =
        dialog.findChild<QComboBox*>(
            QStringLiteral("scheduleImportUserCombo")
            );
    QVERIFY(users);
    int selectedUser = -1;
    for (int index = 0; index < users->count(); ++index)
    {
        if (users->itemData(index).toInt() >= 0)
        {
            selectedUser = index;
            break;
        }
    }
    QVERIFY(selectedUser >= 0);
    users->setCurrentIndex(selectedUser);
    QVERIFY(next->isEnabled());
    next->click();
    QCOMPARE(pages->currentIndex(), 2);

    const auto roomChoices =
        dialog.findChildren<QComboBox*>(
            QRegularExpression(
                QStringLiteral(
                    "^scheduleImportTeacherRoom_"
                    )
                )
            );
    for (QComboBox* room : roomChoices)
    {
        if (!room->currentData().toString().isEmpty())
        {
            continue;
        }
        for (int index = 0;
             index < room->count();
             ++index)
        {
            if (!room->itemData(index).toString().isEmpty())
            {
                room->setCurrentIndex(index);
                break;
            }
        }
    }

    if (
        auto* acknowledgement =
            dialog.findChild<QCheckBox*>(
                QStringLiteral(
                    "scheduleImportWarningAcknowledgement"
                    )
                )
        )
    {
        acknowledgement->setChecked(true);
    }

    auto* summary =
        dialog.findChild<QLabel*>(
            QStringLiteral("scheduleImportReviewSummary")
            );
    auto* reviewStatus =
        dialog.findChild<QLabel*>(
            QStringLiteral("scheduleImportReviewStatus")
            );
    auto* import =
        dialog.findChild<QPushButton*>(
            QStringLiteral("scheduleImportAcceptButton")
            );
    const auto colorButtons =
        dialog.findChildren<QPushButton*>(
            QRegularExpression(
                QStringLiteral(
                    "^scheduleImportClassColor_"
                    )
                )
            );
    QVERIFY(summary);
    QVERIFY(reviewStatus);
    QVERIFY(import);
    QVERIFY(!colorButtons.isEmpty());
    bool foundSpreadsheetColor = false;
    for (const QPushButton* colorButton : colorButtons)
    {
        QVERIFY(
            colorButton->text().startsWith(
                QStringLiteral("Color #")
                )
            );
        foundSpreadsheetColor =
            foundSpreadsheetColor
            || !colorButton->text().contains(
                QStringLiteral("#FFFFFF")
                );
    }
    QVERIFY(foundSpreadsheetColor);
    bool foundInformativeClassTarget = false;
    const auto classActions =
        dialog.findChildren<QComboBox*>(
            QRegularExpression(
                QStringLiteral(
                    "^scheduleImportClassAction_"
                    )
                )
            );
    for (const QComboBox* action : classActions)
    {
        for (int index = 0; index < action->count(); ++index)
        {
            foundInformativeClassTarget =
                foundInformativeClassTarget
                || action->itemText(index).contains(
                    QStringLiteral(
                        "E4 Hercules (김선생 Tues 4pm) [Reg]"
                        )
                    );
        }
    }
    QVERIFY(foundInformativeClassTarget);
    QVERIFY(
        summary->text().contains(
            QStringLiteral("existing schedule")
            )
        );
    QVERIFY(
        summary->text().contains(
            QStringLiteral("My Information name")
            )
        );
    QStringList reviewDetails;
    const auto detailLabels =
        dialog.findChildren<QLabel*>(
            QRegularExpression(
                QStringLiteral(
                    "^scheduleImportClassDifferences_"
                    )
                )
            );
    for (const QLabel* detail : detailLabels)
    {
        reviewDetails.append(detail->text());
    }
    const QString failureDetails =
        reviewStatus->text()
        + QLatin1Char('\n')
        + reviewDetails.join(QLatin1Char('\n'));
    QVERIFY2(
        import->isEnabled(),
        qPrintable(failureDetails)
        );
}

QTEST_MAIN(ScheduleImportDialogTests)

#include "schedule_import_dialog_tests.moc"
