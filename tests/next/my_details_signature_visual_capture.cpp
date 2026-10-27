#include "core/application_services.h"
#include "core/process_memory_snapshot.h"
#include "data/database/workspace_store.h"
#include "features/my_info/data/signature_image_processor.h"
#include "features/my_info/data/typed_signature_renderer.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "next/application/personal_details_save_use_case.h"
#include "next/platform/workspace_store_personal_details_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/my_workspace_page.h"
#include "next/ui/page_host.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImage>
#include <QLabel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTranslator>

#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr;

namespace
{

constexpr QSize Viewport{1024, 768};

class EmptyPreferencesReadPort final
    : public Next::Application::PreferencesStateReadPort
{
public:
    [[nodiscard]] Next::Application::PreferencesStateReadResult read() const override
    {
        return Next::Application::PreferencesStateReadResult::success({});
    }
};

class EmptyPreferencesPersistPort final
    : public Next::Application::PreferencesSnapshotPersistPort
{
public:
    [[nodiscard]] Next::Application::PreferencesPersistResult persist(
        const Next::Application::UserPreferencesSnapshot&) const override
    {
        return Next::Application::PreferencesPersistResult::success();
    }
};

struct CaptureMemory final
{
    ProcessMemorySnapshot before;
    ProcessMemorySnapshot rendered;
    ProcessMemorySnapshot cleaned;
    bool previewHasVisibleSignature = false;
    bool footerHiddenWhileClean = false;
    bool footerHiddenWhileDirty = false;
    bool manualSaveButtonAbsent = false;
};

QString fromStdString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::string toStdString(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QString readText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return {};
    }
    return QString::fromUtf8(file.readAll());
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
        {QStringLiteral("handleCount"), static_cast<double>(snapshot.handleCount)},
        {QStringLiteral("threadCount"), static_cast<double>(snapshot.threadCount)},
        {QStringLiteral("capturedAt"),
         snapshot.capturedAt.toString(Qt::ISODateWithMs)}
    };
}

bool hasVisibleContent(const QImage& image)
{
    if (image.isNull())
    {
        return false;
    }
    const QRgb background = image.pixel(0, 0);
    qsizetype contrastingPixels = 0;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QRgb pixel = image.pixel(x, y);
            const int difference = std::abs(qRed(pixel) - qRed(background))
                + std::abs(qGreen(pixel) - qGreen(background))
                + std::abs(qBlue(pixel) - qBlue(background));
            if (difference >= 48)
            {
                ++contrastingPixels;
            }
        }
    }
    return contrastingPixels >= 128;
}

bool hasVisibleSignaturePreview(QLabel* preview)
{
    if (!preview || preview->pixmap().isNull())
    {
        return false;
    }
    const QImage rendered = preview->pixmap().toImage()
        .convertToFormat(QImage::Format_ARGB32);
    qsizetype signaturePixels = 0;
    for (int y = 0; y < rendered.height(); ++y)
    {
        for (int x = 0; x < rendered.width(); ++x)
        {
            const QRgb pixel = rendered.pixel(x, y);
            if (qAlpha(pixel) > 32 && qGray(pixel) < 240)
            {
                ++signaturePixels;
            }
        }
    }
    return signaturePixels >= 128;
}

Next::Application::PersonalDetailsSaveRequest captureDetails()
{
    using namespace Next::Application;
    const QByteArray signature = TypedSignature::renderForEmbedding(
        QStringLiteral("Alex Morgan"),
        TypedSignatureFont::DancingScript
        );
    return {
        .name = "Alex Morgan",
        .campus = CurrentCampusPreferenceValue("Bundang"),
        .zoomLoginId = "alex.morgan.zoom",
        .zoomPassword = "capture-secret",
        .zoomNotAvailable = false,
        .signatureImage = Next::Domain::PersonalSignatureImageSaveBytes(
            std::string(signature.constData(),
                        static_cast<std::size_t>(signature.size()))),
        .signatureMode = PersonalSignatureMode::Image,
        .typedSignatureText = "Alex Morgan",
        .typedSignatureFont = PersonalSignatureFontChoice::DancingScript
    };
}

