#include "features/speaking_eval/ui/speaking_eval_report_widget.h"

#include "features/speaking_eval/ui/speaking_eval_report_assets_p.h"

#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QTextLayout>
#include <QTextOption>

#include <algorithm>

namespace
{
QRectF contentRect(
    const SpeakingEvalFieldAsset& field
    )
{
    return field.rect.marginsRemoved(field.margins);
}

QFont fieldFont(
    const SpeakingEvalFieldAsset& field,
    qreal pointSize
    )
{
    QFont font =
        speakingEvalTemplateFont(
            field.fontRole,
            pointSize
            );
    font.setLetterSpacing(
        QFont::AbsoluteSpacing,
        field.letterSpacing
        );
    font.setWordSpacing(field.wordSpacing);
    return font;
}

qreal singleLineWidth(
    const QFont& font,
    const QString& text
    )
{
    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::NoWrap);
    layout.setTextOption(option);
    layout.beginLayout();
    const QTextLine line = layout.createLine();
    layout.endLayout();
    return line.isValid()
        ? line.naturalTextWidth()
        : 0.0;
}

qreal wrappedTextHeight(
    const SpeakingEvalFieldAsset& field,
    const QFont& font,
    const QString& text,
    qreal width
    )
{
    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    qreal nextTop = 0.0;
    qreal height = 0.0;
    while (true)
    {
        QTextLine line = layout.createLine();
        if (!line.isValid())
        {
            break;
        }
        line.setLineWidth(width);
        line.setPosition(QPointF(0.0, nextTop));
        height = nextTop + line.height();
        nextTop += line.height() * field.lineHeight;
    }
    layout.endLayout();
    return height;
}

QFont fittedFieldFont(
    const SpeakingEvalFieldAsset& field,
    const QString& text
    )
{
    const QRectF available = contentRect(field);
    const int maximumPointSize =
        qMax(1, qRound(field.fontSizePoints));
    const int minimumPointSize =
        qMax(
            1,
            qCeil(field.fontSizePoints * field.minimumScale)
            );

    for (
        int pointSize = maximumPointSize;
        pointSize >= minimumPointSize;
        --pointSize
        )
    {
        const QFont font = fieldFont(field, pointSize);
        const QFontMetricsF metrics(font);
        const bool fits =
            field.comments
                ? wrappedTextHeight(
                    field,
                    font,
                    text,
                    available.width()
                    ) <= available.height()
                : singleLineWidth(font, text) <= available.width()
                    && metrics.height() <= available.height();
        if (fits)
        {
            return font;
        }
    }

    return fieldFont(field, minimumPointSize);
}

void drawSingleLineField(
    QPainter* painter,
    const SpeakingEvalFieldAsset& field,
    const QRectF& available,
    const QFont& font,
    const QString& text
    )
{
    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::NoWrap);
    layout.setTextOption(option);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    if (!line.isValid())
    {
        layout.endLayout();
        return;
    }
    line.setLineWidth(available.width());

    qreal x = 0.0;
    if (field.horizontalAlignment.testFlag(Qt::AlignHCenter))
    {
        x = (available.width() - line.naturalTextWidth()) / 2.0;
    }
    else if (field.horizontalAlignment.testFlag(Qt::AlignRight))
    {
        x = available.width() - line.naturalTextWidth();
    }

    qreal y = 0.0;
    if (field.verticalAlignment.testFlag(Qt::AlignVCenter))
    {
        y = (available.height() - line.height()) / 2.0;
    }
    else if (field.verticalAlignment.testFlag(Qt::AlignBottom))
    {
        y = available.height() - line.height();
    }
    line.setPosition(QPointF(qMax(0.0, x), qMax(0.0, y)));
    layout.endLayout();
    layout.draw(
        painter,
        available.topLeft() + QPointF(0.0, field.baselineOffset)
        );
}

