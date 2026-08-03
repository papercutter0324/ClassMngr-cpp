#include "speaking_eval_report_assets_p.h"

#include "core/resource_paths.h"

#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

#include <cmath>

namespace
{
constexpr auto AssetRoot =
    ":/assets/templates/speaking-eval";

const QStringList& gradeNames()
{
    static const QStringList grades{
        QStringLiteral("A+"),
        QStringLiteral("A"),
        QStringLiteral("B+"),
        QStringLiteral("B"),
        QStringLiteral("C")
    };
    return grades;
}

const QStringList& fieldNames()
{
    static const QStringList fields{
        QStringLiteral("englishName"),
        QStringLiteral("koreanName"),
        QStringLiteral("classLabel"),
        QStringLiteral("nativeTeacher"),
        QStringLiteral("koreanTeacher"),
        QStringLiteral("date"),
        QStringLiteral("comments")
    };
    return fields;
}

bool finite(
    qreal value
    )
{
    return std::isfinite(
        static_cast<double>(value)
        );
}

bool readNumberArray(
    const QJsonValue& value,
    int expectedSize,
    QVector<qreal>* numbers
    )
{
    if (!numbers || !value.isArray())
    {
        return false;
    }

    const QJsonArray array = value.toArray();
    if (array.size() != expectedSize)
    {
        return false;
    }

    numbers->clear();
    numbers->reserve(expectedSize);
    for (const QJsonValue& item : array)
    {
        if (!item.isDouble() || !finite(item.toDouble()))
        {
            return false;
        }
        numbers->append(item.toDouble());
    }
    return true;
}

bool readRect(
    const QJsonValue& value,
    QRectF* rect
    )
{
    QVector<qreal> numbers;
    if (!rect || !readNumberArray(value, 4, &numbers))
    {
        return false;
    }

    *rect = QRectF(
        numbers.at(0),
        numbers.at(1),
        numbers.at(2),
        numbers.at(3)
        );
    return rect->isValid()
        && rect->width() > 0.0
        && rect->height() > 0.0;
}

bool readPixelRect(
    const QJsonValue& value,
    QRect* rect
    )
{
    QVector<qreal> numbers;
    if (!rect || !readNumberArray(value, 4, &numbers))
    {
        return false;
    }

    for (const qreal number : numbers)
    {
        if (std::floor(number) != number)
        {
            return false;
        }
    }

    *rect = QRect(
        static_cast<int>(numbers.at(0)),
        static_cast<int>(numbers.at(1)),
        static_cast<int>(numbers.at(2)),
        static_cast<int>(numbers.at(3))
        );
    return rect->isValid()
        && rect->width() > 0
        && rect->height() > 0;
}

bool readSize(
    const QJsonValue& value,
    QSizeF* size
    )
{
    QVector<qreal> numbers;
    if (!size || !readNumberArray(value, 2, &numbers))
    {
        return false;
    }

    *size = QSizeF(numbers.at(0), numbers.at(1));
    return size->isValid()
        && size->width() > 0.0
        && size->height() > 0.0;
}

Qt::Alignment horizontalAlignment(
    const QString& name
    )
{
    if (name == QStringLiteral("center"))
    {
        return Qt::AlignHCenter;
    }
    if (name == QStringLiteral("right"))
    {
        return Qt::AlignRight;
    }
    return Qt::AlignLeft;
}

Qt::Alignment verticalAlignment(
    const QString& name
    )
{
    if (name == QStringLiteral("center"))
    {
        return Qt::AlignVCenter;
    }
    if (name == QStringLiteral("bottom"))
    {
        return Qt::AlignBottom;
    }
    return Qt::AlignTop;
}

bool supportedFontRole(
    const QString& role
    )
{
    return role == QStringLiteral("latin")
        || role == QStringLiteral("latinSemibold")
        || role == QStringLiteral("latinCondensedBold")
        || role == QStringLiteral("korean")
        || role == QStringLiteral("handwritten");
}

bool backgroundIsOpaque(
    const QImage& image
    )
{
    if (!image.hasAlphaChannel())
    {
        return true;
    }

    const QImage argb =
        image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < argb.height(); ++y)
    {
        const QRgb* line =
            reinterpret_cast<const QRgb*>(argb.constScanLine(y));
        for (int x = 0; x < argb.width(); ++x)
        {
            if (qAlpha(line[x]) != 255)
            {
                return false;
            }
        }
    }
    return true;
}

