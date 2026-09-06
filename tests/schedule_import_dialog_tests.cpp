#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/utils/colorutils.h"
#include "app/services/feature_services.h"
#include "features/schedule/ui/schedule_import_dialog.h"
#include "features/schedule/ui/schedule_import_review_dialog.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/widgets/no_wheel_combobox.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QSplitterHandle>
#include <QTableWidget>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtTest>

#include <algorithm>

#include <zlib.h>

namespace ScheduleWidgetTestStubs
{
void reset();
void setIncludeAdditionalClass(bool include);
void setMatchImportedClasses(bool match);
void setPossibleImportedClasses(bool match);
void setExistingIntensiveHours(bool exists);
void setIncludeAlternativeMatchingClass(bool include);
}

class ScheduleImportDialogTests : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void selectsHighestContrastFontColor();
    void requiresFileAndScheduleKind();
    void reportsAsyncWorkbookFailure();
    void discardsSupersededAndClosedLoads();
    void acceptedReviewCanTearDownSourceDialog();
    void mismatchedProfileRequiresConfirmation();
    void compactFlowAndReviewPresentation();
    void reviewWarningsUseDedicatedTab();
    void intensiveModeChoiceReflectsExistingSchedule();
    void regularPreviewShowsFullEssayGrid();
    void possibleMatchIsPreselectedForUpdate();
    void reviewWarnsForDuplicateClassTargets();
    void reviewWarnsForOverlappingProjectedTimes();
    void reviewWarnsWhenRetainedIntensiveClassOverlaps();
    void reviewPreviewUsesSavedScheduleDisplaySettings();
    void intensivePreviewPreservesEssayAndLunchBlocks();
    void suppliedWorkbookBuildsStagedReview();
};

namespace
{
void saveSettingOrFail(
    SettingsService* settingsService,
    const QString& key,
    const QVariant& value
    )
{
    QVERIFY(settingsService);
    QVERIFY(settingsService->save(key, value).has_value());
}

constexpr int ExpectedSourceDialogWidth = 436;

int actionIndex(
    const QComboBox* combo,
    ScheduleImportClassAction action,
    int target = -2
    )
{
    if (!combo)
    {
        return -1;
    }

    for (int index = 0; index < combo->count(); ++index)
    {
        if (
            combo->itemData(index, Qt::UserRole).toInt()
            != static_cast<int>(action)
            )
        {
            continue;
        }
        if (
            target == -2
            || combo->itemData(index, Qt::UserRole + 1).toInt()
                == target
            )
        {
            return index;
        }
    }
    return -1;
}

void appendLe16(
    QByteArray& data,
    quint16 value
    )
{
    data.append(static_cast<char>(value & 0xff));
    data.append(static_cast<char>((value >> 8) & 0xff));
}

void appendLe32(
    QByteArray& data,
    quint32 value
    )
{
    appendLe16(data, static_cast<quint16>(value & 0xffff));
    appendLe16(data, static_cast<quint16>((value >> 16) & 0xffff));
}

struct TestZipEntry
{
    QByteArray name;
    QByteArray contents;
    quint32 crc = 0;
    quint32 localOffset = 0;
};

QByteArray storedZip(
    QList<TestZipEntry> entries
    )
{
    QByteArray result;
    for (TestZipEntry& entry : entries)
    {
        entry.localOffset =
            static_cast<quint32>(result.size());
        entry.crc =
            static_cast<quint32>(
                crc32(
                    crc32(0L, Z_NULL, 0),
                    reinterpret_cast<const Bytef*>(
                        entry.contents.constData()
                        ),
                    static_cast<uInt>(
                        entry.contents.size()
                        )
                    )
                );
        appendLe32(result, 0x04034b50);
        appendLe16(result, 20);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe32(result, entry.crc);
        appendLe32(result, entry.contents.size());
        appendLe32(result, entry.contents.size());
        appendLe16(result, entry.name.size());
        appendLe16(result, 0);
        result.append(entry.name);
        result.append(entry.contents);
    }

    const quint32 centralOffset =
        static_cast<quint32>(result.size());
    for (const TestZipEntry& entry : entries)
    {
        appendLe32(result, 0x02014b50);
        appendLe16(result, 20);
        appendLe16(result, 20);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe32(result, entry.crc);
        appendLe32(result, entry.contents.size());
        appendLe32(result, entry.contents.size());
        appendLe16(result, entry.name.size());
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe16(result, 0);
        appendLe32(result, 0);
        appendLe32(result, entry.localOffset);
        result.append(entry.name);
    }

    const quint32 centralSize =
        static_cast<quint32>(result.size())
        - centralOffset;
    appendLe32(result, 0x06054b50);
    appendLe16(result, 0);
    appendLe16(result, 0);
    appendLe16(result, entries.size());
    appendLe16(result, entries.size());
    appendLe32(result, centralSize);
    appendLe32(result, centralOffset);
    appendLe16(result, 0);
    return result;
}

QByteArray dialogWorkbookData()
{
    const QByteArray workbook = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"
                  xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
          <sheets>
            <sheet name="Current" sheetId="1" r:id="rId1"/>
            <sheet name="Alternate" sheetId="2" r:id="rId2"/>
          </sheets>
        </workbook>)");
    const QByteArray relationships = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
          <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>
          <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet2.xml"/>
        </Relationships>)");
    const QByteArray styles = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
          <fonts count="1"><font><color rgb="FF000000"/></font></fonts>
          <fills count="3">
            <fill><patternFill patternType="none"/></fill>
            <fill><patternFill patternType="gray125"/></fill>
            <fill><patternFill patternType="solid"><fgColor rgb="FF6D9EEB"/></patternFill></fill>
          </fills>
          <cellXfs count="2">
            <xf fontId="0" fillId="0"/>
            <xf fontId="0" fillId="2"/>
          </cellXfs>
        </styleSheet>)");
    const QByteArray sheet1 = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
          <sheetData>
            <row r="1">
              <c r="A1" t="inlineStr"><is><t>Alice</t></is></c>
              <c r="B1" t="inlineStr"><is><t>MON</t></is></c>
              <c r="C1" t="inlineStr"><is><t>TUE</t></is></c>
              <c r="D1" t="inlineStr"><is><t>WED</t></is></c>
              <c r="E1" t="inlineStr"><is><t>THU</t></is></c>
              <c r="F1" t="inlineStr"><is><t>FRI</t></is></c>
            </row>
            <row r="2">
              <c r="A2" t="inlineStr"><is><t>4:00~4:55</t></is></c>
              <c r="B2" t="inlineStr"><is><t>박선생 (415)&#10;M3-Song's</t></is></c>
            </row>
            <row r="3">
              <c r="A3" t="inlineStr"><is><t>5:00~5:55</t></is></c>
              <c r="C3" s="1" t="inlineStr"><is><t>최선생 (416)&#10;E4-Hercules</t></is></c>
              <c r="E3" s="1" t="inlineStr"><is><t>최선생 (416)&#10;E4-Hercules</t></is></c>
            </row>
            <row r="4">
              <c r="A4" t="inlineStr"><is><t>6:00~6:55</t></is></c>
              <c r="D4" t="inlineStr"><is><t>김선생 (413)&#10;E4-Theseus</t></is></c>
            </row>
          </sheetData>
        </worksheet>)");
    const QByteArray sheet2 = QByteArrayLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
          <sheetData>
            <row r="1">
              <c r="A1" t="inlineStr"><is><t>Charlie</t></is></c>
              <c r="B1" t="inlineStr"><is><t>MON</t></is></c>
              <c r="C1" t="inlineStr"><is><t>TUE</t></is></c>
              <c r="D1" t="inlineStr"><is><t>WED</t></is></c>
              <c r="E1" t="inlineStr"><is><t>THU</t></is></c>
              <c r="F1" t="inlineStr"><is><t>FRI</t></is></c>
            </row>
            <row r="2">
              <c r="A2" t="inlineStr"><is><t>4:00~4:55</t></is></c>
              <c r="B2" t="inlineStr"><is><t>이선생 (512)&#10;E5-Athena</t></is></c>
            </row>
          </sheetData>
        </worksheet>)");

    return storedZip({
        {QByteArrayLiteral("xl/workbook.xml"), workbook},
        {QByteArrayLiteral("xl/_rels/workbook.xml.rels"), relationships},
        {QByteArrayLiteral("xl/styles.xml"), styles},
        {QByteArrayLiteral("xl/worksheets/sheet1.xml"), sheet1},
        {QByteArrayLiteral("xl/worksheets/sheet2.xml"), sheet2}
    });
}

