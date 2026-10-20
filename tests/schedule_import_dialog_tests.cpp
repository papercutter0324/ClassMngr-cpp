#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/utils/colorutils.h"
#include "app/services/feature_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "domain/models/teacher.h"
#include "features/schedule/ui/schedule_import_dialog.h"
#include "features/schedule/ui/schedule_import_review_presentation.h"
#include "features/schedule/ui/schedule_import_review_dialog.h"
#include "features/schedule/ui/schedule_widget.h"
#include "features/schedule/ui/schedule_widget_delegates.h"
#include "features/schedule/services/schedule_import_plan_validator.h"
#include "features/schedule/services/schedule_import_review_model.h"
#include "features/schedule/import/schedule_workbook_parser.h"
#include "features/teacher/import/teacher_import_name_utils.h"
#include "next/application/schedule_import_apply_review_decisions.h"
#include "next/application/schedule_import_review_readiness.h"
#include "next/application/schedule_import_review_summary_projection.h"
#include "next/application/schedule_import_schedules_cleared_projection.h"
#include "next/platform/application_services_schedule_import_state_snapshot_port.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/widgets/no_wheel_combobox.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QColor>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QPalette>
#include <QProgressBar>
#include <QPointer>
#include <QRect>
#include <QDir>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSplitter>
#include <QSplitterHandle>
#include <QTableWidget>
#include <QTabWidget>
#include <QTabBar>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtTest>

#include <algorithm>
#include <string>

#include <zlib.h>

namespace ScheduleWidgetTestStubs
{
void reset();
void setIncludeAdditionalClass(bool include);
void setMatchImportedClasses(bool match);
void setPossibleImportedClasses(bool match);
void setMatchingImportedTeacherIds(QList<qint64> teacherIds);
void setExistingIntensiveHours(bool exists);
void setDistinctIntensiveDays(bool distinct);
void setIncludeAlternativeMatchingClass(bool include);
void setClassGrade(int classId, const QString& grade);
void setClassLevel(int classId, const QString& level);
void setClassRoom(int classId, const QString& room);
void setClassRegularHoursEmpty(int classId, bool empty);
void setDuplicateKoreanTeacherNames(bool duplicate);
void setDatabaseSessionOpen(bool open);
void setScheduleClassInfoReadFailure(bool fail);
extern int legacyClassInfoReadCount;
extern int scheduleImportPreviewCallCount;
}

namespace ImportState = ClassMngr::Next::Application;
namespace ImportDomain = ClassMngr::Next::Domain;

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
    void compactMeetingTextParsesAndOrdersDisplayFormats();
    void compactMeetingTextUsesCanonicalWeekdayLabels_data();
    void compactMeetingTextUsesCanonicalWeekdayLabels();
    void classDifferencesNormalizeSpacedTimesAndEscapeRawFallbacks();
    void reviewWarningsUseDedicatedTab();
    void intensiveModeChoiceReflectsExistingSchedule();
    void regularPreviewShowsFullEssayGrid();
    void possibleMatchIsPreselectedForUpdate();
    void resolutionChoicesUseTypedSnapshotAndPreserveOrdering();
    void reviewDisplaysProjectedAffectedClassCount();
    void reviewMatchingUsesNormalizedSnapshotFields();
    void reviewSummaryUsesApplicationProjection();
    void reviewPreviewUsesProjectedRowsInControlOrder();
    void intensivePreviewKeepsProjectedPreservationOrder();
    void resolutionBuilderRetainsAdditionalEligibleTargets();
    void resolutionBuilderBoundsRowsAndRestoresDecisions();
    void reviewPrepareSnapshotFailureDoesNotBuildControlsOrFallback();
    void reviewPrepareRejectsUnknownKindBeforeLoadingSnapshot();
    void reviewPrepareClosedSessionUsesSnapshotWarning();
    void reviewRefreshUsesFreshTypedStateSnapshot();
    void reviewSnapshotFailureStaysInvalidWithoutLegacyFallback();
    void wideTeacherIdSurvivesReviewResolutionAndTypedApplyProjection();
    void ambiguousTargetedClassSkipIsRejected();
    void uniqueExactTargetedClassSkipIsAllowed();
    void targetlessClassSkipIsAllowed();
    void reviewWarnsForDuplicateClassTargets();
    void invalidExistingClassTargetMessageIsClear();
    void reviewWarnsForOverlappingProjectedTimes();
    void reviewRejectsMalformedProjectedTime();
    void reviewWarnsWhenRetainedIntensiveClassOverlaps();
    void checkedInOverlapWorkbookPresentsReviewConflict();
    void reviewPreviewUsesSavedScheduleDisplaySettings();
    void intensivePreviewPreservesEssayAndLunchBlocks();
    void suppliedWorkbookBuildsStagedReview();
    void permanentConflictWorkbookPresentsReviewWarning();
    void applyUsesConfirmationAndReportsRepositoryOutcome();
    void applyDisplaysFreshStateValidationFailure();
    void policyFailureMessageRetainsLegacyText();
    void featureValidatorMatchesQStringTeacherKeyConstruction();
    void reviewModelBuildsTypedApplyRequest();
};

namespace
{
void saveSettingOrFail(
    DataService* dataService,
    const QString& key,
    const QVariant& value
    )
{
    QVERIFY(dataService);
    QVERIFY(dataService->saveSetting(key, value).has_value());
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

int unselectedActionIndex(
    const QComboBox* combo
    )
{
    if (!combo)
    {
        return -1;
    }

    for (int index = 0; index < combo->count(); ++index)
    {
        if (combo->itemData(index, Qt::UserRole).toInt() < 0)
        {
            return index;
        }
    }
    return -1;
}

QList<ScheduleEntry> previewEntriesAt(
    ScheduleImportReviewDialog& review,
    const QString& day,
    const QString& timeLabel
    )
{
    auto* preview = review.findChild<ScheduleWidget*>(
        QStringLiteral("scheduleImportPreview")
        );
    if (!preview)
    {
        return {};
    }

    const ScheduleViewModel model = preview->scheduleModel();
    const int dayIndex = model.days.indexOf(day);
    if (dayIndex < 0)
    {
        return {};
    }
    for (const ScheduleRowView& row : model.rows)
    {
        if (row.timeLabel == timeLabel && dayIndex < row.cells.size())
        {
            return row.cells.at(dayIndex).entries;
        }
    }
    return {};
}

ScheduleImportReviewRequest classSkipRequest(const Teacher& teacher)
{
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");

    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = TeacherImportNameUtils::hangulOnly(
        teacher.teacherKr
        );
    candidate.teacherKr = teacher.teacherKr;
    candidate.rooms = {teacher.roomNumber};
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
    return request;
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

QByteArray scheduleWorkbookPackage(
    const QByteArray& sheet1,
    const QByteArray& sheet2
    )
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
    return storedZip({
        {QByteArrayLiteral("xl/workbook.xml"), workbook},
        {QByteArrayLiteral("xl/_rels/workbook.xml.rels"), relationships},
        {QByteArrayLiteral("xl/styles.xml"), styles},
        {QByteArrayLiteral("xl/worksheets/sheet1.xml"), sheet1},
        {QByteArrayLiteral("xl/worksheets/sheet2.xml"), sheet2}
    });
}

QByteArray dialogWorkbookData()
{
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
              <c r="F2" t="inlineStr"><is><t>박선생 (415)&#10;M3-Song's</t></is></c>
            </row>
            <row r="3">
              <c r="A3" t="inlineStr"><is><t>5:00~5:55</t></is></c>
              <c r="C3" s="1" t="inlineStr"><is><t>최선생 (416)&#10;E4-Hercules</t></is></c>
              <c r="E3" s="1" t="inlineStr"><is><t>최선생 (416)&#10;E4-Hercules</t></is></c>
            </row>
            <row r="4">
              <c r="A4" t="inlineStr"><is><t>6:00~6:55</t></is></c>
              <c r="B4" t="inlineStr"><is><t>김선생 (413)&#10;E4-Theseus</t></is></c>
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

    return scheduleWorkbookPackage(sheet1, sheet2);
}

QByteArray scheduleConflictWorkbookData()
{
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
              <c r="B2" s="1" t="inlineStr"><is><t>ê¹€ì„ ìƒ (413)&#10;E4-Hercules</t></is></c>
              <c r="D2" s="1" t="inlineStr"><is><t>ê¹€ì„ ìƒ (413)&#10;E4-Hercules</t></is></c>
            </row>
            <row r="3">
              <c r="A3" t="inlineStr"><is><t>5:00~5:55</t></is></c>
              <c r="C3" s="1" t="inlineStr"><is><t>ì´ì„ ìƒ (512)&#10;E4-Hercules</t></is></c>
              <c r="E3" s="1" t="inlineStr"><is><t>ì´ì„ ìƒ (512)&#10;E4-Hercules</t></is></c>
            </row>
            <row r="4">
              <c r="A4" t="inlineStr"><is><t>6:00~6:55</t></is></c>
              <c r="B4" t="inlineStr"><is><t>ë°•ì„ ìƒ (415)&#10;M3-Song's</t></is></c>
              <c r="F4" t="inlineStr"><is><t>ë°•ì„ ìƒ (415)&#10;M3-Song's</t></is></c>
            </row>
            <row r="5">
              <c r="A5" t="inlineStr"><is><t>7:00~7:55</t></is></c>
              <c r="C5" s="1" t="inlineStr"><is><t>ìµœì„ ìƒ (416)&#10;E5-Apollo</t></is></c>
              <c r="E5" s="1" t="inlineStr"><is><t>ìµœì„ ìƒ (416)&#10;E5-Apollo</t></is></c>
            </row>
            <row r="6">
              <c r="A6" t="inlineStr"><is><t>8:00~8:55</t></is></c>
              <c r="B6" t="inlineStr"><is><t>ê¹€ì„ ìƒ (413)&#10;E4-Athena</t></is></c>
              <c r="F6" t="inlineStr"><is><t>ê¹€ì„ ìƒ (413)&#10;E4-Athena</t></is></c>
            </row>
            <row r="7">
              <c r="A7" t="inlineStr"><is><t>4:00~4:55</t></is></c>
              <c r="C7" t="inlineStr"><is><t>ë°•ì„ ìƒ (415)&#10;M2-Zeus</t></is></c>
              <c r="E7" t="inlineStr"><is><t>ë°•ì„ ìƒ (415)&#10;M2-Zeus</t></is></c>
            </row>
            <row r="8">
              <c r="A8" t="inlineStr"><is><t>5:00~5:55</t></is></c>
              <c r="B8" t="inlineStr"><is><t>ì´ì„ ìƒ (512)&#10;E5-Poseidon</t></is></c>
              <c r="D8" t="inlineStr"><is><t>ì´ì„ ìƒ (512)&#10;E5-Poseidon</t></is></c>
            </row>
            <row r="9">
              <c r="A9" t="inlineStr"><is><t>6:00~6:55</t></is></c>
              <c r="C9" t="inlineStr"><is><t>ìµœì„ ìƒ (416)&#10;M3-Odyssey</t></is></c>
              <c r="E9" t="inlineStr"><is><t>ìµœì„ ìƒ (416)&#10;M3-Odyssey</t></is></c>
            </row>
            <row r="10">
              <c r="A10" t="inlineStr"><is><t>7:00~7:55</t></is></c>
              <c r="B10" t="inlineStr"><is><t>ê¹€ì„ ìƒ (413)&#10;E4-Theseus</t></is></c>
              <c r="F10" t="inlineStr"><is><t>ê¹€ì„ ìƒ (413)&#10;E4-Theseus</t></is></c>
            </row>
            <row r="11">
              <c r="A11" t="inlineStr"><is><t>8:00~8:55</t></is></c>
              <c r="C11" t="inlineStr"><is><t>ë°•ì„ ìƒ (415)&#10;E5-Hera</t></is></c>
              <c r="E11" t="inlineStr"><is><t>ë°•ì„ ìƒ (415)&#10;E5-Hera</t></is></c>
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
              <c r="B2" t="inlineStr"><is><t>ì´ì„ ìƒ (512)&#10;E5-Athena</t></is></c>
            </row>
          </sheetData>
        </worksheet>)");

    return scheduleWorkbookPackage(sheet1, sheet2);
}

QByteArray scheduleConflictWorkbookDataFixed()
{
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
              <c r="B2" s="1" t="inlineStr"><is><t>&#xAE40;&#xC120;&#xC0DD; (413)&#10;E4-Hercules</t></is></c>
              <c r="D2" s="1" t="inlineStr"><is><t>&#xAE40;&#xC120;&#xC0DD; (413)&#10;E4-Hercules</t></is></c>
            </row>
            <row r="3">
              <c r="A3" t="inlineStr"><is><t>5:00~5:55</t></is></c>
              <c r="C3" s="1" t="inlineStr"><is><t>&#xC774;&#xC120;&#xC0DD; (512)&#10;E4-Hercules</t></is></c>
              <c r="E3" s="1" t="inlineStr"><is><t>&#xC774;&#xC120;&#xC0DD; (512)&#10;E4-Hercules</t></is></c>
            </row>
            <row r="4">
              <c r="A4" t="inlineStr"><is><t>6:00~6:55</t></is></c>
              <c r="B4" t="inlineStr"><is><t>&#xBC15;&#xC120;&#xC0DD; (415)&#10;M3-Song's</t></is></c>
              <c r="F4" t="inlineStr"><is><t>&#xBC15;&#xC120;&#xC0DD; (415)&#10;M3-Song's</t></is></c>
            </row>
            <row r="5">
              <c r="A5" t="inlineStr"><is><t>7:00~7:55</t></is></c>
              <c r="C5" s="1" t="inlineStr"><is><t>&#xCD5C;&#xC120;&#xC0DD; (416)&#10;E5-Apollo</t></is></c>
              <c r="E5" s="1" t="inlineStr"><is><t>&#xCD5C;&#xC120;&#xC0DD; (416)&#10;E5-Apollo</t></is></c>
            </row>
            <row r="6">
              <c r="A6" t="inlineStr"><is><t>8:00~8:55</t></is></c>
              <c r="B6" t="inlineStr"><is><t>&#xAE40;&#xC120;&#xC0DD; (413)&#10;E4-Athena</t></is></c>
              <c r="F6" t="inlineStr"><is><t>&#xAE40;&#xC120;&#xC0DD; (413)&#10;E4-Athena</t></is></c>
            </row>
            <row r="7">
              <c r="A7" t="inlineStr"><is><t>4:00~4:55</t></is></c>
              <c r="C7" t="inlineStr"><is><t>&#xBC15;&#xC120;&#xC0DD; (415)&#10;M2-Zeus</t></is></c>
              <c r="E7" t="inlineStr"><is><t>&#xBC15;&#xC120;&#xC0DD; (415)&#10;M2-Zeus</t></is></c>
            </row>
            <row r="8">
              <c r="A8" t="inlineStr"><is><t>5:00~5:55</t></is></c>
              <c r="B8" t="inlineStr"><is><t>&#xC774;&#xC120;&#xC0DD; (512)&#10;E5-Poseidon</t></is></c>
              <c r="D8" t="inlineStr"><is><t>&#xC774;&#xC120;&#xC0DD; (512)&#10;E5-Poseidon</t></is></c>
            </row>
            <row r="9">
              <c r="A9" t="inlineStr"><is><t>6:00~6:55</t></is></c>
              <c r="C9" t="inlineStr"><is><t>&#xCD5C;&#xC120;&#xC0DD; (416)&#10;M3-Odyssey</t></is></c>
              <c r="E9" t="inlineStr"><is><t>&#xCD5C;&#xC120;&#xC0DD; (416)&#10;M3-Odyssey</t></is></c>
            </row>
            <row r="10">
              <c r="A10" t="inlineStr"><is><t>7:00~7:55</t></is></c>
              <c r="B10" t="inlineStr"><is><t>&#xAE40;&#xC120;&#xC0DD; (413)&#10;E4-Theseus</t></is></c>
              <c r="F10" t="inlineStr"><is><t>&#xAE40;&#xC120;&#xC0DD; (413)&#10;E4-Theseus</t></is></c>
            </row>
            <row r="11">
              <c r="A11" t="inlineStr"><is><t>8:00~8:55</t></is></c>
              <c r="C11" t="inlineStr"><is><t>&#xBC15;&#xC120;&#xC0DD; (415)&#10;E5-Hera</t></is></c>
              <c r="E11" t="inlineStr"><is><t>&#xBC15;&#xC120;&#xC0DD; (415)&#10;E5-Hera</t></is></c>
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
              <c r="B2" t="inlineStr"><is><t>&#xC774;&#xC120;&#xC0DD; (512)&#10;E5-Athena</t></is></c>
            </row>
          </sheetData>
        </worksheet>)");

    return scheduleWorkbookPackage(sheet1, sheet2);
}

QString writeDialogWorkbook(
    QTemporaryDir* directory
    )
{
    const QString configuredFixturePath =
        qEnvironmentVariable(
            "CLASSMNGR_SCHEDULE_IMPORT_FIXTURE_OUTPUT_PATH"
            ).trimmed();
    const QString path =
        configuredFixturePath.isEmpty()
            ? directory->filePath(QStringLiteral("schedule.xlsx"))
            : configuredFixturePath;
    if (!configuredFixturePath.isEmpty()
        && !QDir().mkpath(QFileInfo(path).absolutePath()))
    {
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return {};
    }
    file.write(dialogWorkbookData());
    file.close();
    return path;
}

QString writeScheduleConflictWorkbook(
    QTemporaryDir* directory
    )
{
    const QString configuredFixturePath =
        qEnvironmentVariable(
            "CLASSMNGR_SCHEDULE_CONFLICT_FIXTURE_OUTPUT_PATH"
            ).trimmed();
    const QString path =
        configuredFixturePath.isEmpty()
            ? directory->filePath(QStringLiteral("schedule-conflict.xlsx"))
            : configuredFixturePath;
    if (!configuredFixturePath.isEmpty()
        && !QDir().mkpath(QFileInfo(path).absolutePath()))
    {
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return {};
    }
    file.write(scheduleConflictWorkbookDataFixed());
    file.close();
    return path;
}

