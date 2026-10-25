#include "next/resources/resource_loader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cerrno>
#include <iostream>
#include <optional>

#if defined(Q_OS_UNIX)
#include <fcntl.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <io.h>
#define NOMINMAX
#include <windows.h>
#endif

using namespace ClassMngr::Next::Resources;

namespace
{

bool fail(const QString& message)
{
    std::cerr << message.toStdString() << '\n';
    return false;
}

std::optional<ResourceId> resourceId(const char* value)
{
    return ResourceId::fromString(QString::fromLatin1(value));
}

bool stageFile(
    const QString& sourceRoot,
    const QString& packageRoot,
    const QString& relativePath,
    const QString& alternateSource = {}
    )
{
    const QString sourcePath = alternateSource.isEmpty()
        ? QDir(sourceRoot).filePath(relativePath)
        : QDir(alternateSource).filePath(QFileInfo(relativePath).fileName());
    const QString destination = QDir(packageRoot).filePath(relativePath);
    if (!QDir().mkpath(QFileInfo(destination).absolutePath()))
    {
        return fail(QStringLiteral("cannot create fixture directory for ") + relativePath);
    }
    QFile::remove(destination);
    if (!QFile::copy(sourcePath, destination))
    {
        return fail(QStringLiteral("cannot stage fixture ") + sourcePath);
    }
    return true;
}

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }
    return file.write(bytes) == bytes.size();
}

std::optional<QByteArray> sameSizeValidJsonMutation(const QByteArray& original)
{
    QByteArray changed = original;
    bool inString = false;
    bool escaped = false;
    for (qsizetype index = 0; index < changed.size(); ++index)
    {
        const unsigned char byte = static_cast<unsigned char>(changed.at(index));
        if (!inString)
        {
            if (byte == '"')
            {
                inString = true;
            }
            continue;
        }
        if (escaped)
        {
            escaped = false;
            continue;
        }
        if (byte == '\\')
        {
            escaped = true;
            continue;
        }
        if (byte == '"')
        {
            inString = false;
            continue;
        }
        if (byte >= 'a' && byte <= 'z')
        {
            changed[index] = static_cast<char>(byte - 'a' + 'A');
            return changed;
        }
        if (byte >= 'A' && byte <= 'Z')
        {
            changed[index] = static_cast<char>(byte - 'A' + 'a');
            return changed;
        }
    }
    return std::nullopt;
}