QString writeDialogWorkbook(
    QTemporaryDir* directory
    )
{
    const QString path =
        directory->filePath(QStringLiteral("schedule.xlsx"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return {};
    }
    file.write(dialogWorkbookData());
    file.close();
    return path;
}

bool loadSourceSelections(
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
    auto* sheets =
        dialog->findChild<QComboBox*>(
            QStringLiteral("scheduleImportSheetCombo")
            );
    auto* progress =
        dialog->findChild<QProgressBar*>(
            QStringLiteral("scheduleImportProgressBar")
            );
    if (!normal || !next || !sheets || !progress)
    {
        return false;
    }

    normal->setChecked(true);
    next->click();
    if (
        progress->isHidden()
        || next->isEnabled()
        )
    {
        return false;
    }

    for (
        int attempt = 0;
        attempt < 500 && !progress->isHidden();
        ++attempt
        )
    {
        QTest::qWait(10);
    }
    if (!progress->isHidden())
    {
        return false;
    }

    if (dialog->width() != ExpectedSourceDialogWidth)
    {
        return false;
    }

    for (int index = 0; index < sheets->count(); ++index)
    {
        if (sheets->itemData(index).toInt() < 0)
        {
            continue;
        }
        sheets->setCurrentIndex(index);
        if (
            auto* users =
                dialog->findChild<QComboBox*>(
                    QStringLiteral("scheduleImportUserCombo")
                    );
            users && users->isVisible()
            )
        {
            return true;
        }
    }
    return false;
}
}

void ScheduleImportDialogTests::cleanup()
{
    ScheduleWidgetTestStubs::reset();
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

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
    auto* browse =
        dialog.findChild<QPushButton*>(
            QStringLiteral("scheduleImportBrowseButton")
            );
    auto* scheduleTypeSection =
        dialog.findChild<QGroupBox*>(
            QStringLiteral("scheduleImportScheduleTypeSection")
            );
    auto* fileSection =
        dialog.findChild<QGroupBox*>(
            QStringLiteral("scheduleImportFileSection")
            );
    QVERIFY(next);
    QVERIFY(normal);
    QVERIFY(status);
    QVERIFY(browse);
    QVERIFY(scheduleTypeSection);
    QVERIFY(fileSection);
    QVERIFY(!next->isEnabled());
    QCOMPARE(dialog.width(), ExpectedSourceDialogWidth);
    QCOMPARE(dialog.minimumHeight(), 520);
    QCOMPARE(dialog.maximumHeight(), 520);
    QCOMPARE(next->text(), QStringLiteral("Load"));
    QCOMPARE(browse->text(), QStringLiteral("Browse"));
    QCOMPARE(
        status->text(),
        QStringLiteral("Choose a file and schedule type.")
        );
    QVERIFY(status->alignment().testFlag(Qt::AlignHCenter));
    QCOMPARE(status->parentWidget()->layout()->indexOf(status), 0);
    QCOMPARE(status->parentWidget()->layout()->spacing(), 10);
    QCOMPARE(fileSection->title(), QStringLiteral("Choose a spreadsheet"));
    QVERIFY(scheduleTypeSection->isHidden());

    dialog.setFilePath(
        QStringLiteral("not-an-xlsx-file.txt")
        );
    QVERIFY(!scheduleTypeSection->isHidden());
    QCOMPARE(
        status->text(),
        QStringLiteral("Ready to read the spreadsheet.")
        );
    QCOMPARE(scheduleTypeSection->title(), QStringLiteral("Schedule type"));
    QCOMPARE(normal->text(), QStringLiteral("Regular"));
    auto* intensive =
        dialog.findChild<QRadioButton*>(
            QStringLiteral("scheduleImportIntensiveRadio")
            );
    QVERIFY(intensive);
    QCOMPARE(intensive->text(), QStringLiteral("Intensives"));
    QCOMPARE(scheduleTypeSection->layout()->spacing(), 16);
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
    ScheduleWidgetTestStubs::setMatchImportedClasses(true);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path =
        writeDialogWorkbook(&directory);
    QVERIFY(!path.isEmpty());

    ApplicationServices services;
    saveSettingOrFail(services.settingsService(),
        QStringLiteral("myInfo/name"),
        QStringLiteral("A Name That Is Not In The Workbook")
        );
    ScheduleImportDialog dialog(&services);
    dialog.setFilePath(path);
    dialog.show();
    QCoreApplication::processEvents();
    QCOMPARE(dialog.height(), 520);
    QVERIFY(loadSourceSelections(&dialog));

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
    auto* status =
        dialog.findChild<QLabel*>(
            QStringLiteral("scheduleImportUserStatus")
            );
    QVERIFY(users);
    QVERIFY(confirmation);
    QVERIFY(next);
    QVERIFY(status);

    for (int index = 0; index < users->count(); ++index)
    {
        if (users->itemData(index).toInt() >= 0)
        {
            users->setCurrentIndex(index);
            break;
        }
    }
    QVERIFY(!confirmation->isHidden());
    QVERIFY(!confirmation->isChecked());
    QCOMPARE(
        confirmation->text(),
        QStringLiteral(
            "Update my name on the My Information page to match the selected name."
            )
        );
    QCOMPARE(
        status->text(),
        QStringLiteral(
            "Entered name on the My Information page: "
            "A Name That Is Not In The Workbook"
            )
        );
    QVERIFY(next->isEnabled());

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Rejected);
    DialogServices::setUserPromptServiceForTesting(&prompts);
    next->click();
    QCOMPARE(prompts.confirmations.size(), 1);
    QCOMPARE(
        prompts.confirmations.constFirst().title,
        QStringLiteral("Name Mismatch")
        );
    QCOMPARE(
        prompts.confirmations.constFirst().message,
        QStringLiteral(
            "The selected name does not match the name entered "
            "on the My Information page. Do you want to continue anyway?"
            )
        );
    QVERIFY(
        !dialog.findChild<ScheduleImportReviewDialog*>()
        );

    prompts.scriptedChoices.enqueue(PromptChoice::Accepted);
    next->click();
    QCOMPARE(prompts.confirmations.size(), 2);
    auto* continueReview =
        dialog.findChild<ScheduleImportReviewDialog*>();
    QVERIFY(continueReview);
    QPointer<ScheduleImportReviewDialog> closedReview =
        continueReview;
    continueReview->reject();
    QCoreApplication::sendPostedEvents(
        nullptr,
        QEvent::DeferredDelete
        );
    QVERIFY(closedReview.isNull());

    confirmation->setChecked(true);
    QVERIFY(next->isEnabled());
    next->click();

    auto* review =
        dialog.findChild<ScheduleImportReviewDialog*>();
    QVERIFY(review);
    auto* reviewSummary =
        review->findChild<QLabel*>(
            QStringLiteral("scheduleImportReviewSummary")
            );
    QVERIFY(reviewSummary);
    QVERIFY(
        reviewSummary->text().contains(
            QStringLiteral(
                "My Information name will be updated to “Alice”."
                )
            )
        );
}