QString loadReviewFontFamily()
{
    QString interFamily;
    const QString sourceDirectory =
        QStringLiteral(CLASSMNGR_SOURCE_DIR);
    for (const QString& relativePath : {
             QStringLiteral("resources/assets/fonts/Inter.ttc"),
             QStringLiteral("resources/assets/fonts/PretendardVariable.ttf")
         })
    {
        const int fontId = QFontDatabase::addApplicationFont(
            QDir(sourceDirectory).filePath(relativePath));
        if (fontId < 0)
        {
            continue;
        }

        const QStringList families =
            QFontDatabase::applicationFontFamilies(fontId);
        if (interFamily.isEmpty() && !families.isEmpty())
        {
            interFamily = families.first();
        }
    }
    return interFamily;
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
    auto* sourceStatus =
        dialog->findChild<QLabel*>(
            QStringLiteral("scheduleImportSourceStatus")
            );
    auto* users =
        dialog->findChild<QComboBox*>(
            QStringLiteral("scheduleImportUserCombo")
            );
    if (!normal || !next || !sheets || !progress || !sourceStatus || !users)
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

    constexpr int MaximumWaitAttempts = 500;
    for (int attempt = 0; attempt < MaximumWaitAttempts; ++attempt)
    {
        if (!progress->isHidden())
        {
            QTest::qWait(10);
            continue;
        }

        if (
            sheets->currentIndex() < 0
            || sheets->currentData().toInt() < 0
            )
        {
            int firstValidSheet = -1;
            for (int index = 0; index < sheets->count(); ++index)
            {
                if (sheets->itemData(index).toInt() >= 0)
                {
                    firstValidSheet = index;
                    break;
                }
            }
            if (firstValidSheet >= 0)
            {
                sheets->setCurrentIndex(firstValidSheet);
            }
        }

        if (users->currentIndex() < 0 || users->currentData().toInt() < 0)
        {
            for (int index = 0; index < users->count(); ++index)
            {
                if (users->itemData(index).toInt() >= 0)
                {
                    users->setCurrentIndex(index);
                    break;
                }
            }
        }

        if (
            sourceStatus->text()
                == QStringLiteral("Workbook and worksheet are valid.")
            && sheets->currentIndex() >= 0
            && sheets->currentData().toInt() >= 0
            && users->isVisible()
            && users->currentIndex() >= 0
            && users->currentData().toInt() >= 0
            && next->isEnabled()
            )
        {
            return true;
        }

        QTest::qWait(10);
    }
    return false;
}

class ScopedTestUserPromptService final
{
public:
    explicit ScopedTestUserPromptService(IUserPromptService* service)
    {
        DialogServices::setUserPromptServiceForTesting(service);
    }

    ~ScopedTestUserPromptService()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }
};
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
    saveSettingOrFail(services.dataService(),
        QStringLiteral("myInfo/name"),
        QStringLiteral("A Name That Is Not In The Workbook")
        );
    ScheduleImportDialog dialog(&services);
    dialog.setFilePath(path);
    dialog.show();
    QTRY_COMPARE(dialog.height(), 520);
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
    saveSettingOrFail(services.dataService(),
        QStringLiteral("myInfo/name"),
        QStringLiteral("Alice")
        );
    FakeUserPromptService unexpectedPrompts;
    ScopedTestUserPromptService promptService(&unexpectedPrompts);
    ScheduleImportDialog dialog(&services);
    dialog.setFilePath(path);
    dialog.show();
    QTRY_COMPARE(dialog.height(), 520);
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
    QPointer<ScheduleImportReviewDialog> reviewLifetime(review);
    review->accept();
    QTRY_COMPARE(dialog.result(), QDialog::Accepted);
    QTRY_VERIFY(reviewLifetime.isNull());
    QCOMPARE(unexpectedPrompts.messages.size(), 0);
    QCOMPARE(unexpectedPrompts.asynchronousMessages.size(), 0);
}

