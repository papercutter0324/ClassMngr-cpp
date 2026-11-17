#include "core/process_memory_snapshot.h"

#include <QApplication>
#include <QDateTime>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRawFont>
#include <QSysInfo>
#include <QTextStream>

#include <QtGui/private/qfontengine_p.h>
#include <QtGui/private/qrawfont_p.h>

#define NOMINMAX
#include <windows.h>

#include <chrono>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using Clock = std::chrono::steady_clock;

struct StaticFace final
{
    QString key;
    QString path;
    int weight = 0;
    bool italic = false;
    QString expectedStyle;
    int registrationId = -1;
    QStringList registeredFamilies;
};

struct ProbeArguments final
{
    QString scenario;
    QString interPath;
    QStringList staticFacePaths;
};

struct FaceExpectation final
{
    QString key;
    QString style;
    int weight;
    bool italic;
};

const std::vector<FaceExpectation>& expectedFaces()
{
    static const std::vector<FaceExpectation> faces{
        {QStringLiteral("regular"), QStringLiteral("Regular"), 400, false},
        {QStringLiteral("medium"), QStringLiteral("Medium"), 500, false},
        {QStringLiteral("semibold"), QStringLiteral("SemiBold"), 600, false},
        {QStringLiteral("bold"), QStringLiteral("Bold"), 700, false},
        {QStringLiteral("bold-italic"), QStringLiteral("Bold Italic"), 700, true},
        {QStringLiteral("black"), QStringLiteral("Black"), 900, false}
    };
    return faces;
}

QString schema()
{
    return QStringLiteral("classmngr-phase9-font-allocation-probe-v2");
}

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
        else if (argument == "--static-face")
        {
            result.staticFacePaths.push_back(value);
        }
        else
        {
            return std::nullopt;
        }
    }

    const bool staticScenario = result.scenario == QStringLiteral("inter-static-directwrite");
    const bool ttcScenario = result.scenario == QStringLiteral("inter-ttc-directwrite")
        || result.scenario == QStringLiteral("inter-ttc-gdi");
    if ((!staticScenario && !ttcScenario)
        || (ttcScenario && result.interPath.isEmpty())
        || (staticScenario && result.staticFacePaths.size() != static_cast<qsizetype>(expectedFaces().size())))
    {
        return std::nullopt;
    }
    return result;
}