void ScheduleImportDialogTests::reportsAsyncWorkbookFailure()
{
    ApplicationServices services;
    ScheduleImportDialog dialog(&services);
    dialog.setFilePath(
        QStringLiteral("/definitely/missing/schedule.xlsx")
        );

    auto* normal =
        dialog.findChild<QRadioButton*>(
            QStringLiteral("scheduleImportNormalRadio")
            );
    auto* next =
        dialog.findChild<QPushButton*>(
            QStringLiteral("scheduleImportNextButton")
            );
    auto* browse =
        dialog.findChild<QPushButton*>(
            QStringLiteral("scheduleImportBrowseButton")
            );
    auto* status =
        dialog.findChild<QLabel*>(
            QStringLiteral("scheduleImportSourceStatus")
            );
    auto* progress =
        dialog.findChild<QProgressBar*>(
            QStringLiteral("scheduleImportProgressBar")
            );
    QVERIFY(normal);
    QVERIFY(next);
    QVERIFY(browse);
    QVERIFY(status);
    QVERIFY(progress);

    normal->setChecked(true);
    next->click();
    QCOMPARE(
        status->text(),
        QStringLiteral("Loading workbook...")
        );
    QVERIFY(!progress->isHidden());
    QVERIFY(!next->isEnabled());
    QVERIFY(!browse->isEnabled());

    QTRY_VERIFY_WITH_TIMEOUT(progress->isHidden(), 5000);
    QCOMPARE(
        status->text(),
        QStringLiteral(
            "The selected workbook could not be opened."
            )
        );
    QVERIFY(next->isEnabled());
    QVERIFY(browse->isEnabled());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString malformedPath =
        directory.filePath(
            QStringLiteral("malformed.xlsx")
            );
    QFile malformedFile(malformedPath);
    QVERIFY(malformedFile.open(QIODevice::WriteOnly));
    QCOMPARE(
        malformedFile.write(
            QByteArrayLiteral("not an xlsx workbook")
            ),
        20
        );
    malformedFile.close();

    dialog.setFilePath(malformedPath);
    next->click();
    QVERIFY(!progress->isHidden());
    QVERIFY(!next->isEnabled());
    QTRY_VERIFY_WITH_TIMEOUT(progress->isHidden(), 5000);
    QVERIFY(
        status->text().startsWith(
            QStringLiteral("Invalid workbook:")
            )
        );
    QVERIFY(next->isEnabled());
    QVERIFY(browse->isEnabled());
}

void ScheduleImportDialogTests::discardsSupersededAndClosedLoads()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path =
        writeDialogWorkbook(&directory);
    QVERIFY(!path.isEmpty());

    ApplicationServices services;
    ScheduleImportDialog dialog(&services);
    dialog.setFilePath(path);
    auto* normal =
        dialog.findChild<QRadioButton*>(
            QStringLiteral("scheduleImportNormalRadio")
            );
    auto* next =
        dialog.findChild<QPushButton*>(
            QStringLiteral("scheduleImportNextButton")
            );
    auto* status =
        dialog.findChild<QLabel*>(
            QStringLiteral("scheduleImportSourceStatus")
            );
    auto* progress =
        dialog.findChild<QProgressBar*>(
            QStringLiteral("scheduleImportProgressBar")
            );
    auto* sheets =
        dialog.findChild<QComboBox*>(
            QStringLiteral("scheduleImportSheetCombo")
            );
    QVERIFY(normal);
    QVERIFY(next);
    QVERIFY(status);
    QVERIFY(progress);
    QVERIFY(sheets);

    normal->setChecked(true);
    next->click();
    QVERIFY(!progress->isHidden());
    dialog.setFilePath(
        QStringLiteral("/replacement/schedule.xlsx")
        );
    QCOMPARE(
        status->text(),
        QStringLiteral(
            "Ready to read the spreadsheet."
            )
        );
    QVERIFY(progress->isHidden());
    QCOMPARE(sheets->count(), 0);
    QTest::qWait(100);
    QCOMPARE(
        status->text(),
        QStringLiteral(
            "Ready to read the spreadsheet."
            )
        );
    QCOMPARE(sheets->count(), 0);

    auto* closingDialog =
        new ScheduleImportDialog(&services);
    closingDialog->setFilePath(path);
    auto* closingNormal =
        closingDialog->findChild<QRadioButton*>(
            QStringLiteral("scheduleImportNormalRadio")
            );
    auto* closingNext =
        closingDialog->findChild<QPushButton*>(
            QStringLiteral("scheduleImportNextButton")
            );
    auto* closingProgress =
        closingDialog->findChild<QProgressBar*>(
            QStringLiteral("scheduleImportProgressBar")
            );
    QVERIFY(closingNormal);
    QVERIFY(closingNext);
    QVERIFY(closingProgress);
    closingNormal->setChecked(true);
    closingNext->click();
    QVERIFY(!closingProgress->isHidden());
    delete closingDialog;
    QTest::qWait(100);
}

void ScheduleImportDialogTests
    ::acceptedReviewCanTearDownSourceDialog()
{
    ScheduleWidgetTestStubs::setMatchImportedClasses(true);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path =
        writeDialogWorkbook(&directory);
    QVERIFY(!path.isEmpty());

    ApplicationServices services;
    saveSettingOrFail(services.settingsService(),
        QStringLiteral("myInfo/name"),
        QStringLiteral("Alice")
        );

    ScheduleImportDialog dialog(&services);
    dialog.setFilePath(path);
    dialog.show();
    QCoreApplication::processEvents();
    QVERIFY(loadSourceSelections(&dialog));

    auto* next =
        dialog.findChild<QPushButton*>(
            QStringLiteral("scheduleImportNextButton")
            );
    QVERIFY(next);
    QVERIFY(next->isEnabled());
    next->click();

    auto* review =
        dialog.findChild<ScheduleImportReviewDialog*>();
    QVERIFY(review);
    review->accept();
    QCOMPARE(dialog.result(), QDialog::Accepted);
}