bool safeAssetFileName(
    const QString& fileName
    )
{
    return !fileName.isEmpty()
        && !fileName.contains(QLatin1Char('/'))
        && !fileName.contains(QLatin1Char('\\'))
        && fileName != QStringLiteral(".")
        && fileName != QStringLiteral("..");
}

QString scoreHighlightFileName(
    const SpeakingEvalReportTemplate reportTemplate,
    const QString& grade
    )
{
    const QString prefix =
        reportTemplate == SpeakingEvalReportTemplate::Advanced
            ? QStringLiteral("advanced")
            : QStringLiteral("standard");
    if (grade == QStringLiteral("A+"))
    {
        return prefix + QStringLiteral("-yellow-aplus.png");
    }
    if (grade == QStringLiteral("A"))
    {
        return prefix + QStringLiteral("-yellow-a.png");
    }
    if (grade == QStringLiteral("B+"))
    {
        return prefix + QStringLiteral("-yellow-bplus.png");
    }
    if (grade == QStringLiteral("B"))
    {
        return prefix + QStringLiteral("-yellow-b.png");
    }
    if (grade == QStringLiteral("C"))
    {
        return prefix + QStringLiteral("-yellow-c.png");
    }
    return {};
}

QString studentGradeFileName(
    const QString& grade
    )
{
    if (grade == QStringLiteral("A+"))
    {
        return QStringLiteral("advanced-grey-aplus.png");
    }
    if (grade == QStringLiteral("A"))
    {
        return QStringLiteral("advanced-grey-a.png");
    }
    if (grade == QStringLiteral("B+"))
    {
        return QStringLiteral("advanced-grey-bplus.png");
    }
    if (grade == QStringLiteral("B"))
    {
        return QStringLiteral("advanced-grey-b.png");
    }
    if (grade == QStringLiteral("C"))
    {
        return QStringLiteral("advanced-grey-c.png");
    }
    return {};
}

bool isScoreCellGray(
    const QColor& color
    )
{
    return color.red() >= 205
        && color.red() <= 225
        && color.green() >= 205
        && color.green() <= 225
        && color.blue() >= 205
        && color.blue() <= 225;
}

bool isNeutralCellGray(
    const QColor& color
    )
{
    return color.red() >= 190
        && color.red() <= 240
        && color.green() >= 190
        && color.green() <= 240
        && color.blue() >= 190
        && color.blue() <= 240
        && qAbs(color.red() - color.green()) <= 4
        && qAbs(color.red() - color.blue()) <= 4
        && qAbs(color.green() - color.blue()) <= 4;
}

QRectF nativeGrayDestination(
    const QRectF& bounds,
    const QSizeF& logicalSize,
    const QImage& background
    )
{
    if (!bounds.isValid()
        || logicalSize.isEmpty()
        || background.isNull())
    {
        return {};
    }

    const qreal horizontalScale =
        background.width() / logicalSize.width();
    const qreal verticalScale =
        background.height() / logicalSize.height();
    const QRect searchRect =
        QRectF(
            bounds.left() * horizontalScale,
            bounds.top() * verticalScale,
            bounds.width() * horizontalScale,
            bounds.height() * verticalScale
            )
            .toAlignedRect()
            .intersected(background.rect());
    const QPoint center(
        qRound(bounds.center().x() * horizontalScale),
        qRound(bounds.center().y() * verticalScale)
        );
    if (!searchRect.contains(center)
        || !isScoreCellGray(background.pixelColor(center)))
    {
        return {};
    }

    int left = center.x();
    int right = center.x();
    int top = center.y();
    int bottom = center.y();
    while (left > searchRect.left()
        && isScoreCellGray(
            background.pixelColor(left - 1, center.y())
            ))
    {
        --left;
    }
    while (right < searchRect.right()
        && isScoreCellGray(
            background.pixelColor(right + 1, center.y())
            ))
    {
        ++right;
    }
    while (top > searchRect.top()
        && isScoreCellGray(
            background.pixelColor(center.x(), top - 1)
            ))
    {
        --top;
    }
    while (bottom < searchRect.bottom()
        && isScoreCellGray(
            background.pixelColor(center.x(), bottom + 1)
            ))
    {
        ++bottom;
    }
    const QRect pixelRect(
        QPoint(left, top),
        QPoint(right, bottom)
        );

    return QRectF(
        pixelRect.x() / horizontalScale,
        pixelRect.y() / verticalScale,
        pixelRect.width() / horizontalScale,
        pixelRect.height() / verticalScale
        );
}

