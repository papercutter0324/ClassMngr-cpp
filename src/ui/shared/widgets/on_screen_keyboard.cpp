#include "on_screen_keyboard.h"

#include <QAbstractButton>
#include <QAbstractItemDelegate>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QGuiApplication>
#include <QIcon>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QTextEdit>
#include <QToolTip>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

namespace
{

constexpr int KeyboardWidth = 760;
constexpr int KeyboardHeight = 310;
constexpr int KeyMinimumHeight = 42;
constexpr int CharacterKeyWidth = 48;

QChar shiftedEnglishKey(
    QChar key
    )
{
    const QString normal = QStringLiteral("1234567890-=,./");
    const QString shifted = QStringLiteral("!@#$%^&*()_+<>?");
    const qsizetype index = normal.indexOf(key);

    if (index >= 0)
    {
        return shifted.at(index);
    }

    return key.isLetter()
        ? key.toUpper()
        : key;
}

QChar koreanKey(
    QChar key,
    bool shifted
    )
{
    const QString normalKeys = QStringLiteral("qwertyuiopasdfghjklzxcvbnm");
    const QString normalJamo = QStringLiteral("ㅂㅈㄷㄱㅅㅛㅕㅑㅐㅔㅁㄴㅇㄹㅎㅗㅓㅏㅣㅋㅌㅊㅍㅠㅜㅡ");
    const QString shiftedJamo = QStringLiteral("ㅃㅉㄸㄲㅆㅛㅕㅑㅒㅖㅁㄴㅇㄹㅎㅗㅓㅏㅣㅋㅌㅊㅍㅠㅜㅡ");
    const qsizetype index = normalKeys.indexOf(key.toLower());

    if (index < 0)
    {
        return shifted
            ? shiftedEnglishKey(key)
            : key;
    }

    return shifted
        ? shiftedJamo.at(index)
        : normalJamo.at(index);
}

void configureKeyButton(
    QPushButton* button
    )
{
    button->setFocusPolicy(Qt::NoFocus);
    button->setMinimumHeight(KeyMinimumHeight);
    button->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
        );
}

void configureCharacterKeyButton(
    QPushButton* button
    )
{
    configureKeyButton(button);
    button->setFixedWidth(CharacterKeyWidth);
    button->setProperty("keyboardCharacterKey", true);
}

} // namespace

OnScreenKeyboard::OnScreenKeyboard(
    QWidget* parent
    )
    : QWidget(
        parent,
        Qt::Tool | Qt::WindowDoesNotAcceptFocus
        )
{
    setObjectName(QStringLiteral("onScreenKeyboard"));
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_QuitOnClose, false);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);
    setMinimumSize(620, 260);
    resize(KeyboardWidth, KeyboardHeight);
    buildUi();
    retranslateUi();
}

void OnScreenKeyboard::setTriggerButton(
    QAbstractButton* button
    )
{
    if (m_triggerButton)
    {
        m_triggerButton->removeEventFilter(this);
    }

    m_triggerButton = button;

    if (m_triggerButton)
    {
        m_triggerButton->installEventFilter(this);
        refreshTriggerIcon();
    }
}

void OnScreenKeyboard::setTarget(
    QWidget* target
    )
{
    if (target == m_target)
    {
        return;
    }

    commitComposition();
    commitEditorData();
    m_target = isEligibleTarget(target)
        ? target
        : nullptr;
    m_targetIndex = QPersistentModelIndex{};
}

void OnScreenKeyboard::clearTarget()
{
    commitComposition();
    m_target.clear();
    m_targetIndex = QPersistentModelIndex{};
}

void OnScreenKeyboard::showFor(
    QAbstractItemView* view
    )
{
    attachView(view);
    show();
    raise();

    if (!m_positioned)
    {
        positionForParent();
    }

    retarget(view);
}

