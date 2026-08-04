#pragma once

#include <QPlainTextEdit>

class QKeyEvent;
class QMimeData;

class SpeakingEvalCommentEdit final : public QPlainTextEdit
{
public:
    explicit SpeakingEvalCommentEdit(
        QWidget* parent = nullptr
        );

    void setStudentNames(
        const QString& englishName,
        const QString& koreanName
        );

    [[nodiscard]] int textLength() const;

    [[nodiscard]] QString cleanText() const;

protected:
    void keyPressEvent(
        QKeyEvent* event
        ) override;

    void insertFromMimeData(
        const QMimeData* source
        ) override;

private:
    void trimToLimit();

private:
    QString m_studentName;
};
