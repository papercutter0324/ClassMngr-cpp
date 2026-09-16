#include "app/mainwindow.h"
#include "app/services/feature_services.h"
#include "app/controllers/update_controller.h"
#include "app/startup_database_path.h"
#include "core/application_services.h"
#include "core/appsettings.h"
#include "core/build_info.h"
#include "core/fontmanager.h"
#include "core/language_service.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "core/resource_paths.h"
#include "core/settingsmanager.h"
#include "core/startup_profiler.h"
#include "core/theme_service.h"
#include "core/updater/update_service.h"
#include "ui/shared/widgets/splash/splashscreen.h"
#include "ui/shared/constants/options.h"
#include "ui/shared/state/option_state_keys.h"
#include "core/utils/platform.h"
#include "features/calendar/ui/calendar_page.h"
#include "features/classes/services/class_transfer_json_codec.h"
#include "features/classes/ui/class_import_dialog.h"
#include "features/classes/ui/classes_page.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "features/schedule/ui/schedule_import_dialog.h"
#include "features/schedule/ui/schedule_import_review_dialog.h"
#include "features/schedule/ui/schedule_page.h"
#include "features/speaking_eval/services/speaking_eval_batch_report_service.h"
#include "features/speaking_eval/ui/speaking_eval_ai_batch_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_batch_export_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_model.h"
#include "features/speaking_eval/ui/speaking_eval_page.h"
#include "features/speaking_eval/ui/speaking_eval_report_dialog.h"
#include "features/speaking_eval/ui/speaking_eval_table_view.h"
#include "features/sub_prep/services/sub_prep_package_service.h"
#include "features/sub_prep/ui/sub_prep_print_dialog.h"
#include "features/sub_prep/ui/sub_prep_page.h"
#include "features/teacher/ui/staff_directory_page.h"
#include "ui/shared/pages/pdf_viewer_page.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#if !defined(Q_OS_MACOS)
#include "ui/shared/styles/file_dialog_icon_style.h"
#endif

#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPdfDocument>
#include <QPointer>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QTimer>
#include <QElapsedTimer>
#include <QTabWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QDebug>

#include <functional>
#include <algorithm>
#include <memory>
#include <optional>

// Later: move MainWindow construction behind an ApplicationBootstrap class

// =========================================================
// Helpers
// =========================================================

QIcon getAppIcon()
{
    Platform userPlatform = getPlatform();

    if (userPlatform == Platform::WINDOWS)
    {
        return QIcon(ResourcePaths::Icons::appWindows());
    }

    return QIcon(ResourcePaths::Icons::appDefault());
}


bool isAdminMode(const QStringList &args)
{
    return args.contains(AppSettings::AdminModeArgument);
}

struct StartupPerformanceMode
{
    bool enabled = false;
    QString outputPath;
    bool visualCaptureEnabled = false;
    QString visualCaptureOutputPath;
    std::optional<Language> visualLanguageOverride;
    std::optional<Theme> visualThemeOverride;
    bool workflowEnabled = false;
    bool scheduleLifecycleEnabled = false;
    bool scheduleImportLifecycleEnabled = false;
    bool scheduleImportApplyLifecycleEnabled = false;
    bool calendarImportLifecycleEnabled = false;
    bool classesLifecycleEnabled = false;
    bool classTransferLifecycleEnabled = false;
    bool speakingEvaluationLifecycleEnabled = false;
    bool staffDirectoryLifecycleEnabled = false;
    bool subPrepLifecycleEnabled = false;
    bool subPrepOutputLifecycleEnabled = false;
    bool subPrepVisualStatesEnabled = false;
    enum class Scenario
    {
        Minimal,
        Representative
    };

    Scenario scenario = Scenario::Minimal;
    int settleMilliseconds = 0;
};

QString startupScenarioName(
    StartupPerformanceMode::Scenario scenario
    )
{
    return scenario == StartupPerformanceMode::Scenario::Representative
        ? QStringLiteral("representative-startup")
        : QStringLiteral("minimal-startup");
}

constexpr int StartupWorkflowStepDelayMilliseconds = 150;
constexpr int StartupFiveMinuteIdleMilliseconds = 5 * 60 * 1000;
constexpr auto StartupPdfWorkflowRelativePath =
    "Guides/DYB Lesson Planning Guide.pdf";

const QList<PageType>& startupWorkflowPageTypes()
{
    static const QList<PageType> pageTypes{
        PageType::MyWorkspace,
        PageType::Schedule,
        PageType::Classes,
        PageType::TestingClasses,
        PageType::TeacherInfo,
        PageType::NativeEnglishTeachers,
        PageType::GsTeam,
        PageType::CampusDashboard,
        PageType::SubPrep,
        PageType::MyClasses,
        PageType::PdfViewer,
        PageType::MyWorkspace
    };

    return pageTypes;
}

void appendStartupWorkflowTrace(const QString& message)
{
    const QString outputPath =
        qEnvironmentVariable("CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH")
            .trimmed();
    if (outputPath.isEmpty())
    {
        return;
    }

    QFile file(outputPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        file.write((message + QLatin1Char('\n')).toUtf8());
        file.flush();
    }
}

bool saveStartupPdfCapture(
    const QPixmap& image,
    const QString& fileName
    )
{
    const QString outputDirectoryPath =
        qEnvironmentVariable("CLASSMNGR_STARTUP_PDF_CAPTURE_OUTPUT_DIR")
            .trimmed();
    if (outputDirectoryPath.isEmpty())
    {
        return true;
    }

    QDir outputDirectory(outputDirectoryPath);
    if (!outputDirectory.mkpath(QStringLiteral(".")))
    {
        qWarning().noquote()
            << QStringLiteral(
                "Unable to create startup PDF capture directory %1."
                ).arg(outputDirectoryPath);
        return false;
    }

    const QString outputPath =
        outputDirectory.filePath(fileName);
    if (!image.save(outputPath, "PNG"))
    {
        qWarning().noquote()
            << QStringLiteral(
                "Unable to write startup PDF capture to %1."
                ).arg(outputPath);
        return false;
    }

    return true;
}

struct StartupGeneratedPdfSummary
{
    bool valid = false;
    QString error;
    QStringList pdfPaths;
    int pageCount = 0;
    qint64 pdfBytes = 0;
    qint64 decodedFirstPageBytes = 0;
};

StartupGeneratedPdfSummary inspectStartupGeneratedPdfs(
    const QString& outputRoot
    )
{
    StartupGeneratedPdfSummary summary;
    QDirIterator iterator(
        outputRoot,
        {QStringLiteral("*.pdf")},
        QDir::Files,
        QDirIterator::Subdirectories
        );
    while (iterator.hasNext())
    {
        summary.pdfPaths.append(iterator.next());
    }

    std::sort(summary.pdfPaths.begin(), summary.pdfPaths.end());
    for (int index = 0; index < summary.pdfPaths.size(); ++index)
    {
        const QString& pdfPath = summary.pdfPaths.at(index);
        QPdfDocument document;
        const QPdfDocument::Error loadError = document.load(pdfPath);
        if (
            loadError != QPdfDocument::Error::None
            || document.status() != QPdfDocument::Status::Ready
            || document.pageCount() <= 0
            )
        {
            summary.error =
                QStringLiteral("generated-pdf-load-failed: %1").arg(pdfPath);
            return summary;
        }

        const QSizeF pagePoints = document.pagePointSize(0);
        const QSize renderSize(
            std::max(1, qRound(pagePoints.width() * 150.0 / 72.0)),
            std::max(1, qRound(pagePoints.height() * 150.0 / 72.0))
            );
        const QImage firstPage = document.render(0, renderSize);
        if (firstPage.isNull())
        {
            summary.error =
                QStringLiteral("generated-pdf-render-failed: %1").arg(pdfPath);
            return summary;
        }

        const QString capturePath =
            QDir(outputRoot).filePath(
                QStringLiteral("generated-output-%1-first-page.png")
                    .arg(index + 1)
                );
        if (!firstPage.save(capturePath, "PNG"))
        {
            summary.error =
                QStringLiteral("generated-pdf-capture-failed: %1")
                    .arg(capturePath);
            return summary;
        }

        summary.pageCount += document.pageCount();
        summary.pdfBytes += QFileInfo(pdfPath).size();
        summary.decodedFirstPageBytes +=
            static_cast<qint64>(firstPage.sizeInBytes());
    }

    if (summary.pdfPaths.isEmpty())
    {
        summary.error = QStringLiteral("generated-pdf-output-empty");
        return summary;
    }

    summary.valid = true;
    return summary;
}

void scheduleStartupPerformanceSubPrepOutputLifecycle(
    QApplication& app,
    SubPrepPage* page,
    StartupProfiler& profiler,
    const std::shared_ptr<bool>& workflowSucceeded,
    std::function<void()> completion
    )
{
    const QString targetRoot =
        qEnvironmentVariable(
            "CLASSMNGR_STARTUP_SUB_PREP_OUTPUT_TARGET_ROOT"
            ).trimmed();
    if (targetRoot.isEmpty())
    {
        *workflowSucceeded = false;
        StartupProfiler::recordSubPrepOutputFailed(
            QStringLiteral("target-root-not-configured")
            );
        StartupProfiler::recordSubPrepOutputOperationReleased();
        completion();
        return;
    }

    if (!QDir().mkpath(targetRoot))
    {
        *workflowSucceeded = false;
        StartupProfiler::recordSubPrepOutputFailed(
            QStringLiteral("target-root-create-failed: %1").arg(targetRoot)
            );
        StartupProfiler::recordSubPrepOutputOperationReleased();
        completion();
        return;
    }

    StartupProfiler::recordSubPrepOutputOperationStarted(targetRoot);
    const auto dialogAccepted = std::make_shared<bool>(false);
    const auto generationWarning = std::make_shared<QString>();
    const auto controllerStopped = std::make_shared<bool>(false);
    const auto controllerAttempts = std::make_shared<int>(0);
    const auto controller =
        std::make_shared<std::function<void()>>();
    *controller =
        [
            &app,
            targetRoot,
            dialogAccepted,
            generationWarning,
            controllerStopped,
            controllerAttempts,
            controller
        ]()
    {
        if (*controllerStopped)
        {
            return;
        }

        if (*dialogAccepted)
        {
            QMessageBox* warning =
                qobject_cast<QMessageBox*>(
                    QApplication::activeModalWidget()
                    );
            if (!warning)
            {
                for (QWidget* widget : QApplication::topLevelWidgets())
                {
                    if (widget->isVisible())
                    {
                        warning = qobject_cast<QMessageBox*>(widget);
                        if (warning)
                        {
                            break;
                        }
                    }
                }
            }

            if (warning)
            {
                const QString detail =
                    QStringLiteral("generation-warning title=%1; text=%2")
                        .arg(
                            warning->windowTitle().simplified(),
                            warning->text().simplified()
                            );
                *generationWarning = detail;
                StartupProfiler::recordSubPrepOutputFailed(detail);
                warning->reject();
                *controllerStopped = true;
                return;
            }
        }

        SubPrepPrintDialog* dialog = nullptr;
        if (!*dialogAccepted)
        {
            dialog =
                qobject_cast<SubPrepPrintDialog*>(
                    QApplication::activeModalWidget()
                    );
            if (!dialog)
            {
                for (QWidget* widget : QApplication::topLevelWidgets())
                {
                    dialog = qobject_cast<SubPrepPrintDialog*>(widget);
                    if (dialog)
                    {
                        break;
                    }
                }
            }
        }

        if (!dialog)
        {
            ++*controllerAttempts;
            const int maxAttempts = *dialogAccepted ? 6000 : 100;
            if (*controllerAttempts < maxAttempts)
            {
                QTimer::singleShot(
                    10,
                    &app,
                    [controller]()
                    {
                        (*controller)();
                    }
                    );
            }
            return;
        }

        if (QLineEdit* targetEdit =
                dialog->findChild<QLineEdit*>(
                    QStringLiteral("subPrepTargetFolderEdit")
                    ))
        {
            targetEdit->setText(targetRoot);
        }
        if (QLineEdit* nameEdit =
                dialog->findChild<QLineEdit*>(
                    QStringLiteral("subPrepUserNameEdit")
                    ))
        {
            nameEdit->setText(QStringLiteral("Phase 0 Heavy"));
        }
        if (QCheckBox* createFolderCheck =
                dialog->findChild<QCheckBox*>(
                    QStringLiteral("subPrepCreateFolderCheckBox")
                    ))
        {
            createFolderCheck->setChecked(true);
        }
        if (QCheckBox* openFolderCheck =
                dialog->findChild<QCheckBox*>(
                    QStringLiteral("subPrepOpenFolderCheckBox")
                    ))
        {
            openFolderCheck->setChecked(false);
        }
        if (QCheckBox* printPaperCheck =
                dialog->findChild<QCheckBox*>(
                    QStringLiteral("subPrepPrintPaperCopiesCheckBox")
                    ))
        {
            printPaperCheck->setChecked(false);
        }
        for (QCheckBox* dayCheck : dialog->findChildren<QCheckBox*>())
        {
            if (!dayCheck->property("day").toString().isEmpty())
            {
                dayCheck->setChecked(true);
            }
        }

        const QPixmap dialogCapture = dialog->grab();
        if (
            dialogCapture.isNull()
            || !dialogCapture.save(
                QDir(targetRoot).filePath(
                    QStringLiteral("sub-prep-output-dialog.png")
                    ),
                "PNG"
                )
            )
        {
            StartupProfiler::recordSubPrepOutputFailed(
                QStringLiteral("output-dialog-capture-failed")
                );
            dialog->reject();
            return;
        }

        dialog->accept();
        *dialogAccepted = true;
        *controllerAttempts = 0;
        QTimer::singleShot(
            10,
            &app,
            [controller]()
            {
                (*controller)();
            }
            );
    };

    QTimer::singleShot(
        0,
        &app,
        [controller]()
        {
            (*controller)();
        }
        );

    if (!QMetaObject::invokeMethod(
            page,
            "generateSubPrep",
            Qt::DirectConnection
            ))
    {
        *workflowSucceeded = false;
        StartupProfiler::recordSubPrepOutputFailed(
            QStringLiteral("generate-invoke-failed")
            );
        StartupProfiler::recordSubPrepOutputOperationReleased();
        completion();
        return;
    }

    *controllerStopped = true;

    if (!generationWarning->isEmpty())
    {
        *workflowSucceeded = false;
        StartupProfiler::recordSubPrepOutputOperationReleased();
        completion();
        return;
    }

    if (!*dialogAccepted)
    {
        *workflowSucceeded = false;
        StartupProfiler::recordSubPrepOutputFailed(
            QStringLiteral("output-dialog-not-accepted")
            );
        StartupProfiler::recordSubPrepOutputOperationReleased();
        completion();
        return;
    }

    const StartupGeneratedPdfSummary summary =
        inspectStartupGeneratedPdfs(targetRoot);
    if (!summary.valid)
    {
        *workflowSucceeded = false;
        StartupProfiler::recordSubPrepOutputFailed(summary.error);
        StartupProfiler::recordSubPrepOutputOperationReleased();
        completion();
        return;
    }

    StartupProfiler::recordSubPrepOutputGenerated(
        summary.pdfPaths.size(),
        summary.pageCount,
        summary.pdfBytes,
        summary.decodedFirstPageBytes
        );
    StartupProfiler::recordSubPrepOutputOperationReleased();
    completion();
}