void ScheduleImportDialogTests
    ::compactFlowAndReviewPresentation()
{
    ScheduleWidgetTestStubs::reset();
    ScheduleWidgetTestStubs::setMatchImportedClasses(true);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path =
        writeDialogWorkbook(&directory);
    QVERIFY(!path.isEmpty());

    ApplicationServices services;
    saveSettingOrFail(services.settingsService(),
        QStringLiteral("myInfo/name"),
        QStringLiteral("Alice")
        );
    ScheduleImportDialog dialog(&services);
    dialog.setFilePath(path);
    dialog.show();
    QCoreApplication::processEvents();
    QCOMPARE(dialog.height(), 520);
    QVERIFY(loadSourceSelections(&dialog));

    auto* sourceStatus =
        dialog.findChild<QLabel*>(
            QStringLiteral("scheduleImportSourceStatus")
            );
    auto* continuationHint =
        dialog.findChild<QLabel*>(
            QStringLiteral("scheduleImportContinuationHint")
            );
    auto* continuationSpacer =
        dialog.findChild<QWidget*>(
            QStringLiteral("scheduleImportContinuationSpacer")
            );
    auto* userSection =
        dialog.findChild<QGroupBox*>(
            QStringLiteral("scheduleImportUserSection")
            );
    auto* worksheetSection =
        dialog.findChild<QGroupBox*>(
            QStringLiteral("scheduleImportWorksheetSection")
            );
    auto* userStatus =
        dialog.findChild<QLabel*>(
            QStringLiteral("scheduleImportUserStatus")
            );

    auto* users =
        dialog.findChild<QComboBox*>(
            QStringLiteral("scheduleImportUserCombo")
            );
    auto* next =
        dialog.findChild<QPushButton*>(
            QStringLiteral("scheduleImportNextButton")
            );
    QVERIFY(users);
    QVERIFY(next);
    QVERIFY(sourceStatus);
    QVERIFY(continuationHint);
    QVERIFY(continuationSpacer);
    QVERIFY(userSection);
    QVERIFY(worksheetSection);
    QVERIFY(userStatus);
    QCOMPARE(next->text(), QStringLiteral("Next"));
    QCOMPARE(
        sourceStatus->text(),
        QStringLiteral("Workbook and worksheet are valid.")
        );
    QCOMPARE(
        continuationHint->text(),
        QStringLiteral("Click Next to continue.")
    );
    QVERIFY(continuationHint->alignment().testFlag(Qt::AlignHCenter));
    auto* sourceLayout =
        qobject_cast<QVBoxLayout*>(dialog.layout());
    QVERIFY(sourceLayout);
    QCOMPARE(
        sourceLayout->indexOf(continuationSpacer),
        sourceLayout->indexOf(userSection) + 1
        );
    QCOMPARE(
        sourceLayout->indexOf(continuationHint),
        sourceLayout->indexOf(continuationSpacer) + 1
        );
    QCOMPARE(continuationSpacer->height(), 8);
    QCOMPARE(
        userSection->title(),
        QStringLiteral("Select the schedule to import")
        );
    QCOMPARE(worksheetSection->title(), QStringLiteral("Worksheet"));
    QVERIFY(!worksheetSection->title().endsWith(QLatin1Char(':')));
    QVERIFY(userStatus->isHidden());
    QCOMPARE(dialog.findChildren<QDialogButtonBox*>().size(), 1);
    QCOMPARE(
        dialog.findChildren<QPushButton*>(
            QStringLiteral("scheduleImportNextButton")
            ).size(),
        1
        );
    QCOMPARE(
        dialog.findChildren<QPushButton*>(
            QStringLiteral("scheduleImportBackButton")
            ).size(),
        0
        );
    QCOMPARE(dialog.height(), 520);
    QCOMPARE(dialog.minimumHeight(), 520);
    QCOMPARE(dialog.maximumHeight(), 520);
    QCOMPARE(dialog.width(), ExpectedSourceDialogWidth);
    QVERIFY(users->height() >= users->sizeHint().height());

    for (int index = 0; index < users->count(); ++index)
    {
        if (users->itemData(index).toInt() >= 0)
        {
            users->setCurrentIndex(index);
            break;
        }
    }
    QVERIFY(next->isEnabled());
    next->click();
    QCoreApplication::processEvents();
    auto* review =
        dialog.findChild<ScheduleImportReviewDialog*>();
    QVERIFY(review);
    QVERIFY(review->isVisible());
    QCOMPARE(dialog.height(), 520);
    QVERIFY(review->height() > 520);
    QVERIFY(review->height() <= 820);
    QVERIFY(review->width() <= 1180);

    auto* reviewHeading =
        review->findChild<QLabel*>(
            QStringLiteral("pageTitle")
            );
    QVERIFY(reviewHeading);
    QCOMPARE(reviewHeading->text(), QStringLiteral("Review & Reconcile"));
    QCOMPARE(
        reviewHeading->font().pointSize(),
        UiConstants::Pages::TitleFontSize
        );
    QVERIFY(reviewHeading->font().bold());

    auto* reviewSubtitle =
        review->findChild<QLabel*>(
            QStringLiteral("pageSubtitle")
            );
    QVERIFY(reviewSubtitle);
    QCOMPARE(
        reviewSubtitle->text(),
        QStringLiteral(
            "Review imported classes and resolve any conflicts before continuing."
            )
        );
    QCOMPARE(
        reviewSubtitle->font().pointSize(),
        UiConstants::Pages::SubtitleFontSize
        );

    auto* splitter =
        review->findChild<QSplitter*>(
            QStringLiteral("scheduleImportReviewSplitter")
            );
    QVERIFY(splitter);
    QCOMPARE(splitter->orientation(), Qt::Horizontal);
    QCOMPARE(splitter->count(), 2);
    QVERIFY(splitter->sizes()[0] > 0);
    QVERIFY(splitter->sizes()[1] > 0);

    auto* tabs =
        review->findChild<QTabWidget*>(
            QStringLiteral("scheduleImportResolutionTabs")
            );
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 2);
    QCOMPARE(tabs->tabText(0), QStringLiteral("Classes"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("Korean Teachers"));
    auto* preview =
        review->findChild<QWidget*>(
            QStringLiteral("scheduleImportPreview")
            );
    auto* previewHeading =
        review->findChild<QLabel*>(
            QStringLiteral("scheduleImportPreviewHeading")
            );
    QVERIFY(preview);
    QVERIFY(previewHeading);
    QCOMPARE(
        previewHeading->text(),
        QStringLiteral("Schedule Preview")
        );
    QVERIFY(
        previewHeading->alignment().testFlag(
            Qt::AlignHCenter
            )
        );
    QCOMPARE(
        preview->geometry().top()
        - previewHeading->geometry().bottom()
        - 1,
        16
        );
    QVERIFY(splitter->widget(0)->isAncestorOf(preview));
    QCOMPARE(splitter->widget(1), tabs);
    QCOMPARE(preview->width(), 540);

    auto* teacherScrollArea =
        review->findChild<QScrollArea*>(
            QStringLiteral("scheduleImportTeacherScrollArea")
            );
    auto* classScrollArea =
        review->findChild<QScrollArea*>(
            QStringLiteral("scheduleImportClassScrollArea")
            );
    QVERIFY(teacherScrollArea);
    QVERIFY(classScrollArea);
    QCOMPARE(tabs->currentWidget(), classScrollArea);
    QCOMPARE(
        teacherScrollArea->horizontalScrollBarPolicy(),
        Qt::ScrollBarAlwaysOff
        );
    QCOMPARE(
        classScrollArea->horizontalScrollBarPolicy(),
        Qt::ScrollBarAlwaysOff
        );

    const auto roomCombos =
        review->findChildren<QComboBox*>(
            QRegularExpression(
                QStringLiteral("^scheduleImportTeacherRoom_")
                )
            );
    QVERIFY(!roomCombos.isEmpty());
    const auto teacherActionCombos =
        review->findChildren<QComboBox*>(
            QRegularExpression(
                QStringLiteral("^scheduleImportTeacherAction_")
                )
            );
    QVERIFY(!teacherActionCombos.isEmpty());
    for (const QComboBox* combo : teacherActionCombos)
    {
        QCOMPARE(combo->minimumContentsLength(), 14);
        QCOMPARE(
            combo->sizeAdjustPolicy(),
            QComboBox::AdjustToMinimumContentsLengthWithIcon
            );
    }
    auto* firstTeacherSource =
        review->findChild<QLabel*>(
            QStringLiteral("scheduleImportTeacherSource_0")
            );
    auto* firstTeacherAction =
        review->findChild<QComboBox*>(
            QStringLiteral("scheduleImportTeacherAction_0")
            );
    auto* firstTeacherRoom =
        review->findChild<QComboBox*>(
            QStringLiteral("scheduleImportTeacherRoom_0")
            );
    QVERIFY(firstTeacherSource);
    QVERIFY(firstTeacherAction);
    QVERIFY(firstTeacherRoom);
    auto* firstTeacherCard =
        review->findChild<QFrame*>(
            QStringLiteral("scheduleImportTeacherCard_0")
            );
    QVERIFY(firstTeacherCard);
    QVERIFY(firstTeacherCard->isAncestorOf(firstTeacherSource));
    QVERIFY(firstTeacherCard->isAncestorOf(firstTeacherAction));
    QVERIFY(firstTeacherCard->isAncestorOf(firstTeacherRoom));

    tabs->setCurrentWidget(teacherScrollArea);
    QCoreApplication::processEvents();
    QCOMPARE(tabs->currentWidget(), teacherScrollArea);

    const int initialPreviewWidth = preview->width();
    QSplitterHandle* splitterHandle = splitter->handle(1);
    QVERIFY(splitterHandle);
    const QPoint handleCenter = splitterHandle->rect().center();
    QTest::mousePress(
        splitterHandle,
        Qt::LeftButton,
        Qt::NoModifier,
        handleCenter
        );
    QTest::mouseMove(
        splitterHandle,
        handleCenter + QPoint(100, 0)
        );
    QTest::mouseRelease(
        splitterHandle,
        Qt::LeftButton,
        Qt::NoModifier,
        handleCenter + QPoint(100, 0)
        );
    QCoreApplication::processEvents();
    QVERIFY(preview->width() > initialPreviewWidth);
    const auto classCards =
        review->findChildren<QFrame*>(
            QRegularExpression(
                QStringLiteral("^scheduleImportClassCard_")
                )
            );
    QCOMPARE(classCards.size(), 3);

    const auto candidateLabels =
        review->findChildren<QLabel*>(
            QRegularExpression(
                QStringLiteral("^scheduleImportClassCandidate_")
                )
            );
    QCOMPARE(candidateLabels.size(), 3);
    QVERIFY(candidateLabels[0]->text().contains(QStringLiteral("E4 Theseus")));
    QVERIFY(candidateLabels[1]->text().contains(QStringLiteral("E4 Hercules")));
    QVERIFY(candidateLabels[2]->text().contains(QStringLiteral("M3 Song's")));
    QVERIFY(candidateLabels[1]->text().contains(QStringLiteral("Tues.")));
    QVERIFY(candidateLabels[1]->text().contains(QStringLiteral("Thurs.")));
    QVERIFY(candidateLabels[1]->text().contains(QStringLiteral("\n(")));

    const auto classActionCombos =
        review->findChildren<QComboBox*>(
            QRegularExpression(
                QStringLiteral("^scheduleImportClassAction_")
                )
            );
    QCOMPARE(classActionCombos.size(), 3);
    for (const QComboBox* combo : classActionCombos)
    {
        QCOMPARE(combo->minimumContentsLength(), 14);
        QCOMPARE(
            combo->sizeAdjustPolicy(),
            QComboBox::AdjustToMinimumContentsLengthWithIcon
            );
    }

    const auto colorButtons =
        review->findChildren<QPushButton*>(
            QRegularExpression(
                QStringLiteral("^scheduleImportClassColor_")
                )
            );
    QCOMPARE(colorButtons.size(), 3);
    for (const QPushButton* colorButton : colorButtons)
    {
        QCOMPARE(colorButton->text(), QString());
        QCOMPARE(
            colorButton->width(),
            UiConstants::ClassInfo::Details::ColorPreviewWidth
            );
        QCOMPARE(
            colorButton->height(),
            UiConstants::ClassInfo::Details::ColorPreviewHeight
            );
        QVERIFY(!colorButton->toolTip().isEmpty());
        QCOMPARE(
            colorButton->accessibleName(),
            colorButton->toolTip()
            );
    }
    auto* firstColorLabel =
        review->findChild<QLabel*>(
            QStringLiteral("scheduleImportClassColorLabel_0")
            );
    auto* firstClassCandidate =
        review->findChild<QLabel*>(
            QStringLiteral("scheduleImportClassCandidate_0")
            );
    auto* firstColorButton =
        review->findChild<QPushButton*>(
            QStringLiteral("scheduleImportClassColor_0")
            );
    auto* firstClassAction =
        review->findChild<QComboBox*>(
            QStringLiteral("scheduleImportClassAction_0")
            );
    QVERIFY(firstColorLabel);
    QVERIFY(firstClassCandidate);
    QVERIFY(firstColorButton);
    QVERIFY(firstClassAction);
    QCOMPARE(firstColorLabel->text(), QStringLiteral("Color"));
    QVERIFY(
        firstColorLabel->alignment().testFlag(Qt::AlignRight)
        );
    QCOMPARE(
        firstClassCandidate->geometry().bottom(),
        firstColorButton->geometry().bottom()
        );
    QVERIFY(
        firstClassCandidate->geometry().right()
        < firstColorLabel->geometry().left()
        );
    QVERIFY(
        firstColorButton->geometry().bottom()
        < firstClassAction->geometry().top()
        );
    QCOMPARE(
        firstColorButton->geometry().right(),
        firstClassAction->geometry().right()
        );

    const QLabel* changedDetailsLabel = nullptr;
    QString changedDetails;
    const auto details =
        review->findChildren<QLabel*>(
            QRegularExpression(
                QStringLiteral("^scheduleImportClassDifferences_")
                )
            );
    for (const QLabel* detail : details)
    {
        if (detail->text().contains(QStringLiteral("<ul")))
        {
            changedDetailsLabel = detail;
            changedDetails = detail->text();
            break;
        }
    }
    QVERIFY(changedDetailsLabel);
    QVERIFY(!changedDetails.isEmpty());
    QVERIFY(!changedDetails.contains(QStringLiteral("Imported Class:")));
    QVERIFY(
        changedDetails.contains(
            QStringLiteral(
                "<b style=\"color:%1\">Changes:</b>"
                )
                .arg(
                    changedDetailsLabel->palette()
                        .color(QPalette::Text)
                        .name(QColor::HexRgb)
                    )
            )
        );
    const int gradePosition =
        changedDetails.indexOf(QStringLiteral("Grade:"));
    const int levelPosition =
        changedDetails.indexOf(QStringLiteral("Level:"));
    const int teacherPosition =
        changedDetails.indexOf(QStringLiteral("Teacher:"));
    const int daysPosition =
        changedDetails.indexOf(QStringLiteral("Days:"));
    const int colorPosition =
        changedDetails.indexOf(QStringLiteral("Color:"));
    QVERIFY(gradePosition >= 0);
    QVERIFY(gradePosition < levelPosition);
    QVERIFY(levelPosition < teacherPosition);
    QVERIFY(teacherPosition < daysPosition);
    QVERIFY(daysPosition < colorPosition);
    QVERIFY2(
        changedDetails.contains(
            QStringLiteral("— → Tues. 5:00pm - 5:55pm")
            ),
        qPrintable(changedDetails)
        );
    QVERIFY(
        changedDetails.contains(
            QStringLiteral("<br>Thurs. 5:00pm - 5:50pm → Thurs. 5:00pm - 5:55pm")
            )
        );
    QVERIFY(
        changedDetails.contains(
            review->palette()
                .color(QPalette::Link)
                .name(QColor::HexRgb)
            )
        );

    const auto combos =
        review->findChildren<QComboBox*>();
    QVERIFY(!combos.isEmpty());
    for (QComboBox* combo : combos)
    {
        QVERIFY(qobject_cast<NoWheelComboBox*>(combo));
    }
    QComboBox* wheelCombo = nullptr;
    for (QComboBox* combo : combos)
    {
        if (
            combo->objectName().startsWith(
                QStringLiteral("scheduleImportClassAction_")
                )
            && combo->count() > 1
            )
        {
            wheelCombo = combo;
            break;
        }
    }
    QVERIFY(wheelCombo);
    const int originalIndex =
        wheelCombo->currentIndex();
    QWheelEvent wheelEvent(
        QPointF(4, 4),
        QPointF(4, 4),
        QPoint(),
        QPoint(0, 120),
        Qt::NoButton,
        Qt::NoModifier,
        Qt::NoScrollPhase,
        false
        );
    QApplication::sendEvent(wheelCombo, &wheelEvent);
    QCOMPARE(wheelCombo->currentIndex(), originalIndex);

    auto* back =
        review->findChild<QPushButton*>(
            QStringLiteral("scheduleImportBackButton")
            );
    QVERIFY(back);
    back->click();
    QCoreApplication::processEvents();
    QCOMPARE(dialog.height(), 520);
    QTRY_VERIFY(next->isEnabled());
}

