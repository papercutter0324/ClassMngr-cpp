#include "app/controllers/navigation_controller.h"
#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/resource_packs/resource_pack_manager.h"
#include "domain/models/teacher.h"
#include "features/teacher/ui/teacher_info_page.h"
#include "fakes/fake_user_prompt_service.h"
#include "next/application/document_content_session.h"
#include "next/platform/application_services_document_catalog_port.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/pagemanager.h"
#include "ui/shared/pages/pdf_viewer_page.h"
#include "ui/shared/widgets/sidebar/sidebar.h"
#include "ui/shared/widgets/sidebar/sidebar_types.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMessageLogContext>
#include <QLocale>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QUuid>
#include <QVector>
#include <QtTest/QtTest>

#include <algorithm>
#include <optional>
#include <utility>

namespace
{

constexpr auto RequestedDocumentId = "document_guides_lesson_planning";
constexpr auto PersistedTeacherNotes = "Persisted teacher profile notes";
constexpr auto UnsavedTeacherNotes =
    "Exact unsaved document route notes\nPreserve every character.";
constexpr auto ExpectedDocumentReference =
    "resource://documents/Guides/DYB Lesson Planning Guide.pdf";
constexpr auto ExpectedDocumentRelativePath =
    "Guides/DYB Lesson Planning Guide.pdf";

using DocumentCatalogProjection =
    ClassMngr::Next::Application::DocumentCatalogProjection;

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("document-navigation-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

struct DirtyTeacherPage final
{
    Teacher persistedTeacher;
    TeacherInfoPage* page = nullptr;
    QTextEdit* notes = nullptr;
};

std::optional<DirtyTeacherPage> prepareDirtyTeacherPage(
    ApplicationServices& services,
    PageManager& pages,
    QString* error
    )
{
    Teacher teacher;
    teacher.teacherKr = QStringLiteral("\uAE40\uC120\uC0DD");
    teacher.teacherEn = QStringLiteral("Route Doc");
    teacher.preferredRomanization = QStringLiteral("Route Doc");
    teacher.preferredName = QStringLiteral("Route Doc");
    teacher.notes = QString::fromLatin1(PersistedTeacherNotes);

    const auto created = services.teacherService()->create(teacher);
    if (!created)
    {
        if (error)
        {
            *error = created.error();
        }
        return std::nullopt;
    }

    teacher.id = *created;
    const auto persisted = services.teacherService()->teacher(teacher.id);
    if (!persisted)
    {
        if (error)
        {
            *error = persisted.error();
        }
        return std::nullopt;
    }

    pages.initialize(&services, false);
    pages.showPage(PageType::TeacherInfo);
    TeacherInfoPage* const page = pages.teacherPage();
    if (!page)
    {
        if (error)
        {
            *error = QStringLiteral("Teacher Info page was not initialized.");
        }
        return std::nullopt;
    }

    page->setSaveMode(SaveMode::Manual);
    page->loadTeacher(*persisted);
    QTextEdit* const notes = page->findChild<QTextEdit*>(
        QStringLiteral("teacherNotesEdit")
        );
    if (!notes)
    {
        if (error)
        {
            *error = QStringLiteral("Teacher notes editor was not found.");
        }
        return std::nullopt;
    }

    notes->setPlainText(QString::fromLatin1(UnsavedTeacherNotes));
    if (!page->hasUnsavedChanges())
    {
        if (error)
        {
            *error = QStringLiteral("Teacher notes edit was not marked dirty.");
        }
        return std::nullopt;
    }

    return DirtyTeacherPage{
        .persistedTeacher = *persisted,
        .page = page,
        .notes = notes
    };
}

std::optional<DocumentCatalogProjection> projectSidebarDocuments(
    ApplicationServices& services,
    Sidebar& sidebar,
    QString* error
    )
{
    ClassMngr::Next::Platform::ApplicationServicesDocumentCatalogPort port(
        services
        );
    const QString localeName = QLocale().name();
    auto projection = port.projection(localeName);
    if (!projection)
    {
        if (error)
        {
            *error = QString::fromUtf8(projection.error().message.c_str());
        }
        return std::nullopt;
    }

    sidebar.setDocumentCatalog(projection.value(), localeName);
    return std::move(projection.value());
}

std::optional<NavigationData> clickDocumentLeaf(
    Sidebar& sidebar,
    const QString& documentId,
    QString* error
    )
{
    sidebar.resize(360, 680);
    sidebar.show();

    QTreeWidget* const tree = sidebar.findChild<QTreeWidget*>(
        QStringLiteral("sidebarTree")
        );
    if (!tree)
    {
        if (error)
        {
            *error = QStringLiteral("Sidebar tree was not found.");
        }
        return std::nullopt;
    }

    QTreeWidgetItem* leaf = nullptr;
    for (QTreeWidgetItemIterator iterator(tree); *iterator; ++iterator)
    {
        if ((*iterator)->data(0, Qt::UserRole + 4).toString() == documentId)
        {
            leaf = *iterator;
            break;
        }
    }
    if (!leaf || leaf->parent() == nullptr)
    {
        if (error)
        {
            *error = QStringLiteral("Requested document leaf was not found.");
        }
        return std::nullopt;
    }

    for (QTreeWidgetItem* ancestor = leaf->parent();
         ancestor;
         ancestor = ancestor->parent())
    {
        ancestor->setExpanded(true);
    }

    QCoreApplication::processEvents();
    tree->scrollToItem(leaf);
    QCoreApplication::processEvents();
    const QRect leafRect = tree->visualItemRect(leaf);
    if (!leafRect.isValid() || leafRect.isEmpty())
    {
        if (error)
        {
            *error = QStringLiteral("Requested document leaf is not visible.");
        }
        return std::nullopt;
    }

    QSignalSpy selectedRoutes(&sidebar, &Sidebar::itemSelected);
    if (!selectedRoutes.isValid())
    {
        if (error)
        {
            *error = QStringLiteral("Sidebar navigation signal could not be observed.");
        }
        return std::nullopt;
    }

    QTest::mouseClick(
        tree->viewport(),
        Qt::LeftButton,
        Qt::NoModifier,
        leafRect.center()
        );
    if (selectedRoutes.size() != 1)
    {
        if (error)
        {
            *error = QStringLiteral(
                "Clicking the document leaf did not emit exactly one route."
                );
        }
        return std::nullopt;
    }

    return qvariant_cast<NavigationData>(selectedRoutes.takeFirst().at(0));
}

void verifyDocumentRoute(
    const NavigationData& route,
    const QString& documentId
    )
{
    QVERIFY(route.type == NodeType::Page);
    QCOMPARE(route.keys.size(), 3);
    QCOMPARE(route.keys.first(), QStringLiteral("document"));
    QCOMPARE(route.keys.at(1), QStringLiteral("document_guides"));
    QCOMPARE(route.keys.last(), documentId);
    QCOMPARE(route.routeKey, documentId);
    QCOMPARE(route.path.size(), 3);
}

void verifyNoOtherPrompts(const FakeUserPromptService& prompts)
{
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.actionPrompts.isEmpty());
}

void verifyDirtyTeacherUnchanged(
    PageManager& pages,
    const DirtyTeacherPage& source
    )
{
    QVERIFY(pages.isCurrentPage(PageType::TeacherInfo));
    QCOMPARE(pages.currentWidget(), static_cast<QWidget*>(source.page));
    QCOMPARE(pages.teacherPage(), source.page);
    QCOMPARE(source.page->teacher().id, source.persistedTeacher.id);
    QCOMPARE(source.page->teacher().teacherKr,
        source.persistedTeacher.teacherKr);
    QCOMPARE(source.page->teacher().teacherEn,
        source.persistedTeacher.teacherEn);
    QCOMPARE(source.page->teacher().preferredRomanization,
        source.persistedTeacher.preferredRomanization);
    QCOMPARE(source.page->teacher().preferredName,
        source.persistedTeacher.preferredName);
    QCOMPARE(source.notes->toPlainText(),
        QString::fromLatin1(UnsavedTeacherNotes));
    QVERIFY(source.page->hasUnsavedChanges());
}

QVector<QString>* capturedWarnings = nullptr;

void captureQtWarning(
    const QtMsgType type,
    const QMessageLogContext&,
    const QString& message
    )
{
    if (capturedWarnings
        && (type == QtWarningMsg || type == QtCriticalMsg))
    {
        capturedWarnings->append(message);
    }
}

class ScopedWarningCapture final
{
public:
    explicit ScopedWarningCapture(QVector<QString>& warnings)
        : m_previous(qInstallMessageHandler(captureQtWarning))
    {
        capturedWarnings = &warnings;
    }