bool testLoader(QCoreApplication& application)
{
    QTemporaryDir temporaryRoot;
    if (!temporaryRoot.isValid())
    {
        return fail(QStringLiteral("could not create resource package fixture root"));
    }

    auto loadedCatalog = ResourceCatalog::load(
        QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
        temporaryRoot.path()
        );
    if (!loadedCatalog)
    {
        return fail(QStringLiteral("catalog load failed: ") + loadedCatalog.error().message);
    }
    if (loadedCatalog->records().size() != 198)
    {
        return fail(QStringLiteral("generated catalog did not contain 198 runtime records"));
    }

    int directCount = 0;
    int generatedTranslationCount = 0;
    int qtResourceCount = 0;
    for (const auto& record : loadedCatalog->records())
    {
        if (record.location() == ResourceLocation::PackageFile)
        {
            ++directCount;
            if (record.expectedType() == ResourceType::Qm)
            {
                ++generatedTranslationCount;
            }
        }
        else if (record.location() == ResourceLocation::QtResource)
        {
            ++qtResourceCount;
        }
    }
    if (directCount != 196 || generatedTranslationCount != 5 || qtResourceCount != 2)
    {
        return fail(QStringLiteral("catalog generated resource-kind counts are incorrect"));
    }

    ResourceLoader loader(std::move(*loadedCatalog));
    const QString packageRoot = temporaryRoot.path();
    const QString bakedRoot = QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT);
    const QString translationDirectory = QString::fromUtf8(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY
        );

    // Only stage requested examples. The rest of the catalog, including every
    // PDF body, is absent while initialization and the first loads succeed.
    if (!stageFile(
            bakedRoot,
            packageRoot,
            QStringLiteral("assets/campuses/bundang.json")
            )
        || !stageFile(
            bakedRoot,
            packageRoot,
            QStringLiteral("assets/campuses/bundang/bundang_building.png")
            )
        || !stageFile(
            bakedRoot,
            packageRoot,
            QStringLiteral("assets/fonts/Caveat-wght.ttf")
            )
        || !stageFile(
            bakedRoot,
            packageRoot,
            QStringLiteral("assets/styles/dark.qss")
            )
        || !stageFile(
            bakedRoot,
            packageRoot,
            QStringLiteral("assets/documents/Book Reports/Book Report Grading Rubric.pdf")
            )
        || !stageFile(
            bakedRoot,
            packageRoot,
            QStringLiteral("assets/translations/ClassMngr_ko_KR.qm"),
            translationDirectory
            ))
    {
        return false;
    }

    const auto jsonId = resourceId("resource:assets/campuses/bundang.json");
    const auto imageId = resourceId("resource:assets/campuses/bundang/bundang_building.png");
    const auto fontId = resourceId("resource:assets/fonts/Caveat-wght.ttf");
    const auto stylesheetId = resourceId("resource:assets/styles/dark.qss");
    const auto translationId = resourceId("resource:assets/translations/ClassMngr_ko_KR.ts");
    const auto pdfId = resourceId(
        "document.body:Book Reports/Book Report Grading Rubric.pdf"
        );
    const auto qmlId = resourceId("calendar.qml:EventCalendar.qml");
    if (!jsonId || !imageId || !fontId || !stylesheetId || !translationId || !pdfId || !qmlId)
    {
        return fail(QStringLiteral("test resource IDs were rejected"));
    }

    auto json = loader.loadJson(*jsonId);
    if (!json || !json->isObject())
    {
        return fail(QStringLiteral("JSON loader failed: ")
            + (json ? QStringLiteral("document was not an object") : json.error().message));
    }
    auto wrongType = loader.loadImage(*jsonId);
    if (wrongType || wrongType.error().code != ResourceErrorCode::TypeMismatch)
    {
        return fail(QStringLiteral("image loader did not reject a JSON resource"));
    }

    auto image = loader.loadImage(*imageId);
    if (!image || image->isNull())
    {
        return fail(QStringLiteral("image loader failed: ")
            + (image ? QStringLiteral("image was null") : image.error().message));
    }

    auto stylesheet = loader.loadStylesheet(*stylesheetId);
    if (!stylesheet || stylesheet->isEmpty())
    {
        return fail(QStringLiteral("stylesheet loader failed"));
    }
    auto wrongStylesheetType = loader.loadStylesheet(*jsonId);
    if (wrongStylesheetType
        || wrongStylesheetType.error().code != ResourceErrorCode::TypeMismatch)
    {
        return fail(QStringLiteral("stylesheet loader did not reject a JSON resource"));
    }

    auto qmlText = loader.loadText(*qmlId);
    if (!qmlText || !qmlText->contains(QStringLiteral("Calendar")))
    {
        return fail(QStringLiteral("qrc Calendar text load failed: ")
            + (qmlText ? QStringLiteral("unexpected QML contents") : qmlText.error().message));
    }
    const ResourceDescriptor* qmlDescriptor = loader.catalog().find(*qmlId);
    if (qmlDescriptor == nullptr
        || qmlDescriptor->location() != ResourceLocation::QtResource
        || !qmlDescriptor->runtimeUrl().startsWith(QStringLiteral("qrc:/")))
    {
        return fail(QStringLiteral("Calendar resource URL was conflated with a package file path"));
    }
    auto qrcStream = loader.openStream(*qmlId);
    if (qrcStream || qrcStream.error().code != ResourceErrorCode::UnsupportedOperation)
    {
        return fail(QStringLiteral("file stream loader accepted a qrc resource"));
    }

    const int fontRegistrationId = [&]() {
        auto font = loader.loadFont(*fontId);
        if (!font || font->familyNames().isEmpty())
        {
            fail(QStringLiteral("font loader failed: ")
                + (font ? QStringLiteral("no family names") : font.error().message));
            return -2;
        }
        const int registrationId = font->registrationId();
        return registrationId;
    }();
    if (fontRegistrationId < 0
        || !QFontDatabase::applicationFontFamilies(fontRegistrationId).isEmpty())
    {
        return fail(QStringLiteral("font registration was not removed when its owner expired"));
    }

    const QString untranslatedPrint = QCoreApplication::translate("ActionRegistry", "Print");
    if (untranslatedPrint != QStringLiteral("Print"))
    {
        return fail(QStringLiteral("translation fallback was not the source text before installation"));
    }
    {
        auto translation = loader.loadTranslation(*translationId, application);
        if (!translation || !translation->isInstalled() || translation->translator().isEmpty())
        {
            return fail(QStringLiteral("translation loader failed: ")
                + (translation ? QStringLiteral("translation was not installed")
                               : translation.error().message));
        }
        if (QCoreApplication::translate("ActionRegistry", "Print") != QStringLiteral("인쇄"))
        {
            return fail(QStringLiteral("Korean translator did not translate ActionRegistry::Print"));
        }
    }
    if (QCoreApplication::translate("ActionRegistry", "Print") != QStringLiteral("Print"))
    {
        return fail(QStringLiteral("translation was still installed after its owner expired"));
    }

    const QString pdfPath = QDir(packageRoot).filePath(
        QStringLiteral("assets/documents/Book Reports/Book Report Grading Rubric.pdf")
        );
    qintptr streamHandle = -1;
