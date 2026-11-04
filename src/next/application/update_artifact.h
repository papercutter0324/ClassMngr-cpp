#pragma once

#include "next/application/encoded_url_text.h"
#include "next/application/update_artifact_file_name.h"
#include "next/application/update_artifact_platform_key.h"
#include "next/application/update_artifact_sha256.h"
#include "next/application/update_artifact_size_bytes.h"

namespace ClassMngr::Next::Application
{

// Shared application value for a release package. Update-check contracts keep
// their existing result types; this value is used by download and cleanup
// operations that are shared across those check paths.
struct UpdateArtifact final
{
    UpdateArtifactPlatformKey platformKey;
    EncodedUrlText url;
    UpdateArtifactFileName fileName;
    UpdateArtifactSha256 sha256;
    UpdateArtifactSizeBytes sizeBytes;
};

} // namespace ClassMngr::Next::Application