void ScheduleImportDialogTests
    ::compactFlowAndReviewPresentation()
{
    ScheduleWidgetTestStubs::reset();
    ScheduleWidgetTestStubs::setIncludeAdditionalClass(true);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path =
        writeDialogWorkbook(&directory);
    QVERIFY(!path.isEmpty());

    ApplicationServices services;
    saveSettingOrFail(services.dataService(),
        QStringLiteral("myInfo/name"),
        QStringLiteral("Alice")
        );
    FakeUserPromptService unexpectedPrompts;
    ScopedTestUserPromptService promptService(&unexpectedPrompts);
    ScheduleImportDialog dialog(&services);
    dialog.setFilePath(path);
    dialog.show();
    QTRY_COMPARE(dialog.height(), 520);
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
    QTRY_VERIFY(
        dialog.findChild<ScheduleImportReviewDialog*>()
        && dialog.findChild<ScheduleImportReviewDialog*>()->isVisible()
        );
    auto* review = dialog.findChild<ScheduleImportReviewDialog*>();
    QVERIFY(review);
    bool initialReviewResizeCompleted = false;
    QTimer::singleShot(
        0,
        review,
        [&initialReviewResizeCompleted]()
        {
            initialReviewResizeCompleted = true;
        }
        );
    QTRY_VERIFY(initialReviewResizeCompleted);
    QCOMPARE(dialog.height(), 520);
    QTRY_VERIFY(review->height() > 520);
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
    QTRY_VERIFY(splitter->sizes()[0] > 0 && splitter->sizes()[1] > 0);

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
    QTRY_COMPARE(preview->width(), 540);
    QTRY_VERIFY(preview->height() > 0);
    QCOMPARE(
        previewHeading->text(),
        QStringLiteral("Schedule Preview")
        );
    QVERIFY(
        previewHeading->alignment().testFlag(
            Qt::AlignHCenter
            )
        );
    QTRY_COMPARE(
        preview->geometry().top()
        - previewHeading->geometry().bottom()
        - 1,
        16
        );
    QVERIFY(splitter->widget(0)->isAncestorOf(preview));
    QCOMPARE(splitter->widget(1), tabs);

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
    const auto widgetState = [review](const QWidget* widget)
    {
        if (!widget)
        {
            return QStringLiteral("<null>");
        }
        const QRect localGeometry = widget->geometry();
        const QRect reviewGeometry(
            widget->mapTo(review, QPoint(0, 0)),
            widget->size()
            );
        const QWidget* parent = widget->parentWidget();
        return QStringLiteral(
            "%1#%2 hidden=%3 visible=%4 local=[%5,%6 %7x%8] "
            "review=[%9,%10 %11x%12] parent=%13#%14"
            )
            .arg(
                QString::fromLatin1(widget->metaObject()->className()),
                widget->objectName(),
                QString::number(widget->isHidden()),
                QString::number(widget->isVisible())
                )
            .arg(localGeometry.x())
            .arg(localGeometry.y())
            .arg(localGeometry.width())
            .arg(localGeometry.height())
            .arg(reviewGeometry.x())
            .arg(reviewGeometry.y())
            .arg(reviewGeometry.width())
            .arg(reviewGeometry.height())
            .arg(
                parent
                    ? QString::fromLatin1(parent->metaObject()->className())
                    : QStringLiteral("<none>"),
                parent ? parent->objectName() : QStringLiteral("<none>")
                );
    };
    const auto layoutState = [](const QLayout* layout)
    {
        if (!layout)
        {
            return QStringLiteral("<none>");
        }
        const QRect geometry = layout->geometry();
        return QStringLiteral("[%1,%2 %3x%4]")
            .arg(geometry.x())
            .arg(geometry.y())
            .arg(geometry.width())
            .arg(geometry.height());
    };
    const QRegularExpression teacherRoomName(
        QStringLiteral("^scheduleImportTeacherRoom_")
        );
    tabs->setCurrentWidget(teacherScrollArea);
    QTRY_VERIFY(
        !review->findChildren<QComboBox*>(teacherRoomName).isEmpty()
        );
    const auto roomCombos =
        review->findChildren<QComboBox*>(
            teacherRoomName
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
    QTRY_VERIFY(firstTeacherCard->isVisible());
    QTRY_VERIFY(firstTeacherCard->width() > 0);
    auto* firstTeacherCardLayout = firstTeacherCard->layout();
    QVERIFY(firstTeacherCardLayout);
    QLayout* firstTeacherActionRow =
        firstTeacherCardLayout->itemAt(1)->layout();
    QLayout* firstTeacherRoomRow =
        firstTeacherCardLayout->itemAt(2)->layout();
    const auto teacherControlSnapshot = [&]()
    {
        QStringList teacherState;
        teacherState
            << QStringLiteral("card=%1").arg(widgetState(firstTeacherCard))
            << QStringLiteral("source=%1").arg(widgetState(firstTeacherSource))
            << QStringLiteral("action=%1").arg(widgetState(firstTeacherAction))
            << QStringLiteral("room=%1").arg(widgetState(firstTeacherRoom))
            << QStringLiteral("layouts(card=%1,action=%2,room=%3)")
                .arg(
                    layoutState(firstTeacherCardLayout),
                    layoutState(firstTeacherActionRow),
                    layoutState(firstTeacherRoomRow)
                    );
        return teacherState.join(QStringLiteral("; "));
    };
    QTRY_VERIFY2(
        firstTeacherSource->isVisible()
        && !firstTeacherSource->isHidden()
        && firstTeacherAction->isVisible()
        && !firstTeacherAction->isHidden()
        && firstTeacherRoom->isVisible()
        && !firstTeacherRoom->isHidden()
        && firstTeacherCardLayout->geometry().width() > 0
        && firstTeacherActionRow->geometry().width() > 0
        && firstTeacherRoomRow->geometry().width() > 0,
        qPrintable(teacherControlSnapshot())
        );
    const QString firstTeacherControlSnapshot = teacherControlSnapshot();

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
    QTRY_VERIFY(preview->width() > initialPreviewWidth);
    const int classTabIndex = tabs->indexOf(classScrollArea);
    QVERIFY(classTabIndex >= 0);
    auto* resolutionTabBar = tabs->tabBar();
    QVERIFY(resolutionTabBar);
    QTest::mouseClick(
        resolutionTabBar,
        Qt::LeftButton,
        Qt::NoModifier,
        resolutionTabBar->tabRect(classTabIndex).center()
        );
    QTRY_COMPARE(tabs->currentWidget(), classScrollArea);
    QTRY_VERIFY(
        classScrollArea->isVisible()
        && classScrollArea->viewport()->width() > 0
        );
    const auto findFirstClassCard = [review]()
    {
        return review->findChild<QFrame*>(
            QStringLiteral("scheduleImportClassCard_0")
            );
    };
    const QRegularExpression classCardName(
        QStringLiteral("^scheduleImportClassCard_")
        );
    QTRY_COMPARE(
        review->findChildren<QFrame*>(classCardName).size(),
        3
        );
    QWidget* classContent = classScrollArea->widget();
    QVERIFY(classContent);
    QCOMPARE(
        classContent->objectName(),
        QStringLiteral("scheduleImportClassesGroup")
        );
    const auto parentChain = [](const QWidget* widget)
    {
        QStringList chain;
        for (const QWidget* current = widget; current;
             current = current->parentWidget())
        {
            chain.prepend(
                QStringLiteral("%1#%2")
                    .arg(
                        QString::fromLatin1(
                            current->metaObject()->className()
                            ),
                        current->objectName()
                        )
                );
        }
        return chain.join(QStringLiteral(" <- "));
    };
    const auto classPageSnapshot = [&]()
    {
        auto* card = findFirstClassCard();
        auto* viewport = classScrollArea->viewport();
        const QWidget* currentPage = tabs->currentWidget();
        const int currentIndex = tabs->currentIndex();
        const QString currentText = currentIndex >= 0
            ? tabs->tabText(currentIndex)
            : QStringLiteral("<none>");
        const QString classText = classTabIndex >= 0
            ? tabs->tabText(classTabIndex)
            : QStringLiteral("<missing>");
        const QRect cardLayoutGeometry = card && card->layout()
            ? card->layout()->geometry()
            : QRect();
        const QString cardLayoutState = card && card->layout()
            ? QStringLiteral("[%1,%2 %3x%4]")
                .arg(cardLayoutGeometry.x())
                .arg(cardLayoutGeometry.y())
                .arg(cardLayoutGeometry.width())
                .arg(cardLayoutGeometry.height())
            : QStringLiteral("<none>");
        const int realizedCardCount =
            review->findChildren<QFrame*>(classCardName).size();
        return QStringLiteral(
            "tabs[index=%1 text=%2 currentIndex=%3 currentText=%4 "
            "currentWidget=%5 directClassPage=%6]; "
            "scrollArea={%7}; viewport={%8}; content={%9}; "
            "card={%10}; cardParents=%11; cardLayout={%12}; "
            "realizedCards=%13"
            )
            .arg(classTabIndex)
            .arg(classText)
            .arg(currentIndex)
            .arg(currentText)
            .arg(currentPage ? currentPage->objectName() : QStringLiteral("<none>"))
            .arg(tabs->widget(classTabIndex) == classScrollArea ? 1 : 0)
            .arg(widgetState(classScrollArea))
            .arg(widgetState(viewport))
            .arg(widgetState(classContent))
            .arg(widgetState(card))
            .arg(parentChain(card))
            .arg(cardLayoutState)
            .arg(realizedCardCount);
    };
    QTRY_VERIFY2(
        findFirstClassCard()
        && findFirstClassCard()->isVisible()
        && findFirstClassCard()->width() > 0,
        qPrintable(classPageSnapshot())
        );
    auto* firstClassCard = findFirstClassCard();
    QVERIFY(firstClassCard);
    QVERIFY(firstClassCard->layout());
    bool firstClassCardLayoutActivated = false;
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
    auto* exactMatchAction = review->findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_1")
        );
    QVERIFY(exactMatchAction);
    QCOMPARE(exactMatchAction->count(), 4);
    QCOMPARE(
        exactMatchAction->itemText(0),
        QStringLiteral("Choose Update, Create, or Skip...")
        );
    QVERIFY(exactMatchAction->itemText(1).startsWith(
        QStringLiteral("Update suggested: E4 Hercules")
        ));
    QVERIFY(exactMatchAction->itemText(1).contains(
        QStringLiteral("\uAE40\uC120\uC0DD")
        ));
    QVERIFY(exactMatchAction->itemText(1).contains(QStringLiteral("[Reg]")));
    QCOMPARE(exactMatchAction->itemData(1, Qt::UserRole + 1).toInt(), 42);
    QCOMPARE(
        exactMatchAction->itemText(2),
        QStringLiteral("Create new class")
        );
    QCOMPARE(
        exactMatchAction->itemText(3),
        QStringLiteral("Skip imported class")
        );
    QCOMPARE(exactMatchAction->currentIndex(), 1);
    QCOMPARE(exactMatchAction->currentData(Qt::UserRole + 1).toInt(), 42);

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
    QVERIFY(firstClassCandidate->parentWidget() == firstClassCard);
    QVERIFY(firstColorLabel->parentWidget() == firstClassCard);
    QVERIFY(firstColorButton->parentWidget() == firstClassCard);
    QVERIFY(firstClassAction->parentWidget() == firstClassCard);
    auto* firstClassDetails =
        review->findChild<QLabel*>(
            QStringLiteral("scheduleImportClassDifferences_0")
            );
    QVERIFY(firstClassDetails);
    QCOMPARE(firstColorLabel->text(), QStringLiteral("Color"));
    QVERIFY(
        firstColorLabel->alignment().testFlag(Qt::AlignRight)
        );
    const auto rectInReview = [review](const QWidget* widget)
    {
        return QRect(
            widget->mapTo(review, QPoint(0, 0)),
            widget->size()
            );
    };
    const auto parentIdentity = [](const QWidget* widget)
    {
        const QWidget* parent = widget->parentWidget();
        return parent
            ? QStringLiteral("%1#%2")
                .arg(
                    QString::fromLatin1(parent->metaObject()->className()),
                    parent->objectName()
                    )
            : QStringLiteral("<none>");
    };
    const auto rectDescription = [](const QRect& rect)
    {
        return QStringLiteral("[%1,%2 %3x%4]")
            .arg(rect.x())
            .arg(rect.y())
            .arg(rect.width())
            .arg(rect.height());
    };
    QLayout* firstClassCardLayout = firstClassCard->layout();
    QVERIFY(firstClassCardLayout);
    QLayout* firstClassHeaderLayout =
        firstClassCardLayout->itemAt(0)->layout();
    QVERIFY(firstClassHeaderLayout);
    QLayout* firstClassColorLayout =
        firstClassHeaderLayout->itemAt(1)->layout();
    QVERIFY(firstClassColorLayout);
    QLayout* firstClassActionRow =
        firstClassCardLayout->itemAt(2)->layout();
    QVERIFY(firstClassActionRow);
    const auto geometrySnapshot = [&]()
    {
        QStringList snapshot;
        snapshot
            << QStringLiteral("viewport={%1}")
                .arg(widgetState(classScrollArea->viewport()))
            << QStringLiteral("content={%1}")
                .arg(widgetState(classContent))
            << QStringLiteral("layoutDirection(review/card/content)=%1/%2/%3")
                .arg(
                    QString::number(
                        static_cast<int>(review->layoutDirection())
                        ),
                    QString::number(
                        static_cast<int>(firstClassCard->layoutDirection())
                        ),
                    QString::number(
                        static_cast<int>(classContent->layoutDirection())
                        )
                    )
            << QStringLiteral(
                "card={%1}; layouts(card=%2 activated=%3 header=%4 "
                "color=%5 actionRow=%6)"
                )
                .arg(
                    widgetState(firstClassCard),
                    layoutState(firstClassCardLayout),
                    QString::number(firstClassCardLayoutActivated),
                    layoutState(firstClassHeaderLayout),
                    layoutState(firstClassColorLayout),
                    layoutState(firstClassActionRow)
                    )
            << QStringLiteral("teacher={%1}")
                .arg(firstTeacherControlSnapshot)
            << QStringLiteral("candidate=%1 (%2)")
                .arg(
                rectDescription(rectInReview(firstClassCandidate)),
                parentIdentity(firstClassCandidate)
                )
            << QStringLiteral("colorLabel=%1 (%2)")
                .arg(
                rectDescription(rectInReview(firstColorLabel)),
                parentIdentity(firstColorLabel)
                )
            << QStringLiteral("colorButton=%1 (%2)")
                .arg(
                rectDescription(rectInReview(firstColorButton)),
                parentIdentity(firstColorButton)
                )
            << QStringLiteral("action=%1 (%2)")
                .arg(
                rectDescription(rectInReview(firstClassAction)),
                parentIdentity(firstClassAction)
                )
            << QStringLiteral(
                "childStates(candidate=%1; colorLabel=%2; colorButton=%3; "
                "action=%4; details=%5)"
                )
                .arg(
                    widgetState(firstClassCandidate),
                    widgetState(firstColorLabel),
                    widgetState(firstColorButton),
                    widgetState(firstClassAction),
                    widgetState(firstClassDetails)
                    );
        return snapshot.join(QStringLiteral("; "));
    };
    QTRY_VERIFY2(
        firstClassCandidate->isVisible()
        && !firstClassCandidate->isHidden()
        && firstColorLabel->isVisible()
        && !firstColorLabel->isHidden()
        && firstColorButton->isVisible()
        && !firstColorButton->isHidden()
        && firstClassAction->isVisible()
        && !firstClassAction->isHidden()
        && firstClassDetails->isVisible()
        && !firstClassDetails->isHidden(),
        qPrintable(geometrySnapshot())
        );
    firstClassCardLayout->invalidate();
    firstClassCardLayoutActivated = firstClassCardLayout->activate();
    QTRY_VERIFY2(
        firstClassCardLayout->geometry().width() > 0
        && firstClassCardLayout->geometry().height() > 0
        && firstClassHeaderLayout->geometry().width() > 0
        && firstClassColorLayout->geometry().width() > 0
        && firstClassActionRow->geometry().width() > 0,
        qPrintable(geometrySnapshot())
        );
    QTRY_VERIFY2(
        rectInReview(firstClassCandidate).width() > 0
            && !rectInReview(firstClassCandidate).intersects(
                rectInReview(firstColorLabel)
                )
            && !rectInReview(firstClassCandidate).intersects(
                rectInReview(firstColorButton)
                ),
        qPrintable(geometrySnapshot())
        );
    QTRY_VERIFY2(
        rectInReview(firstColorLabel).bottom()
            < rectInReview(firstColorButton).top(),
        qPrintable(geometrySnapshot())
        );
    QTRY_VERIFY2(
        rectInReview(firstColorButton).bottom()
            < rectInReview(firstClassAction).top(),
        qPrintable(geometrySnapshot())
        );
    const bool colorColumnCanContainButton =
        firstClassColorLayout->geometry().width()
        >= firstColorButton->width();
    if (colorColumnCanContainButton)
    {
        QTRY_VERIFY2(
            rectInReview(firstColorLabel).right()
                == rectInReview(firstColorButton).right(),
            qPrintable(geometrySnapshot())
            );
    }
    else
    {
        QTRY_VERIFY2(
            firstClassCard->contentsRect().contains(
                firstColorButton->geometry()
                ),
            qPrintable(geometrySnapshot())
            );
        QTRY_VERIFY2(
            !rectInReview(firstClassCandidate).intersects(
                rectInReview(firstColorButton)
                ),
            qPrintable(geometrySnapshot())
            );
    }

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
    QVERIFY(changedDetails.contains(QStringLiteral("Days:")));
    QVERIFY(!changedDetails.contains(QStringLiteral("Grade:")));
    QVERIFY(!changedDetails.contains(QStringLiteral("Level:")));
    QVERIFY(changedDetails.contains(QStringLiteral("Teacher:")));
    QVERIFY2(
        changedDetails.contains(
            QStringLiteral("Tues. 4:00pm - 4:50pm")
            ),
        qPrintable(changedDetails)
        );
    QVERIFY(
        changedDetails.contains(
            QStringLiteral("Tues. 5:00pm - 5:55pm")
            )
        );
    QVERIFY2(
        changedDetails.contains(
            QStringLiteral("Thurs. 5:00pm - 5:55pm")
            ),
        qPrintable(changedDetails)
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
    QTRY_COMPARE(dialog.height(), 520);
    QTRY_VERIFY(next->isEnabled());
    QCOMPARE(unexpectedPrompts.messages.size(), 0);
    QCOMPARE(unexpectedPrompts.asynchronousMessages.size(), 0);
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void ScheduleImportDialogTests
    ::compactMeetingTextParsesAndOrdersDisplayFormats()
{
    const QList<ClassTime> times{
        {
            QStringLiteral("Monday"),
            QStringLiteral("4:20 PM"),
            QStringLiteral("5:15 PM")
        },
        {
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("5:00 PM")
        },
        {
            QStringLiteral("Monday"),
            QStringLiteral("4:07 PM"),
            QStringLiteral("5:07 PM")
        },
        {
            QStringLiteral("Monday"),
            QStringLiteral("4:15PM"),
            QStringLiteral("5:10PM")
        },
        {
            QStringLiteral("Monday"),
            QStringLiteral("9:45"),
            QStringLiteral("10:40")
        },
        {
            QStringLiteral("Monday"),
            QStringLiteral("09:50"),
            QStringLiteral("10:45")
        },
        {
            QStringLiteral("Monday"),
            QStringLiteral(" 9:40 "),
            QStringLiteral("10:35")
        },
        {
            QStringLiteral("Monday"),
            QStringLiteral("not-a-time"),
            QStringLiteral("5:15 PM")
        },
        {
            QStringLiteral("Tuesday"),
            QStringLiteral(" 4:15 PM "),
            QStringLiteral(" 5:15 PM ")
        },
        {
            QStringLiteral(" Monday "),
            QStringLiteral("4:03 PM"),
            QStringLiteral("5:03 PM")
        },
        {
            QStringLiteral("monday"),
            QStringLiteral("4:02 PM"),
            QStringLiteral("5:02 PM")
        }
    };

    QCOMPARE(
        ScheduleImportReviewPresentation::compactMeetingText(times),
        QStringLiteral(
            "Mon. not-a-time / Mon.  9:40  / Mon. 9:45 / Mon. 09:50 / "
            "Mon. 4pm / Monday 4:03pm / Mon. 4:07pm / Mon. 4:15PM / "
            "Mon. 4:20pm / Tues.  4:15 PM  / monday 4:02pm"
            )
        );
}

void ScheduleImportDialogTests
    ::compactMeetingTextUsesCanonicalWeekdayLabels_data()
{
    QTest::addColumn<QString>("day");
    QTest::addColumn<QString>("expectedText");

    QTest::newRow("Monday")
        << QStringLiteral("Monday") << QStringLiteral("Mon. 4pm");
    QTest::newRow("Tuesday")
        << QStringLiteral("Tuesday") << QStringLiteral("Tues. 4pm");
    QTest::newRow("Wednesday")
        << QStringLiteral("Wednesday") << QStringLiteral("Wed. 4pm");
    QTest::newRow("Thursday")
        << QStringLiteral("Thursday") << QStringLiteral("Thurs. 4pm");
    QTest::newRow("Friday")
        << QStringLiteral("Friday") << QStringLiteral("Fri. 4pm");
    QTest::newRow("Saturday")
        << QStringLiteral("Saturday") << QStringLiteral("Sat. 4pm");
    QTest::newRow("Sunday")
        << QStringLiteral("Sunday") << QStringLiteral("Sun. 4pm");
    QTest::newRow("padded-canonical")
        << QStringLiteral(" Monday ") << QStringLiteral("Monday 4pm");
    QTest::newRow("lowercase")
        << QStringLiteral("monday") << QStringLiteral("monday 4pm");
    QTest::newRow("padded-unknown")
        << QStringLiteral(" Funday ") << QStringLiteral("Funday 4pm");
    QTest::newRow("empty")
        << QString() << QStringLiteral(" 4pm");
}

void ScheduleImportDialogTests
    ::compactMeetingTextUsesCanonicalWeekdayLabels()
{
    QFETCH(QString, day);
    QFETCH(QString, expectedText);

    const QList<ClassTime> times{
        {
            day,
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:55 PM")
        }
    };

    QCOMPARE(
        ScheduleImportReviewPresentation::compactMeetingText(times),
        expectedText
        );
}

void ScheduleImportDialogTests
    ::classDifferencesNormalizeSpacedTimesAndEscapeRawFallbacks()
{
    ScheduleImportClassCandidate candidate;
    candidate.classGrade = QStringLiteral("Grade 7");
    candidate.classLevel = QStringLiteral("A");
    candidate.times = {
        {
            QStringLiteral("Monday"),
            QStringLiteral("4:07 PM"),
            QStringLiteral("5:07 PM")
        },
        {
            QStringLiteral("Tuesday"),
            QStringLiteral(" 4:07 PM "),
            QStringLiteral(" 5:07 PM ")
        },
        {
            QStringLiteral("Wednesday"),
            QStringLiteral("4:15PM"),
            QStringLiteral("5:15PM")
        },
        {
            QStringLiteral("Thursday"),
            QStringLiteral("16:20"),
            QStringLiteral("17:20")
        },
        {
            QStringLiteral("Friday"),
            QStringLiteral("<bad>"),
            QStringLiteral("<bad>")
        }
    };

    const ImportState::ScheduleImportStateReadClassSnapshot existing{
        *ImportDomain::ClassId::fromString("1"),
        *ImportDomain::TeacherId::fromString("2"),
        u"Class",
        u"Grade 7",
        u"A",
        u"#ffffff",
        {
            {u"Monday", u"4:00 PM", u"5:00 PM"},
            {u"Tuesday", u"4:00 PM", u"5:00 PM"},
            {u"Wednesday", u"4:00 PM", u"5:00 PM"},
            {u"Thursday", u"4:00 PM", u"5:00 PM"},
            {u"Friday", u"4:00 PM", u"5:00 PM"}
        },
        {},
        u""
    };

    QCOMPARE(
        ScheduleImportReviewPresentation::classDifferences(
            candidate,
            &existing,
            nullptr,
            1,
            ScheduleImportKind::Normal,
            QStringLiteral("#ffffff"),
            QColor(QStringLiteral("#123456")),
            QColor(QStringLiteral("#654321"))
            ),
        QStringLiteral(
            "<span style=\"color:#123456\"><b style=\"color:#654321\">"
            "Changes:</b><ul style=\"margin-top:2px; margin-bottom:0px;\">"
            "<li><b>Days:</b><br>"
            "Mon. 4:00pm - 5:00pm \u2192 Mon. 4:07pm - 5:07pm<br>"
            "Tues. 4:00pm - 5:00pm \u2192 Tues.  4:07 PM  -  5:07 PM <br>"
            "Wed. 4:00pm - 5:00pm \u2192 Wed. 4:15PM - 5:15PM<br>"
            "Thurs. 4:00pm - 5:00pm \u2192 Thurs. 16:20 - 17:20<br>"
            "Fri. 4:00pm - 5:00pm \u2192 Fri. &lt;bad&gt; - &lt;bad&gt;"
            "</li></ul></span>"
            )
        );
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
    FakeUserPromptService unexpectedPrompts;
    DialogServices::setUserPromptServiceForTesting(&unexpectedPrompts);
    QVERIFY(review.prepare());
    review.show();

    auto* table =
        review.findChild<QTableWidget*>(
            QStringLiteral("scheduleTable")
            );
    QVERIFY(table);
    QTRY_VERIFY(table->isVisible());
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
                table->item(row, column);
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
    QCOMPARE(unexpectedPrompts.messages.size(), 0);
    QCOMPARE(unexpectedPrompts.asynchronousMessages.size(), 0);
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void ScheduleImportDialogTests::possibleMatchIsPreselectedForUpdate()
{
    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");

    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("김선생");
    candidate.teacherKr = QStringLiteral("김선생");
    candidate.rooms = {QStringLiteral("414")};
    candidate.classGrade = QStringLiteral("E4");
    candidate.classLevel = QStringLiteral("Hercules");
    candidate.times = {
        {
            QStringLiteral("Tuesday"),
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
    QCOMPARE(action->currentData(Qt::UserRole + 1).toInt(), 42);
}

void ScheduleImportDialogTests::
resolutionChoicesUseTypedSnapshotAndPreserveOrdering()
{
    ScheduleWidgetTestStubs::setIncludeAdditionalClass(true);
    ScheduleWidgetTestStubs::setDuplicateKoreanTeacherNames(true);

    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");

    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("\uAE40\uC120\uC0DD");
    candidate.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
    candidate.rooms = {QStringLiteral("414")};
    candidate.classGrade = QStringLiteral("E4");
    candidate.classLevel = QStringLiteral("Hercules");
    candidate.times = {
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:50 PM")
        }
    };
    request.user.classes = {candidate};

    ScheduleImportReviewDialog review(&services, request);
    QVERIFY(review.prepare());
    review.show();

    auto* tabs = review.findChild<QTabWidget*>(
        QStringLiteral("scheduleImportResolutionTabs")
        );
    auto* teacherScrollArea = review.findChild<QScrollArea*>(
        QStringLiteral("scheduleImportTeacherScrollArea")
        );
    auto* classScrollArea = review.findChild<QScrollArea*>(
        QStringLiteral("scheduleImportClassScrollArea")
        );
    QVERIFY(tabs);
    QVERIFY(teacherScrollArea);
    QVERIFY(classScrollArea);
    tabs->setCurrentWidget(teacherScrollArea);
    QTRY_VERIFY(
        teacherScrollArea->viewport()->width() > 0
        && teacherScrollArea->viewport()->height() > 0
        );
    const auto findTeacherAction = [&review]()
    {
        return review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportTeacherAction_0")
            );
    };
    const auto findTeacherRoom = [&review]()
    {
        return review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportTeacherRoom_0")
            );
    };
    QTRY_VERIFY(
        findTeacherAction()
        && findTeacherAction()->isVisible()
        && findTeacherRoom()
        && findTeacherRoom()->isVisible()
        );

    auto* teacherAction = findTeacherAction();
    auto* teacherRoom = findTeacherRoom();
    QVERIFY(teacherAction);
    QVERIFY(teacherRoom);
    QCOMPARE(teacherRoom->count(), 1);
    QCOMPARE(teacherRoom->itemText(0), QStringLiteral("414"));
    QCOMPARE(teacherRoom->currentData().toString(), QStringLiteral("414"));

    const QStringList expectedTeacherChoices{
        QStringLiteral("Choose a resolution..."),
        QStringLiteral("Use existing: \uAE40\uC120\uC0DD \u2014 Room 413"),
        QStringLiteral("Update existing room: \uAE40\uC120\uC0DD \u2014 Room 413"),
        QStringLiteral("Use existing: \uAE40\uC120\uC0DD \u2014 Room 512"),
        QStringLiteral("Update existing room: \uAE40\uC120\uC0DD \u2014 Room 512"),
        QStringLiteral("Create a new Korean teacher"),
        QStringLiteral("Skip affected classes")
    };
    QCOMPARE(teacherAction->count(), expectedTeacherChoices.size());
    for (int index = 0; index < expectedTeacherChoices.size(); ++index)
    {
        QCOMPARE(teacherAction->itemText(index), expectedTeacherChoices[index]);
    }
    QCOMPARE(teacherAction->currentIndex(), 0);
    QCOMPARE(teacherAction->currentData(Qt::UserRole).toInt(), -1);
    QCOMPARE(teacherAction->itemData(1, Qt::UserRole + 1).toInt(), 7);
    QCOMPARE(teacherAction->itemData(3, Qt::UserRole + 1).toInt(), 8);

    tabs->setCurrentWidget(classScrollArea);
    QTRY_VERIFY(
        classScrollArea->viewport()->width() > 0
        && classScrollArea->viewport()->height() > 0
        );
    const auto findClassAction = [&review]()
    {
        return review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportClassAction_0")
            );
    };
    QTRY_VERIFY(findClassAction() && findClassAction()->isVisible());
    auto* classAction = findClassAction();
    QVERIFY(classAction);
    QCOMPARE(classAction->count(), 4);
    QCOMPARE(
        classAction->itemText(0),
        QStringLiteral("Choose Update, Create, or Skip...")
        );
    QVERIFY(classAction->itemText(1).startsWith(
        QStringLiteral("Update suggested: E4 Hercules")
        ));
    QVERIFY(classAction->itemText(1).contains(
        QStringLiteral("\uAE40\uC120\uC0DD")
        ));
    QVERIFY(classAction->itemText(1).contains(QStringLiteral("[Reg]")));
    QCOMPARE(classAction->itemData(1, Qt::UserRole + 1).toInt(), 42);
    QCOMPARE(
        classAction->itemText(2),
        QStringLiteral("Create new class")
        );
    QCOMPARE(
        classAction->itemText(3),
        QStringLiteral("Skip imported class")
        );
    QCOMPARE(classAction->currentIndex(), 1);
    QCOMPARE(
        classAction->currentData(Qt::UserRole).toInt(),
        static_cast<int>(ScheduleImportClassAction::UpdateExisting)
        );
}

void ScheduleImportDialogTests::reviewDisplaysProjectedAffectedClassCount()
{
    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");

    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("\uAE40\uC120\uC0DD");
    candidate.teacherKr = candidate.teacherKey;
    candidate.rooms = {QStringLiteral("414")};
    candidate.classGrade = QStringLiteral("E4");
    candidate.classLevel = QStringLiteral("Hercules");
    candidate.times = {
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:50 PM")
        }
    };
    request.user.classes = {candidate};

    ScheduleImportReviewDialog review(&services, request);
    QVERIFY(review.prepare());
    review.show();

    auto* tabs = review.findChild<QTabWidget*>(
        QStringLiteral("scheduleImportResolutionTabs")
        );
    auto* teacherScrollArea = review.findChild<QScrollArea*>(
        QStringLiteral("scheduleImportTeacherScrollArea")
        );
    QVERIFY(tabs);
    QVERIFY(teacherScrollArea);
    tabs->setCurrentWidget(teacherScrollArea);
    QTRY_VERIFY(
        teacherScrollArea->viewport()->width() > 0
        && teacherScrollArea->viewport()->height() > 0
        );

    auto* teacherAction = review.findChild<QComboBox*>(
        QStringLiteral("scheduleImportTeacherAction_0")
        );
    QTRY_VERIFY(teacherAction && teacherAction->isVisible());
    QVERIFY(
        teacherAction->findText(
            QStringLiteral("Update room globally (1 affected classes)")
            ) >= 0
        );
}

void ScheduleImportDialogTests::reviewMatchingUsesNormalizedSnapshotFields()
{
    ScheduleWidgetTestStubs::setClassGrade(
        42,
        QStringLiteral(" E4 ")
        );
    ScheduleWidgetTestStubs::setClassLevel(
        42,
        QStringLiteral(" hErCuLeS ")
        );
    ScheduleWidgetTestStubs::setClassRoom(
        42,
        QStringLiteral(" Room   413 ")
        );

    ApplicationServices services;
    const Result<Teacher> teacher = services.teacherService()->teacher(7);
    QVERIFY(teacher);

    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");

    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = TeacherImportNameUtils::hangulOnly(
        teacher->teacherKr
        );
    candidate.teacherKr = teacher->teacherKr;
    candidate.rooms = {QStringLiteral(" rOoM  413 ")};
    candidate.classGrade = QStringLiteral(" e4 ");
    candidate.classLevel = QStringLiteral(" HERCULES ");
    candidate.times = {
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:50 PM")
        }
    };
    request.user.classes = {candidate};

    ScheduleImportReviewDialog review(&services, request);
    QVERIFY(review.prepare());

    auto* action = review.findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_0")
        );
    QVERIFY(action);
    QCOMPARE(
        action->currentData(Qt::UserRole).toInt(),
        static_cast<int>(ScheduleImportClassAction::UpdateExisting)
        );
    QCOMPARE(action->currentData(Qt::UserRole + 1).toInt(), 42);
    QVERIFY(action->currentText().startsWith(
        QStringLiteral("Update suggested: E4 hErCuLeS")
        ));
    QCOMPARE(ScheduleWidgetTestStubs::scheduleImportPreviewCallCount, 0);
}

