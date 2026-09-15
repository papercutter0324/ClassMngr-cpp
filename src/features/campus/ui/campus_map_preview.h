#pragma once

#include <QWidget>

#include <QList>
#include <QString>
#include <QStringList>

class QLabel;
class QGridLayout;
class QFrame;
class QResizeEvent;

class CampusMapPreview : public QWidget
{
    Q_OBJECT

public:
    static constexpr int HorizontalBreakpoint = 760;
    static constexpr int MaximumImageHeight = 360;
    static constexpr int MaximumDecodedImageDimension =
        HorizontalBreakpoint * 2;

    explicit CampusMapPreview(
        QWidget* parent = nullptr
        );

    void setImagePaths(
        const QStringList& imagePaths
        );
    void setMapControls(
        QWidget* controls
        );

    void retranslateUi();

    [[nodiscard]] int displayedImageCount() const;
    [[nodiscard]] QSize decodedImageSize(
        int index
        ) const;
    [[nodiscard]] bool isHorizontal() const;
    [[nodiscard]] bool hasImages() const;

    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(
        int width
        ) const override;
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

    [[nodiscard]] static bool isValidMapUrl(
        const QString& url
        );

protected:
    void resizeEvent(
        QResizeEvent* event
        ) override;

private:
    void clearImages();
    [[nodiscard]] int controlsHeight() const;
    [[nodiscard]] int titlesHeight(
        bool horizontal
        ) const;
    [[nodiscard]] int imageHeightForWidth(
        int width
        ) const;
    [[nodiscard]] int dividerExtent() const;
    void rebuildLayout(
        bool horizontal
        );
    void updateLayoutForWidth(
        int width
        );
    void updateImageSizing(
        int width
        );
    void updatePreviewHeight(
        int width
        );

private:
    QGridLayout* m_layout = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QFrame* m_divider = nullptr;
    QWidget* m_mapControls = nullptr;
    QList<QLabel*> m_imageLabels;
    QList<QSize> m_decodedImageSizes;
    QList<QLabel*> m_titleLabels;
    bool m_horizontal = false;
};
