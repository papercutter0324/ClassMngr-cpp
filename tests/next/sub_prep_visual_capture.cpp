#include "core/process_memory_snapshot.h"
#include "data/database/workspace_store.h"
#include "features/sub_prep/ui/sub_prep_class_information_list_model.h"
#include "next/platform/workspace_store_gateway.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/page_host.h"
#include "next/ui/sub_prep_page.h"
#include "preferences_test_ports.h"
#include "sql_script_runner.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QTranslator>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr;
using namespace ClassMngr::Next;

namespace
{

constexpr QSize Viewport{800, 600};

QString readText(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly | QIODevice::Text)
        ? QString::fromUtf8(file.readAll()) : QString{};
}

QPalette themePalette(const QString& theme)
{
    QPalette palette;
    const bool dark = theme == QStringLiteral("dark");
    palette.setColor(QPalette::Button,
        QColor(dark ? QStringLiteral("#292c30") : QStringLiteral("#fcfcfc")));
    palette.setColor(QPalette::ButtonText,
        QColor(dark ? QStringLiteral("#fcfcfc") : QStringLiteral("#232629")));
    palette.setColor(QPalette::Mid,
        QColor(dark ? QStringLiteral("#4b5056") : QStringLiteral("#b0b5bb")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#3daee9")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    return palette;
}

bool seedWorkspace(
    const QString& path,
    const QString& fixtureName,
    QString* error
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.open(path);
    if (!opened)
    {
        *error = opened.error().diagnostic;
        return false;
    }
    QFile fixture(QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral("/tests/fixtures/workspaces/") + fixtureName);
    if (!fixture.open(QIODevice::ReadOnly))
    {
        *error = QStringLiteral("Could not read the retained large_startup.sql fixture.");
        return false;
    }
    return Test::executeSqlScript(
        store.database(), QString::fromUtf8(fixture.readAll()), error);
}

ClassMngr::Next::Ui::SubPrepPageActions actionsFor(
    Platform::WorkspaceStoreGateway& gateway,
    const QString& identity
    )
{
    Ui::SubPrepPageActions actions;
    actions.workspaceIdentity = [&gateway, identity] {
        static_cast<void>(gateway);
        return identity;
    };
    actions.readSchedulePage = [&gateway] {
        return gateway.readSubPrepSchedulePage();
    };
    actions.readSummaries = [&gateway](
        const Application::SubPrepScheduleScopeRequest& request) {
        return gateway.readSubPrepSummaries(request);
    };
    actions.readSelectedDetails = [&gateway](const Next::Domain::ClassId& id) {
        return gateway.readSubPrepSelectedDetails(id);
    };
    actions.readPreferences = [&gateway] {
        return gateway.readSubPrepPreferences();
    };
    actions.savePreferences = [&gateway](
        const Application::SubPrepPreferences& preferences) {
        return gateway.saveSubPrepPreferences(preferences);
    };
    actions.readPersonalZoomPreferences = [&gateway] {
        return gateway.readSubPrepPersonalZoomPreferences();
    };
    actions.readCurrentCampus = [&gateway] {
        return gateway.readSubPrepCurrentCampus();
    };
    actions.readCampuses = [] {
        return std::vector<Application::SubPrepCampusMetadata>{};
    };
    actions.readFolderUserName = [&gateway] {
        return gateway.readSubPrepFolderUserName();
    };
    actions.saveFolderUserName = [&gateway](const std::string& name) {
        return gateway.saveSubPrepFolderUserName(name);
    };
    actions.readCalendarIntervals = [&gateway](
        const Application::SubPrepCalendarEventIntervalsRequest& request) {
        return gateway.readSubPrepCalendarEventIntervals(request);
    };
    actions.readPrintSource = [&gateway](
        const Application::SubPrepPrintSourceRequest& request) {
        return gateway.readSubPrepPrintSource(request);
    };
    actions.createRosterOutputSourceReadPort = [&gateway] {
        return gateway.createSubPrepRosterOutputSourceReadPort();
    };
    return actions;
}

class EmptyPage final : public Ui::Page
{
public:
    using Ui::Page::Page;
};

int widgetCount(QWidget* root)
{
    return root ? root->findChildren<QWidget*>().size() + 1 : 0;
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

QJsonObject metricsJson(
    const Ui::SubPrepPage& page,
    const int widgets
    )
{
    const auto metrics = page.visualMetrics();
    return {
        {QStringLiteral("summaryModelCount"), metrics.summaryModelCount},
        {QStringLiteral("visibleSummaryCount"), metrics.visibleSummaryCount},
        {QStringLiteral("projectionClassCount"), static_cast<int>(
            page.summaryModel()->projection().classes().size())},
        {QStringLiteral("activeGradeRowCount"), metrics.summaryRowCount},
        {QStringLiteral("selectedDetailPanelCount"),
         metrics.selectedDetailPanelCount},
        {QStringLiteral("textEditorCount"), metrics.textEditorCount},
        {QStringLiteral("summaryReadCount"), metrics.summaryReadCount},
        {QStringLiteral("selectedDetailReadCount"),
         metrics.selectedDetailReadCount},
        {QStringLiteral("scheduleReadCount"), metrics.scheduleReadCount},
        {QStringLiteral("selectedDetailsRetained"),
         metrics.selectedDetailsRetained},
        {QStringLiteral("selectedClassId"), page.selectedClassId()
            ? QString::fromUtf8(page.selectedClassId()->value()) : QString{}},
        {QStringLiteral("widgetCount"), widgets}
    };
}

bool saveCapture(
    QWidget& widget,
    const QString& path,
    QString* error
    )
{
    widget.setFixedSize(Viewport);
    widget.show();
    QApplication::processEvents();
    if (QWidget* const focused = QApplication::focusWidget())
    {
        focused->clearFocus();
    }
    QApplication::processEvents();
    const QImage image = widget.grab().toImage();
    if (image.size() != Viewport || !image.save(path, "PNG"))
    {
        *error = QStringLiteral(
            "Could not save the requested Sub Prep capture: %1 (%2x%3)")
            .arg(path).arg(image.width()).arg(image.height());
        return false;
    }
    return true;
}

bool generateOutput(
    Ui::SubPrepPage& page,
    const QString& outputRoot,
    QString* outputStatus,
    QString* error
    )
{
    auto* const generate = page.findChild<QPushButton*>(
        QStringLiteral("subPrepGenerateButton"));
    if (!generate)
    {
        *error = QStringLiteral("The Sub Prep output action is missing.");
        return false;
    }

    bool dialogAccepted = false;
    bool automationFailed = false;
    QTimer automation;
    automation.setInterval(10);
    QObject::connect(&automation, &QTimer::timeout, &page, [&] {
        auto* const active = QApplication::activeModalWidget();
        auto* const dialog = qobject_cast<QDialog*>(active);
        if (!dialog)
        {
            return;
        }

        if (dialog->objectName() == QStringLiteral("subPrepOutputDialog"))
        {
            auto* const folder = dialog->findChild<QLineEdit*>(
                QStringLiteral("subPrepOutputFolder"));
            auto* const name = dialog->findChild<QLineEdit*>(
                QStringLiteral("subPrepOutputUserName"));
            auto* const openAfter = dialog->findChild<QCheckBox*>(
                QStringLiteral("subPrepOpenFolderAfterGeneration"));
            auto* const vacation = dialog->findChild<QCheckBox*>(
                QStringLiteral("subPrepNextVacationCheckBox"));
            auto* const buttons = dialog->findChild<QDialogButtonBox*>();
            if (!folder || !name || !openAfter || !buttons
                || !buttons->button(QDialogButtonBox::Ok))
            {
                automationFailed = true;
                dialog->reject();
                return;
            }
            folder->setText(outputRoot);
            name->setText(QStringLiteral("C44 Next"));
            openAfter->setChecked(false);
            if (vacation && vacation->isEnabled())
            {
                vacation->setChecked(false);
            }
            for (int index = 0; index < 7; ++index)
            {
                if (auto* const day = dialog->findChild<QCheckBox*>(
                        QStringLiteral("subPrepOutputDay_%1").arg(index)))
                {
                    day->setChecked(index == 0);
                }
            }
            dialogAccepted = true;
            buttons->button(QDialogButtonBox::Ok)->click();
            return;
        }

        if (auto* const message = qobject_cast<QMessageBox*>(dialog))
        {
            if (message->text().contains(QStringLiteral("Replace the existing folder")))
            {
                message->button(QMessageBox::Yes)->click();
            }
            else
            {
                automationFailed = true;
                message->accept();
            }
        }
    });
    QTimer safetyClose;
    safetyClose.setSingleShot(true);
    QObject::connect(&safetyClose, &QTimer::timeout, &page, [&] {
        if (QWidget* const modal = QApplication::activeModalWidget())
        {
            automationFailed = true;
            modal->close();
        }
    });

    automation.start();
    safetyClose.start(30'000);
    generate->click();
    safetyClose.stop();
    automation.stop();
    if (!dialogAccepted || automationFailed)
    {
        *error = QStringLiteral("The Sub Prep output dialog did not complete cleanly.");
        return false;
    }
    *outputStatus = page.findChild<QLabel*>(
        QStringLiteral("subPrepRouteStatus"))->text();
    return true;
}

QJsonObject captureVariant(
    QApplication& application,
    Resources::ResourceLoader& resourceLoader,
    const QString& outputDirectory,
    const QString& locale,
    const QString& theme,
    QString* error
    )
{
    QLocale::setDefault(QLocale(locale));
    application.setPalette(themePalette(theme));
    const QString resourceRoot = QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const QString stylePath = QDir(resourceRoot).filePath(
        QStringLiteral("assets/styles/%1.qss").arg(theme));
    application.setStyleSheet(readText(stylePath));

    std::unique_ptr<QTranslator> translator;
    if (locale == QStringLiteral("ko_KR"))
    {
        translator = std::make_unique<QTranslator>();
        const QString translationPath = QDir(QStringLiteral(
            CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY)).filePath(
                QStringLiteral("ClassMngr_ko_KR.qm"));
        if (!translator->load(translationPath)
            || !application.installTranslator(translator.get()))
        {
            *error = QStringLiteral("Could not load compiled Korean Sub Prep translations.");
            return {};
        }
    }

    QTemporaryDir temporary;
    if (!temporary.isValid())
    {
        *error = QStringLiteral("Could not create a Sub Prep fixture workspace.");
        return {};
    }
    const QString databasePath = temporary.filePath(QStringLiteral("sub-prep.tps"));
    if (!seedWorkspace(databasePath, QStringLiteral("large_startup.sql"), error))
    {
        return {};
    }

    Platform::WorkspaceStoreGateway gateway;
    const auto opened = gateway.openWorkspace({
        .location = Application::WorkspaceLocation(
            databasePath.toUtf8().toStdString())
    });
    if (!opened)
    {
        *error = QString::fromUtf8(opened.error().message.data(),
            static_cast<qsizetype>(opened.error().message.size()));
        return {};
    }

    Test::MemoryPreferences preferences;
    PlatformProcessMemorySnapshotProvider memoryProvider;
    const ProcessMemorySnapshot before = memoryProvider.snapshot();
    QJsonObject states;
    {
        Ui::PageHost host(resourceLoader, preferences.model);
        Ui::PageDescriptor subPrep{
            .id = *Ui::PageId::fromString(QStringLiteral("sub_prep")),
            .label = QStringLiteral("Sub Prep"),
            .factory = [&gateway, databasePath](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<Ui::SubPrepPage>(
                        context, actionsFor(gateway, databasePath)));
            }
        };
        Ui::PageDescriptor other{
            .id = *Ui::PageId::fromString(QStringLiteral("other")),
            .label = QStringLiteral("Other"),
            .factory = [](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<EmptyPage>(context));
            }
        };
        if (!host.registerPage(std::move(subPrep))
            || !host.registerPage(std::move(other))
            || !host.activatePage(QStringLiteral("sub_prep")))
        {
            *error = QStringLiteral("The Next Sub Prep route could not activate.");
            return {};
        }
        auto* const page = dynamic_cast<Ui::SubPrepPage*>(
            host.pageInstance(QStringLiteral("sub_prep")));
        if (!page)
        {
            *error = QStringLiteral("The Sub Prep route factory returned the wrong page.");
            return {};
        }

        host.resize(Viewport);
        host.show();
        QApplication::processEvents();
        const ProcessMemorySnapshot active = memoryProvider.snapshot();
        const QString key = (locale == QStringLiteral("ko_KR")
            ? QStringLiteral("korean") : QStringLiteral("english"))
            + QLatin1Char('-') + theme;
        const QString selectedPath = QDir(outputDirectory).filePath(
            QStringLiteral("selected-%1.png").arg(key));
        if (!saveCapture(host, selectedPath, error))
        {
            return {};
        }
        const int widgetsSelected = widgetCount(page);
        const auto selectedMetrics = page->visualMetrics();
        if (selectedMetrics.visibleSummaryCount != 96
            || selectedMetrics.summaryModelCount != 1
            || selectedMetrics.selectedDetailPanelCount != 1
            || selectedMetrics.textEditorCount != 6
            || !selectedMetrics.selectedDetailsRetained)
        {
            *error = QStringLiteral("Selected-state Sub Prep structure exceeded its bounds.");
            return {};
        }
        auto* const mode = page->findChild<QComboBox*>(
            QStringLiteral("subPrepScheduleMode"));
        auto* const monday = page->findChild<QCheckBox*>(
            QStringLiteral("subPrepDay_0"));
        if (!mode || !monday)
        {
            *error = QStringLiteral("Sub Prep language controls are missing.");
            return {};
        }
        const QString expectedMode = locale == QStringLiteral("ko_KR")
            ? QStringLiteral("정규 수업 일정") : QStringLiteral("Regular Schedule");
        const QString expectedMonday = locale == QStringLiteral("ko_KR")
            ? QStringLiteral("월요일") : QStringLiteral("Monday");
        if (mode->currentText() != expectedMode
            || monday->text() != expectedMonday)
        {
            *error = QStringLiteral("Sub Prep did not use the requested %1 labels.")
                .arg(locale);
            return {};
        }

        states.insert(QStringLiteral("selected"), QJsonObject{
            {QStringLiteral("image"), QFileInfo(selectedPath).fileName()},
            {QStringLiteral("metrics"), metricsJson(*page, widgetsSelected)},
            {QStringLiteral("modeLabel"), mode->currentText()},
            {QStringLiteral("mondayLabel"), monday->text()}
        });

        if (locale == QStringLiteral("en_US")
            && theme == QStringLiteral("light"))
        {
            const auto classFifty = Next::Domain::ClassId::fromString("50");
            if (!classFifty || !page->selectClassForTesting(*classFifty))
            {
                *error = QStringLiteral("A different grade/level selection could not load.");
                return {};
            }
            const QString changedPath = QDir(outputDirectory).filePath(
                QStringLiteral("changed-selection-en-light.png"));
            if (!saveCapture(host, changedPath, error))
            {
                return {};
            }
            states.insert(QStringLiteral("changedSelection"), QJsonObject{
                {QStringLiteral("image"), QFileInfo(changedPath).fileName()},
                {QStringLiteral("classId"), QStringLiteral("50")},
                {QStringLiteral("metrics"), metricsJson(*page, widgetCount(page))}
            });

            const QString outputRoot = QDir(outputDirectory).filePath(
                QStringLiteral("generated-package"));
            if (!QDir().mkpath(outputRoot))
            {
                *error = QStringLiteral("Could not create the evidence output folder.");
                return {};
            }
            QString outputStatus;
            if (!generateOutput(*page, outputRoot, &outputStatus, error))
            {
                return {};
            }
            if (!outputStatus.contains(QStringLiteral(
                    "Multiple selected classes use the Monday 4:00 PM slot.")))
            {
                *error = QStringLiteral(
                    "The unchanged 96-class fixture did not exercise its expected output conflict: %1")
                    .arg(outputStatus);
                return {};
            }
            if (!QDir(outputRoot).entryList(
                    QDir::Dirs | QDir::NoDotAndDotDot).isEmpty())
            {
                *error = QStringLiteral(
                    "Rejected conflicting Sub Prep output left a package folder.");
                return {};
            }
            const QString outputPath = QDir(outputDirectory).filePath(
                QStringLiteral("output-conflict-en-light.png"));
            if (!saveCapture(host, outputPath, error))
            {
                return {};
            }
            states.insert(QStringLiteral("outputFailure"), QJsonObject{
                {QStringLiteral("image"), QFileInfo(outputPath).fileName()},
                {QStringLiteral("status"), outputStatus},
                {QStringLiteral("packageFolderCreated"), false},
                {QStringLiteral("metrics"), metricsJson(*page, widgetCount(page))}
            });
        }

        const int summaryBeforeEmpty = page->visualMetrics().summaryReadCount;
        const int scheduleBeforeEmpty = page->visualMetrics().scheduleReadCount;
        for (int day = 0; day < 5; ++day)
        {
            auto* const check = page->findChild<QCheckBox*>(
                QStringLiteral("subPrepDay_%1").arg(day));
            if (!check)
            {
                *error = QStringLiteral("A weekday scope control is missing.");
                return {};
            }
            check->setChecked(false);
        }
        if (page->visualMetrics().visibleSummaryCount != 0
            || page->selectedDetails())
        {
            *error = QStringLiteral("Empty scope retained Sub Prep summaries or details.");
            return {};
        }
        const QString emptyPath = QDir(outputDirectory).filePath(
            QStringLiteral("empty-%1.png").arg(key));
        if (!saveCapture(host, emptyPath, error))
        {
            return {};
        }
        const auto emptyMetrics = page->visualMetrics();
        if (emptyMetrics.summaryReadCount != summaryBeforeEmpty + 5
            || emptyMetrics.scheduleReadCount != scheduleBeforeEmpty + 5)
        {
            *error = QStringLiteral("Day changes exceeded one summary read per refresh.");
            return {};
        }
        states.insert(QStringLiteral("empty"), QJsonObject{
            {QStringLiteral("image"), QFileInfo(emptyPath).fileName()},
            {QStringLiteral("metrics"), metricsJson(*page, widgetCount(page))}
        });

        if (!host.activatePage(QStringLiteral("other")))
        {
            *error = QStringLiteral("Sub Prep could not leave its route cleanly.");
            return {};
        }
        const ProcessMemorySnapshot suspended = memoryProvider.snapshot();
        const auto suspendedMetrics = page->visualMetrics();
        if (suspendedMetrics.visibleSummaryCount != 0
            || suspendedMetrics.selectedDetailsRetained)
        {
            *error = QStringLiteral("Sub Prep retained page data after suspension.");
            return {};
        }
        states.insert(QStringLiteral("memory"), QJsonObject{
            {QStringLiteral("beforePage"), memoryJson(before)},
            {QStringLiteral("active"), memoryJson(active)},
            {QStringLiteral("suspended"), memoryJson(suspended)},
            {QStringLiteral("scope"), QStringLiteral(
                "Process-wide samples; not per-page allocations and no Phase 9 threshold")}
        });
        states.insert(QStringLiteral("suspendedMetrics"), metricsJson(
            *page, widgetCount(page)));
    }

    static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
    if (translator)
    {
        application.removeTranslator(translator.get());
    }
    return states;
}

QJsonObject captureSuccessfulOutput(
    QApplication& application,
    Resources::ResourceLoader& resourceLoader,
    const QString& outputDirectory,
    QString* error
    )
{
    QLocale::setDefault(QLocale(QStringLiteral("en_US")));
    application.setPalette(themePalette(QStringLiteral("light")));
    QTemporaryDir temporary;
    if (!temporary.isValid())
    {
        *error = QStringLiteral("Could not create the successful-output workspace.");
        return {};
    }
    const QString databasePath = temporary.filePath(QStringLiteral("sub-prep.tps"));
    if (!seedWorkspace(databasePath,
            QStringLiteral("representative_startup.sql"), error))
    {
        return {};
    }

    Platform::WorkspaceStoreGateway gateway;
    const auto opened = gateway.openWorkspace({
        .location = Application::WorkspaceLocation(
            databasePath.toUtf8().toStdString())
    });
    if (!opened)
    {
        *error = QString::fromUtf8(opened.error().message.data(),
            static_cast<qsizetype>(opened.error().message.size()));
        return {};
    }

    Test::MemoryPreferences preferences;
    QJsonObject result;
    {
        Ui::PageHost host(resourceLoader, preferences.model);
        Ui::PageDescriptor subPrep{
            .id = *Ui::PageId::fromString(QStringLiteral("sub_prep")),
            .label = QStringLiteral("Sub Prep"),
            .factory = [&gateway, databasePath](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<Ui::SubPrepPage>(
                        context, actionsFor(gateway, databasePath)));
            }
        };
        if (!host.registerPage(std::move(subPrep))
            || !host.activatePage(QStringLiteral("sub_prep")))
        {
            *error = QStringLiteral("The compatible Sub Prep route could not activate.");
            return {};
        }
        auto* const page = dynamic_cast<Ui::SubPrepPage*>(
            host.pageInstance(QStringLiteral("sub_prep")));
        if (!page || page->visualMetrics().visibleSummaryCount == 0)
        {
            *error = QStringLiteral("The compatible Sub Prep route has no visible classes.");
            return {};
        }

        const QString outputRoot = QDir(outputDirectory).filePath(
            QStringLiteral("generated-package-success"));
        if (!QDir().mkpath(outputRoot))
        {
            *error = QStringLiteral("Could not create the successful package folder.");
            return {};
        }
        QString status;
        if (!generateOutput(*page, outputRoot, &status, error))
        {
            return {};
        }
        if (!status.contains(QStringLiteral("The package was created at:")))
        {
            *error = QStringLiteral("Compatible Sub Prep output failed: %1")
                .arg(status);
            return {};
        }

        const QStringList folders = QDir(outputRoot).entryList(
            QDir::Dirs | QDir::NoDotAndDotDot);
        if (folders.size() != 1)
        {
            *error = QStringLiteral("Successful Sub Prep output did not create one package folder.");
            return {};
        }
        const QString packagePath = QDir(outputRoot).filePath(folders.front());
        const QStringList pdfs = QDir(packagePath).entryList(
            {QStringLiteral("*.pdf")}, QDir::Files);
        if (pdfs.size() < 2)
        {
            *error = QStringLiteral("Successful Sub Prep output did not create both PDFs.");
            return {};
        }
        QJsonArray files;
        for (const QString& pdf : pdfs)
        {
            const QFileInfo info(QDir(packagePath).filePath(pdf));
            if (info.size() <= 0)
            {
                *error = QStringLiteral("Generated Sub Prep PDF is empty: %1")
                    .arg(info.fileName());
                return {};
            }
            files.append(QJsonObject{
                {QStringLiteral("name"), info.fileName()},
                {QStringLiteral("bytes"), info.size()}
            });
        }
        result = QJsonObject{
            {QStringLiteral("fixture"), QStringLiteral(
                "tests/fixtures/workspaces/representative_startup.sql")},
            {QStringLiteral("visibleSummaryCount"),
             page->visualMetrics().visibleSummaryCount},
            {QStringLiteral("status"), status},
            {QStringLiteral("outputFolder"), QStringLiteral(
                "generated-package-success/%1").arg(folders.front())},
            {QStringLiteral("files"), files}
        };
    }
    static_cast<void>(gateway.closeWorkspace({.session = opened.value()}));
    return result;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    if (!QFile::exists(QStringLiteral(":/assets/icons/combo_arrow_light.svg"))
        || !QFile::exists(QStringLiteral(":/assets/icons/combo_arrow_dark.svg")))
    {
        qCritical("Next Sub Prep mode combo arrow resources are unavailable.");
        return 1;
    }
    if (argc != 2)
    {
        qCritical("Usage: ClassMngrNextSubPrepVisualCapture OUTPUT_DIR");
        return 2;
    }
    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create the C44 capture directory.");
        return 2;
    }
    const auto loadedCatalog = Resources::ResourceCatalog::load(
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT));
    if (!loadedCatalog)
    {
        qCritical().noquote() << "Could not load Next resource catalog:"
                              << loadedCatalog.error().message;
        return 1;
    }
    Resources::ResourceLoader resourceLoader(std::move(*loadedCatalog));
    const QString legacyRoot = QStringLiteral(
        CLASSMNGR_SOURCE_DIR "/docs/qt-rewrite/visual-baseline/release/large-sub-prep-visual-states");
    const std::array<QString, 2> locales{
        QStringLiteral("en_US"), QStringLiteral("ko_KR")
    };
    const std::array<QString, 2> themes{
        QStringLiteral("light"), QStringLiteral("dark")
    };
    QJsonArray variants;
    for (const QString& locale : locales)
    {
        for (const QString& theme : themes)
        {
            QString error;
            const QJsonObject states = captureVariant(
                application, resourceLoader, outputDirectory,
                locale, theme, &error);
            if (states.isEmpty())
            {
                qCritical().noquote() << error;
                return 1;
            }
            const QString language = locale == QStringLiteral("ko_KR")
                ? QStringLiteral("korean") : QStringLiteral("english");
            const QString key = language + QLatin1Char('-') + theme;
            variants.append(QJsonObject{
                {QStringLiteral("locale"), locale},
                {QStringLiteral("theme"), theme},
                {QStringLiteral("fixture"), QStringLiteral(
                    "tests/fixtures/workspaces/large_startup.sql (96 classes, 8 slots each)")},
                {QStringLiteral("legacySelectedReference"),
                 QStringLiteral("%1/%2/sub-prep-selected.png").arg(legacyRoot, key)},
                {QStringLiteral("states"), states}
            });
        }
    }
    QString successfulOutputError;
    const QJsonObject successfulOutput = captureSuccessfulOutput(
        application, resourceLoader, outputDirectory, &successfulOutputError);
    if (successfulOutput.isEmpty())
    {
        qCritical().noquote() << successfulOutputError;
        return 1;
    }
    const QJsonObject manifest{
        {QStringLiteral("taskId"), QStringLiteral("phase7_c44_subprep_implementation_01")},
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("route"), QStringLiteral("Next Sub Prep")},
        {QStringLiteral("viewport"), QJsonObject{
            {QStringLiteral("width"), Viewport.width()},
            {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("style"), QStringLiteral("Fusion")},
        {QStringLiteral("platform"), QStringLiteral("offscreen")},
        {QStringLiteral("memoryThreshold"), QStringLiteral(
            "Not evaluated; Phase 9 packaged Release owns the 250 MiB gate")},
        {QStringLiteral("successfulOutput"), successfulOutput},
        {QStringLiteral("variants"), variants}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qCritical("Could not write the C44 capture manifest.");
        return 1;
    }
    const QByteArray json = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
    if (manifestFile.write(json) != json.size())
    {
        qCritical("Could not finish the C44 capture manifest.");
        return 1;
    }
    return 0;
}
