#include "app/startup_database_path.h"

#include "core/database_file_format.h"

QString startupDatabasePath(
    const QStringList& arguments
    )
{
    for (int index = 1; index < arguments.size(); ++index)
    {
        const QString argument =
            arguments.at(index);

        if (
            argument
                == QStringLiteral(
                    "--startup-performance-output"
                    )
            || argument
                == QStringLiteral(
                    "--startup-performance-scenario"
                    )
            || argument
                == QStringLiteral(
                    "--startup-performance-settle-ms"
                    )
            )
        {
            ++index;
            continue;
        }

        if (
            argument.startsWith(QLatin1Char('-'))
            )
        {
            continue;
        }

        if (
            DatabaseFileFormat::isSupportedInputPath(
                argument
                )
            )
        {
            return argument;
        }
    }

    return {};
}