bool seedWorkspace(const QString& path, QString* error)
{
    WorkspaceStore store;
    const auto opened = store.open(path);
    if (!opened)
    {
        if (error)
        {
            *error = opened.error().diagnostic;
        }
        return false;
    }
    Next::Platform::WorkspaceStorePersonalDetailsPort port(store);
    const auto saved = port.save(captureDetails());
    if (!saved)
    {
        if (error)
        {
            *error = QString::fromUtf8(saved.error().message.data(),
                                       static_cast<qsizetype>(saved.error().message.size()));
        }
        return false;
    }
    store.close();
    return true;
}

bool captureLegacy(
    const QString& databasePath,
    const QString& mode,
    const QString& output,
    PlatformProcessMemorySnapshotProvider& memoryProvider,
    CaptureMemory* memory,
    QString* error
    )
{
    if (!memory)
    {
        return false;
    }
    memory->before = memoryProvider.snapshot();
    bool captured = false;
    {
        ApplicationServices services;
        const RepositoryStatus opened = services.openExistingDatabase(databasePath);
        if (!opened)
        {
            if (error)
            {
                *error = opened.error().diagnostic;
            }
            return false;
        }

        MyWorkspacePage page(&services);
        page.setDatabaseOpen(true);
        page.setSaveMode(SaveMode::Manual);
        page.openTab(WorkspaceTab::Details);
        if (mode == QStringLiteral("type"))
        {
            auto* const typeMode = page.findChild<QPushButton*>(
                QStringLiteral("signatureTypeModeButton"));
            if (!typeMode)
            {
                if (error)
                {
                    *error = QStringLiteral("Legacy typed signature control is missing.");
                }
                return false;
            }
            typeMode->click();
        }

        page.resize(Viewport);
        page.show();
        QApplication::processEvents();
        if (QWidget* const focused = QApplication::focusWidget())
        {
            focused->clearFocus();
        }
        QApplication::processEvents();
        memory->previewHasVisibleSignature = hasVisibleSignaturePreview(
            page.findChild<QLabel*>(QStringLiteral("signatureImagePreview")));
        if (!memory->previewHasVisibleSignature)
        {
            if (error)
            {
                *error = QStringLiteral(
                    "The legacy signature preview is blank in the requested mode.");
            }
            return false;
        }
        memory->rendered = memoryProvider.snapshot();
        const QImage image = page.grab().toImage();
        captured = image.size() == Viewport && hasVisibleContent(image)
            && image.save(output, "PNG");
    }
    QApplication::processEvents();
    memory->cleaned = memoryProvider.snapshot();
    if (!captured && error)
    {
        *error = QStringLiteral("Could not save the legacy My Details capture.");
    }
    return captured;
}