void scheduleStartupPerformancePdfLifecycle(
    QApplication& app,
    MainWindow& window,
    StartupProfiler& profiler,
    const std::shared_ptr<bool>& workflowSucceeded,
    std::function<void()> completion
    )
{
    const auto phase = std::make_shared<int>(0);
    const auto pdfPath = std::make_shared<QString>();
    const auto runPhase =
        std::make_shared<std::function<void()>>();

    *runPhase =
        [
            &app,
            &window,
            &profiler,
            workflowSucceeded,
            pdfPath,
            phase,
            runPhase,
            completion
        ]()
    {
        PageManager* pages = window.pageManager();
        PdfViewerPage* viewer =
            pages ? pages->pdfViewerPage() : nullptr;

        const auto fail =
            [
                &profiler,
                workflowSucceeded,
                completion
            ](const QString& detail)
        {
            *workflowSucceeded = false;
            profiler.checkpoint(
                QStringLiteral("pdf-workflow-failed"),
                detail
                );
            completion();
        };

        if (!viewer)
        {
            fail(QStringLiteral("viewer-unavailable"));
            return;
        }

        if (*phase == 0)
        {
            profiler.checkpoint(
                QStringLiteral("pdf-workflow-start"),
                QString::fromUtf8(StartupPdfWorkflowRelativePath)
                );

            auto lease = ResourcePaths::Documents::acquire();
            if (!lease)
            {
                fail(
                    QStringLiteral("documents-pack-unavailable: %1")
                        .arg(lease.error())
                    );
                return;
            }

            *pdfPath =
                ResourcePaths::Documents::filePath(
                    *lease,
                    QString::fromUtf8(StartupPdfWorkflowRelativePath)
                    );
            if (!QFile::exists(*pdfPath))
            {
                fail(
                    QStringLiteral("pdf-not-found: %1")
                        .arg(QString::fromUtf8(StartupPdfWorkflowRelativePath))
                    );
                return;
            }

            PdfViewerDocumentDescriptor descriptor;
            descriptor.pdfFilePath = *pdfPath;
            descriptor.printEnabled = true;
            descriptor.exportEnabled = true;
            descriptor.exportFilePath = *pdfPath;
            descriptor.exportFileName =
                QStringLiteral("DYB Lesson Planning Guide.pdf");
            descriptor.resourceLease = std::move(*lease);

            profiler.checkpoint(
                QStringLiteral("pdf-open-start"),
                QString::fromUtf8(StartupPdfWorkflowRelativePath)
                );
            if (!viewer->loadPdf(std::move(descriptor)))
            {
                fail(QStringLiteral("initial-open-rejected"));
                return;
            }

            *phase = 1;
            QTimer::singleShot(
                StartupWorkflowStepDelayMilliseconds,
                &app,
                [runPhase]()
                {
                    (*runPhase)();
                }
                );
            return;
        }

        if (*phase == 1)
        {
            app.processEvents();
            if (!viewer->hasLoadedDocument())
            {
                fail(QStringLiteral("initial-open-not-ready"));
                return;
            }

            const QPixmap renderedImage = viewer->grab();
            if (
                renderedImage.isNull()
                || renderedImage.size().isEmpty()
                )
            {
                fail(QStringLiteral("initial-render-empty"));
                return;
            }

            if (
                !saveStartupPdfCapture(
                    renderedImage,
                    QStringLiteral("pdf-opened.png")
                    )
                )
            {
                fail(QStringLiteral("initial-render-capture-failed"));
                return;
            }

            StartupProfiler::recordPdfDocumentRendered(
                *pdfPath,
                renderedImage.width(),
                renderedImage.height()
                );
            profiler.checkpoint(
                QStringLiteral("pdf-opened"),
                QStringLiteral("%1; rendered=%2x%3")
                    .arg(
                        QString::fromUtf8(StartupPdfWorkflowRelativePath),
                        QString::number(renderedImage.width()),
                        QString::number(renderedImage.height())
                        )
                );
            profiler.checkpoint(
                QStringLiteral("pdf-rendered"),
                QStringLiteral("%1x%2")
                    .arg(
                        renderedImage.width(),
                        renderedImage.height()
                        )
                );

            viewer->releaseDocument();
            *phase = 2;
            QTimer::singleShot(
                StartupWorkflowStepDelayMilliseconds,
                &app,
                [runPhase]()
                {
                    (*runPhase)();
                }
                );
            return;
        }

        if (*phase == 2)
        {
            app.processEvents();
            if (
                viewer->hasLoadedDocument()
                || !viewer->currentFilePath().isEmpty()
                )
            {
                fail(QStringLiteral("initial-release-incomplete"));
                return;
            }

            profiler.checkpoint(
                QStringLiteral("pdf-released"),
                QString::fromUtf8(StartupPdfWorkflowRelativePath)
                );

            auto lease = ResourcePaths::Documents::acquire();
            if (!lease)
            {
                fail(
                    QStringLiteral("documents-pack-unavailable-on-reopen: %1")
                        .arg(lease.error())
                    );
                return;
            }

            PdfViewerDocumentDescriptor descriptor;
            descriptor.pdfFilePath = *pdfPath;
            descriptor.printEnabled = true;
            descriptor.exportEnabled = true;
            descriptor.exportFilePath = *pdfPath;
            descriptor.exportFileName =
                QStringLiteral("DYB Lesson Planning Guide.pdf");
            descriptor.resourceLease = std::move(*lease);

            profiler.checkpoint(
                QStringLiteral("pdf-reopen-start"),
                QString::fromUtf8(StartupPdfWorkflowRelativePath)
                );
            if (!viewer->loadPdf(std::move(descriptor)))
            {
                fail(QStringLiteral("reopen-rejected"));
                return;
            }

            *phase = 3;
            QTimer::singleShot(
                StartupWorkflowStepDelayMilliseconds,
                &app,
                [runPhase]()
                {
                    (*runPhase)();
                }
                );
            return;
        }

        if (*phase == 3)
        {
            app.processEvents();
            if (!viewer->hasLoadedDocument())
            {
                fail(QStringLiteral("reopen-not-ready"));
                return;
            }

            const QPixmap renderedImage = viewer->grab();
            if (
                renderedImage.isNull()
                || renderedImage.size().isEmpty()
                )
            {
                fail(QStringLiteral("reopen-render-empty"));
                return;
            }

            if (
                !saveStartupPdfCapture(
                    renderedImage,
                    QStringLiteral("pdf-reopened.png")
                    )
                )
            {
                fail(QStringLiteral("reopen-render-capture-failed"));
                return;
            }

            StartupProfiler::recordPdfDocumentRendered(
                *pdfPath,
                renderedImage.width(),
                renderedImage.height()
                );
            profiler.checkpoint(
                QStringLiteral("pdf-reopened"),
                QStringLiteral("%1; rendered=%2x%3")
                    .arg(
                        QString::fromUtf8(StartupPdfWorkflowRelativePath),
                        QString::number(renderedImage.width()),
                        QString::number(renderedImage.height())
                        )
                );
            profiler.checkpoint(
                QStringLiteral("pdf-reopened-rendered"),
                QStringLiteral("%1x%2")
                    .arg(
                        renderedImage.width(),
                        renderedImage.height()
                        )
                );

            viewer->releaseDocument();
            *phase = 4;
            QTimer::singleShot(
                StartupWorkflowStepDelayMilliseconds,
                &app,
                [runPhase]()
                {
                    (*runPhase)();
                }
                );
            return;
        }

        app.processEvents();
        if (
            viewer->hasLoadedDocument()
            || !viewer->currentFilePath().isEmpty()
            )
        {
            fail(QStringLiteral("reopen-release-incomplete"));
            return;
        }

        profiler.checkpoint(
            QStringLiteral("pdf-released-after-reopen"),
            QString::fromUtf8(StartupPdfWorkflowRelativePath)
            );
        profiler.checkpoint(
            QStringLiteral("pdf-workflow-complete"),
            QStringLiteral("opened=2; rendered=2; released=2")
            );
        completion();
    };

    (*runPhase)();
}

StartupPerformanceMode startupPerformanceMode(
    const QStringList& args
    )
{
    StartupPerformanceMode mode;

    mode.enabled =
        args.contains(
            QStringLiteral("--startup-performance-test")
            );
    mode.workflowEnabled =
        args.contains(
            QStringLiteral("--startup-performance-workflow")
            );
    mode.scheduleLifecycleEnabled =
        args.contains(
            QStringLiteral("--startup-performance-schedule-lifecycle")
            );
    mode.scheduleImportLifecycleEnabled =
        args.contains(
            QStringLiteral("--startup-performance-schedule-import-lifecycle")
            );
    mode.scheduleImportApplyLifecycleEnabled =
        args.contains(
            QStringLiteral(
                "--startup-performance-schedule-import-apply-lifecycle"
                )
            );
    mode.calendarImportLifecycleEnabled =
        args.contains(
            QStringLiteral("--startup-performance-calendar-import-lifecycle")
            );
    mode.classesLifecycleEnabled =
        args.contains(
            QStringLiteral("--startup-performance-classes-lifecycle")
            );
    mode.classTransferLifecycleEnabled =
        args.contains(
            QStringLiteral("--startup-performance-class-transfer-lifecycle")
            );
    mode.speakingEvaluationLifecycleEnabled =
        args.contains(
            QStringLiteral(
                "--startup-performance-speaking-evaluation-lifecycle"
                )
            );
    mode.staffDirectoryLifecycleEnabled =
        args.contains(
            QStringLiteral("--startup-performance-staff-directory-lifecycle")
            );
    mode.subPrepOutputLifecycleEnabled =
        args.contains(
            QStringLiteral(
                "--startup-performance-sub-prep-output-lifecycle"
                )
            );
    mode.subPrepVisualStatesEnabled =
        args.contains(
            QStringLiteral(
                "--startup-performance-sub-prep-visual-states"
                )
            );
    mode.subPrepLifecycleEnabled =
        args.contains(
            QStringLiteral("--startup-performance-sub-prep-lifecycle")
            )
        || mode.subPrepOutputLifecycleEnabled;
    mode.enabled =
        mode.enabled
        || mode.workflowEnabled
        || mode.scheduleLifecycleEnabled
        || mode.scheduleImportLifecycleEnabled
        || mode.scheduleImportApplyLifecycleEnabled
        || mode.calendarImportLifecycleEnabled
        || mode.classesLifecycleEnabled
        || mode.classTransferLifecycleEnabled
        || mode.speakingEvaluationLifecycleEnabled
        || mode.staffDirectoryLifecycleEnabled
        || mode.subPrepLifecycleEnabled
        || mode.subPrepOutputLifecycleEnabled
        || mode.subPrepVisualStatesEnabled;

    const int outputIndex =
        args.indexOf(
            QStringLiteral("--startup-performance-output")
            );

    if (
        outputIndex >= 0
        && outputIndex + 1 < args.size()
        )
    {
        mode.outputPath =
            args.at(outputIndex + 1);
    }

    const int visualCaptureIndex =
        args.indexOf(
            QStringLiteral("--startup-visual-capture-output")
            );
    if (visualCaptureIndex >= 0)
    {
        mode.enabled = true;
        mode.visualCaptureEnabled = true;

        if (visualCaptureIndex + 1 < args.size())
        {
            mode.visualCaptureOutputPath =
                args.at(visualCaptureIndex + 1);
        }
    }

    const int visualLanguageIndex =
        args.indexOf(
            QStringLiteral("--startup-visual-capture-language")
            );
    if (visualLanguageIndex >= 0 && visualLanguageIndex + 1 < args.size())
    {
        const QString language =
            args.at(visualLanguageIndex + 1).trimmed().toLower();
        if (language == QStringLiteral("english"))
        {
            mode.visualLanguageOverride = Language::English;
        }
        else if (language == QStringLiteral("korean"))
        {
            mode.visualLanguageOverride = Language::Korean;
        }
        else
        {
            qWarning().noquote()
                << QStringLiteral(
                    "Unknown startup visual capture language '%1'."
                    ).arg(language);
        }
    }

    const int visualThemeIndex =
        args.indexOf(
            QStringLiteral("--startup-visual-capture-theme")
            );
    if (visualThemeIndex >= 0 && visualThemeIndex + 1 < args.size())
    {
        const QString theme =
            args.at(visualThemeIndex + 1).trimmed().toLower();
        if (theme == QStringLiteral("light"))
        {
            mode.visualThemeOverride = Theme::Light;
        }
        else if (theme == QStringLiteral("dark"))
        {
            mode.visualThemeOverride = Theme::Dark;
        }
        else
        {
            qWarning().noquote()
                << QStringLiteral(
                    "Unknown startup visual capture theme '%1'."
                    ).arg(theme);
        }
    }

    const int scenarioIndex =
        args.indexOf(
            QStringLiteral("--startup-performance-scenario")
            );
    if (scenarioIndex >= 0 && scenarioIndex + 1 < args.size())
    {
        const QString scenario = args.at(scenarioIndex + 1).trimmed();
        if (scenario == QStringLiteral("representative"))
        {
            mode.scenario = StartupPerformanceMode::Scenario::Representative;
            mode.settleMilliseconds = 30000;
        }
        else if (scenario != QStringLiteral("minimal"))
        {
            qWarning().noquote()
                << QStringLiteral(
                    "Unknown startup profiling scenario '%1'; using minimal."
                    ).arg(scenario);
        }
    }

    // A supplied database makes a visual capture representative unless the
    // caller explicitly requested the minimal scenario.
    if (
        mode.visualCaptureEnabled
        && scenarioIndex < 0
        && !startupDatabasePath(args).trimmed().isEmpty()
        )
    {
        mode.scenario = StartupPerformanceMode::Scenario::Representative;
        mode.settleMilliseconds = 30000;
    }

    if (
        mode.workflowEnabled
        && scenarioIndex < 0
        && !startupDatabasePath(args).trimmed().isEmpty()
        )
    {
        mode.scenario = StartupPerformanceMode::Scenario::Representative;
        mode.settleMilliseconds = 5000;
    }

    const int settleIndex =
        args.indexOf(
            QStringLiteral("--startup-performance-settle-ms")
            );
    if (settleIndex >= 0 && settleIndex + 1 < args.size())
    {
        bool converted = false;
        const int settleMilliseconds = args.at(settleIndex + 1).toInt(&converted);
        if (converted && settleMilliseconds >= 0)
        {
            mode.settleMilliseconds = settleMilliseconds;
        }
        else
        {
            qWarning().noquote()
                << QStringLiteral(
                    "Ignoring invalid --startup-performance-settle-ms value '%1'."
                    ).arg(args.at(settleIndex + 1));
        }
    }

    return mode;
}

bool captureStartupVisual(
    const QString& outputDirectoryPath,
    QWidget& window,
    const QString& checkpointName
    )
{
    if (outputDirectoryPath.trimmed().isEmpty())
    {
        qWarning()
            << "Startup visual capture output directory was not provided.";
        return false;
    }

    QDir outputDirectory(outputDirectoryPath);
    if (!outputDirectory.mkpath(QStringLiteral(".")))
    {
        qWarning().noquote()
            << QStringLiteral(
                "Unable to create startup visual capture directory %1."
                ).arg(outputDirectoryPath);
        return false;
    }

    const QPixmap screenshot = window.grab();
    if (screenshot.isNull() || screenshot.size().isEmpty())
    {
        qWarning()
            << "Startup visual capture returned an empty window image.";
        return false;
    }

    const QString outputPath =
        outputDirectory.filePath(
            QStringLiteral("%1.png").arg(checkpointName)
            );
    if (!screenshot.save(outputPath, "PNG"))
    {
        qWarning().noquote()
            << QStringLiteral(
                "Unable to write startup visual capture to %1."
                ).arg(outputPath);
        return false;
    }

    qInfo().noquote()
        << QStringLiteral(
            "Wrote startup visual capture %1 (%2x%3)."
            )
            .arg(
                outputPath,
                QString::number(screenshot.width()),
                QString::number(screenshot.height())
                );
    return true;
}

void scheduleStartupPerformanceSubPrepVisualStates(
    QApplication& app,
    MainWindow& window,
    StartupProfiler& profiler,
    const std::shared_ptr<bool>& workflowSucceeded,
    const QString& outputDirectoryPath,
    std::function<void()> completion
    )
{
    QTimer::singleShot(
        0,
        &app,
        [
            &app,
            &window,
            &profiler,
            workflowSucceeded,
            outputDirectoryPath,
            completion
        ]()
        {
            PageManager* pages = window.pageManager();
            SubPrepPage* page = pages ? pages->subPrepPage() : nullptr;

            if (
                !pages
                || !page
                || !pages->isCurrentPage(PageType::SubPrep)
                )
            {
                *workflowSucceeded = false;
                profiler.checkpoint(
                    QStringLiteral("sub-prep-visual-states-failed"),
                    QStringLiteral("sub-prep-page-not-current")
                    );
                StartupProfiler::setSubPrepDiagnosticsActive(false);
                completion();
                return;
            }

            bool visualStatesSucceeded = true;
            const auto captureState =
                [
                    &app,
                    &window,
                    &page,
                    &profiler,
                    &visualStatesSucceeded,
                    &outputDirectoryPath
                ](const QString& stateName)
                {
                    page->scrollToClassInformationForStartupDiagnostics(
                        stateName == QStringLiteral("empty")
                        );
                    app.processEvents();
                    const SubPrepPageRuntimeMetrics metrics =
                        page->runtimeMetrics();
                    const bool captured =
                        captureStartupVisual(
                            outputDirectoryPath,
                            window,
                            QStringLiteral("sub-prep-%1").arg(stateName)
                            );
                    if (!captured)
                    {
                        visualStatesSucceeded = false;
                    }

                    const QString detail =
                        QStringLiteral(
                            "state=%1; visibleClasses=%2; selectedClassId=%3; "
                            "widgets=%4; textEdits=%5; captured=%6"
                            )
                            .arg(stateName)
                            .arg(metrics.classInformationVisibleClassCount)
                            .arg(metrics.selectedClassId)
                            .arg(metrics.classInformationWidgetCount)
                            .arg(metrics.classInformationTextEditCount)
                            .arg(
                                captured
                                    ? QStringLiteral("true")
                                    : QStringLiteral("false")
                                );
                    profiler.checkpoint(
                        QStringLiteral("sub-prep-visual-%1").arg(stateName),
                        detail
                        );
                    appendStartupWorkflowTrace(
                        QStringLiteral(
                            "sub-prep-visual-%1 visibleClasses=%2 selectedClassId=%3 captured=%4"
                            )
                            .arg(stateName)
                            .arg(metrics.classInformationVisibleClassCount)
                            .arg(metrics.selectedClassId)
                            .arg(
                                captured
                                    ? QStringLiteral("true")
                                    : QStringLiteral("false")
                                )
                        );
                };

            const SubPrepPageRuntimeMetrics initialMetrics =
                page->runtimeMetrics();
            appendStartupWorkflowTrace(
                QStringLiteral("sub-prep-visual-states-start")
                );
            profiler.checkpoint(
                QStringLiteral("sub-prep-visual-states-start"),
                QStringLiteral(
                    "visibleClasses=%1; selectedClassId=%2"
                    )
                    .arg(initialMetrics.classInformationVisibleClassCount)
                    .arg(initialMetrics.selectedClassId)
                );

            if (initialMetrics.classInformationVisibleClassCount <= 0)
            {
                captureState(QStringLiteral("empty"));
            }
            else
            {
                if (initialMetrics.selectedClassId <= 0)
                {
                    const bool selected =
                        page->selectClassForStartupDiagnostics(1);
                    app.processEvents();
                    if (!selected)
                    {
                        visualStatesSucceeded = false;
                    }
                }

                const SubPrepPageRuntimeMetrics selectedMetrics =
                    page->runtimeMetrics();
                if (selectedMetrics.selectedClassId <= 0)
                {
                    visualStatesSucceeded = false;
                    profiler.checkpoint(
                        QStringLiteral("sub-prep-visual-states-failed"),
                        QStringLiteral("populated-state-has-no-selection")
                        );
                }
                else
                {
                    captureState(QStringLiteral("selected"));

                    const int changedClassId =
                        selectedMetrics.selectedClassId == 1 ? 48 : 1;
                    const bool changed =
                        page->selectClassForStartupDiagnostics(changedClassId);
                    app.processEvents();
                    if (
                        !changed
                        || page->runtimeMetrics().selectedClassId
                            == selectedMetrics.selectedClassId
                        )
                    {
                        visualStatesSucceeded = false;
                        profiler.checkpoint(
                            QStringLiteral("sub-prep-visual-states-failed"),
                            QStringLiteral(
                                "changed-selection-failed; requestedClassId=%1; selectedClassId=%2"
                                )
                                .arg(changedClassId)
                                .arg(page->runtimeMetrics().selectedClassId)
                            );
                    }
                    else
                    {
                        captureState(QStringLiteral("changed-selection"));
                    }
                }
            }

            if (!visualStatesSucceeded)
            {
                *workflowSucceeded = false;
            }
            profiler.checkpoint(
                QStringLiteral("sub-prep-visual-states-complete"),
                QStringLiteral(
                    "passed=%1; visibleClasses=%2; selectedClassId=%3"
                    )
                    .arg(
                        visualStatesSucceeded
                            ? QStringLiteral("true")
                            : QStringLiteral("false")
                        )
                    .arg(page->runtimeMetrics().classInformationVisibleClassCount)
                    .arg(page->runtimeMetrics().selectedClassId)
                );
            appendStartupWorkflowTrace(
                QStringLiteral(
                    "sub-prep-visual-states-complete passed=%1"
                    )
                    .arg(
                        visualStatesSucceeded
                            ? QStringLiteral("true")
                            : QStringLiteral("false")
                        )
                );
            StartupProfiler::setSubPrepDiagnosticsActive(false);
            completion();
        }
        );
}