    ~ScopedWarningCapture()
    {
        qInstallMessageHandler(m_previous);
        capturedWarnings = nullptr;
    }

    ScopedWarningCapture(const ScopedWarningCapture&) = delete;
    ScopedWarningCapture& operator=(const ScopedWarningCapture&) = delete;

private:
    QtMessageHandler m_previous = nullptr;
};

class ScopedPromptService final
{
public:
    explicit ScopedPromptService(IUserPromptService& service)
    {
        DialogServices::setUserPromptServiceForTesting(&service);
    }

    ~ScopedPromptService()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }

    ScopedPromptService(const ScopedPromptService&) = delete;
    ScopedPromptService& operator=(const ScopedPromptService&) = delete;
};

QString documentResourceBaselineDirectory()
{
    return QStringLiteral(CLASSMNGR_TEST_DOCUMENT_BASELINE_PACK_DIR);
}

QString missingDocumentResourceBaselineDirectory()
{
    return QStringLiteral(CLASSMNGR_TEST_DOCUMENT_MISSING_RESOURCE_PACK_DIR);
}

}

class DocumentCatalogNavigationParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void canceledRouteReleasesPreflightLeaseAndPreservesDirtySource();
    void discardedRouteLoadsRequestedPdfAndCapabilities();
    void missingResourceReturnsBeforePromptAndPreservesDirtySource();
};