QRectF nativeCenteredDestination(
    const QRectF& bounds,
    const QSizeF& logicalSize,
    const QImage& background,
    const QSize& authoredSize
    )
{
    if (!bounds.isValid()
        || logicalSize.isEmpty()
        || background.isNull()
        || authoredSize.isEmpty())
    {
        return {};
    }

    const qreal horizontalScale =
        background.width() / logicalSize.width();
    const qreal verticalScale =
        background.height() / logicalSize.height();
    const QRect cellPixels =
        QRectF(
            bounds.left() * horizontalScale,
            bounds.top() * verticalScale,
            bounds.width() * horizontalScale,
            bounds.height() * verticalScale
            )
            .toAlignedRect()
            .intersected(background.rect());
    QRect pixelRect(QPoint(), authoredSize);

    int grayLeft = -1;
    int grayWidth = 0;
    for (int y = cellPixels.top(); y <= cellPixels.bottom(); ++y)
    {
        int runLeft = -1;
        for (int x = cellPixels.left(); x <= cellPixels.right() + 1; ++x)
        {
            const bool gray =
                x <= cellPixels.right()
                && isNeutralCellGray(
                    background.pixelColor(x, y)
                    );
            if (gray && runLeft < 0)
            {
                runLeft = x;
            }
            else if (!gray && runLeft >= 0)
            {
                const int runWidth = x - runLeft;
                if (runWidth > grayWidth)
                {
                    grayLeft = runLeft;
                    grayWidth = runWidth;
                }
                runLeft = -1;
            }
        }
    }

    int grayTop = -1;
    int grayHeight = 0;
    for (int x = cellPixels.left(); x <= cellPixels.right(); ++x)
    {
        int runTop = -1;
        for (int y = cellPixels.top(); y <= cellPixels.bottom() + 1; ++y)
        {
            const bool gray =
                y <= cellPixels.bottom()
                && isNeutralCellGray(
                    background.pixelColor(x, y)
                    );
            if (gray && runTop < 0)
            {
                runTop = y;
            }
            else if (!gray && runTop >= 0)
            {
                const int runHeight = y - runTop;
                if (runHeight > grayHeight)
                {
                    grayTop = runTop;
                    grayHeight = runHeight;
                }
                runTop = -1;
            }
        }
    }

    if (grayWidth >= authoredSize.width()
        && grayHeight >= authoredSize.height())
    {
        pixelRect.moveTopLeft(
            QPoint(
                grayLeft
                    + (grayWidth - authoredSize.width()) / 2,
                grayTop
                    + (grayHeight - authoredSize.height()) / 2
                )
            );
    }
    else
    {
        pixelRect.moveCenter(
            QPoint(
                qRound(bounds.center().x() * horizontalScale),
                qRound(bounds.center().y() * verticalScale)
                )
            );
    }
    if (!cellPixels.contains(pixelRect))
    {
        return {};
    }

    return QRectF(
        pixelRect.x() / horizontalScale,
        pixelRect.y() / verticalScale,
        pixelRect.width() / horizontalScale,
        pixelRect.height() / verticalScale
        );
}