void scheduleStartupPerformanceScheduleLifecycle(
    QApplication& app,
    MainWindow& window,
    StartupProfiler& profiler,
    const std::shared_ptr<bool>& workflowSucceeded,
    std::function<void()> completion
    )
{
    QTimer::singleShot(
        0,
        &app,
        [
            &app,
            &window,
            &profiler,
            workflowSucceeded,
            completion
        ]()
        {
            PageManager* pages = window.pageManager();
            SchedulePage* page = pages ? pages->schedulePage() : nullptr;

            if (
                !pages
                || !page
                || !pages->isCurrentPage(PageType::Schedule)
                )
            {
                *workflowSucceeded = false;
                profiler.checkpoint(
                    QStringLiteral("schedule-lifecycle-failed"),
                    QStringLiteral("schedule-page-not-current")
                    );
                completion();
                return;
            }

            bool lifecycleSucceeded = true;
            const auto metricsDetail =
                [&page]()
                {
                    const ScheduleWidgetRuntimeMetrics metrics =
                        page->runtimeMetrics();
                    return QStringLiteral(
                        "modelRows=%1; modelCells=%2; modelEntries=%3; "
                        "tableRows=%4; tableColumns=%5; tableItems=%6; "
                        "tableCellWidgets=%7; visibleClasses=%8"
                        )
                        .arg(metrics.modelRowCount)
                        .arg(metrics.modelCellCount)
                        .arg(metrics.modelEntryCount)
                        .arg(metrics.tableRowCount)
                        .arg(metrics.tableColumnCount)
                        .arg(metrics.tableItemCount)
                        .arg(metrics.tableCellWidgetCount)
                        .arg(metrics.visibleClassCount);
                };
            const auto checkpoint =
                [&profiler](
                    const QString& name,
                    const QString& detail = QString()
                    )
                {
                    profiler.checkpoint(name, detail);
                };

            const auto refresh =
                [
                    &app,
                    &page,
                    metricsDetail,
                    checkpoint
                ](int ordinal)
                {
                    const QString startName =
                        QStringLiteral("schedule-refresh-%1-start")
                            .arg(ordinal);
                    const QString completeName =
                        QStringLiteral("schedule-refresh-%1-complete")
                            .arg(ordinal);
                    appendStartupWorkflowTrace(startName);
                    checkpoint(startName, metricsDetail());
                    page->refresh();
                    app.processEvents();
                    appendStartupWorkflowTrace(completeName);
                    checkpoint(completeName, metricsDetail());
                };

            const auto navigate =
                [
                    &app,
                    &pages,
                    &lifecycleSucceeded,
                    metricsDetail,
                    checkpoint
                ](
                    PageType destination,
                    const QString& operation
                    )
                {
                    pages->showPage(destination);
                    app.processEvents();
                    const bool reached = pages->isCurrentPage(destination);
                    appendStartupWorkflowTrace(
                        QStringLiteral("%1 returned=%2")
                            .arg(
                                operation,
                                reached ? QStringLiteral("true")
                                        : QStringLiteral("false")
                                )
                        );
                    if (!reached)
                    {
                        lifecycleSucceeded = false;
                    }
                    checkpoint(
                        operation,
                        QStringLiteral("currentPage=%1; passed=%2; %3")
                            .arg(pages->currentPageIdentifier())
                            .arg(reached ? QStringLiteral("true")
                                         : QStringLiteral("false"))
                            .arg(metricsDetail())
                        );
                };

            appendStartupWorkflowTrace(
                QStringLiteral("schedule-lifecycle-start")
                );
            checkpoint(
                QStringLiteral("schedule-lifecycle-entry"),
                metricsDetail()
                );
            refresh(1);
            navigate(
                PageType::MyWorkspace,
                QStringLiteral("schedule-left-1")
                );
            navigate(
                PageType::Schedule,
                QStringLiteral("schedule-reentry-1")
                );
            refresh(2);
            navigate(
                PageType::MyWorkspace,
                QStringLiteral("schedule-left-2")
                );
            navigate(
                PageType::Schedule,
                QStringLiteral("schedule-reentry-2")
                );

            appendStartupWorkflowTrace(
                QStringLiteral("schedule-lifecycle-complete")
                );
            checkpoint(
                QStringLiteral("schedule-lifecycle-complete"),
                QStringLiteral("passed=%1; %2")
                    .arg(
                        lifecycleSucceeded
                            ? QStringLiteral("true")
                            : QStringLiteral("false")
                        )
                    .arg(metricsDetail())
                );
            if (!lifecycleSucceeded)
            {
                *workflowSucceeded = false;
            }
            completion();
        }
        );
}

void scheduleStartupPerformanceClassesLifecycle(
    QApplication& app,
    MainWindow& window,
    StartupProfiler& profiler,
    const std::shared_ptr<bool>& workflowSucceeded,
    std::function<void()> completion
    )
{
    QTimer::singleShot(
        0,
        &app,
        [
            &app,
            &window,
            &profiler,
            workflowSucceeded,
            completion
        ]()
        {
            PageManager* pages = window.pageManager();
            ClassesPage* page = pages ? pages->classesPage() : nullptr;

            if (
                !pages
                || !page
                || !pages->isCurrentPage(PageType::Classes)
                )
            {
                *workflowSucceeded = false;
                profiler.checkpoint(
                    QStringLiteral("classes-lifecycle-failed"),
                    QStringLiteral("classes-page-not-current")
                    );
                completion();
                return;
            }

            bool lifecycleSucceeded = true;
            const auto checkpoint =
                [&profiler](
                    const QString& name,
                    const QString& detail = QString()
                    )
                {
                    profiler.checkpoint(name, detail);
                };

            const auto selectClass =
                [
                    &app,
                    &page,
                    &lifecycleSucceeded,
                    checkpoint
                ](int classId)
                {
                    const bool selected =
                        page->selectClassForStartupDiagnostics(classId);
                    app.processEvents();
                    appendStartupWorkflowTrace(
                        QStringLiteral(
                            "classes-select classId=%1 returned=%2"
                            )
                            .arg(classId)
                            .arg(selected ? QStringLiteral("true")
                                          : QStringLiteral("false"))
                        );
                    if (!selected)
                    {
                        lifecycleSucceeded = false;
                    }
                    checkpoint(
                        QStringLiteral("classes-selection-%1").arg(classId),
                        QStringLiteral(
                            "requestedClassId=%1; selectedClassId=%2; passed=%3"
                            )
                            .arg(classId)
                            .arg(page->runtimeMetrics().selectedClassId)
                            .arg(selected ? QStringLiteral("true")
                                          : QStringLiteral("false"))
                        );
                };

            const auto refresh =
                [
                    &app,
                    &page,
                    checkpoint
                ](int ordinal)
                {
                    const QString startName =
                        QStringLiteral("classes-refresh-%1-start")
                            .arg(ordinal);
                    const QString completeName =
                        QStringLiteral("classes-refresh-%1-complete")
                            .arg(ordinal);
                    appendStartupWorkflowTrace(startName);
                    checkpoint(
                        startName,
                        QStringLiteral("selectedClassId=%1")
                            .arg(page->runtimeMetrics().selectedClassId)
                        );
                    page->refresh();
                    app.processEvents();
                    appendStartupWorkflowTrace(completeName);
                    checkpoint(
                        completeName,
                        QStringLiteral("selectedClassId=%1")
                            .arg(page->runtimeMetrics().selectedClassId)
                        );
                };

            const auto navigate =
                [
                    &app,
                    &pages,
                    &page,
                    &lifecycleSucceeded,
                    checkpoint
                ](
                    PageType destination,
                    const QString& operation
                    )
                {
                    pages->showPage(destination);
                    app.processEvents();
                    const bool reached = pages->isCurrentPage(destination);
                    appendStartupWorkflowTrace(
                        QStringLiteral("%1 returned=%2")
                            .arg(
                                operation,
                                reached ? QStringLiteral("true")
                                        : QStringLiteral("false")
                                )
                        );
                    if (!reached)
                    {
                        lifecycleSucceeded = false;
                    }
                    checkpoint(
                        operation,
                        QStringLiteral(
                            "currentPage=%1; selectedClassId=%2; passed=%3"
                            )
                            .arg(pages->currentPageIdentifier())
                            .arg(page->runtimeMetrics().selectedClassId)
                            .arg(reached ? QStringLiteral("true")
                                         : QStringLiteral("false"))
                        );
                };

            appendStartupWorkflowTrace(
                QStringLiteral("classes-lifecycle-start")
                );
            checkpoint(
                QStringLiteral("classes-lifecycle-entry"),
                QStringLiteral("selectedClassId=%1")
                    .arg(page->runtimeMetrics().selectedClassId)
                );

            selectClass(96);
            refresh(1);
            navigate(
                PageType::MyWorkspace,
                QStringLiteral("classes-left-1")
                );
            navigate(
                PageType::Classes,
                QStringLiteral("classes-reentry-1")
                );
            selectClass(1);
            refresh(2);
            navigate(
                PageType::MyWorkspace,
                QStringLiteral("classes-left-2")
                );
            navigate(
                PageType::Classes,
                QStringLiteral("classes-reentry-2")
                );

            appendStartupWorkflowTrace(
                QStringLiteral("classes-lifecycle-complete")
                );
            checkpoint(
                QStringLiteral("classes-lifecycle-complete"),
                QStringLiteral("passed=%1; selectedClassId=%2")
                    .arg(
                        lifecycleSucceeded
                            ? QStringLiteral("true")
                            : QStringLiteral("false")
                        )
                    .arg(page->runtimeMetrics().selectedClassId)
                );
            if (!lifecycleSucceeded)
            {
                *workflowSucceeded = false;
            }
            completion();
        }
        );
}