void DocumentCatalogNavigationParityTests::
canceledRouteReleasesPreflightLeaseAndPreservesDirtySource()
{
    QTemporaryDir databaseDirectory;
    QVERIFY(databaseDirectory.isValid());
    QTemporaryDir resourceStorage;
    QVERIFY(resourceStorage.isValid());
    ResourcePackManager resources(
        resourceStorage.filePath(QStringLiteral("storage")),
        documentResourceBaselineDirectory()
        );
    QVERIFY(!resources.isMounted(QStringLiteral("documents")));
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(databaseDirectory)));

    QString error;
    PageManager pages;
    const auto source = prepareDirtyTeacherPage(services, pages, &error);
    QVERIFY2(source.has_value(), qPrintable(error));

    Sidebar sidebar;
    const auto projection = projectSidebarDocuments(services, sidebar, &error);
    QVERIFY2(projection.has_value(), qPrintable(error));
    const auto requestedDocument = std::find_if(
        projection->documents().cbegin(),
        projection->documents().cend(),
        [](const auto& document)
        {
            return document.id.value() == RequestedDocumentId;
        }
        );
    QVERIFY(requestedDocument != projection->documents().cend());
    QCOMPARE(
        QString::fromUtf8(requestedDocument->contentReference.value().c_str()),
        QString::fromLatin1(ExpectedDocumentReference)
        );

    NavigationController navigation(&services, &sidebar, &pages, resources);
    const auto route = clickDocumentLeaf(
        sidebar,
        QString::fromLatin1(RequestedDocumentId),
        &error
        );
    QVERIFY2(route.has_value(), qPrintable(error));
    verifyDocumentRoute(*route, QString::fromLatin1(RequestedDocumentId));

    FakeUserPromptService prompts;
    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Cancel
        );
    ScopedPromptService promptOverride(prompts);
    QVector<QString> warnings;
    {
        ScopedWarningCapture warningCapture(warnings);
        navigation.handleNavigation(*route);
    }

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    verifyNoOtherPrompts(prompts);
    QVERIFY2(warnings.isEmpty(),
        qPrintable(warnings.isEmpty() ? QString() : warnings.constFirst()));
    verifyDirtyTeacherUnchanged(pages, *source);
    QVERIFY(!pages.isPageInstantiated(PageType::PdfViewer));
    QVERIFY(pages.pdfViewerPage() == nullptr);
    QVERIFY(!resources.isMounted(QStringLiteral("documents")));
    QVERIFY(resources.activeRoot(QStringLiteral("documents")).isEmpty());
}

