#include "typed_signature_renderer.h"

#include "features/my_info/data/signature_image_processor.h"

#include <QFontMetricsF>
#include <QPainter>

#include <array>
#include <utility>

namespace
{
struct SignatureFontDefinition
{
    TypedSignatureFont font;
    ClassMngr::Core::FontResource resource;
    QString fallbackFamily;
};

const std::array<SignatureFontDefinition, 4>& fontDefinitions()
{
    static const std::array definitions = {
        SignatureFontDefinition{
            TypedSignatureFont::JustAnotherHand,
            ClassMngr::Core::FontResource::JustAnotherHand,
            QStringLiteral("cursive")
        },
        SignatureFontDefinition{
            TypedSignatureFont::DancingScript,
            ClassMngr::Core::FontResource::DancingScript,
            QStringLiteral("cursive")
        },
        SignatureFontDefinition{
            TypedSignatureFont::GreatVibes,
            ClassMngr::Core::FontResource::GreatVibes,
            QStringLiteral("cursive")
        },
        SignatureFontDefinition{
            TypedSignatureFont::Caveat,
            ClassMngr::Core::FontResource::Caveat,
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

}

TypedSignatureFontSet TypedSignatureFontSet::load(
    ClassMngr::Core::FontRegistrationPort& port)
{
    TypedSignatureFontSet result;
    for (const auto& definition : fontDefinitions())
    {
        auto registration = port.loadFont(definition.resource);
        if (registration)
        {
            result.m_registrations.at(indexFor(definition.font)) =
                std::move(*registration);
        }
    }
    return result;
}

QString TypedSignatureFontSet::familyFor(const TypedSignatureFont font) const
{
    const auto& registration = m_registrations.at(indexFor(font));
    return registration && !registration->familyName().isEmpty()
        ? registration->familyName()
        : definitionFor(font).fallbackFamily;
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
    int pixelSize,
    const TypedSignatureFontSet* fontSet
    )
{
    QFont result(fontSet
        ? fontSet->familyFor(font)
        : definitionFor(font).fallbackFamily);
    result.setPixelSize(qMax(1, pixelSize));
    result.setStyleStrategy(QFont::PreferAntialias);
    result.setHintingPreference(QFont::PreferFullHinting);

    return result;
}

QImage render(
    const QString& text,
    TypedSignatureFont font,
    const QSize& size,
    const TypedSignatureFontSet* fontSet
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
    QFont signatureFont = fontFor(font, pixelSize, fontSet);
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
        signatureFont = fontFor(font, pixelSize, fontSet);
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
    TypedSignatureFont font,
    const TypedSignatureFontSet* fontSet
    )
{
    if (text.trimmed().isEmpty())
    {
        return {};
    }

    return SignatureImage::prepareForEmbedding(
        render(text, font, QSize(1600, 500), fontSet)
        );
}
}