void drawWrappedField(
    QPainter* painter,
    const SpeakingEvalFieldAsset& field,
    const QRectF& available,
    const QFont& font,
    const QString& text
    )
{
    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    QVector<QTextLine> lines;
    qreal nextTop = 0.0;
    qreal height = 0.0;
    while (true)
    {
        QTextLine line = layout.createLine();
        if (!line.isValid())
        {
            break;
        }
        line.setLineWidth(available.width());
        lines.append(line);
        height = nextTop + line.height();
        nextTop += line.height() * field.lineHeight;
    }

    qreal verticalOffset = 0.0;
    if (field.verticalAlignment.testFlag(Qt::AlignVCenter))
    {
        verticalOffset = (available.height() - height) / 2.0;
    }
    else if (field.verticalAlignment.testFlag(Qt::AlignBottom))
    {
        verticalOffset = available.height() - height;
    }
    verticalOffset = qMax(0.0, verticalOffset);

    nextTop = 0.0;
    for (QTextLine& line : lines)
    {
        qreal x = 0.0;
        if (field.horizontalAlignment.testFlag(Qt::AlignHCenter))
        {
            x = (available.width() - line.naturalTextWidth()) / 2.0;
        }
        else if (field.horizontalAlignment.testFlag(Qt::AlignRight))
        {
            x = available.width() - line.naturalTextWidth();
        }
        line.setPosition(
            QPointF(qMax(0.0, x), verticalOffset + nextTop)
            );
        nextTop += line.height() * field.lineHeight;
    }
    layout.endLayout();
    layout.draw(
        painter,
        available.topLeft() + QPointF(0.0, field.baselineOffset)
        );
}

void drawField(
    QPainter* painter,
    const SpeakingEvalTemplateAssets& assets,
    const QString& fieldName,
    const QString& text
    )
{
    if (!painter || text.isEmpty())
    {
        return;
    }

    const auto iterator =
        assets.fields.constFind(fieldName);
    if (iterator == assets.fields.cend())
    {
        return;
    }

    const SpeakingEvalFieldAsset& field =
        iterator.value();
    const QRectF available =
        contentRect(field);
    const QFont font = fittedFieldFont(field, text);

    painter->save();
    painter->setClipRect(available);
    painter->setPen(Qt::black);
    painter->setFont(font);
    if (field.comments)
    {
        drawWrappedField(
            painter,
            field,
            available,
            font,
            text
            );
    }
    else
    {
        drawSingleLineField(
            painter,
            field,
            available,
            font,
            text
            );
    }
    painter->restore();
}

void drawSprite(
    QPainter* painter,
    const QImage& spriteSheet,
    const SpeakingEvalSpriteAsset& sprite,
    const QRectF& destination = {}
    )
{
    if (!painter
        || spriteSheet.isNull()
        || !sprite.source.isValid())
    {
        return;
    }

    painter->drawImage(
        destination.isValid()
            ? destination
            : sprite.destination,
        spriteSheet,
        sprite.source
        );
}

QRectF centeredSpriteDestination(
    const QRectF& bounds,
    const QSizeF& requestedSize
    )
{
    QSizeF size = requestedSize;
    if (size.width() > bounds.width() || size.height() > bounds.height())
    {
        size.scale(bounds.size(), Qt::KeepAspectRatio);
    }
    return QRectF(
        bounds.center() - QPointF(size.width() / 2.0, size.height() / 2.0),
        size
        );
}

