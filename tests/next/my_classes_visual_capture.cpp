#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/fontmanager.h"
#include "core/process_memory_snapshot.h"
#include "data/database/database_session.h"
#include "data/database/workspace_store.h"
#include "features/my_info/ui/my_classes_page.h"
#include "ui/shared/widgets/navigation_tab_widget.h"
#include "next/application/my_classes_page_read_query.h"
#include "next/platform/workspace_store_my_classes_page_read_port.h"
#include "next/platform/qt_font_family_availability_adapter.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/font_size_presenter.h"
#include "next/ui/my_classes_page.h"
#include "next/ui/page_host.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontInfo>
#include <QFrame>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QPalette>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QTranslator>
#include <QListView>

#include <memory>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr;
using namespace ClassMngr::Next;

namespace
{

constexpr QSize Viewport{1024, 768};

QString utf8(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString readText(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly | QIODevice::Text)
        ? QString::fromUtf8(file.readAll())
        : QString{};
}

QJsonArray fontFamilies(const QFont& font)
{
    QJsonArray result;
    for (const QString& family : font.families())
    {
        result.append(family);
    }
    return result;
}

QJsonObject fontJson(const QFont& font)
{
    const QFontInfo resolved(font);
    return {
        {QStringLiteral("families"), fontFamilies(font)},
        {QStringLiteral("family"), font.family()},
        {QStringLiteral("resolvedFamily"), resolved.family()},
        {QStringLiteral("pointSize"), font.pointSize()},
        {QStringLiteral("pointSizeF"), font.pointSizeF()},
        {QStringLiteral("weight"), font.weight()},
        {QStringLiteral("italic"), font.italic()}
    };
}

QString sha256File(const QString& path, QString* error = nullptr)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error)
        {
            *error = QStringLiteral("Could not read file for SHA-256: %1")
                .arg(path);
        }
        return {};
    }
    const QByteArray hash = QCryptographicHash::hash(
        file.readAll(), QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(hash);
}