bool readSprite(
    const QJsonValue& value,
    const QImage& spriteSheet,
    SpeakingEvalSpriteAsset* sprite,
    bool destinationRequired
    )
{
    if (!sprite || !value.isObject())
    {
        return false;
    }

    const QJsonObject object = value.toObject();
    if (!readPixelRect(object.value(QStringLiteral("source")), &sprite->source)
        || !readSize(
            object.value(QStringLiteral("pointSize")),
            &sprite->pointSize
            )
        || (destinationRequired
            && !readRect(
                object.value(QStringLiteral("destination")),
                &sprite->destination
                )))
    {
        return false;
    }

    return spriteSheet.rect().contains(sprite->source);
}

struct TemplateFontFamilies
{
    bool valid = false;
    QString error;
    QString inter;
    QString pretendard;
    QString handwritten;
};

QString loadApplicationFont(
    const QString& path,
    const QString& familyFragment
    )
{
    if (!QFile::exists(path))
    {
        return {};
    }

    const int id =
        QFontDatabase::addApplicationFont(path);
    if (id < 0)
    {
        return {};
    }

    const QStringList families =
        QFontDatabase::applicationFontFamilies(id);
    for (const QString& family : families)
    {
        if (family.contains(
                familyFragment,
                Qt::CaseInsensitive
                ))
        {
            return family;
        }
    }
    return families.value(0);
}

TemplateFontFamilies loadTemplateFonts()
{
    TemplateFontFamilies fonts;
    if (!QGuiApplication::instance())
    {
        fonts.error =
            QStringLiteral("A GUI application is required to load report fonts.");
        return fonts;
    }

    fonts.inter =
        loadApplicationFont(
            ResourcePaths::Fonts::inter(),
            QStringLiteral("Inter")
            );
    fonts.pretendard =
        loadApplicationFont(
            ResourcePaths::Fonts::pretendard(),
            QStringLiteral("Pretendard")
            );
    fonts.handwritten =
        loadApplicationFont(
            ResourcePaths::Fonts::justAnotherHand(),
            QStringLiteral("Just Another Hand")
            );

    QStringList missing;
    if (fonts.inter.isEmpty())
    {
        missing.append(QStringLiteral("Inter"));
    }
    if (fonts.pretendard.isEmpty())
    {
        missing.append(QStringLiteral("Pretendard"));
    }
    if (fonts.handwritten.isEmpty())
    {
        missing.append(QStringLiteral("Just Another Hand"));
    }

    if (!missing.isEmpty())
    {
        fonts.error =
            QStringLiteral("Unable to load bundled report font(s): %1.")
                .arg(missing.join(QStringLiteral(", ")));
        return fonts;
    }

    fonts.valid = true;
    return fonts;
}

const TemplateFontFamilies& templateFonts()
{
    static const TemplateFontFamilies fonts =
        loadTemplateFonts();
    return fonts;
}