void drawScores(
    QPainter* painter,
    const SpeakingEvalTemplateAssets& assets,
    const std::array<QString, 6>& scores
    )
{
    static const QStringList grades{
        QStringLiteral("A+"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("B"),
        QStringLiteral("C")
    };
    const int metricCount =
        std::min(
            static_cast<int>(scores.size()),
            static_cast<int>(assets.scoreCells.size())
            );
    for (int metric = 0; metric < metricCount; ++metric)
    {
        for (const QString& grade : grades)
        {
            const QRectF cell =
                assets.scoreCells.at(metric).value(grade);
            if (!cell.isValid())
            {
                continue;
            }

            if (scores.at(metric) == grade)
            {
                const qreal inset = assets.scoreHighlightInset;
                const QRectF highlight =
                    cell.adjusted(inset, inset, -inset, -inset);
                if (highlight.isValid())
                {
                    painter->fillRect(
                        highlight,
                        assets.scoreHighlightColor
                        );
                }
            }

            const auto label =
                assets.scoreLabels.constFind(grade);
            if (label != assets.scoreLabels.cend())
            {
                drawSprite(
                    painter,
                    assets.sprites,
                    label.value(),
                    centeredSpriteDestination(
                        cell,
                        label.value().pointSize
                        )
                    );
            }
        }
    }
}

void drawSignature(
    QPainter* painter,
    const QByteArray& imageData,
    const QRectF& bounds
    )
{
    if (!painter
        || imageData.isEmpty()
        || !bounds.isValid())
    {
        return;
    }

    const QImage image =
        QImage::fromData(imageData);
    if (image.isNull())
    {
        return;
    }

    QSizeF size = image.size();
    size.scale(bounds.size(), Qt::KeepAspectRatio);
    const QRectF destination(
        bounds.center()
            - QPointF(
                size.width() / 2.0,
                size.height() / 2.0
                ),
        size
        );
    painter->drawImage(destination, image);
}
}

void SpeakingEvalReportWidget::paintReport(
    QPainter* painter,
    const QRectF& targetRect
    ) const
{
    if (!painter || !targetRect.isValid())
    {
        return;
    }

    const SpeakingEvalTemplateAssets& assets =
        speakingEvalTemplateAssets(reportTemplate());
    const QSizeF reportSize =
        speakingEvalReportTemplateLayout(
            reportTemplate()
            ).pageSize;
    const qreal scale =
        std::min(
            targetRect.width() / reportSize.width(),
            targetRect.height() / reportSize.height()
            );
    const QPointF origin(
        targetRect.left()
            + ((targetRect.width() - (reportSize.width() * scale)) / 2.0),
        targetRect.top()
            + ((targetRect.height() - (reportSize.height() * scale)) / 2.0)
        );

    painter->save();
    painter->translate(origin);
    painter->scale(scale, scale);
    painter->fillRect(
        QRectF(QPointF(), reportSize),
        Qt::white
        );

    if (!assets.valid)
    {
        painter->setPen(QColor(QStringLiteral("#9b1c1c")));
        painter->setFont(
            speakingEvalTemplateFont(
                QStringLiteral("latinSemibold"),
                14.0
                )
            );
        painter->drawText(
            QRectF(QPointF(), reportSize).adjusted(
                30.0,
                30.0,
                -30.0,
                -30.0
                ),
            Qt::AlignCenter | Qt::TextWordWrap,
            tr("Speaking-evaluation template unavailable.\n%1")
                .arg(assets.error)
            );
        painter->restore();
        return;
    }

    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    painter->drawImage(
        QRectF(QPointF(), assets.logicalSize),
        assets.background
        );

    drawScores(
        painter,
        assets,
        m_data.scores
        );

    const auto overallIterator =
        assets.overallGrades.constFind(overallGrade());
    if (overallIterator != assets.overallGrades.cend())
    {
        drawSprite(
            painter,
            assets.sprites,
            overallIterator.value()
            );
    }

    drawField(
        painter,
        assets,
        QStringLiteral("englishName"),
        m_data.englishName
        );
    drawField(
        painter,
        assets,
        QStringLiteral("koreanName"),
        m_data.koreanName
        );
    drawField(
        painter,
        assets,
        QStringLiteral("classLabel"),
        m_data.classLabel
        );
    drawField(
        painter,
        assets,
        QStringLiteral("nativeTeacher"),
        m_data.nativeTeacher
        );
    drawField(
        painter,
        assets,
        QStringLiteral("koreanTeacher"),
        m_data.koreanTeacher
        );
    drawField(
        painter,
        assets,
        QStringLiteral("date"),
        m_data.date
        );
    drawField(
        painter,
        assets,
        QStringLiteral("comments"),
        m_data.comments
        );

    drawSignature(
        painter,
        m_data.signatureImage,
        assets.signatureBounds
        );

    painter->restore();
}