void ScheduleImportDialogTests
    ::reviewWarningsUseDedicatedTab()
{
    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");
    request.user.diagnostics.append(
        {
            QStringLiteral("Schedule"),
            QStringLiteral("Alice"),
            QStringLiteral("B12"),
            QStringLiteral("Unexpected value"),
            QStringLiteral("Unrecognized cell")
        }
        );

    ScheduleImportReviewDialog review(
        &services,
        request
        );
    QVERIFY(review.prepare());
    review.show();
    QCoreApplication::processEvents();

    auto* tabs =
        review.findChild<QTabWidget*>(
            QStringLiteral("scheduleImportResolutionTabs")
            );
    auto* warningScrollArea =
        review.findChild<QScrollArea*>(
            QStringLiteral("scheduleImportWarningScrollArea")
            );
    auto* acknowledgement =
        review.findChild<QCheckBox*>(
            QStringLiteral("scheduleImportWarningAcknowledgement")
            );
    QVERIFY(tabs);
    QVERIFY(warningScrollArea);
    QVERIFY(acknowledgement);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->tabText(0), QStringLiteral("Classes"));
    QCOMPARE(tabs->tabText(2), QStringLiteral("Unrecognized cells"));
    QCOMPARE(
        tabs->currentWidget(),
        review.findChild<QScrollArea*>(
            QStringLiteral("scheduleImportClassScrollArea")
            )
        );
    QCOMPARE(
        warningScrollArea->horizontalScrollBarPolicy(),
        Qt::ScrollBarAlwaysOff
        );
    QVERIFY(warningScrollArea->isAncestorOf(acknowledgement));
}

