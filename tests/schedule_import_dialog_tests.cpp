#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/utils/colorutils.h"
#include "data/data_service.h"
#include "features/schedule/ui/schedule_import_dialog.h"
#include "features/schedule/ui/schedule_import_review_dialog.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/utils/widget_sizing.h"
#include "ui/shared/widgets/no_wheel_combobox.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPalette>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtTest>

#include <algorithm>

#include <zlib.h>

namespace ScheduleWidgetTestStubs
{
void reset();
void setMatchImportedClasses(bool match);
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
    void mismatchedProfileRequiresConfirmation();
    void compactFlowAndReviewPresentation();
    void reviewPreviewUsesSavedScheduleDisplaySettings();
    void intensivePreviewPreservesEssayAndLunchBlocks();
    void suppliedWorkbookBuildsStagedReview();
};

namespace
{
constexpr int ExpectedSourceDialogWidth = 436;

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
    QCOMPARE(status->parentWidget()->layout()->spacing(), 12);
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
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path =
        writeDialogWorkbook(&directory);
    QVERIFY(!path.isEmpty());

    ApplicationServices services;
    services.dataService()->saveSetting(
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
    services.dataService()->saveSetting(
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
    QVERIFY(review->width() <= 1000);

    auto* reviewHeading =
        review->findChild<QLabel*>(
            QStringLiteral("scheduleImportReviewHeading")
            );
    QVERIFY(reviewHeading);
    QCOMPARE(reviewHeading->text(), QStringLiteral("Review & Reconcile"));

    auto* teacherSourceHeader =
        review->findChild<QLabel*>(
            QStringLiteral("scheduleImportTeacherSourceHeader")
            );
    auto* teacherActionHeader =
        review->findChild<QLabel*>(
            QStringLiteral("scheduleImportTeacherActionHeader")
            );
    auto* teacherRoomHeader =
        review->findChild<QLabel*>(
            QStringLiteral("scheduleImportTeacherRoomHeader")
            );
    QVERIFY(teacherSourceHeader);
    QVERIFY(teacherActionHeader);
    QVERIFY(teacherRoomHeader);
    auto* teacherGroup =
        review->findChild<QGroupBox*>(
            QStringLiteral("scheduleImportTeacherGroup")
            );
    QVERIFY(teacherGroup);
    QCOMPARE(
        teacherGroup->title(),
        QStringLiteral("Korean Teachers and Rooms")
        );
    QCOMPARE(
        teacherSourceHeader->text(),
        QStringLiteral("Korean Teacher")
        );
    QCOMPARE(
        teacherActionHeader->text(),
        QStringLiteral("Import Action")
        );
    QCOMPARE(
        teacherRoomHeader->text(),
        QStringLiteral("Imported Room")
        );
    const auto roomCombos =
        review->findChildren<QComboBox*>(
            QRegularExpression(
                QStringLiteral("^scheduleImportTeacherRoom_")
                )
            );
    QVERIFY(!roomCombos.isEmpty());
    for (const QComboBox* roomCombo : roomCombos)
    {
        QCOMPARE(
            roomCombo->width(),
            teacherRoomHeader->sizeHint().width()
            );
    }
    const auto teacherActionCombos =
        review->findChildren<QComboBox*>(
            QRegularExpression(
                QStringLiteral("^scheduleImportTeacherAction_")
                )
            );
    QVERIFY(!teacherActionCombos.isEmpty());
    int expectedTeacherActionWidth = 0;
    for (const QComboBox* combo : teacherActionCombos)
    {
        QStringList texts;
        for (int index = 0; index < combo->count(); ++index)
        {
            texts.append(combo->itemText(index));
        }
        expectedTeacherActionWidth =
            std::max(
                expectedTeacherActionWidth,
                WidgetSizing::comboMinimumWidthForTexts(
                    combo,
                    texts
                    )
                );
    }
    for (const QComboBox* combo : teacherActionCombos)
    {
        QCOMPARE(
            combo->width(),
            expectedTeacherActionWidth
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
    QCOMPARE(
        firstTeacherAction->geometry().left()
            - firstTeacherSource->geometry().right()
            - 1,
        24
        );
    QCOMPARE(
        firstTeacherRoom->geometry().left()
            - firstTeacherAction->geometry().right()
            - 1,
        24
        );

    auto* importedClassHeader =
        review->findChild<QLabel*>(
            QStringLiteral("scheduleImportClassSourceHeader")
            );
    auto* classActionHeader =
        review->findChild<QLabel*>(
            QStringLiteral("scheduleImportClassActionHeader")
            );
    auto* classColorHeader =
        review->findChild<QLabel*>(
            QStringLiteral("scheduleImportClassColorHeader")
            );
    QVERIFY(importedClassHeader);
    QVERIFY(classActionHeader);
    QVERIFY(classColorHeader);
    QCOMPARE(
        importedClassHeader->text(),
        QStringLiteral("Imported Class")
        );
    QCOMPARE(
        classActionHeader->text(),
        QStringLiteral("Import Action")
        );
    QCOMPARE(
        classColorHeader->text(),
        QStringLiteral("Color")
        );

    auto* classesGroup =
        review->findChild<QGroupBox*>(
            QStringLiteral("scheduleImportClassesGroup")
            );
    QVERIFY(classesGroup);
    auto* classGrid =
        qobject_cast<QGridLayout*>(
            classesGroup->layout()
            );
    QVERIFY(classGrid);
    QCOMPARE(classGrid->horizontalSpacing(), 12);
    QCOMPARE(classGrid->verticalSpacing(), 0);
    QCOMPARE(classGrid->rowMinimumHeight(1), 8);

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
    for (const QLabel* candidate : candidateLabels)
    {
        QCOMPARE(candidate->minimumWidth(), candidate->maximumWidth());
    }

    int row = -1;
    int column = -1;
    int rowSpan = -1;
    int columnSpan = -1;
    classGrid->getItemPosition(
        classGrid->indexOf(candidateLabels.first()),
        &row,
        &column,
        &rowSpan,
        &columnSpan
        );
    QCOMPARE(row, 2);
    QCOMPARE(column, 0);
    QCOMPARE(rowSpan, 3);
    QCOMPARE(columnSpan, 1);
    QVERIFY(
        classGrid->itemAt(
            classGrid->indexOf(candidateLabels.first())
            )->alignment().testFlag(Qt::AlignTop)
        );
    QCOMPARE(classGrid->rowMinimumHeight(3), 4);
    QCOMPARE(classGrid->rowMinimumHeight(5), 8);

    const auto classActionCombos =
        review->findChildren<QComboBox*>(
            QRegularExpression(
                QStringLiteral("^scheduleImportClassAction_")
                )
            );
    QCOMPARE(classActionCombos.size(), 3);
    int expectedClassActionWidth = 0;
    for (const QComboBox* combo : classActionCombos)
    {
        QStringList texts;
        for (int index = 0; index < combo->count(); ++index)
        {
            texts.append(combo->itemText(index));
        }
        expectedClassActionWidth =
            std::max(
                expectedClassActionWidth,
                WidgetSizing::comboMinimumWidthForTexts(
                    combo,
                    texts
                    )
                );
    }
    for (const QComboBox* combo : classActionCombos)
    {
        QCOMPARE(
            combo->width(),
            expectedClassActionWidth
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
            changedDetails = detail->text();
            break;
        }
    }
    QVERIFY(!changedDetails.isEmpty());
    QVERIFY(!changedDetails.contains(QStringLiteral("Imported Class:")));
    QVERIFY(
        changedDetails.contains(
            QStringLiteral(
                "<b style=\"color:white\">Changes:</b>"
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

    auto* scrollArea =
        review->findChild<QScrollArea*>(
            QStringLiteral("scheduleImportResolutionScrollArea")
            );
    QVERIFY(scrollArea);
    const int availableWidth =
        review->screen()
            ? review->screen()->availableGeometry().width()
            : review->width();
    if (
        review->width() < 1000
        && review->width() < availableWidth
        )
    {
        QVERIFY(!scrollArea->horizontalScrollBar()->isVisible());
    }

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

    services.dataService()->saveSetting(
        QStringLiteral("schedule_use_24h"),
        QStringLiteral("true")
        );
    services.dataService()->saveSetting(
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
    QVERIFY(table);
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
    QCOMPARE(table->rowHeight(0), 64);

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
    QVERIFY(scheduledClass->styleSheet().contains(QStringLiteral("padding:3px")));
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
    QVERIFY(essay->styleSheet().contains(QStringLiteral("padding:4px")));
    QVERIFY(lunch->styleSheet().contains(QStringLiteral("padding:4px")));
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

QTEST_MAIN(ScheduleImportDialogTests)

#include "schedule_import_dialog_tests.moc"