void scheduleStartupPerformanceSpeakingEvaluationLifecycle(
    QApplication& app,
    MainWindow& window,
    StartupProfiler& profiler,
    const std::shared_ptr<bool>& workflowSucceeded,
    std::function<void()> completion
    )
{
    QTimer::singleShot(
        0,
        &app,
        [
            &app,
            &window,
            &profiler,
            workflowSucceeded,
            completion
        ]()
        {
            PageManager* pages = window.pageManager();
            ClassesPage* classesPage =
                pages ? pages->classesPage() : nullptr;
            ApplicationServices* services = window.services();
            ClassService* classService = services
                ? services->classService()
                : nullptr;
            SpeakingEvaluationService* evaluationService = services
                ? services->speakingEvaluationService()
                : nullptr;

            const auto fail =
                [
                    &completion,
                    &workflowSucceeded
                ](const QString& detail)
                {
                    *workflowSucceeded = false;
                    StartupProfiler::recordSpeakingEvaluationFailed(detail);
                    StartupProfiler::recordSpeakingEvaluationOperationReleased();
                    completion();
                };

            if (
                !pages
                || !classesPage
                || !classService
                || !evaluationService
                || !classService->isAvailable()
                || !evaluationService->isAvailable()
                || !pages->isCurrentPage(PageType::Classes)
                )
            {
                *workflowSucceeded = false;
                profiler.checkpoint(
                    QStringLiteral("speaking-evaluation-lifecycle-failed"),
                    QStringLiteral(
                        "classesPageCurrent=%1; classServiceAvailable=%2; evaluationServiceAvailable=%3"
                        )
                        .arg(
                            pages && pages->isCurrentPage(PageType::Classes)
                                ? QStringLiteral("true")
                                : QStringLiteral("false")
                            )
                        .arg(
                            classService && classService->isAvailable()
                                ? QStringLiteral("true")
                                : QStringLiteral("false")
                            )
                        .arg(
                            evaluationService && evaluationService->isAvailable()
                                ? QStringLiteral("true")
                                : QStringLiteral("false")
                            )
                    );
                completion();
                return;
            }

            StartupProfiler::recordSpeakingEvaluationOperationStarted();
            appendStartupWorkflowTrace(
                QStringLiteral("speaking-evaluation-source-opened")
                );

            if (!classesPage->openClass(1, ClassesSection::Evaluations))
            {
                fail(QStringLiteral("classes-evaluation-section-not-opened"));
                return;
            }
            app.processEvents();

            SpeakingEvalPage* evaluationPage =
                classesPage->findChild<SpeakingEvalPage*>();
            SpeakingEvalModel* model = evaluationPage
                ? evaluationPage->findChild<SpeakingEvalModel*>()
                : nullptr;
            SpeakingEvalTableView* table = evaluationPage
                ? evaluationPage->findChild<SpeakingEvalTableView*>()
                : nullptr;

            if (!evaluationPage || !model || !table)
            {
                fail(QStringLiteral("speaking-evaluation-page-controls-missing"));
                return;
            }

            int classTabWidgetCount = 0;
            int classTabCount = 0;
            int evaluationTabCount = 0;
            for (
                NavigationTabWidget* tabs :
                evaluationPage->findChildren<NavigationTabWidget*>()
                )
            {
                if (!tabs)
                {
                    continue;
                }
                if (tabs->objectName() == QStringLiteral("speakingEvalClassTabs"))
                {
                    ++classTabWidgetCount;
                    classTabCount += tabs->count();
                }
                else if (
                    tabs->objectName()
                        == QStringLiteral("classEvaluationsEvaluationTabs")
                    )
                {
                    evaluationTabCount += tabs->count();
                }
            }

            const SpeakingEvalRows displayedRows = model->rows();
            const auto matrixCellCount =
                [](const SpeakingEvalRows& rows)
                {
                    int cells = 0;
                    for (const QStringList& row : rows)
                    {
                        cells += row.size();
                    }
                    return cells;
                };

            StartupProfiler::recordSpeakingEvaluationPagePrepared(
                classesPage->runtimeMetrics().sourceClassCount,
                classesPage->runtimeMetrics().visibleClassCount,
                classTabWidgetCount,
                classTabCount,
                evaluationTabCount,
                model->rowCount(),
                model->columnCount(),
                model->rowCount() * model->columnCount(),
                1,
                displayedRows.size(),
                matrixCellCount(displayedRows)
                );
            appendStartupWorkflowTrace(
                QStringLiteral("speaking-evaluation-page-ready")
                );

            const QString configuredOutputRoot =
                qEnvironmentVariable(
                    "CLASSMNGR_STARTUP_SPEAKING_EVALUATION_OUTPUT_DIR"
                    ).trimmed();
            const QString outputRoot = configuredOutputRoot.isEmpty()
                ? QDir(QDir::tempPath()).filePath(
                    QStringLiteral("ClassMngr-speaking-evaluation-boundary")
                    )
                : QFileInfo(configuredOutputRoot).absoluteFilePath();
            if (!QDir().mkpath(outputRoot))
            {
                fail(
                    QStringLiteral("speaking-evaluation-output-directory-unavailable")
                    );
                return;
            }
            evaluationPage->grab().save(
                QDir(outputRoot).filePath(
                    QStringLiteral("speaking-evaluation-page.png")
                    ),
                "PNG"
                );

            const Result<SpeakingEvalRows> loadedEvaluation =
                evaluationService->evaluation(
                    classesPage->currentClassId(),
                    QStringLiteral("Winter")
                    );
            if (!loadedEvaluation)
            {
                fail(loadedEvaluation.error());
                return;
            }

            const Result<ClassInfo> loadedClassInfo =
                classService->classInfo(classesPage->currentClassId());
            if (!loadedClassInfo)
            {
                fail(loadedClassInfo.error());
                return;
            }

            QList<SpeakingEvalBatchReportService::StudentReport> reports =
                buildSpeakingEvalStudentReports(
                    *loadedEvaluation,
                    *loadedClassInfo
                    );
            if (reports.isEmpty())
            {
                fail(QStringLiteral("speaking-evaluation-report-batch-empty"));
                return;
            }

            const auto reportTextBytes =
                [](const QList<SpeakingEvalBatchReportService::StudentReport>&
                   reportList)
                {
                    qint64 bytes = 0;
                    const auto add =
                        [&bytes](const QString& value)
                        {
                            bytes += value.toUtf8().size();
                        };
                    for (const auto& student : reportList)
                    {
                        const SpeakingEvalReportData& report = student.report;
                        add(student.displayName);
                        add(report.englishName);
                        add(report.koreanName);
                        add(report.classLabel);
                        add(report.nativeTeacher);
                        add(report.koreanTeacher);
                        add(report.date);
                        add(report.comments);
                        add(report.notes);
                        for (const QString& score : report.scores)
                        {
                            add(score);
                        }
                        bytes += report.signatureImage.size();
                    }
                    return bytes;
                };

            StartupProfiler::recordSpeakingEvaluationReportsPrepared(
                reports.size(),
                reportTextBytes(reports)
                );
            appendStartupWorkflowTrace(
                QStringLiteral("speaking-evaluation-reports-ready")
                );

            {
                StartupProfiler::recordSpeakingEvaluationReportDialogPrepared(
                    reports.size()
                    );
                SpeakingEvalReportDialog dialog(
                    reports,
                    0,
                    evaluationPage,
                    true
                    );
                dialog.show();
                app.processEvents();
                dialog.grab().save(
                    QDir(outputRoot).filePath(
                        QStringLiteral("speaking-evaluation-report.png")
                        ),
                    "PNG"
                    );
                dialog.reject();
                app.processEvents();
                StartupProfiler::recordSpeakingEvaluationReportDialogReleased();
            }

            const QString reportOutputDirectory =
                QDir(outputRoot).filePath(
                    QStringLiteral("speaking-evaluation-output")
                    );
            if (!QDir().mkpath(reportOutputDirectory))
            {
                fail(QStringLiteral("speaking-evaluation-report-output-unavailable"));
                return;
            }

            {
                StartupProfiler::recordSpeakingEvaluationExportDialogPrepared(
                    reports.size()
                    );
                SpeakingEvalBatchExportDialog dialog(
                    reports,
                    0,
                    reportOutputDirectory,
                    SpeakingEvalBatchExportDialog::Mode::SaveAs,
                    evaluationPage
                    );
                dialog.show();
                app.processEvents();
                dialog.grab().save(
                    QDir(outputRoot).filePath(
                        QStringLiteral("speaking-evaluation-export-dialog.png")
                        ),
                    "PNG"
                    );
                dialog.reject();
                app.processEvents();
                StartupProfiler::recordSpeakingEvaluationExportDialogReleased();
            }

            StartupProfiler::recordSpeakingEvaluationExportStarted(
                reports.size()
                );
            SpeakingEvalBatchReportService::Request exportRequest;
            exportRequest.parent = evaluationPage;
            exportRequest.reports = reports;
            exportRequest.renderer =
                SpeakingEvalBatchReportService::Renderer::Internal;
            exportRequest.savePdf = true;
            exportRequest.keepIndividualPdfFiles = true;
            exportRequest.overwriteExisting = true;
            exportRequest.outputDirectory = reportOutputDirectory;
            exportRequest.progressCallback =
                [&app](int, int, const QString&)
                {
                    app.processEvents();
                    return true;
                };
            const SpeakingEvalBatchReportService::Result exportResult =
                SpeakingEvalBatchReportService::exportReports(exportRequest);
            if (!exportResult.succeeded())
            {
                fail(exportResult.message);
                return;
            }

            qint64 exportedPdfBytes = 0;
            for (const QString& path : exportResult.savedPdfPaths)
            {
                exportedPdfBytes += QFileInfo(path).size();
            }
            const qint64 exportedArchiveBytes =
                QFileInfo(exportResult.savedArchivePath).size();
            StartupProfiler::recordSpeakingEvaluationExportCompleted(
                exportResult.savedPdfPaths.size(),
                exportedPdfBytes,
                exportedArchiveBytes
                );
            appendStartupWorkflowTrace(
                QStringLiteral("speaking-evaluation-reports-exported")
                );

            QList<SpeakingEvalAiBatchAcceptedComment> acceptedComments;
            QString aiError;
            {
                SpeakingEvalAiBatchDialog dialog(
                    reports,
                    evaluationPage
                    );
                dialog.show();
                app.processEvents();

                QTableWidget* selectionTable = dialog.findChild<QTableWidget*>(
                    QStringLiteral("speakingEvalAiBatchSelectionTable")
                    );
                QTableWidget* reviewTable = dialog.findChild<QTableWidget*>(
                    QStringLiteral("speakingEvalAiBatchReviewTable")
                    );
                QPushButton* createPromptButton =
                    dialog.findChild<QPushButton*>(
                        QStringLiteral("speakingEvalAiBatchCreatePrompt")
                        );
                QPlainTextEdit* promptEdit = dialog.findChild<QPlainTextEdit*>(
                    QStringLiteral("speakingEvalAiBatchPrompt")
                    );
                QPlainTextEdit* responseEdit = dialog.findChild<QPlainTextEdit*>(
                    QStringLiteral("speakingEvalAiBatchResponse")
                    );
                QPushButton* parseButton = dialog.findChild<QPushButton*>(
                    QStringLiteral("speakingEvalAiBatchParse")
                    );
                QPushButton* applyButton = dialog.findChild<QPushButton*>(
                    QStringLiteral("speakingEvalAiBatchApply")
                    );
                QTabWidget* tabs = dialog.findChild<QTabWidget*>(
                    QStringLiteral("speakingEvalAiBatchTabs")
                    );

                const auto itemCount =
                    [](const QTableWidget* widget)
                    {
                        if (!widget)
                        {
                            return 0;
                        }
                        int count = 0;
                        for (int row = 0; row < widget->rowCount(); ++row)
                        {
                            for (int column = 0;
                                 column < widget->columnCount();
                                 ++column)
                            {
                                count += widget->item(row, column) ? 1 : 0;
                            }
                        }
                        return count;
                    };

                if (
                    !selectionTable
                    || !reviewTable
                    || !createPromptButton
                    || !promptEdit
                    || !responseEdit
                    || !parseButton
                    || !applyButton
                    || !tabs
                    )
                {
                    aiError = QStringLiteral("speaking-evaluation-ai-controls-missing");
                }
                else
                {
                    StartupProfiler::recordSpeakingEvaluationAiDialogPrepared(
                        reports.size(),
                        selectionTable->rowCount(),
                        selectionTable->columnCount(),
                        itemCount(selectionTable)
                        );
                    createPromptButton->click();
                    app.processEvents();
                    tabs->setCurrentIndex(1);
                    app.processEvents();

                    QString response;
                    const QString comment = QStringLiteral(
                        "STD_NAME speaks clearly and uses strong grammar in class. "
                        "Keep practicing pronunciation and vocabulary during each lesson. "
                        "Your steady effort is helping you grow."
                        );
                    for (int index = 0; index < reports.size(); ++index)
                    {
                        response +=
                            QStringLiteral("<<<STUDENT_%1>>>\n%2\n<<<END_STUDENT_%1>>>\n")
                                .arg(index + 1, 2, 10, QLatin1Char('0'))
                                .arg(comment);
                    }
                    responseEdit->setPlainText(response);
                    app.processEvents();
                    parseButton->click();
                    app.processEvents();

                    const int reviewRows = reviewTable->rowCount();
                    const int reviewColumns = reviewTable->columnCount();
                    const int reviewItems = itemCount(reviewTable);
                    if (!applyButton->isEnabled())
                    {
                        aiError = QStringLiteral("speaking-evaluation-ai-apply-disabled");
                    }
                    else
                    {
                        dialog.grab().save(
                            QDir(outputRoot).filePath(
                                QStringLiteral("speaking-evaluation-ai-review.png")
                                ),
                            "PNG"
                            );
                        applyButton->click();
                        app.processEvents();
                        acceptedComments = dialog.acceptedComments();
                        StartupProfiler::recordSpeakingEvaluationAiResponsePrepared(
                            promptEdit->toPlainText().toUtf8().size(),
                            response.toUtf8().size(),
                            reviewRows,
                            reviewColumns,
                            reviewItems,
                            acceptedComments.size()
                            );
                    }
                }

                if (dialog.isVisible())
                {
                    dialog.reject();
                    app.processEvents();
                }
                StartupProfiler::recordSpeakingEvaluationAiDialogReleased();
            }

            if (!aiError.isEmpty())
            {
                fail(aiError);
                return;
            }

            QList<SpeakingEvalCellEdit> commentChanges;
            commentChanges.reserve(acceptedComments.size());
            for (const SpeakingEvalAiBatchAcceptedComment& comment
                 : acceptedComments)
            {
                commentChanges.append(
                    {
                        comment.sourceRow,
                        SpeakingEval::toInt(SpeakingEvalColumn::Comments),
                        comment.oldComment,
                        comment.newComment
                    }
                    );
            }
            if (!commentChanges.isEmpty())
            {
                table->applyChanges(
                    commentChanges,
                    QStringLiteral("Startup diagnostics: Apply AI comments")
                    );
                app.processEvents();
            }
            profiler.checkpoint(
                QStringLiteral("speaking-evaluation-comments-applied"),
                QStringLiteral(
                    "acceptedComments=%1; modelDirty=%2"
                    )
                    .arg(acceptedComments.size())
                    .arg(
                        model->isDirty()
                            ? QStringLiteral("true")
                            : QStringLiteral("false")
                        )
                );
            appendStartupWorkflowTrace(
                QStringLiteral("speaking-evaluation-comments-applied")
                );

            reports.clear();
            StartupProfiler::recordSpeakingEvaluationOperationReleased();
            appendStartupWorkflowTrace(
                QStringLiteral("speaking-evaluation-operation-released")
                );
            evaluationPage->refresh();
            classesPage->refresh();
            app.processEvents();
            profiler.checkpoint(
                QStringLiteral("speaking-evaluation-page-refreshed"),
                QStringLiteral(
                    "classId=%1; modelRows=%2; reportListRetained=false"
                    )
                    .arg(classesPage->currentClassId())
                    .arg(model->rowCount())
                );
            appendStartupWorkflowTrace(
                QStringLiteral("speaking-evaluation-page-refreshed")
                );
            completion();
        }
        );
}

void scheduleStartupPerformanceClassTransferLifecycle(
    QApplication& app,
    MainWindow& window,
    StartupProfiler& profiler,
    const std::shared_ptr<bool>& workflowSucceeded,
    std::function<void()> completion
    )
{
    QTimer::singleShot(
        0,
        &app,
        [
            &app,
            &window,
            &profiler,
            workflowSucceeded,
            completion
        ]()
        {
            PageManager* pages = window.pageManager();
            ClassesPage* page = pages ? pages->classesPage() : nullptr;
            ApplicationServices* services = window.services();
            ClassService* classService = services
                ? services->classService()
                : nullptr;
            TeacherService* teacherService = services
                ? services->teacherService()
                : nullptr;
            const QString filePath =
                qEnvironmentVariable(
                    "CLASSMNGR_STARTUP_CLASS_TRANSFER_PATH"
                    ).trimmed();

            const auto fail =
                [
                    &completion,
                    &workflowSucceeded
                ](const QString& detail)
                {
                    *workflowSucceeded = false;
                    StartupProfiler::recordClassTransferFailed(detail);
                    StartupProfiler::recordClassTransferOperationReleased();
                    completion();
                };

            if (
                !pages
                || !page
                || !classService
                || !teacherService
                || !classService->isAvailable()
                || !teacherService->isAvailable()
                || !pages->isCurrentPage(PageType::Classes)
                || filePath.isEmpty()
                || !QFileInfo::exists(filePath)
                )
            {
                *workflowSucceeded = false;
                profiler.checkpoint(
                    QStringLiteral("class-transfer-lifecycle-failed"),
                    QStringLiteral(
                        "classes-page-current=%1; file-exists=%2"
                        )
                        .arg(
                            pages && pages->isCurrentPage(PageType::Classes)
                                ? QStringLiteral("true")
                                : QStringLiteral("false")
                            )
                        .arg(
                            QFileInfo::exists(filePath)
                                ? QStringLiteral("true")
                                : QStringLiteral("false")
                            )
                    );
                completion();
                return;
            }

            StartupProfiler::recordClassTransferStarted(
                filePath,
                QFileInfo(filePath).size()
                );
            appendStartupWorkflowTrace(
                QStringLiteral("class-transfer-source-opened")
                );

            const auto packageResult =
                ClassTransferJsonCodec::loadFile(filePath);
            if (!packageResult)
            {
                fail(packageResult.error());
                return;
            }

            ClassTransferPackage package = std::move(*packageResult);
            int rosterColumnCount = 0;
            int rosterRowCount = 0;
            int rosterCellCount = 0;
            int evaluationCount = 0;
            int evaluationRowCount = 0;
            int evaluationCellCount = 0;
            int scheduleRowCount = 0;

            for (const ClassTransferClass& transferClass : package.classes)
            {
                rosterColumnCount += transferClass.roster.columns.size();
                rosterRowCount += transferClass.roster.rows.size();
                for (const QStringList& row : transferClass.roster.rows)
                {
                    rosterCellCount += row.size();
                }

                evaluationCount += transferClass.evaluations.size();
                for (const ClassTransferEvaluation& evaluation
                     : transferClass.evaluations)
                {
                    evaluationRowCount += evaluation.rows.size();
                    for (const QStringList& row : evaluation.rows)
                    {
                        evaluationCellCount += row.size();
                    }
                }

                scheduleRowCount += transferClass.info.classTimes.size();
                scheduleRowCount += transferClass.info.intensiveTimes.size();
            }

            StartupProfiler::recordClassTransferPackageLoaded(
                package.teachers.size(),
                package.classes.size(),
                rosterColumnCount,
                rosterRowCount,
                rosterCellCount,
                evaluationCount,
                evaluationRowCount,
                evaluationCellCount,
                scheduleRowCount
                );

            const auto destinationTeachers = teacherService->teachers();
            const auto destinationClasses = classService->classes();
            if (!destinationTeachers || !destinationClasses)
            {
                fail(
                    !destinationTeachers
                        ? destinationTeachers.error()
                        : destinationClasses.error()
                    );
                return;
            }

            const auto previewResult = classService->previewImport(package);
            if (!previewResult)
            {
                fail(previewResult.error());
                return;
            }

            ClassImportPreview preview = std::move(*previewResult);
            int matchingTeacherCount = 0;
            int matchingClassCount = 0;
            for (const ClassImportTeacherPreview& teacherPreview
                 : preview.teachers)
            {
                matchingTeacherCount += teacherPreview.matchingTeacherIds.size();
            }
            for (const ClassImportClassPreview& classPreview : preview.classes)
            {
                matchingClassCount += classPreview.matchingClassIds.size();
            }

            StartupProfiler::recordClassTransferPreviewPrepared(
                preview.teachers.size(),
                preview.classes.size(),
                matchingTeacherCount,
                matchingClassCount,
                destinationTeachers->size(),
                destinationClasses->size(),
                destinationClasses->size()
                );
            appendStartupWorkflowTrace(
                QStringLiteral("class-transfer-preview-ready")
                );

            auto* dialog = new ClassImportDialog(
                classService,
                teacherService,
                package,
                preview,
                page
                );
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            QPointer<ClassImportDialog> dialogGuard(dialog);
            dialog->show();
            app.processEvents();

            const QString outputRoot =
                qEnvironmentVariable(
                    "CLASSMNGR_STARTUP_CLASS_TRANSFER_OUTPUT_DIR"
                    ).trimmed();
            if (!outputRoot.isEmpty())
            {
                QDir().mkpath(outputRoot);
                dialog->grab().save(
                    QDir(outputRoot).filePath(
                        QStringLiteral("class-transfer-review.png")
                        ),
                    "PNG"
                    );
            }

            int teacherControlCount = 0;
            int classControlCount = 0;
            for (QComboBox* combo : dialog->findChildren<QComboBox*>())
            {
                if (combo->objectName().startsWith(
                        QStringLiteral("teacherImportChoice_")))
                {
                    ++teacherControlCount;
                }
                else if (combo->objectName().startsWith(
                             QStringLiteral("classImportChoice_")))
                {
                    ++classControlCount;
                }
            }

            auto* importButton = dialog->findChild<QPushButton*>(
                QStringLiteral("importClassesButton")
                );
            if (!importButton)
            {
                if (dialogGuard)
                {
                    dialogGuard->reject();
                    app.processEvents();
                    QCoreApplication::sendPostedEvents(
                        nullptr,
                        QEvent::DeferredDelete
                        );
                    app.processEvents();
                }
                StartupProfiler::recordClassTransferDialogReleased();
                fail(QStringLiteral("import-button-missing"));
                return;
            }

            ClassImportPlan plan = dialog->importPlan();
            int teachersCreated = 0;
            int teachersKept = 0;
            int teachersReplaced = 0;
            for (const TeacherImportResolution& resolution : plan.teachers)
            {
                switch (resolution.action)
                {
                case TeacherImportAction::Create:
                    ++teachersCreated;
                    break;
                case TeacherImportAction::KeepExisting:
                    ++teachersKept;
                    break;
                case TeacherImportAction::ReplaceExisting:
                    ++teachersReplaced;
                    break;
                }
            }

            int classesCreated = 0;
            int classesReplaced = 0;
            int classesSkipped = 0;
            for (const ClassImportResolution& resolution : plan.classes)
            {
                switch (resolution.action)
                {
                case ClassImportAction::Create:
                    ++classesCreated;
                    break;
                case ClassImportAction::Replace:
                    ++classesReplaced;
                    break;
                case ClassImportAction::Skip:
                    ++classesSkipped;
                    break;
                }
            }

            StartupProfiler::recordClassTransferDialogPrepared(
                teacherControlCount,
                classControlCount,
                plan.teachers.size(),
                plan.classes.size()
                );
            appendStartupWorkflowTrace(
                QStringLiteral("class-transfer-dialog-opened")
                );
            profiler.checkpoint(
                QStringLiteral("class-transfer-dialog-opened"),
                QStringLiteral(
                    "teacherControls=%1; classControls=%2; importEnabled=%3"
                    )
                    .arg(teacherControlCount)
                    .arg(classControlCount)
                    .arg(
                        importButton->isEnabled()
                            ? QStringLiteral("true")
                            : QStringLiteral("false")
                        )
                );

            if (!importButton->isEnabled())
            {
                if (dialogGuard)
                {
                    dialogGuard->reject();
                    app.processEvents();
                    QCoreApplication::sendPostedEvents(
                        nullptr,
                        QEvent::DeferredDelete
                        );
                    app.processEvents();
                }
                StartupProfiler::recordClassTransferDialogReleased();
                fail(QStringLiteral("import-transition-disabled"));
                return;
            }

            profiler.checkpoint(
                QStringLiteral("class-transfer-apply-start"),
                QStringLiteral(
                    "teachers=%1; classes=%2; action=create"
                    )
                    .arg(teachersCreated)
                    .arg(classesCreated)
                );
            appendStartupWorkflowTrace(
                QStringLiteral("class-transfer-apply-start")
                );

            if (dialogGuard)
            {
                dialogGuard->accept();
                app.processEvents();
                QCoreApplication::sendPostedEvents(
                    nullptr,
                    QEvent::DeferredDelete
                    );
                app.processEvents();
            }
            StartupProfiler::recordClassTransferDialogReleased();

            const auto summary = classService->importClasses(package, plan);
            if (!summary)
            {
                package = {};
                preview = {};
                plan = {};
                fail(summary.error());
                return;
            }

            const auto destinationTeachersAfter = teacherService->teachers();
            const auto destinationClassesAfter = classService->classes();
            if (!destinationTeachersAfter || !destinationClassesAfter)
            {
                package = {};
                preview = {};
                plan = {};
                fail(
                    !destinationTeachersAfter
                        ? destinationTeachersAfter.error()
                        : destinationClassesAfter.error()
                    );
                return;
            }

            const int firstAffectedClassId =
                !summary->createdClassIds.isEmpty()
                    ? summary->createdClassIds.first()
                    : !summary->replacedClassIds.isEmpty()
                        ? summary->replacedClassIds.first()
                        : -1;
            StartupProfiler::recordClassTransferApplied(
                teachersCreated,
                teachersKept,
                teachersReplaced,
                summary->createdClassIds.size(),
                summary->replacedClassIds.size(),
                summary->skippedClassCount,
                destinationClasses->size(),
                destinationClassesAfter->size(),
                destinationTeachers->size(),
                destinationTeachersAfter->size()
                );

            package = {};
            preview = {};
            plan = {};
            StartupProfiler::recordClassTransferOperationReleased();
            profiler.checkpoint(
                QStringLiteral("class-transfer-post-release"),
                QStringLiteral(
                    "rawBytesRetained=false; jsonDocumentRetained=false; packageRetained=false; previewRetained=false; dialogRetained=false"
                    )
                );
            appendStartupWorkflowTrace(
                QStringLiteral("class-transfer-post-release")
                );

            pages->refreshAll();
            app.processEvents();
            if (firstAffectedClassId > 0)
            {
                page->openClass(
                    firstAffectedClassId,
                    ClassesSection::Details
                    );
                app.processEvents();
            }
            profiler.checkpoint(
                QStringLiteral("class-transfer-page-refreshed"),
                QStringLiteral(
                    "visibleClasses=%1; selectedClassId=%2"
                    )
                    .arg(page->runtimeMetrics().visibleClassCount)
                    .arg(page->runtimeMetrics().selectedClassId)
                );
            appendStartupWorkflowTrace(
                QStringLiteral("class-transfer-page-refreshed")
                );
            profiler.checkpoint(
                QStringLiteral("class-transfer-operation-end"),
                QStringLiteral("committed=true")
                );
            appendStartupWorkflowTrace(
                QStringLiteral("class-transfer-operation-end committed=true")
                );
            completion();
        }
        );
}