void ScheduleImportDialogTests::reviewSummaryUsesApplicationProjection()
{
    ScheduleWidgetTestStubs::setDuplicateKoreanTeacherNames(true);

    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");

    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("\uAE40\uC120\uC0DD");
    candidate.teacherKr = candidate.teacherKey;
    QCOMPARE(
        candidate.teacherKey.toStdString(),
        std::string("\xEA\xB9\x80\xEC\x84\xA0\xEC\x83\x9D")
        );
    candidate.rooms = {QStringLiteral("414")};
    candidate.classGrade = QStringLiteral("E4");
    candidate.classLevel = QStringLiteral("Hercules");
    candidate.times = {
        {
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:50 PM")
        }
    };
    request.user.classes = {candidate};

    ScheduleImportReviewDialog review(&services, request);
    QVERIFY(review.prepare());
    review.show();

    auto* summaryLabel = review.findChild<QLabel*>(
        QStringLiteral("scheduleImportReviewSummary")
        );
    QVERIFY(summaryLabel);

    auto* tabs = review.findChild<QTabWidget*>(
        QStringLiteral("scheduleImportResolutionTabs")
        );
    auto* teacherScrollArea = review.findChild<QScrollArea*>(
        QStringLiteral("scheduleImportTeacherScrollArea")
        );
    auto* classScrollArea = review.findChild<QScrollArea*>(
        QStringLiteral("scheduleImportClassScrollArea")
        );
    QVERIFY(tabs);
    QVERIFY(teacherScrollArea);
    QVERIFY(classScrollArea);
    const auto findTeacherAction = [&review]()
    {
        return review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportTeacherAction_0")
            );
    };
    const auto findClassAction = [&review]()
    {
        return review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportClassAction_0")
            );
    };

    tabs->setCurrentWidget(teacherScrollArea);
    QTRY_VERIFY(
        teacherScrollArea->viewport()->width() > 0
        && teacherScrollArea->viewport()->height() > 0
        && findTeacherAction()
        && findTeacherAction()->isVisible()
        );
    auto* teacherAction = findTeacherAction();

    const int createTeacherIndex = teacherAction->findData(
        static_cast<int>(ScheduleImportTeacherAction::Create),
        Qt::UserRole
        );
    QVERIFY(createTeacherIndex >= 0);
    teacherAction->setCurrentIndex(createTeacherIndex);

    tabs->setCurrentWidget(classScrollArea);
    QTRY_VERIFY(
        classScrollArea->viewport()->width() > 0
        && classScrollArea->viewport()->height() > 0
        && findClassAction()
        && findClassAction()->isVisible()
        );
    auto* classAction = findClassAction();
    const int createClassIndex = classAction->findData(
        static_cast<int>(ScheduleImportClassAction::CreateNew),
        Qt::UserRole
        );
    QVERIFY(createClassIndex >= 0);
    classAction->setCurrentIndex(createClassIndex);

    ClassMngr::Next::Application::ScheduleImportReviewDecisionRequest decisions;
    decisions.teachers.push_back({
        ClassMngr::Next::Application::ScheduleImportReviewTeacherKey{
            candidate.teacherKey.toStdString()
        },
        ClassMngr::Next::Application::ScheduleImportReviewTeacherAction::Create
    });
    decisions.classes.push_back({
        ClassMngr::Next::Application::ScheduleImportCandidateIndex{0},
        ClassMngr::Next::Application::ScheduleImportReviewClassAction::CreateNew
    });
    ClassMngr::Next::Platform::
        ApplicationServicesScheduleImportStateSnapshotPort snapshotPort(
            services
            );
    const auto snapshotOutcome =
        ImportState::ScheduleImportStateSnapshotQueryHandler::execute(
            {},
            snapshotPort
            );
    const auto* snapshot =
        std::get_if<ImportState::ScheduleImportStateSnapshot>(
            &snapshotOutcome
            );
    QVERIFY(snapshot);
    const auto expectedCleared =
        ClassMngr::Next::Application::projectScheduleImportSchedulesCleared(
            *snapshot,
            decisions,
            ImportState::ScheduleImportStateKind::Normal,
            ImportState::ScheduleImportStateIntensiveMode::ReplaceWithNew
            );
    QVERIFY(expectedCleared.rawValue() > 0);
    const auto expected =
        ClassMngr::Next::Application::projectScheduleImportReviewSummary(
            decisions,
            ClassMngr::Next::Application::ScheduleImportReviewCount{0},
            expectedCleared
            );
    const QString expectedActionSummary =
        QStringLiteral(
            "Proposed import: %1 teacher(s) created, %2 room update(s), %3 teacher group(s) skipped; "
            "%4 class(es) created, %5 updated, %6 skipped;"
            )
            .arg(expected.teachersCreated.rawValue())
            .arg(expected.teacherRoomsUpdated.rawValue())
            .arg(expected.teachersSkipped.rawValue())
            .arg(expected.classesCreated.rawValue())
            .arg(expected.classesUpdated.rawValue())
            .arg(expected.classesSkipped.rawValue());
    QTRY_VERIFY(summaryLabel->text().startsWith(expectedActionSummary));
    QVERIFY(summaryLabel->text().contains(
        QStringLiteral("%1 existing schedule(s) cleared;")
            .arg(expected.schedulesCleared.rawValue())
        ));

    tabs->setCurrentWidget(teacherScrollArea);
    QTRY_VERIFY(
        teacherScrollArea->viewport()->width() > 0
        && teacherScrollArea->viewport()->height() > 0
        && findTeacherAction()
        && findTeacherAction()->isVisible()
        );
    teacherAction = findTeacherAction();
    const int skipTeacherIndex = teacherAction->findData(
        static_cast<int>(ScheduleImportTeacherAction::Skip),
        Qt::UserRole
        );
    QVERIFY(skipTeacherIndex >= 0);
    teacherAction->setCurrentIndex(skipTeacherIndex);
    tabs->setCurrentWidget(classScrollArea);
    QTRY_VERIFY(
        classScrollArea->viewport()->width() > 0
        && classScrollArea->viewport()->height() > 0
        && findClassAction()
        && findClassAction()->isVisible()
        );
    classAction = findClassAction();
    QTRY_COMPARE(
        classAction->currentData(Qt::UserRole).toInt(),
        static_cast<int>(ScheduleImportClassAction::Skip)
        );
    QTRY_VERIFY(summaryLabel->text().contains(
        QStringLiteral(
            "0 teacher(s) created, 0 room update(s), 1 teacher group(s) skipped; "
            "0 class(es) created, 0 updated, 1 skipped;"
            )
        ));
}

void ScheduleImportDialogTests::
reviewPreviewUsesProjectedRowsInControlOrder()
{
    ScheduleWidgetTestStubs::setPossibleImportedClasses(true);

    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");

    const auto candidate = [](
                               const QString& grade,
                               const QString& level,
                               const QString& day,
                               const QString& start
                               )
    {
        ScheduleImportClassCandidate result;
        result.teacherKey = QStringLiteral("\uAE40\uC120\uC0DD");
        result.teacherKr = result.teacherKey;
        result.rooms = {QStringLiteral("414")};
        result.classGrade = grade;
        result.classLevel = level;
        const QString end =
            start.section(QLatin1Char(':'), 0, 0)
            + QStringLiteral(":50 PM");
        result.times = {
            {
                day,
                start,
                end
            }
        };
        return result;
    };
    request.user.classes = {
        candidate(
            QStringLiteral("E4"),
            QStringLiteral("Hercules"),
            QStringLiteral("Tuesday"),
            QStringLiteral("5:00 PM")
            ),
        candidate(
            QStringLiteral("E6"),
            QStringLiteral("Nova"),
            QStringLiteral("Tuesday"),
            QStringLiteral("4:00 PM")
            ),
        candidate(
            QStringLiteral("E4"),
            QStringLiteral("Hercules"),
            QStringLiteral("Tuesday"),
            QStringLiteral("6:00 PM")
            ),
        candidate(
            QStringLiteral("E7"),
            QStringLiteral("Orion"),
            QStringLiteral("Thursday"),
            QStringLiteral("7:00 PM")
            )
    };
    request.user.classes[2].teacherKey = QStringLiteral("\uBC15\uC120\uC0DD");
    request.user.classes[2].teacherKr = request.user.classes[2].teacherKey;

    ScheduleImportReviewDialog review(&services, request);
    FakeUserPromptService unexpectedPrompts;
    DialogServices::setUserPromptServiceForTesting(&unexpectedPrompts);
    QVERIFY(review.prepare());
    review.show();
    auto* classScrollArea = review.findChild<QScrollArea*>(
        QStringLiteral("scheduleImportClassScrollArea")
        );
    QVERIFY(classScrollArea);
    QTRY_VERIFY(classScrollArea->viewport()->height() > 0);
    QTRY_VERIFY(
        review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportClassAction_0")
            )
        && review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportClassAction_1")
            )
        && review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportClassAction_2")
            )
        );

    auto* skipAction = review.findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_0")
        );
    auto* createAction = review.findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_1")
        );
    auto* incompleteAction = review.findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_2")
        );
    QVERIFY(skipAction);
    QVERIFY(createAction);
    QVERIFY(incompleteAction);

    const int skipIndex = actionIndex(
        skipAction,
        ScheduleImportClassAction::Skip,
        42
        );
    const int createIndex = actionIndex(
        createAction,
        ScheduleImportClassAction::CreateNew
        );
    const int incompleteIndex = unselectedActionIndex(incompleteAction);
    QVERIFY(skipIndex >= 0);
    QVERIFY(createIndex >= 0);
    QVERIFY(incompleteIndex >= 0);
    skipAction->setCurrentIndex(skipIndex);
    createAction->setCurrentIndex(createIndex);
    incompleteAction->setCurrentIndex(incompleteIndex);
    QCOMPARE(incompleteAction->currentData(Qt::UserRole).toInt(), -1);

    QTRY_VERIFY(classScrollArea->verticalScrollBar()->maximum() > 0);
    classScrollArea->verticalScrollBar()->setValue(
        classScrollArea->verticalScrollBar()->maximum()
        );
    QTRY_VERIFY(
        review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportClassAction_3")
            )
        );
    auto* targetlessSkipAction = review.findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_3")
        );
    QVERIFY(targetlessSkipAction);
    const int targetlessSkipIndex = actionIndex(
        targetlessSkipAction,
        ScheduleImportClassAction::Skip
        );
    QVERIFY(targetlessSkipIndex >= 0);
    targetlessSkipAction->setItemData(
        targetlessSkipIndex,
        0,
        Qt::UserRole + 1
        );
    targetlessSkipAction->setCurrentIndex(targetlessSkipIndex);

    const QList<ScheduleEntry> tuesdayEntries = previewEntriesAt(
        review,
        QStringLiteral("Tuesday"),
        QStringLiteral("16:00")
        );
    QCOMPARE(tuesdayEntries.size(), 2);
    QCOMPARE(tuesdayEntries.at(0).classGrade, QStringLiteral("E4"));
    QCOMPARE(tuesdayEntries.at(0).classLevel, QStringLiteral("Hercules"));
    QCOMPARE(tuesdayEntries.at(1).classGrade, QStringLiteral("E6"));
    QCOMPARE(tuesdayEntries.at(1).classLevel, QStringLiteral("Nova"));
    QVERIFY(
        previewEntriesAt(
            review,
            QStringLiteral("Tuesday"),
            QStringLiteral("17:00")
            ).isEmpty()
        );
    QVERIFY(
        previewEntriesAt(
            review,
            QStringLiteral("Tuesday"),
            QStringLiteral("18:00")
            ).isEmpty()
        );
    QVERIFY(
        previewEntriesAt(
            review,
            QStringLiteral("Thursday"),
            QStringLiteral("19:00")
            ).isEmpty()
        );
    QCOMPARE(unexpectedPrompts.messages.size(), 0);
    QCOMPARE(unexpectedPrompts.asynchronousMessages.size(), 0);
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void ScheduleImportDialogTests::
intensivePreviewKeepsProjectedPreservationOrder()
{
    ScheduleWidgetTestStubs::setIncludeAdditionalClass(true);
    ScheduleWidgetTestStubs::setIncludeAlternativeMatchingClass(true);
    ScheduleWidgetTestStubs::setExistingIntensiveHours(true);
    ScheduleWidgetTestStubs::setDistinctIntensiveDays(true);
    ScheduleWidgetTestStubs::setClassGrade(44, QStringLiteral("E7"));
    ScheduleWidgetTestStubs::setClassLevel(44, QStringLiteral("Orion"));

    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Intensive;
    request.user.name = QStringLiteral("Alice");

    ScheduleImportClassCandidate skipped;
    skipped.teacherKey = QStringLiteral("\uAE40\uC120\uC0DD");
    skipped.teacherKr = skipped.teacherKey;
    skipped.rooms = {QStringLiteral("414")};
    skipped.classGrade = QStringLiteral("E4");
    skipped.classLevel = QStringLiteral("Hercules");
    skipped.times = {
        {
            QStringLiteral("Monday"),
            QStringLiteral("10:00 AM"),
            QStringLiteral("10:50 AM")
        }
    };

    ScheduleImportClassCandidate created;
    created.teacherKey = skipped.teacherKey;
    created.teacherKr = skipped.teacherKr;
    created.rooms = skipped.rooms;
    created.classGrade = QStringLiteral("E6");
    created.classLevel = QStringLiteral("Nova");
    created.times = {
        {
            QStringLiteral("Friday"),
            QStringLiteral("9:00 AM"),
            QStringLiteral("9:50 AM")
        }
    };
    request.user.classes = {skipped, created};

    ScheduleImportReviewDialog review(&services, request);
    FakeUserPromptService unexpectedPrompts;
    DialogServices::setUserPromptServiceForTesting(&unexpectedPrompts);
    QVERIFY(review.prepare());

    auto* updateMode = review.findChild<QRadioButton*>(
        QStringLiteral("scheduleImportUpdateIntensiveRadio")
        );
    auto* skipAction = review.findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_0")
        );
    auto* createAction = review.findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_1")
        );
    QVERIFY(updateMode);
    QVERIFY(skipAction);
    QVERIFY(createAction);
    QVERIFY(updateMode->isChecked());

    const int skipIndex = actionIndex(
        skipAction,
        ScheduleImportClassAction::Skip
        );
    const int createIndex = actionIndex(
        createAction,
        ScheduleImportClassAction::CreateNew
        );
    QVERIFY(skipIndex >= 0);
    QVERIFY(createIndex >= 0);
    skipAction->setItemData(skipIndex, 42, Qt::UserRole + 1);
    skipAction->setCurrentIndex(skipIndex);
    createAction->setCurrentIndex(createIndex);

    const QList<ScheduleEntry> fridayEntries = previewEntriesAt(
        review,
        QStringLiteral("Friday"),
        QStringLiteral("09:00")
        );
    QCOMPARE(fridayEntries.size(), 3);
    QCOMPARE(fridayEntries.at(0).classGrade, QStringLiteral("E4"));
    QCOMPARE(fridayEntries.at(0).classLevel, QStringLiteral("Hercules"));
    QCOMPARE(fridayEntries.at(1).classGrade, QStringLiteral("E6"));
    QCOMPARE(fridayEntries.at(1).classLevel, QStringLiteral("Nova"));
    QCOMPARE(fridayEntries.at(2).classGrade, QStringLiteral("E7"));
    QCOMPARE(fridayEntries.at(2).classLevel, QStringLiteral("Orion"));

    const QList<ScheduleEntry> mondayEntries = previewEntriesAt(
        review,
        QStringLiteral("Monday"),
        QStringLiteral("09:00")
        );
    QCOMPARE(mondayEntries.size(), 1);
    QCOMPARE(mondayEntries.first().classGrade, QStringLiteral("E5"));
    QCOMPARE(mondayEntries.first().classLevel, QStringLiteral("Athena"));
    QCOMPARE(unexpectedPrompts.messages.size(), 0);
    QCOMPARE(unexpectedPrompts.asynchronousMessages.size(), 0);
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void ScheduleImportDialogTests::
resolutionBuilderRetainsAdditionalEligibleTargets()
{
    const auto teacherId = *ImportDomain::TeacherId::fromString("7");
    const auto suggestedClassId = *ImportDomain::ClassId::fromString("43");
    const auto additionalClassId = *ImportDomain::ClassId::fromString("42");

    ImportState::ScheduleImportStateSnapshot snapshot;
    snapshot.teachers.push_back({teacherId, u"\uAE40\uC120\uC0DD", u"413"});
    const auto makeClass = [teacherId](
                               const ImportDomain::ClassId& id,
                               const std::u16string& name
                               )
    {
        return ImportState::ScheduleImportStateReadClassSnapshot{
            id,
            teacherId,
            name,
            u"E4",
            u"Hercules",
            {},
            {{u"Tuesday", u"4:00 PM", u"4:50 PM"}},
            {},
            {}
        };
    };
    snapshot.classes = {
        makeClass(suggestedClassId, u"Athena"),
        makeClass(additionalClassId, u"Hercules")
    };

    ScheduleImportPreview preview;
    preview.kind = ScheduleImportKind::Normal;
    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("\uAE40\uC120\uC0DD");
    candidate.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
    candidate.classGrade = QStringLiteral("E4");
    candidate.classLevel = QStringLiteral("Hercules");
    candidate.times = {
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:50 PM")
        }
    };
    preview.user.classes = {candidate};
    ScheduleImportClassPreview projected;
    projected.candidateIndex = 0;
    projected.matchingClassIds = {43};
    projected.suggestedClassId = 43;
    projected.exactMatch = true;
    projected.matchConfidence = ScheduleImportClassMatchConfidence::Confident;
    preview.classes = {projected};

    QWidget root;
    QWidget teacherContent(&root);
    QWidget classContent(&root);
    QVBoxLayout teacherLayout(&teacherContent);
    QVBoxLayout classLayout(&classContent);
    const auto result = ScheduleImportResolutionControls::build({
        &root,
        &teacherContent,
        &classContent,
        &teacherLayout,
        &classLayout,
        &snapshot,
        &preview,
        ScheduleImportKind::Normal,
        {},
        {}
    });
    QVERIFY(result);
    QCOMPARE(result->classControls.size(), 1);
    const QComboBox* action = result->classControls.constFirst().action;
    QVERIFY(action);
    QCOMPARE(action->count(), 4);
    QCOMPARE(action->itemData(0, Qt::UserRole + 1).toInt(), 43);
    QCOMPARE(action->itemData(1, Qt::UserRole + 1).toInt(), 42);
    QVERIFY(action->itemText(0).startsWith(
        QStringLiteral("Update suggested:")
        ));
    QVERIFY(action->itemText(1).startsWith(
        QStringLiteral("Update existing:")
        ));
}