QString executablePath()
{
    std::vector<wchar_t> path(32768);
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
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

QString engineName(const QRawFont& rawFont)
{
    const QRawFontPrivate* const rawPrivate = QRawFontPrivate::get(rawFont);
    if (rawPrivate == nullptr || rawPrivate->fontEngine == nullptr)
    {
        return QStringLiteral("unavailable");
    }
    switch (rawPrivate->fontEngine->type())
    {
    case QFontEngine::Win:
        return QStringLiteral("Win");
    case QFontEngine::DirectWrite:
        return QStringLiteral("DirectWrite");
    case QFontEngine::Multi:
        return QStringLiteral("Multi");
    case QFontEngine::Freetype:
        return QStringLiteral("Freetype");
    case QFontEngine::Box:
        return QStringLiteral("Box");
    case QFontEngine::Mac:
        return QStringLiteral("Mac");
    case QFontEngine::QPF1:
        return QStringLiteral("QPF1");
    case QFontEngine::QPF2:
        return QStringLiteral("QPF2");
    case QFontEngine::Proxy:
        return QStringLiteral("Proxy");
    case QFontEngine::TestFontEngine:
        return QStringLiteral("TestFontEngine");
    }
    return QStringLiteral("unknown");
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
        QTextStream(stderr) << "ProcessMemorySnapshotProvider did not return working set and private usage.\n";
        return false;
    }

    const QJsonObject memory{
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
    const QJsonObject record{
        {QStringLiteral("schema"), QStringLiteral("classmngr-phase9-font-allocation-snapshot-v2")},
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

QJsonArray registrationRecords(const std::vector<StaticFace>& faces)
{
    QJsonArray result;
    for (const StaticFace& face : faces)
    {
        result.append(QJsonObject{
            {QStringLiteral("key"), face.key},
            {QStringLiteral("path"), face.path},
            {QStringLiteral("expectedStyle"), face.expectedStyle},
            {QStringLiteral("expectedWeight"), face.weight},
            {QStringLiteral("expectedItalic"), face.italic},
            {QStringLiteral("registrationId"), face.registrationId},
            {QStringLiteral("registeredFamilies"), stringArray(face.registeredFamilies)}
        });
    }
    return result;
}

QJsonArray resolveInterStyles()
{
    QJsonArray results;
    const QStringList allFamilies = QFontDatabase::families();
    QString interFamily;
    for (const QString& family : allFamilies)
    {
        if (family.compare(QStringLiteral("Inter"), Qt::CaseInsensitive) == 0)
        {
            interFamily = family;
            break;
        }
    }

    const QStringList availableStyles = interFamily.isEmpty()
        ? QStringList{}
        : QFontDatabase::styles(interFamily);
    for (const FaceExpectation& expected : expectedFaces())
    {
        QString matchedStyle;
        for (const QString& style : availableStyles)
        {
            if (QFontDatabase::weight(interFamily, style) == expected.weight
                && QFontDatabase::italic(interFamily, style) == expected.italic)
            {
                matchedStyle = style;
                break;
            }
        }

        QJsonObject resolved{
            {QStringLiteral("requestedFamily"), QStringLiteral("Inter")},
            {QStringLiteral("requestedStyle"), expected.style},
            {QStringLiteral("requestedWeight"), expected.weight},
            {QStringLiteral("requestedItalic"), expected.italic},
            {QStringLiteral("availableStyles"), stringArray(availableStyles)},
            {QStringLiteral("matchedStyle"), matchedStyle},
            {QStringLiteral("styleMatchFound"), !matchedStyle.isEmpty()}
        };
        if (!matchedStyle.isEmpty())
        {
            const QFont font = QFontDatabase::font(interFamily, matchedStyle, 14);
            const QRawFont rawFont = QRawFont::fromFont(font, QFontDatabase::Latin);
            resolved.insert(QStringLiteral("resolvedFontFamily"), rawFont.familyName());
            resolved.insert(QStringLiteral("resolvedFontStyle"), rawFont.styleName());
            resolved.insert(QStringLiteral("resolvedWeight"), rawFont.weight());
            resolved.insert(QStringLiteral("resolvedItalic"), rawFont.style() == QFont::StyleItalic);
            resolved.insert(QStringLiteral("resolvedRawFontValid"), rawFont.isValid());
            resolved.insert(QStringLiteral("engineType"), engineName(rawFont));
            const bool familySelectionMatches = interFamily.compare(
                QStringLiteral("Inter"), Qt::CaseInsensitive
                ) == 0;
            const bool weightMatches = rawFont.isValid() && rawFont.weight() == expected.weight;
            const bool italicMatches = rawFont.isValid()
                && (rawFont.style() == QFont::StyleItalic) == expected.italic;
            resolved.insert(QStringLiteral("familySelectionMatches"), familySelectionMatches);
            resolved.insert(QStringLiteral("resolvedFamilyNameIsLegacyAlias"), rawFont.isValid()
                && rawFont.familyName().compare(interFamily, Qt::CaseInsensitive) != 0);
            resolved.insert(QStringLiteral("weightMatches"), weightMatches);
            resolved.insert(QStringLiteral("italicMatches"), italicMatches);
            resolved.insert(
                QStringLiteral("identityMatches"),
                familySelectionMatches && weightMatches && italicMatches
                );
        }
        else
        {
            resolved.insert(QStringLiteral("resolvedRawFontValid"), false);
            resolved.insert(QStringLiteral("identityMatches"), false);
            resolved.insert(QStringLiteral("engineType"), QStringLiteral("unavailable"));
        }
        results.append(resolved);
    }
    return results;
}

int runProbe(const ProbeArguments& arguments)
{
    const QString processExecutablePath = executablePath();
    if (processExecutablePath.isEmpty())
    {
        QTextStream(stderr) << "Could not resolve the probe executable path.\n";
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
            processExecutablePath,
            {{QStringLiteral("qpaPlatformOption"), qEnvironmentVariable("QT_QPA_PLATFORM")}}
            ))
    {
        return 2;
    }

    char applicationName[] = "ClassMngrNextFontAllocationProbe";
    char* applicationArguments[] = {applicationName, nullptr};
    int applicationArgumentCount = 1;
    QApplication application(applicationArgumentCount, applicationArguments);
    const QString locale = QLocale::system().name();
    const QString actualPlatformName = QGuiApplication::platformName();
    const QString qpaPlatformOption = qEnvironmentVariable("QT_QPA_PLATFORM");
    if (!capture(
            provider,
            sequence,
            arguments.scenario,
            locale,
            QStringLiteral("after-qapplication"),
            processExecutablePath,
            {
                {QStringLiteral("qpaPlatformOption"), qpaPlatformOption},
                {QStringLiteral("actualPlatformName"), actualPlatformName}
            }
            ))
    {
        return 2;
    }

    const bool staticScenario = arguments.scenario == QStringLiteral("inter-static-directwrite");
    const bool gdiScenario = arguments.scenario == QStringLiteral("inter-ttc-gdi");
    std::vector<StaticFace> faces;
    QJsonArray registrationEvents;
    if (staticScenario)
    {
        for (std::size_t index = 0; index < expectedFaces().size(); ++index)
        {
            const FaceExpectation& expected = expectedFaces()[index];
            faces.push_back({
                expected.key,
                arguments.staticFacePaths.at(static_cast<qsizetype>(index)),
                expected.weight,
                expected.italic,
                expected.style
            });
        }
    }

    QString ttcFamily;
    int ttcRegistrationId = -1;
    if (!staticScenario)
    {
        if (!capture(
                provider,
                sequence,
                arguments.scenario,
                locale,
                QStringLiteral("before-inter-ttc-registration"),
                processExecutablePath,
                {{QStringLiteral("path"), arguments.interPath}}
                ))
        {
            return 2;
        }
        const auto started = Clock::now();
        ttcRegistrationId = QFontDatabase::addApplicationFont(arguments.interPath);
        const auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - started).count();
        if (ttcRegistrationId < 0)
        {
            QTextStream(stderr) << "Qt could not register packaged Inter TTC.\n";
            return 3;
        }
        if (!capture(
                provider,
                sequence,
                arguments.scenario,
                locale,
                QStringLiteral("after-inter-ttc-registration"),
                processExecutablePath,
                {
                    {QStringLiteral("path"), arguments.interPath},
                    {QStringLiteral("registrationId"), ttcRegistrationId},
                    {QStringLiteral("registrationApi"), QStringLiteral("QFontDatabase::addApplicationFont(file)")},
                    {QStringLiteral("operationDurationNanoseconds"), static_cast<qint64>(duration)}
                }
                ))
        {
            return 2;
        }
        if (!capture(
                provider,
                sequence,
                arguments.scenario,
                locale,
                QStringLiteral("before-inter-ttc-family-enumeration"),
                processExecutablePath,
                {{QStringLiteral("registrationId"), ttcRegistrationId}}
                ))
        {
            return 2;
        }
        const QStringList families = QFontDatabase::applicationFontFamilies(ttcRegistrationId);
        for (const QString& family : families)
        {
            if (family.compare(QStringLiteral("Inter"), Qt::CaseInsensitive) == 0)
            {
                ttcFamily = family;
                break;
            }
        }
        if (!capture(
                provider,
                sequence,
                arguments.scenario,
                locale,
                QStringLiteral("after-inter-ttc-family-enumeration"),
                processExecutablePath,
                {
                    {QStringLiteral("registrationId"), ttcRegistrationId},
                    {QStringLiteral("familyNames"), stringArray(families)},
                    {QStringLiteral("selectedInterFamily"), ttcFamily}
                }
                ))
        {
            return 2;
        }
    }
    else
    {
        for (std::size_t index = 0; index < faces.size(); ++index)
        {
            StaticFace& face = faces[index];
            const QString eventBase = QStringLiteral("static-face-%1-registration").arg(face.key);
            if (!capture(
                    provider,
                    sequence,
                    arguments.scenario,
                    locale,
                    QStringLiteral("before-") + eventBase,
                    processExecutablePath,
                    {
                        {QStringLiteral("faceKey"), face.key},
                        {QStringLiteral("faceIndex"), static_cast<int>(index)},
                        {QStringLiteral("path"), face.path},
                        {QStringLiteral("expectedStyle"), face.expectedStyle},
                        {QStringLiteral("expectedWeight"), face.weight},
                        {QStringLiteral("expectedItalic"), face.italic}
                    }
                    ))
            {
                return 2;
            }
            const auto started = Clock::now();
            face.registrationId = QFontDatabase::addApplicationFont(face.path);
            const auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - started).count();
            if (face.registrationId < 0)
            {
                QTextStream(stderr) << "Qt could not register Inter static face: " << face.path << "\n";
                return 3;
            }
            if (!capture(
                    provider,
                    sequence,
                    arguments.scenario,
                    locale,
                    QStringLiteral("after-") + eventBase,
                    processExecutablePath,
                    {
                        {QStringLiteral("faceKey"), face.key},
                        {QStringLiteral("faceIndex"), static_cast<int>(index)},
                        {QStringLiteral("path"), face.path},
                        {QStringLiteral("expectedStyle"), face.expectedStyle},
                        {QStringLiteral("expectedWeight"), face.weight},
                        {QStringLiteral("expectedItalic"), face.italic},
                        {QStringLiteral("registrationId"), face.registrationId},
                        {QStringLiteral("registrationApi"), QStringLiteral("QFontDatabase::addApplicationFont(file)")},
                        {QStringLiteral("operationDurationNanoseconds"), static_cast<qint64>(duration)}
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
                QStringLiteral("before-static-face-family-enumeration"),
                processExecutablePath,
                {{QStringLiteral("registrationCount"), static_cast<int>(faces.size())}}
                ))
        {
            return 2;
        }
        for (StaticFace& face : faces)
        {
            face.registeredFamilies = QFontDatabase::applicationFontFamilies(face.registrationId);
            if (face.registeredFamilies.contains(QStringLiteral("Inter"), Qt::CaseInsensitive))
            {
                ttcFamily = QStringLiteral("Inter");
            }
        }
        registrationEvents = registrationRecords(faces);
        if (!capture(
                provider,
                sequence,
                arguments.scenario,
                locale,
                QStringLiteral("after-static-face-family-enumeration"),
                processExecutablePath,
                {{QStringLiteral("registrations"), registrationEvents}}
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
            QStringLiteral("before-style-resolution"),
            processExecutablePath,
            {
                {QStringLiteral("qpaPlatformOption"), qpaPlatformOption},
                {QStringLiteral("actualPlatformName"), actualPlatformName},
                {QStringLiteral("selectedInterFamily"), ttcFamily},
                {QStringLiteral("registrationCount"), staticScenario ? static_cast<int>(faces.size()) : 1}
            }
            ))
    {
        return 2;
    }
    const auto styleStarted = Clock::now();
    const QJsonArray styleResolutions = resolveInterStyles();
    const auto styleDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - styleStarted).count();
    bool allStylesMatch = styleResolutions.size() == static_cast<qsizetype>(expectedFaces().size());
    QJsonArray selectedEngines;
    for (const QJsonValue& value : styleResolutions)
    {
        const QJsonObject style = value.toObject();
        allStylesMatch = allStylesMatch && style.value(QStringLiteral("identityMatches")).toBool();
        selectedEngines.append(style.value(QStringLiteral("engineType")));
    }
    if (!capture(
            provider,
            sequence,
            arguments.scenario,
            locale,
            QStringLiteral("after-style-resolution"),
            processExecutablePath,
            {
                {QStringLiteral("selectedInterFamily"), ttcFamily},
                {QStringLiteral("allStylesMatchExpectedIdentity"), allStylesMatch},
                {QStringLiteral("styleResolutions"), styleResolutions},
                {QStringLiteral("selectedEngineTypes"), selectedEngines},
                {QStringLiteral("operationDurationNanoseconds"), static_cast<qint64>(styleDuration)}
            }
            ))
    {
        return 2;
    }

    const QJsonObject completion{
        {QStringLiteral("schema"), schema()},
        {QStringLiteral("recordType"), QStringLiteral("complete")},
        {QStringLiteral("scenario"), arguments.scenario},
        {QStringLiteral("pid"), static_cast<qint64>(GetCurrentProcessId())},
        {QStringLiteral("architecture"), architecture()},
        {QStringLiteral("executablePath"), processExecutablePath},
        {QStringLiteral("qtVersion"), QStringLiteral(QT_VERSION_STR)},
        {QStringLiteral("locale"), locale},
        {QStringLiteral("snapshotCount"), sequence},
        {QStringLiteral("qpaPlatformOption"), qpaPlatformOption},
        {QStringLiteral("actualPlatformName"), actualPlatformName},
        {QStringLiteral("selectedInterFamily"), ttcFamily},
        {QStringLiteral("allStylesMatchExpectedIdentity"), allStylesMatch},
        {QStringLiteral("ttcRegistrationId"), ttcRegistrationId},
        {QStringLiteral("staticFaceRegistrations"), registrationEvents},
        {QStringLiteral("staticFacePaths"), QJsonArray::fromStringList(arguments.staticFacePaths)},
        {QStringLiteral("backendExpectation"), gdiScenario ? QStringLiteral("Win") : QStringLiteral("DirectWrite")}
    };
    QTextStream output(stdout);
    output << QJsonDocument(completion).toJson(QJsonDocument::Compact) << Qt::endl;
    return 0;
}

}

int main(int argc, char** argv)
{
    const auto arguments = parseArguments(argc, argv);
    if (!arguments)
    {
        QTextStream(stderr) << "Expected a known --scenario, --inter for TTC arms, or six --static-face paths.\n";
        return 1;
    }
    return runProbe(*arguments);
}
