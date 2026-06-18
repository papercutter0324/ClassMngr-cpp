#pragma once

#include <QFrame>

class QLabel;
class QVBoxLayout;

class TeacherSectionCard : public QFrame
{
    Q_OBJECT

public:
    explicit TeacherSectionCard(const QString& title, QWidget* parent = nullptr);

    void setTitle(
        const QString& title
        );

    QVBoxLayout* contentLayout() const;

private:
    QLabel* m_titleLabel = nullptr;
    QVBoxLayout* m_layout = nullptr;
};