void ScheduleImportDialogTests::
resolutionBuilderBoundsRowsAndRestoresDecisions()
{
    constexpr int CandidateCount = 96;
    const auto teacherId7 = *ImportDomain::TeacherId::fromString("7");
    const auto teacherId8 = *ImportDomain::TeacherId::fromString("8");

    ImportState::ScheduleImportStateSnapshot snapshot;
    snapshot.teachers = {
        {teacherId7, u"\uAE40\uC120\uC0DD", u"413"},
        {teacherId8, u"\uC774\uC120\uC0DD", u"415"}
    };
    for (int index = 0; index < CandidateCount; ++index)
    {
        const auto id = *ImportDomain::ClassId::fromString(
            std::to_string(1001 + index)
            );
        snapshot.classes.push_back(
            {
                id,
                teacherId7,
                u"Hercules",
                u"E4",
                u"Hercules",
                u"#FFFFFF",
                {{u"Tuesday", u"4:00 PM", u"4:50 PM"}},
                {},
                u"413"
            }
            );
    }

    ScheduleImportPreview preview;
    preview.kind = ScheduleImportKind::Normal;
    preview.teachers = {
        {
            QStringLiteral("teacher-key"),
            QStringLiteral("\uAE40\uC120\uC0DD"),
            {QStringLiteral("414"), QStringLiteral("415")},
            {7, 8},
            1
        }
    };
    preview.user.classes.reserve(CandidateCount);
    preview.classes.reserve(CandidateCount);
    for (int index = 0; index < CandidateCount; ++index)
    {
        ScheduleImportClassCandidate candidate;
        candidate.teacherKey = QStringLiteral("teacher-key");
        candidate.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
        candidate.classGrade = QStringLiteral("E4");
        candidate.classLevel = QStringLiteral("Hercules");
        candidate.times = {
            {
                QStringLiteral("Tuesday"),
                QStringLiteral("4:00 PM"),
                QStringLiteral("4:50 PM")
            }
        };
        preview.user.classes.append(candidate);

        ScheduleImportClassPreview row;
        row.candidateIndex = index;
        row.matchingClassIds = {1001 + index};
        row.suggestedClassId = 1001 + index;
        row.exactMatch = true;
        row.matchConfidence = ScheduleImportClassMatchConfidence::Confident;
        preview.classes.append(row);
    }

    QWidget root;
    QWidget teacherContent(&root);
    QWidget classContent(&root);
    teacherContent.resize(400, 900);
    classContent.resize(400, 32000);
    ScheduleImportResolutionControls::BuildRequest request;
    request.context = &root;
    request.teacherContent = &teacherContent;
    request.classContent = &classContent;
    request.stateSnapshot = &snapshot;
    request.preview = &preview;
    request.kind = ScheduleImportKind::Normal;
    request.virtualized = true;
    request.realizedTeacherRows = {0};
    constexpr int FixedViewportHeight = 600;
    const int fixedViewportLastRow =
        FixedViewportHeight / ScheduleImportResolutionControls::ClassRowExtent
        + ScheduleImportResolutionControls::RealizedRowOverscan;
    for (int row = 0; row <= fixedViewportLastRow; ++row)
    {
        request.realizedClassRows.insert(row);
    }

    auto smallSnapshot = snapshot;
    smallSnapshot.classes.erase(
        smallSnapshot.classes.begin() + 8,
        smallSnapshot.classes.end()
        );
    auto smallPreview = preview;
    smallPreview.classes = smallPreview.classes.mid(0, 8);
    smallPreview.user.classes = smallPreview.user.classes.mid(0, 8);
    QWidget smallRoot;
    QWidget smallTeacherContent(&smallRoot);
    QWidget smallClassContent(&smallRoot);
    smallTeacherContent.resize(400, 900);
    smallClassContent.resize(400, 3200);
    auto smallRequest = request;
    smallRequest.context = &smallRoot;
    smallRequest.teacherContent = &smallTeacherContent;
    smallRequest.classContent = &smallClassContent;
    smallRequest.stateSnapshot = &smallSnapshot;
    smallRequest.preview = &smallPreview;
    auto smallBuild = ScheduleImportResolutionControls::build(smallRequest);
    QVERIFY(smallBuild);
    auto smallState = *smallBuild;
    QCOMPARE(smallState.teacherControls.size(), 1);
    QCOMPARE(smallState.classControls.size(), 8);
    QCOMPARE(ScheduleImportResolutionControls::realizedClassCount(smallState), 4);
    QCOMPARE(
        ScheduleImportResolutionControls::realizedClassTargetChoiceCount(smallState),
        4 * 8
        );
    ScheduleImportResolutionControls::clearRealizedControls(&smallState);

    auto firstBuild = ScheduleImportResolutionControls::build(request);
    QVERIFY(firstBuild);
    auto state = *firstBuild;
    QCOMPARE(state.teacherControls.size(), 1);
    QCOMPARE(state.classControls.size(), CandidateCount);
    QCOMPARE(ScheduleImportResolutionControls::realizedTeacherCount(state), 1);
    QCOMPARE(ScheduleImportResolutionControls::realizedClassCount(state), 4);
    QCOMPARE(
        ScheduleImportResolutionControls::realizedClassTargetChoiceCount(state),
        4 * CandidateCount
        );
    QCOMPARE(teacherContent.findChildren<QComboBox*>().size(), 2);
    QCOMPARE(classContent.findChildren<QComboBox*>().size(), 4);
    QCOMPARE(classContent.findChildren<QPushButton*>().size(), 4);
    QVERIFY(
        ScheduleImportResolutionControls::realizedClassTargetChoiceCount(state)
        < CandidateCount * CandidateCount
        );

    auto& teacher = state.teacherControls[0];
    QVERIFY(teacher.action);
    QVERIFY(teacher.room);
    int teacherReuseIndex = -1;
    for (int index = 0; index < teacher.action->count(); ++index)
    {
        if (
            teacher.action->itemData(index, Qt::UserRole).toInt()
                == static_cast<int>(ScheduleImportTeacherAction::Reuse)
            && teacher.action->itemData(index, Qt::UserRole + 1).toInt() == 8
            )
        {
            teacherReuseIndex = index;
        }
    }
    QVERIFY(teacherReuseIndex >= 0);
    teacher.action->setCurrentIndex(teacherReuseIndex);
    const int roomIndex = teacher.room->findData(QStringLiteral("415"));
    QVERIFY(roomIndex >= 0);
    teacher.room->setCurrentIndex(roomIndex);

    auto& firstClass = state.classControls[0];
    QVERIFY(firstClass.action);
    int lastExistingTargetIndex = -1;
    for (int index = 0; index < firstClass.action->count(); ++index)
    {
        if (
            firstClass.action->itemData(index, Qt::UserRole).toInt()
                == static_cast<int>(ScheduleImportClassAction::UpdateExisting)
            && firstClass.action->itemData(index, Qt::UserRole + 1).toInt()
                == 1096
            )
        {
            lastExistingTargetIndex = index;
        }
    }
    QVERIFY(lastExistingTargetIndex >= 0);
    firstClass.action->setCurrentIndex(lastExistingTargetIndex);
    firstClass.color = QStringLiteral("#123456");
    ScheduleImportResolutionControls::updateClassColorButton(&firstClass);
    ScheduleImportResolutionControls::captureRealizedDecisions(&state);
    QCOMPARE(teacher.targetTeacherId, 8);
    QCOMPARE(teacher.selectedRoom, QStringLiteral("415"));
    QCOMPARE(firstClass.targetClassId, 1096);

    ScheduleImportResolutionControls::clearRealizedControls(&state);
    request.previousState = &state;
    request.realizedTeacherRows.clear();
    request.realizedClassRows.clear();
    for (int row = 40; row < 44; ++row)
    {
        request.realizedClassRows.insert(row);
    }
    auto scrolledBuild = ScheduleImportResolutionControls::build(request);
    QVERIFY(scrolledBuild);
    state = *scrolledBuild;
    QCOMPARE(state.classControls.size(), CandidateCount);
    QCOMPARE(state.classControls[0].action, nullptr);
    QCOMPARE(state.classControls[0].targetClassId, 1096);
    QCOMPARE(state.classControls[0].color, QStringLiteral("#123456"));
    QCOMPARE(state.teacherControls[0].action, nullptr);
    QCOMPARE(teacherContent.findChildren<QComboBox*>().size(), 0);
    QCOMPARE(classContent.findChildren<QComboBox*>().size(), 4);
    QCOMPARE(state.teacherControls[0].targetTeacherId, 8);
    QCOMPARE(state.teacherControls[0].selectedRoom, QStringLiteral("415"));

    ScheduleImportResolutionControls::clearRealizedControls(&state);
    request.previousState = &state;
    request.realizedTeacherRows = {0};
    request.realizedClassRows = {0};
    auto returnedBuild = ScheduleImportResolutionControls::build(request);
    QVERIFY(returnedBuild);
    state = *returnedBuild;
    QCOMPARE(state.classControls[0].action->currentData(Qt::UserRole + 1).toInt(), 1096);
    QCOMPARE(state.classControls[0].color, QStringLiteral("#123456"));
    QCOMPARE(state.classControls[0].action->count(), CandidateCount + 2);
    QCOMPARE(state.teacherControls[0].action->currentData(Qt::UserRole).toInt(),
        static_cast<int>(ScheduleImportTeacherAction::Reuse));
    QCOMPARE(state.teacherControls[0].action->currentData(Qt::UserRole + 1).toInt(), 8);
    QCOMPARE(state.teacherControls[0].room->currentData().toString(), QStringLiteral("415"));

    const QList<ScheduleImportTeacherResolution> teacherResolutions =
        ScheduleImportResolutionControls::teacherResolutions(state);
    QCOMPARE(teacherResolutions.size(), 1);
    QCOMPARE(teacherResolutions.first().teacherKey, QStringLiteral("teacher-key"));
    QCOMPARE(teacherResolutions.first().targetTeacherId, 8);
    QCOMPARE(teacherResolutions.first().selectedRoom, QStringLiteral("415"));
    const QList<ScheduleImportClassResolution> classResolutions =
        ScheduleImportResolutionControls::classResolutions(state);
    QCOMPARE(classResolutions.size(), CandidateCount);
    QCOMPARE(classResolutions.first().candidateIndex, 0);
    QCOMPARE(classResolutions.first().targetClassId, 1096);
    QCOMPARE(classResolutions.first().classColor, QStringLiteral("#123456"));
    QCOMPARE(classResolutions.last().candidateIndex, CandidateCount - 1);
    QCOMPARE(classResolutions.last().targetClassId, 1096);

    ScheduleImportReviewContext context;
    context.kind = ScheduleImportKind::Normal;
    context.selectedUserName = QStringLiteral("Alice");
    context.candidates = preview.user.classes;
    const auto completeApplyRequest =
        ScheduleImportReviewModel::buildApplyRequest(
            context,
            teacherResolutions,
            classResolutions
            );
    QCOMPARE(completeApplyRequest.teachers.size(), std::size_t(1));
    QCOMPARE(completeApplyRequest.classes.size(),
        static_cast<std::size_t>(CandidateCount));
    QCOMPARE(completeApplyRequest.classes.front().candidateIndex.rawValue(), 0);
    QVERIFY(completeApplyRequest.classes.front().targetClassId.has_value());
    QCOMPARE(completeApplyRequest.classes.front().targetClassId->value(),
        std::string("1096"));
    QCOMPARE(completeApplyRequest.classes.back().candidateIndex.rawValue(),
        CandidateCount - 1);

    auto uniqueValidationClasses = classResolutions;
    uniqueValidationClasses.last().action = ScheduleImportClassAction::CreateNew;
    uniqueValidationClasses.last().targetClassId = -1;
    const auto uniqueApplyRequest =
        ScheduleImportReviewModel::buildApplyRequest(
            context,
            teacherResolutions,
            uniqueValidationClasses
            );
    const auto decisionRequest =
        ClassMngr::Next::Application::
            projectScheduleImportApplyReviewDecisions(uniqueApplyRequest);
    const auto readiness =
        ClassMngr::Next::Application::evaluateScheduleImportReviewReadiness(
            decisionRequest,
            std::nullopt
            );
    QVERIFY(readiness.decisions.accepted());
    QCOMPARE(decisionRequest.classes.size(),
        static_cast<std::size_t>(CandidateCount));

    state.teacherControls[0].actionValue = static_cast<int>(
        ScheduleImportTeacherAction::Skip
        );
    ScheduleImportResolutionControls::forceSkippedTeachers(
        &state,
        {QStringLiteral("teacher-key")}
        );
    for (int row = 0; row < CandidateCount; ++row)
    {
        QCOMPARE(
            state.classControls[row].actionValue,
            static_cast<int>(ScheduleImportClassAction::Skip)
            );
        QCOMPARE(state.classControls[row].targetClassId, 1001 + row);
    }
    state.teacherControls[0].actionValue = static_cast<int>(
        ScheduleImportTeacherAction::Create
        );
    ScheduleImportResolutionControls::forceSkippedTeachers(
        &state,
        {}
        );
    QCOMPARE(
        state.classControls.first().actionValue,
        static_cast<int>(ScheduleImportClassAction::Skip)
        );
    ScheduleImportResolutionControls::clearRealizedControls(&state);
}

void ScheduleImportDialogTests::
reviewPrepareRejectsUnknownKindBeforeLoadingSnapshot()
{
    // The kind failure must take precedence over a snapshot read failure.
    ScheduleWidgetTestStubs::setScheduleClassInfoReadFailure(true);
    ApplicationServices services;
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    for (const int kind : {-1, 255})
    {
        ScheduleImportReviewRequest request;
        request.kind = static_cast<ScheduleImportKind>(kind);
        request.user.name = QStringLiteral("Alice");
        request.user.classes.append(ScheduleImportClassCandidate{});
        ScheduleImportReviewDialog review(&services, request);
        QVERIFY(!review.prepare());
        QCOMPARE(
            review.findChildren<QComboBox*>(
                QRegularExpression(QStringLiteral("^scheduleImportTeacherAction_"))
                ).size(),
            0
            );
        QCOMPARE(
            review.findChildren<QComboBox*>(
                QRegularExpression(QStringLiteral("^scheduleImportClassAction_"))
                ).size(),
            0
            );
    }
    QCOMPARE(ScheduleWidgetTestStubs::scheduleImportPreviewCallCount, 0);
    QCOMPARE(prompts.messages.size(), 2);
    for (const PromptRequest& message : prompts.messages)
    {
        QCOMPARE(message.title, QStringLiteral("Review Schedule Import"));
        QCOMPARE(
            message.message,
            QStringLiteral("The schedule import kind is invalid.")
            );
    }
}

void ScheduleImportDialogTests::
reviewPrepareSnapshotFailureDoesNotBuildControlsOrFallback()
{
    ScheduleWidgetTestStubs::setScheduleClassInfoReadFailure(true);

    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");

    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("\uAE40\uC120\uC0DD");
    candidate.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
    candidate.rooms = {QStringLiteral("413")};
    candidate.importedColors = {QStringLiteral("#123456")};
    candidate.classGrade = QStringLiteral("E4");
    candidate.classLevel = QStringLiteral("Hercules");
    candidate.times = {
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:50 PM")
        }
    };
    request.user.classes = {candidate};

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    ScheduleImportReviewDialog review(&services, request);
    QVERIFY(!review.prepare());

    QCOMPARE(ScheduleWidgetTestStubs::scheduleImportPreviewCallCount, 0);
    QCOMPARE(
        review.findChildren<QComboBox*>(
            QRegularExpression(QStringLiteral("^scheduleImportTeacherAction_"))
            ).size(),
        0
        );
    QCOMPARE(
        review.findChildren<QComboBox*>(
            QRegularExpression(QStringLiteral("^scheduleImportClassAction_"))
            ).size(),
        0
        );
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Review Schedule Import")
        );
    QVERIFY(prompts.messages.constFirst().message.contains(
        QStringLiteral("Import resolution data could not be loaded.")
        ));
    QVERIFY(prompts.messages.constFirst().details.contains(
        QStringLiteral("injected schedule read failure")
        ));
}

void ScheduleImportDialogTests::
reviewPrepareClosedSessionUsesSnapshotWarning()
{
    ScheduleWidgetTestStubs::setDatabaseSessionOpen(false);

    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");

    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("\uAE40\uC120\uC0DD");
    candidate.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
    candidate.rooms = {QStringLiteral("413")};
    candidate.classGrade = QStringLiteral("E4");
    candidate.classLevel = QStringLiteral("Hercules");
    candidate.times = {
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:50 PM")
        }
    };
    request.user.classes = {candidate};

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    ScheduleImportReviewDialog review(&services, request);
    QVERIFY(!review.prepare());

    QCOMPARE(ScheduleWidgetTestStubs::scheduleImportPreviewCallCount, 0);
    QCOMPARE(
        review.findChildren<QComboBox*>(
            QRegularExpression(QStringLiteral("^scheduleImportTeacherAction_"))
            ).size(),
        0
        );
    QCOMPARE(
        review.findChildren<QComboBox*>(
            QRegularExpression(QStringLiteral("^scheduleImportClassAction_"))
            ).size(),
        0
        );
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Review Schedule Import")
        );
    QVERIFY(prompts.messages.constFirst().message.contains(
        QStringLiteral("Import resolution data could not be loaded.")
        ));
    QVERIFY(prompts.messages.constFirst().details.contains(
        QStringLiteral("no open profile")
        ));
}