void OnScreenKeyboard::retarget(
    QAbstractItemView* view
    )
{
    if (!view)
    {
        clearTarget();
        return;
    }

    const QModelIndex index = view->currentIndex();

    if (
        !index.isValid()
        || !(index.flags() & Qt::ItemIsEditable)
        )
    {
        clearTarget();
        showNoTargetMessage(view);
        return;
    }

    view->edit(index);
    QWidget* editor = editorForView(view);

    if (!editor)
    {
        clearTarget();
        showNoTargetMessage(view);
        return;
    }

    setTargetForIndex(editor, index);
}

QWidget* OnScreenKeyboard::target() const
{
    return m_target;
}

bool OnScreenKeyboard::isKoreanLayout() const
{
    return m_koreanLayout;
}

bool OnScreenKeyboard::eventFilter(
    QObject* watched,
    QEvent* event
    )
{
    if (
        watched == m_triggerButton
        && (
            event->type() == QEvent::PaletteChange
            || event->type() == QEvent::ApplicationPaletteChange
            || event->type() == QEvent::StyleChange
            || event->type() == QEvent::DynamicPropertyChange
            )
        )
    {
        refreshTriggerIcon();
    }

    if (
        m_view
        && watched == m_view->viewport()
        && event->type() == QEvent::MouseButtonPress
        )
    {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const QModelIndex nextIndex =
            m_view->indexAt(mouseEvent->position().toPoint());

        if (
            nextIndex.isValid()
            && nextIndex != m_view->currentIndex()
            )
        {
            commitComposition();
            commitEditorData();
            scheduleViewRetarget();
        }
    }

    if (
        m_view
        && watched == m_view
        && event->type() == QEvent::KeyPress
        )
    {
        const int key = static_cast<QKeyEvent*>(event)->key();
        const bool navigates =
            key == Qt::Key_Left
            || key == Qt::Key_Right
            || key == Qt::Key_Up
            || key == Qt::Key_Down
            || key == Qt::Key_Tab
            || key == Qt::Key_Backtab
            || key == Qt::Key_Home
            || key == Qt::Key_End
            || key == Qt::Key_PageUp
            || key == Qt::Key_PageDown;

        if (navigates)
        {
            commitComposition();
            commitEditorData();
            scheduleViewRetarget();
        }
    }

    return QWidget::eventFilter(watched, event);
}

void OnScreenKeyboard::changeEvent(
    QEvent* event
    )
{
    QWidget::changeEvent(event);

    if (event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
}

void OnScreenKeyboard::closeEvent(
    QCloseEvent* event
    )
{
    commitComposition();
    commitEditorData();
    detachView();
    QWidget::closeEvent(event);
}

void OnScreenKeyboard::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto* title = new QLabel(this);
    title->setObjectName(QStringLiteral("onScreenKeyboardTitle"));
    title->setProperty("keyboardTitle", true);
    root->addWidget(title);

    auto* keys = new QVBoxLayout;
    keys->setSpacing(6);
    root->addLayout(keys, 1);

    addCharacterRow(
        keys,
        QStringLiteral("1234567890-=")
        );
    addCharacterRow(
        keys,
        QStringLiteral("qwertyuiop")
        );
    addCharacterRow(
        keys,
        QStringLiteral("asdfghjkl")
        );
    addCharacterRow(
        keys,
        QStringLiteral("zxcvbnm,./")
        );

    auto* controls = new QGridLayout;
    controls->setHorizontalSpacing(6);
    root->addLayout(controls);

    m_shiftButton = new QPushButton(this);
    m_shiftButton->setObjectName(QStringLiteral("onScreenKeyboardShift"));
    m_shiftButton->setCheckable(true);
    configureKeyButton(m_shiftButton);
    controls->addWidget(m_shiftButton, 0, 0);

    m_languageButton = new QPushButton(this);
    m_languageButton->setObjectName(QStringLiteral("onScreenKeyboardLanguage"));
    configureKeyButton(m_languageButton);
    controls->addWidget(m_languageButton, 0, 1);

    m_spaceButton = new QPushButton(this);
    m_spaceButton->setObjectName(QStringLiteral("onScreenKeyboardSpace"));
    configureKeyButton(m_spaceButton);
    controls->addWidget(m_spaceButton, 0, 2, 1, 3);

    m_backspaceButton = new QPushButton(this);
    m_backspaceButton->setObjectName(QStringLiteral("onScreenKeyboardBackspace"));
    configureKeyButton(m_backspaceButton);
    controls->addWidget(m_backspaceButton, 0, 5);

    m_enterButton = new QPushButton(this);
    m_enterButton->setObjectName(QStringLiteral("onScreenKeyboardEnter"));
    configureKeyButton(m_enterButton);
    controls->addWidget(m_enterButton, 0, 6);

    m_closeButton = new QPushButton(this);
    m_closeButton->setObjectName(QStringLiteral("onScreenKeyboardClose"));
    configureKeyButton(m_closeButton);
    controls->addWidget(m_closeButton, 0, 7);

    connect(
        m_shiftButton,
        &QPushButton::clicked,
        this,
        &OnScreenKeyboard::toggleShift
        );
    connect(
        m_languageButton,
        &QPushButton::clicked,
        this,
        &OnScreenKeyboard::toggleLanguage
        );
    connect(
        m_spaceButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            commitComposition();
            commitText(QStringLiteral(" "));
        }
        );
    connect(
        m_backspaceButton,
        &QPushButton::clicked,
        this,
        &OnScreenKeyboard::handleBackspace
        );
    connect(
        m_enterButton,
        &QPushButton::clicked,
        this,
        &OnScreenKeyboard::handleEnter
        );
    connect(
        m_closeButton,
        &QPushButton::clicked,
        this,
        &QWidget::close
        );
}

