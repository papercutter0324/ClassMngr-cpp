#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QString>

class QLabel;
class QVBoxLayout;
class QWidget;
class TextFitDialogButtonBox;

// Shared policy for feature dialogs. Subclasses add their widgets to
// contentLayout(); the shell owns spacing, standard buttons, accessibility,
// screen clamping, and geometry persistence.
class DialogShell : public QDialog
{
    Q_OBJECT

public:
    explicit DialogShell(
        const QString& dialogKey,
        QWidget* parent = nullptr
        );
    ~DialogShell() override;

    [[nodiscard]] QString dialogKey() const;
    [[nodiscard]] QVBoxLayout* contentLayout() const;

    void setHeader(
        const QString& title,
        const QString& subtitle = QString()
        );

    TextFitDialogButtonBox* addButtonBox(
        QDialogButtonBox::StandardButtons buttons
        );

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

    virtual void retranslateDialog();
    virtual void refreshDialogAppearance();

private:
    [[nodiscard]] QString geometrySettingsKey() const;
    void restoreSavedGeometry();
    void persistGeometry() const;
    void clampToAvailableScreen();
    void updateAccessibleName();
    void retranslateShellChrome();
    void configureDefaultButton(TextFitDialogButtonBox* buttons);

    QString m_dialogKey;
    QVBoxLayout* m_contentLayout = nullptr;
    QWidget* m_header = nullptr;
    QLabel* m_headerTitle = nullptr;
    QLabel* m_headerSubtitle = nullptr;
    bool m_geometryRestored = false;
};