void ScheduleImportDialogTests::reviewRefreshUsesFreshTypedStateSnapshot()
{
    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");

    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("\uAE40\uC120\uC0DD");
    candidate.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
    candidate.rooms = {QStringLiteral("413")};
    candidate.classGrade = QStringLiteral("E4");
    candidate.classLevel = QStringLiteral("Hercules");
    candidate.times = {
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:50 PM")
        }
    };
    request.user.classes = {candidate};

    ScheduleImportReviewDialog review(&services, request);
    QVERIFY(review.prepare());
    auto* action = review.findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_0")
        );
    auto* details = review.findChild<QLabel*>(
        QStringLiteral("scheduleImportClassDifferences_0")
        );
    QVERIFY(action);
    QVERIFY(details);

    const int updateIndex = actionIndex(
        action,
        ScheduleImportClassAction::UpdateExisting,
        42
        );
    const int createIndex = actionIndex(
        action,
        ScheduleImportClassAction::CreateNew
        );
    QVERIFY(updateIndex >= 0);
    QVERIFY(createIndex >= 0);

    ScheduleWidgetTestStubs::setClassGrade(
        42,
        QStringLiteral("M1")
        );
    action->setCurrentIndex(createIndex);
    action->setCurrentIndex(updateIndex);

    QVERIFY(details->text().contains(QStringLiteral("M1")));
    QVERIFY(details->text().contains(QStringLiteral("E4")));
}

void ScheduleImportDialogTests::
reviewSnapshotFailureStaysInvalidWithoutLegacyFallback()
{
    ScheduleWidgetTestStubs::setIncludeAdditionalClass(true);
    ScheduleWidgetTestStubs::setPossibleImportedClasses(true);

    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");

    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("\uAE40\uC120\uC0DD");
    candidate.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
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
    candidate.meetingPatternError = QStringLiteral("invalid meeting data");
    request.user.classes = {candidate};

    FakeUserPromptService prompts;
    ScopedTestUserPromptService promptService(&prompts);
    ScheduleImportReviewDialog review(&services, request);
    QVERIFY(review.prepare());
    review.show();

    auto* tabs = review.findChild<QTabWidget*>(
        QStringLiteral("scheduleImportResolutionTabs")
        );
    auto* teacherScrollArea = review.findChild<QScrollArea*>(
        QStringLiteral("scheduleImportTeacherScrollArea")
        );
    auto* classScrollArea = review.findChild<QScrollArea*>(
        QStringLiteral("scheduleImportClassScrollArea")
        );
    QVERIFY(tabs);
    QVERIFY(teacherScrollArea);
    QVERIFY(classScrollArea);
    tabs->setCurrentWidget(classScrollArea);

    const auto findClassAction = [&review]()
    {
        return review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportClassAction_0")
            );
    };
    QTRY_VERIFY(
        classScrollArea->viewport()->width() > 0
        && classScrollArea->viewport()->height() > 0
        && findClassAction()
        && findClassAction()->isVisible()
        );
    auto* classAction = findClassAction();
    QVERIFY(classAction);
    QVERIFY(classAction->count() > 1);
    ScheduleWidgetTestStubs::setScheduleClassInfoReadFailure(true);
    classAction->setCurrentIndex(classAction->count() - 1);

    auto* import = review.findChild<QPushButton*>(
        QStringLiteral("scheduleImportAcceptButton")
        );
    auto* status = review.findChild<QLabel*>(
        QStringLiteral("scheduleImportReviewStatus")
        );
    auto* summary = review.findChild<QLabel*>(
        QStringLiteral("scheduleImportReviewSummary")
        );
    QVERIFY(import);
    QVERIFY(status);
    QVERIFY(summary);
    QVERIFY(summary->text().contains(
        QStringLiteral("0 existing schedule(s) cleared;")
        ));
    QVERIFY(!import->isEnabled());
    QVERIFY(status->text().contains(
        QStringLiteral("injected schedule read failure")
        ));
    QCOMPARE(
        review.findChildren<QComboBox*>(
            QRegularExpression(QStringLiteral("^scheduleImportClassAction_"))
            ).size(),
        1
        );
    QCOMPARE(prompts.messages.size(), 0);
    QCOMPARE(ScheduleWidgetTestStubs::scheduleImportPreviewCallCount, 0);

    tabs->setCurrentWidget(teacherScrollArea);
    const auto findTeacherAction = [&review]()
    {
        return review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportTeacherAction_0")
            );
    };
    QTRY_VERIFY(
        teacherScrollArea->viewport()->width() > 0
        && teacherScrollArea->viewport()->height() > 0
        && findTeacherAction()
        && findTeacherAction()->isVisible()
        );
    QCOMPARE(
        review.findChildren<QComboBox*>(
            QRegularExpression(QStringLiteral("^scheduleImportTeacherAction_"))
            ).size(),
        1
        );
    QCOMPARE(
        review.findChildren<QComboBox*>(
            QRegularExpression(QStringLiteral("^scheduleImportClassAction_"))
            ).size(),
        0
        );
    QVERIFY(!import->isEnabled());
    QVERIFY(status->text().contains(
        QStringLiteral("injected schedule read failure")
        ));
    QCOMPARE(prompts.messages.size(), 0);
}

void ScheduleImportDialogTests::
wideTeacherIdSurvivesReviewResolutionAndTypedApplyProjection()
{
    constexpr qint64 wideTeacherIdValue = 2147483648LL;
    const ImportDomain::TeacherId lowTeacherId =
        *ImportDomain::TeacherId::fromString("7");
    const ImportDomain::TeacherId wideTeacherId =
        *ImportDomain::TeacherId::fromString("2147483648");

    ImportState::ScheduleImportStateSnapshot snapshot;
    snapshot.teachers = {
        {
            lowTeacherId,
            u"Same teacher",
            u"Low room"
        },
        {
            wideTeacherId,
            u"Same teacher",
            u"Wide room"
        }
    };
    snapshot.classes = {
        {
            *ImportDomain::ClassId::fromString("42"),
            lowTeacherId,
            u"Low teacher class",
            u"E4",
            u"Hercules",
            u"#FFFFFF",
            {},
            {},
            u"Low room"
        },
        {
            *ImportDomain::ClassId::fromString("43"),
            wideTeacherId,
            u"Wide teacher class",
            u"E5",
            u"Athena",
            u"#FFFFFF",
            {},
            {},
            u"Wide room"
        }
    };

    ScheduleImportPreview preview;
    preview.kind = ScheduleImportKind::Normal;
    preview.user.name = QStringLiteral("Alice");
    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("same-teacher");
    candidate.teacherKr = QStringLiteral("Same teacher");
    candidate.rooms = {QStringLiteral("Imported room")};
    candidate.classGrade = QStringLiteral("E5");
    candidate.classLevel = QStringLiteral("Athena");
    preview.user.classes = {candidate};
    ScheduleImportTeacherPreview teacherPreview;
    teacherPreview.teacherKey = candidate.teacherKey;
    teacherPreview.teacherKr = candidate.teacherKr;
    teacherPreview.importedRooms = candidate.rooms;
    teacherPreview.matchingTeacherIds = {7, wideTeacherIdValue};
    preview.teachers = {teacherPreview};

    QObject context;
    QWidget teacherContent;
    QWidget classContent;
    QVBoxLayout teacherLayout(&teacherContent);
    QVBoxLayout classLayout(&classContent);
    ScheduleImportResolutionControls::BuildRequest buildRequest;
    buildRequest.context = &context;
    buildRequest.teacherContent = &teacherContent;
    buildRequest.classContent = &classContent;
    buildRequest.teacherLayout = &teacherLayout;
    buildRequest.classLayout = &classLayout;
    buildRequest.stateSnapshot = &snapshot;
    buildRequest.preview = &preview;
    buildRequest.kind = ScheduleImportKind::Normal;

    auto controls = ScheduleImportResolutionControls::build(buildRequest);
    if (!controls.has_value())
    {
        QFAIL(qPrintable(controls.error()));
    }
    QCOMPARE(controls->teacherControls.size(), 1);
    QComboBox* action = controls->teacherControls.first().action;
    QVERIFY(action);

    int updateWideTeacherIndex = -1;
    for (int index = 0; index < action->count(); ++index)
    {
        if (
            action->itemData(index, Qt::UserRole).toInt()
                == static_cast<int>(ScheduleImportTeacherAction::UpdateRoom)
            && action->itemData(index, Qt::UserRole + 1).toLongLong()
                == wideTeacherIdValue
            )
        {
            updateWideTeacherIndex = index;
            break;
        }
    }
    QVERIFY(updateWideTeacherIndex >= 0);
    action->setCurrentIndex(updateWideTeacherIndex);
    ScheduleImportResolutionControls::captureRealizedDecisions(&*controls);
    QCOMPARE(
        controls->teacherControls.first().targetTeacherId,
        wideTeacherIdValue
        );

    ScheduleImportReviewContext reviewContext;
    reviewContext.kind = ScheduleImportKind::Normal;
    reviewContext.selectedUserName = preview.user.name;
    reviewContext.candidates = preview.user.classes;
    const auto request = ScheduleImportReviewModel::buildApplyRequest(
        reviewContext,
        ScheduleImportResolutionControls::teacherResolutions(*controls),
        {}
        );
    QCOMPARE(request.teachers.size(), std::size_t(1));
    QVERIFY(request.teachers.front().targetTeacherId.has_value());
    QCOMPARE(
        request.teachers.front().targetTeacherId->value(),
        std::string("2147483648")
        );
}

void ScheduleImportDialogTests::ambiguousTargetedClassSkipIsRejected()
{
    ScheduleWidgetTestStubs::setIncludeAdditionalClass(true);
    ScheduleWidgetTestStubs::setIncludeAlternativeMatchingClass(true);

    ApplicationServices services;
    const Result<Teacher> teacher = services.teacherService()->teacher(7);
    QVERIFY(teacher);
    ScheduleImportReviewDialog review(
        &services,
        classSkipRequest(*teacher)
        );
    QVERIFY(review.prepare());

    auto* action = review.findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_0")
        );
    auto* import = review.findChild<QPushButton*>(
        QStringLiteral("scheduleImportAcceptButton")
        );
    auto* status = review.findChild<QLabel*>(
        QStringLiteral("scheduleImportReviewStatus")
        );
    QVERIFY(action);
    QVERIFY(import);
    QVERIFY(status);

    const int skipIndex = actionIndex(
        action,
        ScheduleImportClassAction::Skip
        );
    QVERIFY(skipIndex >= 0);
    action->setItemData(skipIndex, 42, Qt::UserRole + 1);
    action->setCurrentIndex(skipIndex);

    QCOMPARE(action->currentData(Qt::UserRole + 1).toInt(), 42);
    QVERIFY(!import->isEnabled());
    QVERIFY(status->text().contains(QStringLiteral("unique exact")));
    auto* details = review.findChild<QLabel*>(
        QStringLiteral("scheduleImportClassDifferences_0")
        );
    QVERIFY(details);
    QVERIFY(
        details->text().contains(
            QStringLiteral("selected existing class")
            )
        );
}

void ScheduleImportDialogTests::uniqueExactTargetedClassSkipIsAllowed()
{
    ApplicationServices services;
    const Result<Teacher> teacher = services.teacherService()->teacher(7);
    QVERIFY(teacher);
    ScheduleImportReviewDialog review(
        &services,
        classSkipRequest(*teacher)
        );
    QVERIFY(review.prepare());

    auto* action = review.findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_0")
        );
    auto* import = review.findChild<QPushButton*>(
        QStringLiteral("scheduleImportAcceptButton")
        );
    auto* status = review.findChild<QLabel*>(
        QStringLiteral("scheduleImportReviewStatus")
        );
    QVERIFY(action);
    QVERIFY(import);
    QVERIFY(status);

    const int skipIndex = actionIndex(
        action,
        ScheduleImportClassAction::Skip
        );
    QVERIFY(skipIndex >= 0);
    action->setItemData(skipIndex, 42, Qt::UserRole + 1);
    action->setCurrentIndex(skipIndex);

    QTRY_VERIFY(import->isEnabled());
    QCOMPARE(
        status->text(),
        QStringLiteral("All required resolutions are complete.")
        );
}

void ScheduleImportDialogTests::targetlessClassSkipIsAllowed()
{
    ApplicationServices services;
    const Result<Teacher> teacher = services.teacherService()->teacher(7);
    QVERIFY(teacher);
    ScheduleImportReviewDialog review(
        &services,
        classSkipRequest(*teacher)
        );
    QVERIFY(review.prepare());

    auto* action = review.findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_0")
        );
    auto* import = review.findChild<QPushButton*>(
        QStringLiteral("scheduleImportAcceptButton")
        );
    auto* status = review.findChild<QLabel*>(
        QStringLiteral("scheduleImportReviewStatus")
        );
    QVERIFY(action);
    QVERIFY(import);
    QVERIFY(status);

    const int skipIndex = actionIndex(
        action,
        ScheduleImportClassAction::Skip
        );
    QVERIFY(skipIndex >= 0);
    action->setItemData(skipIndex, 0, Qt::UserRole + 1);
    action->setCurrentIndex(skipIndex);

    QTRY_VERIFY(import->isEnabled());
    QCOMPARE(
        status->text(),
        QStringLiteral("All required resolutions are complete.")
        );
}

