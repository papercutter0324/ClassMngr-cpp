#include "campus_map_preview.h"

#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPixmap>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
constexpr int ImageSpacing = 16;
constexpr int DividerThickness = 2;
constexpr int EmptyStateHeight = 260;
constexpr auto ImageAspectRatioProperty =
    "_classmngr_map_image_aspect_ratio";
constexpr auto ViewKindProperty =
    "_classmngr_map_view_kind";

enum class ViewKind
{
    Map,
    Building
};

QString viewTitle(
    ViewKind kind
    )
{
    return kind == ViewKind::Map
        ? CampusMapPreview::tr("Map View")
        : CampusMapPreview::tr("Building View");
}

double imageAspectRatio(
    const QLabel* label
    )
{
    return label
        ? label
            ->property(ImageAspectRatioProperty)
            .toDouble()
        : 0.0;
}

class AspectRatioImageLabel : public QLabel
{
public:
    explicit AspectRatioImageLabel(
        QPixmap source,
        QWidget* parent = nullptr
        )
        : QLabel(parent)
        , m_source(std::move(source))
    {
        setObjectName(QStringLiteral("campusMapImage"));
        setProperty(
            ImageAspectRatioProperty,
            static_cast<double>(m_source.width())
                / m_source.height()
            );
        setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Preferred
            );
        setMinimumWidth(1);
    }

    bool hasHeightForWidth() const override
    {
        return true;
    }

    int heightForWidth(
        int width
        ) const override
    {
        if (m_source.isNull() || width <= 0)
        {
            return 0;
        }

        return qMax(
            1,
            static_cast<int>(
                std::lround(
                    static_cast<double>(width)
                    * m_source.height()
                    / m_source.width()
                    )
                )
            );
    }

    QSize sizeHint() const override
    {
        const double aspectRatio =
            static_cast<double>(m_source.width())
                / m_source.height();

        return {
            qMax(
                1,
                static_cast<int>(
                    std::lround(
                        CampusMapPreview::MaximumImageHeight
                            * aspectRatio
                        )
                    )
                ),
            CampusMapPreview::MaximumImageHeight
            };
    }

    QSize minimumSizeHint() const override
    {
        return {
            1,
            heightForWidth(1)
            };
    }

    int widthForHeight(
        int height
        ) const
    {
        return m_source
            .scaledToHeight(
                height,
                Qt::SmoothTransformation
                )
            .width();
    }

    void setSharedHeight(
        int height
        )
    {
        m_sharedHeight = qMax(1, height);

        const QPixmap scaled =
            m_source.scaledToHeight(
                m_sharedHeight,
                Qt::SmoothTransformation
                );

        setFixedSize(scaled.size());
        setPixmap(scaled);
    }

    void setDisplayWidth(
        int width
        )
    {
        const QPixmap scaled =
            m_source.scaledToWidth(
                qMax(1, width),
                Qt::SmoothTransformation
            );

        m_sharedHeight = scaled.height();
        setFixedSize(scaled.size());
        setPixmap(scaled);
    }

protected:
    void resizeEvent(
        QResizeEvent* event
        ) override
    {
        QLabel::resizeEvent(event);

        if (m_sharedHeight > 0)
        {
            return;
        }

        if (m_source.isNull() || event->size().isEmpty())
        {
            clear();
            return;
        }

        setPixmap(
            m_source.scaled(
                event->size(),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
                )
            );
    }

private:
    QPixmap m_source;
    int m_sharedHeight = 0;
};
}

CampusMapPreview::CampusMapPreview(
    QWidget* parent
    )
    : QWidget(parent)
{
    setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    m_layout =
        new QGridLayout(this);

    m_layout->setContentsMargins(
        0,
        0,
        0,
        0
        );
    m_layout->setHorizontalSpacing(ImageSpacing);
    m_layout->setVerticalSpacing(ImageSpacing);
    m_layout->setAlignment(Qt::AlignTop);

    m_emptyLabel =
        new QLabel(
            tr("No map images available"),
            this
            );
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setMinimumHeight(EmptyStateHeight);

    m_divider = new QFrame(this);
    m_divider->setObjectName(QStringLiteral("campusMapDivider"));
    m_divider->setFrameShadow(QFrame::Sunken);
    m_divider->hide();

    m_layout->addWidget(m_emptyLabel, 0, 0);
}

