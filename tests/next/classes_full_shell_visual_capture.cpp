#include "next/platform/workspace_store_class_editor_actions.h"
#include "next/platform/workspace_store_classes_navigator_read_actions.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/application_shell.h"
#include "next/ui/classes_navigator_page.h"
#include "next/ui/page_host.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPalette>
#include <QPixmap>
#include <QTemporaryDir>
#include <QTranslator>

#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

constexpr QSize Viewport{800, 600};
constexpr int SelectedClassId = 25;

QString readText(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly | QIODevice::Text)
        ? QString::fromUtf8(file.readAll())
        : QString{};
}

QPalette themePalette(const QString& theme)
{
    QPalette palette;
    const bool dark = theme == QStringLiteral("dark");
    palette.setColor(QPalette::Window,
        QColor(dark ? QStringLiteral("#202327") : QStringLiteral("#f5f6f7")));
    palette.setColor(QPalette::WindowText,
        QColor(dark ? QStringLiteral("#f5f6f7") : QStringLiteral("#232629")));
    palette.setColor(QPalette::Base,
        QColor(dark ? QStringLiteral("#292c30") : QStringLiteral("#ffffff")));
    palette.setColor(QPalette::Text,
        QColor(dark ? QStringLiteral("#fcfcfc") : QStringLiteral("#232629")));
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

QByteArray sha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256)
        .toHex();
}

bool copyLegacyCapture(
    const QString& locale,
    const QString& theme,
    const QString& outputDirectory,
    QString* outputPath,
    QString* error
    )
{
    const QString localeName = locale == QStringLiteral("en_US")
        ? QStringLiteral("english") : QStringLiteral("korean");
    const QString sourcePath = QDir(QStringLiteral(
            CLASSMNGR_CLASSES_LEGACY_CAPTURE_ROOT))
        .filePath(QStringLiteral("%1-%2/classes-entry.png")
            .arg(localeName, theme));
    const QString fileName = QStringLiteral("legacy_classes_%1_%2.png")
        .arg(locale, theme);
    const QString destinationPath = QDir(outputDirectory).filePath(fileName);
    if (QFileInfo::exists(destinationPath) && !QFile::remove(destinationPath))
    {
        if (error) *error = QStringLiteral("Could not replace Legacy capture: %1")
            .arg(destinationPath);
        return false;
    }
    if (!QFile::copy(sourcePath, destinationPath))
    {
        if (error) *error = QStringLiteral("Could not copy Legacy capture: %1")
            .arg(sourcePath);
        return false;
    }
    const QImage image(destinationPath);
    if (image.size() != Viewport)
    {
        if (error) *error = QStringLiteral(
            "Legacy capture has an unexpected viewport: %1x%2")
                .arg(image.width()).arg(image.height());
        return false;
    }
    *outputPath = destinationPath;
    return true;
}

