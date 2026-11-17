#include "core/process_memory_snapshot.h"
#include "next/application/font_presentation_port.h"
#include "next/ui/font_size_presenter.h"

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSysInfo>
#include <QTextStream>

#define NOMINMAX
#include <windows.h>

#include <chrono>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using Clock = std::chrono::steady_clock;

struct ProbeArguments final
{
    QString scenario;
    QString interPath;
    QString pretendardPath;
};

class ProbeFontPresentationPort final
    : public ClassMngr::Next::Application::FontPresentationPort
{
public:
    [[nodiscard]] int applicationBaseFontPointSize() const override
    {
        return 14;
    }

    [[nodiscard]] int teacherProfileKoreanFontPointSize() const override
    {
        return 15;
    }
};

std::optional<ProbeArguments> parseArguments(const int argc, char** argv)
{
    ProbeArguments result;
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (index + 1 >= argc)
        {
            return std::nullopt;
        }
        const QString value = QString::fromLocal8Bit(argv[++index]);
        if (argument == "--scenario")
        {
            result.scenario = value;
        }
        else if (argument == "--inter")
        {
            result.interPath = value;
        }
        else if (argument == "--pretendard")
        {
            result.pretendardPath = value;
        }
        else
        {
            return std::nullopt;
        }
    }

    static const QStringList scenarios{
        QStringLiteral("no-font"),
        QStringLiteral("inter-only"),
        QStringLiteral("pretendard-only"),
        QStringLiteral("inter-then-pretendard"),
        QStringLiteral("pretendard-then-inter")
    };
    if (!scenarios.contains(result.scenario)
        || result.interPath.isEmpty()
        || result.pretendardPath.isEmpty())
    {
        return std::nullopt;
    }
    return result;
}

QString executablePath()
{
    std::vector<wchar_t> path(32768);
    const DWORD length = GetModuleFileNameW(
        nullptr,
        path.data(),
        static_cast<DWORD>(path.size())
        );
    if (length == 0 || length >= path.size())
    {
        return {};
    }
    return QString::fromWCharArray(path.data(), static_cast<qsizetype>(length));
}

QString architecture()
{
    if constexpr (sizeof(void*) == 8)
    {
        return QStringLiteral("x64");
    }
    return QStringLiteral("x86");
}

QJsonArray stringArray(const QStringList& values)
{
    QJsonArray result;
    for (const QString& value : values)
    {
        result.append(value);
    }
    return result;
}

bool captureSnapshot(
    const QString& scenario,
    const QString& locale,
    const QString& event,
    const QString& processExecutablePath,
    const ProcessMemorySnapshot& snapshot,
    const int sequence,
    const QJsonObject& details
    )
{
    if (!snapshot.isAvailable
        || !snapshot.workingSetBytesAvailable
        || !snapshot.privateUsageBytesAvailable)
    {
        std::cerr << "ProcessMemorySnapshotProvider did not return working set and private usage.\n";
        return false;
    }

    QJsonObject memory{
        {QStringLiteral("isAvailable"), snapshot.isAvailable},
        {QStringLiteral("workingSetBytesAvailable"), snapshot.workingSetBytesAvailable},
        {QStringLiteral("workingSetBytes"), static_cast<qint64>(snapshot.workingSetBytes)},
        {QStringLiteral("privateUsageBytesAvailable"), snapshot.privateUsageBytesAvailable},
        {QStringLiteral("privateUsageBytes"), static_cast<qint64>(snapshot.privateUsageBytes)},
        {QStringLiteral("privateWorkingSetBytesAvailable"), snapshot.privateWorkingSetBytesAvailable},
        {QStringLiteral("privateWorkingSetBytes"), static_cast<qint64>(snapshot.privateWorkingSetBytes)},
        {QStringLiteral("peakWorkingSetBytesAvailable"), snapshot.peakWorkingSetBytesAvailable},
        {QStringLiteral("peakWorkingSetBytes"), static_cast<qint64>(snapshot.peakWorkingSetBytes)},
        {QStringLiteral("pagefileUsageBytesAvailable"), snapshot.pagefileUsageBytesAvailable},
        {QStringLiteral("pagefileUsageBytes"), static_cast<qint64>(snapshot.pagefileUsageBytes)},
        {QStringLiteral("handleCountAvailable"), snapshot.handleCountAvailable},
        {QStringLiteral("handleCount"), static_cast<int>(snapshot.handleCount)},
        {QStringLiteral("threadCountAvailable"), snapshot.threadCountAvailable},
        {QStringLiteral("threadCount"), static_cast<int>(snapshot.threadCount)},
        {QStringLiteral("platform"), snapshot.platform}
    };

    QJsonObject record{
        {QStringLiteral("schema"), QStringLiteral("classmngr-phase9-font-allocation-snapshot-v1")},
        {QStringLiteral("recordType"), QStringLiteral("snapshot")},
        {QStringLiteral("scenario"), scenario},
        {QStringLiteral("event"), event},
        {QStringLiteral("sequence"), sequence},
        {QStringLiteral("capturedAtUtc"), snapshot.capturedAt.toUTC().toString(Qt::ISODateWithMs)},
        {QStringLiteral("pid"), static_cast<qint64>(GetCurrentProcessId())},
        {QStringLiteral("architecture"), architecture()},
        {QStringLiteral("executablePath"), processExecutablePath},
        {QStringLiteral("qtVersion"), QStringLiteral(QT_VERSION_STR)},
        {QStringLiteral("locale"), locale},
        {QStringLiteral("memory"), memory},
        {QStringLiteral("details"), details}
    };
    QTextStream output(stdout);
    output << QJsonDocument(record).toJson(QJsonDocument::Compact) << Qt::endl;
    return true;
}

