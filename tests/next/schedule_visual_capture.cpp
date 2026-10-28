#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/process_memory_snapshot.h"
#include "data/database/workspace_store.h"
#include "features/schedule/ui/schedule_widget.h"
#include "features/schedule/services/schedule_print_service.h"
#include "features/schedule/ui/testing_assignment_dialog.h"
#include "next/platform/workspace_store_schedule_actions.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/page_host.h"
#include "next/ui/schedule_intensive_cell_layout.h"
#include "next/ui/schedule_output_dialog.h"
#include "next/ui/schedule_page.h"
#include "next/ui/schedule_testing_assignment_dialog.h"
#include "preferences_test_ports.h"
#include "sql_script_runner.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QHeaderView>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPageLayout>
#include <QPalette>
#include <QPainter>
#include <QPdfDocument>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegion>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTranslator>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr;
using namespace ClassMngr::Next;

namespace
{

constexpr QSize Viewport{1024, 768};

QPalette themePalette(const QString& theme)
{
    QPalette palette;
    const bool dark = theme == QStringLiteral("dark");
    palette.setColor(QPalette::Window,
        QColor(dark ? QStringLiteral("#202326") : QStringLiteral("#f5f6f7")));
    palette.setColor(QPalette::WindowText,
        QColor(dark ? QStringLiteral("#f1f3f4") : QStringLiteral("#202326")));
    palette.setColor(QPalette::Base,
        QColor(dark ? QStringLiteral("#292c30") : QStringLiteral("#ffffff")));
    palette.setColor(QPalette::AlternateBase,
        QColor(dark ? QStringLiteral("#30343a") : QStringLiteral("#f4f6f8")));
    palette.setColor(QPalette::Button,
        QColor(dark ? QStringLiteral("#292c30") : QStringLiteral("#fcfcfc")));
    palette.setColor(QPalette::ButtonText,
        QColor(dark ? QStringLiteral("#fcfcfc") : QStringLiteral("#232629")));
    palette.setColor(QPalette::Text,
        QColor(dark ? QStringLiteral("#f1f3f4") : QStringLiteral("#202326")));
    palette.setColor(QPalette::Mid,
        QColor(dark ? QStringLiteral("#4b5056") : QStringLiteral("#b0b5bb")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#3daee9")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    return palette;
}

QString readText(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly | QIODevice::Text)
        ? QString::fromUtf8(file.readAll()) : QString{};
}

QJsonObject memoryJson(const ProcessMemorySnapshot& snapshot)
{
    return {
        {QStringLiteral("available"), snapshot.isAvailable},
        {QStringLiteral("platform"), snapshot.platform},
        {QStringLiteral("workingSetBytes"),
         static_cast<double>(snapshot.workingSetBytes)},
        {QStringLiteral("peakWorkingSetBytes"),
         static_cast<double>(snapshot.peakWorkingSetBytes)},
        {QStringLiteral("privateUsageBytes"),
         static_cast<double>(snapshot.privateUsageBytes)},
        {QStringLiteral("privateWorkingSetBytes"),
         static_cast<double>(snapshot.privateWorkingSetBytes)},
        {QStringLiteral("capturedAt"),
         snapshot.capturedAt.toString(Qt::ISODateWithMs)}
    };
}

QJsonArray rowHeights(const QTableView& table)
{
    QJsonArray result;
    for (int row = 0; row < table.model()->rowCount(); ++row)
    {
        result.append(table.rowHeight(row));
    }
    return result;
}

int maximumRowHeight(const QTableView& table)
{
    int maximum = 0;
    for (int row = 0; row < table.model()->rowCount(); ++row)
    {
        maximum = std::max(maximum, table.rowHeight(row));
    }
    return maximum;
}

class CaptureTestingClassChoicesPort final
    : public Application::ScheduleTestingClassChoicesReadPort
{
public:
    Application::ScheduleTestingClassChoicesReadResult readTestingClassChoices(
        const Application::ScheduleTestingClassChoicesReadQuery&
        ) const override
    {
        Application::ScheduleTestingClassChoicesSnapshot result;
        const auto classId = Next::Domain::ClassId::fromString("701");
        if (classId)
        {
            result.choices.push_back({
                .classId = *classId,
                .name = u"Writing Lab",
                .grade = u"M2",
                .level = u"Mixed (All)",
                .room = u"Library"
            });
        }
        return Application::ScheduleTestingClassChoicesReadResult::success(
            std::move(result));
    }
};

bool saveDialogCapture(
    QDialog& dialog,
    const QString& path,
    QJsonObject* state,
    QString* error
    )
{
    dialog.show();
    QApplication::processEvents();
    const QImage image = dialog.grab().toImage();
    dialog.hide();
    if (image.isNull() || !image.save(path, "PNG"))
    {
        *error = QStringLiteral("Could not capture dialog: %1")
            .arg(QFileInfo(path).fileName());
        return false;
    }
    *state = {
        {QStringLiteral("image"), QFileInfo(path).fileName()},
        {QStringLiteral("width"), image.width()},
        {QStringLiteral("height"), image.height()}
    };
    return true;
}

QString compactOutputText(QString text)
{
    text.remove(QRegularExpression(QStringLiteral("\\s+")));
    return text;
}

bool captureScheduleTestingOutputEvidence(
    QApplication& application,
    const QString& outputDirectory,
    const QString& translationDirectory,
    const QString& stylesheetDirectory,
    QString* error
    )
{
    if (!QDir().mkpath(outputDirectory))
    {
        *error = QStringLiteral("Could not create Schedule output capture directory.");
        return false;
    }

    CaptureTestingClassChoicesPort choicesPort;
    const Application::ScheduleTestingClassChoicesSnapshot choices =
        choicesPort.readTestingClassChoices({}).value();
    QJsonArray dialogCaptures;
    for (const QString& locale : {QStringLiteral("en_US"), QStringLiteral("ko_KR")})
    {
        for (const QString& theme : {QStringLiteral("light"), QStringLiteral("dark")})
        {
            QLocale::setDefault(QLocale(locale));
            application.setPalette(themePalette(theme));
            application.setStyleSheet(readText(QDir(stylesheetDirectory)
                .filePath(theme + QStringLiteral(".qss"))));

            std::unique_ptr<QTranslator> translator;
            if (locale == QStringLiteral("ko_KR"))
            {
                translator = std::make_unique<QTranslator>();
                if (!translator->load(QDir(translationDirectory).filePath(
                        QStringLiteral("ClassMngr_ko_KR.qm")))
                    || !application.installTranslator(translator.get()))
                {
                    *error = QStringLiteral("Could not install Korean Schedule output translation.");
                    return false;
                }
            }

            QComboBox* legacyMode = nullptr;
            TestingAssignmentDialog legacyDialog(
                choicesPort,
                nullptr
                );
            legacyMode = legacyDialog.findChild<QComboBox*>(
                QStringLiteral("testingAssignmentModeCombo"));
            if (!legacyMode)
            {
                *error = QStringLiteral("Legacy assignment editor mode control was not found.");
                return false;
            }
            legacyMode->setCurrentIndex(1);
            const QString legacyPath = QDir(outputDirectory).filePath(
                QStringLiteral("legacy_assignment_%1_%2.png").arg(locale, theme));
            QJsonObject legacyCapture;
            if (!saveDialogCapture(legacyDialog, legacyPath, &legacyCapture, error))
            {
                return false;
            }
            legacyCapture.insert(QStringLiteral("route"), QStringLiteral("Legacy editor"));
            legacyCapture.insert(QStringLiteral("locale"), locale);
            legacyCapture.insert(QStringLiteral("theme"), theme);
            dialogCaptures.append(legacyCapture);

            Ui::ScheduleTestingAssignmentDialog nextDialog(
                choices,
                false,
                QString(),
                std::nullopt
                );
            QComboBox* const nextMode = nextDialog.findChild<QComboBox*>(
                QStringLiteral("nextTestingAssignmentMode"));
            if (!nextMode)
            {
                *error = QStringLiteral("Next assignment editor mode control was not found.");
                return false;
            }
            nextMode->setCurrentIndex(1);
            const QString nextPath = QDir(outputDirectory).filePath(
                QStringLiteral("next_assignment_%1_%2.png").arg(locale, theme));
            QJsonObject nextCapture;
            if (!saveDialogCapture(nextDialog, nextPath, &nextCapture, error))
            {
                return false;
            }
            nextCapture.insert(QStringLiteral("route"), QStringLiteral("Next editor"));
            nextCapture.insert(QStringLiteral("locale"), locale);
            nextCapture.insert(QStringLiteral("theme"), theme);
            dialogCaptures.append(nextCapture);

            Ui::ScheduleOutputDialog outputDialog(
                [](QWidget*) -> std::optional<QString> { return std::nullopt; });
            const QString outputPath = QDir(outputDirectory).filePath(
                QStringLiteral("next_output_options_%1_%2.png").arg(locale, theme));
            QJsonObject outputCapture;
            if (!saveDialogCapture(outputDialog, outputPath, &outputCapture, error))
            {
                return false;
            }
            outputCapture.insert(QStringLiteral("route"), QStringLiteral("Next output options"));
            outputCapture.insert(QStringLiteral("locale"), locale);
            outputCapture.insert(QStringLiteral("theme"), theme);
            dialogCaptures.append(outputCapture);

            if (translator)
            {
                application.removeTranslator(translator.get());
            }
        }
    }

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        *error = QStringLiteral("Could not create temporary Schedule PDF capture directory.");
        return false;
    }
    SchedulePrintService::Request request;
    request.document.days = {QStringLiteral("Monday"), QStringLiteral("Tuesday")};
    request.document.summary = {.essayBlocks = 1, .scheduledBlocks = 2};
    SchedulePrintService::Row row;
    row.timeLabel = QStringLiteral("17:00");
    row.timeRangeLabel = QStringLiteral("5:00 - 5:50");
    SchedulePrintService::Cell oral;
    oral.day = request.document.days.at(0);
    oral.timeLabel = row.timeLabel;
    oral.slotState = SchedulePrintService::SlotState::Testing;
    oral.testingRoom = QStringLiteral("Library");
    SchedulePrintService::Cell classCard;
    classCard.day = request.document.days.at(1);
    classCard.timeLabel = row.timeLabel;
    classCard.entries.append({
        .className = QStringLiteral("Writing Lab"),
        .teacherKoreanName = QStringLiteral("한 선생님"),
        .teacherEnglishName = QStringLiteral("Ms Han"),
        .teacherPreferredName = QStringLiteral("Han"),
        .room = QStringLiteral("Library"),
        .grade = QStringLiteral("M2"),
        .level = QStringLiteral("Mixed (All)"),
        .classColor = QStringLiteral("#C9D8A6"),
        .fontColor = QStringLiteral("#000000"),
        .testingClass = true
    });
    row.cells = {oral, classCard};
    request.document.rows.append(row);
    request.style = SchedulePrintStyle::LightTheme;
    request.currentTheme = Theme::Dark;
    request.englishFont = QFont(QStringLiteral("Inter"));
    request.koreanFont = QFont(QStringLiteral("Pretendard"));
    request.userName = QStringLiteral("Warren");
    request.showEnglishNames = true;
    request.pageOrientation = QPageLayout::Landscape;

    const QString pdfPath = temporaryDirectory.filePath(
        QStringLiteral("schedule-testing-output.pdf"));
    const auto pdfResult = SchedulePrintService::saveSchedulePdf(request, pdfPath);
    if (pdfResult.status != SchedulePrintService::Status::Sent)
    {
        *error = QStringLiteral("Could not generate representative Schedule PDF: %1")
            .arg(pdfResult.message);
        return false;
    }
    QPdfDocument pdf;
    if (pdf.load(pdfPath) != QPdfDocument::Error::None || pdf.pageCount() != 1)
    {
        *error = QStringLiteral("Could not read representative Schedule PDF.");
        return false;
    }
    const QString pdfText = compactOutputText(pdf.getAllText(0).text());
    const QStringList expectedText{
        QStringLiteral("OralTesting"),
        QStringLiteral("Library"),
        QStringLiteral("WritingLab"),
        QStringLiteral("M2"),
        QStringLiteral("Mixed(All)")
    };
    for (const QString& expected : expectedText)
    {
        if (!pdfText.contains(expected))
        {
            *error = QStringLiteral("Representative PDF is missing output content: %1")
                .arg(expected);
            return false;
        }
    }
    const QSizeF pageSize = pdf.pagePointSize(0);
    const QSize imageSize(
        qRound(pageSize.width() * 150.0 / 72.0),
        qRound(pageSize.height() * 150.0 / 72.0));
    const QImage pdfImage = pdf.render(0, imageSize);
    const QString pdfImagePath = QDir(outputDirectory).filePath(
        QStringLiteral("representative_testing_output_pdf.png"));
    if (pdfImage.isNull() || !pdfImage.save(pdfImagePath, "PNG"))
    {
        *error = QStringLiteral("Could not save representative Schedule PDF image.");
        return false;
    }

    QJsonArray handoffImages;
    const QString handoffDirectory = QDir(outputDirectory).absoluteFilePath(
        QStringLiteral("../testing-classes"));
    for (const QString& locale : {QStringLiteral("en_US"), QStringLiteral("ko_KR")})
    {
        for (const QString& theme : {QStringLiteral("light"), QStringLiteral("dark")})
        {
            for (const QString& route : {QStringLiteral("legacy"), QStringLiteral("next")})
            {
                const QString fileName = QStringLiteral(
                    "testing_classes_pending_slot_%1_%2_%3.png")
                    .arg(locale, theme, route);
                if (!QFileInfo::exists(QDir(handoffDirectory).filePath(fileName)))
                {
                    *error = QStringLiteral("Existing #28 handoff capture is missing: %1")
                        .arg(fileName);
                    return false;
                }
                handoffImages.append(QJsonObject{
                    {QStringLiteral("image"), QStringLiteral("../testing-classes/") + fileName},
                    {QStringLiteral("route"), route},
                    {QStringLiteral("locale"), locale},
                    {QStringLiteral("theme"), theme}
                });
            }
        }
    }

    const QJsonObject manifest{
        {QStringLiteral("taskId"), QStringLiteral("phase7-c32-testing-assignments-output-20261028")},
        {QStringLiteral("dialogViewport"), QStringLiteral("Native dialog bounds captured with Qt Fusion and explicit light/dark application palettes")},
        {QStringLiteral("assignmentEditorsAndOutputOptions"), dialogCaptures},
        {QStringLiteral("managerHandoffPendingSlotEvidence"), handoffImages},
        {QStringLiteral("representativePdf"), QJsonObject{
            {QStringLiteral("image"), QFileInfo(pdfImagePath).fileName()},
            {QStringLiteral("pageCount"), pdf.pageCount()},
            {QStringLiteral("pageWidthPoints"), pageSize.width()},
            {QStringLiteral("pageHeightPoints"), pageSize.height()},
            {QStringLiteral("renderedImageWidth"), pdfImage.width()},
            {QStringLiteral("renderedImageHeight"), pdfImage.height()},
            {QStringLiteral("contentsChecked"), QJsonArray::fromStringList(expectedText)},
            {QStringLiteral("pdfLifetime"), QStringLiteral("Saved under QTemporaryDir and removed when capture operation returns.")}
        }},
        {QStringLiteral("visualDifferences"), QJsonArray{
            QStringLiteral("Legacy assignment editing uses the shared DialogShell and shows Essay Block; Next uses a plain QDialog and labels the remove action Essay Block."),
            QStringLiteral("The Legacy manager retains the pending weekday/start internally while Next shows the pending slot; see the paired #28 captures."),
            QStringLiteral("Qt native widget and glyph rasterization differs between the two editor implementations; no pixel-parity claim is made.")
        }},
        {QStringLiteral("memoryAndLifetime"), QStringLiteral("The output request and PDF operation are local values; the PDF writer/document are destroyed before returning, and the temporary PDF is removed with QTemporaryDir. Dialog captures do not estimate allocation or memory budgets.")}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(
        QStringLiteral("schedule_testing_output_manifest.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || manifestFile.write(QJsonDocument(manifest).toJson(
            QJsonDocument::Indented)) < 0)
    {
        *error = QStringLiteral("Could not write Schedule Testing/output capture manifest.");
        return false;
    }
    return true;
}

QString modeName(const Application::ScheduleDisplayMode mode)
{
    switch (mode)
    {
    case Application::ScheduleDisplayMode::Regular: return QStringLiteral("regular");
    case Application::ScheduleDisplayMode::Intensive: return QStringLiteral("intensive");
    case Application::ScheduleDisplayMode::Testing: return QStringLiteral("testing");
    }
    return QStringLiteral("invalid");
}

bool createWorkspaceFixture(const QString& path,
                            const Application::ScheduleDisplayMode mode,
                            QString* error,
                            const bool focusedTestingFixture = false,
                            const bool wrappedIntensiveFixture = false)
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.open(path);
    if (!opened)
    {
        *error = opened.error().diagnostic;
        return false;
    }
    QFile fixture(QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral("/tests/fixtures/workspaces/large_startup.sql"));
    if (!fixture.open(QIODevice::ReadOnly))
    {
        *error = QStringLiteral("Could not read large_startup.sql.");
        return false;
    }
    if (!Test::executeSqlScript(store.database(),
                                QString::fromUtf8(fixture.readAll()), error))
    {
        return false;
    }