std::optional<QJsonObject> registeredFontJson(
    const Resources::ResourceLoader& loader,
    const QString& resourceId,
    const QString& registrationPath,
    const Resources::RegisteredFont& registration,
    QString* error)
{
    const auto id = Resources::ResourceId::fromString(resourceId);
    if (!id)
    {
        if (error)
        {
            *error = QStringLiteral("Invalid font resource ID: %1")
                .arg(resourceId);
        }
        return std::nullopt;
    }
    const Resources::ResourceDescriptor* const descriptor =
        loader.catalog().find(*id);
    if (!descriptor)
    {
        if (error)
        {
            *error = QStringLiteral("Font is missing from the resource catalog: %1")
                .arg(resourceId);
        }
        return std::nullopt;
    }
    const QString packagePath = QDir(loader.catalog().resourceRoot()).filePath(
        descriptor->packageRelativePath());
    QFile packageFile(packagePath);
    if (!packageFile.open(QIODevice::ReadOnly))
    {
        if (error)
        {
            *error = QStringLiteral("Could not read packaged font: %1")
                .arg(packagePath);
        }
        return std::nullopt;
    }
    const QByteArray packageBytes = packageFile.readAll();
    const QByteArray actualSha256 = QCryptographicHash::hash(
        packageBytes, QCryptographicHash::Sha256).toHex();
    if (packageBytes.isEmpty()
        || (descriptor->expectedBytes()
            && packageBytes.size() != *descriptor->expectedBytes())
        || (descriptor->expectedSha256()
            && actualSha256 != descriptor->expectedSha256()->toHex()))
    {
        if (error)
        {
            *error = QStringLiteral("Packaged font does not match catalog metadata: %1")
                .arg(resourceId);
        }
        return std::nullopt;
    }

    QJsonArray families;
    for (const QString& family : registration.familyNames())
    {
        families.append(family);
    }
    if (families.isEmpty())
    {
        if (error)
        {
            *error = QStringLiteral("Registered font has no family name: %1")
                .arg(resourceId);
        }
        return std::nullopt;
    }
    return QJsonObject{
        {QStringLiteral("resourceId"), resourceId},
        {QStringLiteral("registrationPath"), registrationPath},
        {QStringLiteral("packageFile"), QFileInfo(packagePath).canonicalFilePath()},
        {QStringLiteral("packageRelativePath"), descriptor->packageRelativePath()},
        {QStringLiteral("packageFileBytes"), packageBytes.size()},
        {QStringLiteral("packageFileSha256"), QString::fromLatin1(actualSha256)},
        {QStringLiteral("catalogExpectedSha256"), descriptor->expectedSha256()
            ? QJsonValue(QString::fromLatin1(descriptor->expectedSha256()->toHex()))
            : QJsonValue{}},
        {QStringLiteral("registeredFamilyNames"), families},
        {QStringLiteral("registeredSourceFile"), registration.sourceFilePath()}
    };
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

int widgetCount(const QWidget* root)
{
    return root ? root->findChildren<QWidget*>().size() + 1 : 0;
}

int classShellCount(const QWidget* root)
{
    if (!root)
    {
        return 0;
    }
    int count = 0;
    for (QWidget* const widget : root->findChildren<QWidget*>())
    {
        if (widget->property("class_id").isValid())
        {
            ++count;
        }
    }
    return count;
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

bool copyFixture(
    const QString& source,
    const QString& destination,
    QString* error
    )
{
    QFile::remove(destination);
    if (QFile::copy(source, destination))
    {
        return true;
    }
    if (error)
    {
        *error = QStringLiteral("The My Classes fixture could not be copied: %1")
            .arg(source);
    }
    return false;
}

bool createEmptyFixture(const QString& destination, QString* error)
{
    QFile::remove(destination);
    ApplicationServices creator;
    const RepositoryStatus opened = creator.openDatabase(destination);
    if (!opened)
    {
        if (error)
        {
            *error = toLegacyDiagnostic(opened.error());
        }
        return false;
    }
    creator.closeDatabase();
    if (!QFileInfo::exists(destination) || QFileInfo(destination).size() <= 0)
    {
        if (error)
        {
            *error = QStringLiteral("The empty My Classes workspace was not created.");
        }
        return false;
    }
    return true;
}

QJsonObject labelState(QWidget* root, QLabel* label)
{
    return label
        ? QJsonObject{
            {QStringLiteral("text"), label->text()},
            {QStringLiteral("visible"), label->isVisible()},
            {QStringLiteral("geometry"), QJsonObject{
                {QStringLiteral("x"), label->mapTo(root, QPoint(0, 0)).x()},
                {QStringLiteral("y"), label->mapTo(root, QPoint(0, 0)).y()},
                {QStringLiteral("width"), label->width()},
                {QStringLiteral("height"), label->height()}}}}
        : QJsonObject{{QStringLiteral("found"), false}};
}

QLabel* findLabel(QWidget* root, const QString& text)
{
    for (QLabel* const label : root->findChildren<QLabel*>())
    {
        if (label->text() == text)
        {
            return label;
        }
    }
    return nullptr;
}

bool captureLegacy(
    const QString& databasePath,
    const QString& outputPath,
    const QString& locale,
    const bool injectClassListReadError,
    const QString& dialogOutputPath,
    QJsonObject* state,
    QString* error
    )
{
    FontManager::setSizeOffset(0);
    FontManager::applyGlobalFont(*qApp, locale);
    PlatformProcessMemorySnapshotProvider memoryProvider;
    const ProcessMemorySnapshot before = memoryProvider.snapshot();
    ApplicationServices services;
    const RepositoryStatus opened = services.openDatabase(databasePath);
    if (!opened)
    {
        if (error)
        {
            *error = toLegacyDiagnostic(opened.error());
        }
        return false;
    }

    MyClassesPage page(&services);
    page.resize(Viewport);
    page.refresh();
    page.show();
    QApplication::processEvents();
    if (QWidget* const focused = QApplication::focusWidget())
    {
        focused->clearFocus();
    }
    QApplication::processEvents();

    QJsonObject errorStatus;
    QImage dialogImage;
    if (injectClassListReadError)
    {
        if (!services.databaseSession())
        {
            if (error)
            {
                *error = QStringLiteral("The Legacy My Classes database is unavailable.");
            }
            return false;
        }
        QSqlDatabase database = services.databaseSession()->database();
        QSqlQuery disableForeignKeys(database);
        QSqlQuery dropClasses(database);
        if (!disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF"))
            || !dropClasses.exec(QStringLiteral("DROP TABLE classes")))
        {
            if (error)
            {
                *error = disableForeignKeys.lastError().text().isEmpty()
                    ? dropClasses.lastError().text()
                    : disableForeignKeys.lastError().text();
            }
            return false;
        }
        QString dialogTitle;
        QString dialogText;
        QString dialogDetails;
        bool dialogCaptured = false;
        QTimer::singleShot(0, &page,
            [&dialogImage, &dialogTitle, &dialogText, &dialogDetails,
             &dialogCaptured, &dialogOutputPath]
            {
                auto* const warning = qobject_cast<QMessageBox*>(
                    QApplication::activeModalWidget());
                if (!warning)
                {
                    return;
                }
                dialogTitle = warning->windowTitle();
                dialogText = warning->text();
                dialogDetails = warning->detailedText();
                dialogImage = warning->grab().toImage();
                dialogCaptured = !dialogImage.isNull()
                    && dialogImage.save(dialogOutputPath, "PNG");
                warning->accept();
            });
        page.refresh();
        if (!dialogCaptured)
        {
            if (error)
            {
                *error = QStringLiteral("The Legacy class-list error dialog was not captured.");
            }
            return false;
        }
        errorStatus = {
            {QStringLiteral("kind"), QStringLiteral("modal warning")},
            {QStringLiteral("title"), dialogTitle},
            {QStringLiteral("text"), dialogText},
            {QStringLiteral("details"), dialogDetails},
            {QStringLiteral("dialogWidth"), dialogImage.width()},
            {QStringLiteral("dialogHeight"), dialogImage.height()},
            {QStringLiteral("image"), QFileInfo(dialogOutputPath).fileName()}
        };
        QApplication::processEvents();
    }

    const ProcessMemorySnapshot active = memoryProvider.snapshot();
    const QImage image = page.grab().toImage();
    if (image.size() != Viewport || !image.save(outputPath, "PNG"))
    {
        if (error)
        {
            *error = QStringLiteral("The legacy My Classes capture could not be saved.");
        }
        return false;
    }
    const auto* const tabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("myInfoClassTabs"));
    const auto* const gradeTabs = page.findChild<NavigationTabWidget*>(
        QStringLiteral("myInfoGradeTabs"));
    const QString emptyStateText = QCoreApplication::translate(
        "MyClassesPage", "No classes available.");
    QLabel* const emptyState = findLabel(&page, emptyStateText);
    const MyClassesPageRuntimeMetrics metrics = page.runtimeMetrics();
    const int classShells = classShellCount(&page);
    const int widgets = widgetCount(&page);
    page.releaseFeatureResources();
    QApplication::processEvents();
    const ProcessMemorySnapshot suspended = memoryProvider.snapshot();
    services.closeDatabase();
    const QString databaseSha256 = sha256File(databasePath, error);
    if (databaseSha256.isEmpty())
    {
        return false;
    }
    *state = {
        {QStringLiteral("page"), QStringLiteral("legacy")},
        {QStringLiteral("image"), QFileInfo(outputPath).fileName()},
        {QStringLiteral("databaseSha256"), databaseSha256},
        {QStringLiteral("width"), image.width()},
        {QStringLiteral("height"), image.height()},
        {QStringLiteral("summaryReadErrorInjected"), injectClassListReadError},
        {QStringLiteral("errorStatus"), errorStatus},
        {QStringLiteral("emptyStatus"), labelState(&page, emptyState)},
        {QStringLiteral("fontPreference"), QStringLiteral("Normal")},
        {QStringLiteral("applicationFont"), fontJson(QApplication::font())},
        {QStringLiteral("pageFont"), fontJson(page.font())},
        {QStringLiteral("summaryListReadCount"), metrics.classSummaryListQueryCount},
        {QStringLiteral("gradeTabCount"), gradeTabs ? gradeTabs->count() : 0},
        {QStringLiteral("visibleClassTabCount"), tabs ? tabs->count() : 0},
        {QStringLiteral("classWidgetShellCount"), classShells},
        {QStringLiteral("widgetCountWhileActive"), widgets},
        {QStringLiteral("widgetCountAfterFeatureRelease"), widgetCount(&page)},
        {QStringLiteral("memoryProcessWide"), QJsonObject{
            {QStringLiteral("before"), memoryJson(before)},
            {QStringLiteral("active"), memoryJson(active)},
            {QStringLiteral("suspended"), memoryJson(suspended)}
        }}
    };
    return true;
}

bool captureNext(
    const QString& databasePath,
    Resources::ResourceLoader& loader,
    Application::PreferencesModel& preferences,
    const QString& locale,
    const QString& outputPath,
    const bool injectSummaryReadError,
    const bool expectEmptyState,
    QJsonObject* state,
    QString* error
    )
{
    const auto startupScope = Resources::ResourceScope::create(
        Resources::ResourceScopeKind::Startup,
        QStringLiteral("application-bootstrap"));
    if (!startupScope)
    {
        if (error)
        {
            *error = startupScope.error().message;
        }
        return false;
    }

    const std::array<QString, 6> interResources{
        QStringLiteral("resource:assets/fonts/Inter-Regular.ttf"),
        QStringLiteral("resource:assets/fonts/Inter-Medium.ttf"),
        QStringLiteral("resource:assets/fonts/Inter-SemiBold.ttf"),
        QStringLiteral("resource:assets/fonts/Inter-Bold.ttf"),
        QStringLiteral("resource:assets/fonts/Inter-BoldItalic.ttf"),
        QStringLiteral("resource:assets/fonts/Inter-Black.ttf")
    };
    std::vector<Resources::ScopedResource<Resources::RegisteredFont>>
        interFontRegistrations;
    interFontRegistrations.reserve(interResources.size());
    QString interFamily;
    QJsonArray fontResources;
    for (const QString& resourceId : interResources)
    {
        const auto id = Resources::ResourceId::fromString(resourceId);
        if (!id)
        {
            if (error)
            {
                *error = QStringLiteral("Invalid Inter font resource ID: %1")
                    .arg(resourceId);
            }
            return false;
        }
        const auto request = Resources::ResourceRequest::create(
            *id, *startupScope);
        if (!request)
        {
            if (error)
            {
                *error = request.error().message;
            }
            return false;
        }
        auto registration = loader.loadFileBackedFont(*request);
        if (!registration)
        {
            if (error)
            {
                *error = registration.error().message;
            }
            return false;
        }
        const auto identity = registeredFontJson(
            loader, resourceId,
            QStringLiteral("ResourceLoader::loadFileBackedFont"),
            registration->get(), error);
        if (!identity)
        {
            return false;
        }
        if (interFamily.isEmpty())
        {
            interFamily = registration->get().familyNames().value(0);
        }
        fontResources.append(*identity);
        interFontRegistrations.emplace_back(std::move(*registration));
    }

    const QString pretendardResource = QStringLiteral(
        "resource:assets/fonts/PretendardVariable.ttf");
    const auto pretendardId = Resources::ResourceId::fromString(pretendardResource);
    if (!pretendardId)
    {
        if (error)
        {
            *error = QStringLiteral("Invalid Pretendard font resource ID");
        }
        return false;
    }
    const auto pretendardRequest = Resources::ResourceRequest::create(
        *pretendardId, *startupScope);
    if (!pretendardRequest)
    {
        if (error)
        {
            *error = pretendardRequest.error().message;
        }
        return false;
    }
    auto pretendardRegistration = loader.loadFont(*pretendardRequest);
    if (!pretendardRegistration)
    {
        if (error)
        {
            *error = pretendardRegistration.error().message;
        }
        return false;
    }
    const auto pretendardIdentity = registeredFontJson(
        loader, pretendardResource, QStringLiteral("ResourceLoader::loadFont"),
        pretendardRegistration->get(), error);
    if (!pretendardIdentity)
    {
        return false;
    }
    fontResources.append(*pretendardIdentity);
    const QString pretendardFamily =
        pretendardRegistration->get().familyNames().value(0);
    Platform::QtFontFamilyAvailabilityAdapter fontPresentation;
    Ui::FontSizePresenter fontSizePresenter(*qApp, fontPresentation);
    fontSizePresenter.applyStartupPreference(
        Application::FontSize::Normal, locale, interFamily, pretendardFamily);

    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        if (error)
        {
            *error = toLegacyDiagnostic(opened.error());
        }
        return false;
    }

    PlatformProcessMemorySnapshotProvider memoryProvider;
    const ProcessMemorySnapshot before = memoryProvider.snapshot();
    Ui::PageHost host(loader, preferences);
    Ui::PageDescriptor myClasses{
        .id = *Ui::PageId::fromString(QStringLiteral("my_classes")),
        .label = QStringLiteral("My Classes"),
        .factory = [&store, injectSummaryReadError](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            Ui::MyClassesPageActions actions;
            actions.workspaceIdentity = [] { return QStringLiteral("large-fixture"); };
            actions.readSummaries = [&store, injectSummaryReadError] {
                if (injectSummaryReadError)
                {
                    return Application::MyClassesPageSummaryResult::failure({
                        .code = Domain::ErrorCode::Technical,
                        .message = "summary read failed",
                        .recoverable = true
                    });
                }
                Platform::WorkspaceStoreMyClassesPageReadPort port(store);
                return Application::MyClassesPageReadQuery(port).readSummaries();
            };
            actions.readSelectedDetails = [&store](const Domain::ClassId& id) {
                Platform::WorkspaceStoreMyClassesPageReadPort port(store);
                return Application::MyClassesPageReadQuery(port)
                    .readSelectedDetails(id);
            };
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::MyClassesPage>(context, std::move(actions)));
        }
    };
    Ui::PageDescriptor other{
        .id = *Ui::PageId::fromString(QStringLiteral("other")),
        .label = QStringLiteral("Other"),
        .factory = [](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(std::make_unique<Ui::Page>(context));
        }
    };
    if (!host.registerPage(std::move(myClasses))
        || !host.registerPage(std::move(other))
        || !host.activatePage(QStringLiteral("my_classes")))
    {
        if (error)
        {
            *error = QStringLiteral("The Next My Classes page could not activate.");
        }
        return false;
    }
    auto* const page = dynamic_cast<Ui::MyClassesPage*>(
        host.pageInstance(QStringLiteral("my_classes")));
    if (!page)
    {
        if (error)
        {
            *error = QStringLiteral("The Next page factory returned the wrong page.");
        }
        return false;
    }

    host.resize(Viewport);
    host.show();
    QApplication::processEvents();
    if (QWidget* const focused = QApplication::focusWidget())
    {
        focused->clearFocus();
    }
    QApplication::processEvents();
    QLabel* const loadState = page->findChild<QLabel*>(
        QStringLiteral("myClassesLoadState"));
    if (expectEmptyState)
    {
        const QString expected = QCoreApplication::translate(
            "NextMyClassesPage", "No classes available.");
        if (!loadState || !loadState->isVisible() || loadState->text() != expected)
        {
            if (error)
            {
                *error = QStringLiteral("The Next empty-workspace message is not visible.");
            }
            return false;
        }
    }
    if (injectSummaryReadError
        && (!loadState || !loadState->isVisible()
            || loadState->text() != QStringLiteral("summary read failed")))
    {
        if (error)
        {
            *error = QStringLiteral("The Next summary-read error status is not visible.");
        }
        return false;
    }
    const QJsonObject loadStateAtCapture = labelState(page, loadState);
    const ProcessMemorySnapshot active = memoryProvider.snapshot();
    const QImage image = host.grab().toImage();
    if (image.size() != Viewport || !image.save(outputPath, "PNG"))
    {
        if (error)
        {
            *error = QStringLiteral("The Next My Classes capture could not be saved.");
        }
        return false;
    }

    const int modelCount = page->summaryModel()
        && page->classList()->model() == page->summaryModel() ? 1 : 0;
    const int classListViewCount = page->findChildren<QListView*>(
        QStringLiteral("myClassesSummaryList")).size();
    const int gradeTabCount = page->gradeTabs()->count();
    const int activeGradeSummaryCount = page->classList()->model()->rowCount(
        page->classList()->rootIndex());
    const int classCardCount = page->findChildren<QFrame*>(
        QStringLiteral("myClassesClassCard")).size();
    const int summaryCount = page->summaryModel()->summaryCount();
    const int detailReadClassCount = store.classRepository()->readMetrics()
        .getClassByIdCallCount;
    const int detailReadInfoClassCount = store.classInfoRepository()
        ->myClassesClassInformationBatchReadMetrics().requestedClassCount;
    const int detailReadRosterClassCount = store.rosterRepository()
        ->myClassesStudentCountBatchReadMetrics().requestedClassCount;
    const int widgets = widgetCount(page);

    static_cast<void>(host.activatePage(QStringLiteral("other")));
    QApplication::processEvents();
    const bool released = page->detailsReleased()
        && page->summaryModel()->summaryCount() == 0;
    const ProcessMemorySnapshot suspended = memoryProvider.snapshot();
    *state = {
        {QStringLiteral("page"), QStringLiteral("next")},
        {QStringLiteral("image"), QFileInfo(outputPath).fileName()},
        {QStringLiteral("width"), image.width()},
        {QStringLiteral("height"), image.height()},
        {QStringLiteral("summaryReadErrorInjected"), injectSummaryReadError},
        {QStringLiteral("loadState"), loadStateAtCapture},
        {QStringLiteral("fontPreference"), QStringLiteral("Normal")},
        {QStringLiteral("applicationFont"), fontJson(QApplication::font())},
        {QStringLiteral("pageFont"), fontJson(page->font())},
        {QStringLiteral("registeredFonts"), fontResources},
        {QStringLiteral("summaryModelCount"), modelCount},
        {QStringLiteral("summaryRowCountWhileActive"), summaryCount},
        {QStringLiteral("gradeTabCount"), gradeTabCount},
        {QStringLiteral("classListViewCount"), classListViewCount},
        {QStringLiteral("activeGradeSummaryCount"), activeGradeSummaryCount},
        {QStringLiteral("selectedDetailPanelCount"), classCardCount},
        {QStringLiteral("selectedDetailReadClassCount"), detailReadClassCount},
        {QStringLiteral("selectedDetailInfoReadClassCount"), detailReadInfoClassCount},
        {QStringLiteral("selectedDetailRosterCountReadClassCount"), detailReadRosterClassCount},
        {QStringLiteral("widgetCountWhileActive"), widgets},
        {QStringLiteral("releasedAfterSuspension"), released},
        {QStringLiteral("summaryRowCountAfterSuspension"),
         page->summaryModel()->summaryCount()},
        {QStringLiteral("memoryProcessWide"), QJsonObject{
            {QStringLiteral("before"), memoryJson(before)},
            {QStringLiteral("active"), memoryJson(active)},
            {QStringLiteral("suspended"), memoryJson(suspended)}
        }}
    };
    return released;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    FontManager::loadFonts();
    FontManager::setSizeOffset(0);
    QString scenario = QStringLiteral("populated");
    QString outputDirectory;
    if (argc == 2)
    {
        outputDirectory = QString::fromLocal8Bit(argv[1]);
    }
    else if (argc == 3)
    {
        const QString option = QString::fromLocal8Bit(argv[1]);
        if (option == QStringLiteral("--empty"))
        {
            scenario = QStringLiteral("empty");
        }
        else if (option == QStringLiteral("--summary-error"))
        {
            scenario = QStringLiteral("summary-error");
        }
        else
        {
            qCritical("Unknown My Classes visual capture scenario");
            return 2;
        }
        outputDirectory = QString::fromLocal8Bit(argv[2]);
    }
    else
    {
        qCritical("Usage: ClassMngrNextMyClassesVisualCapture OUTPUT_DIR | "
                  "--empty OUTPUT_DIR | --summary-error OUTPUT_DIR");
        return 2;
    }
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create capture output directory");
        return 2;
    }

    const QString resourceRoot = QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const QString sourceFixturePath = QStringLiteral(
        CLASSMNGR_NEXT_MY_CLASSES_LARGE_FIXTURE);
    QString hashError;
    const QString sourceFixtureSha256 = sha256File(sourceFixturePath, &hashError);
    if (sourceFixtureSha256.isEmpty())
    {
        qCritical().noquote() << hashError;
        return 1;
    }
    QString fixturePath = sourceFixturePath;
    if (scenario == QStringLiteral("empty"))
    {
        const QString fixturesDirectory = QDir(outputDirectory)
            .filePath(QStringLiteral("fixtures"));
        if (!QDir().mkpath(fixturesDirectory))
        {
            qCritical("Could not create synthetic fixture directory");
            return 1;
        }
        fixturePath = QDir(fixturesDirectory)
            .filePath(QStringLiteral("empty-workspace.tps"));
        QString fixtureError;
        if (!createEmptyFixture(fixturePath, &fixtureError))
        {
            qCritical().noquote() << fixtureError;
            return 1;
        }
    }
    const QString fixtureSha256 = sha256File(fixturePath, &hashError);
    if (fixtureSha256.isEmpty())
    {
        qCritical().noquote() << hashError;
        return 1;
    }
    const QString sourceRoot = QStringLiteral(CLASSMNGR_SOURCE_DIR);
    Platform::QtFontFamilyAvailabilityAdapter fontPresentation;
    QJsonArray sourceFiles;
    for (const QString& sourcePath : {
             QStringLiteral("tests/next/my_classes_visual_capture.cpp"),
             QStringLiteral("cmake/tests/next.cmake")
         })
    {
        const QString absolutePath = QDir(sourceRoot).filePath(sourcePath);
        const QString sha256 = sha256File(absolutePath, &hashError);
        if (sha256.isEmpty())
        {
            qCritical().noquote() << hashError;
            return 1;
        }
        sourceFiles.append(QJsonObject{
            {QStringLiteral("path"), QFileInfo(absolutePath).canonicalFilePath()},
            {QStringLiteral("sha256"), sha256}
        });
    }
    QJsonArray fontAssets;
    const QStringList fontAssetNames{
        QStringLiteral("Inter-Regular.ttf"),
        QStringLiteral("Inter-Medium.ttf"),
        QStringLiteral("Inter-SemiBold.ttf"),
        QStringLiteral("Inter-Bold.ttf"),
        QStringLiteral("Inter-BoldItalic.ttf"),
        QStringLiteral("Inter-Black.ttf"),
        QStringLiteral("PretendardVariable.ttf")
    };
    for (const QString& fontName : fontAssetNames)
    {
        const QString fontPath = QDir(sourceRoot).filePath(
            QStringLiteral("resources/assets/fonts/") + fontName);
        const QString sha256 = sha256File(fontPath, &hashError);
        if (sha256.isEmpty())
        {
            qCritical().noquote() << hashError;
            return 1;
        }
        fontAssets.append(QJsonObject{
            {QStringLiteral("file"), QFileInfo(fontPath).canonicalFilePath()},
            {QStringLiteral("bytes"), QFileInfo(fontPath).size()},
            {QStringLiteral("sha256"), sha256}
        });
    }
    const auto loadedCatalog = Resources::ResourceCatalog::load(
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG), resourceRoot);
    if (!loadedCatalog)
    {
        qCritical().noquote() << "Could not load Next resource catalog:"
                              << loadedCatalog.error().message;
        return 1;
    }
    Resources::ResourceLoader loader(std::move(*loadedCatalog));
    Test::MemoryPreferences preferences;
    const QString translations = QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY);
    const QString styles = QDir(resourceRoot).filePath(QStringLiteral("assets/styles"));
    const QStringList locales{QStringLiteral("en_US"), QStringLiteral("ko_KR")};
    const QStringList themes{QStringLiteral("light"), QStringLiteral("dark")};
    QJsonArray captures;

    for (const QString& locale : locales)
    {
        for (const QString& theme : themes)
        {
            QLocale::setDefault(QLocale(locale));
            application.setPalette(themePalette(theme));
            application.setStyleSheet(readText(
                QDir(styles).filePath(theme + QStringLiteral(".qss"))));
            std::unique_ptr<QTranslator> translator;
            if (locale == QStringLiteral("ko_KR"))
            {
                translator = std::make_unique<QTranslator>();
                if (!translator->load(QDir(translations).filePath(
                        QStringLiteral("ClassMngr_ko_KR.qm")))
                    || !application.installTranslator(translator.get()))
                {
                    qCritical("Could not load the compiled Korean translation");
                    return 1;
                }
            }

            QTemporaryDir temporary;
            if (!temporary.isValid())
            {
                qCritical("Could not create a temporary fixture directory");
                return 1;
            }
            QString error;
            const QString legacyDb = temporary.filePath(QStringLiteral("legacy.tps"));
            const QString nextDb = temporary.filePath(QStringLiteral("next.tps"));
            if (!copyFixture(fixturePath, legacyDb, &error)
                || !copyFixture(fixturePath, nextDb, &error))
            {
                qCritical().noquote() << error;
                return 1;
            }
            const QString key = locale + QLatin1Char('_') + theme;
            const bool injectSummaryError =
                scenario == QStringLiteral("summary-error");
            const bool expectEmptyState = scenario == QStringLiteral("empty");
            const QString imagePrefix = expectEmptyState
                ? QStringLiteral("empty_")
                : injectSummaryError ? QStringLiteral("summary_error_")
                                     : QString{};
            QJsonObject legacyState;
            QJsonObject nextState;
            const QString legacyImage = QDir(outputDirectory).filePath(
                QStringLiteral("legacy_%1%2.png").arg(imagePrefix, key));
            const QString nextImage = QDir(outputDirectory).filePath(
                QStringLiteral("next_%1%2.png").arg(imagePrefix, key));
            const QString legacyDialogImage = injectSummaryError
                ? QDir(outputDirectory).filePath(
                    QStringLiteral("legacy_summary_error_dialog_%1.png").arg(key))
                : QString{};
            if (!captureLegacy(legacyDb, legacyImage, locale,
                               injectSummaryError, legacyDialogImage,
                               &legacyState, &error))
            {
                qCritical().noquote() << error;
                return 1;
            }
            if (!captureNext(nextDb, loader, preferences.model, locale, nextImage,
                             injectSummaryError, expectEmptyState,
                             &nextState, &error))
            {
                qCritical().noquote() << error;
                return 1;
            }
            if (injectSummaryError && locale == QStringLiteral("en_US")
                && theme == QStringLiteral("light"))
            {
                const QString fixturesDirectory = QDir(outputDirectory)
                    .filePath(QStringLiteral("fixtures"));
                if (!QDir().mkpath(fixturesDirectory))
                {
                    qCritical("Could not create preserved fixture directory");
                    return 1;
                }
                const QString preservedPath = QDir(fixturesDirectory)
                    .filePath(QStringLiteral("summary-error-legacy.tps"));
                if (!copyFixture(legacyDb, preservedPath, &error))
                {
                    qCritical().noquote() << error;
                    return 1;
                }
                legacyState.insert(QStringLiteral("preservedFixtureFile"),
                                   QFileInfo(preservedPath).canonicalFilePath());
            }
            captures.append(QJsonObject{
                {QStringLiteral("locale"), locale},
                {QStringLiteral("theme"), theme},
                {QStringLiteral("legacy"), legacyState},
                {QStringLiteral("next"), nextState}
            });
            if (translator)
            {
                application.removeTranslator(translator.get());
            }
        }
    }

    const QJsonObject manifest{
        {QStringLiteral("purpose"), QStringLiteral(
            "Matched legacy and Next My Classes page captures")},
        {QStringLiteral("scenario"), scenario},
        {QStringLiteral("viewport"), QJsonObject{
            {QStringLiteral("width"), Viewport.width()},
            {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("style"), QStringLiteral("Fusion")},
        {QStringLiteral("platform"), QStringLiteral("offscreen")},
        {QStringLiteral("fixture"), scenario == QStringLiteral("empty")
            ? QStringLiteral("Synthetic schema-valid empty workspace")
            : QStringLiteral("tracked 96-class large-classes visual fixture")},
        {QStringLiteral("fixtureFile"), QFileInfo(fixturePath).canonicalFilePath()},
        {QStringLiteral("fixtureSha256"), fixtureSha256},
        {QStringLiteral("sourceFixtureFile"),
         QFileInfo(sourceFixturePath).canonicalFilePath()},
        {QStringLiteral("sourceFixtureSha256"), sourceFixtureSha256},
        {QStringLiteral("sourceRoot"), QFileInfo(sourceRoot).canonicalFilePath()},
        {QStringLiteral("sourceFiles"), sourceFiles},
        {QStringLiteral("fontPreference"), QStringLiteral("Normal")},
        {QStringLiteral("configuredBaseFontPointSize"),
         fontPresentation.applicationBaseFontPointSize()},
        {QStringLiteral("legacyFontSetup"), QStringLiteral(
            "FontManager::loadFonts + FontManager::applyGlobalFont")},
        {QStringLiteral("nextFontSetup"), QStringLiteral(
            "ResourceLoader::loadFileBackedFont (six Inter faces) + ResourceLoader::loadFont (Pretendard) + FontSizePresenter::applyStartupPreference")},
        {QStringLiteral("fontAssets"), fontAssets},
        {QStringLiteral("runtime"), QJsonObject{
            {QStringLiteral("qtVersion"), QString::fromLatin1(qVersion())},
            {QStringLiteral("platformName"), QGuiApplication::platformName()}
        }},
        {QStringLiteral("memoryScope"), QStringLiteral(
            "Process-wide samples; not per-page allocation measurements")},
        {QStringLiteral("captures"), captures}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qCritical("Could not write the capture manifest");
        return 1;
    }
    manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    return 0;
}
