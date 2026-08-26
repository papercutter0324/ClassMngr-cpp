#include "about_dialog.h"
#include "license_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "ui/shared/widgets/text_fit_push_button.h"

#include "core/appsettings.h"
#include "core/fontmanager.h"
#include "core/resource_paths.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

namespace
{
constexpr auto ProjectSourceUrl =
    "https://github.com/PaperCloud/ClassMngr-cpp";
constexpr auto QtSourceUrl =
    "https://code.qt.io/cgit/qt/";
constexpr auto QtLicensesUrl =
    "https://doc.qt.io/qt-6/licenses-used-in-qt.html";
constexpr auto InterSourceUrl =
    "https://github.com/rsms/inter";
constexpr auto PretendardSourceUrl =
    "https://github.com/orioncactus/pretendard";
constexpr auto JustAnotherHandSourceUrl =
    "https://github.com/google/fonts/tree/main/apache/justanotherhand";
constexpr auto DancingScriptSourceUrl =
    "https://github.com/google/fonts/tree/main/ofl/dancingscript";
constexpr auto GreatVibesSourceUrl =
    "https://github.com/google/fonts/tree/main/ofl/greatvibes";
constexpr auto CaveatSourceUrl =
    "https://github.com/google/fonts/tree/main/ofl/caveat";

constexpr auto InterLicensePath =
    "licenses/fonts/inter/LICENSE.txt";
constexpr auto PretendardLicensePath =
    "licenses/fonts/pretendard/LICENSE.txt";
constexpr auto JustAnotherHandLicensePath =
    "licenses/fonts/just-another-hand/LICENSE.txt";
constexpr auto DancingScriptLicensePath =
    "licenses/fonts/dancing-script/LICENSE.txt";
constexpr auto GreatVibesLicensePath =
    "licenses/fonts/great-vibes/LICENSE.txt";
constexpr auto CaveatLicensePath =
    "licenses/fonts/caveat/LICENSE.txt";

constexpr int DialogWidthSafetyBuffer = 12;

QLabel* createLinkLabel(
    const QString& html,
    QWidget* parent
    )
{
    auto* label =
        new QLabel(html, parent);

    label->setOpenExternalLinks(true);
    label->setTextInteractionFlags(
        Qt::TextBrowserInteraction
        );
    label->setWordWrap(true);

    return label;
}

QString link(
    const QString& text,
    const QString& url
    )
{
    return QStringLiteral("<a href=\"%1\">%2</a>")
        .arg(
            url.toHtmlEscaped(),
            text.toHtmlEscaped()
            );
}

QString resolveLicensePath(
    const QString& relativePath
    )
{
    if (QFile::exists(relativePath))
    {
        return relativePath;
    }

    QStringList candidates;

    const QString appDir =
        QCoreApplication::applicationDirPath();

    if (!appDir.isEmpty())
    {
        candidates.append(
            QDir::cleanPath(
                appDir + "/" + relativePath
                )
            );
        candidates.append(
            QDir::cleanPath(
                appDir + "/../" + relativePath
                )
            );
        candidates.append(
            QDir::cleanPath(
                appDir + "/../../" + relativePath
                )
            );
        candidates.append(
            QDir::cleanPath(
                appDir + "/../../../" + relativePath
                )
            );
    }

    candidates.append(
        QDir::cleanPath(
            QDir::currentPath() + "/" + relativePath
            )
        );

    const QString sourceDir =
        QStringLiteral(CLASSMNGR_SOURCE_DIR);

    if (!sourceDir.isEmpty())
    {
        candidates.append(
            QDir::cleanPath(
                sourceDir + "/" + relativePath
                )
            );
    }

    for (const QString& candidate : candidates)
    {
        if (QFile::exists(candidate))
        {
            return candidate;
        }
    }

    return relativePath;
}
}

AboutDialog::AboutDialog(
    QWidget* parent
    )
    : DialogShell(QStringLiteral("about"), parent)
{
    buildUi();
}

void AboutDialog::showInterLicense()
{
    showLicense(
        tr("Inter Font License"),
        QString::fromUtf8(InterLicensePath)
        );
}

void AboutDialog::showPretendardLicense()
{
    showLicense(
        tr("Pretendard Font License"),
        QString::fromUtf8(PretendardLicensePath)
        );
}