void OnScreenKeyboard::addCharacterRow(
    QVBoxLayout* layout,
    const QString& keys
    )
{
    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    row->addStretch();

    for (int index = 0; index < keys.size(); ++index)
    {
        auto* button = new QPushButton(this);
        button->setObjectName(
            QStringLiteral("onScreenKeyboardKey_%1")
                .arg(keys.at(index))
            );
        configureCharacterKeyButton(button);

        const QChar key = keys.at(index);
        connect(
            button,
            &QPushButton::clicked,
            this,
            [this, key]()
            {
                handleCharacter(key);
            }
            );

        row->addWidget(button);
        m_characterButtons.append({button, key});
    }

    row->addStretch();
    layout->addLayout(row);
}

void OnScreenKeyboard::handleCharacter(
    QChar key
    )
{
    if (!m_target)
    {
        return;
    }

    if (m_koreanLayout && key.isLetter())
    {
        dispatchComposition(
            m_composer.input(
                koreanKey(key, m_shifted)
                )
            );
    }
    else
    {
        commitComposition();
        const QChar output = m_shifted
            ? shiftedEnglishKey(key)
            : key;
        commitText(QString(1, output));
    }

    if (m_shifted)
    {
        m_shifted = false;
        m_shiftButton->setChecked(false);
        updateKeyLabels();
    }
}

void OnScreenKeyboard::handleBackspace()
{
    const HangulCompositionResult result =
        m_composer.backspace();

    if (result.consumed)
    {
        dispatchComposition(result);
        return;
    }

    sendEditingKey(Qt::Key_Backspace);
}

void OnScreenKeyboard::handleEnter()
{
    commitComposition();
    commitEditorData();
    sendEditingKey(Qt::Key_Return);
}

void OnScreenKeyboard::toggleLanguage()
{
    commitComposition();
    m_koreanLayout = !m_koreanLayout;
    m_shifted = false;
    m_shiftButton->setChecked(false);
    updateKeyLabels();
}

void OnScreenKeyboard::toggleShift()
{
    m_shifted = m_shiftButton->isChecked();
    updateKeyLabels();
}

