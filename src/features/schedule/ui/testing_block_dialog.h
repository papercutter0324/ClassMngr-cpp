#pragma once

#include <QDialog>

class QLineEdit;

class TestingBlockDialog : public QDialog
{
    Q_OBJECT

public:
    TestingBlockDialog(
        const QString& room,
        bool existingBlock,
        QWidget* parent = nullptr
        );

    [[nodiscard]] QString room() const;
    [[nodiscard]] bool removeRequested() const;

private:
    void buildUi(
        bool existingBlock
        );

    QLineEdit* m_roomEdit = nullptr;
    bool m_removeRequested = false;
};
