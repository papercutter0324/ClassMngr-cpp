#include "next/application/document_catalog_projection.h"
#include "next/platform/resource_document_catalog_port.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_diagnostics.h"
#include "next/resources/resource_loader.h"
#include "next/ui/application_shell.h"
#include "next/ui/document_catalog_navigation.h"
#include "next/ui/page_host.h"
#include "next/ui/preferences_dialog.h"
#include "preferences_test_ports.h"

#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QMenuBar>
#include <QTimer>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QTranslator>
#include <QtTest/QtTest>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Ui;

namespace
{

Domain::DocumentId documentId(const char* value)
{
    return *Domain::DocumentId::fromString(value);
}

Domain::DocumentFolderId folderId(const char* value)
{
    return *Domain::DocumentFolderId::fromString(value);
}

Application::DocumentCatalogProjection projectionForLocale(
    const QString& locale
    )
{
    using Application::DocumentCatalogPath;
    using Application::DocumentEntryKey;
    using Application::DocumentFolderKey;
    using Application::DocumentFolderOrder;
    using Application::DocumentOrder;
    using Application::DocumentCatalogProjectionInput;

    const bool korean = locale.startsWith(QStringLiteral("ko"));
    DocumentCatalogProjectionInput input;
    input.folders = {
        {
            folderId("folder-zulu"),
            DocumentCatalogPath("Zulu"),
            DocumentFolderKey("folder-zulu"),
            korean ? "줄루" : "Zulu",
            DocumentFolderOrder(20),
            DocumentCatalogPath("")
        },
        {
            folderId("folder-alpha-nested"),
            DocumentCatalogPath("Alpha/Nested"),
            DocumentFolderKey("folder-alpha-nested"),
            korean ? "중첩" : "Nested",
            DocumentFolderOrder(10),
            DocumentCatalogPath("Alpha")
        },
        {
            folderId("folder-alpha"),
            DocumentCatalogPath("Alpha"),
            DocumentFolderKey("folder-alpha"),
            korean ? "알파" : "Alpha",
            DocumentFolderOrder(20),
            DocumentCatalogPath("")
        }
    };
    input.documents = {
        {
            documentId("document-zulu"),
            folderId("folder-alpha"),
            DocumentCatalogPath("Alpha/zulu.pdf"),
            DocumentEntryKey("document-zulu"),
            korean ? "제브라" : "Zulu document",
            DocumentOrder(15),
            false,
            true,
            Domain::DocumentContentReference("resource://documents/Alpha/zulu.pdf"),
            Domain::DocumentContentReference("resource://documents/Alpha/zulu.docx")
        },
        {
            documentId("document-beta"),
            folderId("folder-zulu"),
            DocumentCatalogPath("Zulu/beta.pdf"),
            DocumentEntryKey("document-beta"),
            korean ? "베타" : "Beta document",
            DocumentOrder(1),
            true,
            false,
            Domain::DocumentContentReference("resource://documents/Zulu/beta.pdf"),
            std::nullopt
        },
        {
            documentId("document-inner"),
            folderId("folder-alpha-nested"),
            DocumentCatalogPath("Alpha/Nested/inner.pdf"),
            DocumentEntryKey("document-inner"),
            korean ? "내부 문서" : "Inner document",
            DocumentOrder(0),
            true,
            true,
            Domain::DocumentContentReference(
                "resource://documents/Alpha/Nested/inner.pdf"
                ),
            Domain::DocumentContentReference(
                "resource://documents/Alpha/Nested/inner.docx"
                )
        },
        {
            documentId("document-alpha"),
            folderId("folder-alpha"),
            DocumentCatalogPath("Alpha/alpha.pdf"),
            DocumentEntryKey("document-alpha"),
            korean ? "알파 문서" : "Alpha document",
            DocumentOrder(10),
            true,
            false,
            Domain::DocumentContentReference("resource://documents/Alpha/alpha.pdf"),
            std::nullopt
        }
    };

    auto result = Application::DocumentCatalogProjection::create(std::move(input));
    if (!result)
    {
        qFatal("Invalid document catalog test projection: %s",
               result.error().message.c_str());
    }
    return std::move(result.value());
}

QTreeWidgetItem* findItemByRole(
    QTreeWidget* tree,
    const DocumentCatalogItemRole role,
    const QString& value
    )
{
    if (!tree)
    {
        return nullptr;
    }

    QTreeWidgetItemIterator iterator(tree);
    while (*iterator)
    {
        if ((*iterator)->data(0, static_cast<int>(role)).toString() == value)
        {
            return *iterator;
        }
        ++iterator;
    }
    return nullptr;
}

QTreeWidgetItem* findCatalogRoot(QTreeWidget* tree)
{
    if (!tree)
    {
        return nullptr;
    }

    for (int index = 0; index < tree->topLevelItemCount(); ++index)
    {
        QTreeWidgetItem* const item = tree->topLevelItem(index);
        if (item->data(0, static_cast<int>(DocumentCatalogItemRole::Kind)).toInt()
            == static_cast<int>(DocumentCatalogItemKind::Root))
        {
            return item;
        }
    }
    return nullptr;
}

Domain::OperationError failure(QString message)
{
    return {
        .code = Domain::ErrorCode::Technical,
        .message = message.toStdString(),
        .recoverable = true
    };
}

class DialogPreferencesStore final
    : public Application::PreferencesStateReadPort,
      public Application::PreferencesSnapshotPersistPort
{
public:
    mutable Application::UserPreferencesState stored;
    mutable Application::PreferencesModel model{*this, *this};
    mutable bool failRead = false;
    mutable bool failPersist = false;
    mutable int readCalls = 0;
    mutable int persistCalls = 0;
    mutable std::vector<Application::UserPreferencesSnapshot> writes;

    [[nodiscard]] Application::PreferencesStateReadResult read() const override
    {
        ++readCalls;
        if (failRead)
        {
            return Application::PreferencesStateReadResult::failure(
                failure(QStringLiteral("injected preferences read failure")));
        }
        return Application::PreferencesStateReadResult::success(stored);
    }

    [[nodiscard]] Application::PreferencesPersistResult persist(
        const Application::UserPreferencesSnapshot& snapshot
        ) const override
    {
        ++persistCalls;
        writes.push_back(snapshot);
        if (failPersist)
        {
            return Application::PreferencesPersistResult::failure(
                failure(QStringLiteral("injected preferences save failure")));
        }
        Application::UserPreferencesState candidate = stored;
        static_cast<void>(candidate.setThemePreference(snapshot.themePreference()));
        static_cast<void>(candidate.setLanguagePreference(snapshot.languagePreference()));
        static_cast<void>(candidate.setFontSize(snapshot.fontSize()));
        static_cast<void>(candidate.setAutomaticUpdateChecksEnabled(
            snapshot.automaticUpdateChecksEnabled()));
        static_cast<void>(candidate.setSidebarTooltipsEnabled(
            snapshot.sidebarTooltipsEnabled()));
        static_cast<void>(candidate.setSidebarMarqueeEnabled(
            snapshot.sidebarMarqueeEnabled()));
        static_cast<void>(candidate.setExcelImportTimeoutSeconds(
            snapshot.excelImportTimeoutSeconds()));
        static_cast<void>(candidate.setShowPowerPointDataAccessNotice(
            snapshot.showPowerPointDataAccessNotice()));
        stored = std::move(candidate);
        return Application::PreferencesPersistResult::success();
    }
};

template <typename Enum>
Domain::Result<void> validateEnum(const Enum value, const int count)
{
    const int raw = static_cast<int>(value);
    return raw >= 0 && raw < count
        ? Domain::Result<void>::success()
        : Domain::Result<void>::failure(failure(QStringLiteral("invalid setting value")));
}

class FakeSaveMode final : public Application::SaveModePreferencesPort
{
public:
    mutable Application::SaveMode value = Application::SaveMode::Automatic;
    mutable bool failWrite = false;
    mutable int reads = 0;
    mutable std::vector<Application::SaveMode> writes;
    [[nodiscard]] Application::SaveMode read() const override { ++reads; return value; }
    [[nodiscard]] Domain::Result<void> write(const Application::SaveMode next) const override
    {
        writes.push_back(next);
        if (failWrite) return Domain::Result<void>::failure(
            failure(QStringLiteral("injected save mode failure")));
        const auto valid = validateEnum(next, 2);
        if (valid) value = next;
        return valid;
    }
};

class FakeDocumentSpacing final : public Application::DocumentPageSpacingPreferencesPort
{
public:
    mutable Application::DocumentPageSpacing value = Application::DocumentPageSpacing::Small;
    mutable bool failWrite = false;
    mutable int reads = 0;
    mutable std::vector<Application::DocumentPageSpacing> writes;
    [[nodiscard]] Application::DocumentPageSpacing read() const override { ++reads; return value; }
    [[nodiscard]] Domain::Result<void> write(
        const Application::DocumentPageSpacing next) const override
    {
        writes.push_back(next);
        if (failWrite) return Domain::Result<void>::failure(
            failure(QStringLiteral("injected document spacing failure")));
        const auto valid = validateEnum(next, 4);
        if (valid) value = next;
        return valid;
    }
};

class FakeDocumentBackground final : public Application::DocumentViewerBackgroundPreferencesPort
{
public:
    mutable Application::DocumentViewerBackground value = Application::DocumentViewerBackground::Default;
    mutable bool failWrite = false;
    mutable int reads = 0;
    mutable std::vector<Application::DocumentViewerBackground> writes;
    [[nodiscard]] Application::DocumentViewerBackground read() const override { ++reads; return value; }
    [[nodiscard]] Domain::Result<void> write(
        const Application::DocumentViewerBackground next) const override
    {
        writes.push_back(next);
        if (failWrite) return Domain::Result<void>::failure(
            failure(QStringLiteral("injected document background failure")));
        const auto valid = validateEnum(next, 3);
        if (valid) value = next;
        return valid;
    }
};

class FakeAiProvider final : public Application::AiCommentProviderPreferencesPort
{
public:
    mutable Application::AiCommentProvider value = Application::AiCommentProvider::ChatGPT;
    mutable bool failWrite = false;
    mutable int reads = 0;
    mutable std::vector<Application::AiCommentProvider> writes;
    [[nodiscard]] Application::AiCommentProvider read() const override { ++reads; return value; }
    [[nodiscard]] Domain::Result<void> write(
        const Application::AiCommentProvider next) const override
    {
        writes.push_back(next);
        if (failWrite) return Domain::Result<void>::failure(
            failure(QStringLiteral("injected AI provider failure")));
        const auto valid = validateEnum(next, 5);
        if (valid) value = next;
        return valid;
    }
};

class FakeAiVoice final : public Application::AiCommentVoicePreferencesPort
{
public:
    mutable Application::AiCommentVoice value = Application::AiCommentVoice::DirectToStudent;
    mutable bool failWrite = false;
    mutable int reads = 0;
    mutable std::vector<Application::AiCommentVoice> writes;
    [[nodiscard]] Application::AiCommentVoice read() const override { ++reads; return value; }
    [[nodiscard]] Domain::Result<void> write(
        const Application::AiCommentVoice next
        ) const override
    {
        writes.push_back(next);
        if (failWrite) return Domain::Result<void>::failure(
            failure(QStringLiteral("injected AI voice failure")));
        const auto valid = validateEnum(next, 2);
        if (valid) value = next;
        return valid;
    }
};

class FakeCustomAiWebsite final : public Application::AiCommentCustomWebsitePort
{
public:
    mutable std::string value = "https://example.com";
    mutable bool failSave = false;
    mutable int reads = 0;
    mutable std::vector<std::string> writes;
    [[nodiscard]] std::string read() const override { ++reads; return value; }
    [[nodiscard]] Domain::Result<void> save(const std::string& next) const override
    {
        writes.push_back(next);
        if (failSave) return Domain::Result<void>::failure(
            failure(QStringLiteral("injected custom website failure")));
        value = next;
        return Domain::Result<void>::success();
    }
    void clear() const override { value.clear(); }
};

struct DialogFixture final
{
    DialogPreferencesStore preferences;
    FakeSaveMode saveMode;
    FakeDocumentSpacing documentSpacing;
    FakeDocumentBackground documentBackground;
    FakeAiProvider aiProvider;
    FakeAiVoice aiVoice;
    FakeCustomAiWebsite customWebsite;