void ScheduleImportDialogTests::reviewWarnsForDuplicateClassTargets()
{
    ScheduleWidgetTestStubs::setIncludeAdditionalClass(true);
    ScheduleWidgetTestStubs::setIncludeAlternativeMatchingClass(true);
    ScheduleWidgetTestStubs::setClassGrade(43, QStringLiteral("E4"));
    ScheduleWidgetTestStubs::setClassLevel(43, QStringLiteral("Hercules"));
    ScheduleWidgetTestStubs::setClassRegularHoursEmpty(43, true);

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

    auto* classScrollArea = review.findChild<QScrollArea*>(
        QStringLiteral("scheduleImportClassScrollArea")
        );
    QVERIFY(classScrollArea);
    QTRY_VERIFY(
        classScrollArea->viewport()->width() > 0
        && classScrollArea->viewport()->height() > 0
        );
    const auto findFirstAction = [&review]()
    {
        return review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportClassAction_0")
            );
    };
    const auto findSecondAction = [&review]()
    {
        return review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportClassAction_1")
            );
    };
    QTRY_VERIFY(
        findFirstAction()
        && findFirstAction()->isVisible()
        && findSecondAction()
        && findSecondAction()->isVisible()
        );
    auto* firstAction = review.findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_0")
        );
    auto* secondAction = review.findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_1")
        );
    QVERIFY(firstAction);
    QVERIFY(secondAction);
    QCOMPARE(
        firstAction->currentData(Qt::UserRole + 1).toInt(),
        44
        );
    QCOMPARE(
        secondAction->currentData(Qt::UserRole + 1).toInt(),
        44
        );
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
    QVERIFY(warning->text().contains(QStringLiteral("E4 Hercules")));
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

    QTRY_VERIFY(findSecondAction() && findSecondAction()->isVisible());
    secondAction = findSecondAction();
    QVERIFY(secondAction);
    const int alternateTargetIndex =
        actionIndex(
            secondAction,
            ScheduleImportClassAction::UpdateExisting,
            43
            );
    QVERIFY(alternateTargetIndex >= 0);
    secondAction->setCurrentIndex(alternateTargetIndex);
    QTRY_VERIFY(import->isEnabled());

    QTRY_VERIFY(findSecondAction() && findSecondAction()->isVisible());
    secondAction = findSecondAction();
    QVERIFY(secondAction);
    const int duplicateTargetIndex =
        actionIndex(
            secondAction,
            ScheduleImportClassAction::UpdateExisting,
            44
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

void ScheduleImportDialogTests::invalidExistingClassTargetMessageIsClear()
{
    using Code = ClassMngr::Next::Application::
        ScheduleImportReviewDecisionIssueCode;
    const ClassMngr::Next::Application::ScheduleImportReviewDecisionIssue issue{
        Code::InvalidExistingClassTarget
    };

    QCOMPARE(
        ScheduleImportReviewPresentation::reviewDecisionMessage(issue),
        QStringLiteral("Choose a valid existing class target.")
        );
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
           const QString& day,
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
                {day, start, end}
            };
            return result;
        };
    request.user.classes = {
        candidate(
            QStringLiteral("김선생"),
            QStringLiteral("Hercules"),
            QStringLiteral("Monday"),
            QStringLiteral("4:00 PM"),
            QStringLiteral("4:55 PM")
            ),
        candidate(
            QStringLiteral("이선생"),
            QStringLiteral("Athena"),
            QStringLiteral("Monday"),
            QStringLiteral("4:30PM"),
            QStringLiteral("5:25PM")
            ),
        candidate(
            QStringLiteral("박선생"),
            QStringLiteral("LowercaseSentinel"),
            QStringLiteral("monday"),
            QStringLiteral("4:15 PM"),
            QStringLiteral("5:10 PM")
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
        !warning->text().contains(QStringLiteral("LowercaseSentinel"))
        );
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

void ScheduleImportDialogTests::reviewRejectsMalformedProjectedTime()
{
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
            QStringLiteral("not-a-time"),
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

    auto* status = review.findChild<QLabel*>(
        QStringLiteral("scheduleImportReviewStatus")
        );
    QVERIFY(status);
    QTRY_VERIFY(
        status->text().contains(
            QStringLiteral("contains an invalid time")
            )
        );
    QCOMPARE(
        status->text(),
        QStringLiteral(
            "E4 Hercules contains an invalid time: "
            "Monday not-a-time–4:55 PM"
            )
        );
    auto* import = review.findChild<QPushButton*>(
        QStringLiteral("scheduleImportAcceptButton")
        );
    QVERIFY(import);
    QVERIFY(!import->isEnabled());
    review.close();

    const QStringList noncanonicalWeekdays{
        QStringLiteral("monday"),
        QStringLiteral(" Monday ")
    };
    for (const QString& day : noncanonicalWeekdays)
    {
        request.user.classes[0].times = {
            {
                day,
                QStringLiteral("4:00 PM"),
                QStringLiteral("4:55 PM")
            }
        };

        ScheduleImportReviewDialog weekdayReview(
            &services,
            request
            );
        QVERIFY(weekdayReview.prepare());
        weekdayReview.show();

        auto* weekdayStatus = weekdayReview.findChild<QLabel*>(
            QStringLiteral("scheduleImportReviewStatus")
            );
        QVERIFY(weekdayStatus);
        QTRY_VERIFY(
            weekdayStatus->text().contains(
                QStringLiteral("contains an invalid time")
                )
            );
        auto* weekdayImport = weekdayReview.findChild<QPushButton*>(
            QStringLiteral("scheduleImportAcceptButton")
            );
        QVERIFY(weekdayImport);
        QVERIFY(!weekdayImport->isEnabled());
        weekdayReview.close();
    }
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
    auto* classAction = review.findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_0")
        );
    QVERIFY(classAction);
    QCOMPARE(classAction->currentData(Qt::UserRole + 1).toInt(), 42);
    const int createNewIndex = actionIndex(
        classAction,
        ScheduleImportClassAction::CreateNew
        );
    QVERIFY(createNewIndex >= 0);
    classAction->setCurrentIndex(createNewIndex);
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

void ScheduleImportDialogTests::
checkedInOverlapWorkbookPresentsReviewConflict()
{
    QFile file(
        QStringLiteral(
            CLASSMNGR_SOURCE_DIR
            "/tests/fixtures/imports/schedule_overlap_conflict.xlsx"
            )
        );
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
    const auto workbook = parseScheduleImportWorkbook(
        file.readAll(),
        ScheduleImportKind::Normal
        );
    const QString parseError =
        workbook.has_value() ? QString() : workbook.error();
    QVERIFY2(workbook.has_value(), qPrintable(parseError));
    QCOMPARE(workbook->sheets.size(), 1);
    QCOMPARE(workbook->sheets.first().users.size(), 1);

    ApplicationServices services;
    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user = workbook->sheets.first().users.first();
    ScheduleImportReviewDialog review(&services, request);
    QVERIFY(review.prepare());
    review.show();

    QTRY_VERIFY(
        review.findChild<QMessageBox*>(
            QStringLiteral("scheduleImportConflictWarning")
            )
        );
    auto* warning = review.findChild<QMessageBox*>(
        QStringLiteral("scheduleImportConflictWarning")
        );
    QVERIFY(warning);
    QVERIFY(warning->text().contains(QStringLiteral("overlaps")));
    QVERIFY(warning->text().contains(QStringLiteral("Monday")));
    QVERIFY(warning->text().contains(QStringLiteral("E4 Theseus")));
    QVERIFY(warning->text().contains(QStringLiteral("E4 Hercules")));
    QVERIFY(warning->text().contains(QStringLiteral("4:00pm")));
    QVERIFY(warning->text().contains(QStringLiteral("4:30pm")));

    auto* import = review.findChild<QPushButton*>(
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

    FakeUserPromptService unexpectedPrompts;
    DialogServices::setUserPromptServiceForTesting(&unexpectedPrompts);
    {
        ScheduleImportReviewDialog review(
            &services,
            request
            );
        QVERIFY(review.prepare());
        review.show();

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
        QTRY_VERIFY(table->isVisible());
        QTRY_VERIFY(preview->height() > table->height());
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

    saveSettingOrFail(services.dataService(),
        QStringLiteral("schedule_use_24h"),
        QStringLiteral("true")
        );
    saveSettingOrFail(services.dataService(),
        QStringLiteral("schedule_show_weekends"),
        QStringLiteral("true")
        );

    ScheduleImportReviewDialog review(
        &services,
        request
        );
    QVERIFY(review.prepare());
    review.show();

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
    QTRY_VERIFY(table->isVisible());
    QTRY_VERIFY(preview->height() > table->height());
    QTRY_COMPARE(table->geometry().top(), 0);
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
    QTRY_COMPARE(preview->width(), 540);
    QTRY_VERIFY(tabs->width() > initialTabsWidth);
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
        table->item(0, 6);
    QVERIFY(scheduledClass);
    QVERIFY(
        scheduledClass->data(ScheduleWidgetDelegates::HtmlRole).toString().contains(
            QStringLiteral("font-size:%1pt").arg(
                FontManager::adjustedPointSize(11)
                )
            )
        );
    QVERIFY(
        scheduledClass->data(ScheduleWidgetDelegates::HtmlRole).toString().contains(
            QStringLiteral("font-size:%1pt").arg(
                FontManager::adjustedPointSize(10)
                )
            )
        );
    QVERIFY(
        scheduledClass->data(ScheduleWidgetDelegates::CompactRole).toBool()
        );
    QCOMPARE(unexpectedPrompts.messages.size(), 0);
    QCOMPARE(unexpectedPrompts.asynchronousMessages.size(), 0);
    DialogServices::setUserPromptServiceForTesting(nullptr);
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
    FakeUserPromptService unexpectedPrompts;
    DialogServices::setUserPromptServiceForTesting(&unexpectedPrompts);
    QVERIFY(review.prepare());
    review.show();

    auto* table =
        review.findChild<QTableWidget*>(
            QStringLiteral("scheduleTable")
            );
    QVERIFY(table);
    QTRY_VERIFY(table->isVisible());
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

    auto* essay = table->item(0, 1);
    auto* lunch = table->item(0, 2);
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
        essay->data(ScheduleWidgetDelegates::BackgroundRole)
            .value<QColor>()
            == QColor(Qt::white)
        );
    QVERIFY(
        lunch->data(ScheduleWidgetDelegates::BackgroundRole)
            .value<QColor>()
            == QColor(QStringLiteral("#DCDCDC"))
        );
    QCOMPARE(
        essay->data(Qt::AccessibleTextRole).toString(),
        QStringLiteral("Essay")
        );
    QCOMPARE(
        lunch->data(Qt::AccessibleTextRole).toString(),
        QStringLiteral("Lunch")
        );
    QVERIFY(
        essay->data(ScheduleWidgetDelegates::ForegroundRole)
            .value<QColor>()
            == QColor(Qt::black)
        );
    QVERIFY(
        lunch->data(ScheduleWidgetDelegates::ForegroundRole)
            .value<QColor>()
            == QColor(Qt::black)
        );
    QCOMPARE(unexpectedPrompts.messages.size(), 0);
    QCOMPARE(unexpectedPrompts.asynchronousMessages.size(), 0);
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void ScheduleImportDialogTests
    ::suppliedWorkbookBuildsStagedReview()
{
    const QString configuredPath =
        qEnvironmentVariable(
            "CLASSMNGR_SCHEDULE_IMPORT_SAMPLE"
            ).trimmed();
    const QString path =
        configuredPath.isEmpty()
            ? QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
                  QStringLiteral("tests/fixtures/imports/schedule_review.xlsx"))
            : configuredPath;
    if (!QFile::exists(path))
    {
        QSKIP(
            "The permanent schedule review fixture is missing; set CLASSMNGR_SCHEDULE_IMPORT_SAMPLE to validate another workbook."
            );
    }

    ApplicationServices services;
    saveSettingOrFail(services.dataService(),
        QStringLiteral("myInfo/name"),
        QString()
        );
    const QString reviewFontFamily = loadReviewFontFamily();
    if (!reviewFontFamily.isEmpty())
    {
        QApplication::setFont(QFont(reviewFontFamily));
    }
    ScheduleImportDialog dialog(&services);
    dialog.setFilePath(path);
    dialog.show();
    QCoreApplication::processEvents();
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

    const QString screenshotPath =
        qEnvironmentVariable(
            "CLASSMNGR_SCHEDULE_REVIEW_OUTPUT_PATH"
            ).trimmed();
    if (!screenshotPath.isEmpty())
    {
        QVERIFY2(
            QDir().mkpath(QFileInfo(screenshotPath).absolutePath()),
            qPrintable(
                QStringLiteral("Could not create schedule review screenshot directory for %1")
                    .arg(screenshotPath)
                )
            );
        review->show();
        QCoreApplication::processEvents();
        const QPixmap screenshot = review->grab();
        QVERIFY(!screenshot.isNull());
        QVERIFY2(
            screenshot.save(screenshotPath, "PNG"),
            qPrintable(QStringLiteral("Could not save schedule review screenshot: %1")
                           .arg(screenshotPath))
            );
    }
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

void ScheduleImportDialogTests
    ::permanentConflictWorkbookPresentsReviewWarning()
{
    const QString configuredPath =
        qEnvironmentVariable(
            "CLASSMNGR_SCHEDULE_CONFLICT_SAMPLE"
            ).trimmed();
    const QString configuredOutputPath =
        qEnvironmentVariable(
            "CLASSMNGR_SCHEDULE_CONFLICT_FIXTURE_OUTPUT_PATH"
            ).trimmed();
    QTemporaryDir generatedFixtureDirectory;
    QString path = configuredPath;
    if (path.isEmpty() && !configuredOutputPath.isEmpty())
    {
        QVERIFY(generatedFixtureDirectory.isValid());
        path = writeScheduleConflictWorkbook(&generatedFixtureDirectory);
    }
    if (path.isEmpty())
    {
        path = QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
            QStringLiteral(
                "tests/fixtures/imports/schedule_large_conflict.xlsx"
                )
            );
    }
    if (!QFile::exists(path))
    {
        QSKIP(
            "The permanent schedule conflict fixture is missing; set CLASSMNGR_SCHEDULE_CONFLICT_SAMPLE to validate another workbook."
            );
    }

    ScheduleWidgetTestStubs::setClassRegularHoursEmpty(42, true);
    ApplicationServices services;
    saveSettingOrFail(services.dataService(),
        QStringLiteral("myInfo/name"),
        QStringLiteral("Alice")
        );
    const QString reviewFontFamily = loadReviewFontFamily();
    if (!reviewFontFamily.isEmpty())
    {
        QApplication::setFont(QFont(reviewFontFamily));
    }

    ScheduleImportDialog dialog(&services);
    dialog.setFilePath(path);
    dialog.show();
    QCoreApplication::processEvents();
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
    auto* import =
        review->findChild<QPushButton*>(
            QStringLiteral("scheduleImportAcceptButton")
            );
    QVERIFY(import);
    QTRY_VERIFY(
        review->findChild<QMessageBox*>(
            QStringLiteral("scheduleImportConflictWarning")
            )
        );
    auto* warning =
        review->findChild<QMessageBox*>(
            QStringLiteral("scheduleImportConflictWarning")
            );
    QVERIFY(warning);
    QVERIFY(
        warning->text().contains(
            QStringLiteral("Multiple imported classes are assigned")
            )
        );
    QVERIFY(
        warning->text().contains(QStringLiteral("E4 Hercules"))
        );
    QVERIFY(!import->isEnabled());

    warning->accept();
    QTRY_VERIFY(
        !review->findChild<QMessageBox*>(
            QStringLiteral("scheduleImportConflictWarning")
            )
        );

    const QString screenshotPath =
        qEnvironmentVariable(
            "CLASSMNGR_SCHEDULE_CONFLICT_REVIEW_OUTPUT_PATH"
            ).trimmed();
    if (!screenshotPath.isEmpty())
    {
        QVERIFY2(
            QDir().mkpath(QFileInfo(screenshotPath).absolutePath()),
            qPrintable(
                QStringLiteral(
                    "Could not create schedule conflict screenshot directory for %1"
                    ).arg(screenshotPath)
                )
            );
        review->show();
        QCoreApplication::processEvents();
        const QPixmap screenshot = review->grab();
        QVERIFY(!screenshot.isNull());
        QVERIFY2(
            screenshot.save(screenshotPath, "PNG"),
            qPrintable(
                QStringLiteral(
                    "Could not save schedule conflict screenshot: %1"
                    ).arg(screenshotPath)
                )
            );
    }
}

void ScheduleImportDialogTests::applyUsesConfirmationAndReportsRepositoryOutcome()
{
    const auto expectedSuccessMessage = [](
                                           const int teachersCreated,
                                           const int teachersUpdated,
                                           const int classesCreated,
                                           const int classesUpdated,
                                           const int classesSkipped,
                                           const int schedulesCleared,
                                           const int ignoredCells,
                                           const bool profileNameUpdated
                                           )
    {
        const QString profileNameSuffix = profileNameUpdated
            ? QCoreApplication::translate(
                  "ScheduleImportReviewDialog",
                  "\nMy Information name was updated."
                  )
            : QString();
        return QCoreApplication::translate(
                   "ScheduleImportReviewDialog",
                   "Schedule imported successfully.\n"
                   "Korean teachers created: %1\n"
                   "Korean teacher rooms updated: %2\n"
                   "Classes created: %3\n"
                   "Classes updated: %4\n"
                   "Classes skipped: %5\n"
                   "Schedules cleared: %6\n"
                   "Ignored occupied cells: %7%8"
                   )
            .arg(teachersCreated)
            .arg(teachersUpdated)
            .arg(classesCreated)
            .arg(classesUpdated)
            .arg(classesSkipped)
            .arg(schedulesCleared)
            .arg(ignoredCells)
            .arg(profileNameSuffix);
    };

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    const Status opened = services.openDatabase(
        directory.filePath(QStringLiteral("schedule-import-dialog.sqlite"))
        );
    QVERIFY2(
        opened.has_value(),
        qPrintable(opened.has_value() ? QString() : opened.error())
        );
    const QSqlDatabase database = services.databaseSession()->database();
    QVERIFY(database.isValid());

    QSqlQuery query(database);
    QVERIFY2(
        query.exec(QStringLiteral(
            "CREATE TRIGGER reject_dialog_import_time "
            "BEFORE INSERT ON class_times BEGIN "
            "SELECT RAISE(ABORT, 'injected repository write failure'); END"
            )),
        qPrintable(query.lastError().text())
        );

    const auto persistedRowCounts = [&database]()
    {
        QSqlQuery snapshot(database);
        if (!snapshot.exec(QStringLiteral(
                "SELECT "
                "(SELECT COUNT(*) FROM teachers), "
                "(SELECT COUNT(*) FROM classes), "
                "(SELECT COUNT(*) FROM class_info), "
                "(SELECT COUNT(*) FROM class_times), "
                "(SELECT COUNT(*) FROM app_settings)"
                ))
            || !snapshot.next())
        {
            return QStringList{
                QStringLiteral("snapshot query failed: %1")
                    .arg(snapshot.lastError().text())
            };
        }

        QStringList counts;
        for (int column = 0; column < 5; ++column)
        {
            counts.append(snapshot.value(column).toString());
        }
        return counts;
    };
    const auto persistedProfileNames = [&database]()
    {
        QSqlQuery profileNameQuery(database);
        if (!profileNameQuery.exec(QStringLiteral(
                "SELECT value FROM app_settings WHERE key='myInfo/name'"
                )))
        {
            return QStringList{
                QStringLiteral("profile name query failed: %1")
                    .arg(profileNameQuery.lastError().text())
            };
        }

        QStringList names;
        while (profileNameQuery.next())
        {
            names.append(profileNameQuery.value(0).toString());
        }
        return names;
    };
    const QStringList emptyDatabaseCounts = {
        QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0"),
        QStringLiteral("0"), QStringLiteral("0")
    };
    QCOMPARE(persistedRowCounts(), emptyDatabaseCounts);

    ScheduleImportReviewRequest request;
    request.kind = ScheduleImportKind::Normal;
    request.user.name = QStringLiteral("Alice");
    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("\uBC15\uC120\uC0DD");
    candidate.teacherKr = candidate.teacherKey;
    candidate.rooms = {QStringLiteral("413")};
    candidate.classGrade = QStringLiteral("E4");
    candidate.classLevel = QStringLiteral("Hercules");
    candidate.times = {
        {QStringLiteral("Monday"), QStringLiteral("4:00 PM"), QStringLiteral("4:50 PM")},
        {QStringLiteral("Wednesday"), QStringLiteral("4:00 PM"), QStringLiteral("4:50 PM")}
    };
    request.user.classes = {candidate};

    FakeUserPromptService prompts;
    ScopedTestUserPromptService promptService(&prompts);
    ScheduleImportReviewDialog review(&services, request);
    QVERIFY(review.prepare());
    review.show();
    auto* tabs = review.findChild<QTabWidget*>(
        QStringLiteral("scheduleImportResolutionTabs")
        );
    auto* teacherScrollArea = review.findChild<QScrollArea*>(
        QStringLiteral("scheduleImportTeacherScrollArea")
        );
    auto* classScrollArea = review.findChild<QScrollArea*>(
        QStringLiteral("scheduleImportClassScrollArea")
        );
    auto* apply = review.findChild<QPushButton*>(
        QStringLiteral("scheduleImportAcceptButton"));
    QVERIFY(tabs);
    QVERIFY(teacherScrollArea);
    QVERIFY(classScrollArea);
    QVERIFY(apply);

    tabs->setCurrentWidget(teacherScrollArea);
    QTRY_VERIFY(
        teacherScrollArea->viewport()->width() > 0
        && teacherScrollArea->viewport()->height() > 0
        );
    const auto findTeacherAction = [&review]()
    {
        return review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportTeacherAction_0")
            );
    };
    const auto findTeacherRoom = [&review]()
    {
        return review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportTeacherRoom_0")
            );
    };
    QTRY_VERIFY(
        findTeacherAction()
        && findTeacherAction()->isVisible()
        && findTeacherRoom()
        && findTeacherRoom()->isVisible()
        );
    auto* teacher = findTeacherAction();
    auto* teacherRoom = findTeacherRoom();
    int createTeacherIndex = -1;
    QStringList teacherOptions;
    for (int index = 0; index < teacher->count(); ++index)
    {
        const int action = teacher->itemData(index, Qt::UserRole).toInt();
        teacherOptions.append(
            QStringLiteral("%1 (%2)")
                .arg(teacher->itemText(index))
                .arg(action)
            );
        if (action
            == static_cast<int>(ScheduleImportTeacherAction::Create))
        {
            createTeacherIndex = index;
            break;
        }
    }
    QVERIFY2(createTeacherIndex >= 0, qPrintable(teacherOptions.join("; ")));
    teacher->setCurrentIndex(createTeacherIndex);
    QCOMPARE(teacherRoom->currentData().toString(), QStringLiteral("413"));

    tabs->setCurrentWidget(classScrollArea);
    QTRY_VERIFY(
        classScrollArea->viewport()->width() > 0
        && classScrollArea->viewport()->height() > 0
        );
    const auto findClassroom = [&review]()
    {
        return review.findChild<QComboBox*>(
            QStringLiteral("scheduleImportClassAction_0")
            );
    };
    QTRY_VERIFY(findClassroom() && findClassroom()->isVisible());
    auto* classroom = findClassroom();
    const int createIndex = actionIndex(classroom, ScheduleImportClassAction::CreateNew);
    QVERIFY(createIndex >= 0);
    classroom->setCurrentIndex(createIndex);
    QVERIFY(apply->isEnabled());
    const QStringList beforeApplyCounts = persistedRowCounts();
    const QStringList emptyScheduleTableCounts = {
        QStringLiteral("0"), QStringLiteral("0"),
        QStringLiteral("0"), QStringLiteral("0")
    };
    QCOMPARE(beforeApplyCounts.mid(0, 4), emptyScheduleTableCounts);
    const QStringList profileNamesBeforeApply = persistedProfileNames();

    prompts.scriptedChoices.enqueue(PromptChoice::Rejected);
    apply->click();
    QCOMPARE(prompts.confirmations.size(), 1);
    QCOMPARE(prompts.messages.size(), 0);
    QCOMPARE(review.result(), 0);
    QCOMPARE(persistedRowCounts(), beforeApplyCounts);
    QCOMPARE(persistedProfileNames(), profileNamesBeforeApply);

    prompts.scriptedChoices.enqueue(PromptChoice::Accepted);
    apply->click();
    QCOMPARE(prompts.confirmations.size(), 2);
    QCOMPARE(prompts.messages.size(), 1);
    QVERIFY(prompts.messages.constLast().message.contains(
        QStringLiteral("injected repository write failure")));
    QCOMPARE(review.result(), 0);
    QCOMPARE(persistedRowCounts(), beforeApplyCounts);
    QCOMPARE(persistedProfileNames(), profileNamesBeforeApply);

    QVERIFY2(
        query.exec(QStringLiteral("DROP TRIGGER reject_dialog_import_time")),
        qPrintable(query.lastError().text())
        );
    prompts.scriptedChoices.enqueue(PromptChoice::Accepted);
    apply->click();
    QCOMPARE(prompts.confirmations.size(), 3);
    QCOMPARE(prompts.messages.size(), 2);
    QCOMPARE(
        prompts.messages.constLast().message,
        expectedSuccessMessage(1, 0, 1, 0, 0, 1, 0, true)
        );
    QCOMPARE(review.result(), static_cast<int>(QDialog::Accepted));
    const QStringList importedDatabaseCounts = {
        QStringLiteral("1"), QStringLiteral("1"), QStringLiteral("1"),
        QStringLiteral("2"),
        QString::number(
            beforeApplyCounts.at(4).toInt()
            + (profileNamesBeforeApply.isEmpty() ? 1 : 0)
            )
    };
    QCOMPARE(persistedRowCounts(), importedDatabaseCounts);

    QSqlQuery schedule(database);
    QVERIFY2(
        schedule.exec(QStringLiteral(
            "SELECT teachers.teacher_kr, teachers.room_number, classes.name, "
            "class_info.class_grade, class_info.class_level, "
            "class_times.day, class_times.start_time, class_times.end_time "
            "FROM teachers "
            "JOIN class_info ON class_info.teacher_id=teachers.id "
            "JOIN classes ON classes.id=class_info.class_id "
            "JOIN class_times ON class_times.class_id=classes.id "
            "ORDER BY class_times.day"
            )),
        qPrintable(schedule.lastError().text())
        );
    QVERIFY(schedule.next());
    QCOMPARE(schedule.value(0).toString(), candidate.teacherKr);
    QCOMPARE(schedule.value(1).toString(), QStringLiteral("413"));
    QCOMPARE(schedule.value(2).toString(), QStringLiteral("E4 Hercules"));
    QCOMPARE(schedule.value(3).toString(), candidate.classGrade);
    QCOMPARE(schedule.value(4).toString(), candidate.classLevel);
    QCOMPARE(schedule.value(5).toString(), QStringLiteral("Monday"));
    QCOMPARE(schedule.value(6).toString(), QStringLiteral("4:00 PM"));
    QCOMPARE(schedule.value(7).toString(), QStringLiteral("4:50 PM"));
    QVERIFY(schedule.next());
    QCOMPARE(schedule.value(0).toString(), candidate.teacherKr);
    QCOMPARE(schedule.value(5).toString(), QStringLiteral("Wednesday"));
    QCOMPARE(schedule.value(6).toString(), QStringLiteral("4:00 PM"));
    QCOMPARE(schedule.value(7).toString(), QStringLiteral("4:50 PM"));
    QVERIFY(!schedule.next());

    const QStringList importedProfileName = {QStringLiteral("Alice")};
    QCOMPARE(persistedProfileNames(), importedProfileName);

    ApplicationServices unchangedProfileServices;
    const Status unchangedProfileOpened =
        unchangedProfileServices.openDatabase(
            directory.filePath(
                QStringLiteral("schedule-import-profile-unchanged.sqlite")
                )
            );
    QVERIFY2(
        unchangedProfileOpened.has_value(),
        qPrintable(
            unchangedProfileOpened.has_value()
                ? QString()
                : unchangedProfileOpened.error()
            )
        );
    const QSqlDatabase unchangedProfileDatabase =
        unchangedProfileServices.databaseSession()->database();

    ScheduleImportReviewRequest profileNameUnchangedRequest;
    profileNameUnchangedRequest.kind = ScheduleImportKind::Normal;
    profileNameUnchangedRequest.profileName = QStringLiteral("Existing Profile");
    profileNameUnchangedRequest.user.name = QStringLiteral("Bob");
    profileNameUnchangedRequest.user.classes = {candidate};

    ScheduleImportReviewDialog profileNameUnchangedReview(
        &unchangedProfileServices,
        profileNameUnchangedRequest
        );
    QVERIFY(profileNameUnchangedReview.prepare());
    profileNameUnchangedReview.show();
    auto* secondTabs = profileNameUnchangedReview.findChild<QTabWidget*>(
        QStringLiteral("scheduleImportResolutionTabs")
        );
    auto* secondTeacherScrollArea =
        profileNameUnchangedReview.findChild<QScrollArea*>(
            QStringLiteral("scheduleImportTeacherScrollArea")
            );
    auto* secondClassScrollArea =
        profileNameUnchangedReview.findChild<QScrollArea*>(
            QStringLiteral("scheduleImportClassScrollArea")
            );
    auto* secondApply = profileNameUnchangedReview.findChild<QPushButton*>(
        QStringLiteral("scheduleImportAcceptButton")
        );
    auto* secondReviewStatus = profileNameUnchangedReview.findChild<QLabel*>(
        QStringLiteral("scheduleImportReviewStatus")
        );
    QVERIFY(secondTabs);
    QVERIFY(secondTeacherScrollArea);
    QVERIFY(secondClassScrollArea);
    QVERIFY(secondApply);
    QVERIFY(secondReviewStatus);

    secondTabs->setCurrentWidget(secondTeacherScrollArea);
    QTRY_VERIFY(
        secondTeacherScrollArea->viewport()->width() > 0
        && secondTeacherScrollArea->viewport()->height() > 0
        );
    auto* secondTeacher = profileNameUnchangedReview.findChild<QComboBox*>(
        QStringLiteral("scheduleImportTeacherAction_0")
        );
    auto* secondTeacherRoom = profileNameUnchangedReview.findChild<QComboBox*>(
        QStringLiteral("scheduleImportTeacherRoom_0")
        );
    QTRY_VERIFY(secondTeacher && secondTeacher->isVisible());
    QTRY_VERIFY(secondTeacherRoom && secondTeacherRoom->isVisible());
    const int secondCreateTeacherIndex = secondTeacher->findData(
        static_cast<int>(ScheduleImportTeacherAction::Create),
        Qt::UserRole
        );
    QVERIFY(secondCreateTeacherIndex >= 0);
    secondTeacher->setCurrentIndex(secondCreateTeacherIndex);
    QCOMPARE(secondTeacherRoom->currentData().toString(), QStringLiteral("413"));

    secondTabs->setCurrentWidget(secondClassScrollArea);
    QTRY_VERIFY(
        secondClassScrollArea->viewport()->width() > 0
        && secondClassScrollArea->viewport()->height() > 0
        );
    auto* secondClassroom = profileNameUnchangedReview.findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_0")
        );
    QTRY_VERIFY(secondClassroom && secondClassroom->isVisible());
    const int secondCreateClassIndex = actionIndex(
        secondClassroom,
        ScheduleImportClassAction::CreateNew
        );
    QVERIFY(secondCreateClassIndex >= 0);
    secondClassroom->setCurrentIndex(secondCreateClassIndex);
    QVERIFY2(
        secondApply->isEnabled(),
        qPrintable(secondReviewStatus->text())
        );

    QSqlQuery seedUnchangedProfileName(unchangedProfileDatabase);
    QVERIFY2(
        seedUnchangedProfileName.exec(QStringLiteral(
            "INSERT INTO app_settings (key, value) "
            "VALUES ('myInfo/name', 'Existing Profile')"
            )),
        qPrintable(seedUnchangedProfileName.lastError().text())
        );
    {
        QSqlQuery unchangedProfileNameBeforeApply(unchangedProfileDatabase);
        QVERIFY2(
            unchangedProfileNameBeforeApply.exec(QStringLiteral(
                "SELECT value FROM app_settings WHERE key='myInfo/name'"
                )),
            qPrintable(unchangedProfileNameBeforeApply.lastError().text())
            );
        QVERIFY(unchangedProfileNameBeforeApply.next());
        QCOMPARE(
            unchangedProfileNameBeforeApply.value(0).toString(),
            QStringLiteral("Existing Profile")
            );
        QVERIFY(!unchangedProfileNameBeforeApply.next());
    }

    prompts.scriptedChoices.enqueue(PromptChoice::Accepted);
    secondApply->click();
    QCOMPARE(prompts.confirmations.size(), 4);
    QCOMPARE(prompts.messages.size(), 3);
    QCOMPARE(
        prompts.messages.constLast().message,
        expectedSuccessMessage(1, 0, 1, 0, 0, 1, 0, false)
        );
    QCOMPARE(
        profileNameUnchangedReview.result(),
        static_cast<int>(QDialog::Accepted)
        );
    QSqlQuery unchangedProfileNameQuery(
        unchangedProfileDatabase
        );
    QVERIFY2(
        unchangedProfileNameQuery.exec(QStringLiteral(
            "SELECT value FROM app_settings WHERE key='myInfo/name'"
            )),
        qPrintable(unchangedProfileNameQuery.lastError().text())
        );
    QVERIFY(unchangedProfileNameQuery.next());
    QCOMPARE(
        unchangedProfileNameQuery.value(0).toString(),
        QStringLiteral("Existing Profile")
        );
    QVERIFY(!unchangedProfileNameQuery.next());
    QCOMPARE(
        persistedProfileNames(),
        importedProfileName
        );
}