void ScheduleImportDialogTests
    ::intensiveModeChoiceReflectsExistingSchedule()
{
    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Intensive;
    request.user.name = QStringLiteral("Alice");

    {
        ScheduleImportReviewDialog review(
            &services,
            request
            );
        QVERIFY(review.prepare());
        auto* section =
            review.findChild<QGroupBox*>(
                QStringLiteral("scheduleImportIntensiveModeSection")
                );
        QVERIFY(section);
        QVERIFY(section->isHidden());
    }

    ScheduleWidgetTestStubs::setExistingIntensiveHours(true);
    ScheduleImportReviewDialog review(
        &services,
        request
        );
    QVERIFY(review.prepare());
    review.show();
    QCoreApplication::processEvents();

    auto* section =
        review.findChild<QGroupBox*>(
            QStringLiteral("scheduleImportIntensiveModeSection")
            );
    auto* update =
        review.findChild<QRadioButton*>(
            QStringLiteral("scheduleImportUpdateIntensiveRadio")
            );
    auto* replace =
        review.findChild<QRadioButton*>(
            QStringLiteral("scheduleImportReplaceIntensiveRadio")
            );
    auto* summary =
        review.findChild<QLabel*>(
            QStringLiteral("scheduleImportReviewSummary")
            );
    QVERIFY(section);
    QVERIFY(update);
    QVERIFY(replace);
    QVERIFY(summary);
    QVERIFY(section->isVisible());
    QVERIFY(update->isChecked());
    QVERIFY(
        summary->text().contains(
            QStringLiteral("will be retained")
            )
        );

    replace->setChecked(true);
    QCoreApplication::processEvents();
    QVERIFY(replace->isChecked());
    QVERIFY(
        summary->text().contains(
            QStringLiteral("brand-new intensive schedule")
            )
        );

    request.kind = ScheduleImportKind::Normal;
    ScheduleImportReviewDialog regularReview(
        &services,
        request
        );
    QVERIFY(regularReview.prepare());
    auto* regularSection =
        regularReview.findChild<QGroupBox*>(
            QStringLiteral("scheduleImportIntensiveModeSection")
            );
    QVERIFY(regularSection);
    QVERIFY(regularSection->isHidden());
}

void ScheduleImportDialogTests::regularPreviewShowsFullEssayGrid()
{
    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");

    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("김선생");
    candidate.teacherKr = QStringLiteral("김선생");
    candidate.rooms = {QStringLiteral("413")};
    candidate.classGrade = QStringLiteral("E6");
    candidate.classLevel = QStringLiteral("Hera");
    candidate.times = {
        {
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:55 PM")
        }
    };
    request.user.classes = {candidate};

    ScheduleImportReviewDialog review(
        &services,
        request
        );
    QVERIFY(review.prepare());
    review.show();
    QCoreApplication::processEvents();

    auto* table =
        review.findChild<QTableWidget*>(
            QStringLiteral("scheduleTable")
            );
    QVERIFY(table);
    QCOMPARE(table->rowCount(), 6);
    QVERIFY(table->item(0, 0));
    QVERIFY(table->item(5, 0));
    QCOMPARE(
        table->item(0, 0)->text(),
        QStringLiteral("4:00 -\n4:55 PM")
        );
    QCOMPARE(
        table->item(5, 0)->text(),
        QStringLiteral("9:00 -\n9:55 PM")
        );

    for (int row = 0; row < table->rowCount(); ++row)
    {
        for (int column = 1; column < table->columnCount(); ++column)
        {
            auto* cell =
                qobject_cast<QLabel*>(
                    table->cellWidget(row, column)
                    );
            QVERIFY(cell);
            if (row == 0 && column == 1)
            {
                QVERIFY(cell->text() != QStringLiteral("Essay"));
            }
            else
            {
                QCOMPARE(cell->text(), QStringLiteral("Essay"));
            }
        }
    }
}

void ScheduleImportDialogTests::possibleMatchIsPreselectedForUpdate()
{
    ScheduleWidgetTestStubs::setIncludeAdditionalClass(true);
    ScheduleWidgetTestStubs::setPossibleImportedClasses(true);

    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");

    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("김선생");
    candidate.teacherKr = QStringLiteral("김선생");
    candidate.rooms = {QStringLiteral("413")};
    candidate.classGrade = QStringLiteral("E4");
    candidate.classLevel = QStringLiteral("Hercules");
    candidate.times = {
        {
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:55 PM")
        }
    };
    request.user.classes = {candidate};

    ScheduleImportReviewDialog review(
        &services,
        request
        );
    QVERIFY(review.prepare());

    auto* action =
        review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportClassAction_0")
            );
    QVERIFY(action);
    QCOMPARE(
        action->currentData(Qt::UserRole).toInt(),
        static_cast<int>(ScheduleImportClassAction::UpdateExisting)
        );
    QCOMPARE(action->currentData(Qt::UserRole + 1).toInt(), 43);
}