    [[nodiscard]] Ui::PreferencesDialogPorts ports()
    {
        return {saveMode, documentSpacing, documentBackground,
                aiProvider, aiVoice, customWebsite};
    }
};

class NavigationTestPage final : public Ui::Page
{
public:
    using Ui::Page::Page;
};

}  // namespace

class DocumentCatalogNavigationTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void rendersNestedSiblingsByOrderThenStableId();
    void documentSelectionEmitsTypedStableIdAndPreservesFlags();
    void replacingLocalizedProjectionUpdatesLabelsAndKeepsSelection();
    void resourceCatalogConstructionLoadsMetadataWithoutBodyDiagnostics();
    void preferencesMenuIsDiscoverableAndDoesNotDuplicatePageSettings();
    void preferencesDialogLoadsEditsAndPersistsEveryMigratedSetting();
    void preferencesDialogCancellationDoesNotWrite();
    void preferencesDialogShowsLoadAndSaveFailuresAndRollsBackControls();
    void preferencesDialogRejectsInvalidBoundariesAndRestoresAiUrl();
    void languageChangeRetranslatesPreferencesShellAndNavigation();
    void shellAppliesSidebarAndFontPreferencesImmediately();

private:
    Test::MemoryPreferences m_preferences;
    std::unique_ptr<Resources::ResourceLoader> m_loader;
};