SpeakingEvalTemplateAssets loadTemplateAssets(
    SpeakingEvalReportTemplate reportTemplate
    )
{
    SpeakingEvalTemplateAssets assets;
    const QString kind =
        reportTemplate == SpeakingEvalReportTemplate::Advanced
            ? QStringLiteral("advanced")
            : QStringLiteral("standard");
    const QString manifestPath =
        QStringLiteral("%1/%2-manifest.json")
            .arg(
                QString::fromUtf8(AssetRoot),
                kind
                );

    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly))
    {
        assets.error =
            QStringLiteral("The embedded %1 speaking-evaluation manifest is missing.")
                .arg(kind);
        return assets;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(
            manifestFile.readAll(),
            &parseError
            );
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject())
    {
        assets.error =
            QStringLiteral("The embedded %1 speaking-evaluation manifest is corrupt.")
                .arg(kind);
        return assets;
    }

    const QJsonObject root = document.object();
    QVector<qreal> logicalSize;
    const int manifestVersion =
        root.value(QStringLiteral("version")).toInt(-1);
    if (manifestVersion != 3
        || root.value(QStringLiteral("template")).toString() != kind
        || root.value(QStringLiteral("units")).toString()
            != QStringLiteral("pt")
        || !readNumberArray(
            root.value(QStringLiteral("logicalSize")),
            2,
            &logicalSize
            )
        || qAbs(logicalSize.at(0) - 540.0) > 0.01
        || qAbs(logicalSize.at(1) - 780.0) > 0.01)
    {
        assets.error =
            QStringLiteral("The embedded %1 speaking-evaluation manifest has an unsupported schema.")
                .arg(kind);
        return assets;
    }

    assets.logicalSize =
        QSizeF(logicalSize.at(0), logicalSize.at(1));

    QVector<qreal> backgroundSize;
    const QJsonValue backgroundSizeValue =
        root.value(QStringLiteral("backgroundPixelSize"));
    if (!readNumberArray(
            backgroundSizeValue,
            2,
            &backgroundSize
            )
        || std::floor(backgroundSize.at(0)) != backgroundSize.at(0)
        || std::floor(backgroundSize.at(1)) != backgroundSize.at(1))
    {
        assets.error =
            QStringLiteral("The embedded %1 speaking-evaluation background size is invalid.")
                .arg(kind);
        return assets;
    }
    assets.backgroundPixelSize =
        QSize(
            static_cast<int>(backgroundSize.at(0)),
            static_cast<int>(backgroundSize.at(1))
            );
    if (assets.backgroundPixelSize.width() <= 0
        || assets.backgroundPixelSize.height() <= 0)
    {
        assets.error =
            QStringLiteral("The embedded %1 speaking-evaluation background size is invalid.")
                .arg(kind);
        return assets;
    }

    const QString backgroundName =
        root.value(QStringLiteral("background")).toString();
    const QString spriteName =
        root.value(QStringLiteral("sprites")).toString();
    if (!safeAssetFileName(backgroundName)
        || !safeAssetFileName(spriteName))
    {
        assets.error =
            QStringLiteral("The embedded %1 speaking-evaluation asset names are invalid.")
                .arg(kind);
        return assets;
    }

    const QString assetDirectory =
        QStringLiteral("%1/").arg(QString::fromUtf8(AssetRoot));
    if (!assets.background.load(assetDirectory + backgroundName)
        || !assets.sprites.load(assetDirectory + spriteName)
        || !assets.sprites.hasAlphaChannel()
        || assets.background.size() != assets.backgroundPixelSize
        || !backgroundIsOpaque(assets.background))
    {
        assets.error =
            QStringLiteral("The embedded %1 speaking-evaluation images are missing or corrupt.")
                .arg(kind);
        return assets;
    }

    const QJsonObject fields =
        root.value(QStringLiteral("fields")).toObject();
    for (const QString& name : fieldNames())
    {
        const QJsonObject object =
            fields.value(name).toObject();
        SpeakingEvalFieldAsset field;
        QVector<qreal> margins;
        const QString role =
            object.value(QStringLiteral("fontRole")).toString();
        const QString fit =
            object.value(QStringLiteral("fit")).toString();
        const qreal fontSizePoints =
            object.value(QStringLiteral("fontSizePt")).toDouble();
        const qreal minimumScale =
            object.value(QStringLiteral("minimumScale")).toDouble();
        const qreal letterSpacing =
            object.value(QStringLiteral("letterSpacing")).toDouble();
        const qreal wordSpacing =
            object.value(QStringLiteral("wordSpacing")).toDouble();
        const qreal lineHeight =
            object.value(QStringLiteral("lineHeight")).toDouble();
        const qreal baselineOffset =
            object.value(QStringLiteral("baselineOffset")).toDouble();
        if (object.isEmpty()
            || !readRect(object.value(QStringLiteral("rect")), &field.rect)
            || !readNumberArray(
                object.value(QStringLiteral("margins")),
                4,
                &margins
                )
            || !supportedFontRole(role)
            || !object.value(QStringLiteral("fontSizePt")).isDouble()
            || !finite(fontSizePoints)
            || fontSizePoints <= 0.0
            || !object.value(QStringLiteral("minimumScale")).isDouble()
            || !finite(minimumScale)
            || minimumScale <= 0.0
            || minimumScale > 1.0
            || !object.value(QStringLiteral("letterSpacing")).isDouble()
            || !finite(letterSpacing)
            || !object.value(QStringLiteral("wordSpacing")).isDouble()
            || !finite(wordSpacing)
            || !object.value(QStringLiteral("lineHeight")).isDouble()
            || !finite(lineHeight)
            || lineHeight <= 0.0
            || !object.value(QStringLiteral("baselineOffset")).isDouble()
            || !finite(baselineOffset)
            || (fit != QStringLiteral("singleLine")
                && fit != QStringLiteral("comments")))
        {
            assets.error =
                QStringLiteral("The embedded %1 speaking-evaluation field '%2' is invalid.")
                    .arg(kind, name);
            return assets;
        }

        field.margins =
            QMarginsF(
                margins.at(0),
                margins.at(1),
                margins.at(2),
                margins.at(3)
                );
        field.fontRole = role;
        field.fontSizePoints = fontSizePoints;
        field.minimumScale = minimumScale;
        field.letterSpacing = letterSpacing;
        field.wordSpacing = wordSpacing;
        field.lineHeight = lineHeight;
        field.baselineOffset = baselineOffset;
        field.horizontalAlignment =
            horizontalAlignment(
                object.value(
                    QStringLiteral("horizontalAlignment")
                    ).toString()
                );
        field.verticalAlignment =
            verticalAlignment(
                object.value(
                    QStringLiteral("verticalAlignment")
                    ).toString()
                );
        field.comments =
            fit == QStringLiteral("comments");
        assets.fields.insert(name, field);
    }

    const QJsonObject scoreTable =
        root.value(QStringLiteral("scoreTable")).toObject();
    const QColor highlightColor(
        scoreTable.value(QStringLiteral("fillColor")).toString()
        );
    const QJsonValue highlightInsetValue =
        scoreTable.value(QStringLiteral("fillInset"));
    const qreal highlightInset =
        highlightInsetValue.toDouble(-1.0);
    const QJsonArray metrics =
        scoreTable.value(QStringLiteral("metrics")).toArray();
    if (scoreTable.isEmpty()
        || !highlightColor.isValid()
        || !highlightInsetValue.isDouble()
        || !finite(highlightInset)
        || highlightInset < 0.0
        || metrics.size() != 6)
    {
        assets.error =
            QStringLiteral("The embedded %1 speaking-evaluation score table is invalid.")
                .arg(kind);
        return assets;
    }
    assets.scoreHighlightColor = highlightColor;
    assets.scoreHighlightInset = highlightInset;
    assets.scoreCells.reserve(metrics.size());
    const QStringList metricNames{
        QStringLiteral("grammar"),
        QStringLiteral("pronunciation"),
        QStringLiteral("fluency"),
        QStringLiteral("manner"),
        QStringLiteral("content"),
        QStringLiteral("overall-effort")
    };
    const QRectF logicalPage(QPointF(), assets.logicalSize);
    for (int metricIndex = 0; metricIndex < metrics.size(); ++metricIndex)
    {
        const QJsonObject metric = metrics.at(metricIndex).toObject();
        const QJsonObject cells =
            metric.value(QStringLiteral("cells")).toObject();
        QHash<QString, QRectF> parsedCells;
        if (metric.value(QStringLiteral("name")).toString()
                != metricNames.at(metricIndex)
            || cells.isEmpty())
        {
            assets.error =
                QStringLiteral("The embedded %1 speaking-evaluation score metric %2 is invalid.")
                    .arg(kind)
                    .arg(metricIndex);
            return assets;
        }
        for (const QString& grade : gradeNames())
        {
            QRectF cell;
            if (!readRect(cells.value(grade), &cell)
                || !logicalPage.contains(cell))
            {
                assets.error =
                    QStringLiteral("The embedded %1 speaking-evaluation score cell '%2' is invalid.")
                        .arg(kind, grade);
                return assets;
            }
            parsedCells.insert(grade, cell);
        }
        assets.scoreCells.append(parsedCells);
    }

    const QJsonArray studentGradeCells =
        scoreTable.value(
            QStringLiteral("studentGradeCells")
            ).toArray();
    if (!studentGradeCells.isEmpty())
    {
        if (studentGradeCells.size() != assets.scoreCells.size())
        {
            assets.error =
                QStringLiteral("The embedded %1 speaking-evaluation student-grade cells are invalid.")
                    .arg(kind);
            return assets;
        }
        assets.studentGradeCells.reserve(studentGradeCells.size());
        for (
            int metricIndex = 0;
            metricIndex < studentGradeCells.size();
            ++metricIndex
            )
        {
            QRectF cell;
            if (!readRect(studentGradeCells.at(metricIndex), &cell)
                || !logicalPage.contains(cell))
            {
                assets.error =
                    QStringLiteral("The embedded %1 speaking-evaluation student-grade cell %2 is invalid.")
                        .arg(kind)
                        .arg(metricIndex);
                return assets;
            }
            assets.studentGradeCells.append(cell);
        }

        for (const QString& grade : gradeNames())
        {
            const QString fileName =
                studentGradeFileName(grade);
            QImage studentGrade;
            if (fileName.isEmpty()
                || !studentGrade.load(assetDirectory + fileName)
                || studentGrade.isNull())
            {
                assets.error =
                    QStringLiteral("The embedded %1 speaking-evaluation student grade '%2' is missing or corrupt.")
                        .arg(kind, grade);
                return assets;
            }
            assets.studentGrades.insert(grade, studentGrade);
        }

        assets.studentGradeRects.reserve(
            assets.studentGradeCells.size()
            );
        for (const QRectF& cell : assets.studentGradeCells)
        {
            QHash<QString, QRectF> destinations;
            for (const QString& grade : gradeNames())
            {
                const QRectF destination =
                    nativeCenteredDestination(
                        cell,
                        assets.logicalSize,
                        assets.background,
                        assets.studentGrades.value(grade).size()
                        );
                if (!destination.isValid()
                    || !logicalPage.contains(destination))
                {
                    assets.error =
                        QStringLiteral("The embedded %1 speaking-evaluation student-grade destination '%2' is invalid.")
                            .arg(kind, grade);
                    return assets;
                }
                destinations.insert(grade, destination);
            }
            assets.studentGradeRects.append(destinations);
        }
    }

    QSize highlightSize;
    for (const QString& grade : gradeNames())
    {
        const QString fileName =
            scoreHighlightFileName(reportTemplate, grade);
        QImage highlight;
        if (fileName.isEmpty()
            || !highlight.load(assetDirectory + fileName)
            || highlight.isNull()
            || (highlightSize.isValid()
                && highlight.size() != highlightSize))
        {
            assets.error =
                QStringLiteral("The embedded %1 speaking-evaluation highlight '%2' is missing or corrupt.")
                    .arg(kind, grade);
            return assets;
        }
        highlightSize = highlight.size();
        assets.scoreHighlights.insert(grade, highlight);
    }

    assets.scoreHighlightRects.reserve(assets.scoreCells.size());
    for (const QHash<QString, QRectF>& cells : assets.scoreCells)
    {
        QHash<QString, QRectF> destinations;
        for (const QString& grade : gradeNames())
        {
            const QRectF destination =
                reportTemplate == SpeakingEvalReportTemplate::Advanced
                    ? nativeCenteredDestination(
                        cells.value(grade),
                        assets.logicalSize,
                        assets.background,
                        highlightSize
                        )
                    : nativeGrayDestination(
                        cells.value(grade),
                        assets.logicalSize,
                        assets.background
                        );
            if (!destination.isValid()
                || !logicalPage.contains(destination))
            {
                assets.error =
                    QStringLiteral("The embedded %1 speaking-evaluation highlight destination '%2' is invalid.")
                        .arg(kind, grade);
                return assets;
            }
            destinations.insert(grade, destination);
        }
        assets.scoreHighlightRects.append(destinations);
    }

    const QJsonObject labels =
        scoreTable.value(QStringLiteral("labels")).toObject();
    for (const QString& grade : gradeNames())
    {
        SpeakingEvalSpriteAsset sprite;
        if (!readSprite(
                labels.value(grade),
                assets.sprites,
                &sprite,
                false
                ))
        {
            assets.error =
                QStringLiteral("The embedded %1 speaking-evaluation score label '%2' is invalid.")
                    .arg(kind, grade);
            return assets;
        }
        assets.scoreLabels.insert(grade, sprite);
    }

    if (!readRect(
            root.value(QStringLiteral("overallGradeBounds")),
            &assets.overallGradeBounds
            )
        || !logicalPage.contains(assets.overallGradeBounds))
    {
        assets.error =
            QStringLiteral("The embedded %1 speaking-evaluation overall-grade bounds are invalid.")
                .arg(kind);
        return assets;
    }

    const QJsonObject overallGrades =
        root.value(QStringLiteral("overallGrades")).toObject();
    QStringList overallGradeNames = gradeNames();
    overallGradeNames.append(QStringLiteral("N/A"));
    for (const QString& grade : overallGradeNames)
    {
        SpeakingEvalSpriteAsset sprite;
        if (!readSprite(
                overallGrades.value(grade),
                assets.sprites,
                &sprite,
                true
                )
            || !assets.overallGradeBounds.contains(sprite.destination))
        {
            assets.error =
                QStringLiteral("The embedded %1 speaking-evaluation overall-grade asset '%2' is invalid.")
                    .arg(kind, grade);
            return assets;
        }
        assets.overallGrades.insert(grade, sprite);
    }

    if (!readRect(
            root.value(QStringLiteral("signatureBounds")),
            &assets.signatureBounds
            )
        || !logicalPage.contains(assets.signatureBounds))
    {
        assets.error =
            QStringLiteral("The embedded %1 speaking-evaluation signature bounds are invalid.")
                .arg(kind);
        return assets;
    }

    const TemplateFontFamilies& fonts =
        templateFonts();
    if (!fonts.valid)
    {
        assets.error = fonts.error;
        return assets;
    }

    assets.valid = true;
    return assets;
}
}