#if defined(Q_OS_WIN)
    HANDLE nativeStreamHandle = INVALID_HANDLE_VALUE;
#endif
    {
        auto stream = loader.openStream(*pdfId);
        if (!stream || !stream->isOpen())
        {
            return fail(QStringLiteral("PDF file-backed stream failed: ")
                + (stream ? QStringLiteral("stream was closed") : stream.error().message));
        }
        const QByteArray prefix = stream->device().read(4);
        if (prefix != QByteArray("%PDF", 4))
        {
            return fail(QStringLiteral("PDF stream returned unexpected leading bytes"));
        }
        auto* file = dynamic_cast<QFile*>(&stream->device());
        if (file == nullptr)
        {
            return fail(QStringLiteral("file stream did not expose its owning QFile device"));
        }
#if defined(Q_OS_WIN)
        const auto nativeHandleValue = ::_get_osfhandle(file->handle());
        if (nativeHandleValue == -1)
        {
            return fail(QStringLiteral("file stream descriptor had no native Windows handle"));
        }
        nativeStreamHandle = reinterpret_cast<HANDLE>(nativeHandleValue);
#else
        streamHandle = file->handle();
#endif
    }
#if defined(Q_OS_UNIX)
    if (streamHandle < 0)
    {
        return fail(QStringLiteral("file stream returned an invalid POSIX descriptor"));
    }
    errno = 0;
    if (::fcntl(static_cast<int>(streamHandle), F_GETFD) != -1 || errno != EBADF)
    {
        return fail(QStringLiteral("POSIX file descriptor remained open after stream destruction"));
    }
#elif defined(Q_OS_WIN)
    if (nativeStreamHandle == INVALID_HANDLE_VALUE)
    {
        return fail(QStringLiteral("file stream returned an invalid Windows handle"));
    }
    DWORD handleFlags = 0;
    const BOOL handleIsValid = ::GetHandleInformation(nativeStreamHandle, &handleFlags);
    const DWORD handleCheckError = ::GetLastError();
    if (handleIsValid != 0 || handleCheckError != ERROR_INVALID_HANDLE)
    {
        return fail(QStringLiteral("Windows file handle remained open after stream destruction"));
    }