void DocumentCatalogNavigationTests::initTestCase()
{
    const auto catalog = Resources::ResourceCatalog::load(
        QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)
        );
    QVERIFY2(catalog.has_value(), catalog ? "" : qPrintable(catalog.error().message));
    m_loader = std::make_unique<Resources::ResourceLoader>(std::move(*catalog));
}

void DocumentCatalogNavigationTests::rendersNestedSiblingsByOrderThenStableId()
{
    Ui::ApplicationShell shell(*m_loader, m_preferences.model);
    shell.setDocumentCatalogProjection(projectionForLocale(QStringLiteral("en_US")));
    auto* const tree = shell.findChild<QTreeWidget*>(
        QStringLiteral("classMngrNextNavigationTree")
        );
    QVERIFY(tree);

    QTreeWidgetItem* const root = findCatalogRoot(tree);
    QVERIFY(root);
    QCOMPARE(root->text(0), QStringLiteral("Documents"));
    QCOMPARE(root->childCount(), 2);
    QCOMPARE(root->child(0)->text(0), QStringLiteral("Alpha"));
    QCOMPARE(root->child(1)->text(0), QStringLiteral("Zulu"));

    QTreeWidgetItem* const alpha = root->child(0);
    QCOMPARE(alpha->childCount(), 3);
    QCOMPARE(alpha->child(0)->text(0), QStringLiteral("Alpha document"));
    QCOMPARE(
        alpha->child(1)->data(
            0,
            static_cast<int>(DocumentCatalogItemRole::Kind)
            ).toInt(),
        static_cast<int>(DocumentCatalogItemKind::Folder)
        );
    QCOMPARE(alpha->child(1)->text(0), QStringLiteral("Nested"));
    QCOMPARE(alpha->child(2)->text(0), QStringLiteral("Zulu document"));
    QCOMPARE(alpha->child(1)->childCount(), 1);
    QCOMPARE(alpha->child(1)->child(0)->text(0), QStringLiteral("Inner document"));
    QCOMPARE(
        Ui::DocumentCatalogNavigation::documentIdFor(alpha->child(1)),
        std::nullopt
        );
}