void ScheduleImportDialogTests::applyDisplaysFreshStateValidationFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    const Status opened = services.openDatabase(
        directory.filePath(QStringLiteral("schedule-import-state-error.sqlite"))
        );
    QVERIFY2(
        opened.has_value(),
        qPrintable(opened.has_value() ? QString() : opened.error())
        );

    const Result<Teacher> teacher = services.teacherService()->teacher(7);
    QVERIFY(teacher);
    ScheduleImportReviewRequest request = classSkipRequest(*teacher);

    struct LateClassMutationPrompt final : IUserPromptService
    {
        void showMessage(const PromptRequest& request) override
        {
            messages.push_back(request);
        }

        void showMessageAsync(const PromptRequest&) override
        {
        }

        PromptChoice confirm(const PromptRequest& request) override
        {
            confirmations.push_back(request);
            ScheduleWidgetTestStubs::setClassGrade(42, QStringLiteral("E5"));
            return PromptChoice::Accepted;
        }

        UnsavedChangesChoice confirmUnsavedChanges(
            const UnsavedChangesRequest&
            ) override
        {
            return UnsavedChangesChoice::Cancel;
        }

        QString chooseAction(const ActionPromptRequest&) override
        {
            return QString();
        }

        QVector<PromptRequest> messages;
        QVector<PromptRequest> confirmations;
    } prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    ScheduleImportReviewDialog review(&services, request);
    QVERIFY(review.prepare());
    review.show();
    QVERIFY(review.isVisible());
    auto* classAction = review.findChild<QComboBox*>(
        QStringLiteral("scheduleImportClassAction_0"));
    auto* apply = review.findChild<QPushButton*>(
        QStringLiteral("scheduleImportAcceptButton"));
    QVERIFY(classAction && apply);

    const int skipIndex = actionIndex(
        classAction,
        ScheduleImportClassAction::Skip
        );
    QVERIFY(skipIndex >= 0);
    classAction->setItemData(skipIndex, 42, Qt::UserRole + 1);
    classAction->setCurrentIndex(skipIndex);
    QVERIFY(apply->isEnabled());

    apply->click();

    QCOMPARE(prompts.confirmations.size(), 1);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constLast().message,
        QStringLiteral(
            "A skipped imported class can preserve only its unique exact existing match."
            )
        );
    QVERIFY(review.isVisible());
    QCOMPARE(review.result(), 0);
}

void ScheduleImportDialogTests::policyFailureMessageRetainsLegacyText()
{
    ScheduleImportPlan legacyPlan;
    legacyPlan.diagnostics.append(ScheduleImportDiagnostic{});
    const auto legacyValidation = ScheduleImportPlanValidator::validate(legacyPlan);
    QVERIFY(!legacyValidation);

    ImportState::ScheduleImportApplyRequest request;
    request.diagnostics.push_back({});
    const ImportState::ScheduleImportPlanEligibilityIssue issue{
        ImportState::ScheduleImportPlanEligibilityIssueCode::UnacknowledgedDiagnostics
    };
    QCOMPARE(
        ScheduleImportPlanValidator::policyFailureMessage(request, issue),
        legacyValidation.error()
        );
}

void ScheduleImportDialogTests::
    featureValidatorMatchesQStringTeacherKeyConstruction()
{
    ScheduleImportPlan plan;
    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("\uAE40\uC120\uC0DD");
    candidate.teacherKr = candidate.teacherKey;
    candidate.rooms = {QStringLiteral("414")};
    candidate.classGrade = QStringLiteral("E4");
    candidate.classLevel = QStringLiteral("Hercules");
    candidate.times = {
        {QStringLiteral("Monday"), QStringLiteral("4:00 PM"), QStringLiteral("4:50 PM")},
        {QStringLiteral("Wednesday"), QStringLiteral("4:00 PM"), QStringLiteral("4:50 PM")}
    };
    plan.candidates = {candidate};
    plan.teachers = {{
        candidate.teacherKey,
        ScheduleImportTeacherAction::Reuse,
        17,
        QString()
    }};
    plan.classes = {{
        0,
        ScheduleImportClassAction::CreateNew,
        -1,
        QStringLiteral("#AABBCC"),
        QStringLiteral("#000000")
    }};

    const auto validated = ScheduleImportPlanValidator::validate(plan);
    QVERIFY2(
        validated.has_value(),
        qPrintable(validated.has_value() ? QString() : validated.error())
        );
    QCOMPARE(
        validated->teacherResolutions.value(candidate.teacherKey).teacherKey,
        candidate.teacherKey
        );
}

void ScheduleImportDialogTests::reviewModelBuildsTypedApplyRequest()
{
    ScheduleImportReviewContext context;
    context.kind = ScheduleImportKind::Intensive;
    context.intensiveMode = ScheduleImportIntensiveMode::ReplaceWithNew;
    context.selectedUserName = QStringLiteral("Alice");
    context.saveProfileNameIfBlank = true;
    context.updateProfileName = true;
    context.unknownCellsAcknowledged = true;
    ScheduleImportClassCandidate candidate;
    candidate.teacherKey = QStringLiteral("Kim");
    candidate.teacherKr = QStringLiteral("Kim");
    candidate.rooms = {QStringLiteral(" 413 ")};
    candidate.importedColors = {QStringLiteral("#123456")};
    candidate.classGrade = QStringLiteral("E4");
    candidate.classLevel = QStringLiteral("Hercules");
    candidate.times = {{QStringLiteral("Monday"), QStringLiteral("4:00 PM"),
        QStringLiteral("4:50 PM")}};
    candidate.sourceCells = {QStringLiteral("B12")};
    candidate.meetingPatternError = QStringLiteral("pattern");
    context.candidates = {candidate};
    context.intensiveSlotStates = {{QStringLiteral("Tuesday"),
        QStringLiteral("5:00 PM"), QStringLiteral("Occupied")}};
    context.diagnostics = {{QStringLiteral("Sheet"), QStringLiteral("Alice"),
        QStringLiteral("B12"), QStringLiteral("?"), QStringLiteral("Ignored")}};

    QList<ScheduleImportTeacherResolution> teachers{{
        QStringLiteral("Kim"), ScheduleImportTeacherAction::UpdateRoom, 17,
        QStringLiteral(" 413 ")
    }};
    QList<ScheduleImportClassResolution> classes{{
        0, ScheduleImportClassAction::UpdateExisting, 42,
        QStringLiteral(" #FFFFFF "), QStringLiteral(" #000000 ")
    }};

    const auto request = ScheduleImportReviewModel::buildApplyRequest(
        context, teachers, classes);
    QVERIFY(request.intensiveSchedule);
    QCOMPARE(request.intensiveMode,
        ImportState::ScheduleImportPlanIntensiveMode::ReplaceWithNew);
    QCOMPARE(request.selectedUserName, std::u16string(u"Alice"));
    QVERIFY(request.saveProfileNameIfBlank && request.updateProfileName
        && request.diagnosticsAcknowledged);
    QCOMPARE(request.candidates.size(), std::size_t(1));
    QCOMPARE(
        request.candidates.at(0).teacherKey.value(),
        std::u16string(u"Kim")
        );
    QCOMPARE(request.candidates.at(0).teacherName, std::u16string(u"Kim"));
    QCOMPARE(request.candidates.at(0).rooms.at(0), std::u16string(u"413"));
    QCOMPARE(request.candidates.at(0).importedColors.at(0),
        std::u16string(u"#123456"));
    QCOMPARE(request.candidates.at(0).grade, std::u16string(u"E4"));
    QCOMPARE(request.candidates.at(0).level, std::u16string(u"Hercules"));
    QCOMPARE(request.candidates.at(0).times.at(0).day, std::u16string(u"Monday"));
    QCOMPARE(request.candidates.at(0).times.at(0).startTime,
        std::u16string(u"4:00 PM"));
    QCOMPARE(request.candidates.at(0).sourceCells.at(0), std::u16string(u"B12"));
    QCOMPARE(request.candidates.at(0).meetingPatternError, std::u16string(u"pattern"));
    QCOMPARE(request.intensiveSlotStates.at(0).day, std::u16string(u"Tuesday"));
    QCOMPARE(request.diagnostics.at(0).cellReference, std::u16string(u"B12"));
    QCOMPARE(request.teachers.at(0).action,
        ImportState::ScheduleImportReviewTeacherAction::UpdateRoom);
    QVERIFY(request.teachers.at(0).targetTeacherId.has_value());
    QCOMPARE(request.teachers.at(0).targetTeacherId->value(), std::string("17"));
    QCOMPARE(request.teachers.at(0).selectedRoom, std::u16string(u"413"));
    QCOMPARE(request.classes.at(0).action,
        ImportState::ScheduleImportReviewClassAction::UpdateExisting);
    QVERIFY(request.classes.at(0).targetClassId.has_value());
    QCOMPARE(request.classes.at(0).targetClassId->value(), std::string("42"));
    QCOMPARE(request.classes.at(0).classColor, std::u16string(u"#FFFFFF"));
    QCOMPARE(request.classes.at(0).fontColor, std::u16string(u"#000000"));
}

QTEST_MAIN(ScheduleImportDialogTests)

#include "schedule_import_dialog_tests.moc"