void CampusMapPreview::setImagePaths(
    const QStringList& imagePaths
    )
{
    clearImages();

    for (int index = 0; index < imagePaths.size(); ++index)
    {
        const QString& imagePath = imagePaths.at(index);
        const QString trimmedPath =
            imagePath.trimmed();

        if (
            trimmedPath.isEmpty()
            || !QFileInfo::exists(trimmedPath)
            )
        {
            continue;
        }

        QPixmap pixmap(trimmedPath);

        if (pixmap.isNull())
        {
            continue;
        }

        m_imageLabels.append(
            new AspectRatioImageLabel(
                std::move(pixmap),
                this
                )
            );

        const ViewKind kind =
            index == 0
                ? ViewKind::Map
                : ViewKind::Building;
        auto* titleLabel =
            new QLabel(viewTitle(kind), this);
        titleLabel->setObjectName(
            kind == ViewKind::Map
                ? QStringLiteral("mapViewTitle")
                : QStringLiteral("buildingViewTitle")
            );
        titleLabel->setProperty(
            "role",
            QStringLiteral("section_header_center")
            );
        titleLabel->setProperty(
            ViewKindProperty,
            static_cast<int>(kind)
            );
        titleLabel->setAlignment(Qt::AlignCenter);
        m_titleLabels.append(titleLabel);
    }

    m_emptyLabel->setVisible(m_imageLabels.isEmpty());

    rebuildLayout(
        width() >= HorizontalBreakpoint
        );
    updateImageSizing(width());
    updatePreviewHeight(width());
    updateGeometry();
}

void CampusMapPreview::setMapControls(
    QWidget* controls
    )
{
    if (controls == m_mapControls)
    {
        return;
    }

    if (m_mapControls)
    {
        m_layout->removeWidget(m_mapControls);
        delete m_mapControls;
    }

    m_mapControls = controls;

    if (m_mapControls)
    {
        m_mapControls->setParent(this);
    }

    rebuildLayout(
        width() >= HorizontalBreakpoint
        );
    updateImageSizing(width());
    updatePreviewHeight(width());
    updateGeometry();
}

void CampusMapPreview::retranslateUi()
{
    m_emptyLabel->setText(
        tr("No map images available")
        );

    for (QLabel* titleLabel : std::as_const(m_titleLabels))
    {
        titleLabel->setText(
            viewTitle(
                static_cast<ViewKind>(
                    titleLabel
                        ->property(ViewKindProperty)
                        .toInt()
                    )
                )
            );
    }

    updatePreviewHeight(width());
}

int CampusMapPreview::displayedImageCount() const
{
    return m_imageLabels.size();
}

bool CampusMapPreview::isHorizontal() const
{
    return m_horizontal;
}

bool CampusMapPreview::hasImages() const
{
    return !m_imageLabels.isEmpty();
}

bool CampusMapPreview::hasHeightForWidth() const
{
    return true;
}

int CampusMapPreview::heightForWidth(
    int width
    ) const
{
    return titlesHeight(width >= HorizontalBreakpoint)
        + controlsHeight()
        + (width < HorizontalBreakpoint
            ? dividerExtent()
            : 0)
        + imageHeightForWidth(width);
}

int CampusMapPreview::dividerExtent() const
{
    return m_imageLabels.size() >= 2
        ? DividerThickness + ImageSpacing
        : 0;
}

int CampusMapPreview::controlsHeight() const
{
    if (!m_mapControls)
    {
        return 0;
    }

    return m_mapControls->sizeHint().height()
        + ImageSpacing;
}