void OnScreenKeyboard::updateKeyLabels()
{
    for (const CharacterButton& entry : m_characterButtons)
    {
        if (!entry.button)
        {
            continue;
        }

        const QChar label = m_koreanLayout && entry.key.isLetter()
            ? koreanKey(entry.key, m_shifted)
            : (
                m_shifted
                    ? shiftedEnglishKey(entry.key)
                    : entry.key
                );
        entry.button->setText(QString(1, label));
    }

    if (m_languageButton)
    {
        m_languageButton->setText(
            m_koreanLayout
                ? tr("English")
                : tr("Korean")
            );
    }
}

void OnScreenKeyboard::retranslateUi()
{
    setWindowTitle(tr("On-Screen Keyboard"));

    if (auto* title = findChild<QLabel*>(
            QStringLiteral("onScreenKeyboardTitle")
            ))
    {
        title->setText(tr("Korean / English Keyboard"));
    }

    if (m_shiftButton) m_shiftButton->setText(tr("Shift"));
    if (m_spaceButton) m_spaceButton->setText(tr("Space"));
    if (m_backspaceButton) m_backspaceButton->setText(tr("Backspace"));
    if (m_enterButton) m_enterButton->setText(tr("Enter"));
    if (m_closeButton) m_closeButton->setText(tr("Close"));

    updateKeyLabels();
}

void OnScreenKeyboard::refreshTriggerIcon()
{
    if (!m_triggerButton)
    {
        return;
    }

    const QString theme =
        m_triggerButton->property("theme").toString();
    const bool usesDarkTheme = theme == QStringLiteral("dark")
        || (
            theme.isEmpty()
            && m_triggerButton
                ->palette()
                .color(QPalette::ButtonText)
                .lightness() > 128
            );
    const QString iconPath = usesDarkTheme
        ? QStringLiteral(":/assets/icons/keyboard_dark.svg")
        : QStringLiteral(":/assets/icons/keyboard_light.svg");

    m_triggerButton->setIcon(QIcon(iconPath));
    m_triggerButton->setIconSize(QSize(24, 24));
}

void OnScreenKeyboard::positionForParent()
{
    QWidget* owner = parentWidget();
    QWidget* window = owner
        ? owner->window()
        : nullptr;
    const QRect ownerRect = window
        ? QRect(window->mapToGlobal(QPoint{}), window->size())
        : QRect{};
    QScreen* screen = window && window->windowHandle()
        ? window->windowHandle()->screen()
        : QGuiApplication::primaryScreen();

    if (!screen)
    {
        return;
    }

    const QRect available = screen->availableGeometry();
    QPoint position = ownerRect.isValid()
        ? QPoint(
            ownerRect.center().x() - width() / 2,
            ownerRect.bottom() - height() - 24
            )
        : available.center() - rect().center();

    position.setX(
        qBound(
            available.left(),
            position.x(),
            available.right() - width() + 1
            )
        );
    position.setY(
        qBound(
            available.top(),
            position.y(),
            available.bottom() - height() + 1
            )
        );
    move(position);
    m_positioned = true;
}

void OnScreenKeyboard::dispatchComposition(
    const HangulCompositionResult& result
    )
{
    if (!m_target)
    {
        m_composer.reset();
        return;
    }

    QInputMethodEvent event(result.preedit, {});
    event.setCommitString(result.committed);
    QCoreApplication::sendEvent(m_target, &event);

    if (!result.committed.isEmpty())
    {
        commitEditorData();
    }
}

void OnScreenKeyboard::commitComposition()
{
    if (!m_composer.isComposing())
    {
        return;
    }

    dispatchComposition(m_composer.commit());
}

void OnScreenKeyboard::commitEditorData()
{
    if (
        !m_view
        || !m_target
        || !m_targetIndex.isValid()
        || m_targetIndex.model() != m_view->model()
        )
    {
        return;
    }

    QAbstractItemDelegate* delegate =
        m_view->itemDelegateForIndex(m_targetIndex);

    if (delegate)
    {
        delegate->setModelData(
            m_target,
            m_view->model(),
            m_targetIndex
            );
    }
}

