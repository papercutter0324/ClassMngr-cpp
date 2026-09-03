#pragma once

#include "classmngr/engine/result.h"
#include "classmngr/engine/semantic_version.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace classmngr::engine
{

struct ResourcePackRawArtifact
{
    std::string id;
    std::string version;
    std::string url;
    std::string fileName;
    std::string sha256;
    long long sizeBytes = 0;
};

struct ResourcePackRawManifest
{
    int schemaVersion = 0;
    std::vector<ResourcePackRawArtifact> artifacts;
};

struct ResourcePackArtifact
{
    std::string id;
    SemanticVersion version;
    std::string url;
    std::string fileName;
    std::string sha256;
    long long sizeBytes = 0;
};

struct ResourcePackManifest
{
    int schemaVersion = 0;
    std::vector<ResourcePackArtifact> artifacts;

    [[nodiscard]] const ResourcePackArtifact* find(std::string_view id) const;
};

struct ResourcePackDefinition
{
    std::string id;
    SemanticVersion baselineVersion;
    bool updateable = false;
};

struct InstalledResourcePackMetadata
{
    int schemaVersion = 0;
    std::string id;
    std::string version;
    std::string fileName;
    std::string sha256;
};

enum class ResourcePackSource { Baseline, Installed };

struct ResourcePackSelection
{
    ResourcePackSource source = ResourcePackSource::Baseline;
    SemanticVersion version;
    bool discardInstalled = false;
};

class ResourcePackPolicy final
{
public:
    [[nodiscard]] static Result<ResourcePackManifest> validateManifest(
        const ResourcePackRawManifest& raw,
        const std::vector<std::string>& requiredIds = {}
        );
    [[nodiscard]] static Result<InstalledResourcePackMetadata> validateInstalledMetadata(
        const InstalledResourcePackMetadata& raw,
        const ResourcePackDefinition& definition
        );
    [[nodiscard]] static ResourcePackSelection select(
        const ResourcePackDefinition& definition,
        const std::optional<InstalledResourcePackMetadata>& installed,
        bool installedIntegrityValid
        );
    [[nodiscard]] static bool shouldDownload(
        const ResourcePackArtifact& artifact,
        const ResourcePackDefinition& definition,
        const SemanticVersion& currentVersion
        );
    [[nodiscard]] static bool acceptsDownload(
        const ResourcePackArtifact& artifact,
        long long actualSizeBytes,
        std::string_view actualSha256
        );
    [[nodiscard]] static bool signatureRequired(
        bool configuredRequired,
        std::string_view publicKeyPem
        );
    [[nodiscard]] static bool isAllowedHttpsUrl(std::string_view url);
    [[nodiscard]] static bool isPlainRccFileName(std::string_view fileName);
    [[nodiscard]] static bool isValidPackId(std::string_view id);
    [[nodiscard]] static bool isSha256(std::string_view digest);
    [[nodiscard]] static Result<std::string> normalizeCampusResourceReference(
        std::string_view reference,
        std::string_view activeCampusRoot
        );
};

} // namespace classmngr::engine