void DocumentCatalogNavigationTests::
documentSelectionEmitsTypedStableIdAndPreservesFlags()
{
    Ui::ApplicationShell shell(*m_loader, m_preferences.model);
    shell.setDocumentCatalogProjection(projectionForLocale(QStringLiteral("en_US")));
    std::vector<Domain::DocumentId> selectedIds;
    shell.setDocumentCatalogSelectionHandler(
        [&selectedIds](const Domain::DocumentId& id) {
            selectedIds.push_back(id);
        }
        );

    auto* const tree = shell.findChild<QTreeWidget*>(
        QStringLiteral("classMngrNextNavigationTree")
        );
    QVERIFY(tree);
    QTreeWidgetItem* const selectedItem = findItemByRole(
        tree,
        DocumentCatalogItemRole::DocumentId,
        QStringLiteral("document-alpha")
        );
    QVERIFY(selectedItem);
    QCOMPARE(
        selectedItem->data(
            0,
            static_cast<int>(DocumentCatalogItemRole::Kind)
            ).toInt(),
        static_cast<int>(DocumentCatalogItemKind::Document)
        );
    QVERIFY(selectedItem->data(
        0,
        static_cast<int>(DocumentCatalogItemRole::Printable)
        ).toBool());
    QVERIFY(!selectedItem->data(
        0,
        static_cast<int>(DocumentCatalogItemRole::Exportable)
        ).toBool());

    const QTreeWidgetItem* const exportable = findItemByRole(
        tree,
        DocumentCatalogItemRole::DocumentId,
        QStringLiteral("document-zulu")
        );
    QVERIFY(exportable);
    QVERIFY(!exportable->data(
        0,
        static_cast<int>(DocumentCatalogItemRole::Printable)
        ).toBool());
    QVERIFY(exportable->data(
        0,
        static_cast<int>(DocumentCatalogItemRole::Exportable)
        ).toBool());

    const QTreeWidgetItem* const printableAndExportable = findItemByRole(
        tree,
        DocumentCatalogItemRole::DocumentId,
        QStringLiteral("document-inner")
        );
    QVERIFY(printableAndExportable);
    QVERIFY(printableAndExportable->data(
        0,
        static_cast<int>(DocumentCatalogItemRole::Printable)
        ).toBool());
    QVERIFY(printableAndExportable->data(
        0,
        static_cast<int>(DocumentCatalogItemRole::Exportable)
        ).toBool());

    tree->setCurrentItem(selectedItem);
    QCOMPARE(selectedIds.size(), std::size_t(1));
    QCOMPARE(selectedIds.front(), documentId("document-alpha"));
    QCOMPARE(
        Ui::DocumentCatalogNavigation::documentIdFor(tree->currentItem()),
        std::optional<Domain::DocumentId>(documentId("document-alpha"))
        );
}

void DocumentCatalogNavigationTests::
replacingLocalizedProjectionUpdatesLabelsAndKeepsSelection()
{
    Ui::ApplicationShell shell(*m_loader, m_preferences.model);
    shell.setDocumentCatalogProjection(projectionForLocale(QStringLiteral("en_US")));
    QString requestedLocale;
    shell.setDocumentCatalogProjectionLoader(
        [&requestedLocale](const QString& locale) {
            requestedLocale = locale;
            return Domain::Result<Application::DocumentCatalogProjection>::success(
                projectionForLocale(locale)
                );
        }
        );
    auto* const tree = shell.findChild<QTreeWidget*>(
        QStringLiteral("classMngrNextNavigationTree")
        );
    QVERIFY(tree);
    QTreeWidgetItem* selectedItem = findItemByRole(
        tree,
        DocumentCatalogItemRole::DocumentId,
        QStringLiteral("document-alpha")
        );
    QVERIFY(selectedItem);
    tree->setCurrentItem(selectedItem);

    const auto refreshed = shell.refreshDocumentCatalogProjection(
        QStringLiteral("ko_KR")
        );
    QVERIFY(refreshed);
    QCOMPARE(requestedLocale, QStringLiteral("ko_KR"));
    QCOMPARE(
        Ui::DocumentCatalogNavigation::documentIdFor(tree->currentItem()),
        std::optional<Domain::DocumentId>(documentId("document-alpha"))
        );
    QCOMPARE(tree->currentItem()->text(0), QStringLiteral("알파 문서"));
    QCOMPARE(findCatalogRoot(tree)->child(0)->text(0), QStringLiteral("알파"));
    QTreeWidgetItemIterator iterator(tree);
    while (*iterator)
    {
        QVERIFY((*iterator)->text(0) != QStringLiteral("Alpha document"));
        ++iterator;
    }
}

void DocumentCatalogNavigationTests::
resourceCatalogConstructionLoadsMetadataWithoutBodyDiagnostics()
{
    const auto catalog = Resources::ResourceCatalog::load(
        QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT)
        );
    QVERIFY2(catalog.has_value(), catalog ? "" : qPrintable(catalog.error().message));

    std::vector<Resources::ResourceDiagnostic> diagnostics;
    Resources::ResourceLoader loader(
        std::move(*catalog),
        [&diagnostics](const Resources::ResourceDiagnostic& diagnostic) {
            diagnostics.push_back(diagnostic);
        }
        );
    const Platform::ResourceDocumentCatalogPort port(loader);
    const auto projection = port.projection(QStringLiteral("en_US"));
    QVERIFY(projection);
    QVERIFY(!projection.value().folders().empty());
    QVERIFY(!projection.value().documents().empty());
    QCOMPARE(diagnostics.size(), std::size_t(1));
    QCOMPARE(
        diagnostics.front().resourceId.value(),
        QStringLiteral("document.catalog-metadata")
        );
    QVERIFY(diagnostics.front().succeeded);
}

void DocumentCatalogNavigationTests::
preferencesMenuIsDiscoverableAndDoesNotDuplicatePageSettings()
{
    DialogFixture fixture;
    Ui::ApplicationShell shell(*m_loader, fixture.preferences.model);
    int openCalls = 0;
    shell.setPreferencesHandler([&openCalls] { ++openCalls; });

    auto* const editMenu = shell.findChild<QMenu*>(
        QStringLiteral("classMngrNextEditMenu"));
    auto* const fileMenu = shell.findChild<QMenu*>(
        QStringLiteral("classMngrNextFileMenu"));
    auto* const action = shell.findChild<QAction*>(
        QStringLiteral("classMngrNextPreferences"));
    QVERIFY(editMenu);
    QVERIFY(fileMenu);
    QVERIFY(action);
    QVERIFY(editMenu->actions().contains(action));
    QVERIFY(!fileMenu->actions().contains(action));
    QCOMPARE(action->text(), QStringLiteral("Preferences…"));
    action->trigger();
    QCOMPARE(openCalls, 1);

    Ui::PreferencesDialog dialog(fixture.preferences.model, fixture.ports());
    QVERIFY(!dialog.findChild<QWidget*>(QStringLiteral("preferencesSchedule")));
    QVERIFY(!dialog.findChild<QWidget*>(QStringLiteral("preferencesCalendar")));
    QVERIFY(!dialog.findChild<QWidget*>(QStringLiteral("preferencesNavigationBar")));
}

