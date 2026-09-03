#include "classmngr/engine/resource_pack_policy.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <utility>

namespace classmngr::engine
{
namespace
{
constexpr int ManifestSchemaVersion = 1;
constexpr int InstalledMetadataSchemaVersion = 1;

std::string trim(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.remove_suffix(1);
    return std::string(value);
}

Result<ResourcePackManifest> fail(std::string message)
{
    return std::unexpected(Error{ErrorCode::InvalidFormat, std::move(message), std::nullopt});
}

bool equalsIgnoreCase(std::string_view lhs, std::string_view rhs)
{
    if (lhs.size() != rhs.size()) return false;
    for (std::size_t i = 0; i < lhs.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(lhs[i])) != std::tolower(static_cast<unsigned char>(rhs[i]))) return false;
    return true;
}
}

const ResourcePackArtifact* ResourcePackManifest::find(std::string_view id) const
{
    const auto item = std::find_if(artifacts.begin(), artifacts.end(), [id](const auto& artifact) { return artifact.id == id; });
    return item == artifacts.end() ? nullptr : &*item;
}

bool ResourcePackPolicy::isValidPackId(std::string_view id)
{
    if (id.empty() || id.front() == '-' || id.back() == '-') return false;
    bool previousDash = false;
    for (const char c : id) {
        const bool dash = c == '-';
        if (!dash && !(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9')) return false;
        if (dash && previousDash) return false;
        previousDash = dash;
    }
    return true;
}

bool ResourcePackPolicy::isAllowedHttpsUrl(std::string_view url)
{
    const std::string normalized = trim(url);
    const std::size_t schemeEnd = normalized.find("://");
    if (schemeEnd == std::string::npos
        || !equalsIgnoreCase(
            std::string_view(normalized).substr(0, schemeEnd),
            "https"
            ))
    {
        return false;
    }
    const std::string_view authorityAndPath(
        normalized.data() + schemeEnd + 3,
        normalized.size() - schemeEnd - 3
        );
    const std::size_t endAuthority = authorityAndPath.find_first_of("/?#");
    const std::string_view authority = authorityAndPath.substr(0, endAuthority);
    if (authority.empty()
        || authority.find('@') != std::string_view::npos
        || authority.find_first_of(" \r\n\t\\") != std::string_view::npos)
    {
        return false;
    }

    std::string_view host = authority;
    std::string_view port;
    if (authority.front() == '[')
    {
        const std::size_t closingBracket = authority.find(']');
        if (closingBracket == std::string_view::npos || closingBracket == 1)
        {
            return false;
        }
        host = authority.substr(0, closingBracket + 1);
        if (closingBracket + 1 < authority.size())
        {
            if (authority[closingBracket + 1] != ':')
            {
                return false;
            }
            port = authority.substr(closingBracket + 2);
        }
    }
    else if (const std::size_t colon = authority.rfind(':');
             colon != std::string_view::npos)
    {
        host = authority.substr(0, colon);
        port = authority.substr(colon + 1);
    }

    if (host.empty() || host.front() == '.' || host.back() == '.')
    {
        return false;
    }
    if (!port.empty())
    {
        unsigned int value = 0;
        for (const char character : port)
        {
            if (character < '0' || character > '9')
            {
                return false;
            }
            value = value * 10U + static_cast<unsigned int>(character - '0');
            if (value > 65535U)
            {
                return false;
            }
        }
        if (value == 0U)
        {
            return false;
        }
    }
    else if (authority.back() == ':')
    {
        return false;
    }

    for (std::size_t index = 0; index < normalized.size(); ++index)
    {
        if (normalized[index] != '%')
        {
            continue;
        }
        if (index + 2 >= normalized.size()
            || !std::isxdigit(static_cast<unsigned char>(normalized[index + 1]))
            || !std::isxdigit(static_cast<unsigned char>(normalized[index + 2])))
        {
            return false;
        }
        index += 2;
    }
    return true;
}

bool ResourcePackPolicy::isPlainRccFileName(std::string_view fileName)
{
    const std::string value = trim(fileName);
    if (value.empty() || value == "." || value == ".." || value.find_first_of("/\\") != std::string::npos || value.find(':') != std::string::npos) return false;
    return value.size() > 4 && equalsIgnoreCase(std::string_view(value).substr(value.size() - 4), ".rcc");
}

bool ResourcePackPolicy::isSha256(std::string_view digest)
{
    const std::string value = trim(digest);
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
}

Result<ResourcePackManifest> ResourcePackPolicy::validateManifest(const ResourcePackRawManifest& raw, const std::vector<std::string>& requiredIds)
{
    if (raw.schemaVersion != ManifestSchemaVersion) return fail("Unsupported resource-pack manifest schema version.");
    if (raw.artifacts.empty()) return fail("Resource-pack manifest must include at least one pack.");
    ResourcePackManifest result{raw.schemaVersion, {}};
    std::set<std::string> ids;
    for (const ResourcePackRawArtifact& item : raw.artifacts) {
        const std::string id = trim(item.id);
        if (!isValidPackId(id) || !ids.insert(id).second) return fail("Resource-pack id is invalid or duplicated.");
        const auto version = SemanticVersion::parse(item.version);
        if (!version) return fail("Resource pack '" + id + "' has an invalid version.");
        if (!isAllowedHttpsUrl(item.url)) return fail("Resource pack '" + id + "' must use an HTTPS download URL.");
        if (!isPlainRccFileName(item.fileName)) return fail("Resource pack '" + id + "' fileName must be a plain .rcc file name.");
        if (!isSha256(item.sha256)) return fail("Resource pack '" + id + "' has an invalid SHA-256 checksum.");
        if (item.sizeBytes <= 0) return fail("Resource pack '" + id + "' sizeBytes must be a positive integer.");
        std::string digest = trim(item.sha256);
        std::transform(digest.begin(), digest.end(), digest.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        result.artifacts.push_back({id, *version, trim(item.url), trim(item.fileName), std::move(digest), item.sizeBytes});
    }
    for (const std::string& id : requiredIds) if (result.find(id) == nullptr) return fail("Resource-pack manifest is missing '" + id + "'.");
    return result;
}

Result<InstalledResourcePackMetadata> ResourcePackPolicy::validateInstalledMetadata(const InstalledResourcePackMetadata& raw, const ResourcePackDefinition& definition)
{
    const auto version = SemanticVersion::parse(raw.version);
    if (raw.schemaVersion != InstalledMetadataSchemaVersion || raw.id != definition.id || !version || *version <= definition.baselineVersion || !isPlainRccFileName(raw.fileName) || !isSha256(raw.sha256)) {
        return std::unexpected(Error{ErrorCode::InvalidFormat, "Metadata for resource pack '" + definition.id + "' is invalid.", std::nullopt});
    }
    InstalledResourcePackMetadata result = raw;
    result.fileName = trim(result.fileName);
    result.sha256 = trim(result.sha256);
    std::transform(result.sha256.begin(), result.sha256.end(), result.sha256.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

ResourcePackSelection ResourcePackPolicy::select(const ResourcePackDefinition& definition, const std::optional<InstalledResourcePackMetadata>& installed, bool installedIntegrityValid)
{
    if (!installed || !definition.updateable) return {ResourcePackSource::Baseline, definition.baselineVersion, false};
    const auto validated = validateInstalledMetadata(*installed, definition);
    if (!validated || !installedIntegrityValid) return {ResourcePackSource::Baseline, definition.baselineVersion, true};
    const auto version = SemanticVersion::parse(validated->version);
    if (!version) return {ResourcePackSource::Baseline, definition.baselineVersion, true};
    return {ResourcePackSource::Installed, *version, false};
}

bool ResourcePackPolicy::shouldDownload(const ResourcePackArtifact& artifact, const ResourcePackDefinition& definition, const SemanticVersion& currentVersion)
{
    return definition.updateable && artifact.id == definition.id && artifact.version > currentVersion;
}

bool ResourcePackPolicy::acceptsDownload(const ResourcePackArtifact& artifact, long long actualSizeBytes, std::string_view actualSha256)
{
    return actualSizeBytes == artifact.sizeBytes && equalsIgnoreCase(trim(actualSha256), artifact.sha256);
}

bool ResourcePackPolicy::signatureRequired(bool configuredRequired, std::string_view publicKeyPem)
{
    static_cast<void>(publicKeyPem);
    return configuredRequired;
}

Result<std::string> ResourcePackPolicy::normalizeCampusResourceReference(
    std::string_view reference,
    std::string_view activeCampusRoot
    )
{
    std::string value = trim(reference);
    std::replace(value.begin(), value.end(), '\\', '/');
    constexpr std::string_view legacy = ":/assets/campuses/";
    constexpr std::string_view active = ":/resource-packs/campuses/";
    constexpr std::string_view qrcLegacy = "qrc:/assets/campuses/";
    if (value.starts_with(qrcLegacy)) value = std::string(legacy) + value.substr(qrcLegacy.size());
    const std::string_view prefix = value.starts_with(legacy) ? legacy : (value.starts_with(active) ? active : std::string_view{});
    if (prefix.empty()) return value;
    const std::string_view suffix(value.data() + prefix.size(), value.size() - prefix.size());
    if (suffix.empty() || suffix.front() == '/' || suffix.find("//") != std::string_view::npos) {
        return std::unexpected(Error{ErrorCode::InvalidFormat, "Campus resource reference is empty or unsafe.", std::nullopt});
    }
    std::size_t start = 0;
    while (start < suffix.size()) {
        const std::size_t end = suffix.find('/', start);
        const std::string_view part = suffix.substr(start, end == std::string_view::npos ? suffix.size() - start : end - start);
        if (part.empty() || part == "." || part == "..") return std::unexpected(Error{ErrorCode::InvalidFormat, "Campus resource reference escapes its root.", std::nullopt});
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    const std::string root = trim(activeCampusRoot);
    return root.empty() ? value : root + "/" + std::string(suffix);
}

} // namespace classmngr::engine