bool captureNext(
    Next::Resources::ResourceLoader& loader,
    Next::Application::PreferencesModel& preferences,
    const QString& databasePath,
    const QString& mode,
    const QString& output,
    PlatformProcessMemorySnapshotProvider& memoryProvider,
    CaptureMemory* memory,
    QString* error
    )
{
    if (!memory)
    {
        return false;
    }
    memory->before = memoryProvider.snapshot();
    WorkspaceStore store;
    if (const auto opened = store.openExisting(databasePath); !opened)
    {
        if (error)
        {
            *error = opened.error().diagnostic;
        }
        return false;
    }
    Next::Platform::WorkspaceStorePersonalDetailsPort port(store);
    const Next::Application::PersonalDetailsReadQuery read(port);
    const Next::Application::WorkspacePersonalDetailsSaveUseCase save(port);
    bool captured = false;
    {
        Next::Ui::PageHost host(loader, preferences);
        auto registration = host.registerPage({
            .id = *Next::Ui::PageId::fromString(QStringLiteral("my_workspace")),
            .label = QStringLiteral("My Workspace"),
            .factory = [&read, &save, &loader, &store](
                const Next::Ui::PageContext& context
                ) -> Next::Ui::PageOperationResult<std::unique_ptr<Next::Ui::Page>> {
                Next::Ui::MyWorkspacePageActions actions;
                actions.saveMode = Next::Application::SaveMode::Manual;
                actions.readDetails = [&read] { return read.execute(); };
                actions.saveDetails = [&save](
                    const Next::Application::PersonalDetailsSaveRequest& request
                    ) { return save.execute(request); };
                actions.workspaceIdentity = [&store] {
                    return store.isOpen() ? QStringLiteral("capture-profile") : QString{};
                };
                actions.loadCampuses = [&loader](const Next::Ui::PageContext& pageContext) {
                    std::vector<Next::Application::MyInfoCampusMetadata> campuses;
                    for (const auto& descriptor : loader.catalog().records())
                    {
                        const QString path = descriptor.packageRelativePath();
                        if (descriptor.expectedType() != Next::Resources::ResourceType::Json
                            || !path.startsWith(QStringLiteral("assets/campuses/"))
                            || path.endsWith(QLatin1Char('/'))
                            || path.indexOf(QLatin1Char('/'),
                                             QStringLiteral("assets/campuses/").size()) >= 0)
                        {
                            continue;
                        }
                        auto request = Next::Resources::ResourceRequest::create(
                            descriptor.id(), pageContext.pageScope());
                        if (!request)
                        {
                            continue;
                        }
                        auto json = loader.loadJson(*request);
                        if (!json)
                        {
                            continue;
                        }
                        const QJsonObject object = json->get().object();
                        const QString id = object.value(QStringLiteral("id")).toString();
                        const QString display = object.value(QStringLiteral("campus_name"))
                            .toString().trimmed();
                        const auto typedId = Next::Domain::CampusId::fromString(
                            id.toUtf8().toStdString());
                        if (typedId && !display.isEmpty())
                        {
                            campuses.push_back({*typedId, display.toUtf8().toStdString()});
                        }
                    }
                    std::stable_sort(campuses.begin(), campuses.end(), [](const auto& left,
                                                                           const auto& right) {
                        return QString::compare(
                            fromStdString(left.displayName),
                            fromStdString(right.displayName),
                            Qt::CaseInsensitive) < 0;
                    });
                    return campuses;
                };
                return std::unique_ptr<Next::Ui::Page>(
                    std::make_unique<Next::Ui::MyWorkspacePage>(
                        context, std::move(actions)));
            }
        });
        if (!registration
            || !host.activatePage(QStringLiteral("my_workspace")))
        {
            if (error)
            {
                *error = QStringLiteral("Next My Workspace page did not activate.");
            }
            return false;
        }
        auto* const page = dynamic_cast<Next::Ui::MyWorkspacePage*>(
            host.pageInstance(QStringLiteral("my_workspace")));
        if (!page)
        {
            return false;
        }
        host.resize(Viewport);
        host.show();
        QApplication::processEvents();
        auto* const footer = page->findChild<QWidget*>(
            QStringLiteral("nextMyWorkspaceStatusFooter"));
        auto* const name = page->findChild<QLineEdit*>(
            QStringLiteral("nextMyWorkspaceName"));
        const auto* const saveButton = page->findChild<QPushButton*>(
            QStringLiteral("nextMyWorkspaceSaveDetails"));
        if (!footer || !name || saveButton)
        {
            if (error)
            {
                *error = QStringLiteral(
                    "The Next My Details page has a persistent save footer or button.");
            }
            return false;
        }
        memory->manualSaveButtonAbsent = true;
        memory->footerHiddenWhileClean = !footer->isVisible();
        if (!memory->footerHiddenWhileClean)
        {
            if (error)
            {
                *error = QStringLiteral(
                    "The Next My Details footer is visible in the clean state.");
            }
            return false;
        }

        const QString originalName = name->text();
        name->setText(originalName + QStringLiteral(" draft"));
        name->setText(originalName);
        if (mode == QStringLiteral("type"))
        {
            auto* const typeMode = page->findChild<QPushButton*>(
                QStringLiteral("nextMyWorkspaceSignatureTypeMode"));
            if (!typeMode)
            {
                return false;
            }
            typeMode->click();
        }
        QApplication::processEvents();
        memory->footerHiddenWhileDirty = page->hasUnsavedChanges()
            && !footer->isVisible();
        if (!memory->footerHiddenWhileDirty)
        {
            if (error)
            {
                *error = QStringLiteral(
                    "The Next My Details footer is visible in the dirty state.");
            }
            return false;
        }
        if (QWidget* const focused = QApplication::focusWidget())
        {
            focused->clearFocus();
        }
        QApplication::processEvents();
        memory->previewHasVisibleSignature = hasVisibleSignaturePreview(
            page->findChild<QLabel*>(
                QStringLiteral("nextMyWorkspaceSignaturePreview")));
        if (!memory->previewHasVisibleSignature)
        {
            if (error)
            {
                *error = QStringLiteral(
                    "The Next signature preview is blank in the requested mode.");
            }
            return false;
        }
        memory->rendered = memoryProvider.snapshot();
        const QImage image = page->grab().toImage();
        captured = image.size() == Viewport && hasVisibleContent(image)
            && image.save(output, "PNG");
    }
    store.close();
    QApplication::processEvents();
    memory->cleaned = memoryProvider.snapshot();
    if (!captured && error)
    {
        *error = QStringLiteral("Could not save the Next My Details capture.");
    }
    return captured;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    if (argc != 2)
    {
        qCritical("Usage: ClassMngrMyDetailsSignatureVisualCapture OUTPUT_DIR");
        return 2;
    }
    QDir::setCurrent(QStringLiteral(CLASSMNGR_SOURCE_DIR));
    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create capture output directory");
        return 2;
    }

    const QString resourceRoot = QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    auto catalog = Next::Resources::ResourceCatalog::load(
        QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG), resourceRoot);
    if (!catalog)
    {
        qCritical().noquote() << catalog.error().message;
        return 1;
    }
    Next::Resources::ResourceLoader loader(std::move(*catalog));
    EmptyPreferencesReadPort preferencesRead;
    EmptyPreferencesPersistPort preferencesPersist;
    Next::Application::PreferencesModel preferences(
        preferencesRead, preferencesPersist);

    for (const QString& font : {
             QStringLiteral("Inter.ttc"),
             QStringLiteral("PretendardVariable.ttf"),
             QStringLiteral("DancingScript.ttf"),
             QStringLiteral("GreatVibes.ttf"),
             QStringLiteral("Caveat.ttf"),
             QStringLiteral("JustAnotherHand.ttf")
         })
    {
        const QString path = QDir(resourceRoot).filePath(
            QStringLiteral("assets/fonts/") + font);
        if (QFile::exists(path))
        {
            static_cast<void>(QFontDatabase::addApplicationFont(path));
        }
    }

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        qCritical("Could not create temporary profile directory");
        return 1;
    }
    const QString databasePath = temporaryDirectory.filePath(
        QStringLiteral("my-details-signature.tps"));
    QString error;
    if (!seedWorkspace(databasePath, &error))
    {
        qCritical().noquote() << error;
        return 1;
    }

    PlatformProcessMemorySnapshotProvider memoryProvider;
    QJsonArray captures;
    for (const auto& [locale, theme] : {
             std::pair{QStringLiteral("en_US"), QStringLiteral("light")},
             std::pair{QStringLiteral("ko_KR"), QStringLiteral("dark")}
         })
    {
        const QString stylesheet = readText(QDir(resourceRoot).filePath(
            QStringLiteral("assets/styles/%1.qss").arg(theme)));
        if (stylesheet.isEmpty())
        {
            qCritical().noquote() << "Missing theme stylesheet for" << theme;
            return 1;
        }
        application.setStyleSheet(stylesheet);
        QTranslator translator;
        bool installed = false;
        if (locale == QStringLiteral("ko_KR"))
        {
            const QString translationDirectory = QFileInfo(
                QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG)
                ).absolutePath() + QStringLiteral("/assets/translations");
            if (!translator.load(QDir(translationDirectory).filePath(
                    QStringLiteral("ClassMngr_ko_KR.qm")))
                || !application.installTranslator(&translator))
            {
                qCritical("Could not install Korean translation");
                return 1;
            }
            installed = true;
        }

        for (const QString& mode : {QStringLiteral("image"), QStringLiteral("type")})
        {
            for (const QString& variant : {
                     QStringLiteral("legacy"), QStringLiteral("next")})
            {
                const QString fileName = QStringLiteral("%1_%2-%3_%4.png")
                    .arg(variant, locale, theme, mode);
                const QString path = QDir(outputDirectory).filePath(fileName);
                CaptureMemory memory;
                const bool saved = variant == QStringLiteral("legacy")
                    ? captureLegacy(databasePath, mode, path, memoryProvider,
                                    &memory, &error)
                    : captureNext(loader, preferences, databasePath, mode, path,
                                  memoryProvider, &memory, &error);
                if (!saved)
                {
                    qCritical().noquote() << error << "(" << path << ")";
                    return 1;
                }
                captures.append(QJsonObject{
                    {QStringLiteral("variant"), variant},
                    {QStringLiteral("locale"), locale},
                    {QStringLiteral("theme"), theme},
                    {QStringLiteral("signatureMode"), mode},
                    {QStringLiteral("file"), fileName},
                    {QStringLiteral("width"), Viewport.width()},
                    {QStringLiteral("height"), Viewport.height()},
                    {QStringLiteral("previewHasVisibleSignature"),
                     memory.previewHasVisibleSignature},
                    {QStringLiteral("footerHiddenWhileClean"),
                     memory.footerHiddenWhileClean},
                    {QStringLiteral("footerHiddenWhileDirty"),
                     memory.footerHiddenWhileDirty},
                    {QStringLiteral("manualSaveButtonAbsent"),
                     memory.manualSaveButtonAbsent},
                    {QStringLiteral("memory"), QJsonObject{
                         {QStringLiteral("before"), memoryJson(memory.before)},
                         {QStringLiteral("rendered"), memoryJson(memory.rendered)},
                         {QStringLiteral("afterCleanup"), memoryJson(memory.cleaned)}}}
                });
            }
        }
        if (installed)
        {
            application.removeTranslator(&translator);
        }
    }

    const QJsonObject manifest{
        {QStringLiteral("purpose"), QStringLiteral(
             "Matched legacy and Next My Details and Signature captures")},
        {QStringLiteral("viewport"), QJsonObject{
             {QStringLiteral("width"), Viewport.width()},
             {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("qtPlatform"), QStringLiteral("offscreen")},
        {QStringLiteral("style"), QStringLiteral("Fusion")},
        {QStringLiteral("workspace"), QJsonObject{
             {QStringLiteral("name"), QStringLiteral("Alex Morgan")},
             {QStringLiteral("campus"), QStringLiteral("Bundang")},
             {QStringLiteral("signatureModes"), QJsonArray{
                  QStringLiteral("image"), QStringLiteral("type")}}}},
        {QStringLiteral("memoryMetricLimit"), QStringLiteral(
             "Platform process snapshots measure the whole process and include Qt, legacy runtime, and loaded resources; they are comparative lifecycle observations, not a page-specific allocation budget.")},
        {QStringLiteral("captures"), captures}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || manifestFile.write(QJsonDocument(manifest).toJson(
               QJsonDocument::Indented)) < 0)
    {
        qCritical("Could not write capture manifest");
        return 1;
    }
    qInfo().noquote() << "Captured My Details and Signature surfaces in"
                      << outputDirectory;
    return 0;
}