void DocumentCatalogNavigationTests::
preferencesDialogLoadsEditsAndPersistsEveryMigratedSetting()
{
    DialogFixture fixture;
    QVERIFY(fixture.preferences.stored.setThemePreference(
        Application::ThemePreference::SystemDefault));
    QVERIFY(fixture.preferences.stored.setLanguagePreference(
        Application::LanguagePreference::SystemDefault));
    QVERIFY(fixture.preferences.stored.setFontSize(Application::FontSize::Normal));
    std::optional<Application::SaveMode> liveSaveMode;
    Ui::PreferencesDialog dialog(
        fixture.preferences.model,
        fixture.ports(),
        {.saveModeChanged = [&liveSaveMode](const Application::SaveMode mode) {
            liveSaveMode = mode;
        }});
    QVERIFY(dialog.applicationPreferencesLoaded());
    QCOMPARE(fixture.preferences.readCalls, 1);
    QCOMPARE(fixture.saveMode.reads, 1);
    QCOMPARE(fixture.documentSpacing.reads, 1);
    QCOMPARE(fixture.documentBackground.reads, 1);
    QCOMPARE(fixture.aiProvider.reads, 1);
    QCOMPARE(fixture.aiVoice.reads, 1);
    QCOMPARE(fixture.customWebsite.reads, 1);

    const auto combo = [&dialog](const char* name) {
        return dialog.findChild<QComboBox*>(QString::fromLatin1(name));
    };
    auto* const theme = combo("preferencesTheme");
    auto* const language = combo("preferencesLanguage");
    auto* const fontSize = combo("preferencesFontSize");
    auto* const saveMode = combo("preferencesSaveMode");
    auto* const timeout = combo("preferencesExcelImportTimeout");
    auto* const spacing = combo("preferencesDocumentSpacing");
    auto* const background = combo("preferencesDocumentBackground");
    auto* const provider = combo("preferencesAiCommentProvider");
    auto* const voice = combo("preferencesAiCommentVoice");
    auto* const updates = dialog.findChild<QCheckBox*>(QStringLiteral("preferencesAutomaticUpdates"));
    auto* const tooltips = dialog.findChild<QCheckBox*>(QStringLiteral("preferencesSidebarTooltips"));
    auto* const animation = dialog.findChild<QCheckBox*>(QStringLiteral("preferencesSidebarAnimation"));
    QVERIFY(theme && language && fontSize && saveMode && timeout && spacing
            && background && provider && voice && updates && tooltips && animation);

    theme->setCurrentIndex(theme->findData(static_cast<int>(Application::ThemePreference::Dark)));
    QCOMPARE(fixture.preferences.model.snapshot().themePreference(), Application::ThemePreference::Dark);
    language->setCurrentIndex(language->findData(static_cast<int>(Application::LanguagePreference::Korean)));
    QCOMPARE(fixture.preferences.model.snapshot().languagePreference(), Application::LanguagePreference::Korean);
    fontSize->setCurrentIndex(fontSize->findData(static_cast<int>(Application::FontSize::ExtraLarge)));
    QCOMPARE(fixture.preferences.model.snapshot().fontSize(), Application::FontSize::ExtraLarge);
    updates->setChecked(false);
    tooltips->setChecked(false);
    animation->setChecked(false);
    timeout->setCurrentIndex(timeout->findData(300));
    QCOMPARE(fixture.preferences.model.snapshot().automaticUpdateChecksEnabled(), false);
    QCOMPARE(fixture.preferences.model.snapshot().sidebarTooltipsEnabled(), false);
    QCOMPARE(fixture.preferences.model.snapshot().sidebarMarqueeEnabled(), false);
    QCOMPARE(fixture.preferences.model.snapshot().excelImportTimeoutSeconds(), 300);
#ifdef Q_OS_MACOS
    auto* const powerPointNotice = dialog.findChild<QCheckBox*>(
        QStringLiteral("preferencesPowerPointNotice"));
    QVERIFY(powerPointNotice);
    powerPointNotice->setChecked(false);
    QCOMPARE(fixture.preferences.model.snapshot().showPowerPointDataAccessNotice(), false);
#endif

    saveMode->setCurrentIndex(saveMode->findData(static_cast<int>(Application::SaveMode::Manual)));
    QCOMPARE(fixture.saveMode.value, Application::SaveMode::Manual);
    QCOMPARE(liveSaveMode, std::optional<Application::SaveMode>(Application::SaveMode::Manual));
    spacing->setCurrentIndex(spacing->findData(static_cast<int>(Application::DocumentPageSpacing::Large)));
    QCOMPARE(fixture.documentSpacing.value, Application::DocumentPageSpacing::Large);
    background->setCurrentIndex(background->findData(static_cast<int>(Application::DocumentViewerBackground::Black)));
    QCOMPARE(fixture.documentBackground.value, Application::DocumentViewerBackground::Black);
    provider->setCurrentIndex(provider->findData(static_cast<int>(Application::AiCommentProvider::Gemini)));
    QCOMPARE(fixture.aiProvider.value, Application::AiCommentProvider::Gemini);
    voice->setCurrentIndex(voice->findData(static_cast<int>(Application::AiCommentVoice::ThirdPerson)));
    QCOMPARE(fixture.aiVoice.value, Application::AiCommentVoice::ThirdPerson);

    QTimer::singleShot(0, [] {
        auto* const input = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (!input) return;
        if (auto* const line = input->findChild<QLineEdit*>())
        {
            line->setText(QStringLiteral("https://school.example/ai"));
        }
        input->accept();
    });
    provider->setCurrentIndex(provider->findData(
        static_cast<int>(Application::AiCommentProvider::CustomWebsite)));
    QCOMPARE(fixture.aiProvider.value, Application::AiCommentProvider::CustomWebsite);
    QCOMPARE(fixture.customWebsite.value, std::string("https://school.example/ai"));

    QVERIFY(fixture.preferences.writes.size() >= 7);
    QCOMPARE(fixture.preferences.writes.back().fontSize(), Application::FontSize::ExtraLarge);
    QCOMPARE(fixture.saveMode.writes.size(), std::size_t(1));
    QCOMPARE(fixture.documentSpacing.writes.size(), std::size_t(1));
    QCOMPARE(fixture.documentBackground.writes.size(), std::size_t(1));
    QCOMPARE(fixture.aiProvider.writes.size(), std::size_t(2));
    QCOMPARE(fixture.aiVoice.writes.size(), std::size_t(1));

    Application::PreferencesModel reloaded(fixture.preferences, fixture.preferences);
    QVERIFY(reloaded.load());
    Ui::PreferencesDialog secondDialog(reloaded, fixture.ports());
    QCOMPARE(reloaded.snapshot().themePreference(), Application::ThemePreference::Dark);
    QCOMPARE(reloaded.snapshot().languagePreference(), Application::LanguagePreference::Korean);
    QCOMPARE(reloaded.snapshot().fontSize(), Application::FontSize::ExtraLarge);
    QCOMPARE(reloaded.snapshot().automaticUpdateChecksEnabled(), false);
    QCOMPARE(reloaded.snapshot().sidebarTooltipsEnabled(), false);
    QCOMPARE(reloaded.snapshot().sidebarMarqueeEnabled(), false);
    QCOMPARE(reloaded.snapshot().excelImportTimeoutSeconds(), 300);
    QCOMPARE(fixture.saveMode.value, Application::SaveMode::Manual);
    QCOMPARE(fixture.documentSpacing.value, Application::DocumentPageSpacing::Large);
    QCOMPARE(fixture.documentBackground.value, Application::DocumentViewerBackground::Black);
    QCOMPARE(fixture.aiProvider.value, Application::AiCommentProvider::CustomWebsite);
    QCOMPARE(fixture.aiVoice.value, Application::AiCommentVoice::ThirdPerson);
    QCOMPARE(fixture.customWebsite.value, std::string("https://school.example/ai"));
}