void DocumentCatalogNavigationParityTests::
discardedRouteLoadsRequestedPdfAndCapabilities()
{
    QTemporaryDir databaseDirectory;
    QVERIFY(databaseDirectory.isValid());
    QTemporaryDir resourceStorage;
    QVERIFY(resourceStorage.isValid());
    ResourcePackManager resources(
        resourceStorage.filePath(QStringLiteral("storage")),
        documentResourceBaselineDirectory()
        );
    QVERIFY(!resources.isMounted(QStringLiteral("documents")));
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(databaseDirectory)));

    QString error;
    PageManager pages;
    const auto source = prepareDirtyTeacherPage(services, pages, &error);
    QVERIFY2(source.has_value(), qPrintable(error));

    Sidebar sidebar;
    const auto projection = projectSidebarDocuments(services, sidebar, &error);
    QVERIFY2(projection.has_value(), qPrintable(error));
    const auto requestedDocument = std::find_if(
        projection->documents().cbegin(),
        projection->documents().cend(),
        [](const auto& document)
        {
            return document.id.value() == RequestedDocumentId;
        }
        );
    QVERIFY(requestedDocument != projection->documents().cend());
    QCOMPARE(
        QString::fromUtf8(requestedDocument->contentReference.value().c_str()),
        QString::fromLatin1(ExpectedDocumentReference)
        );
    QVERIFY(requestedDocument->printable);
    QVERIFY(requestedDocument->exportable);

    NavigationController navigation(&services, &sidebar, &pages, resources);
    const auto route = clickDocumentLeaf(
        sidebar,
        QString::fromLatin1(RequestedDocumentId),
        &error
        );
    QVERIFY2(route.has_value(), qPrintable(error));
    verifyDocumentRoute(*route, QString::fromLatin1(RequestedDocumentId));

    FakeUserPromptService prompts;
    prompts.scriptedUnsavedChangesChoices.enqueue(
        UnsavedChangesChoice::Discard
        );
    ScopedPromptService promptOverride(prompts);
    QVector<QString> warnings;
    {
        ScopedWarningCapture warningCapture(warnings);
        navigation.handleNavigation(*route);
    }

    QCOMPARE(prompts.unsavedChangesConfirmations.size(), 1);
    verifyNoOtherPrompts(prompts);
    QVERIFY2(warnings.isEmpty(),
        qPrintable(warnings.isEmpty() ? QString() : warnings.constFirst()));
    QVERIFY(pages.isCurrentPage(PageType::PdfViewer));
    PdfViewerPage* const viewer = pages.pdfViewerPage();
    QVERIFY(viewer);
    QVERIFY(resources.isMounted(QStringLiteral("documents")));

    QTRY_VERIFY(viewer->hasLoadedDocument());
    QTRY_VERIFY(
        viewer->documentContentSnapshot().phase()
            == ClassMngr::Next::Application::DocumentContentPhase::Ready
        );
    QVERIFY(viewer->documentContentSnapshot().reference().has_value());
    const QString expectedPdfPath = QDir(
        resources.activeRoot(QStringLiteral("documents"))
        ).filePath(QString::fromLatin1(ExpectedDocumentRelativePath));
    QCOMPARE(viewer->currentFilePath(), expectedPdfPath);
    QCOMPARE(
        QString::fromUtf8(
            viewer->documentContentSnapshot().reference()->value().c_str()
            ),
        QString::fromLatin1(ExpectedDocumentReference)
        );
    QVERIFY(viewer->outputCapabilities().printEnabled);
    QVERIFY(viewer->outputCapabilities().saveAsEnabled);

    QCOMPARE(pages.teacherPage(), source->page);
    QCOMPARE(source->page->teacher().id, source->persistedTeacher.id);
    QCOMPARE(source->page->teacher().teacherKr,
        source->persistedTeacher.teacherKr);
    QCOMPARE(source->page->teacher().teacherEn,
        source->persistedTeacher.teacherEn);
    QCOMPARE(source->page->teacher().preferredRomanization,
        source->persistedTeacher.preferredRomanization);
    QCOMPARE(source->page->teacher().preferredName,
        source->persistedTeacher.preferredName);
    QCOMPARE(source->notes->toPlainText(),
        QString::fromLatin1(PersistedTeacherNotes));
    QVERIFY(!source->page->hasUnsavedChanges());
}

