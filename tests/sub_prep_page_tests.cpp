#include "core/application_services.h"
#include "data/data_service.h"
#include "features/sub_prep/ui/sub_prep_page.h"
#include "features/sub_prep/ui/sub_prep_print_dialog.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "ui/shared/widgets/sections/schedule_section_widget.h"

#include <QtTest>

#include <QLineEdit>
#include <QLabel>
#include <QScrollArea>
#include <QCheckBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace ScheduleSectionWidgetTestStubs
{
void reset();
}

namespace
{
int layoutIndexForSection(
    QVBoxLayout* layout,
    const QString& section
    )
{
    for (int index = 0; index < layout->count(); ++index)
    {
        QWidget* widget =
            layout->itemAt(index)->widget();

        if (
            widget
            && widget->property("subPrepSection").toString() == section
            )
        {
            return index;
        }
    }

    return -1;
}
}

class SubPrepPageTests : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void sectionsAppearInRequestedOrderAndUseExpectedEditability();
    void freshAndExistingGradingSettingsResolveWithoutDataLoss();
    void zoomUnavailableHidesStoredCredentials();
    void printDialogSelectsVacationDaysFromTheCurrentOrFollowingWeek();
};

void SubPrepPageTests::init()
{
    ScheduleSectionWidgetTestStubs::reset();
}

void SubPrepPageTests
    ::sectionsAppearInRequestedOrderAndUseExpectedEditability()
{
    ApplicationServices services;
    SubPrepPage page(&services);

    auto* scrollArea =
        page.findChild<QScrollArea*>();
    QVERIFY(scrollArea);
    QVERIFY(scrollArea->widget());

    auto* layout =
        qobject_cast<QVBoxLayout*>(
            scrollArea->widget()->layout()
            );
    QVERIFY(layout);

    const QStringList sections{
        QStringLiteral("campus"),
        QStringLiteral("zoom"),
        QStringLiteral("materials"),
        QStringLiteral("grading"),
        QStringLiteral("schedule"),
        QStringLiteral("class_information"),
        QStringLiteral("sub_notes")
    };

    int previousIndex = -1;

    for (const QString& section : sections)
    {
        const int index =
            layoutIndexForSection(
                layout,
                section
                );
        QVERIFY2(index > previousIndex, qPrintable(section));
        previousIndex = index;
    }

    auto* zoomLogin =
        page.findChild<QLineEdit*>(
            QStringLiteral("subPrepZoomLoginIdEdit")
            );
    auto* zoomPassword =
        page.findChild<QLineEdit*>(
            QStringLiteral("subPrepZoomPasswordEdit")
            );
    auto* materials =
        page.findChild<QTextEdit*>(
            QStringLiteral("subPrepClassMaterialsEdit")
            );
    auto* grading =
        page.findChild<QTextEdit*>(
            QStringLiteral("subPrepGradingInstructionsEdit")
            );
    auto* special =
        page.findChild<QTextEdit*>(
            QStringLiteral("subPrepSpecialInstructionsEdit")
            );
    auto* notes =
        page.findChild<QTextEdit*>(
            QStringLiteral("subPrepNotesEdit")
            );
    auto* schedule =
        page.findChild<ScheduleSectionWidget*>(
            QStringLiteral("subPrepScheduleWidget")
            );

    QVERIFY(zoomLogin && zoomLogin->isReadOnly());
    QVERIFY(zoomPassword && zoomPassword->isReadOnly());
    QVERIFY(materials && !materials->isReadOnly());
    QVERIFY(grading && !grading->isReadOnly());
    QVERIFY(special && !special->isReadOnly());
    QVERIFY(notes && !notes->isReadOnly());
    QVERIFY(schedule);

    auto* importantHeading =
        page.findChild<QLabel*>(
            QStringLiteral("subPrepImportantInformationHeading")
            );
    auto* scheduleHeading =
        page.findChild<QLabel*>(
            QStringLiteral("subPrepScheduleHeading")
            );
    auto* classInformationHeading =
        page.findChild<QLabel*>(
            QStringLiteral("subPrepClassInformationHeading")
            );

    QVERIFY(importantHeading);
    QVERIFY(scheduleHeading);
    QVERIFY(classInformationHeading);
    QCOMPARE(scheduleHeading->font(), importantHeading->font());
    QCOMPARE(classInformationHeading->font(), importantHeading->font());
    QCOMPARE(scheduleHeading->alignment(), importantHeading->alignment());
    QCOMPARE(
        classInformationHeading->alignment(),
        importantHeading->alignment()
        );

    auto* scheduleCard =
        qobject_cast<SectionCard*>(schedule->parentWidget());
    QVERIFY(scheduleCard);
    auto* scheduleCardTitle =
        scheduleCard->findChild<QLabel*>(
            QStringLiteral("sectionTitle")
            );
    QVERIFY(scheduleCardTitle && scheduleCardTitle->isHidden());

    QTextEdit* classNotes = nullptr;
    QTextEdit* teacherNotes = nullptr;

    for (QTextEdit* textEdit : page.findChildren<QTextEdit*>())
    {
        if (textEdit->property("classId").toInt() == 42)
        {
            classNotes = textEdit;
        }

        if (textEdit->property("teacherId").toInt() == 7)
        {
            teacherNotes = textEdit;
        }
    }

    QVERIFY(classNotes && classNotes->isReadOnly());
    QCOMPARE(
        classNotes->toPlainText(),
        QStringLiteral("Read chapter three.")
        );
    QVERIFY(teacherNotes && teacherNotes->isReadOnly());
    QCOMPARE(
        teacherNotes->toPlainText(),
        QStringLiteral("Call before class.")
        );

    const auto teacherCards =
        page.findChildren<SectionCard*>(
            QStringLiteral("subPrepTeacherSectionCard")
            );
    QCOMPARE(teacherCards.size(), 1);
    QCOMPARE(teacherCards.first()->property("teacherId").toInt(), 7);

    auto* teacherHeading =
        teacherCards.first()->findChild<QLabel*>(
            QStringLiteral("sectionTitle")
            );
    QVERIFY(teacherHeading);
    QCOMPARE(
        teacherHeading->text(),
        QStringLiteral("Susan: E4 Hercules")
        );

    auto* classDetails =
        page.findChild<QWidget*>(
            QStringLiteral("subPrepClassDetails")
            );
    QVERIFY(classDetails);
    QCOMPARE(classDetails->property("classId").toInt(), 42);
    QVERIFY(classDetails->layout());
    QCOMPARE(classDetails->layout()->contentsMargins().left(), 0);

    auto* controls =
        schedule->findChild<QWidget*>(
            QStringLiteral("scheduleControls")
            );
    QVERIFY(controls && controls->isHidden());
    QCOMPARE(
        page.currentSectionKey(),
        QStringLiteral("sub_prep_important")
        );
    page.scrollToSection(SubPrepSection::SubNotes);
    QCOMPARE(
        page.currentSectionKey(),
        QStringLiteral("sub_prep_notes")
        );
    bool foundAdditionalNotes = false;

    for (QLabel* label : page.findChildren<QLabel*>())
    {
        if (label->text() == QStringLiteral("Additional Notes"))
        {
            foundAdditionalNotes = true;
            break;
        }
    }

    QVERIFY(foundAdditionalNotes);
}