void DocumentCatalogNavigationTests::preferencesDialogCancellationDoesNotWrite()
{
    DialogFixture fixture;
    Ui::PreferencesDialog dialog(fixture.preferences.model, fixture.ports());
    QVERIFY(dialog.applicationPreferencesLoaded());
    dialog.reject();
    QCOMPARE(fixture.preferences.persistCalls, 0);
    QVERIFY(fixture.preferences.writes.empty());
    QVERIFY(fixture.saveMode.writes.empty());
    QVERIFY(fixture.documentSpacing.writes.empty());
    QVERIFY(fixture.documentBackground.writes.empty());
    QVERIFY(fixture.aiProvider.writes.empty());
    QVERIFY(fixture.aiVoice.writes.empty());
    QVERIFY(fixture.customWebsite.writes.empty());
}

void DocumentCatalogNavigationTests::
preferencesDialogShowsLoadAndSaveFailuresAndRollsBackControls()
{
    {
        DialogFixture fixture;
        fixture.preferences.failRead = true;
        Ui::PreferencesDialog dialog(fixture.preferences.model, fixture.ports());
        QVERIFY(!dialog.applicationPreferencesLoaded());
        auto* const error = dialog.findChild<QLabel*>(QStringLiteral("preferencesError"));
        auto* const theme = dialog.findChild<QComboBox*>(QStringLiteral("preferencesTheme"));
        QVERIFY(error && theme);
        QVERIFY(!error->isHidden());
        QVERIFY(!theme->isEnabled());
    }
    {
        DialogFixture fixture;
        Ui::PreferencesDialog dialog(fixture.preferences.model, fixture.ports());
        auto* const updates = dialog.findChild<QCheckBox*>(
            QStringLiteral("preferencesAutomaticUpdates"));
        auto* const error = dialog.findChild<QLabel*>(QStringLiteral("preferencesError"));
        QVERIFY(updates && error);
        const bool previous = fixture.preferences.model.snapshot()
            .automaticUpdateChecksEnabled();
        fixture.preferences.failPersist = true;
        updates->setChecked(!previous);
        QCOMPARE(updates->isChecked(), previous);
        QCOMPARE(fixture.preferences.model.snapshot()
            .automaticUpdateChecksEnabled(), previous);
        QVERIFY(fixture.preferences.model.isDirty());
        QVERIFY(!error->isHidden());
    }
    {
        DialogFixture fixture;
        Ui::PreferencesDialog dialog(fixture.preferences.model, fixture.ports());
        auto* const spacing = dialog.findChild<QComboBox*>(
            QStringLiteral("preferencesDocumentSpacing"));
        auto* const error = dialog.findChild<QLabel*>(QStringLiteral("preferencesError"));
        QVERIFY(spacing && error);
        const int previous = spacing->currentIndex();
        fixture.documentSpacing.failWrite = true;
        spacing->setCurrentIndex(spacing->findData(
            static_cast<int>(Application::DocumentPageSpacing::Large)));
        QCOMPARE(spacing->currentIndex(), previous);
        QVERIFY(!error->isHidden());
    }
    {
        DialogFixture fixture;
        Ui::PreferencesDialog dialog(fixture.preferences.model, fixture.ports());
        auto* const background = dialog.findChild<QComboBox*>(
            QStringLiteral("preferencesDocumentBackground"));
        auto* const error = dialog.findChild<QLabel*>(QStringLiteral("preferencesError"));
        QVERIFY(background && error);
        const int previous = background->currentIndex();
        fixture.documentBackground.failWrite = true;
        background->setCurrentIndex(background->findData(
            static_cast<int>(Application::DocumentViewerBackground::Black)));
        QCOMPARE(background->currentIndex(), previous);
        QCOMPARE(fixture.documentBackground.value,
            Application::DocumentViewerBackground::Default);
        QVERIFY(!error->isHidden());
    }
}

