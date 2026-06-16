#pragma once

#include <QWidget>

class QComboBox;
class QLineEdit;
class ClickableColorPreview;
class ApplicationServices;

class ClassDetailsSection : public QWidget
{
    Q_OBJECT

public:
    explicit ClassDetailsSection(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void loadInfo(
        const QString& grade,
        const QString& level,
        const QString& readingBook,
        const QString& essayBook,
        const QString& classColor,
        const QString& fontColor,
        int studentCount
        );

    QString grade() const;
    QString level() const;
    QString readingBook() const;
    QString essayBook() const;

    QString classColor() const;
    QString fontColor() const;

signals:
    void dataChanged();

private slots:
    void updateLevelOptions();
    void updateBookOptions();
    void openColorPicker();

private:
    void rebuildLevelOptions(const QString& preferredLevel = {});
    void rebuildBookOptions(
        const QString& preferredReadingBook = {},
        const QString& preferredEssayBook = {}
        );
    void updateColorPreview(const QString& color);

private:
    // =====================================================
    // Core dependency (ONLY source of services)
    // =====================================================
    ApplicationServices* m_services{nullptr};

    // =====================================================
    // UI state
    // =====================================================
    bool m_loadingInfo{false};
    QString m_pendingClassColor;
    QString m_pendingFontColor;

    ClickableColorPreview* m_colorPreview{nullptr};

    QComboBox* m_gradeCombo{nullptr};
    QComboBox* m_levelCombo{nullptr};

    QComboBox* m_readingBookCombo{nullptr};
    QComboBox* m_essayBookCombo{nullptr};

    QLineEdit* m_studentCountEdit{nullptr};
};