void scheduleStartupPerformanceSubPrepLifecycle(
    QApplication& app,
    MainWindow& window,
    StartupProfiler& profiler,
    const std::shared_ptr<bool>& workflowSucceeded,
    bool outputLifecycleEnabled,
    std::function<void()> completion
    )
{
    QTimer::singleShot(
        0,
        &app,
        [
            &app,
            &window,
            &profiler,
            workflowSucceeded,
            outputLifecycleEnabled,
            completion
        ]()
        {
            PageManager* pages = window.pageManager();
            SubPrepPage* page = pages ? pages->subPrepPage() : nullptr;

            if (
                !pages
                || !page
                || !pages->isCurrentPage(PageType::SubPrep)
                )
            {
                *workflowSucceeded = false;
                profiler.checkpoint(
                    QStringLiteral("sub-prep-lifecycle-failed"),
                    QStringLiteral("sub-prep-page-not-current")
                    );
                StartupProfiler::setSubPrepDiagnosticsActive(false);
                completion();
                return;
            }

            bool lifecycleSucceeded = true;
            const auto checkpoint =
                [&profiler](
                    const QString& name,
                    const QString& detail = QString()
                    )
                {
                    profiler.checkpoint(name, detail);
                };

            const auto selectClass =
                [
                    &app,
                    &page,
                    &lifecycleSucceeded,
                    checkpoint
                ](int classId)
                {
                    const bool selected =
                        page->selectClassForStartupDiagnostics(classId);
                    app.processEvents();
                    appendStartupWorkflowTrace(
                        QStringLiteral(
                            "sub-prep-select classId=%1 returned=%2"
                            )
                            .arg(classId)
                            .arg(selected ? QStringLiteral("true")
                                          : QStringLiteral("false"))
                        );
                    if (!selected)
                    {
                        lifecycleSucceeded = false;
                    }
                    checkpoint(
                        QStringLiteral("sub-prep-selection-%1").arg(classId),
                        QStringLiteral(
                            "requestedClassId=%1; selectedClassId=%2; passed=%3"
                            )
                            .arg(classId)
                            .arg(page->runtimeMetrics().selectedClassId)
                            .arg(selected ? QStringLiteral("true")
                                          : QStringLiteral("false"))
                        );
                };

            const auto refresh =
                [
                    &app,
                    &page,
                    checkpoint
                ](int ordinal)
                {
                    const QString startName =
                        QStringLiteral("sub-prep-refresh-%1-start")
                            .arg(ordinal);
                    const QString completeName =
                        QStringLiteral("sub-prep-refresh-%1-complete")
                            .arg(ordinal);
                    appendStartupWorkflowTrace(
                        startName
                        );
                    checkpoint(
                        startName,
                        QStringLiteral("selectedClassId=%1")
                            .arg(page->runtimeMetrics().selectedClassId)
                        );
                    page->refresh();
                    app.processEvents();
                    appendStartupWorkflowTrace(
                        completeName
                        );
                    checkpoint(
                        completeName,
                        QStringLiteral("selectedClassId=%1")
                            .arg(page->runtimeMetrics().selectedClassId)
                        );
                };

            const auto navigate =
                [
                    &app,
                    &pages,
                    &page,
                    &lifecycleSucceeded,
                    checkpoint
                ](
                    PageType destination,
                    const QString& operation
                    )
                {
                    pages->showPage(destination);
                    app.processEvents();
                    const bool reached = pages->isCurrentPage(destination);
                    appendStartupWorkflowTrace(
                        QStringLiteral("%1 returned=%2")
                            .arg(
                                operation,
                                reached ? QStringLiteral("true")
                                        : QStringLiteral("false")
                                )
                        );
                    if (!reached)
                    {
                        lifecycleSucceeded = false;
                    }
                    checkpoint(
                        operation,
                        QStringLiteral(
                            "currentPage=%1; selectedClassId=%2; passed=%3"
                            )
                            .arg(pages->currentPageIdentifier())
                            .arg(page->runtimeMetrics().selectedClassId)
                            .arg(reached ? QStringLiteral("true")
                                         : QStringLiteral("false"))
                        );
                };

            appendStartupWorkflowTrace(
                QStringLiteral("sub-prep-lifecycle-start")
                );
            checkpoint(
                QStringLiteral("sub-prep-lifecycle-entry"),
                QStringLiteral("selectedClassId=%1")
                    .arg(page->runtimeMetrics().selectedClassId)
                );

            selectClass(96);
            refresh(1);
            navigate(
                PageType::MyWorkspace,
                QStringLiteral("sub-prep-left-1")
                );
            navigate(
                PageType::SubPrep,
                QStringLiteral("sub-prep-reentry-1")
                );
            selectClass(1);
            refresh(2);
            navigate(
                PageType::MyWorkspace,
                QStringLiteral("sub-prep-left-2")
                );
            navigate(
                PageType::SubPrep,
                QStringLiteral("sub-prep-reentry-2")
                );

            appendStartupWorkflowTrace(
                QStringLiteral("sub-prep-lifecycle-complete")
                );
            checkpoint(
                QStringLiteral("sub-prep-lifecycle-complete"),
                QStringLiteral("passed=%1; selectedClassId=%2")
                    .arg(
                        lifecycleSucceeded
                            ? QStringLiteral("true")
                            : QStringLiteral("false")
                        )
                    .arg(page->runtimeMetrics().selectedClassId)
                );
            if (!lifecycleSucceeded)
            {
                *workflowSucceeded = false;
            }

            if (lifecycleSucceeded && outputLifecycleEnabled)
            {
                scheduleStartupPerformanceSubPrepOutputLifecycle(
                    app,
                    page,
                    profiler,
                    workflowSucceeded,
                    [completion]()
                    {
                        StartupProfiler::setSubPrepDiagnosticsActive(false);
                        completion();
                    }
                    );
                return;
            }

            StartupProfiler::setSubPrepDiagnosticsActive(false);
            completion();
        }
        );
}

void scheduleStartupPerformanceScheduleImportLifecycle(
    QApplication& app,
    MainWindow& window,
    StartupProfiler& profiler,
    const std::shared_ptr<bool>& workflowSucceeded,
    bool applyImport,
    std::function<void()> completion
    )
{
    QTimer::singleShot(
        0,
        &app,
        [
            &app,
            &window,
            &profiler,
            workflowSucceeded,
            applyImport,
            completion
        ]()
        {
            PageManager* pages = window.pageManager();
            SchedulePage* page = pages ? pages->schedulePage() : nullptr;
            const QString filePath =
                qEnvironmentVariable(
                    "CLASSMNGR_STARTUP_SCHEDULE_IMPORT_PATH"
                    ).trimmed();

            if (
                !pages
                || !page
                || !pages->isCurrentPage(PageType::Schedule)
                || filePath.isEmpty()
                || !QFileInfo::exists(filePath)
                )
            {
                *workflowSucceeded = false;
                profiler.checkpoint(
                    QStringLiteral("schedule-import-lifecycle-failed"),
                    QStringLiteral(
                        "schedule-page-current=%1; file-exists=%2"
                        )
                        .arg(
                            pages && pages->isCurrentPage(PageType::Schedule)
                                ? QStringLiteral("true")
                                : QStringLiteral("false")
                            )
                        .arg(
                            QFileInfo::exists(filePath)
                                ? QStringLiteral("true")
                                : QStringLiteral("false")
                            )
                    );
                completion();
                return;
            }

            auto* dialog =
                new ScheduleImportDialog(
                    window.services(),
                    page
                    );
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            QPointer<ScheduleImportDialog> dialogGuard(dialog);
            dialog->setFilePath(filePath);
            dialog->show();
            app.processEvents();

            auto* normal =
                dialog->findChild<QRadioButton*>(
                    QStringLiteral("scheduleImportNormalRadio")
                    );
            auto* next =
                dialog->findChild<QPushButton*>(
                    QStringLiteral("scheduleImportNextButton")
                    );
            if (!normal || !next)
            {
                *workflowSucceeded = false;
                profiler.checkpoint(
                    QStringLiteral("schedule-import-lifecycle-failed"),
                    QStringLiteral("source-dialog-controls-missing")
                    );
                dialog->reject();
                completion();
                return;
            }

            normal->setChecked(true);
            appendStartupWorkflowTrace(
                QStringLiteral("schedule-import-dialog-opened")
                );
            profiler.checkpoint(
                QStringLiteral("schedule-import-dialog-opened"),
                QStringLiteral("path=%1; fileBytes=%2")
                    .arg(filePath)
                    .arg(QFileInfo(filePath).size())
                );
            next->click();
            app.processEvents();

            const auto stage = std::make_shared<int>(0);
            const auto attempts = std::make_shared<int>(0);
            const auto poll =
                std::make_shared<std::function<void()>>();

            *poll =
                [
                    &app,
                    &profiler,
                    workflowSucceeded,
                    applyImport,
                    page,
                    completion,
                    dialogGuard,
                    stage,
                    attempts,
                    poll
                ]()
                {
                    if (!dialogGuard)
                    {
                        *workflowSucceeded = false;
                        profiler.checkpoint(
                            QStringLiteral(
                                "schedule-import-lifecycle-failed"
                                ),
                            QStringLiteral("source-dialog-closed-early")
                            );
                        completion();
                        return;
                    }

                    ++*attempts;
                    if (*attempts > 400)
                    {
                        *workflowSucceeded = false;
                        profiler.checkpoint(
                            QStringLiteral(
                                "schedule-import-lifecycle-failed"
                                ),
                            QStringLiteral("timed-out-waiting-for-review")
                            );
                        dialogGuard->reject();
                        app.processEvents();
                        completion();
                        return;
                    }

                    if (*stage == 0)
                    {
                        auto* status =
                            dialogGuard->findChild<QLabel*>(
                                QStringLiteral(
                                    "scheduleImportSourceStatus"
                                    )
                                );
                        auto* users =
                            dialogGuard->findChild<QComboBox*>(
                                QStringLiteral("scheduleImportUserCombo")
                                );
                        auto* sheets =
                            dialogGuard->findChild<QComboBox*>(
                                QStringLiteral("scheduleImportSheetCombo")
                                );
                        auto* next =
                            dialogGuard->findChild<QPushButton*>(
                                QStringLiteral("scheduleImportNextButton")
                                );
                        auto* progress =
                            dialogGuard->findChild<QProgressBar*>(
                                QStringLiteral(
                                    "scheduleImportProgressBar"
                                    )
                                );

                        const QString statusText =
                            status ? status->text() : QString();
                        if (
                            statusText.startsWith(
                                QStringLiteral("Invalid workbook:")
                                )
                            || statusText.startsWith(
                                QStringLiteral(
                                    "The selected workbook could not be opened."
                                    )
                                )
                            )
                        {
                            *workflowSucceeded = false;
                            profiler.checkpoint(
                                QStringLiteral(
                                    "schedule-import-lifecycle-failed"
                                    ),
                                statusText
                                );
                            dialogGuard->reject();
                            app.processEvents();
                            completion();
                            return;
                        }

                        if (
                            progress
                            && !progress->isHidden()
                            )
                        {
                            QTimer::singleShot(25, &app, [poll]() { (*poll)(); });
                            return;
                        }

                        if (sheets && users && !users->isVisible())
                        {
                            for (int index = 0; index < sheets->count(); ++index)
                            {
                                if (sheets->itemData(index).toInt() >= 0)
                                {
                                    sheets->setCurrentIndex(index);
                                    app.processEvents();
                                    break;
                                }
                            }
                        }

                        int selectedUser = -1;
                        if (users && users->isVisible())
                        {
                            for (int index = 0; index < users->count(); ++index)
                            {
                                if (users->itemData(index).toInt() >= 0)
                                {
                                    selectedUser = index;
                                    break;
                                }
                            }
                        }

                        if (
                            !users
                            || selectedUser < 0
                            || !next
                            )
                        {
                            QTimer::singleShot(25, &app, [poll]() { (*poll)(); });
                            return;
                        }

                        users->setCurrentIndex(selectedUser);
                        if (
                            auto* confirmation =
                                dialogGuard->findChild<QCheckBox*>(
                                    QStringLiteral(
                                        "scheduleImportNameConfirmation"
                                        )
                                    )
                            )
                        {
                            confirmation->setChecked(true);
                        }

                        profiler.checkpoint(
                            QStringLiteral("schedule-import-parse-complete"),
                            QStringLiteral(
                                "source-status=%1; selectedUserIndex=%2"
                                )
                                .arg(statusText)
                                .arg(selectedUser)
                            );
                        appendStartupWorkflowTrace(
                            QStringLiteral("schedule-import-parse-complete")
                            );
                        if (!next->isEnabled())
                        {
                            *workflowSucceeded = false;
                            profiler.checkpoint(
                                QStringLiteral(
                                    "schedule-import-lifecycle-failed"
                                    ),
                                QStringLiteral("review-transition-disabled")
                                );
                            dialogGuard->reject();
                            app.processEvents();
                            completion();
                            return;
                        }

                        profiler.checkpoint(
                            QStringLiteral("schedule-import-review-start")
                            );
                        appendStartupWorkflowTrace(
                            QStringLiteral("schedule-import-review-start")
                            );
                        next->click();
                        app.processEvents();
                        *stage = 1;
                    }

                    if (*stage == 1)
                    {
                        auto* review =
                            dialogGuard->findChild<ScheduleImportReviewDialog*>();
                        if (!review)
                        {
                            QTimer::singleShot(25, &app, [poll]() { (*poll)(); });
                            return;
                        }

                        auto* import =
                            review->findChild<QPushButton*>(
                                QStringLiteral("scheduleImportAcceptButton")
                                );
                        if (!import)
                        {
                            QTimer::singleShot(25, &app, [poll]() { (*poll)(); });
                            return;
                        }

                        app.processEvents();
                        if (
                            auto* conflictWarning =
                                review->findChild<QMessageBox*>(
                                    QStringLiteral(
                                        "scheduleImportConflictWarning"
                                        )
                                    )
                            )
                        {
                            conflictWarning->accept();
                            app.processEvents();
                        }

                        if (applyImport)
                        {
                            if (
                                auto* warningAcknowledgement =
                                    review->findChild<QCheckBox*>(
                                        QStringLiteral(
                                            "scheduleImportWarningAcknowledgement"
                                            )
                                        )
                                )
                            {
                                warningAcknowledgement->setChecked(true);
                                app.processEvents();
                            }
                        }

                        const QString outputRoot =
                            qEnvironmentVariable(
                                "CLASSMNGR_STARTUP_SCHEDULE_IMPORT_OUTPUT_DIR"
                                ).trimmed();
                        if (!outputRoot.isEmpty())
                        {
                            QDir().mkpath(outputRoot);
                            dialogGuard->grab().save(
                                QDir(outputRoot).filePath(
                                    QStringLiteral("schedule-import-source.png")
                                    ),
                                "PNG"
                                );
                            review->show();
                            app.processEvents();
                            review->grab().save(
                                QDir(outputRoot).filePath(
                                    QStringLiteral("schedule-import-review.png")
                                    ),
                                "PNG"
                                );
                        }

                        profiler.checkpoint(
                            QStringLiteral("schedule-import-review-ready"),
                            QStringLiteral(
                                "reviewVisible=%1; importEnabled=%2"
                                )
                                .arg(
                                    review->isVisible()
                                        ? QStringLiteral("true")
                                        : QStringLiteral("false")
                                    )
                                .arg(
                                    import->isEnabled()
                                        ? QStringLiteral("true")
                                        : QStringLiteral("false")
                                    )
                            );
                        appendStartupWorkflowTrace(
                            QStringLiteral("schedule-import-review-ready")
                            );
                        if (applyImport)
                        {
                            if (!import->isEnabled())
                            {
                                *workflowSucceeded = false;
                                profiler.checkpoint(
                                    QStringLiteral(
                                        "schedule-import-lifecycle-failed"
                                        ),
                                    QStringLiteral(
                                        "apply-transition-disabled"
                                        )
                                    );
                                review->reject();
                                app.processEvents();
                                completion();
                                return;
                            }

                            profiler.checkpoint(
                                QStringLiteral("schedule-import-apply-start"),
                                QStringLiteral("review-confirmed")
                                );
                            appendStartupWorkflowTrace(
                                QStringLiteral("schedule-import-apply-start")
                                );

                            auto* promptTimer = new QTimer(&app);
                            promptTimer->setInterval(10);
                            QObject::connect(
                                promptTimer,
                                &QTimer::timeout,
                                &app,
                                []()
                                {
                                    for (
                                        QWidget* widget :
                                            QApplication::topLevelWidgets()
                                        )
                                    {
                                        auto* prompt =
                                            qobject_cast<QMessageBox*>(widget);
                                        if (
                                            !prompt
                                            || !prompt->isVisible()
                                            || prompt->objectName()
                                                != QStringLiteral(
                                                    "classmngrUserPrompt"
                                                    )
                                            )
                                        {
                                            continue;
                                        }

                                        if (QPushButton* button =
                                                prompt->defaultButton())
                                        {
                                            button->click();
                                            return;
                                        }
                                    }
                                }
                                );
                            promptTimer->start();
                            import->click();
                            promptTimer->stop();
                            promptTimer->deleteLater();
                            app.processEvents();

                            if (dialogGuard && dialogGuard->isVisible())
                            {
                                *workflowSucceeded = false;
                                profiler.checkpoint(
                                    QStringLiteral(
                                        "schedule-import-lifecycle-failed"
                                        ),
                                    QStringLiteral(
                                        "apply-dialog-remained-open"
                                        )
                                    );
                                dialogGuard->reject();
                                app.processEvents();
                                completion();
                                return;
                            }

                            profiler.checkpoint(
                                QStringLiteral(
                                    "schedule-import-apply-complete"
                                    ),
                                QStringLiteral("committed=true")
                                );
                            appendStartupWorkflowTrace(
                                QStringLiteral(
                                    "schedule-import-apply-complete committed=true"
                                    )
                                );

                            QCoreApplication::sendPostedEvents(
                                nullptr,
                                QEvent::DeferredDelete
                                );
                            app.processEvents();
                            profiler.checkpoint(
                                QStringLiteral(
                                    "schedule-import-post-review-release"
                                    )
                                );
                            appendStartupWorkflowTrace(
                                QStringLiteral(
                                    "schedule-import-post-review-release"
                                    )
                                );
                            profiler.checkpoint(
                                QStringLiteral("schedule-import-post-release")
                                );
                            appendStartupWorkflowTrace(
                                QStringLiteral("schedule-import-post-release")
                                );

                            if (page)
                            {
                                page->markStale();
                                if (page->isVisible())
                                {
                                    page->activate();
                                }
                                app.processEvents();
                            }
                            profiler.checkpoint(
                                QStringLiteral(
                                    "schedule-import-page-refreshed"
                                    )
                                );
                            appendStartupWorkflowTrace(
                                QStringLiteral(
                                    "schedule-import-page-refreshed"
                                    )
                                );
                            profiler.checkpoint(
                                QStringLiteral("schedule-import-operation-end"),
                                QStringLiteral("committed=true")
                                );
                            appendStartupWorkflowTrace(
                                QStringLiteral(
                                    "schedule-import-operation-end committed=true"
                                    )
                                );
                            completion();
                            return;
                        }

                        profiler.checkpoint(
                            QStringLiteral("schedule-import-cancel-start")
                            );
                        appendStartupWorkflowTrace(
                            QStringLiteral("schedule-import-cancel-start")
                            );
                        review->reject();
                        StartupProfiler::recordScheduleImportCancelled();
                        app.processEvents();
                        QCoreApplication::sendPostedEvents(
                            nullptr,
                            QEvent::DeferredDelete
                            );
                        app.processEvents();
                        profiler.checkpoint(
                            QStringLiteral("schedule-import-post-review-release")
                            );
                        appendStartupWorkflowTrace(
                            QStringLiteral("schedule-import-post-review-release")
                            );

                        dialogGuard->reject();
                        app.processEvents();
                        QCoreApplication::sendPostedEvents(
                            nullptr,
                            QEvent::DeferredDelete
                            );
                        app.processEvents();
                        profiler.checkpoint(
                            QStringLiteral("schedule-import-post-release")
                            );
                        appendStartupWorkflowTrace(
                            QStringLiteral("schedule-import-post-release")
                            );
                        profiler.checkpoint(
                            QStringLiteral("schedule-import-operation-end"),
                            QStringLiteral("cancelled=true")
                            );
                        appendStartupWorkflowTrace(
                            QStringLiteral("schedule-import-operation-end")
                            );
                        completion();
                    }
                };

            QTimer::singleShot(25, &app, [poll]() { (*poll)(); });
        }
        );
}