void AboutDialog::showJustAnotherHandLicense()
{
    showLicense(
        tr("Just Another Hand Font License"),
        QString::fromUtf8(JustAnotherHandLicensePath)
        );
}

void AboutDialog::showTypedSignatureFontLicenses()
{
    const QList<QPair<QString, QString>> licenses = {
        {
            tr("Dancing Script"),
            QString::fromUtf8(DancingScriptLicensePath)
        },
        {
            tr("Great Vibes"),
            QString::fromUtf8(GreatVibesLicensePath)
        },
        {
            tr("Caveat"),
            QString::fromUtf8(CaveatLicensePath)
        }
    };

    QStringList licenseText;

    for (const auto& [fontName, relativePath] : licenses)
    {
        QFile file(resolveLicensePath(relativePath));

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            DialogServices::showWarning(
                this,
                tr("License Not Available"),
                tr("The license file could not be opened:\n%1")
                    .arg(relativePath)
                );
            return;
        }

        licenseText.append(
            QStringLiteral("%1\n\n%2")
                .arg(fontName, QString::fromUtf8(file.readAll()))
            );
    }

    LicenseDialog dialog(
        tr("Typed Signature Font Licenses"),
        licenseText.join(QStringLiteral("\n\n==========\n\n")),
        this
        );
    dialog.exec();
}

