#include "features/documents/document_catalog.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace
{
bool writeFile(
    const QString& filePath,
    const QByteArray& contents = QByteArrayLiteral("fixture")
    )
{
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }

    return file.write(contents) == contents.size();
}

QByteArray validCatalogJson()
{
    return R"JSON({
  "schemaVersion": 1,
  "folders": [
    {
      "path": "Guides",
      "id": "document_guides",
      "order": 20,
      "sidebarNames": {
        "default": "Guides",
        "ko": "언어 안내",
        "ko_KR": "한국 안내"
      }
    },
    {
      "path": "Guides/Advanced",
      "id": "document_guides_advanced",
      "order": 10,
      "sidebarNames": {
        "default": "Advanced"
      }
    }
  ],
  "documents": [
    {
      "id": "document_guide",
      "order": 10,
      "sidebarNames": {
        "default": "Guide",
        "ko_KR": "안내"
      },
      "pdf": {
        "path": "Guides/Advanced",
        "fileName": "Guide.pdf"
      },
      "printingEnabled": true,
      "exportingEnabled": true,
      "export": {
        "path": "Guides/Advanced",
        "fileName": "Guide.docx"
      }
    }
  ]
})JSON";
}

bool createValidCatalogRoot(
    const QString& rootPath
    )
{
    const QString folderPath =
        QDir(rootPath).filePath(QStringLiteral("Guides/Advanced"));

    return QDir().mkpath(folderPath)
        && writeFile(QDir(folderPath).filePath(QStringLiteral("Guide.pdf")))
        && writeFile(QDir(folderPath).filePath(QStringLiteral("Guide.docx")))
        && writeFile(QDir(folderPath).filePath(QStringLiteral("Unlisted.pdf")))
        && writeFile(
            QDir(rootPath).filePath(QStringLiteral("documents.json")),
            validCatalogJson()
            );
}
}

class DocumentCatalogTests : public QObject
{
    Q_OBJECT

private slots:
    void loadsValidCatalogAndResolvesLocales();
    void skipsInvalidAndDuplicateDocuments();
    void malformedActiveCatalogUsesEmbeddedCatalog();
    void rejectsCatalogLevelFailures();
};

void DocumentCatalogTests::loadsValidCatalogAndResolvesLocales()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QVERIFY(createValidCatalogRoot(temporaryDirectory.path()));

    const auto catalog =
        DocumentCatalog::loadFromRoot(
            temporaryDirectory.path()
            );

    if (!catalog)
    {
        QFAIL(qPrintable(catalog.error()));
    }
    QCOMPARE(catalog->folders().size(), 2);
    QCOMPARE(catalog->documents().size(), 1);
    QVERIFY(catalog->warnings().isEmpty());

    const DocumentFolderDefinition& folder =
        catalog->folders().first();
    QCOMPARE(
        folder.sidebarNames.forLocale(QStringLiteral("ko_KR")),
        QStringLiteral("한국 안내")
        );
    QCOMPARE(
        folder.sidebarNames.forLocale(QStringLiteral("ko_KP")),
        QStringLiteral("언어 안내")
        );
    QCOMPARE(
        folder.sidebarNames.forLocale(QStringLiteral("en_US")),
        QStringLiteral("Guides")
        );

    const DocumentDefinition* document =
        catalog->document(QStringLiteral("document_guide"));
    QVERIFY(document);
    QVERIFY(document->printingEnabled);
    QVERIFY(document->exportingEnabled);
    QVERIFY(document->exportFile.has_value());
    QCOMPARE(document->exportFile->fileName, QStringLiteral("Guide.docx"));
    QVERIFY(
        !catalog->document(QStringLiteral("Unlisted"))
        );
}

