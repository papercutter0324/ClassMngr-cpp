#include "classmngr/engine/resource_pack_policy.h"

#include <iostream>
#include <string>

using namespace classmngr::engine;

namespace
{
bool expect(bool condition, const char* message)
{
    if (!condition) std::cerr << "ClassMngrEngineResourcePackPolicyTests: " << message << '\n';
    return condition;
}

ResourcePackRawArtifact artifact()
{
    return {"campuses", "1.2.3", "https://updates.example.test/campuses.rcc", "campuses.rcc", std::string(64, 'a'), 42};
}
}

int main()
{
    bool passed = true;
    const auto manifest = ResourcePackPolicy::validateManifest({1, {artifact()}}, {"campuses"});
    passed &= expect(manifest.has_value() && manifest->find("campuses") != nullptr, "valid manifest was rejected");
    auto unsafe = artifact(); unsafe.url = "http://updates.example.test/pack.rcc";
    passed &= expect(!ResourcePackPolicy::validateManifest({1, {unsafe}}).has_value(), "HTTP artifact URL was accepted");
    unsafe = artifact(); unsafe.url = "HTTPS://updates.example.test:443/campuses.rcc";
    passed &= expect(ResourcePackPolicy::validateManifest({1, {unsafe}}).has_value(), "uppercase HTTPS URL was rejected");
    unsafe = artifact(); unsafe.url = "https://updates.example.test:not-a-port/campuses.rcc";
    passed &= expect(!ResourcePackPolicy::validateManifest({1, {unsafe}}).has_value(), "malformed HTTPS port was accepted");
    unsafe = artifact(); unsafe.url = "https://updates.example.test/%zz";
    passed &= expect(!ResourcePackPolicy::validateManifest({1, {unsafe}}).has_value(), "malformed HTTPS escape was accepted");
    unsafe = artifact(); unsafe.fileName = "../pack.rcc";
    passed &= expect(!ResourcePackPolicy::validateManifest({1, {unsafe}}).has_value(), "unsafe RCC filename was accepted");
    unsafe = artifact(); unsafe.sha256 = "abcd";
    passed &= expect(!ResourcePackPolicy::validateManifest({1, {unsafe}}).has_value(), "invalid digest was accepted");
    unsafe = artifact(); unsafe.sizeBytes = 0;
    passed &= expect(!ResourcePackPolicy::validateManifest({1, {unsafe}}).has_value(), "zero-size artifact was accepted");

    const ResourcePackDefinition definition{"campuses", SemanticVersion(1, 0, 0), true};
    const InstalledResourcePackMetadata installed{1, "campuses", "1.2.3", "campuses-1.2.3.rcc", std::string(64, 'B')};
    const auto validInstalled = ResourcePackPolicy::validateInstalledMetadata(installed, definition);
    passed &= expect(validInstalled.has_value(), "valid installed metadata was rejected");
    const auto selection = ResourcePackPolicy::select(definition, installed, true);
    passed &= expect(selection.source == ResourcePackSource::Installed && selection.version == SemanticVersion(1, 2, 3), "installed pack was not selected");
    const auto discarded = ResourcePackPolicy::select(definition, installed, false);
    passed &= expect(discarded.source == ResourcePackSource::Baseline && discarded.discardInstalled, "invalid installed pack was not discarded");
    if (manifest) {
        passed &= expect(ResourcePackPolicy::shouldDownload(*manifest->find("campuses"), definition, SemanticVersion(1, 0, 0)), "newer update was not eligible");
        passed &= expect(ResourcePackPolicy::acceptsDownload(*manifest->find("campuses"), 42, std::string(64, 'A')), "matching download was rejected");
        passed &= expect(!ResourcePackPolicy::acceptsDownload(*manifest->find("campuses"), 41, std::string(64, 'A')), "wrong-size download was accepted");
    }
    passed &= expect(ResourcePackPolicy::signatureRequired(true, "public key") && ResourcePackPolicy::signatureRequired(true, " ") && !ResourcePackPolicy::signatureRequired(false, "public key"), "signature requirement policy changed");
    const auto rebased = ResourcePackPolicy::normalizeCampusResourceReference(
        ":/assets/campuses/bundang/map.png", ":/resource-packs/campuses");
    passed &= expect(rebased && *rebased == ":/resource-packs/campuses/bundang/map.png", "legacy campus reference was not rebased");
    passed &= expect(!ResourcePackPolicy::normalizeCampusResourceReference(
        ":/assets/campuses/../private.png", ":/resource-packs/campuses"), "campus root escape was accepted");
    return passed ? 0 : 1;
}