void AboutDialog::buildUi()
{
    const QString applicationName =
        QString::fromUtf8(AppSettings::ApplicationName);

    setWindowTitle(
        tr("About %1")
            .arg(applicationName)
        );
    setMinimumWidth(560);

    auto* layout = contentLayout();

    auto* headerLayout =
        new QHBoxLayout;
    headerLayout->setSpacing(14);
    headerLayout->setAlignment(Qt::AlignTop);

    auto* iconLabel =
        new QLabel(this);
    iconLabel->setFixedSize(72, 72);
    iconLabel->setAlignment(Qt::AlignCenter);

    const QPixmap icon(
        ResourcePaths::Icons::appDefault()
        );

    if (!icon.isNull())
    {
        iconLabel->setPixmap(
            icon.scaled(
                iconLabel->size(),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
                )
            );
    }

    auto* titleLayout =
        new QVBoxLayout;
    titleLayout->setSpacing(4);

    auto* title =
        new QLabel(
            applicationName,
            this
            );
    title->setObjectName("pageTitle");
    title->setFont(
        FontManager::getUiFont(
            20,
            QFont::DemiBold
            )
        );

    auto* version =
        new QLabel(
            tr("Version %1")
                .arg(QApplication::applicationVersion()),
            this
            );
    version->setObjectName("pageSubtitle");

    titleLayout->addWidget(title, 0, Qt::AlignTop);
    titleLayout->addWidget(version, 0, Qt::AlignTop);
    titleLayout->setAlignment(Qt::AlignTop);

    headerLayout->addWidget(iconLabel, 0, Qt::AlignTop);
    headerLayout->addLayout(titleLayout, 1);
    headerLayout->setAlignment(
        titleLayout,
        Qt::AlignTop
        );

    auto* description =
        new QLabel(
            tr("%1 is a desktop classroom management tool for organizing schedules, rosters, campus information, teacher details, and substitute-prep notes.")
                .arg(applicationName),
            this
            );
    description->setWordWrap(true);

    auto* sourceLinks =
        createLinkLabel(
            tr("Source code: %1")
                .arg(
                    link(
                        applicationName,
                        QString::fromUtf8(ProjectSourceUrl)
                        )
                    ),
            this
            );

    auto* acknowledgementsTitle =
        new QLabel(
            tr("Acknowledgements"),
            this
            );
    acknowledgementsTitle->setObjectName("sectionTitle");
    acknowledgementsTitle->setFont(
        FontManager::getUiFont(
            13,
            QFont::DemiBold
            )
        );

    auto* acknowledgements =
        createLinkLabel(
            tr("This non-commercial application is built with %1 under Qt's open-source licensing terms and includes these font families: %2.")
                .arg(
                    link(
                        tr("Qt 6"),
                        QString::fromUtf8(QtSourceUrl)
                        ),
                    QStringList{
                        link(
                            tr("Inter"),
                            QString::fromUtf8(InterSourceUrl)
                            ),
                        link(
                            tr("Pretendard"),
                            QString::fromUtf8(PretendardSourceUrl)
                            ),
                        link(
                            tr("Just Another Hand"),
                            QString::fromUtf8(JustAnotherHandSourceUrl)
                            ),
                        link(
                            tr("Dancing Script"),
                            QString::fromUtf8(DancingScriptSourceUrl)
                            ),
                        link(
                            tr("Great Vibes"),
                            QString::fromUtf8(GreatVibesSourceUrl)
                            ),
                        link(
                            tr("Caveat"),
                            QString::fromUtf8(CaveatSourceUrl)
                            )
                    }.join(tr(", "))
                    ),
            this
            );

    auto* qtLicenseLink =
        createLinkLabel(
            tr("Qt source and license information: %1")
                .arg(
                    link(
                        tr("Open-source licenses used in Qt"),
                        QString::fromUtf8(QtLicensesUrl)
                        )
                    ),
            this
            );
    qtLicenseLink->setWordWrap(false);

    auto* licenseButtonLayout =
        new QHBoxLayout;

    auto* aboutQtButton =
        new TextFitPushButton(
            tr("About Qt"),
            this
            );

    auto* interLicenseButton =
        new TextFitPushButton(
            tr("View Inter License"),
            this
            );

    auto* pretendardLicenseButton =
        new TextFitPushButton(
            tr("View Pretendard License"),
            this
            );

    auto* justAnotherHandLicenseButton =
        new TextFitPushButton(
            tr("View Just Another Hand License"),
            this
            );

    auto* typedSignatureFontsLicenseButton =
        new TextFitPushButton(
            tr("View Typed Signature Font Licenses"),
            this
            );

    connect(
        interLicenseButton,
        &QPushButton::clicked,
        this,
        &AboutDialog::showInterLicense
        );

    connect(
        pretendardLicenseButton,
        &QPushButton::clicked,
        this,
        &AboutDialog::showPretendardLicense
        );

    connect(
        justAnotherHandLicenseButton,
        &QPushButton::clicked,
        this,
        &AboutDialog::showJustAnotherHandLicense
        );

    connect(
        typedSignatureFontsLicenseButton,
        &QPushButton::clicked,
        this,
        &AboutDialog::showTypedSignatureFontLicenses
        );

    connect(
        aboutQtButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            QMessageBox::aboutQt(this);
        }
        );

    licenseButtonLayout->addWidget(aboutQtButton);
    licenseButtonLayout->addWidget(interLicenseButton);
    licenseButtonLayout->addWidget(pretendardLicenseButton);
    licenseButtonLayout->addWidget(justAnotherHandLicenseButton);
    licenseButtonLayout->addStretch();

    auto* typedSignatureLicenseLayout =
        new QHBoxLayout;
    typedSignatureLicenseLayout->addWidget(typedSignatureFontsLicenseButton);
    typedSignatureLicenseLayout->addStretch();

    layout->addLayout(headerLayout);
    layout->setAlignment(headerLayout, Qt::AlignTop);
    layout->addWidget(description);
    layout->addWidget(sourceLinks);
    layout->addSpacing(4);
    layout->addWidget(acknowledgementsTitle);
    layout->addWidget(acknowledgements);
    layout->addWidget(qtLicenseLink);
    layout->addLayout(licenseButtonLayout);
    layout->addLayout(typedSignatureLicenseLayout);
    addButtonBox(QDialogButtonBox::Close);

    const QMargins margins =
        layout->contentsMargins();
    const int qtLicenseLineWidth =
        qtLicenseLink->sizeHint().width()
        + margins.left()
        + margins.right()
        + DialogWidthSafetyBuffer;
    const int dialogWidth =
        qMax(
            minimumWidth(),
            qMax(
                layout->minimumSize().width(),
                qtLicenseLineWidth
                )
            );

    setFixedWidth(dialogWidth);
    layout->activate();
    setFixedHeight(
        layout->totalHeightForWidth(dialogWidth)
        );
}

void AboutDialog::showLicense(
    const QString& title,
    const QString& relativePath
    )
{
    const QString licensePath =
        resolveLicensePath(relativePath);

    QFile file(licensePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        DialogServices::showWarning(
            this,
            tr("License Not Available"),
            tr("The license file could not be opened:\n%1")
                .arg(relativePath)
            );
        return;
    }

    LicenseDialog dialog(
        title,
        QString::fromUtf8(file.readAll()),
        this
        );
    dialog.exec();
}
