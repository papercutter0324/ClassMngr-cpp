#include "dialog_shell.h"

#include "core/settingsmanager.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include <QCloseEvent>
#include <QEvent>
#include <QFont>
#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QVBoxLayout>

namespace
{
constexpr int DialogMargin = 18;
constexpr int DialogSpacing = 10;

QString normalizedDialogKey(QString key)
{
    key = key.trimmed();

    for (QChar& character : key)
    {
        if (
            !character.isLetterOrNumber()
            && character != u'-'
            && character != u'_'
            && character != u'.'
            )
        {
            character = u'_';
        }
    }

    return key;
}
}

DialogShell::DialogShell(
    const QString& dialogKey,
    QWidget* parent
    )
    : QDialog(parent)
    , m_dialogKey(normalizedDialogKey(dialogKey))
    , m_contentLayout(new QVBoxLayout(this))
{
    Q_ASSERT(!m_dialogKey.isEmpty());

    setObjectName(m_dialogKey + QStringLiteral("Dialog"));
    setModal(true);
    setSizeGripEnabled(true);

    m_contentLayout->setContentsMargins(
        DialogMargin,
        DialogMargin,
        DialogMargin,
        DialogMargin
        );
    m_contentLayout->setSpacing(DialogSpacing);
}

DialogShell::~DialogShell()
{
    persistGeometry();
}

QString DialogShell::dialogKey() const
{
    return m_dialogKey;
}

QVBoxLayout* DialogShell::contentLayout() const
{
    return m_contentLayout;
}

void DialogShell::setHeader(
    const QString& title,
    const QString& subtitle
    )
{
    if (!m_header)
    {
        m_header = new QWidget(this);
        m_header->setObjectName(m_dialogKey + QStringLiteral("Header"));
        m_header->setAccessibleName(tr("Dialog header"));

        auto* headerLayout = new QVBoxLayout(m_header);
        headerLayout->setContentsMargins(0, 0, 0, 4);
        headerLayout->setSpacing(4);

        m_headerTitle = new QLabel(m_header);
        m_headerTitle->setObjectName(
            m_dialogKey + QStringLiteral("HeaderTitle")
            );
        QFont titleFont = m_headerTitle->font();
        titleFont.setBold(true);
        m_headerTitle->setFont(titleFont);
        m_headerTitle->setWordWrap(true);

        m_headerSubtitle = new QLabel(m_header);
        m_headerSubtitle->setObjectName(
            m_dialogKey + QStringLiteral("HeaderSubtitle")
            );
        m_headerSubtitle->setWordWrap(true);

        headerLayout->addWidget(m_headerTitle);
        headerLayout->addWidget(m_headerSubtitle);
        m_contentLayout->insertWidget(0, m_header);
    }

    m_headerTitle->setText(title);
    m_headerTitle->setAccessibleName(title);
    m_headerSubtitle->setText(subtitle);
    m_headerSubtitle->setAccessibleName(subtitle);
    m_headerSubtitle->setVisible(!subtitle.isEmpty());
}

TextFitDialogButtonBox* DialogShell::addButtonBox(
    QDialogButtonBox::StandardButtons standardButtons
    )
{
    auto* buttons =
        new TextFitDialogButtonBox(standardButtons, this);
    buttons->setObjectName(
        m_dialogKey + QStringLiteral("ButtonBox")
        );
    buttons->setAccessibleName(tr("Dialog actions"));

    connect(
        buttons,
        &QDialogButtonBox::accepted,
        this,
        &QDialog::accept
        );
    connect(
        buttons,
        &QDialogButtonBox::rejected,
        this,
        &QDialog::reject
        );

    configureDefaultButton(buttons);
    m_contentLayout->addWidget(buttons);
    return buttons;
}

void DialogShell::showEvent(QShowEvent* event)
{
    if (!m_geometryRestored)
    {
        restoreSavedGeometry();
        m_geometryRestored = true;
    }

    clampToAvailableScreen();
    updateAccessibleName();
    QDialog::showEvent(event);
}

void DialogShell::closeEvent(QCloseEvent* event)
{
    persistGeometry();
    QDialog::closeEvent(event);
}

void DialogShell::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);

    switch (event->type())
    {
    case QEvent::LanguageChange:
        retranslateShellChrome();
        retranslateDialog();
        updateAccessibleName();
        break;
    case QEvent::ApplicationPaletteChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        refreshDialogAppearance();
        break;
    default:
        break;
    }
}

void DialogShell::retranslateDialog()
{
}

void DialogShell::refreshDialogAppearance()
{
}

QString DialogShell::geometrySettingsKey() const
{
    return QStringLiteral("ui/dialogs/%1/geometry").arg(m_dialogKey);
}

void DialogShell::restoreSavedGeometry()
{
    const QByteArray geometry =
        SettingsManager::instance()
            .get(geometrySettingsKey())
            .toByteArray();

    if (!geometry.isEmpty())
    {
        restoreGeometry(geometry);
    }
}

void DialogShell::persistGeometry() const
{
    if (!m_geometryRestored || m_dialogKey.isEmpty())
    {
        return;
    }

    SettingsManager::instance().set(
        geometrySettingsKey(),
        saveGeometry()
        );
}

void DialogShell::clampToAvailableScreen()
{
    QScreen* targetScreen = screen();

    if (!targetScreen && parentWidget())
    {
        targetScreen = parentWidget()->screen();
    }

    if (!targetScreen)
    {
        targetScreen = QGuiApplication::primaryScreen();
    }

    if (!targetScreen)
    {
        return;
    }

    const QRect available = targetScreen->availableGeometry();
    const QSize boundedSize(
        qMin(width(), available.width()),
        qMin(height(), available.height())
        );

    if (boundedSize != size())
    {
        resize(boundedSize);
    }

    const int maximumX = qMax(
        available.left(),
        available.right() - width() + 1
        );
    const int maximumY = qMax(
        available.top(),
        available.bottom() - height() + 1
        );
    move(
        qBound(available.left(), x(), maximumX),
        qBound(available.top(), y(), maximumY)
        );
}

void DialogShell::updateAccessibleName()
{
    setAccessibleName(
        windowTitle().isEmpty()
            ? objectName()
            : windowTitle()
        );
}

void DialogShell::retranslateShellChrome()
{
    if (m_header)
    {
        m_header->setAccessibleName(tr("Dialog header"));
    }

    for (auto* buttons : findChildren<QDialogButtonBox*>())
    {
        buttons->setAccessibleName(tr("Dialog actions"));
    }
}

void DialogShell::configureDefaultButton(
    TextFitDialogButtonBox* buttons
    )
{
    const QDialogButtonBox::StandardButton priorities[] = {
        QDialogButtonBox::Save,
        QDialogButtonBox::Open,
        QDialogButtonBox::Ok,
        QDialogButtonBox::Yes,
        QDialogButtonBox::Apply
    };

    for (const auto standardButton : priorities)
    {
        if (auto* button = buttons->button(standardButton))
        {
            button->setDefault(true);
            button->setAutoDefault(true);
            return;
        }
    }
}