void scheduleStartupPerformanceCalendarImportLifecycle(
    QApplication& app,
    MainWindow& window,
    StartupProfiler& profiler,
    const std::shared_ptr<bool>& workflowSucceeded,
    std::function<void()> completion
    )
{
    QTimer::singleShot(
        0,
        &app,
        [
            &app,
            &window,
            &profiler,
            workflowSucceeded,
            completion
        ]()
        {
            PageManager* pages = window.pageManager();
            MyWorkspacePage* workspace =
                pages ? pages->myWorkspacePage() : nullptr;
            CalendarPage* page = workspace
                ? workspace->calendarPage()
                : nullptr;
            QAction* preferencesAction =
                window.findChild<QAction*>(
                    QStringLiteral("preferencesAction")
                    );

            if (
                !pages
                || !workspace
                || !page
                || !pages->isCurrentPage(PageType::MyWorkspace)
                || !preferencesAction
                )
            {
                *workflowSucceeded = false;
                profiler.checkpoint(
                    QStringLiteral("calendar-import-lifecycle-failed"),
                    QStringLiteral(
                        "workspace-current=%1; calendar-page=%2; preferences-action=%3"
                        )
                        .arg(
                            pages && pages->isCurrentPage(PageType::MyWorkspace)
                                ? QStringLiteral("true")
                                : QStringLiteral("false")
                            )
                        .arg(page ? QStringLiteral("true")
                                  : QStringLiteral("false"))
                        .arg(preferencesAction ? QStringLiteral("true")
                                                : QStringLiteral("false"))
                    );
                completion();
                return;
            }

            const QPointer<CalendarPage> pageGuard(page);
            QPointer<QDialog> dialogGuard;
            const auto attempts = std::make_shared<int>(0);
            const auto stage = std::make_shared<int>(0);
            const auto preferencesOpened = std::make_shared<bool>(false);
            const auto importFinished = std::make_shared<bool>(false);
            const auto poll =
                std::make_shared<std::function<void()>>();

            profiler.checkpoint(
                QStringLiteral("calendar-import-preferences-start")
                );
            appendStartupWorkflowTrace(
                QStringLiteral("calendar-import-preferences-start")
                );

            *poll =
                [
                    &app,
                    &window,
                    &profiler,
                    workflowSucceeded,
                    completion,
                    pageGuard,
                    dialogGuard,
                    attempts,
                    stage,
                    preferencesOpened,
                    importFinished,
                    poll
                ]()
                mutable
                {
                    auto schedulePoll =
                        [
                            &app,
                            poll
                        ]()
                        {
                            QTimer::singleShot(
                                25,
                                &app,
                                [poll]()
                                {
                                    (*poll)();
                                }
                                );
                        };
                    auto fail =
                        [
                            &app,
                            &profiler,
                            workflowSucceeded,
                            completion,
                            dialogGuard
                        ](const QString& detail)
                        {
                            *workflowSucceeded = false;
                            profiler.checkpoint(
                                QStringLiteral(
                                    "calendar-import-lifecycle-failed"
                                    ),
                                detail
                                );
                            if (dialogGuard)
                            {
                                dialogGuard->reject();
                                app.processEvents();
                                QCoreApplication::sendPostedEvents(
                                    nullptr,
                                    QEvent::DeferredDelete
                                    );
                                app.processEvents();
                            }
                            completion();
                        };

                    ++*attempts;
                    if (*attempts > 600)
                    {
                        fail(
                            QStringLiteral(
                                "timed-out-waiting-for-calendar-import"
                                )
                            );
                        return;
                    }

                    if (!dialogGuard)
                    {
                        QDialog* dialog =
                            window.findChild<QDialog*>(
                                QStringLiteral("preferencesDialog")
                                );
                        if (!dialog)
                        {
                            dialog = qobject_cast<QDialog*>(
                                QApplication::activeModalWidget()
                                );
                        }
                        if (dialog)
                        {
                            dialogGuard = dialog;
                        }
                    }

                    if (!dialogGuard)
                    {
                        schedulePoll();
                        return;
                    }

                    auto* tabs =
                        dialogGuard->findChild<QTabWidget*>(
                            QStringLiteral("preferencesTabs")
                            );
                    auto* calendarTab =
                        dialogGuard->findChild<QWidget*>(
                            QStringLiteral("preferencesCalendarTab")
                            );
                    auto* panel =
                        dialogGuard->findChild<QWidget*>(
                            QStringLiteral("calendarPreferencesPanel")
                            );
                    auto* importButton = panel
                        ? panel->findChild<QPushButton*>(
                            QStringLiteral(
                                "preferencesCalendarImportEvents"
                                )
                            )
                        : nullptr;
                    auto* status = panel
                        ? panel->findChild<QLabel*>(
                            QStringLiteral("sectionSubtitle")
                            )
                        : nullptr;

                    if (*stage == 0)
                    {
                        if (!tabs || !calendarTab || !panel || !importButton)
                        {
                            schedulePoll();
                            return;
                        }

                        tabs->setCurrentWidget(calendarTab);
                        app.processEvents();

                        if (!*preferencesOpened)
                        {
                            *preferencesOpened = true;
                            profiler.checkpoint(
                                QStringLiteral(
                                    "calendar-import-preferences-opened"
                                    ),
                                QStringLiteral("calendar-tab-index=%1")
                                    .arg(tabs->indexOf(calendarTab))
                                );
                            appendStartupWorkflowTrace(
                                QStringLiteral(
                                    "calendar-import-preferences-opened"
                                    )
                                );
                            profiler.checkpoint(
                                QStringLiteral("calendar-import-ui-start")
                                );
                            appendStartupWorkflowTrace(
                                QStringLiteral("calendar-import-ui-start")
                                );
                            importButton->click();
                            app.processEvents();
                            *stage = 1;
                        }
                    }

                    if (*stage != 1 || !status)
                    {
                        schedulePoll();
                        return;
                    }

                    const QString statusText = status->text();
                    if (statusText.startsWith(QStringLiteral("Import failed:")))
                    {
                        fail(statusText);
                        return;
                    }

                    if (!statusText.startsWith(QStringLiteral("Imported ")))
                    {
                        schedulePoll();
                        return;
                    }

                    if (!*importFinished)
                    {
                        *importFinished = true;
                        profiler.checkpoint(
                            QStringLiteral("calendar-import-finished"),
                            statusText
                            );
                        appendStartupWorkflowTrace(
                            QStringLiteral("calendar-import-finished")
                            );
                    }

                    if (!pageGuard)
                    {
                        fail(QStringLiteral("calendar-page-closed-after-import"));
                        return;
                    }

                    const CalendarPageRuntimeMetrics metrics =
                        pageGuard->runtimeMetrics();
                    if (metrics.cacheLoading || metrics.cacheEventCount <= 0)
                    {
                        schedulePoll();
                        return;
                    }

                    const QString outputRoot =
                        qEnvironmentVariable(
                            "CLASSMNGR_STARTUP_CALENDAR_IMPORT_OUTPUT_DIR"
                            ).trimmed();
                    if (!outputRoot.isEmpty())
                    {
                        QDir().mkpath(outputRoot);
                        dialogGuard->grab().save(
                            QDir(outputRoot).filePath(
                                QStringLiteral(
                                    "calendar-import-preferences.png"
                                    )
                                ),
                            "PNG"
                            );
                        pageGuard->grab().save(
                            QDir(outputRoot).filePath(
                                QStringLiteral("calendar-page.png")
                                ),
                            "PNG"
                            );
                    }

                    profiler.checkpoint(
                        QStringLiteral("calendar-import-page-refreshed"),
                        QStringLiteral(
                            "cacheEvents=%1; dateBuckets=%2; loadedRanges=%3; retainedRanges=%4; loadedMonths=%5; modelRevision=%6"
                            )
                            .arg(metrics.cacheEventCount)
                            .arg(metrics.cacheDateBucketCount)
                            .arg(metrics.cacheLoadedRangeCount)
                            .arg(metrics.cacheRetainedRangeCount)
                            .arg(metrics.loadedMonthCount)
                            .arg(metrics.modelRevision)
                        );
                    appendStartupWorkflowTrace(
                        QStringLiteral("calendar-import-page-refreshed")
                        );

                    dialogGuard->reject();
                    app.processEvents();
                    QCoreApplication::sendPostedEvents(
                        nullptr,
                        QEvent::DeferredDelete
                        );
                    app.processEvents();
                    profiler.checkpoint(
                        QStringLiteral("calendar-import-dialog-released")
                        );
                    appendStartupWorkflowTrace(
                        QStringLiteral("calendar-import-dialog-released")
                        );
                    profiler.checkpoint(
                        QStringLiteral("calendar-import-operation-end"),
                        QStringLiteral("applied=true")
                        );
                    appendStartupWorkflowTrace(
                        QStringLiteral("calendar-import-operation-end")
                        );
                    completion();
                };

            QTimer::singleShot(
                0,
                &app,
                [preferencesAction]()
                {
                    preferencesAction->trigger();
                }
                );
            QTimer::singleShot(
                25,
                &app,
                [poll]()
                {
                    (*poll)();
                }
                );
        }
        );
}