void ScheduleImportDialogTests::reviewWarnsForDuplicateClassTargets()
{
    ScheduleWidgetTestStubs::setIncludeAdditionalClass(true);
    ScheduleWidgetTestStubs::setIncludeAlternativeMatchingClass(true);
    ScheduleWidgetTestStubs::setPossibleImportedClasses(true);

    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");

    const auto candidate =
        [](const QString& start, const QString& end)
        {
            ScheduleImportClassCandidate result;
            result.teacherKey = QStringLiteral("김선생");
            result.teacherKr = QStringLiteral("김선생");
            result.rooms = {QStringLiteral("413")};
            result.classGrade = QStringLiteral("E4");
            result.classLevel = QStringLiteral("Hercules");
            result.times = {
                {QStringLiteral("Monday"), start, end}
            };
            return result;
        };
    request.user.classes = {
        candidate(
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:55 PM")
            ),
        candidate(
            QStringLiteral("5:00 PM"),
            QStringLiteral("5:55 PM")
            )
    };

    ScheduleImportReviewDialog review(
        &services,
        request
        );
    QVERIFY(review.prepare());
    review.show();

    auto* import =
        review.findChild<QPushButton*>(
            QStringLiteral("scheduleImportAcceptButton")
            );
    QVERIFY(import);

    QTRY_VERIFY(
        review.findChild<QMessageBox*>(
            QStringLiteral("scheduleImportConflictWarning")
            )
        );
    auto* warning =
        review.findChild<QMessageBox*>(
            QStringLiteral("scheduleImportConflictWarning")
            );
    QVERIFY(warning);
    QVERIFY(warning->isModal());
    QVERIFY(
        warning->text().contains(
            QStringLiteral("Multiple imported classes are assigned")
            )
        );
    QVERIFY(warning->text().contains(QStringLiteral("E5 Athena")));
    QVERIFY(!import->isEnabled());
    warning->accept();
    QTRY_VERIFY(
        !review.findChild<QMessageBox*>(
            QStringLiteral("scheduleImportConflictWarning")
            )
        );
    QTest::qWait(50);
    QVERIFY(
        !review.findChild<QMessageBox*>(
            QStringLiteral("scheduleImportConflictWarning")
            )
        );

    auto* secondAction =
        review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportClassAction_1")
            );
    QVERIFY(secondAction);
    const int alternateTargetIndex =
        actionIndex(
            secondAction,
            ScheduleImportClassAction::UpdateExisting,
            44
            );
    QVERIFY(alternateTargetIndex >= 0);
    secondAction->setCurrentIndex(alternateTargetIndex);
    QTRY_VERIFY(import->isEnabled());

    const int duplicateTargetIndex =
        actionIndex(
            secondAction,
            ScheduleImportClassAction::UpdateExisting,
            43
            );
    QVERIFY(duplicateTargetIndex >= 0);
    secondAction->setCurrentIndex(duplicateTargetIndex);
    QTRY_VERIFY(
        review.findChild<QMessageBox*>(
            QStringLiteral("scheduleImportConflictWarning")
            )
        );
    warning =
        review.findChild<QMessageBox*>(
            QStringLiteral("scheduleImportConflictWarning")
            );
    QVERIFY(warning);
    warning->accept();
}

void ScheduleImportDialogTests::reviewWarnsForOverlappingProjectedTimes()
{
    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");

    const auto candidate =
        [](const QString& teacher,
           const QString& level,
           const QString& start,
           const QString& end)
        {
            ScheduleImportClassCandidate result;
            result.teacherKey = teacher;
            result.teacherKr = teacher;
            result.rooms = {QStringLiteral("413")};
            result.classGrade = QStringLiteral("E4");
            result.classLevel = level;
            result.times = {
                {QStringLiteral("Monday"), start, end}
            };
            return result;
        };
    request.user.classes = {
        candidate(
            QStringLiteral("김선생"),
            QStringLiteral("Hercules"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:55 PM")
            ),
        candidate(
            QStringLiteral("이선생"),
            QStringLiteral("Athena"),
            QStringLiteral("4:30 PM"),
            QStringLiteral("5:25 PM")
            )
    };

    ScheduleImportReviewDialog review(
        &services,
        request
        );
    QVERIFY(review.prepare());
    review.show();

    QTRY_VERIFY(
        review.findChild<QMessageBox*>(
            QStringLiteral("scheduleImportConflictWarning")
            )
        );
    auto* warning =
        review.findChild<QMessageBox*>(
            QStringLiteral("scheduleImportConflictWarning")
            );
    QVERIFY(warning);
    QVERIFY(warning->text().contains(QStringLiteral("overlaps")));
    QVERIFY(warning->text().contains(QStringLiteral("Monday")));
    QVERIFY(warning->text().contains(QStringLiteral("4:00pm")));
    QVERIFY(
        !warning->text().contains(
            QStringLiteral("Multiple imported classes are assigned")
            )
        );
    auto* import =
        review.findChild<QPushButton*>(
            QStringLiteral("scheduleImportAcceptButton")
            );
    QVERIFY(import);
    QVERIFY(!import->isEnabled());
    warning->accept();
}

void ScheduleImportDialogTests
    ::reviewWarnsWhenRetainedIntensiveClassOverlaps()
{
    ScheduleWidgetTestStubs::setExistingIntensiveHours(true);

    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Intensive;
    request.user.name = QStringLiteral("Alice");

    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("김선생");
    candidate.teacherKr = QStringLiteral("김선생");
    candidate.rooms = {QStringLiteral("413")};
    candidate.classGrade = QStringLiteral("E4");
    candidate.classLevel = QStringLiteral("Hercules");
    candidate.times = {
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("9:00 AM"),
            QStringLiteral("9:55 AM")
        }
    };
    request.user.classes = {candidate};

    ScheduleImportReviewDialog review(
        &services,
        request
        );
    QVERIFY(review.prepare());
    review.show();

    QTRY_VERIFY(
        review.findChild<QMessageBox*>(
            QStringLiteral("scheduleImportConflictWarning")
            )
        );
    auto* warning =
        review.findChild<QMessageBox*>(
            QStringLiteral("scheduleImportConflictWarning")
            );
    QVERIFY(warning);
    QVERIFY(warning->text().contains(QStringLiteral("overlaps")));
    QVERIFY(warning->text().contains(QStringLiteral("Tuesday")));
    auto* import =
        review.findChild<QPushButton*>(
            QStringLiteral("scheduleImportAcceptButton")
            );
    QVERIFY(import);
    QVERIFY(!import->isEnabled());
    warning->accept();
}

void ScheduleImportDialogTests
    ::reviewPreviewUsesSavedScheduleDisplaySettings()
{
    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");

    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("weekend-teacher");
    candidate.teacherKr = QStringLiteral("주말 선생님");
    candidate.rooms = {QStringLiteral("401")};
    candidate.importedColors = {QStringLiteral("#336699")};
    candidate.classGrade = QStringLiteral("E4");
    candidate.classLevel = QStringLiteral("Weekend");
    candidate.times.append(
        {
            QStringLiteral("Saturday"),
            QStringLiteral("9:00 AM"),
            QStringLiteral("9:55 AM")
        }
        );
    request.user.classes.append(candidate);

    {
        ScheduleImportReviewDialog review(
            &services,
            request
            );
        QVERIFY(review.prepare());
        review.show();
        QCoreApplication::processEvents();

        auto* table =
            review.findChild<QTableWidget*>(
                QStringLiteral("scheduleTable")
                );
        QVERIFY(table);
        QCOMPARE(table->columnCount(), 6);
        QVERIFY(table->horizontalHeaderItem(5));
        QCOMPARE(
            table->horizontalHeaderItem(5)->text(),
            QStringLiteral("Friday")
            );
        QVERIFY(table->item(0, 0));
        QCOMPARE(
            table->item(0, 0)->text(),
            QStringLiteral("9:00 -\n9:55 AM")
            );
    }

    saveSettingOrFail(services.settingsService(),
        QStringLiteral("schedule_use_24h"),
        QStringLiteral("true")
        );
    saveSettingOrFail(services.settingsService(),
        QStringLiteral("schedule_show_weekends"),
        QStringLiteral("true")
        );

    ScheduleImportReviewDialog review(
        &services,
        request
        );
    QVERIFY(review.prepare());
    review.show();
    QCoreApplication::processEvents();

    auto* table =
        review.findChild<QTableWidget*>(
            QStringLiteral("scheduleTable")
            );
    auto* preview =
        review.findChild<QWidget*>(
            QStringLiteral("scheduleImportPreview")
            );
    QVERIFY(table);
    QVERIFY(preview);
    QCOMPARE(table->geometry().top(), 0);
    QVERIFY(preview->height() > table->height());
    auto* tabs =
        review.findChild<QTabWidget*>(
            QStringLiteral("scheduleImportResolutionTabs")
            );
    QVERIFY(tabs);
    const int initialTabsWidth = tabs->width();
    review.resize(
        review.width() + 200,
        review.height()
        );
    QCoreApplication::processEvents();
    QCOMPARE(preview->width(), 540);
    QVERIFY(tabs->width() > initialTabsWidth);
    QCOMPARE(table->columnCount(), 8);
    QVERIFY(table->horizontalHeaderItem(6));
    QVERIFY(table->horizontalHeaderItem(7));
    QCOMPARE(
        table->horizontalHeaderItem(6)->text(),
        QStringLiteral("Saturday")
        );
    QCOMPARE(
        table->horizontalHeaderItem(7)->text(),
        QStringLiteral("Sunday")
        );
    QVERIFY(table->item(0, 0));
    QCOMPARE(
        table->item(0, 0)->text(),
        QStringLiteral("09:00 - 09:55")
        );
    QCOMPARE(
        table->horizontalHeader()->font().pointSize(),
        FontManager::adjustedPointSize(8)
        );
    QCOMPARE(
        table->item(0, 0)->font().pointSize(),
        FontManager::adjustedPointSize(9)
        );
    QCOMPARE(table->columnWidth(0), 84);
    QCOMPARE(table->rowHeight(0), 51);

    auto* scheduledClass =
        qobject_cast<QLabel*>(
            table->cellWidget(0, 6)
            );
    QVERIFY(scheduledClass);
    QVERIFY(
        scheduledClass->text().contains(
            QStringLiteral("font-size:%1pt").arg(
                FontManager::adjustedPointSize(11)
                )
            )
        );
    QVERIFY(
        scheduledClass->text().contains(
            QStringLiteral("font-size:%1pt").arg(
                FontManager::adjustedPointSize(10)
                )
            )
        );
    QVERIFY(
        scheduledClass->styleSheet().contains(
            QStringLiteral("padding:2.4px 3px")
            )
        );
}