#else
    const QString renamedPdfPath = pdfPath + QStringLiteral(".renamed");
    if (!QFile::rename(pdfPath, renamedPdfPath) || !QFile::rename(renamedPdfPath, pdfPath))
    {
        return fail(QStringLiteral("stream handle did not close when its RAII owner expired"));
    }
#endif

    auto missingId = resourceId("resource:assets/campuses/bundang/bundang_map.png");
    if (!missingId)
    {
        return fail(QStringLiteral("missing resource ID was rejected"));
    }
    auto missingImage = loader.loadImage(*missingId);
    if (missingImage || missingImage.error().code != ResourceErrorCode::MissingFile)
    {
        return fail(QStringLiteral("missing packaged image did not return a missing-file error"));
    }

    const QString stagedJsonPath = QDir(packageRoot).filePath(
        QStringLiteral("assets/campuses/bundang.json")
        );
    const QByteArray originalJson = [&]() {
        QFile file(stagedJsonPath);
        if (!file.open(QIODevice::ReadOnly)) return QByteArray{};
        return file.readAll();
    }();
    const auto corruptedJson = sameSizeValidJsonMutation(originalJson);
    if (originalJson.isEmpty() || !corruptedJson || corruptedJson->size() != originalJson.size()
        || !writeBytes(stagedJsonPath, *corruptedJson))
    {
        return fail(QStringLiteral("could not prepare corrupt JSON fixture"));
    }
    auto corruptJson = loader.loadJson(*jsonId);
    if (corruptJson || corruptJson.error().code != ResourceErrorCode::CorruptResource
        || !corruptJson.error().message.contains(QStringLiteral("SHA-256")))
    {
        return fail(QStringLiteral("same-size changed JSON did not fail catalog SHA-256 validation"));
    }
    if (!writeBytes(stagedJsonPath, originalJson))
    {
        return fail(QStringLiteral("could not restore JSON fixture"));
    }

    QTemporaryDir outsideRoot;
    if (!outsideRoot.isValid())
    {
        return fail(QStringLiteral("could not create symlink escape target directory"));
    }
    const QString outsideFile = QDir(outsideRoot.path()).filePath(QStringLiteral("outside.json"));
    if (!writeBytes(outsideFile, originalJson))
    {
        return fail(QStringLiteral("could not create symlink escape target"));
    }
    QFile::remove(stagedJsonPath);
    if (QFile::link(outsideFile, stagedJsonPath))
    {
        auto escapedJson = loader.loadJson(*jsonId);
        if (escapedJson || escapedJson.error().code != ResourceErrorCode::UnsafePath)
        {
            return fail(QStringLiteral("resource symlink escape was not rejected"));
        }
    }
    else
    {
        std::cerr << "symlink escape fixture could not be created on this platform; "
                     "path traversal validation remains covered\n";
    }

    const QString maliciousCatalogPath = QDir(temporaryRoot.path()).filePath(
        QStringLiteral("malicious-catalog.json")
        );
    QFile catalogFile(QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG));
    if (!catalogFile.open(QIODevice::ReadOnly))
    {
        return fail(QStringLiteral("could not read source catalog for schema test"));
    }
    QJsonParseError parseError;
    QJsonDocument catalogDocument = QJsonDocument::fromJson(catalogFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !catalogDocument.isObject())
    {
        return fail(QStringLiteral("test source catalog was unexpectedly invalid"));
    }
    QJsonObject catalogObject = catalogDocument.object();
    QJsonArray records = catalogObject.value(QStringLiteral("records")).toArray();
    QJsonObject unsafeRecord = records.first().toObject();
    unsafeRecord.insert(QStringLiteral("package_relative_path"), QStringLiteral("../escape.json"));
    records[0] = unsafeRecord;
    catalogObject.insert(QStringLiteral("records"), records);
    if (!writeBytes(
            maliciousCatalogPath,
            QJsonDocument(catalogObject).toJson(QJsonDocument::Compact)
            ))
    {
        return fail(QStringLiteral("could not write malicious catalog fixture"));
    }
    auto unsafeCatalog = ResourceCatalog::load(maliciousCatalogPath, packageRoot);
    if (unsafeCatalog || unsafeCatalog.error().code != ResourceErrorCode::InvalidCatalog)
    {
        return fail(QStringLiteral("catalog accepted a traversal path"));
    }

    QFile originalCatalogFile(QString::fromUtf8(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG));
    if (!originalCatalogFile.open(QIODevice::ReadOnly))
    {
        return fail(QStringLiteral("could not reopen source catalog for collision tests"));
    }
    const QJsonDocument originalCatalogDocument = QJsonDocument::fromJson(
        originalCatalogFile.readAll(), &parseError
        );
    if (parseError.error != QJsonParseError::NoError || !originalCatalogDocument.isObject())
    {
        return fail(QStringLiteral("source catalog became invalid during collision tests"));
    }
    const QJsonObject originalCatalogObject = originalCatalogDocument.object();
    const QJsonArray originalRecords = originalCatalogObject.value(
        QStringLiteral("records")
        ).toArray();

    QJsonArray duplicateIdRecords = originalRecords;
    QJsonObject duplicateIdRecord = duplicateIdRecords[1].toObject();
    duplicateIdRecord.insert(
        QStringLiteral("logical_id"),
        duplicateIdRecords[0].toObject().value(QStringLiteral("logical_id"))
        );
    duplicateIdRecords[1] = duplicateIdRecord;
    QJsonObject duplicateIdCatalog = originalCatalogObject;
    duplicateIdCatalog.insert(QStringLiteral("records"), duplicateIdRecords);
    const QString duplicateIdCatalogPath = QDir(temporaryRoot.path()).filePath(
        QStringLiteral("duplicate-id-catalog.json")
        );
    if (!writeBytes(
            duplicateIdCatalogPath,
            QJsonDocument(duplicateIdCatalog).toJson(QJsonDocument::Compact)
            ))
    {
        return fail(QStringLiteral("could not write duplicate-ID catalog fixture"));
    }
    auto duplicateIdResult = ResourceCatalog::load(duplicateIdCatalogPath, packageRoot);
    if (duplicateIdResult
        || duplicateIdResult.error().code != ResourceErrorCode::InvalidCatalog)
    {
        return fail(QStringLiteral("catalog accepted duplicate logical IDs"));
    }

    QJsonArray duplicatePathRecords = originalRecords;
    QJsonObject duplicatePathRecord = duplicatePathRecords[1].toObject();
    duplicatePathRecord.insert(QStringLiteral("logical_id"), QStringLiteral("test:duplicate-path"));
    duplicatePathRecord.insert(
        QStringLiteral("package_relative_path"),
        duplicatePathRecords[0].toObject().value(QStringLiteral("package_relative_path"))
        );
    duplicatePathRecords[1] = duplicatePathRecord;
    QJsonObject duplicatePathCatalog = originalCatalogObject;
    duplicatePathCatalog.insert(QStringLiteral("records"), duplicatePathRecords);
    const QString duplicatePathCatalogPath = QDir(temporaryRoot.path()).filePath(
        QStringLiteral("duplicate-path-catalog.json")
        );
    if (!writeBytes(
            duplicatePathCatalogPath,
            QJsonDocument(duplicatePathCatalog).toJson(QJsonDocument::Compact)
            ))
    {
        return fail(QStringLiteral("could not write duplicate-path catalog fixture"));
    }
    auto duplicatePathResult = ResourceCatalog::load(duplicatePathCatalogPath, packageRoot);
    if (duplicatePathResult
        || duplicatePathResult.error().code != ResourceErrorCode::InvalidCatalog)
    {
        return fail(QStringLiteral("catalog accepted duplicate package paths"));
    }

    if (ResourceId::fromString(QString{}).has_value())
    {
        return fail(QStringLiteral("empty ResourceId was accepted"));
    }
    return true;
}

}

int main(int argc, char** argv)
{
    QGuiApplication application(argc, argv);
    if (!testLoader(application))
    {
        return 1;
    }
    return 0;
}