int CampusMapPreview::titlesHeight(
    bool horizontal
    ) const
{
    if (m_titleLabels.isEmpty())
    {
        return 0;
    }

    if (horizontal)
    {
        int maximumHeight = 0;

        for (const QLabel* titleLabel : m_titleLabels)
        {
            maximumHeight = qMax(
                maximumHeight,
                titleLabel->sizeHint().height()
                );
        }

        return maximumHeight + ImageSpacing;
    }

    int totalHeight =
        ImageSpacing * m_titleLabels.size();

    for (const QLabel* titleLabel : m_titleLabels)
    {
        totalHeight += titleLabel->sizeHint().height();
    }

    return totalHeight;
}

int CampusMapPreview::imageHeightForWidth(
    int width
    ) const
{
    if (m_imageLabels.isEmpty())
    {
        return EmptyStateHeight;
    }

    const int imageCount =
        m_imageLabels.size();

    if (width >= HorizontalBreakpoint)
    {
        const int totalSpacing =
            ImageSpacing * (imageCount - 1)
            + dividerExtent();
        double totalAspectRatio = 0.0;

        for (const QLabel* label : m_imageLabels)
        {
            totalAspectRatio +=
                imageAspectRatio(label);
        }

        if (totalAspectRatio <= 0.0)
        {
            return 0;
        }

        return qBound(
            1,
            static_cast<int>(
                std::floor(
                    (width - totalSpacing)
                    / totalAspectRatio
                    )
                ),
            MaximumImageHeight
            );
    }

    int totalHeight =
        ImageSpacing * (imageCount - 1);

    for (const QLabel* label : m_imageLabels)
    {
        totalHeight +=
            label->heightForWidth(width);
    }

    return totalHeight;
}

QSize CampusMapPreview::sizeHint() const
{
    return {
        HorizontalBreakpoint,
        heightForWidth(HorizontalBreakpoint)
        };
}

QSize CampusMapPreview::minimumSizeHint() const
{
    constexpr int minimumWidth = 320;

    return {
        minimumWidth,
        heightForWidth(minimumWidth)
        };
}

bool CampusMapPreview::isValidMapUrl(
    const QString& url
    )
{
    const QUrl parsedUrl =
        QUrl::fromUserInput(url.trimmed());

    return parsedUrl.isValid()
        && !parsedUrl.isRelative()
        && parsedUrl.scheme() == QStringLiteral("https")
        && !parsedUrl.host().isEmpty();
}

void CampusMapPreview::resizeEvent(
    QResizeEvent* event
    )
{
    QWidget::resizeEvent(event);

    updateLayoutForWidth(
        event->size().width()
        );
    updateImageSizing(
        event->size().width()
        );
    updatePreviewHeight(
        event->size().width()
        );
}

void CampusMapPreview::clearImages()
{
    for (QLabel* label : std::as_const(m_imageLabels))
    {
        m_layout->removeWidget(label);
        delete label;
    }

    m_imageLabels.clear();

    for (QLabel* titleLabel : std::as_const(m_titleLabels))
    {
        m_layout->removeWidget(titleLabel);
        delete titleLabel;
    }

    m_titleLabels.clear();
}