void scheduleStartupPerformanceStaffDirectoryLifecycle(
    QApplication& app,
    MainWindow& window,
    StartupProfiler& profiler,
    const std::shared_ptr<bool>& workflowSucceeded,
    bool nativeDirectory,
    std::function<void()> completion
    )
{
    QTimer::singleShot(
        0,
        &app,
        [
            &app,
            &window,
            &profiler,
            workflowSucceeded,
            nativeDirectory,
            completion
        ]()
        {
            PageManager* pages = window.pageManager();
            const PageType pageType = nativeDirectory
                ? PageType::NativeEnglishTeachers
                : PageType::GsTeam;
            StaffDirectoryPage* directory = nullptr;
            if (pages)
            {
                directory = nativeDirectory
                    ? pages->nativeEnglishTeachersPage()
                    : pages->gsTeamPage();
            }

            bool operationStarted = false;
            const auto fail =
                [
                    &completion,
                    &workflowSucceeded,
                    &operationStarted,
                    nativeDirectory
                ](const QString& detail)
                {
                    *workflowSucceeded = false;
                    StartupProfiler::recordStaffDirectoryFailed(
                        nativeDirectory,
                        detail
                        );
                    if (operationStarted)
                    {
                        StartupProfiler::recordStaffDirectoryOperationReleased(
                            nativeDirectory
                            );
                    }
                    completion();
                };

            if (
                !pages
                || !directory
                || !pages->isCurrentPage(pageType)
                )
            {
                profiler.checkpoint(
                    QStringLiteral("staff-directory-lifecycle-failed"),
                    QStringLiteral(
                        "pageCurrent=%1; native=%2"
                        )
                        .arg(
                            pages && pages->isCurrentPage(pageType)
                                ? QStringLiteral("true")
                                : QStringLiteral("false")
                            )
                        .arg(
                            nativeDirectory
                                ? QStringLiteral("true")
                                : QStringLiteral("false")
                            )
                    );
                *workflowSucceeded = false;
                completion();
                return;
            }

            StartupProfiler::recordStaffDirectoryOperationStarted(
                nativeDirectory
                );
            operationStarted = true;

            if (!directory->loadDirectory())
            {
                fail(QStringLiteral("staff-directory-load-failed"));
                return;
            }
            app.processEvents();

            struct TableSnapshot
            {
                int rows = 0;
                int columns = 0;
                int items = 0;
                int pageWidgets = 0;
                qint64 textBytes = 0;
            };
            const auto snapshot =
                [](QTableWidget* table) -> TableSnapshot
                {
                    TableSnapshot result;
                    if (!table)
                    {
                        return result;
                    }
                    result.rows = table->rowCount();
                    result.columns = table->columnCount();
                    result.pageWidgets =
                        table->parentWidget()
                            ? table->parentWidget()->findChildren<QWidget*>().size()
                                + 1
                            : 0;
                    for (int row = 0; row < result.rows; ++row)
                    {
                        for (int column = 0; column < result.columns; ++column)
                        {
                            if (QTableWidgetItem* item = table->item(row, column))
                            {
                                ++result.items;
                                result.textBytes += item->text().toUtf8().size();
                            }
                        }
                    }
                    return result;
                };

            const QString configuredOutputRoot =
                qEnvironmentVariable(
                    "CLASSMNGR_STARTUP_STAFF_DIRECTORY_OUTPUT_DIR"
                    ).trimmed();
            const QString outputRoot = configuredOutputRoot.isEmpty()
                ? QDir(QDir::tempPath()).filePath(
                    QStringLiteral("ClassMngr-staff-directory-boundary")
                    )
                : QFileInfo(configuredOutputRoot).absoluteFilePath();
            if (!QDir().mkpath(outputRoot))
            {
                fail(QStringLiteral("staff-directory-output-unavailable"));
                return;
            }

            QTableWidget* table = directory->findChild<QTableWidget*>();
            if (!table)
            {
                fail(QStringLiteral("staff-directory-table-missing"));
                return;
            }
            TableSnapshot current = snapshot(table);
            StartupProfiler::recordStaffDirectoryPagePrepared(
                nativeDirectory,
                current.rows,
                current.columns,
                current.items,
                current.pageWidgets,
                current.textBytes
                );
            if (!directory->grab().save(
                    QDir(outputRoot).filePath(
                        nativeDirectory
                            ? QStringLiteral(
                                "staff-directory-native-english-teachers.png"
                                )
                            : QStringLiteral("staff-directory-gs-team.png")
                        ),
                    "PNG"
                    ))
            {
                fail(QStringLiteral("staff-directory-capture-failed"));
                return;
            }

            for (int refreshIndex = 1; refreshIndex <= 2; ++refreshIndex)
            {
                directory->refresh();
                app.processEvents();
                table = directory->findChild<QTableWidget*>();
                current = snapshot(table);
                if (!table || current.rows <= 0 || current.items <= 0)
                {
                    fail(
                        QStringLiteral(
                            "staff-directory-refresh-data-missing-%1"
                            ).arg(refreshIndex)
                        );
                    return;
                }
                StartupProfiler::recordStaffDirectoryRefreshed(
                    nativeDirectory,
                    refreshIndex,
                    current.rows,
                    current.columns,
                    current.items,
                    current.pageWidgets,
                    current.textBytes
                    );
            }

            pages->showPage(PageType::MyWorkspace);
            app.processEvents();
            if (!pages->isCurrentPage(PageType::MyWorkspace))
            {
                fail(QStringLiteral("staff-directory-leave-failed"));
                return;
            }
            StartupProfiler::recordStaffDirectoryLeft(nativeDirectory);

            pages->showPage(pageType);
            app.processEvents();
            if (!pages->isCurrentPage(pageType))
            {
                fail(QStringLiteral("staff-directory-reentry-failed"));
                return;
            }
            table = directory->findChild<QTableWidget*>();
            current = snapshot(table);
            if (!table || current.rows <= 0 || current.items <= 0)
            {
                fail(QStringLiteral("staff-directory-reentry-data-missing"));
                return;
            }
            StartupProfiler::recordStaffDirectoryReentered(
                nativeDirectory,
                current.rows,
                current.columns,
                current.items,
                current.pageWidgets,
                current.textBytes
                );
            StartupProfiler::recordStaffDirectoryOperationReleased(
                nativeDirectory
                );
            completion();
        }
        );
}

void scheduleStartupPerformanceWorkflow(
    QApplication& app,
    MainWindow& window,
    StartupProfiler& profiler,
    const std::shared_ptr<bool>& workflowSucceeded,
    bool scheduleLifecycleEnabled,
    bool scheduleImportLifecycleEnabled,
    bool scheduleImportApplyLifecycleEnabled,
    bool calendarImportLifecycleEnabled,
    bool classesLifecycleEnabled,
    bool classTransferLifecycleEnabled,
    bool speakingEvaluationLifecycleEnabled,
    bool staffDirectoryLifecycleEnabled,
    bool subPrepLifecycleEnabled,
    bool subPrepOutputLifecycleEnabled,
    bool subPrepVisualStatesEnabled,
    const QString& subPrepVisualOutputDirectoryPath,
    std::function<void()> completion
    )
{
    const auto pageTypes =
        std::make_shared<QList<PageType>>(startupWorkflowPageTypes());
    const auto pageIndex = std::make_shared<int>(0);
    const auto runNextPage =
        std::make_shared<std::function<void()>>();

    *runNextPage =
        [
            &app,
            &window,
            &profiler,
            workflowSucceeded,
            scheduleLifecycleEnabled,
            scheduleImportLifecycleEnabled,
            scheduleImportApplyLifecycleEnabled,
            calendarImportLifecycleEnabled,
            classesLifecycleEnabled,
            classTransferLifecycleEnabled,
            speakingEvaluationLifecycleEnabled,
            staffDirectoryLifecycleEnabled,
            subPrepLifecycleEnabled,
            subPrepOutputLifecycleEnabled,
            subPrepVisualStatesEnabled,
            subPrepVisualOutputDirectoryPath,
            pageTypes,
            pageIndex,
            runNextPage,
            completion
        ]()
    {
        if (*pageIndex >= pageTypes->size())
        {
            appendStartupWorkflowTrace(QStringLiteral("complete"));
            qInfo().noquote()
                << QStringLiteral(
                    "Startup performance workflow completed; returning to My Workspace."
                    );
            profiler.checkpoint(
                QStringLiteral("workflow-complete"),
                QStringLiteral("returned-to-my-workspace")
                );
            completion();
            return;
        }

        const PageType pageType = pageTypes->at(*pageIndex);
        const QString pageIdentifier =
            PageManager::pageTypeIdentifier(pageType);

        appendStartupWorkflowTrace(
            QStringLiteral("start %1").arg(pageIdentifier)
            );
        qInfo().noquote()
            << QStringLiteral("Startup performance workflow entering %1.")
                .arg(pageIdentifier);

        profiler.checkpoint(
            QStringLiteral("workflow-page-start"),
            pageIdentifier
            );

        PageManager* pages = window.pageManager();
        const QString previousPageIdentifier =
            pages ? pages->currentPageIdentifier() : QString();
        bool pageReady = false;
        if (pages)
        {
            appendStartupWorkflowTrace(
                QStringLiteral("showPage %1").arg(pageIdentifier)
                );
            if (
                (subPrepLifecycleEnabled || subPrepVisualStatesEnabled)
                && pageType == PageType::SubPrep
                )
            {
                StartupProfiler::setSubPrepDiagnosticsActive(true);
            }
            pages->showPage(pageType);
            appendStartupWorkflowTrace(
                QStringLiteral("showPage-returned %1").arg(pageIdentifier)
                );
            app.processEvents();
            appendStartupWorkflowTrace(
                QStringLiteral("events-processed %1").arg(pageIdentifier)
                );
            pageReady = pages->isCurrentPage(pageType);
        }

        if (
            pageReady
            && !previousPageIdentifier.isEmpty()
            && previousPageIdentifier != pageIdentifier
            )
        {
            profiler.checkpoint(
                QStringLiteral("workflow-page-left"),
                previousPageIdentifier
                );
        }

        if (!pageReady)
        {
            *workflowSucceeded = false;
            profiler.checkpoint(
                QStringLiteral("workflow-page-failed"),
                pageIdentifier
                );
        }
        else
        {
            qInfo().noquote()
                << QStringLiteral("Startup performance workflow ready: %1.")
                    .arg(pageIdentifier);
            profiler.checkpoint(
                QStringLiteral("workflow-page-ready"),
                pageIdentifier
                );
        }

        // Calendar is deliberately a child of My Workspace and is not
        // created by the startup route. Open it once in the workflow, then
        // return to the normal schedule tab before leaving the page.
        if (
            pageReady
            && *pageIndex == 0
            && pageType == PageType::MyWorkspace
            && pages->myWorkspacePage()
            )
        {
            MyWorkspacePage* workspace = pages->myWorkspacePage();
            profiler.checkpoint(
                QStringLiteral("workflow-child-start"),
                QStringLiteral("calendar")
                );
            appendStartupWorkflowTrace(QStringLiteral("calendar-start"));
            workspace->openTab(WorkspaceTab::Calendar);
            appendStartupWorkflowTrace(QStringLiteral("calendar-open-returned"));
            app.processEvents();
            appendStartupWorkflowTrace(QStringLiteral("calendar-events-processed"));

            const bool calendarReady = workspace->calendarPage() != nullptr;
            if (!calendarReady)
            {
                *workflowSucceeded = false;
            }
            profiler.checkpoint(
                calendarReady
                    ? QStringLiteral("workflow-child-ready")
                    : QStringLiteral("workflow-child-failed"),
                QStringLiteral("calendar")
                );

            const auto returnToSchedule =
                [
                    &app,
                    &profiler,
                    pageIndex,
                    runNextPage,
                    workspace,
                    calendarImportLifecycleEnabled
                ]()
                {
                    workspace->openTab(WorkspaceTab::Schedule);
                    appendStartupWorkflowTrace(
                        QStringLiteral("calendar-schedule-returned")
                        );
                    app.processEvents();
                    if (calendarImportLifecycleEnabled)
                    {
                        profiler.checkpoint(
                            QStringLiteral("calendar-import-page-released")
                            );
                        appendStartupWorkflowTrace(
                            QStringLiteral("calendar-import-page-released")
                            );
                    }
                    profiler.checkpoint(
                        QStringLiteral("workflow-child-released"),
                        QStringLiteral("calendar")
                        );
                    ++*pageIndex;
                    QTimer::singleShot(
                        StartupWorkflowStepDelayMilliseconds,
                        &app,
                        [runNextPage]()
                        {
                            (*runNextPage)();
                        }
                        );
                };

            if (calendarImportLifecycleEnabled && calendarReady)
            {
                scheduleStartupPerformanceCalendarImportLifecycle(
                    app,
                    window,
                    profiler,
                    workflowSucceeded,
                    returnToSchedule
                    );
                return;
            }

            returnToSchedule();
            return;
        }

        if (
            pageReady
            && (
                scheduleImportLifecycleEnabled
                || scheduleImportApplyLifecycleEnabled
                )
            && pageType == PageType::Schedule
        )
        {
            scheduleStartupPerformanceScheduleImportLifecycle(
                app,
                window,
                profiler,
                workflowSucceeded,
                scheduleImportApplyLifecycleEnabled,
                [
                    &app,
                    pageIndex,
                    runNextPage
                ]()
                {
                    ++*pageIndex;
                    QTimer::singleShot(
                        StartupWorkflowStepDelayMilliseconds,
                        &app,
                        [runNextPage]()
                        {
                            (*runNextPage)();
                        }
                        );
                }
                );
            return;
        }

        if (
            pageReady
            && scheduleLifecycleEnabled
            && pageType == PageType::Schedule
            )
        {
            scheduleStartupPerformanceScheduleLifecycle(
                app,
                window,
                profiler,
                workflowSucceeded,
                [
                    &app,
                    pageIndex,
                    runNextPage
                ]()
                {
                    ++*pageIndex;
                    QTimer::singleShot(
                        StartupWorkflowStepDelayMilliseconds,
                        &app,
                        [runNextPage]()
                        {
                            (*runNextPage)();
                        }
                        );
                }
                );
            return;
        }

        if (
            pageReady
            && speakingEvaluationLifecycleEnabled
            && pageType == PageType::Classes
            )
        {
            scheduleStartupPerformanceSpeakingEvaluationLifecycle(
                app,
                window,
                profiler,
                workflowSucceeded,
                [
                    &app,
                    pageIndex,
                    runNextPage
                ]()
                {
                    ++*pageIndex;
                    QTimer::singleShot(
                        StartupWorkflowStepDelayMilliseconds,
                        &app,
                        [runNextPage]()
                        {
                            (*runNextPage)();
                        }
                        );
                }
                );
            return;
        }

        if (
            pageReady
            && staffDirectoryLifecycleEnabled
            && (
                pageType == PageType::NativeEnglishTeachers
                || pageType == PageType::GsTeam
                )
            )
        {
            scheduleStartupPerformanceStaffDirectoryLifecycle(
                app,
                window,
                profiler,
                workflowSucceeded,
                pageType == PageType::NativeEnglishTeachers,
                [
                    &app,
                    pageIndex,
                    runNextPage
                ]()
                {
                    ++*pageIndex;
                    QTimer::singleShot(
                        StartupWorkflowStepDelayMilliseconds,
                        &app,
                        [runNextPage]()
                        {
                            (*runNextPage)();
                        }
                        );
                }
                );
            return;
        }

        if (
            pageReady
            && classTransferLifecycleEnabled
            && pageType == PageType::Classes
            )
        {
            scheduleStartupPerformanceClassTransferLifecycle(
                app,
                window,
                profiler,
                workflowSucceeded,
                [
                    &app,
                    pageIndex,
                    runNextPage
                ]()
                {
                    ++*pageIndex;
                    QTimer::singleShot(
                        StartupWorkflowStepDelayMilliseconds,
                        &app,
                        [runNextPage]()
                        {
                            (*runNextPage)();
                        }
                        );
                }
                );
            return;
        }

        if (
            pageReady
            && classesLifecycleEnabled
            && pageType == PageType::Classes
            )
        {
            scheduleStartupPerformanceClassesLifecycle(
                app,
                window,
                profiler,
                workflowSucceeded,
                [
                    &app,
                    pageIndex,
                    runNextPage
                ]()
                {
                    ++*pageIndex;
                    QTimer::singleShot(
                        StartupWorkflowStepDelayMilliseconds,
                        &app,
                        [runNextPage]()
                        {
                            (*runNextPage)();
                        }
                        );
                }
                );
            return;
        }

        if (
            pageReady
            && subPrepVisualStatesEnabled
            && pageType == PageType::SubPrep
            )
        {
            scheduleStartupPerformanceSubPrepVisualStates(
                app,
                window,
                profiler,
                workflowSucceeded,
                subPrepVisualOutputDirectoryPath,
                [
                    &app,
                    pageIndex,
                    runNextPage
                ]()
                {
                    ++*pageIndex;
                    QTimer::singleShot(
                        StartupWorkflowStepDelayMilliseconds,
                        &app,
                        [runNextPage]()
                        {
                            (*runNextPage)();
                        }
                        );
                }
                );
            return;
        }

        if (
            pageReady
            && subPrepLifecycleEnabled
            && pageType == PageType::SubPrep
            )
        {
            scheduleStartupPerformanceSubPrepLifecycle(
                app,
                window,
                profiler,
                workflowSucceeded,
                subPrepOutputLifecycleEnabled,
                [
                    &app,
                    pageIndex,
                    runNextPage
                ]()
                {
                    ++*pageIndex;
                    QTimer::singleShot(
                        StartupWorkflowStepDelayMilliseconds,
                        &app,
                        [runNextPage]()
                        {
                            (*runNextPage)();
                        }
                        );
                }
                );
            return;
        }

        if (pageReady && pageType == PageType::PdfViewer)
        {
            scheduleStartupPerformancePdfLifecycle(
                app,
                window,
                profiler,
                workflowSucceeded,
                [
                    &app,
                    pageIndex,
                    runNextPage
                ]()
                {
                    ++*pageIndex;
                    QTimer::singleShot(
                        StartupWorkflowStepDelayMilliseconds,
                        &app,
                        [runNextPage]()
                        {
                            (*runNextPage)();
                        }
                        );
                }
                );
            return;
        }

        ++*pageIndex;
        QTimer::singleShot(
            StartupWorkflowStepDelayMilliseconds,
            &app,
            [runNextPage]()
            {
                (*runNextPage)();
            }
            );
    };

    QTimer::singleShot(
        0,
        &app,
        [runNextPage]()
        {
            (*runNextPage)();
        }
        );
}

