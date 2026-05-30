#pragma once

#include <QFrame>

class QVBoxLayout;

class SectionCard : public QFrame
{
    Q_OBJECT

public:
    explicit SectionCard(const QString& title, QWidget* parent = nullptr);

    QVBoxLayout* contentLayout() const;

private:
    QVBoxLayout* m_layout;
};