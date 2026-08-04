#include "ai_comment_options.h"

QString aiCommentProviderName(
    AiCommentProvider provider
    )
{
    switch (provider)
    {
    case AiCommentProvider::Gemini:
        return QStringLiteral("Gemini");
    case AiCommentProvider::Claude:
        return QStringLiteral("Claude");
    case AiCommentProvider::MicrosoftCopilot:
        return QStringLiteral("Microsoft Copilot");
    case AiCommentProvider::CustomWebsite:
        return QStringLiteral("Custom AI Website");
    case AiCommentProvider::ChatGPT:
    default:
        return QStringLiteral("ChatGPT");
    }
}

bool isValidCustomAiWebsiteUrl(
    const QString& value
    )
{
    const QUrl url(value.trimmed());
    return url.isValid()
        && url.scheme().compare(
            QStringLiteral("https"),
            Qt::CaseInsensitive
            ) == 0
        && !url.host().isEmpty();
}

QUrl aiCommentProviderUrl(
    AiCommentProvider provider,
    const QString& customWebsiteUrl
    )
{
    switch (provider)
    {
    case AiCommentProvider::Gemini:
        return QUrl(QStringLiteral("https://gemini.google.com/app"));
    case AiCommentProvider::Claude:
        return QUrl(QStringLiteral("https://claude.ai/"));
    case AiCommentProvider::MicrosoftCopilot:
        return QUrl(QStringLiteral("https://copilot.microsoft.com/"));
    case AiCommentProvider::CustomWebsite:
        return isValidCustomAiWebsiteUrl(customWebsiteUrl)
            ? QUrl(customWebsiteUrl.trimmed())
            : QUrl();
    case AiCommentProvider::ChatGPT:
    default:
        return QUrl(QStringLiteral("https://chatgpt.com/"));
    }
}