void OnScreenKeyboard::setTargetForIndex(
    QWidget* target,
    const QModelIndex& index
    )
{
    if (
        target == m_target
        && index == m_targetIndex
        )
    {
        return;
    }

    commitComposition();
    commitEditorData();
    m_target = isEligibleTarget(target)
        ? target
        : nullptr;
    m_targetIndex = m_target
        ? QPersistentModelIndex(index)
        : QPersistentModelIndex{};
}

void OnScreenKeyboard::commitText(
    const QString& text
    )
{
    if (!m_target || text.isEmpty())
    {
        return;
    }

    QInputMethodEvent event;
    event.setCommitString(text);
    QCoreApplication::sendEvent(m_target, &event);
    commitEditorData();
}

void OnScreenKeyboard::sendEditingKey(
    int key
    )
{
    QPointer<QWidget> target = m_target;

    if (!target)
    {
        return;
    }

    QKeyEvent press(
        QEvent::KeyPress,
        key,
        Qt::NoModifier
        );
    QCoreApplication::sendEvent(target, &press);

    if (!target)
    {
        return;
    }

    QKeyEvent release(
        QEvent::KeyRelease,
        key,
        Qt::NoModifier
        );
    QCoreApplication::sendEvent(target, &release);
}

QWidget* OnScreenKeyboard::editorForView(
    QAbstractItemView* view
    ) const
{
    QWidget* focused = QApplication::focusWidget();

    if (
        focused
        && view->isAncestorOf(focused)
        && isEligibleTarget(focused)
        )
    {
        return focused;
    }

    const auto lineEdits = view->findChildren<QLineEdit*>();
    for (QLineEdit* editor : lineEdits)
    {
        if (editor->isVisible() && isEligibleTarget(editor))
        {
            return editor;
        }
    }

    const auto plainTextEdits =
        view->findChildren<QPlainTextEdit*>();
    for (QPlainTextEdit* editor : plainTextEdits)
    {
        if (editor->isVisible() && isEligibleTarget(editor))
        {
            return editor;
        }
    }

    const auto textEdits = view->findChildren<QTextEdit*>();
    for (QTextEdit* editor : textEdits)
    {
        if (editor->isVisible() && isEligibleTarget(editor))
        {
            return editor;
        }
    }

    return nullptr;
}

bool OnScreenKeyboard::isEligibleTarget(
    QWidget* widget
    ) const
{
    if (!widget || !widget->isEnabled())
    {
        return false;
    }

    if (auto* lineEdit = qobject_cast<QLineEdit*>(widget))
    {
        return !lineEdit->isReadOnly();
    }

    if (auto* plainTextEdit =
            qobject_cast<QPlainTextEdit*>(widget))
    {
        return !plainTextEdit->isReadOnly();
    }

    if (auto* textEdit = qobject_cast<QTextEdit*>(widget))
    {
        return !textEdit->isReadOnly();
    }

    return false;
}

void OnScreenKeyboard::showNoTargetMessage(
    QAbstractItemView* view
    )
{
    if (!view)
    {
        return;
    }

    QToolTip::showText(
        view->viewport()->mapToGlobal(
            view->viewport()->rect().center()
            ),
        tr("Select an editable text cell to use the on-screen keyboard."),
        view,
        {},
        2500
        );
}

void OnScreenKeyboard::attachView(
    QAbstractItemView* view
    )
{
    if (view == m_view)
    {
        return;
    }

    detachView();
    m_view = view;

    if (!m_view)
    {
        return;
    }

    m_view->installEventFilter(this);
    m_view->viewport()->installEventFilter(this);
}

void OnScreenKeyboard::detachView()
{
    if (m_view)
    {
        m_view->removeEventFilter(this);
        m_view->viewport()->removeEventFilter(this);
    }

    m_view.clear();
}

void OnScreenKeyboard::scheduleViewRetarget()
{
    QTimer::singleShot(
        0,
        this,
        [this]()
        {
            if (m_view && isVisible())
            {
                retarget(m_view);
            }
        }
        );
}