bool captureNextShell(
    Resources::ResourceLoader& loader,
    Application::PreferencesModel& preferences,
    const QString& fixturePath,
    const QString& outputDirectory,
    const QString& classesRouteLabel,
    QString* outputPath,
    QString* error
    )
{
    const auto list = Platform::readClassesNavigatorListAtPath(fixturePath);
    if (!list || list.value().classes.size() != 96)
    {
        if (error) *error = QStringLiteral("The tracked fixture did not return 96 Classes rows.");
        return false;
    }
    const auto selectedClassId = Domain::ClassId::fromString(
        std::to_string(SelectedClassId));
    if (!selectedClassId)
    {
        if (error) *error = QStringLiteral("The selected fixture class ID is invalid.");
        return false;
    }

    Ui::ApplicationShell shell(loader, preferences);
    auto activeSelection = std::optional<Domain::ClassId>(*selectedClassId);
    const Ui::PageId classesPageId = *Ui::PageId::fromString(
        QStringLiteral("classes"));
    Ui::PageDescriptor classesPage{
        .id = classesPageId,
        .label = classesRouteLabel,
        .factory = [&fixturePath, &activeSelection](
            const Ui::PageContext& context
            ) -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            Ui::ClassesNavigatorPageActions actions;
            actions.workspaceIdentity = [&fixturePath] { return fixturePath; };
            actions.activeWorkspacePath = [&fixturePath] {
                return std::optional<QString>(fixturePath);
            };
            actions.readClasses = [&fixturePath](const QString&) {
                return Platform::readClassesNavigatorListAtPath(fixturePath);
            };
            actions.readNavigation = [&fixturePath](
                const QString&,
                const Application::ClassesNavigationSnapshotQuery& query
                ) {
                return Platform::readClassesNavigatorNavigationAtPath(
                    fixturePath, query);
            };
            actions.readPreferences = [&fixturePath](const QString&) {
                return Platform::readClassesNavigatorPreferencesAtPath(fixturePath);
            };
            actions.saveScheduleMode = [&fixturePath](
                const QString&,
                const Application::ScheduleDisplayMode mode
                ) {
                return Platform::saveClassesNavigatorScheduleModeAtPath(
                    fixturePath, mode);
            };
            actions.saveVisibilityScope = [&fixturePath](
                const QString&,
                const Application::ClassVisibilityScope scope
                ) {
                return Platform::saveClassesNavigatorVisibilityAtPath(
                    fixturePath, scope);
            };
            actions.activeClassSelection = [&activeSelection] {
                return activeSelection;
            };
            actions.setActiveClassSelection = [&activeSelection](
                const std::optional<Domain::ClassId>& selected
                ) {
                activeSelection = selected;
            };
            actions.readClassDetails = [&fixturePath](
                const QString&,
                const Domain::ClassId& classId
                ) {
                return Platform::readClassDetailsAtPath(fixturePath, classId);
            };
            return std::unique_ptr<Ui::Page>(
                std::make_unique<Ui::ClassesNavigatorPage>(
                    context, std::move(actions)));
        }
    };
    if (!shell.pageHost().registerPage(std::move(classesPage)))
    {
        if (error) *error = QStringLiteral("Could not register the Next Classes page.");
        return false;
    }
    auto configured = shell.configureNavigation(
        {Ui::NavigationNode::page(classesPageId,
            Ui::NavigationWorkspaceAvailability::WorkspaceOpen)},
        classesPageId,
        classesPageId,
        Application::WorkspaceLifecycleState::Open);
    if (!configured || !shell.navigateTo(classesPageId, Ui::NavigationCause::Initial))
    {
        if (error) *error = QStringLiteral("Could not activate the Next Classes shell route.");
        return false;
    }
    auto* const page = dynamic_cast<Ui::ClassesNavigatorPage*>(
        shell.pageHost().pageInstance(classesPageId.value()));
    if (!page || page->model()->classCount() != 96
        || page->selectedClassId() != selectedClassId
        || !page->selectedClassEditor()
        || !page->selectedClassEditor()->isLoaded())
    {
        if (error) *error = QStringLiteral(
            "Next did not render the same 96-class fixture selection and details.");
        return false;
    }

    shell.resize(Viewport);
    shell.show();
    QApplication::processEvents();
    if (QWidget* const focused = QApplication::focusWidget()) focused->clearFocus();
    QApplication::processEvents();
    const QString fileName = QStringLiteral("next_classes_%1.png")
        .arg(QFileInfo(outputDirectory).fileName());
    const QString destinationPath = QDir(outputDirectory).filePath(fileName);
    const QPixmap pixmap = shell.grab();
    if (pixmap.size() != Viewport || !pixmap.save(destinationPath, "PNG"))
    {
        if (error) *error = QStringLiteral("Could not save the full-shell Next capture.");
        return false;
    }
    *outputPath = destinationPath;
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    if (argc != 2)
    {
        qCritical("Usage: ClassMngrNextClassesFullShellVisualCapture OUTPUT_DIR");
        return 2;
    }

    const QString outputDirectory = QString::fromLocal8Bit(argv[1]);
    if (!QDir().mkpath(outputDirectory))
    {
        qCritical("Could not create full-shell capture directory");
        return 2;
    }
    const QString sourceFixture = QStringLiteral(CLASSMNGR_CLASSES_NAVIGATOR_FIXTURE);
    const QString fixtureHash = QString::fromLatin1(sha256(sourceFixture));
    if (fixtureHash.isEmpty())
    {
        qCritical("Could not hash the tracked Classes fixture");
        return 1;
    }
    const auto loaded = Resources::ResourceCatalog::load(
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT));
    if (!loaded)
    {
        qCritical().noquote() << "Could not load Next resources:" << loaded.error().message;
        return 1;
    }
    Resources::ResourceLoader loader(std::move(*loaded));
    const QString styles = QDir(QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT))
        .filePath(QStringLiteral("assets/styles"));
    const QString translations = QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY);
    const QString outputRoot = QDir(outputDirectory).absolutePath();
    QJsonArray captures;
    for (const QString& locale : {QStringLiteral("en_US"), QStringLiteral("ko_KR")})
    {
        for (const QString& theme : {QStringLiteral("light"), QStringLiteral("dark")})
        {
            QLocale::setDefault(QLocale(locale));
            application.setPalette(themePalette(theme));
            const QString stylesheet = readText(QDir(styles).filePath(
                theme + QStringLiteral(".qss")));
            if (stylesheet.isEmpty())
            {
                qCritical().noquote() << "Could not load the packaged theme:" << theme;
                return 1;
            }
            application.setStyleSheet(stylesheet);
            std::unique_ptr<QTranslator> translator;
            if (locale == QStringLiteral("ko_KR"))
            {
                translator = std::make_unique<QTranslator>();
                if (!translator->load(QDir(translations).filePath(
                        QStringLiteral("ClassMngr_ko_KR.qm")))
                    || !application.installTranslator(translator.get()))
                {
                    qCritical("Could not load the production Korean catalog");
                    return 1;
                }
            }
            const QString classesRouteLabel = QObject::tr("Classes");
            const QString expectedClassesRouteLabel =
                locale == QStringLiteral("ko_KR")
                ? QStringLiteral("수업") : QStringLiteral("Classes");
            if (classesRouteLabel != expectedClassesRouteLabel)
            {
                qCritical().noquote() << "Unexpected translated Classes route label:"
                    << classesRouteLabel;
                return 1;
            }

            QTemporaryDir fixtureDirectory;
            if (!fixtureDirectory.isValid())
            {
                qCritical("Could not create a temporary fixture directory");
                return 1;
            }
            const QString fixturePath = fixtureDirectory.filePath(
                QStringLiteral("classes-shell-fixture.tps"));
            if (!QFile::copy(sourceFixture, fixturePath))
            {
                qCritical("Could not copy the tracked Classes fixture");
                return 1;
            }

            const QString pairName = locale + QStringLiteral("-") + theme;
            const QString pairDirectory = QDir(outputRoot).filePath(pairName);
            if (!QDir().mkpath(pairDirectory))
            {
                qCritical().noquote() << "Could not create capture pair:" << pairDirectory;
                return 1;
            }
            QString legacyPath;
            QString error;
            if (!copyLegacyCapture(locale, theme, pairDirectory, &legacyPath, &error))
            {
                qCritical().noquote() << error;
                return 1;
            }
            Test::MemoryPreferences preferences;
            QString nextPath;
            if (!captureNextShell(loader, preferences.model, fixturePath,
                    pairDirectory, classesRouteLabel, &nextPath, &error))
            {
                qCritical().noquote() << error;
                return 1;
            }
            const QByteArray legacyHash = sha256(legacyPath);
            const QByteArray nextHash = sha256(nextPath);
            if (legacyHash.isEmpty() || nextHash.isEmpty())
            {
                qCritical("Could not hash a matched Classes capture");
                return 1;
            }
            captures.append(QJsonObject{
                {QStringLiteral("locale"), locale},
                {QStringLiteral("theme"), theme},
                {QStringLiteral("viewport"), QJsonObject{
                    {QStringLiteral("width"), Viewport.width()},
                    {QStringLiteral("height"), Viewport.height()}}},
                {QStringLiteral("fixtureClassCount"), 96},
                {QStringLiteral("selectedClassId"), SelectedClassId},
                {QStringLiteral("nextClassesRouteLabel"), classesRouteLabel},
                {QStringLiteral("legacy"), QJsonObject{
                    {QStringLiteral("image"), pairName + QStringLiteral("/")
                        + QFileInfo(legacyPath).fileName()},
                    {QStringLiteral("source"), QStringLiteral(
                        "docs/qt-rewrite/visual-baseline/release/large-classes-visual-states/%1-%2/classes-entry.png")
                            .arg(locale == QStringLiteral("en_US")
                                ? QStringLiteral("english") : QStringLiteral("korean"), theme)},
                    {QStringLiteral("sha256"), QString::fromLatin1(legacyHash)}}},
                {QStringLiteral("next"), QJsonObject{
                    {QStringLiteral("image"), pairName + QStringLiteral("/")
                        + QFileInfo(nextPath).fileName()},
                    {QStringLiteral("sha256"), QString::fromLatin1(nextHash)}}}
            });
            if (translator)
            {
                application.removeTranslator(translator.get());
            }
        }
    }

    const QJsonObject manifest{
        {QStringLiteral("purpose"), QStringLiteral(
            "Matched Legacy application-shell and Next single-route ApplicationShell window captures using the same tracked workspace fixture and selected class.")},
        {QStringLiteral("fixture"), QStringLiteral(
            "docs/qt-rewrite/visual-baseline/release/large-classes-visual-states/generated-large-classes-visual.tps")},
        {QStringLiteral("fixtureSha256"), fixtureHash},
        {QStringLiteral("fixtureClassCount"), 96},
        {QStringLiteral("fixtureTeacherCount"), 24},
        {QStringLiteral("selectedClassId"), SelectedClassId},
        {QStringLiteral("viewport"), QJsonObject{
            {QStringLiteral("width"), Viewport.width()},
            {QStringLiteral("height"), Viewport.height()}}},
        {QStringLiteral("nextStyle"), QStringLiteral("Fusion with packaged locale theme")},
        {QStringLiteral("nextShellComposition"), QStringLiteral(
            "ApplicationShell window with one test-registered Classes page; the production navigation tree is not loaded.")},
        {QStringLiteral("nextClassesRouteLabelSource"), QStringLiteral(
            "QObject::tr(\"Classes\") using the production compiled locale catalog")},
        {QStringLiteral("productionNavigationTreeIncluded"), false},
        {QStringLiteral("parityClaim"), false},
        {QStringLiteral("limitation"), QStringLiteral(
            "The pairs show the same tracked 96-class workspace, class ID 25, locale, theme, and 800x600 viewport. The Legacy images are full-application captures; the Next images show the ApplicationShell window with a single test-registered Classes page, not the production navigation tree. The page navigation and Classes presentation intentionally differ; these captures support visual review and do not claim pixel parity.")},
        {QStringLiteral("captures"), captures}
    };
    QFile manifestFile(QDir(outputRoot).filePath(
        QStringLiteral("matched-full-shell-captures.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Text)
        || manifestFile.write(QJsonDocument(manifest).toJson(
            QJsonDocument::Indented)) < 0)
    {
        qCritical("Could not write the matched full-shell capture manifest");
        return 1;
    }
    return 0;
}