void DocumentCatalogNavigationTests::
preferencesDialogRejectsInvalidBoundariesAndRestoresAiUrl()
{
    DialogFixture fixture;
    Ui::PreferencesDialog dialog(fixture.preferences.model, fixture.ports());
    auto* const timeout = dialog.findChild<QComboBox*>(
        QStringLiteral("preferencesExcelImportTimeout"));
    auto* const theme = dialog.findChild<QComboBox*>(QStringLiteral("preferencesTheme"));
    auto* const provider = dialog.findChild<QComboBox*>(
        QStringLiteral("preferencesAiCommentProvider"));
    auto* const error = dialog.findChild<QLabel*>(QStringLiteral("preferencesError"));
    QVERIFY(timeout && theme && provider && error);

    const int previousTimeout = timeout->currentIndex();
    timeout->setItemData(0, 31);
    timeout->setCurrentIndex(0);
    QCOMPARE(timeout->currentIndex(), previousTimeout);
    QVERIFY(!error->isHidden());

    const int previousTheme = theme->currentIndex();
    theme->setItemData(1, 99);
    theme->setCurrentIndex(1);
    QCOMPARE(theme->currentIndex(), previousTheme);
    QVERIFY(!error->isHidden());

    const std::string originalUrl = fixture.customWebsite.value;
    QTimer::singleShot(0, [] {
        auto* const input = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (input) input->reject();
    });
    provider->setCurrentIndex(provider->findData(
        static_cast<int>(Application::AiCommentProvider::CustomWebsite)));
    QCOMPARE(fixture.customWebsite.value, originalUrl);
    QCOMPARE(fixture.customWebsite.writes.size(), std::size_t(0));
    QCOMPARE(fixture.aiProvider.value, Application::AiCommentProvider::ChatGPT);

    fixture.customWebsite.failSave = true;
    QTimer::singleShot(0, [] {
        auto* const input = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (!input) return;
        if (auto* const line = input->findChild<QLineEdit*>())
        {
            line->setText(QStringLiteral("https://storage-failure.example"));
        }
        input->accept();
    });
    provider->setCurrentIndex(provider->findData(
        static_cast<int>(Application::AiCommentProvider::CustomWebsite)));
    QCOMPARE(fixture.customWebsite.value, originalUrl);
    QCOMPARE(fixture.customWebsite.writes.size(), std::size_t(1));
    QCOMPARE(fixture.aiProvider.writes.size(), std::size_t(0));
    QCOMPARE(fixture.aiProvider.value, Application::AiCommentProvider::ChatGPT);
    QVERIFY(!error->isHidden());
    fixture.customWebsite.failSave = false;

    QTimer::singleShot(0, [] {
        auto* const input = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (!input) return;
        if (auto* const line = input->findChild<QLineEdit*>())
        {
            line->setText(QStringLiteral("http://invalid.example"));
        }
        input->accept();
    });
    provider->setCurrentIndex(provider->findData(
        static_cast<int>(Application::AiCommentProvider::CustomWebsite)));
    QCOMPARE(fixture.customWebsite.value, originalUrl);
    QCOMPARE(fixture.aiProvider.value, Application::AiCommentProvider::ChatGPT);
    QVERIFY(!error->isHidden());

    fixture.aiProvider.failWrite = true;
    QTimer::singleShot(0, [] {
        auto* const input = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (!input) return;
        if (auto* const line = input->findChild<QLineEdit*>())
        {
            line->setText(QStringLiteral("https://rollback.example"));
        }
        input->accept();
    });
    provider->setCurrentIndex(provider->findData(
        static_cast<int>(Application::AiCommentProvider::CustomWebsite)));
    QCOMPARE(fixture.customWebsite.value, originalUrl);
    QCOMPARE(fixture.aiProvider.value, Application::AiCommentProvider::ChatGPT);
    QCOMPARE(fixture.customWebsite.writes.size(), std::size_t(3));
    QCOMPARE(fixture.customWebsite.writes[0],
        std::string("https://storage-failure.example"));
    QCOMPARE(fixture.customWebsite.writes[1], std::string("https://rollback.example"));
    QCOMPARE(fixture.customWebsite.writes[2], originalUrl);
    QVERIFY(!error->isHidden());
    fixture.aiProvider.failWrite = false;

    QTimer::singleShot(0, [] {
        auto* const input = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (!input) return;
        if (auto* const line = input->findChild<QLineEdit*>())
        {
            line->setText(QStringLiteral("https://new.example"));
        }
        input->accept();
    });
    provider->setCurrentIndex(provider->findData(
        static_cast<int>(Application::AiCommentProvider::CustomWebsite)));
    QCOMPARE(fixture.aiProvider.value, Application::AiCommentProvider::CustomWebsite);
    QCOMPARE(fixture.customWebsite.value, std::string("https://new.example"));
    QCOMPARE(fixture.customWebsite.writes.size(), std::size_t(4));
}