const SpeakingEvalTemplateAssets& speakingEvalTemplateAssets(
    SpeakingEvalReportTemplate reportTemplate
    )
{
    static const SpeakingEvalTemplateAssets standard =
        loadTemplateAssets(SpeakingEvalReportTemplate::Standard);
    static const SpeakingEvalTemplateAssets advanced =
        loadTemplateAssets(SpeakingEvalReportTemplate::Advanced);

    return reportTemplate == SpeakingEvalReportTemplate::Advanced
        ? advanced
        : standard;
}

const SpeakingEvalFieldAsset* speakingEvalFieldAsset(
    SpeakingEvalReportTemplate reportTemplate,
    const QString& fieldName
    )
{
    const SpeakingEvalTemplateAssets& assets =
        speakingEvalTemplateAssets(reportTemplate);
    const auto iterator =
        assets.fields.constFind(fieldName);
    return iterator == assets.fields.cend()
        ? nullptr
        : &iterator.value();
}

QRectF speakingEvalScoreCell(
    SpeakingEvalReportTemplate reportTemplate,
    int metricIndex,
    const QString& score
    )
{
    const SpeakingEvalTemplateAssets& assets =
        speakingEvalTemplateAssets(reportTemplate);
    if (!assets.valid
        || metricIndex < 0
        || metricIndex >= assets.scoreCells.size()
        || !assets.scoreCells.at(metricIndex).contains(score))
    {
        return {};
    }

    return assets.scoreCells.at(metricIndex).value(score);
}

QFont speakingEvalTemplateFont(
    const QString& role,
    qreal pointSize
    )
{
    const TemplateFontFamilies& families =
        templateFonts();
    QString primary =
        role == QStringLiteral("korean")
            ? families.pretendard
            : families.inter;
    QString fallback =
        role == QStringLiteral("korean")
            ? families.inter
            : families.pretendard;

    if (role == QStringLiteral("handwritten"))
    {
        primary = families.handwritten;
        fallback = families.inter;
    }

    QFont font;
    font.setFamilies({ primary, fallback });
    font.setPixelSize(
        qMax(1, qRound(pointSize))
        );
    font.setWeight(
        role == QStringLiteral("latinSemibold")
            ? QFont::DemiBold
            : role == QStringLiteral("latinCondensedBold")
                ? QFont::Bold
                : QFont::Normal
        );
    if (role == QStringLiteral("latinCondensedBold"))
    {
        font.setStretch(QFont::Condensed);
    }
    font.setHintingPreference(QFont::PreferNoHinting);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}
