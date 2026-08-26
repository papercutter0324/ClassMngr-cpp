#include "typed_signature_renderer.h"

#include "core/resource_paths.h"
#include "features/my_info/data/signature_image_processor.h"

#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QPainter>

#include <array>

namespace
{
struct SignatureFontDefinition
{
    TypedSignatureFont font;
    QString (*resourcePath)();
    QString fallbackFamily;
};

const std::array<SignatureFontDefinition, 4>& fontDefinitions()
{
    static const std::array definitions = {
        SignatureFontDefinition{
            TypedSignatureFont::JustAnotherHand,
            ResourcePaths::Fonts::justAnotherHand,
            QStringLiteral("cursive")
        },
        SignatureFontDefinition{
            TypedSignatureFont::DancingScript,
            ResourcePaths::Fonts::dancingScript,
            QStringLiteral("cursive")
        },
        SignatureFontDefinition{
            TypedSignatureFont::GreatVibes,
            ResourcePaths::Fonts::greatVibes,
            QStringLiteral("cursive")
        },
        SignatureFontDefinition{
            TypedSignatureFont::Caveat,
            ResourcePaths::Fonts::caveat,
            QStringLiteral("cursive")
        }
    };

    return definitions;
}

int indexFor(TypedSignatureFont font)
{
    return static_cast<int>(font);
}

const SignatureFontDefinition& definitionFor(TypedSignatureFont font)
{
    return fontDefinitions().at(indexFor(font));
}

std::array<QString, 4>& loadedFamilies()
{
    static std::array<QString, 4> families;
    return families;
}

void ensureFontsLoaded()
{
    static bool loaded = false;

    if (loaded || !QGuiApplication::instance())
    {
        return;
    }

    const auto& definitions = fontDefinitions();
    auto& families = loadedFamilies();

    for (int index = 0;
         index < static_cast<int>(definitions.size());
         ++index)
    {
        const int fontId = QFontDatabase::addApplicationFont(
            definitions.at(index).resourcePath()
            );

        if (fontId < 0)
        {
            continue;
        }

        const QStringList fontFamilies =
            QFontDatabase::applicationFontFamilies(fontId);

        if (!fontFamilies.isEmpty())
        {
            families.at(index) = fontFamilies.first();
        }
    }

    loaded = true;
}

QString familyFor(TypedSignatureFont font)
{
    ensureFontsLoaded();

    const QString& family = loadedFamilies().at(indexFor(font));

    return family.isEmpty()
        ? definitionFor(font).fallbackFamily
        : family;
}
}

namespace TypedSignature
{
TypedSignatureFont fontFromStoredValue(int value)
{
    return value >= indexFor(TypedSignatureFont::JustAnotherHand)
        && value <= indexFor(TypedSignatureFont::Caveat)
        ? static_cast<TypedSignatureFont>(value)
        : TypedSignatureFont::JustAnotherHand;
}

QString displayName(TypedSignatureFont font)
{
    switch (font)
    {
    case TypedSignatureFont::JustAnotherHand:
        return QStringLiteral("Just Another Hand");
    case TypedSignatureFont::DancingScript:
        return QStringLiteral("Dancing Script");
    case TypedSignatureFont::GreatVibes:
        return QStringLiteral("Great Vibes");
    case TypedSignatureFont::Caveat:
        return QStringLiteral("Caveat");
    }

    return QString();
}

QFont fontFor(
    TypedSignatureFont font,
    int pixelSize
    )
{
    QFont result(familyFor(font));
    result.setPixelSize(qMax(1, pixelSize));
    result.setStyleStrategy(QFont::PreferAntialias);
    result.setHintingPreference(QFont::PreferFullHinting);

    return result;
}

QImage render(
    const QString& text,
    TypedSignatureFont font,
    const QSize& size
    )
{
    if (text.trimmed().isEmpty() || size.isEmpty())
    {
        return {};
    }

    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    const QString signatureText = text.trimmed();
    const int horizontalMargin = qMax(12, size.width() / 24);
    const int verticalMargin = qMax(10, size.height() / 10);
    const QSize contentSize(
        qMax(1, size.width() - (horizontalMargin * 2)),
        qMax(1, size.height() - (verticalMargin * 2))
        );

    int pixelSize = qMax(18, contentSize.height() * 2 / 3);
    QFont signatureFont = fontFor(font, pixelSize);
    QFontMetricsF metrics(signatureFont);
    const qreal advance = metrics.horizontalAdvance(signatureText);

    if (advance > contentSize.width())
    {
        pixelSize = qMax(
            18,
            static_cast<int>(
                (pixelSize * contentSize.width()) / advance
                )
            );
        signatureFont = fontFor(font, pixelSize);
        metrics = QFontMetricsF(signatureFont);
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setPen(Qt::black);
    painter.setFont(signatureFont);

    const qreal baseline =
        verticalMargin
        + ((contentSize.height() - metrics.height()) / 2.0)
        + metrics.ascent();

    painter.drawText(
        QPointF(horizontalMargin, baseline),
        signatureText
        );

    return image;
}

QByteArray renderForEmbedding(
    const QString& text,
    TypedSignatureFont font
    )
{
    if (text.trimmed().isEmpty())
    {
        return {};
    }

    return SignatureImage::prepareForEmbedding(
        render(text, font, QSize(1600, 500))
        );
}
}