void SubPrepPageTests
    ::freshAndExistingGradingSettingsResolveWithoutDataLoss()
{
    ApplicationServices services;

    {
        SubPrepPage fresh(&services);
        auto* grading =
            fresh.findChild<QTextEdit*>(
                QStringLiteral("subPrepGradingInstructionsEdit")
                );
        auto* special =
            fresh.findChild<QTextEdit*>(
                QStringLiteral("subPrepSpecialInstructionsEdit")
                );

        QVERIFY(grading);
        QVERIFY(special);
        QVERIFY(
            grading->toPlainText().startsWith(
                QStringLiteral("Scoring: 0 / 20 / 40")
                )
            );
        QVERIFY(
            !grading->toPlainText().contains(
                QStringLiteral("Additional Rules")
                )
            );
        QCOMPARE(special->toPlainText(), QStringLiteral("N/A"));
    }

    ScheduleSectionWidgetTestStubs::reset();
    services.dataService()->saveSetting(
        QStringLiteral("subPrep/bookReportGrading"),
        QStringLiteral("Custom legacy grading\nAdditional Rules: keep here")
        );
    services.dataService()->saveSetting(
        QStringLiteral("subPrep/subComments"),
        QStringLiteral("Existing substitute note")
        );

    SubPrepPage existing(&services);
    auto* grading =
        existing.findChild<QTextEdit*>(
            QStringLiteral("subPrepGradingInstructionsEdit")
            );
    auto* special =
        existing.findChild<QTextEdit*>(
            QStringLiteral("subPrepSpecialInstructionsEdit")
            );
    auto* notes =
        existing.findChild<QTextEdit*>(
            QStringLiteral("subPrepNotesEdit")
            );

    QCOMPARE(
        grading->toPlainText(),
        QStringLiteral("Custom legacy grading\nAdditional Rules: keep here")
        );
    QVERIFY(special->toPlainText().isEmpty());
    QCOMPARE(
        notes->toPlainText(),
        QStringLiteral("Existing substitute note")
        );

    special->setPlainText(QStringLiteral("Bring spare books"));
    notes->setPlainText(QStringLiteral("Updated substitute note"));
    QVERIFY(existing.saveChanges());
    QCOMPARE(
        services.dataService()
            ->loadSetting(
                QStringLiteral("subPrep/bookReportSpecialInstructions")
                )
            .toString(),
        QStringLiteral("Bring spare books")
        );
    QCOMPARE(
        services.dataService()
            ->loadSetting(
                QStringLiteral("subPrep/subComments")
                )
            .toString(),
        QStringLiteral("Updated substitute note")
        );
}