void DocumentCatalogTests::skipsInvalidAndDuplicateDocuments()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QVERIFY(createValidCatalogRoot(temporaryDirectory.path()));

    QByteArray json =
        validCatalogJson();
    const QByteArray insertion =
        R"JSON(,
    {
      "id": "document_guide",
      "order": 20,
      "sidebarNames": {"default": "Duplicate"},
      "pdf": {"path": "Guides/Advanced", "fileName": "Guide.pdf"},
      "printingEnabled": false,
      "exportingEnabled": false
    },
    {
      "id": "document_missing",
      "order": 30,
      "sidebarNames": {"default": "Missing"},
      "pdf": {"path": "Guides/Advanced", "fileName": "Missing.pdf"},
      "printingEnabled": false,
      "exportingEnabled": false
    },
    {
      "id": "document_unsafe",
      "order": 40,
      "sidebarNames": {"default": "Unsafe"},
      "pdf": {"path": "../Outside", "fileName": "Unsafe.pdf"},
      "printingEnabled": false,
      "exportingEnabled": false
    })JSON";

    const qsizetype endOfDocuments =
        json.lastIndexOf(QByteArrayLiteral("\n  ]"));
    QVERIFY(endOfDocuments > 0);
    json.insert(endOfDocuments, insertion);

    QVERIFY(
        writeFile(
            QDir(temporaryDirectory.path()).filePath(
                QStringLiteral("documents.json")
                ),
            json
            )
        );

    const auto catalog =
        DocumentCatalog::loadFromRoot(
            temporaryDirectory.path()
            );

    if (!catalog)
    {
        QFAIL(qPrintable(catalog.error()));
    }
    QCOMPARE(catalog->documents().size(), 1);
    QVERIFY(catalog->warnings().size() >= 3);
    QVERIFY(catalog->document(QStringLiteral("document_guide")));
    QVERIFY(!catalog->document(QStringLiteral("document_missing")));
    QVERIFY(!catalog->document(QStringLiteral("document_unsafe")));
}

void DocumentCatalogTests::malformedActiveCatalogUsesEmbeddedCatalog()
{
    QTemporaryDir activeDirectory;
    QTemporaryDir embeddedDirectory;
    QVERIFY(activeDirectory.isValid());
    QVERIFY(embeddedDirectory.isValid());

    QVERIFY(
        writeFile(
            QDir(activeDirectory.path()).filePath(
                QStringLiteral("documents.json")
                ),
            QByteArrayLiteral("{not json")
            )
        );
    QVERIFY(createValidCatalogRoot(embeddedDirectory.path()));

    const auto catalog =
        DocumentCatalog::loadFromRoots(
            activeDirectory.path(),
            embeddedDirectory.path()
            );

    if (!catalog)
    {
        QFAIL(qPrintable(catalog.error()));
    }
    QCOMPARE(catalog->rootPath(), embeddedDirectory.path());
    QCOMPARE(catalog->documents().size(), 1);
    QVERIFY(
        catalog->warnings().first().contains(
            QStringLiteral("embedded catalog was used")
            )
        );
}

void DocumentCatalogTests::rejectsCatalogLevelFailures()
{
    QTemporaryDir activeDirectory;
    QTemporaryDir embeddedDirectory;
    QVERIFY(activeDirectory.isValid());
    QVERIFY(embeddedDirectory.isValid());

    QVERIFY(
        writeFile(
            QDir(activeDirectory.path()).filePath(
                QStringLiteral("documents.json")
                ),
            QByteArrayLiteral(R"({"schemaVersion":2,"folders":[],"documents":[]})")
            )
        );
    QVERIFY(
        writeFile(
            QDir(embeddedDirectory.path()).filePath(
                QStringLiteral("documents.json")
                ),
            QByteArrayLiteral(R"({"schemaVersion":2,"folders":[],"documents":[]})")
            )
        );

    const auto catalog =
        DocumentCatalog::loadFromRoots(
            activeDirectory.path(),
            embeddedDirectory.path()
            );

    QVERIFY(!catalog.has_value());
    QVERIFY(
        catalog.error().contains(
            QStringLiteral("Active Documents catalog failed")
            )
        );
    QVERIFY(
        catalog.error().contains(
            QStringLiteral("Embedded Documents catalog failed")
            )
        );
}

QTEST_MAIN(DocumentCatalogTests)

#include "document_catalog_tests.moc"
