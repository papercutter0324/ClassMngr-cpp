#pragma once

#include <QObject>
#include <QHash>

class QHBoxLayout;
class QPushButton;

class BottomBarBuilder : public QObject
{
    Q_OBJECT

public:
    explicit BottomBarBuilder(QHBoxLayout* layout,
                              QObject* parent = nullptr);

    void clear();

    void setupBase(
        int height = 48,
        int spacing = 8,
        int left = 10,
        int top = 4,
        int right = 10,
        int bottom = 4
        );

    void addStretch();
    void addSpacing(int amount = 20);

    QPushButton* addButton(
        const QString& key,
        const QString& text,
        const QObject* receiver,
        const char* slot,
        bool expand = true
        );

    QPushButton* get(const QString& key) const;

private:
    QHBoxLayout* m_layout {};
    QHash<QString, QPushButton*> m_buttons;
};