    if (wrappedIntensiveFixture)
    {
        if (mode != Application::ScheduleDisplayMode::Intensive)
        {
            *error = QStringLiteral(
                "The wrapped-label fixture requires Intensive mode.");
            return false;
        }
        QString longTeacher = QStringLiteral(
            "Teacher with an exceptionally long schedule display name ")
            .repeated(12).left(256);
        QString longRoom = QStringLiteral(
            "Room with an exceptionally long schedule location label ")
            .repeated(12).left(256);
        if (!longTeacher.isEmpty()
            && longTeacher.at(longTeacher.size() - 1).isSpace())
        {
            longTeacher[longTeacher.size() - 1] = QLatin1Char('T');
        }
        if (!longRoom.isEmpty() && longRoom.at(longRoom.size() - 1).isSpace())
        {
            longRoom[longRoom.size() - 1] = QLatin1Char('R');
        }
        if (longTeacher.size() != 256 || longRoom.size() != 256)
        {
            *error = QStringLiteral(
                "The wrapped-label fixture did not reach the 256-character source limit.");
            return false;
        }
        QSqlQuery updateTeacher(store.database());
        updateTeacher.prepare(QStringLiteral(R"(
            UPDATE teachers
            SET teacher_en=?, preferred_name=?, preferred_romanization=?, room_number=?
            WHERE id BETWEEN 6 AND 10
        )"));
        updateTeacher.addBindValue(longTeacher);
        updateTeacher.addBindValue(longTeacher);
        updateTeacher.addBindValue(longTeacher);
        updateTeacher.addBindValue(longRoom);
        if (!updateTeacher.exec() || updateTeacher.numRowsAffected() != 5)
        {
            *error = updateTeacher.lastError().text().isEmpty()
                ? QStringLiteral("Could not install the long Intensive labels.")
                : updateTeacher.lastError().text();
            return false;
        }
    }

    QSqlQuery setMode(store.database());
    setMode.prepare(QStringLiteral(
        "UPDATE app_settings SET value=? WHERE key='schedule_display_mode'"));
    setMode.addBindValue(modeName(mode));
    if (!setMode.exec())
    {
        *error = setMode.lastError().text();
        return false;
    }
    QSqlQuery assignment(store.database());
    if (!assignment.exec(QStringLiteral(R"(
            INSERT INTO schedule_testing_blocks (day, start_time, room, class_id)
            VALUES ('Monday', '17:00', 'Testing Room', NULL)
        )")))
    {
        *error = assignment.lastError().text();
        return false;
    }
    if (focusedTestingFixture)
    {
        QSqlQuery clearSchedule(store.database());
        if (!clearSchedule.exec(QStringLiteral("DELETE FROM class_times")))
        {
            *error = clearSchedule.lastError().text();
            return false;
        }
        QSqlQuery sparseSchedule(store.database());
        if (!sparseSchedule.exec(QStringLiteral(R"(
                INSERT INTO class_times (class_id, day, start_time, end_time)
                VALUES (1, 'Monday', '3:00 PM', '3:50 PM')
            )")))
        {
            *error = sparseSchedule.lastError().text();
            return false;
        }
    }
    return true;
}

class OtherPage final : public Ui::Page
{
public:
    using Ui::Page::Page;
};

bool saveCapture(QWidget& page, const QString& path,
                 QJsonObject* state, QString* error,
                 const std::function<void()>& beforeRender = {})
{
    page.resize(Viewport);
    page.show();
    QApplication::processEvents();
    if (QWidget* const focused = QApplication::focusWidget())
    {
        focused->clearFocus();
    }
    QApplication::processEvents();
    if (beforeRender)
    {
        beforeRender();
        QApplication::processEvents();
    }
    QImage image(Viewport, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        page.render(&painter, QPoint(0, 0),
                    QRegion(QRect(QPoint(0, 0), Viewport)));
    }
    if (image.size() != Viewport || !image.save(path, "PNG"))
    {
        *error = QStringLiteral("Could not save a 1024x768 Schedule capture from a %1x%2 page.")
            .arg(page.width()).arg(page.height());
        return false;
    }
    *state = {
        {QStringLiteral("image"), QFileInfo(path).fileName()},
        {QStringLiteral("width"), image.width()},
        {QStringLiteral("height"), image.height()}
    };
    state->insert(QStringLiteral("pageWidthBeforeCrop"), page.width());
    state->insert(QStringLiteral("pageHeightBeforeCrop"), page.height());
    return true;
}

bool captureLegacy(const QString& databasePath,
                   const QString& outputPath,
                   QJsonObject* state,
                   QString* error,
                   const bool focusTestingAssignment)
{
    PlatformProcessMemorySnapshotProvider memoryProvider;
    const ProcessMemorySnapshot before = memoryProvider.snapshot();
    ApplicationServices services;
    const RepositoryStatus opened = services.openDatabase(databasePath);
    if (!opened)
    {
        *error = opened.error().diagnostic;
        return false;
    }
    QWidget captureFrame;
    auto* const layout = new QVBoxLayout(&captureFrame);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* const page = new ScheduleWidget(&services, &captureFrame);
    page->refreshSchedule();
    layout->addWidget(page, 0, Qt::AlignTop);
    QTableWidget* table = page->findChild<QTableWidget*>(
        QStringLiteral("scheduleTable"));
    QTableWidgetItem* focusTimeItem = nullptr;
    if (focusTestingAssignment && table)
    {
        for (int row = 0; row < table->rowCount(); ++row)
        {
            QTableWidgetItem* const item = table->item(row, 0);
            if (item && item->text().contains(QStringLiteral("5:00")))
            {
                focusTimeItem = item;
                break;
            }
        }
    }
    if (focusTestingAssignment && !focusTimeItem)
    {
        *error = QStringLiteral(
            "The Legacy Testing capture could not find the Monday 5:00 PM row.");
        return false;
    }
    if (!saveCapture(captureFrame, outputPath, state, error,
                     [table, focusTimeItem] {
                         if (table && focusTimeItem)
                         {
                             table->setVerticalScrollMode(
                                 QAbstractItemView::ScrollPerPixel);
                             table->scrollToItem(
                                 focusTimeItem, QAbstractItemView::PositionAtCenter);
                         }
                     }))
    {
        return false;
    }
    if (focusTestingAssignment)
    {
        const QModelIndex focusedCell = table->model()->index(
            focusTimeItem->row(), 1);
        const bool visible = table->visualRect(focusedCell).intersects(
            table->viewport()->rect());
        if (!visible)
        {
            *error = QStringLiteral(
                "The Legacy Monday 5:00 PM Testing cell is outside the capture crop.");
            return false;
        }
        state->insert(QStringLiteral("captureFocus"),
                      QStringLiteral("Monday 5:00 PM Testing assignment"));
        state->insert(QStringLiteral("focusedCellVisible"), visible);
    }
    int toolbarTop = -1;
    int toolbarHeight = -1;
    if (QPushButton* const modeButton = page->findChild<QPushButton*>(
            QStringLiteral("scheduleRegularModeButton")))
    {
        toolbarTop = modeButton->mapTo(&captureFrame, QPoint(0, 0)).y();
        toolbarHeight = modeButton->height();
    }
    const int gridHeaderTop = table
        ? table->horizontalHeader()->mapTo(&captureFrame, QPoint(0, 0)).y()
        : -1;
    const int gridHeaderHeight = table ? table->horizontalHeader()->height() : -1;
    const ProcessMemorySnapshot active = memoryProvider.snapshot();
    const ScheduleWidgetRuntimeMetrics metrics = page->runtimeMetrics();
    QJsonArray renderedRowHeights;
    int renderedMaximumRowHeight = 0;
    int tableViewCount = 0;
    if (QTableWidget* const metricsTable = page->findChild<QTableWidget*>(
            QStringLiteral("scheduleTable")))
    {
        tableViewCount = 1;
        renderedRowHeights = rowHeights(*metricsTable);
        renderedMaximumRowHeight = maximumRowHeight(*metricsTable);
    }
    page->releaseGeneratedResources();
    QApplication::processEvents();
    const ProcessMemorySnapshot released = memoryProvider.snapshot();
    state->insert(QStringLiteral("route"), QStringLiteral("legacy_schedule"));
    state->insert(QStringLiteral("mode"), modeName(page->displayState().displayMode));
    state->insert(QStringLiteral("toolbarTop"), toolbarTop);
    state->insert(QStringLiteral("toolbarHeight"), toolbarHeight);
    state->insert(QStringLiteral("gridHeaderTop"), gridHeaderTop);
    state->insert(QStringLiteral("gridHeaderHeight"), gridHeaderHeight);
    state->insert(QStringLiteral("visibleGridCropHeight"),
                  Viewport.height() - gridHeaderTop);
    state->insert(QStringLiteral("modelRowCount"), metrics.modelRowCount);
    state->insert(QStringLiteral("modelCellCount"), metrics.modelCellCount);
    state->insert(QStringLiteral("modelEntryCount"), metrics.modelEntryCount);
    state->insert(QStringLiteral("tableRowCount"), metrics.tableRowCount);
    state->insert(QStringLiteral("tableColumnCount"), metrics.tableColumnCount);
    state->insert(QStringLiteral("tableItemCount"), metrics.tableItemCount);
    state->insert(QStringLiteral("tableCellWidgetCount"), metrics.tableCellWidgetCount);
    state->insert(QStringLiteral("tableViewCount"), tableViewCount);
    state->insert(QStringLiteral("rowHeights"), renderedRowHeights);
    state->insert(QStringLiteral("maximumRowHeight"), renderedMaximumRowHeight);
    state->insert(QStringLiteral("rowsAfterRelease"), page->runtimeMetrics().modelRowCount);
    state->insert(QStringLiteral("cellsAfterRelease"), page->runtimeMetrics().modelCellCount);
    state->insert(QStringLiteral("entriesAfterRelease"), page->runtimeMetrics().modelEntryCount);
    state->insert(QStringLiteral("memoryProcessWide"), QJsonObject{
        {QStringLiteral("before"), memoryJson(before)},
        {QStringLiteral("active"), memoryJson(active)},
        {QStringLiteral("released"), memoryJson(released)}
    });
    return true;
}

bool captureNext(const QString& databasePath,
                 Resources::ResourceLoader& loader,
                 Application::PreferencesModel& preferences,
                 const QString& outputPath,
                 QJsonObject* state,
                 QString* error,
                 const bool focusTestingAssignment)
{
    PlatformProcessMemorySnapshotProvider memoryProvider;
    const ProcessMemorySnapshot before = memoryProvider.snapshot();
    Ui::PageHost host(loader, preferences);
    Ui::SchedulePage* schedule = nullptr;
    Ui::PageDescriptor scheduleDescriptor{
        .id = *Ui::PageId::fromString(QStringLiteral("schedule")),
        .label = QStringLiteral("Schedule"),
        .factory = [&databasePath, &schedule](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            Ui::SchedulePageActions actions;
            actions.activeWorkspacePath = [&databasePath] { return databasePath; };
            actions.workspaceIdentity = [&databasePath] {
                return QStringLiteral("capture|") + databasePath;
            };
            actions.readSchedule = [](const QString& path) {
                return Platform::readSchedulePageAtPath(path);
            };
            actions.saveDisplayPreferences = [](
                const QString& path,
                const Application::ScheduleDisplayPreferencesSaveRequest& request) {
                return Platform::saveScheduleDisplayPreferencesAtPath(path, request);
            };
            actions.saveDisplayMode = [](
                const QString& path,
                const Application::ScheduleDisplayMode mode) {
                return Platform::saveScheduleDisplayModeAtPath(path, mode);
            };
            actions.saveSlotState = [](
                const QString& path,
                const Application::ScheduleSlotStateSaveRequest& request) {
                return Platform::saveScheduleSlotStateAtPath(path, request);
            };
            auto page = std::make_unique<Ui::SchedulePage>(context, std::move(actions));
            schedule = page.get();
            return std::unique_ptr<Ui::Page>(std::move(page));
        }
    };
    Ui::PageDescriptor otherDescriptor{
        .id = *Ui::PageId::fromString(QStringLiteral("other")),
        .label = QStringLiteral("Other"),
        .factory = [](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(std::make_unique<OtherPage>(context));
        }
    };
    if (!host.registerPage(std::move(scheduleDescriptor))
        || !host.registerPage(std::move(otherDescriptor))
        || !host.activatePage(QStringLiteral("schedule"))
        || !schedule)
    {
        *error = QStringLiteral("The Next Schedule page could not activate.");
        return false;
    }
    int focusedRow = -1;
    if (focusTestingAssignment)
    {
        for (int row = 0; row < schedule->model()->rowCount(); ++row)
        {
            const Ui::SchedulePageRow* const modelRow =
                schedule->model()->rowAt(row);
            if (modelRow && modelRow->startMinute == 17 * 60)
            {
                focusedRow = row;
                break;
            }
        }
        if (focusedRow < 0)
        {
            *error = QStringLiteral(
                "The Next Testing capture could not find the Monday 5:00 PM row.");
            return false;
        }
    }
    if (!saveCapture(host, outputPath, state, error, [schedule, focusedRow] {
            if (schedule && focusedRow >= 0)
            {
                schedule->scheduleTable()->setVerticalScrollMode(
                    QAbstractItemView::ScrollPerPixel);
                schedule->scheduleTable()->scrollTo(
                    schedule->model()->index(focusedRow, 1),
                    QAbstractItemView::PositionAtCenter);
            }
        }))
    {
        return false;
    }
    if (focusTestingAssignment)
    {
        const QModelIndex focusedCell = schedule->model()->index(focusedRow, 1);
        const bool visible = schedule->scheduleTable()->visualRect(
            focusedCell).intersects(schedule->scheduleTable()->viewport()->rect());
        if (!visible)
        {
            *error = QStringLiteral(
                "The Next Monday 5:00 PM Testing cell is outside the capture crop.");
            return false;
        }
        state->insert(QStringLiteral("captureFocus"),
                      QStringLiteral("Monday 5:00 PM Testing assignment"));
        state->insert(QStringLiteral("focusedCellVisible"), visible);
    }
    int intensiveClassLineExpectedCount = 0;
    int intensiveClassLineVisibleCount = 0;
    QJsonArray intensiveLongLabelProofs;
    if (schedule->model()->displayMode()
        == Application::ScheduleDisplayMode::Intensive)
    {
        QTableView* const table = schedule->scheduleTable();
        for (int row = 0; row < schedule->model()->rowCount(); ++row)
        {
            for (int column = 1;
                 column < schedule->model()->columnCount(); ++column)
            {
                const Ui::SchedulePageCell* const cell =
                    schedule->model()->cellAt(row, column);
                if (!cell || cell->entries.empty())
                {
                    continue;
                }
                const QModelIndex index = schedule->model()->index(row, column);
                const QRect cellRect = table->visualRect(index);
                if (!table->viewport()->rect().contains(cellRect))
                {
                    *error = QStringLiteral(
                        "An Intensive grade/level cell is outside the capture viewport.");
                    return false;
                }
                const QRect card = QRectF(cellRect)
                    .adjusted(2.0, 2.0, -2.0, -2.0).toRect();
                const QRect content = card.adjusted(4, 3, -4, -3);
                const auto layouts = Ui::layoutScheduleIntensiveEntries(
                    content, cell->entries,
                    schedule->model()->preferences().showEnglishNames,
                    table->font());
                if (layouts.size() != cell->entries.size())
                {
                    *error = QStringLiteral(
                        "The Intensive cell text layout is incomplete.");
                    return false;
                }
                const QString accessible = schedule->model()->data(
                    index, Qt::AccessibleTextRole).toString();
                const QString tooltip = schedule->model()->data(
                    index, Qt::ToolTipRole).toString();
                for (std::size_t entryIndex = 0;
                     entryIndex < cell->entries.size(); ++entryIndex)
                {
                    const Ui::ScheduleIntensiveEntryLayout& layout =
                        layouts[entryIndex];
                    const Ui::ScheduleIntensiveTextLine& classLine =
                        layout.classLine;
                    if (classLine.fullText.isEmpty())
                    {
                        continue;
                    }
                    ++intensiveClassLineExpectedCount;
                    const auto isBoundedSingleLine = [](
                        const Ui::ScheduleIntensiveTextLine& line) {
                        const QFontMetrics metrics(line.font);
                        return !line.visibleText.contains(QLatin1Char('\n'))
                            && metrics.height() <= line.bounds.height()
                            && metrics.horizontalAdvance(line.visibleText)
                                <= std::max(0, line.bounds.width() - 4)
                            && (line.visibleText == line.fullText
                                || line.visibleText.endsWith(QChar(0x2026)));
                    };
                    const bool dedicatedGradeBand =
                        cellRect.contains(classLine.bounds)
                        && content.contains(classLine.bounds)
                        && classLine.bounds.top() > layout.room.bounds.bottom()
                        && classLine.bounds.bottom() <= content.bottom();
                    const QString fullTeacher = layout.teacher.fullText;
                    const QString fullRoom = layout.room.fullText;
                    const bool fullValuesAccessible =
                        accessible.contains(fullTeacher)
                        && accessible.contains(fullRoom)
                        && accessible.contains(classLine.fullText)
                        && tooltip.contains(fullTeacher)
                        && tooltip.contains(fullRoom)
                        && tooltip.contains(classLine.fullText);
                    const bool teacherLineFits =
                        isBoundedSingleLine(layout.teacher);
                    const bool roomLineFits = isBoundedSingleLine(layout.room);
                    const bool classLineFits = isBoundedSingleLine(classLine);
                    if (!dedicatedGradeBand || !teacherLineFits || !roomLineFits
                        || !classLineFits || !fullValuesAccessible)
                    {
                        *error = QStringLiteral(
                            "Intensive grade/level layout failed for %1: band=%2 teacher=%3 room=%4 class=%5 fullValues=%6 heights=%7/%8/%9.")
                            .arg(classLine.fullText)
                            .arg(dedicatedGradeBand)
                            .arg(teacherLineFits)
                            .arg(roomLineFits)
                            .arg(classLineFits)
                            .arg(fullValuesAccessible)
                            .arg(QFontMetrics(layout.teacher.font).height())
                            .arg(layout.teacher.bounds.height())
                            .arg(QFontMetrics(layout.classLine.font).height());
                        return false;
                    }
                    ++intensiveClassLineVisibleCount;

                    if (fullTeacher.size() >= 128 || fullRoom.size() >= 128)
                    {
                        const bool teacherElided = layout.teacher.visibleText
                                != fullTeacher
                            && layout.teacher.visibleText.endsWith(QChar(0x2026));
                        const bool roomElided = layout.room.visibleText
                                != fullRoom
                            && layout.room.visibleText.endsWith(QChar(0x2026));
                        if (!teacherElided || !roomElided)
                        {
                            *error = QStringLiteral(
                                "Long Intensive teacher/room labels are not elided with full values retained.");
                            return false;
                        }
                        const Ui::SchedulePageRow* const modelRow =
                            schedule->model()->rowAt(row);
                        intensiveLongLabelProofs.append(QJsonObject{
                            {QStringLiteral("day"), cell->day},
                            {QStringLiteral("time"), modelRow
                                 ? modelRow->timeRangeLabel : QString{}},
                            {QStringLiteral("teacherFullLength"), fullTeacher.size()},
                            {QStringLiteral("roomFullLength"), fullRoom.size()},
                            {QStringLiteral("teacherVisible"),
                             layout.teacher.visibleText},
                            {QStringLiteral("roomVisible"), layout.room.visibleText},
                            {QStringLiteral("gradeLevelVisible"),
                             classLine.visibleText},
                            {QStringLiteral("gradeBandTopInCell"),
                             classLine.bounds.top() - cellRect.top()},
                            {QStringLiteral("gradeBandHeight"),
                             classLine.bounds.height()},
                            {QStringLiteral("fullValuesAccessibleAndTooltip"),
                             fullValuesAccessible}
                        });
                    }
                }
            }
        }
        if (intensiveClassLineExpectedCount == 0
            || intensiveClassLineVisibleCount
                != intensiveClassLineExpectedCount)
        {
            *error = QStringLiteral(
                "The Intensive capture did not verify every populated grade/level cell.");
            return false;
        }
    }
    const ProcessMemorySnapshot active = memoryProvider.snapshot();
    const Ui::SchedulePageVisualMetrics metrics = schedule->visualMetrics();
    const QString displayedMode = modeName(schedule->model()->displayMode());
    const QJsonArray renderedRowHeights = rowHeights(*schedule->scheduleTable());
    const int renderedMaximumRowHeight =
        maximumRowHeight(*schedule->scheduleTable());
    const int toolbarTop = schedule->modeButton(
        Application::ScheduleDisplayMode::Regular)->mapTo(&host, QPoint(0, 0)).y();
    const int toolbarHeight = schedule->modeButton(
        Application::ScheduleDisplayMode::Regular)->height();
    const int gridHeaderTop = schedule->scheduleTable()->horizontalHeader()
        ->mapTo(&host, QPoint(0, 0)).y();
    const int gridHeaderHeight = schedule->scheduleTable()->horizontalHeader()->height();
    static_cast<void>(host.activatePage(QStringLiteral("other")));
    QApplication::processEvents();
    const ProcessMemorySnapshot released = memoryProvider.snapshot();
    const Ui::SchedulePageVisualMetrics afterRelease = schedule->visualMetrics();
    state->insert(QStringLiteral("route"), QStringLiteral("next_schedule"));
    state->insert(QStringLiteral("mode"), displayedMode);
    state->insert(QStringLiteral("toolbarTop"), toolbarTop);
    state->insert(QStringLiteral("toolbarHeight"), toolbarHeight);
    state->insert(QStringLiteral("gridHeaderTop"), gridHeaderTop);
    state->insert(QStringLiteral("gridHeaderHeight"), gridHeaderHeight);
    state->insert(QStringLiteral("visibleGridCropHeight"),
                  Viewport.height() - gridHeaderTop);
    state->insert(QStringLiteral("rowCount"), metrics.model.rowCount);
    state->insert(QStringLiteral("cellCount"), metrics.model.cellCount);
    state->insert(QStringLiteral("entryCount"), metrics.model.entryCount);
    state->insert(QStringLiteral("sourceClassCount"), metrics.model.sourceClassCount);
    state->insert(QStringLiteral("intensiveClassLineExpectedCellCount"),
                  intensiveClassLineExpectedCount);
    state->insert(QStringLiteral("intensiveClassLineVisibleCellCount"),
                  intensiveClassLineVisibleCount);
    state->insert(QStringLiteral("intensiveLongLabelProofs"),
                  intensiveLongLabelProofs);
    state->insert(QStringLiteral("tableViewCount"), metrics.tableViewCount);
    state->insert(QStringLiteral("tableCellWidgetCount"), metrics.tableCellWidgetCount);
    state->insert(QStringLiteral("descendantWidgetCount"), metrics.descendantWidgetCount);
    state->insert(QStringLiteral("rowHeights"), renderedRowHeights);
    state->insert(QStringLiteral("maximumRowHeight"), renderedMaximumRowHeight);
    state->insert(QStringLiteral("rowsAfterSuspension"), afterRelease.model.rowCount);
    state->insert(QStringLiteral("cellsAfterSuspension"), afterRelease.model.cellCount);
    state->insert(QStringLiteral("entriesAfterSuspension"), afterRelease.model.entryCount);
    state->insert(QStringLiteral("sourceClassesAfterSuspension"),
                  afterRelease.model.sourceClassCount);
    state->insert(QStringLiteral("memoryProcessWide"), QJsonObject{
        {QStringLiteral("before"), memoryJson(before)},
        {QStringLiteral("active"), memoryJson(active)},
        {QStringLiteral("released"), memoryJson(released)}
    });
    return true;
}

QString captureStem(const QString& route, const QString& mode,
                    const QString& locale, const QString& theme)
{
    return route + QLatin1Char('_') + mode + QLatin1Char('_')
        + locale + QLatin1Char('_') + theme + QStringLiteral(".png");
}

bool capturePair(const QString& databasePath,
                 Resources::ResourceLoader& loader,
                 Application::PreferencesModel& preferences,
                 const QString& outputDirectory,
                 const QString& mode,
                 const QString& locale,
                 const QString& theme,
                 QJsonArray* captures,
                 QString* error,
                 const bool focusTestingAssignment = false)
{
    QJsonObject legacy;
    QJsonObject next;
    if (!captureLegacy(databasePath,
                       QDir(outputDirectory).filePath(captureStem(
                           QStringLiteral("legacy"), mode, locale, theme)),
                       &legacy, error, focusTestingAssignment)
        || !captureNext(databasePath, loader, preferences,
                        QDir(outputDirectory).filePath(captureStem(
                            QStringLiteral("next"), mode, locale, theme)),
                        &next, error, focusTestingAssignment))
    {
        return false;
    }
    legacy.insert(QStringLiteral("locale"), locale);
    legacy.insert(QStringLiteral("theme"), theme);
    next.insert(QStringLiteral("locale"), locale);
    next.insert(QStringLiteral("theme"), theme);
    if (focusTestingAssignment)
    {
        const QString variant = QStringLiteral(
            "96 classes retained; regular meeting rows reduced to one for the visible assignment crop");
        legacy.insert(QStringLiteral("fixtureVariant"), variant);
        next.insert(QStringLiteral("fixtureVariant"), variant);
    }
    const bool wrappedIntensiveCapture =
        mode == QStringLiteral("wrapped_intensive");
    if (wrappedIntensiveCapture)
    {
        const QString variant = QStringLiteral(
            "large fixture with teacher and room labels at the 256-character source limit");
        legacy.insert(QStringLiteral("fixtureVariant"), variant);
        next.insert(QStringLiteral("fixtureVariant"), variant);
    }
    const bool rowParity = legacy.value(QStringLiteral("modelRowCount")).toInt()
        == next.value(QStringLiteral("rowCount")).toInt();
    const bool cellParity = legacy.value(QStringLiteral("modelCellCount")).toInt()
        == next.value(QStringLiteral("cellCount")).toInt();
    const bool entryParity = legacy.value(QStringLiteral("modelEntryCount")).toInt()
        == next.value(QStringLiteral("entryCount")).toInt();
    const bool rowHeightParity = legacy.value(QStringLiteral("rowHeights")).toArray()
        == next.value(QStringLiteral("rowHeights")).toArray();
    const bool toolbarAlignment = legacy.value(QStringLiteral("toolbarTop")).toInt()
        == next.value(QStringLiteral("toolbarTop")).toInt();
    const bool gridAlignment = legacy.value(QStringLiteral("gridHeaderTop")).toInt()
        == next.value(QStringLiteral("gridHeaderTop")).toInt();
    const bool toolbarHeightParity = legacy.value(QStringLiteral("toolbarHeight")).toInt()
        == next.value(QStringLiteral("toolbarHeight")).toInt();
    const bool headerHeightParity = legacy.value(QStringLiteral("gridHeaderHeight")).toInt()
        == next.value(QStringLiteral("gridHeaderHeight")).toInt();
    const bool visibleGridHeightParity =
        legacy.value(QStringLiteral("visibleGridCropHeight")).toInt()
        == next.value(QStringLiteral("visibleGridCropHeight")).toInt();
    const bool intensiveCapture = mode.endsWith(QStringLiteral("intensive"));
    QString pixelParityReason = QStringLiteral(
        "Legacy shows Import and, in Testing, Testing Classes actions; Next places Preferences in the toolbar.");
    if (intensiveCapture)
    {
        pixelParityReason += QStringLiteral(
            " In Intensive mode, Next uses more compact cell text to keep grade/level visible within matched 60 px rows.");
    }
    if (wrappedIntensiveCapture)
    {
        pixelParityReason += QStringLiteral(
            " For wrapped labels, Next elides teacher and room on one line and retains their full values for accessibility and tooltips.");
    }
    int nextTableContentHeight = next.value(
        QStringLiteral("gridHeaderHeight")).toInt();
    const QJsonArray nextRowHeights = next.value(
        QStringLiteral("rowHeights")).toArray();
    for (const QJsonValue& rowHeight : nextRowHeights)
    {
        nextTableContentHeight += rowHeight.toInt();
    }
    if (nextTableContentHeight
        > next.value(QStringLiteral("visibleGridCropHeight")).toInt())
    {
        pixelParityReason += QStringLiteral(
            " In captures whose rows overflow the visible grid, Next also shows its vertical scroll indicator.");
    }
    pixelParityReason += QStringLiteral(
        " Qt model/view text rendering differs slightly at glyph edges, so pixel parity is not claimed.");
    QJsonObject comparison{
        {QStringLiteral("sameModeAndWorkspaceData"), true},
        {QStringLiteral("rowCountParity"), rowParity},
        {QStringLiteral("cellCountParity"), cellParity},
        {QStringLiteral("entryCountParity"), entryParity},
        {QStringLiteral("rowHeightParity"), rowHeightParity},
        {QStringLiteral("toolbarAlignment"), toolbarAlignment},
        {QStringLiteral("gridHeaderAlignment"), gridAlignment},
        {QStringLiteral("toolbarHeightParity"), toolbarHeightParity},
        {QStringLiteral("gridHeaderHeightParity"), headerHeightParity},
        {QStringLiteral("visibleGridHeightParity"), visibleGridHeightParity},
        {QStringLiteral("pixelParity"), false},
        {QStringLiteral("pixelParityReason"), pixelParityReason}
    };
    if (intensiveCapture)
    {
        const int expectedClassLineCells = next.value(
            QStringLiteral("intensiveClassLineExpectedCellCount")).toInt();
        const int visibleClassLineCells = next.value(
            QStringLiteral("intensiveClassLineVisibleCellCount")).toInt();
        const bool intensiveClassLineVisibility = expectedClassLineCells > 0
            && visibleClassLineCells == expectedClassLineCells;
        comparison.insert(QStringLiteral("intensiveClassLineVisibility"),
                          intensiveClassLineVisibility);
        if (!intensiveClassLineVisibility)
        {
            *error = QStringLiteral(
                "Not every Intensive grade/level line is visible in %1/%2.")
                .arg(locale, theme);
            return false;
        }
    }
    if (wrappedIntensiveCapture)
    {
        const QJsonArray proofs = next.value(
            QStringLiteral("intensiveLongLabelProofs")).toArray();
        if (proofs.size() != 5)
        {
            *error = QStringLiteral(
                "The wrapped-label capture did not exercise all five weekday Intensive cells.");
            return false;
        }
        for (const QJsonValue& proofValue : proofs)
        {
            const QJsonObject proof = proofValue.toObject();
            if (proof.value(QStringLiteral("teacherFullLength")).toInt() != 256
                || proof.value(QStringLiteral("roomFullLength")).toInt() != 256
                || !proof.value(QStringLiteral("teacherVisible")).toString()
                        .endsWith(QChar(0x2026))
                || !proof.value(QStringLiteral("roomVisible")).toString()
                        .endsWith(QChar(0x2026))
                || proof.value(QStringLiteral("gradeLevelVisible"))
                       .toString().isEmpty()
                || proof.value(QStringLiteral("gradeBandHeight")).toInt() <= 0
                || !proof.value(QStringLiteral(
                        "fullValuesAccessibleAndTooltip")).toBool())
            {
                *error = QStringLiteral(
                    "The wrapped-label capture did not preserve a reserved grade band and full tooltip values: %1")
                    .arg(QString::fromUtf8(QJsonDocument(proof).toJson(
                        QJsonDocument::Compact)));
                return false;
            }
        }
        comparison.insert(QStringLiteral("wrappedLabelGradeBandVisibility"), true);
        comparison.insert(QStringLiteral("wrappedTeacherRoomElided"), true);
        comparison.insert(QStringLiteral("wrappedFullValuesAvailable"), true);
    }
    if (!entryParity)
    {
        comparison.insert(QStringLiteral("entryCountDifferenceReason"),
            QStringLiteral("The canonical Monday 17:00 Testing assignment is present. Legacy retains %1 visible entries; Next renders %2. See the measured route counts in the manifest.")
                .arg(legacy.value(QStringLiteral("modelEntryCount")).toInt())
                .arg(next.value(QStringLiteral("entryCount")).toInt()));
    }
    if (!rowHeightParity)
    {
        comparison.insert(QStringLiteral("rowHeightDifferenceReason"),
            QStringLiteral("Legacy and Next have different rendered row heights; see the per-route rowHeights arrays."));
    }
    if (!toolbarAlignment || !gridAlignment || !toolbarHeightParity
        || !headerHeightParity || !visibleGridHeightParity)
    {
        *error = QStringLiteral(
            "The Next Schedule toolbar/grid crop does not align with Legacy for %1/%2/%3.")
            .arg(mode, locale, theme);
        return false;
    }
    legacy.insert(QStringLiteral("comparison"), comparison);
    next.insert(QStringLiteral("comparison"), comparison);
    captures->append(legacy);
    captures->append(next);
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    if (argc == 3
        && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--c32"))
    {
        const QString resourceRoot = QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
        const QString outputDirectory = QString::fromLocal8Bit(argv[2]);
        QString error;
        if (!captureScheduleTestingOutputEvidence(
                application,
                outputDirectory,
                QStringLiteral(CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY),
                QDir(resourceRoot).filePath(QStringLiteral("assets/styles")),
                &error))
        {
            qCritical().noquote() << error;
            return 1;
        }
        qInfo().noquote() << "Wrote Schedule Testing/output captures to"
                          << outputDirectory;
        return 0;
    }
    if (argc != 2)
    {
        qCritical("Usage: ClassMngrNextScheduleVisualCapture OUTPUT_DIR | --c32 OUTPUT_DIR");
        return 2;
    }
    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create Schedule capture output directory");
        return 2;
    }

    const auto catalog = Resources::ResourceCatalog::load(
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT));
    if (!catalog)
    {
        qCritical().noquote() << catalog.error().message;
        return 1;
    }
    Resources::ResourceLoader loader(std::move(*catalog));
    Test::MemoryPreferences memoryPreferences;
    QTemporaryDir temporary;
    if (!temporary.isValid())
    {
        qCritical("Could not create temporary Schedule workspace directory");
        return 1;
    }

    const QString resourceRoot = QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const QString translationDirectory = QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY);
    const QString stylesheetDirectory = QDir(resourceRoot).filePath(
        QStringLiteral("assets/styles"));
    const QStringList locales{QStringLiteral("en_US"), QStringLiteral("ko_KR")};
    const QStringList themes{QStringLiteral("light"), QStringLiteral("dark")};
    const std::array<Application::ScheduleDisplayMode, 3> modes{
        Application::ScheduleDisplayMode::Regular,
        Application::ScheduleDisplayMode::Intensive,
        Application::ScheduleDisplayMode::Testing
    };
    QJsonArray captures;
    QString error;

    for (const QString& locale : locales)
    {
        for (const QString& theme : themes)
        {
            QLocale::setDefault(QLocale(locale));
            application.setPalette(themePalette(theme));
            application.setStyleSheet(readText(QDir(stylesheetDirectory)
                .filePath(theme + QStringLiteral(".qss"))));
            std::unique_ptr<QTranslator> translator;
            if (locale == QStringLiteral("ko_KR"))
            {
                translator = std::make_unique<QTranslator>();
                if (!translator->load(QDir(translationDirectory).filePath(
                        QStringLiteral("ClassMngr_ko_KR.qm")))
                    || !application.installTranslator(translator.get()))
                {
                    qCritical("Could not install Korean Schedule translation");
                    return 1;
                }
            }

            for (const Application::ScheduleDisplayMode mode : modes)
            {
                const QString suffix = locale + QLatin1Char('_') + theme
                    + QLatin1Char('_') + modeName(mode);
                const QString databasePath = temporary.filePath(suffix + QStringLiteral(".tps"));
                if (!createWorkspaceFixture(databasePath, mode, &error)
                    || !capturePair(databasePath, loader, memoryPreferences.model,
                                    outputDirectory, modeName(mode), locale, theme,
                                    &captures, &error))
                {
                    qCritical().noquote() << error;
                    return 1;
                }
            }
            if (translator)
            {
                application.removeTranslator(translator.get());
            }
        }
    }

    QLocale::setDefault(QLocale(QStringLiteral("en_US")));
    application.setPalette(themePalette(QStringLiteral("light")));
    application.setStyleSheet(readText(QDir(stylesheetDirectory)
        .filePath(QStringLiteral("light.qss"))));
    for (const Application::ScheduleDisplayMode mode : {
             Application::ScheduleDisplayMode::Regular,
             Application::ScheduleDisplayMode::Intensive})
    {
        const QString suffix = QStringLiteral("96_") + modeName(mode);
        const QString databasePath = temporary.filePath(suffix + QStringLiteral(".tps"));
        if (!createWorkspaceFixture(databasePath, mode, &error)
            || !capturePair(databasePath, loader, memoryPreferences.model,
                            outputDirectory, QStringLiteral("96_") + modeName(mode),
                            QStringLiteral("en_US"), QStringLiteral("light"),
                            &captures, &error))
        {
            qCritical().noquote() << error;
            return 1;
        }
    }

    const QString focusedTestingPath = temporary.filePath(
        QStringLiteral("testing-assignment-focused.tps"));
    if (!createWorkspaceFixture(focusedTestingPath,
                                Application::ScheduleDisplayMode::Testing,
                                &error, true)
        || !capturePair(focusedTestingPath, loader, memoryPreferences.model,
                        outputDirectory, QStringLiteral("testing-assignment"),
                        QStringLiteral("en_US"), QStringLiteral("light"),
                        &captures, &error, true))
    {
        qCritical().noquote() << error;
        return 1;
    }

    const QString wrappedIntensivePath = temporary.filePath(
        QStringLiteral("intensive-wrapped-labels.tps"));
    if (!createWorkspaceFixture(wrappedIntensivePath,
                                Application::ScheduleDisplayMode::Intensive,
                                &error, false, true)
        || !capturePair(wrappedIntensivePath, loader, memoryPreferences.model,
                        outputDirectory, QStringLiteral("wrapped_intensive"),
                        QStringLiteral("en_US"), QStringLiteral("light"),
                        &captures, &error))
    {
        qCritical().noquote() << error;
        return 1;
    }

    const QJsonObject manifest{
        {QStringLiteral("taskId"), QStringLiteral("phase7-c30-schedule-implementation-20261028")},
        {QStringLiteral("fixture"), QStringLiteral("tests/fixtures/workspaces/large_startup.sql")},
        {QStringLiteral("fixtureClasses"), 96},
        {QStringLiteral("fixtureRegularRows"), 768},
        {QStringLiteral("fixtureIntensiveRows"), 24},
        {QStringLiteral("wrappedLabelSourceLength"), 256},
        {QStringLiteral("intensiveLongLabelLayout"), QStringLiteral(
             "fixed single-line teacher and room bands with right elision; reserved grade/level band; full strings in accessible and tooltip text")},
        {QStringLiteral("testingAssignmentKey"), QStringLiteral("Monday 17:00")},
        {QStringLiteral("viewport"), QJsonArray{1024, 768}},
        {QStringLiteral("pixelParityClaim"), false},
        {QStringLiteral("captures"), captures}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || manifestFile.write(QJsonDocument(manifest).toJson(
               QJsonDocument::Indented)) < 0)
    {
        qCritical("Could not write Schedule capture manifest");
        return 1;
    }
    qInfo().noquote() << "Wrote" << captures.size() << "Schedule captures to"
                      << outputDirectory;
    return 0;
}
