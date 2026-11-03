#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QString>

#include <functional>
#include <string>
#include <string_view>

class QLabel;
class QVBoxLayout;
class QWidget;
class TextFitDialogButtonBox;

// UI-owned bridge for opaque, binary-safe dialog geometry. Next supplies
// callbacks backed by its Application port; empty callbacks retain the
// compatibility behavior for Legacy callers.
struct DialogGeometryPreferencesCallbacks final
{
    using Read = std::function<std::string(std::string_view stableDialogKey)>;
    using Write = std::function<void(
        std::string_view stableDialogKey,
        std::string_view geometryPayload
        )>;

    Read read;
    Write write;
};

// Shared policy for feature dialogs. Subclasses add their widgets to
// contentLayout(); the shell owns spacing, standard buttons, accessibility,
// screen clamping, and geometry persistence.
class DialogShell : public QDialog
{
    Q_OBJECT

public:
    explicit DialogShell(
        const QString& dialogKey,
        QWidget* parent = nullptr,
        DialogGeometryPreferencesCallbacks geometryPreferences = {}
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
    DialogGeometryPreferencesCallbacks m_geometryPreferences;
    QVBoxLayout* m_contentLayout = nullptr;
    QWidget* m_header = nullptr;
    QLabel* m_headerTitle = nullptr;
    QLabel* m_headerSubtitle = nullptr;
    bool m_geometryRestored = false;
};