bool writeStartupPerformanceMetrics(
    const QString& outputPath,
    const StartupProfiler& profiler,
    const StartupPerformanceMode& mode,
    int progressUpdates,
    int finalProgress
    )
{
    if (outputPath.trimmed().isEmpty())
    {
        qWarning()
            << "Startup performance output path was not provided.";
        return false;
    }

    QFile file(outputPath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qWarning().noquote()
            << QStringLiteral(
                "Unable to write startup performance metrics to %1: %2"
                )
                .arg(outputPath, file.errorString());
        return false;
    }

    QJsonObject metrics = profiler.reportJson();

    metrics.insert(
        QStringLiteral("progressUpdates"),
        progressUpdates
        );
    metrics.insert(
        QStringLiteral("finalProgress"),
        finalProgress
        );

    metrics.insert(
        QStringLiteral("format"),
        QStringLiteral("classmngr-startup-profile-v2")
        );

    QJsonArray scenarioActions{
        mode.scenario == StartupPerformanceMode::Scenario::Minimal
            ? QStringLiteral("launch without a startup database")
            : QStringLiteral("load the supplied or saved startup database"),
        QStringLiteral("construct the main window"),
        QStringLiteral("capture startup-complete and requested settled checkpoints"),
        QStringLiteral("suppress interactive startup prompts during profiling")
    };
    if (mode.workflowEnabled)
    {
        scenarioActions.append(
            QStringLiteral(
                "navigate through all registered page routes and return to My Workspace"
                )
            );
    }
    if (mode.subPrepLifecycleEnabled)
    {
        scenarioActions.append(
            QStringLiteral(
                "exercise large Sub Prep selection, refresh, leave, and repeated re-entry"
                )
        );
    }
    if (mode.subPrepOutputLifecycleEnabled)
    {
        scenarioActions.append(
            QStringLiteral(
                "exercise the real heavy Sub Prep generation dialog, PDF outputs, first-page decoding, and release"
                )
        );
    }
    if (mode.subPrepVisualStatesEnabled)
    {
        scenarioActions.append(
            QStringLiteral(
                "capture populated, changed-selection, and empty Sub Prep visual states"
                )
        );
    }
    if (mode.classesLifecycleEnabled)
    {
        scenarioActions.append(
            QStringLiteral(
                "exercise large Classes selection, refresh, leave, and repeated re-entry"
                )
            );
    }
    if (mode.classTransferLifecycleEnabled)
    {
        scenarioActions.append(
            QStringLiteral(
                "exercise large multi-class transfer package review, transaction commit, cleanup, and Classes refresh"
                )
            );
    }
    if (mode.speakingEvaluationLifecycleEnabled)
    {
        scenarioActions.append(
            QStringLiteral(
                "exercise large Speaking Evaluation page, report review, AI batch review, PDF export, cleanup, and refresh"
                )
            );
    }
    if (mode.staffDirectoryLifecycleEnabled)
    {
        scenarioActions.append(
            QStringLiteral(
                "exercise large Native English Teacher and GS Team directory tables, refresh, leave, re-entry, and retention"
                )
            );
    }
    if (mode.scheduleLifecycleEnabled)
    {
        scenarioActions.append(
            QStringLiteral(
                "exercise large Schedule refresh, leave, and repeated re-entry"
                )
            );
    }
    if (mode.calendarImportLifecycleEnabled)
    {
        scenarioActions.append(
            QStringLiteral(
                "exercise large Calendar workbook import, Preferences close, and Calendar cache refresh"
                )
            );
    }
    if (mode.scheduleImportApplyLifecycleEnabled)
    {
        scenarioActions.append(
            QStringLiteral(
                "exercise large Schedule Import workbook review, transaction commit, cleanup, and schedule refresh"
                )
            );
    }

    QJsonArray workflowPages;
    for (const PageType pageType : startupWorkflowPageTypes())
    {
        workflowPages.append(PageManager::pageTypeIdentifier(pageType));
    }

    metrics.insert(
        QStringLiteral("scenario"),
        QJsonObject{
            {QStringLiteral("name"), startupScenarioName(mode.scenario)},
            {QStringLiteral("actions"), scenarioActions},
            {QStringLiteral("settleMilliseconds"), mode.settleMilliseconds}
        }
        );

    metrics.insert(
        QStringLiteral("workflow"),
        QJsonObject{
            {QStringLiteral("enabled"), mode.workflowEnabled},
            {QStringLiteral("stepDelayMilliseconds"),
             StartupWorkflowStepDelayMilliseconds},
            {QStringLiteral("pages"), workflowPages}
        }
        );

    file.write(
        QJsonDocument(metrics).toJson(QJsonDocument::Indented)
        );

    return file.error() == QFile::NoError;
}



// =========================================================
// Main
// =========================================================

int main(int argc, char *argv[])
{
    StartupProfiler startupProfiler;
    QStringList launchArguments;
    launchArguments.reserve(argc);
    for (int index = 0; index < argc; ++index)
    {
        launchArguments.append(QString::fromLocal8Bit(argv[index]));
    }

    const StartupPerformanceMode startupPerformance =
        startupPerformanceMode(launchArguments);
    if (startupPerformance.enabled)
    {
        StartupProfiler::activate(&startupProfiler);
        startupProfiler.checkpoint(QStringLiteral("process-start"));
    }

#if !defined(Q_OS_MACOS)
    // The custom file-dialog style is for Qt's widget dialog.  On macOS,
    // retain the native NSOpenPanel: forcing the widget implementation can
    // deadlock while it is initialized.
    QApplication::setAttribute(
        Qt::AA_DontUseNativeDialogs
        );
#endif
    QApplication app(argc, argv);

#if !defined(Q_OS_MACOS)
    app.setStyle(
        new FileDialogIconStyle()
        );
#endif

    if (startupPerformance.enabled)
    {
        startupProfiler.checkpoint(QStringLiteral("qapplication-created"));
    }

    const QString initialDatabasePath =
        startupPerformance.enabled
            && !startupPerformance.visualCaptureEnabled
            && startupPerformance.scenario
                == StartupPerformanceMode::Scenario::Minimal
            ? QString()
            : startupDatabasePath(
                app.arguments()
                );

    app.setApplicationName(AppSettings::ApplicationName);
    app.setOrganizationName(AppSettings::OrganizationName);
    app.setApplicationVersion(
        QString::fromUtf8(BuildInfo::Version)
        );

    // =====================================================
    // Translation Support
    // =====================================================

    LanguageService languageService;

    const Language savedLanguage =
        startupPerformance.visualCaptureEnabled
        && startupPerformance.visualLanguageOverride.has_value()
            ? *startupPerformance.visualLanguageOverride
            : LanguageService::savedLanguage();

    const FontSize savedFontSize =
        fontSizeFromStoredValue(
            SettingsManager::instance().get(
                OptionKeys::FontSize,
                fontSizeOffset(FontSize::Normal)
                ).toInt()
            );

    const Theme savedTheme =
        startupPerformance.visualCaptureEnabled
        && startupPerformance.visualThemeOverride.has_value()
            ? *startupPerformance.visualThemeOverride
            : themeFromStoredValue(
                  SettingsManager::instance().get(
                      OptionKeys::Theme,
                      static_cast<int>(Theme::SystemDefault)
                      ).toInt()
                  );

    if (startupPerformance.enabled)
    {
        startupProfiler.checkpoint(QStringLiteral("preferences-resolved"));
    }

    languageService.setLanguage(
        savedLanguage
        );

    if (startupPerformance.enabled)
    {
        startupProfiler.checkpoint(QStringLiteral("locale-applied"));
    }



    // =====================================================
    // Fonts
    // =====================================================

    FontManager::setSizeOffset(
        fontSizeOffset(savedFontSize)
        );

    FontManager::applyGlobalFont(
        app,
        languageService.loadedLocaleName()
        );

    if (startupPerformance.enabled)
    {
        startupProfiler.checkpoint(QStringLiteral("font-applied"));
    }
    // FontManager::debugDump();

    auto startupThemeService =
        std::make_unique<ThemeService>();
    startupThemeService->setTheme(savedTheme);

    if (startupPerformance.enabled)
    {
        startupProfiler.checkpoint(QStringLiteral("theme-applied"));
    }


    // =====================================================
    // Splash Screen
    // =====================================================

    if (
        const Status resourcePackStatus =
            ResourcePackManager::instance().initialize();
        !resourcePackStatus
        )
    {
        qWarning().noquote() << resourcePackStatus.error();
    }

    if (startupPerformance.enabled)
    {
        startupProfiler.checkpoint(
            QStringLiteral("resource-system-initialized")
            );
    }

    auto splashLease = ResourcePaths::Splash::acquire();
    if (!splashLease)
    {
        qWarning().noquote() << splashLease.error();
        return 1;
    }

    auto splash = std::make_unique<SplashScreen>(
        ResourcePaths::Splash::imagePath(*splashLease)
        );

    splash->centerOnScreen();
    splash->show();

    app.processEvents();

    if (startupPerformance.enabled)
    {
        startupProfiler.checkpoint(QStringLiteral("splash-shown"));
    }



    // =====================================================
    // Progress Callback
    // =====================================================

    // Language and font setup complete before the splash can be displayed.
    int completedStartupSteps = 2;
    int progressUpdates = 0;
    int progress = 0;
    bool visualCaptureSucceeded = true;

    auto updateProgress =
        [&](const QString &message)
    {
        ++completedStartupSteps;
        ++progressUpdates;

        progress =
            qMin(
                100,
                (100 * completedStartupSteps)
                    / AppSettings::StartupProgressSteps
                );

        splash->setMessage(message);
        splash->setProgress(progress);

        app.processEvents();
    };



    // =====================================================
    // Startup Timer
    // =====================================================

    QElapsedTimer startupTimer;

    startupTimer.start();

    // =====================================================
    // Main Window
    // =====================================================

    updateProgress(
        QCoreApplication::translate(
            "MainWindow",
            "Loading resource packs..."
            )
        );

    updateProgress(
        QCoreApplication::translate(
            "MainWindow",
            "Loading application icon..."
            )
        );

    app.setWindowIcon(getAppIcon());

    std::unique_ptr<UpdateService> updateService;
    std::unique_ptr<UpdateController> updateController;

    MainWindow window(
        updateProgress,
        isAdminMode(app.arguments()),
        &languageService,
        nullptr,
        {
            .loadMostRecentDatabase =
                !startupPerformance.enabled
                || startupPerformance.scenario
                    == StartupPerformanceMode::Scenario::Representative,
            .initialDatabasePath = initialDatabasePath,
            .startupThemeService = std::move(startupThemeService),
            .checkpointCallback =
                [&startupProfiler, &startupPerformance](
                    const QString& name,
                    const QString& detail
                    )
                {
                    if (startupPerformance.enabled)
                    {
                        startupProfiler.checkpoint(name, detail);
                    }
                }
        }
        );

    updateProgress(
        QCoreApplication::translate(
            "MainWindow",
            "Ready..."
            )
        );

    Q_ASSERT(
        completedStartupSteps
            == AppSettings::StartupProgressSteps
        );

    // =====================================================
    // Ensure Minimum Splash Time
    // =====================================================

    int elapsed =
        static_cast<int>(
            startupTimer.elapsed()
            );

    int remaining =
        qMax(0, AppSettings::MinimumSplashDurationMs - elapsed);



    // =====================================================
    // Finish Startup
    // =====================================================

    auto runPostStartupTasks =
        [
            &app,
            &window,
            &updateService,
            &updateController,
            &startupPerformance,
            &startupProfiler,
            &visualCaptureSucceeded,
            progressUpdates,
            progress
        ]()
    {
        // These tasks intentionally run in a later event-loop turn, after
        // startup-complete. Normal startup only starts the updater here;
        // explicit profiling may opt into the page lifecycle workflow below.
        if (!startupPerformance.enabled)
        {
            updateService = std::make_unique<UpdateService>();
            updateController = std::make_unique<UpdateController>(
                updateService.get(),
                &app
                );
            window.attachUpdateController(updateController.get());
            updateController->setStartupComplete();
            updateController->startAutomaticCheck();
            return;
        }

        const auto workflowSucceeded = std::make_shared<bool>(true);

        const auto finishPerformanceRun =
            [
                &app,
                &window,
                &startupProfiler,
                &startupPerformance,
                &visualCaptureSucceeded,
                workflowSucceeded,
                progressUpdates,
                progress
            ]()
        {
            if (
                startupPerformance.visualCaptureEnabled
                && startupPerformance.settleMilliseconds > 0
                )
            {
                app.processEvents();
                visualCaptureSucceeded =
                    captureStartupVisual(
                        startupPerformance.visualCaptureOutputPath,
                        window,
                        QStringLiteral("settled-final")
                        )
                    && visualCaptureSucceeded;
            }

            const bool metricsWritten =
                startupPerformance.visualCaptureEnabled
                && startupPerformance.outputPath.trimmed().isEmpty()
                ? true
                : writeStartupPerformanceMetrics(
                      startupPerformance.outputPath,
                      startupProfiler,
                      startupPerformance,
                      progressUpdates,
                      progress
                      );
            app.exit(
                metricsWritten
                    && visualCaptureSucceeded
                    && *workflowSucceeded
                    ? 0
                    : 2
                );
        };

        const auto scheduleSettledCompletion =
            [
                &app,
                &startupProfiler,
                &startupPerformance,
                finishPerformanceRun
            ]()
        {
            const int settleMilliseconds =
                startupPerformance.settleMilliseconds;
            if (settleMilliseconds >= 1000)
            {
                QTimer::singleShot(
                    1000,
                    &app,
                    [&startupProfiler]()
                    {
                        startupProfiler.checkpoint(
                            QStringLiteral("settled-1s")
                            );
                    }
                    );
            }
            if (settleMilliseconds >= 5000)
            {
                QTimer::singleShot(
                    5000,
                    &app,
                    [&startupProfiler]()
                    {
                        startupProfiler.checkpoint(
                            QStringLiteral("settled-5s")
                            );
                    }
                    );
            }
            if (settleMilliseconds >= 30000)
            {
                QTimer::singleShot(
                    30000,
                    &app,
                    [&startupProfiler]()
                    {
                        startupProfiler.checkpoint(
                            QStringLiteral("settled-30s")
                            );
                    }
                    );
            }
            if (settleMilliseconds >= StartupFiveMinuteIdleMilliseconds)
            {
                QTimer::singleShot(
                    StartupFiveMinuteIdleMilliseconds,
                    &app,
                    [&startupProfiler]()
                    {
                        startupProfiler.checkpoint(
                            QStringLiteral("settled-5m")
                            );
                    }
                    );
            }

            const int completionDelayMilliseconds =
                settleMilliseconds > 0
                    ? settleMilliseconds + 500
                    : 0;
            QTimer::singleShot(
                completionDelayMilliseconds,
                &app,
                [
                    &startupProfiler,
                    settleMilliseconds,
                    finishPerformanceRun
                ]()
                {
                    if (
                        settleMilliseconds > 0
                        && settleMilliseconds != 1000
                        && settleMilliseconds != 5000
                        && settleMilliseconds != 30000
                        && settleMilliseconds != StartupFiveMinuteIdleMilliseconds
                        )
                    {
                        startupProfiler.checkpoint(
                            QStringLiteral("settled-final"),
                            QStringLiteral("elapsedMs=%1")
                                .arg(settleMilliseconds)
                            );
                    }
                    finishPerformanceRun();
                }
                );
        };

        if (startupPerformance.workflowEnabled)
        {
            scheduleStartupPerformanceWorkflow(
                app,
                window,
                startupProfiler,
                workflowSucceeded,
                startupPerformance.scheduleLifecycleEnabled,
                startupPerformance.scheduleImportLifecycleEnabled,
                startupPerformance.scheduleImportApplyLifecycleEnabled,
                startupPerformance.calendarImportLifecycleEnabled,
                startupPerformance.classesLifecycleEnabled,
                startupPerformance.classTransferLifecycleEnabled,
                startupPerformance.speakingEvaluationLifecycleEnabled,
                startupPerformance.staffDirectoryLifecycleEnabled,
                startupPerformance.subPrepLifecycleEnabled,
                startupPerformance.subPrepOutputLifecycleEnabled,
                startupPerformance.subPrepVisualStatesEnabled,
                startupPerformance.visualCaptureOutputPath,
                scheduleSettledCompletion
                );
            return;
        }

        scheduleSettledCompletion();
    };

    // This is the single transition from startup to normal operation.  The
    // splash and its resource lease are gone before the checkpoint is taken,
    // and all optional work is queued only after that snapshot.
    auto finish =
        [
            &app,
            &window,
            &splash,
            &splashLease,
            &startupPerformance,
            &startupProfiler,
            &visualCaptureSucceeded,
            runPostStartupTasks
        ]()
    {
        window.show();
        Q_ASSERT(window.isVisible());

        if (startupPerformance.enabled)
        {
            startupProfiler.checkpoint(QStringLiteral("window-shown"));
        }

        splash.reset();
        splashLease->reset();

        if (startupPerformance.enabled)
        {
            startupProfiler.checkpoint(QStringLiteral("startup-complete"));
            StartupProfiler::recordStartupCompleteScheduleWidgetDiagnostic();
        }

        if (startupPerformance.visualCaptureEnabled)
        {
            app.processEvents();
            visualCaptureSucceeded =
                captureStartupVisual(
                    startupPerformance.visualCaptureOutputPath,
                    window,
                    QStringLiteral("startup-complete")
                    )
                && visualCaptureSucceeded;
        }

        QTimer::singleShot(
            0,
            &app,
            runPostStartupTasks
            );
    };

    auto startFinish = [&splash, &finish]()
    {
        splash->fadeOut(finish);
    };

    if (remaining <= 0)
        startFinish();
    else
        QTimer::singleShot(remaining, startFinish);



    // =====================================================
    // Run App
    // =====================================================

    return app.exec();
}
