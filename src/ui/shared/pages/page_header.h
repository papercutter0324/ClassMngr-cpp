#pragma once

#include <QWidget>

class QLabel;
class QHBoxLayout;

class PageHeader final : public QWidget
{
    Q_OBJECT

public:
    explicit PageHeader(
        const QString& title = {},
        const QString& subtitle = {},
        QWidget* parent = nullptr
        );

    QLabel* titleLabel() const;
    QLabel* subtitleLabel() const;
    QString title() const;
    QString subtitle() const;

    void setTitle(const QString& title);
    void setSubtitle(const QString& subtitle);
    void setTrailingWidget(QWidget* widget);
    void refreshFonts();

signals:
    void retranslationRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QHBoxLayout* m_titleRowLayout = nullptr;
    QWidget* m_trailingWidget = nullptr;
};