void SubPrepPageTests
    ::zoomUnavailableHidesStoredCredentials()
{
    ApplicationServices services;
    services.dataService()->saveSetting(
        QStringLiteral("myInfo/zoomLoginId"),
        QStringLiteral("teacher@example.com")
        );
    services.dataService()->saveSetting(
        QStringLiteral("myInfo/zoomPassword"),
        QStringLiteral("secret")
        );
    services.dataService()->saveSetting(
        QStringLiteral("myInfo/zoomNotAvailable"),
        true
        );

    SubPrepPage unavailable(&services);
    QCOMPARE(
        unavailable
            .findChild<QLineEdit*>(
                QStringLiteral("subPrepZoomLoginIdEdit")
                )
            ->text(),
        QStringLiteral("N/A")
        );
    QCOMPARE(
        unavailable
            .findChild<QLineEdit*>(
                QStringLiteral("subPrepZoomPasswordEdit")
                )
            ->text(),
        QStringLiteral("N/A")
        );

    services.dataService()->saveSetting(
        QStringLiteral("myInfo/zoomNotAvailable"),
        false
        );

    SubPrepPage available(&services);
    QCOMPARE(
        available
            .findChild<QLineEdit*>(
                QStringLiteral("subPrepZoomLoginIdEdit")
                )
            ->text(),
        QStringLiteral("teacher@example.com")
        );
    QCOMPARE(
        available
            .findChild<QLineEdit*>(
                QStringLiteral("subPrepZoomPasswordEdit")
            )
            ->text(),
        QStringLiteral("secret")
        );

    ScheduleSectionWidgetTestStubs::reset();
    ApplicationServices legacyServices;
    legacyServices.dataService()->saveSetting(
        QStringLiteral("subPrep/personalZoomEmail"),
        QStringLiteral("legacy@example.com")
        );
    legacyServices.dataService()->saveSetting(
        QStringLiteral("subPrep/personalZoomPassword"),
        QStringLiteral("legacy secret")
        );
    legacyServices.dataService()->saveSetting(
        QStringLiteral("subPrep/personalZoomNotAvailable"),
        false
        );

    SubPrepPage legacy(&legacyServices);
    QCOMPARE(
        legacy
            .findChild<QLineEdit*>(
                QStringLiteral("subPrepZoomLoginIdEdit")
                )
            ->text(),
        QStringLiteral("legacy@example.com")
        );
    QCOMPARE(
        legacyServices.dataService()
            ->loadSetting(
                QStringLiteral("myInfo/zoomLoginId")
                )
            .toString(),
        QStringLiteral("legacy@example.com")
        );
}

void SubPrepPageTests
    ::printDialogSelectsVacationDaysFromTheCurrentOrFollowingWeek()
{
    const QDate wednesday(2026, 7, 15);
    CalendarEvent currentWeekVacation;
    currentWeekVacation.eventType = QStringLiteral("Vacation");
    currentWeekVacation.startDate = QDate(2026, 7, 14);
    currentWeekVacation.endDate = QDate(2026, 7, 16);

    CalendarEvent nextWeekVacation;
    nextWeekVacation.eventType = QStringLiteral("Vacation");
    nextWeekVacation.startDate = QDate(2026, 7, 20);
    nextWeekVacation.endDate = QDate(2026, 7, 21);

    CalendarEvent currentWeekHoliday;
    currentWeekHoliday.eventType = QStringLiteral("Holiday");
    currentWeekHoliday.startDate = QDate(2026, 7, 13);
    currentWeekHoliday.endDate = QDate(2026, 7, 17);

    QCOMPARE(
        SubPrepPrintDialog::defaultSelectedDays(
            {currentWeekVacation, nextWeekVacation, currentWeekHoliday},
            wednesday
            ),
        QStringList({
            QStringLiteral("Tuesday"),
            QStringLiteral("Wednesday"),
            QStringLiteral("Thursday")
        })
        );
    QCOMPARE(
        SubPrepPrintDialog::defaultSelectedDays(
            {nextWeekVacation, currentWeekHoliday},
            wednesday
            ),
        QStringList({
            QStringLiteral("Monday"),
            QStringLiteral("Tuesday")
        })
        );
    QVERIFY(
        SubPrepPrintDialog::defaultSelectedDays(
            {currentWeekHoliday},
            wednesday
            ).isEmpty()
        );

    SubPrepPrintDialog dialog(
        {currentWeekVacation},
        wednesday
        );
    QVERIFY(
        dialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepPrintTuesdayCheckBox")
            )->isChecked()
        );
    QVERIFY(
        dialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepPrintWednesdayCheckBox")
            )->isChecked()
        );
    QVERIFY(
        dialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepPrintThursdayCheckBox")
            )->isChecked()
        );
    QVERIFY(
        !dialog.findChild<QCheckBox*>(
            QStringLiteral("subPrepPrintMondayCheckBox")
            )->isChecked()
        );
    QVERIFY(
        dialog.findChild<QPushButton*>(
            QStringLiteral("subPrepPrintCancelButton")
            )
        );
    QVERIFY(
        dialog.findChild<QPushButton*>(
            QStringLiteral("subPrepPrintSaveAsButton")
            )
        );
    QVERIFY(
        dialog.findChild<QPushButton*>(
            QStringLiteral("subPrepPrintButton")
            )
        );
}

QTEST_MAIN(SubPrepPageTests)

#include "sub_prep_page_tests.moc"