void DocumentCatalogNavigationParityTests::
missingResourceReturnsBeforePromptAndPreservesDirtySource()
{
    QTemporaryDir databaseDirectory;
    QVERIFY(databaseDirectory.isValid());
    QTemporaryDir resourceStorage;
    QVERIFY(resourceStorage.isValid());
    ResourcePackManager resources(
        resourceStorage.filePath(QStringLiteral("storage")),
        missingDocumentResourceBaselineDirectory()
        );
    QVERIFY(!resources.isMounted(QStringLiteral("documents")));
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(databaseDirectory)));

    QString error;
    PageManager pages;
    const auto source = prepareDirtyTeacherPage(services, pages, &error);
    QVERIFY2(source.has_value(), qPrintable(error));

    Sidebar sidebar;
    const auto projection = projectSidebarDocuments(services, sidebar, &error);
    QVERIFY2(projection.has_value(), qPrintable(error));
    const auto requestedDocument = std::find_if(
        projection->documents().cbegin(),
        projection->documents().cend(),
        [](const auto& document)
        {
            return document.id.value() == RequestedDocumentId;
        }
        );
    QVERIFY(requestedDocument != projection->documents().cend());
    QCOMPARE(
        QString::fromUtf8(requestedDocument->contentReference.value().c_str()),
        QString::fromLatin1(ExpectedDocumentReference)
        );

    auto resourceProbe = resources.acquire(QStringLiteral("documents"));
    QVERIFY2(resourceProbe.has_value(),
        "The test documents pack with catalog metadata should mount.");
    QVERIFY(QFile::exists(
        QDir(resourceProbe->root()).filePath(QStringLiteral("documents.json"))
        ));
    QVERIFY(!QFile::exists(
        QDir(resourceProbe->root()).filePath(
            QString::fromLatin1(ExpectedDocumentRelativePath)
            )
        ));
    resourceProbe->reset();
    QVERIFY(!resources.isMounted(QStringLiteral("documents")));

    NavigationController navigation(&services, &sidebar, &pages, resources);
    const auto route = clickDocumentLeaf(
        sidebar,
        QString::fromLatin1(RequestedDocumentId),
        &error
        );
    QVERIFY2(route.has_value(), qPrintable(error));
    verifyDocumentRoute(*route, QString::fromLatin1(RequestedDocumentId));

    FakeUserPromptService prompts;
    ScopedPromptService promptOverride(prompts);
    QVector<QString> warnings;
    {
        ScopedWarningCapture warningCapture(warnings);
        navigation.handleNavigation(*route);
    }

    QVERIFY(prompts.unsavedChangesConfirmations.isEmpty());
    verifyNoOtherPrompts(prompts);
    QVERIFY2(warnings.isEmpty(),
        qPrintable(warnings.isEmpty() ? QString() : warnings.constFirst()));
    verifyDirtyTeacherUnchanged(pages, *source);
    QVERIFY(!pages.isPageInstantiated(PageType::PdfViewer));
    QVERIFY(pages.pdfViewerPage() == nullptr);
    QVERIFY(!resources.isMounted(QStringLiteral("documents")));
    QVERIFY(resources.activeRoot(QStringLiteral("documents")).isEmpty());
}

QTEST_MAIN(DocumentCatalogNavigationParityTests)

#include "document_catalog_navigation_parity_tests.moc"
