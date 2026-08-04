#pragma once

#include <QPlainTextEdit>
#include <QWidget>

class QInputMethodEvent;
class QKeyEvent;
class QMimeData;

class SpeakingEvalBulletListEdit final : public QPlainTextEdit
{
public:
    explicit SpeakingEvalBulletListEdit(
        QWidget* parent = nullptr
        );

protected:
    void keyPressEvent(
        QKeyEvent* event
        ) override;

    void inputMethodEvent(
        QInputMethodEvent* event
        ) override;

    void insertFromMimeData(
        const QMimeData* source
        ) override;

private:
    void ensureCurrentLineHasBullet();
};

class SpeakingEvalPrivateNotesEditor final : public QWidget
{
    Q_OBJECT

public:
    explicit SpeakingEvalPrivateNotesEditor(
        QWidget* parent = nullptr
        );

    void setNotes(
        const QString& notes
        );

    [[nodiscard]] QString notes() const;

    void setEditorHeight(
        int height
        );

    void focusDidWell();

signals:
    void notesChanged();

private:
    SpeakingEvalBulletListEdit* m_didWellEdit = nullptr;
    SpeakingEvalBulletListEdit* m_needsImprovementEdit = nullptr;
};