void CampusMapPreview::rebuildLayout(
    bool horizontal
    )
{
    m_horizontal = horizontal;

    m_layout->removeWidget(m_emptyLabel);
    m_layout->removeWidget(m_divider);
    m_divider->hide();

    if (m_mapControls)
    {
        m_layout->removeWidget(m_mapControls);
    }

    for (QLabel* label : std::as_const(m_imageLabels))
    {
        m_layout->removeWidget(label);
    }

    for (QLabel* titleLabel : std::as_const(m_titleLabels))
    {
        m_layout->removeWidget(titleLabel);
    }

    for (int index = 0;
         index <= m_imageLabels.size();
         ++index)
    {
        m_layout->setColumnStretch(index, 0);
        m_layout->setRowStretch(index, 0);
    }

    if (m_imageLabels.isEmpty())
    {
        m_layout->addWidget(m_emptyLabel, 0, 0);

        if (m_mapControls)
        {
            m_layout->addWidget(
                m_mapControls,
                1,
                0,
                Qt::AlignCenter
                );
        }

        return;
    }

    int verticalRow = 0;

    if (horizontal && m_imageLabels.size() >= 2)
    {
        m_divider->setFrameShape(QFrame::VLine);
        m_divider->setFixedWidth(DividerThickness);
        m_divider->setMinimumHeight(0);
        m_divider->setMaximumHeight(QWIDGETSIZE_MAX);
        m_divider->setSizePolicy(
            QSizePolicy::Fixed,
            QSizePolicy::Expanding
            );
        m_layout->addWidget(
            m_divider,
            0,
            1,
            m_mapControls ? 3 : 2,
            1
            );
        m_divider->show();
    }

    for (int index = 0;
         index < m_imageLabels.size();
         ++index)
    {
        const int column =
            horizontal
                ? (index == 0 ? 0 : index + 1)
                : 0;

        if (!horizontal && index == 1)
        {
            m_divider->setFrameShape(QFrame::HLine);
            m_divider->setFixedHeight(DividerThickness);
            m_divider->setMinimumWidth(0);
            m_divider->setMaximumWidth(QWIDGETSIZE_MAX);
            m_divider->setSizePolicy(
                QSizePolicy::Expanding,
                QSizePolicy::Fixed
                );
            m_layout->addWidget(
                m_divider,
                verticalRow++,
                0
                );
            m_divider->show();
        }

        const int titleRow =
            horizontal
                ? 0
                : verticalRow++;
        const int imageRow =
            horizontal
                ? 1
                : verticalRow++;

        m_layout->addWidget(
            m_titleLabels.at(index),
            titleRow,
            column,
            Qt::AlignCenter
            );

        m_layout->addWidget(
            m_imageLabels.at(index),
            imageRow,
            column,
            horizontal
                ? Qt::AlignTop | Qt::AlignHCenter
                : Qt::Alignment{}
            );

        if (index == 0 && m_mapControls)
        {
            const int controlsRow =
                horizontal
                    ? 2
                    : verticalRow++;

            m_layout->addWidget(
                m_mapControls,
                controlsRow,
                0,
                Qt::AlignCenter
                );
        }

        if (horizontal)
        {
            m_layout->setColumnStretch(
                column,
                qMax(
                    1,
                    static_cast<int>(
                        std::lround(
                            imageAspectRatio(
                                m_imageLabels.at(index)
                                ) * 1000.0
                            )
                        )
                    )
                );
        }
    }
}

void CampusMapPreview::updateLayoutForWidth(
    int width
    )
{
    const bool horizontal =
        width >= HorizontalBreakpoint;

    if (horizontal == m_horizontal)
    {
        return;
    }

    rebuildLayout(horizontal);
    updateGeometry();
}

void CampusMapPreview::updateImageSizing(
    int width
    )
{
    if (m_imageLabels.isEmpty())
    {
        return;
    }

    if (width < HorizontalBreakpoint)
    {
        for (QLabel* label : std::as_const(m_imageLabels))
        {
            static_cast<AspectRatioImageLabel*>(label)
                ->setDisplayWidth(width);
        }

        return;
    }

    const int availableWidth =
        qMax(
            1,
            width
                - ImageSpacing * (m_imageLabels.size() - 1)
                - dividerExtent()
            );

    int sharedHeight =
        imageHeightForWidth(width);

    const auto requiredWidth =
        [this](int height)
        {
            int totalWidth = 0;

            for (QLabel* label : m_imageLabels)
            {
                totalWidth +=
                    static_cast<AspectRatioImageLabel*>(label)
                        ->widthForHeight(height);
            }

            return totalWidth;
        };

    while (
        sharedHeight > 1
        && requiredWidth(sharedHeight) > availableWidth
        )
    {
        --sharedHeight;
    }

    for (QLabel* label : std::as_const(m_imageLabels))
    {
        static_cast<AspectRatioImageLabel*>(label)
            ->setSharedHeight(sharedHeight);
    }
}

void CampusMapPreview::updatePreviewHeight(
    int width
    )
{
    const int targetHeight =
        heightForWidth(qMax(1, width));

    if (height() == targetHeight
        && minimumHeight() == targetHeight
        && maximumHeight() == targetHeight)
    {
        return;
    }

    setFixedHeight(targetHeight);
    updateGeometry();
}