void DocumentCatalogNavigationTests::
languageChangeRetranslatesPreferencesShellAndNavigation()
{
    DialogFixture fixture;
    Ui::ApplicationShell shell(*m_loader, fixture.preferences.model);
    Ui::PageDescriptor scheduleDescriptor{
        .id = *Ui::PageId::fromString(QStringLiteral("schedule")),
        .label = QStringLiteral("Schedule"),
        .labelTranslationSource = QStringLiteral("Schedule"),
        .factory = [](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(new NavigationTestPage(context));
        }
    };
    QVERIFY(shell.pageHost().registerPage(std::move(scheduleDescriptor)));
    const Ui::PageId scheduleId = *Ui::PageId::fromString(QStringLiteral("schedule"));
    QVERIFY(shell.configureNavigation(
        {Ui::NavigationNode::group(
            QStringLiteral("campus_info"),
            QStringLiteral("Campus Directory"),
            {Ui::NavigationNode::page(scheduleId)},
            Ui::NavigationWorkspaceAvailability::Always,
            false,
            QStringLiteral("Campus Directory"))},
        scheduleId,
        scheduleId,
        Application::WorkspaceLifecycleState::Closed));
    Ui::PreferencesDialog dialog(fixture.preferences.model, fixture.ports());
    auto* const tree = shell.findChild<QTreeWidget*>(
        QStringLiteral("classMngrNextNavigationTree"));
    auto* const action = shell.findChild<QAction*>(
        QStringLiteral("classMngrNextPreferences"));
    QVERIFY(tree && action);
    QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("Campus Directory"));
    QCOMPARE(tree->topLevelItem(0)->child(0)->text(0), QStringLiteral("Schedule"));

    QTranslator translator;
    const QString translationPath = QDir(QFileInfo(QString::fromUtf8(
        CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG)).absolutePath()).filePath(
            QStringLiteral("assets/translations/ClassMngr_ko_KR.qm"));
    QVERIFY(translator.load(translationPath));
    QCOMPARE(translator.translate("NextShell", "Preferences…"),
        QStringLiteral("환경 설정…"));
    QVERIFY(QCoreApplication::installTranslator(&translator));
    QCoreApplication::processEvents();
    QCOMPARE(action->text(), QStringLiteral("환경 설정…"));
    QCOMPARE(dialog.windowTitle(), QStringLiteral("환경 설정"));
    QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("캠퍼스 디렉터리"));
    QCOMPARE(tree->topLevelItem(0)->child(0)->text(0), QStringLiteral("일정"));
}

void DocumentCatalogNavigationTests::
shellAppliesSidebarAndFontPreferencesImmediately()
{
    DialogFixture fixture;
    QVERIFY(fixture.preferences.model.load());
    Ui::ApplicationShell shell(*m_loader, fixture.preferences.model);
    const Ui::PageId scheduleId = *Ui::PageId::fromString(QStringLiteral("schedule"));
    Ui::PageDescriptor descriptor{
        .id = scheduleId,
        .label = QStringLiteral("Schedule"),
        .factory = [](const Ui::PageContext& context)
            -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
            return std::unique_ptr<Ui::Page>(new NavigationTestPage(context));
        }
    };
    QVERIFY(shell.pageHost().registerPage(std::move(descriptor)));
    const QString longLabel = QStringLiteral(
        "A navigation item with a deliberately long label that does not fit");
    QVERIFY(shell.configureNavigation(
        {
            Ui::NavigationNode::page(scheduleId),
            Ui::NavigationNode::placeholder(QStringLiteral("long_label"), longLabel)
        },
        scheduleId,
        scheduleId,
        Application::WorkspaceLifecycleState::Closed));
    shell.resize(720, 480);
    shell.show();
    QCoreApplication::processEvents();

    auto* const tree = shell.findChild<QTreeWidget*>(
        QStringLiteral("classMngrNextNavigationTree"));
    QVERIFY(tree);
    QTreeWidgetItem* longItem = nullptr;
    for (int index = 0; index < tree->topLevelItemCount(); ++index)
    {
        if (tree->topLevelItem(index)->text(0) == longLabel)
        {
            longItem = tree->topLevelItem(index);
            break;
        }
    }
    QVERIFY(longItem);
    tree->setFixedWidth(160);
    QCoreApplication::processEvents();
    QCOMPARE(longItem->toolTip(0), longLabel);
    QVERIFY(tree->property("sidebarAnimationEnabled").toBool());
    const int previousPointSize = tree->font().pointSize();

    Application::UserPreferencesState candidate = fixture.preferences.model.state();
    QVERIFY(candidate.setFontSize(Application::FontSize::Large));
    QVERIFY(candidate.setSidebarTooltipsEnabled(false));
    QVERIFY(candidate.setSidebarMarqueeEnabled(false));
    QVERIFY(fixture.preferences.model.applyAndPersist(std::move(candidate)));
    QCoreApplication::processEvents();
    QCOMPARE(tree->property("sidebarAnimationEnabled").toBool(), false);
    QVERIFY(longItem->toolTip(0).isEmpty());
    QVERIFY(tree->font().pointSize() > previousPointSize);
}

QTEST_MAIN(DocumentCatalogNavigationTests)
#include "document_catalog_navigation_tests.moc"
