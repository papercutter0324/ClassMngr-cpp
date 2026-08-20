#pragma once

#include <QFrame>
#include <QVBoxLayout>

class QLabel;
class QFont;

class SectionCard : public QFrame
{
    Q_OBJECT

public:
    explicit SectionCard(
        const QString& title,
        QWidget* parent = nullptr
        );

    void setTitle(
        const QString& title
        );
    void setTitleFont(
        const QFont& font
        );
    void setTitleAlignment(
        Qt::Alignment alignment
        );

    QVBoxLayout* contentLayout() const;

private:
    QLabel* m_titleLabel{};
    QVBoxLayout* m_layout{};
};
