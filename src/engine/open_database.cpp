#include "classmngr/engine/open_database.h"

#include "classmngr/engine/database_schema.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>

namespace classmngr::engine
{
namespace
{
bool isBlank(
    std::string_view text
    ) noexcept
{
    if (text.empty())
    {
        return true;
    }

    for (const char character : text)
    {
        if (std::isspace(static_cast<unsigned char>(character)) == 0)
        {
            return false;
        }
    }

    return true;
}

bool containsNull(
    std::string_view text
    ) noexcept
{
    return text.find('\0') != std::string_view::npos;
}

Error pathError(
    ErrorCode code,
    std::string message
    )
{
    return {
        code,
        std::move(message),
        std::nullopt
    };
}

std::string pathToUtf8(
    const std::filesystem::path& path
    )
{
    const std::u8string encoded = path.generic_u8string();
    std::string result(
        reinterpret_cast<const char*>(encoded.data()),
        encoded.size()
        );
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

Result<std::string> normalizeDatabasePath(
    std::string_view databasePath,
    bool createParentDirectories
    )
{
    if (isBlank(databasePath))
    {
        return std::unexpected(pathError(
            ErrorCode::InvalidArgument,
            "Database path must not be blank."
            ));
    }
    if (containsNull(databasePath))
    {
        return std::unexpected(pathError(
            ErrorCode::InvalidArgument,
            "Database path must not contain an embedded null character."
            ));
    }
    if (databasePath == ":memory:")
    {
        return std::string(databasePath);
    }

    try
    {
        const std::filesystem::path inputPath = std::filesystem::u8path(
            databasePath.begin(),
            databasePath.end()
            );
        std::error_code filesystemError;
        const std::filesystem::path absolutePath = std::filesystem::absolute(
            inputPath,
            filesystemError
            );
        if (filesystemError)
        {
            return std::unexpected(pathError(
                ErrorCode::Io,
                "Unable to resolve database path: "
                + filesystemError.message()
                ));
        }

        if (createParentDirectories)
        {
            const std::filesystem::path parentPath =
                absolutePath.parent_path();
            if (!parentPath.empty())
            {
                std::filesystem::create_directories(
                    parentPath,
                    filesystemError
                    );
                if (filesystemError)
                {
                    return std::unexpected(pathError(
                        ErrorCode::Io,
                        "Unable to create the database directory: "
                        + filesystemError.message()
                        ));
                }
            }
        }

        return pathToUtf8(absolutePath);
    }
    catch (const std::exception& exception)
    {
        return std::unexpected(pathError(
            ErrorCode::Io,
            "Unable to prepare database path: "
            + std::string(exception.what())
            ));
    }
}
} // namespace

Result<std::unique_ptr<SqliteDatabase>> OpenDatabase::execute(
    std::string_view databasePath,
    const OpenDatabaseOptions& options
    )
{
    const Result<std::string> normalizedPath = normalizeDatabasePath(
        databasePath,
        options.createParentDirectories
        );
    if (!normalizedPath)
    {
        return std::unexpected(normalizedPath.error());
    }

    auto database = std::make_unique<SqliteDatabase>();
    const Status opened = database->open(*normalizedPath, options.sqlite);
    if (!opened)
    {
        return std::unexpected(opened.error());
    }

    const Status schema = DatabaseSchemaManager::ensureSchema(*database);
    if (!schema)
    {
        database->close();
        return std::unexpected(schema.error());
    }

    return database;
}

} // namespace classmngr::engine