bool capture(
    PlatformProcessMemorySnapshotProvider& provider,
    int& sequence,
    const QString& scenario,
    const QString& locale,
    const QString& event,
    const QString& processExecutablePath,
    const QJsonObject& details = {}
    )
{
    return captureSnapshot(
        scenario,
        locale,
        event,
        processExecutablePath,
        provider.snapshot(),
        sequence++,
        details
        );
}

QStringList fontOrder(const QString& scenario)
{
    if (scenario == QStringLiteral("inter-only"))
    {
        return {QStringLiteral("inter")};
    }
    if (scenario == QStringLiteral("pretendard-only"))
    {
        return {QStringLiteral("pretendard")};
    }
    if (scenario == QStringLiteral("inter-then-pretendard"))
    {
        return {QStringLiteral("inter"), QStringLiteral("pretendard")};
    }
    if (scenario == QStringLiteral("pretendard-then-inter"))
    {
        return {QStringLiteral("pretendard"), QStringLiteral("inter")};
    }
    return {};
}

int runProbe(const ProbeArguments& arguments)
{
    const QString processExecutablePath = executablePath();
    if (processExecutablePath.isEmpty())
    {
        std::cerr << "Could not resolve the probe executable path.\n";
        return 2;
    }

    PlatformProcessMemorySnapshotProvider provider;
    int sequence = 0;
    if (!capture(
            provider,
            sequence,
            arguments.scenario,
            QString{},
            QStringLiteral("before-qapplication"),
            processExecutablePath
            ))
    {
        return 2;
    }

    char applicationName[] = "ClassMngrNextFontAllocationProbe";
    char* applicationArguments[] = {applicationName, nullptr};
    int applicationArgumentCount = 1;
    QApplication application(applicationArgumentCount, applicationArguments);
    const QString locale = QLocale::system().name();
    if (!capture(
            provider,
            sequence,
            arguments.scenario,
            locale,
            QStringLiteral("after-qapplication"),
            processExecutablePath
            ))
    {
        return 2;
    }

    struct RegisteredFont final
    {
        int registrationId = -1;
        QString familyName;
    };
    RegisteredFont inter;
    RegisteredFont pretendard;

    const auto pathFor = [&arguments](const QString& name) {
        return name == QStringLiteral("inter")
            ? arguments.interPath
            : arguments.pretendardPath;
    };
    const QStringList registrationOrder = fontOrder(arguments.scenario);
    for (const QString& name : registrationOrder)
    {
        const QString path = pathFor(name);
        const QString prefix = name + QStringLiteral("-registration");
        QByteArray fontBytes;
        if (name == QStringLiteral("pretendard"))
        {
            if (!capture(
                    provider,
                    sequence,
                    arguments.scenario,
                    locale,
                    QStringLiteral("before-pretendard-file-read"),
                    processExecutablePath,
                    {{QStringLiteral("path"), path}}
                    ))
            {
                return 2;
            }
            QFile fontFile(path);
            if (!fontFile.open(QIODevice::ReadOnly))
            {
                std::cerr << "Could not read packaged Pretendard font: "
                          << path.toStdString() << ".\n";
                return 3;
            }
            const auto fileReadStarted = Clock::now();
            fontBytes = fontFile.readAll();
            const auto fileReadDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                Clock::now() - fileReadStarted
                ).count();
            if (fontFile.error() != QFileDevice::NoError || fontBytes.isEmpty())
            {
                std::cerr << "Could not read the complete packaged Pretendard font.\n";
                return 3;
            }
            if (!capture(
                    provider,
                    sequence,
                    arguments.scenario,
                    locale,
                    QStringLiteral("after-pretendard-file-read"),
                    processExecutablePath,
                    {
                        {QStringLiteral("path"), path},
                        {QStringLiteral("inputBytes"), fontBytes.size()},
                        {QStringLiteral("operationDurationNanoseconds"), static_cast<qint64>(fileReadDuration)}
                    }
                    ))
            {
                return 2;
            }
        }
        if (!capture(
                provider,
                sequence,
                arguments.scenario,
                locale,
                QStringLiteral("before-") + prefix,
                processExecutablePath,
                {{QStringLiteral("font"), name}, {QStringLiteral("path"), path}}
                ))
        {
            return 2;
        }
        const auto registrationStarted = Clock::now();
        const QString registrationApi = name == QStringLiteral("inter")
            ? QStringLiteral("QFontDatabase::addApplicationFont(file)")
            : QStringLiteral("QFontDatabase::addApplicationFontFromData(bytes)");
        const int registrationId = name == QStringLiteral("inter")
            ? QFontDatabase::addApplicationFont(path)
            : QFontDatabase::addApplicationFontFromData(fontBytes);
        const auto registrationDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - registrationStarted
            ).count();
        if (registrationId < 0)
        {
            std::cerr << "Qt could not register " << name.toStdString() << " from "
                      << path.toStdString() << ".\n";
            return 3;
        }
        RegisteredFont& registered = name == QStringLiteral("inter") ? inter : pretendard;
        registered.registrationId = registrationId;
        if (!capture(
                provider,
                sequence,
                arguments.scenario,
                locale,
                QStringLiteral("after-") + prefix,
                processExecutablePath,
                {
                    {QStringLiteral("font"), name},
                    {QStringLiteral("path"), path},
                    {QStringLiteral("registrationApi"), registrationApi},
                    {QStringLiteral("inputBytes"), name == QStringLiteral("inter") ? 0 : fontBytes.size()},
                    {QStringLiteral("registrationId"), registrationId},
                    {QStringLiteral("operationDurationNanoseconds"), static_cast<qint64>(registrationDuration)}
                }
                ))
        {
            return 2;
        }

        const QString enumerationPrefix = name + QStringLiteral("-family-enumeration");
        if (!capture(
                provider,
                sequence,
                arguments.scenario,
                locale,
                QStringLiteral("before-") + enumerationPrefix,
                processExecutablePath,
                {{QStringLiteral("font"), name}, {QStringLiteral("registrationId"), registrationId}}
                ))
        {
            return 2;
        }
        const auto enumerationStarted = Clock::now();
        const QStringList families = QFontDatabase::applicationFontFamilies(registrationId);
        const auto enumerationDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - enumerationStarted
            ).count();
        if (families.isEmpty())
        {
            std::cerr << "Qt returned no application font families for "
                      << name.toStdString() << ".\n";
            return 4;
        }
        registered.familyName = families.constFirst();
        if (!capture(
                provider,
                sequence,
                arguments.scenario,
                locale,
                QStringLiteral("after-") + enumerationPrefix,
                processExecutablePath,
                {
                    {QStringLiteral("font"), name},
                    {QStringLiteral("registrationId"), registrationId},
                    {QStringLiteral("familyNames"), stringArray(families)},
                    {QStringLiteral("operationDurationNanoseconds"), static_cast<qint64>(enumerationDuration)}
                }
                ))
        {
            return 2;
        }
    }

    if (registrationOrder.isEmpty())
    {
        if (!capture(
                provider,
                sequence,
                arguments.scenario,
                locale,
                QStringLiteral("startup-font-application-skipped"),
                processExecutablePath,
                {{QStringLiteral("reason"), QStringLiteral("no-font baseline")}}
                ))
        {
            return 2;
        }
    }
    else
    {
        ProbeFontPresentationPort fontPresentationPort;
        ClassMngr::Next::Ui::FontSizePresenter fontPresenter(
            application,
            fontPresentationPort
            );
        if (!capture(
                provider,
                sequence,
                arguments.scenario,
                locale,
                QStringLiteral("before-startup-font-application"),
                processExecutablePath,
                {
                    {QStringLiteral("interFamily"), inter.familyName},
                    {QStringLiteral("pretendardFamily"), pretendard.familyName}
                }
                ))
        {
            return 2;
        }
        const auto applicationStarted = Clock::now();
        fontPresenter.applyStartupPreference(
            ClassMngr::Next::Application::FontSize::Normal,
            locale,
            inter.familyName,
            pretendard.familyName
            );
        const auto applicationDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - applicationStarted
            ).count();
        if (!capture(
                provider,
                sequence,
                arguments.scenario,
                locale,
                QStringLiteral("after-startup-font-application"),
                processExecutablePath,
                {
                    {QStringLiteral("interFamily"), inter.familyName},
                    {QStringLiteral("pretendardFamily"), pretendard.familyName},
                    {QStringLiteral("operationDurationNanoseconds"), static_cast<qint64>(applicationDuration)}
                }
                ))
        {
            return 2;
        }
    }

    if (!capture(
            provider,
            sequence,
            arguments.scenario,
            locale,
            QStringLiteral("before-system-font-families-query"),
            processExecutablePath
            ))
    {
        return 2;
    }
    const auto familyQueryStarted = Clock::now();
    const QStringList systemFamilies = QFontDatabase::families();
    const auto familyQueryDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - familyQueryStarted
        ).count();
    if (systemFamilies.isEmpty())
    {
        std::cerr << "Qt returned an empty system font family list.\n";
        return 5;
    }
    if (!capture(
            provider,
            sequence,
            arguments.scenario,
            locale,
            QStringLiteral("after-system-font-families-query"),
            processExecutablePath,
            {
                {QStringLiteral("familyCount"), systemFamilies.size()},
                {QStringLiteral("families"), stringArray(systemFamilies)},
                {QStringLiteral("operationDurationNanoseconds"), static_cast<qint64>(familyQueryDuration)}
            }
            ))
    {
        return 2;
    }

    QTextStream output(stdout);
    output << QJsonDocument(QJsonObject{
        {QStringLiteral("schema"), QStringLiteral("classmngr-phase9-font-allocation-probe-v1")},
        {QStringLiteral("recordType"), QStringLiteral("complete")},
        {QStringLiteral("scenario"), arguments.scenario},
        {QStringLiteral("pid"), static_cast<qint64>(GetCurrentProcessId())},
        {QStringLiteral("architecture"), architecture()},
        {QStringLiteral("executablePath"), processExecutablePath},
        {QStringLiteral("qtVersion"), QStringLiteral(QT_VERSION_STR)},
        {QStringLiteral("locale"), locale},
        {QStringLiteral("snapshotCount"), sequence}
    }).toJson(QJsonDocument::Compact) << Qt::endl;
    return 0;
}

}

int main(int argc, char** argv)
{
    const auto arguments = parseArguments(argc, argv);
    if (!arguments)
    {
        std::cerr << "Expected --scenario, --inter, and --pretendard arguments.\n";
        return 1;
    }
    return runProbe(*arguments);
}
