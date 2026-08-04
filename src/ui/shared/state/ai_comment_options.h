#pragma once

#include "ui/shared/constants/options.h"

#include <QString>
#include <QUrl>

[[nodiscard]] QString aiCommentProviderName(
    AiCommentProvider provider
    );

[[nodiscard]] bool isValidCustomAiWebsiteUrl(
    const QString& value
    );

[[nodiscard]] QUrl aiCommentProviderUrl(
    AiCommentProvider provider,
    const QString& customWebsiteUrl = {}
    );
