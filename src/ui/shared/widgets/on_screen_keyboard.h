#pragma once

#include "ui/shared/input/hangul_composer.h"

#include <QPointer>
#include <QPersistentModelIndex>
#include <QVector>
#include <QWidget>

class QAbstractButton;
class QAbstractItemView;
class QCloseEvent;
class QEvent;
class QVBoxLayout;
class QPushButton;

class OnScreenKeyboard final : public QWidget
{
    Q_OBJECT

public:
    explicit OnScreenKeyboard(
        QWidget* parent = nullptr
        );

    void setTriggerButton(
        QAbstractButton* button
        );

    void setTarget(
        QWidget* target
        );

    void clearTarget();

    void showFor(
        QAbstractItemView* view
        );

    void retarget(
        QAbstractItemView* view
        );

    [[nodiscard]] QWidget* target() const;
    [[nodiscard]] bool isKoreanLayout() const;

protected:
    bool eventFilter(
        QObject* watched,
        QEvent* event
        ) override;

    void changeEvent(
        QEvent* event
        ) override;

    void closeEvent(
        QCloseEvent* event
        ) override;

private:
    struct CharacterButton
    {
        QPushButton* button = nullptr;
        QChar key;
    };

    void buildUi();

    void addCharacterRow(
        QVBoxLayout* layout,
        const QString& keys
        );

    void handleCharacter(
        QChar key
        );

    void handleBackspace();
    void handleEnter();
    void toggleLanguage();
    void toggleShift();
    void updateKeyLabels();
    void retranslateUi();
    void refreshTriggerIcon();
    void positionForParent();

    void dispatchComposition(
        const HangulCompositionResult& result
        );

    void commitComposition();
    void commitEditorData();

    void setTargetForIndex(
        QWidget* target,
        const QModelIndex& index
        );

    void commitText(
        const QString& text
        );

    void sendEditingKey(
        int key
        );

    [[nodiscard]] QWidget* editorForView(
        QAbstractItemView* view
        ) const;

    [[nodiscard]] bool isEligibleTarget(
        QWidget* widget
        ) const;

    void showNoTargetMessage(
        QAbstractItemView* view
        );

    void attachView(
        QAbstractItemView* view
        );

    void detachView();
    void scheduleViewRetarget();

private:
    QPointer<QWidget> m_target;
    QPointer<QAbstractItemView> m_view;
    QPersistentModelIndex m_targetIndex;
    QPointer<QAbstractButton> m_triggerButton;
    QVector<CharacterButton> m_characterButtons;
    QPushButton* m_shiftButton = nullptr;
    QPushButton* m_languageButton = nullptr;
    QPushButton* m_spaceButton = nullptr;
    QPushButton* m_backspaceButton = nullptr;
    QPushButton* m_enterButton = nullptr;
    QPushButton* m_closeButton = nullptr;
    HangulComposer m_composer;
    bool m_koreanLayout = true;
    bool m_shifted = false;
    bool m_positioned = false;
};