void ScheduleImportDialogTests
    ::intensivePreviewPreservesEssayAndLunchBlocks()
{
    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Intensive;
    request.user.name = QStringLiteral("Alice");
    const QStringList days{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };
    for (int hour = 9; hour <= 21; ++hour)
    {
        for (const QString& day : days)
        {
            const QString state =
                day == QStringLiteral("Monday")
                && hour >= 11
                && hour <= 18
                    ? QStringLiteral("essay")
                    : day == QStringLiteral("Tuesday")
                        && hour == 11
                    ? QStringLiteral("lunch")
                    : QStringLiteral("empty");
            request.user.intensiveSlotStates.append(
                {
                    day,
                    QStringLiteral("%1:00").arg(
                        hour,
                        2,
                        10,
                        QLatin1Char('0')
                        ),
                    state
                }
                );
        }
    }

    ScheduleImportReviewDialog review(
        &services,
        request
        );
    QVERIFY(review.prepare());
    review.show();
    QCoreApplication::processEvents();

    auto* table =
        review.findChild<QTableWidget*>(
            QStringLiteral("scheduleTable")
            );
    QVERIFY(table);
    QCOMPARE(table->rowCount(), 8);
    QVERIFY(table->item(0, 0));
    QVERIFY(table->item(7, 0));
    QCOMPARE(
        table->item(0, 0)->text(),
        QStringLiteral("11:00 -\n11:50 AM")
        );
    QCOMPARE(
        table->item(7, 0)->text(),
        QStringLiteral("6:00 -\n6:50 PM")
        );
    QCOMPARE(
        table->verticalScrollBarPolicy(),
        Qt::ScrollBarAsNeeded
        );
    QTRY_VERIFY(table->verticalScrollBar()->maximum() > 0);

    int expectedTableHeight =
        table->horizontalHeader()->height()
        + (2 * table->frameWidth());
    for (int row = 0; row < 6; ++row)
    {
        expectedTableHeight += table->rowHeight(row);
    }
    QCOMPARE(table->height(), expectedTableHeight);

    review.resize(
        review.width(),
        review.height() + 400
        );
    QTRY_VERIFY(table->verticalScrollBar()->maximum() == 0);

    int expandedTableHeight =
        table->horizontalHeader()->height()
        + (2 * table->frameWidth());
    for (int row = 0; row < table->rowCount(); ++row)
    {
        expandedTableHeight += table->rowHeight(row);
    }
    QCOMPARE(table->height(), expandedTableHeight);

    QLabel* essay = nullptr;
    QLabel* lunch = nullptr;
    const auto labels =
        review.findChildren<QLabel*>();
    for (QLabel* label : labels)
    {
        if (
            label->isVisible()
            && label->property("slot_state").toString()
                == QStringLiteral("essay")
            )
        {
            essay = label;
        }
        else if (
            label->isVisible()
            && label->property("slot_state").toString()
                == QStringLiteral("lunch")
            )
        {
            lunch = label;
        }
    }

    QVERIFY(essay);
    QVERIFY(lunch);
    QCOMPARE(essay->text(), QStringLiteral("Essay"));
    QCOMPARE(lunch->text(), QStringLiteral("Lunch"));
    QCOMPARE(
        essay->font().pointSize(),
        FontManager::adjustedPointSize(12)
        );
    QCOMPARE(
        lunch->font().pointSize(),
        FontManager::adjustedPointSize(12)
        );
    QVERIFY(
        essay->styleSheet().contains(
            QStringLiteral("padding:3.2px 4px")
            )
        );
    QVERIFY(
        lunch->styleSheet().contains(
            QStringLiteral("padding:3.2px 4px")
            )
        );
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
    saveSettingOrFail(services.settingsService(),
        QStringLiteral("myInfo/name"),
        QString()
        );
    ScheduleImportDialog dialog(&services);
    dialog.setFilePath(path);
    auto* next =
        dialog.findChild<QPushButton*>(
            QStringLiteral("scheduleImportNextButton")
            );
    QVERIFY(next);
    QVERIFY(loadSourceSelections(&dialog));

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
    QCoreApplication::processEvents();
    auto* review =
        dialog.findChild<ScheduleImportReviewDialog*>();
    QVERIFY(review);

    const auto roomChoices =
        review->findChildren<QComboBox*>(
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
            review->findChild<QCheckBox*>(
                QStringLiteral(
                    "scheduleImportWarningAcknowledgement"
                    )
                )
        )
    {
        acknowledgement->setChecked(true);
    }

    auto* summary =
        review->findChild<QLabel*>(
            QStringLiteral("scheduleImportReviewSummary")
            );
    auto* reviewStatus =
        review->findChild<QLabel*>(
            QStringLiteral("scheduleImportReviewStatus")
            );
    auto* import =
        review->findChild<QPushButton*>(
            QStringLiteral("scheduleImportAcceptButton")
            );
    const auto colorButtons =
        review->findChildren<QPushButton*>(
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
        QCOMPARE(colorButton->text(), QString());
        QVERIFY(
            colorButton->toolTip().contains(
                QLatin1Char('#')
                )
            );
        foundSpreadsheetColor =
            foundSpreadsheetColor
            || !colorButton->toolTip().contains(
                QStringLiteral("#FFFFFF")
                );
    }
    QVERIFY(foundSpreadsheetColor);
    bool foundInformativeClassTarget = false;
    const auto classActions =
        review->findChildren<QComboBox*>(
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
                        "E4 Hercules (김선생 Tues. 4pm) [Reg]"
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
        review->findChildren<QLabel*>(
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

int main(int argc, char** argv)
{
    qputenv(
        "QT_QPA_PLATFORM",
        QByteArrayLiteral("offscreen")
        );
    QApplication app(argc, argv);
    ScheduleImportDialogTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "schedule_import_dialog_tests.moc"
