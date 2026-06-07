#pragma once

#include <QFrame>

class QVBoxLayout;

class TeacherSectionCard : public QFrame
{
    Q_OBJECT

public:
    explicit TeacherSectionCard(const QString& title, QWidget* parent = nullptr);

    QVBoxLayout* contentLayout() const;

private:
    QVBoxLayout* m_layout;
};