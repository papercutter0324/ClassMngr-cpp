#pragma once

#include <QFrame>
#include <QVBoxLayout>

class SectionCard : public QFrame
{
    Q_OBJECT

public:
    explicit SectionCard(
        const QString& title,
        QWidget* parent = nullptr
        );

    QVBoxLayout* contentLayout() const;

private:
    QVBoxLayout* m_layout{};
};