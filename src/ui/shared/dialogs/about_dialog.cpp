#include "about_dialog.h"

#include "ui/shared/widgets/text_fit_push_button.h"

#include "core/appsettings.h"
#include "core/fontmanager.h"
#include "core/resource_paths.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStringList>
#include <QTextEdit>
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

constexpr auto InterLicensePath =
    "licenses/fonts/inter/LICENSE.txt";
constexpr auto PretendardLicensePath =
    "licenses/fonts/pretendard/LICENSE.txt";

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
    : QDialog(parent)
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

void AboutDialog::buildUi()
{
    const QString applicationName =
        QString::fromUtf8(AppSettings::ApplicationName);

    setWindowTitle(
        tr("About %1")
            .arg(applicationName)
        );
    setMinimumWidth(560);

    auto* layout =
        new QVBoxLayout(this);
    layout->setContentsMargins(
        22,
        22,
        22,
        22
        );
    layout->setSpacing(16);

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
    titleLayout->addStretch();
    titleLayout->setAlignment(Qt::AlignTop);

    headerLayout->addWidget(iconLabel, 0, Qt::AlignTop);
    headerLayout->addLayout(titleLayout, 1);

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
            tr("This non-commercial application is built with %1 under Qt's open-source licensing terms and includes the %2 and %3 font families.")
                .arg(
                    link(
                        tr("Qt 6"),
                        QString::fromUtf8(QtSourceUrl)
                        ),
                    link(
                        tr("Inter"),
                        QString::fromUtf8(InterSourceUrl)
                        ),
                    link(
                        tr("Pretendard"),
                        QString::fromUtf8(PretendardSourceUrl)
                        )
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
    licenseButtonLayout->addStretch();

    auto* closeButton =
        new TextFitPushButton(
            tr("Close"),
            this
            );

    connect(
        closeButton,
        &QPushButton::clicked,
        this,
        &QDialog::reject
        );

    auto* dialogButtonLayout =
        new QHBoxLayout;
    dialogButtonLayout->addStretch();
    dialogButtonLayout->addWidget(closeButton);

    layout->addLayout(headerLayout);
    layout->setAlignment(headerLayout, Qt::AlignTop);
    layout->addWidget(description);
    layout->addWidget(sourceLinks);
    layout->addSpacing(4);
    layout->addWidget(acknowledgementsTitle);
    layout->addWidget(acknowledgements);
    layout->addWidget(qtLicenseLink);
    layout->addLayout(licenseButtonLayout);
    layout->addStretch();
    layout->addLayout(dialogButtonLayout);
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
        QMessageBox::warning(
            this,
            tr("License Not Available"),
            tr("The license file could not be opened:\n%1")
                .arg(relativePath)
            );
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setMinimumSize(640, 520);

    auto* layout =
        new QVBoxLayout(&dialog);

    auto* textEdit =
        new QTextEdit(&dialog);
    textEdit->setReadOnly(true);
    textEdit->setLineWrapMode(QTextEdit::NoWrap);
    textEdit->setPlainText(
        QString::fromUtf8(
            file.readAll()
            )
        );
    textEdit->setFont(
        QFontDatabase::systemFont(
            QFontDatabase::FixedFont
            )
        );

    auto* closeButton =
        new TextFitPushButton(
            tr("Close"),
            &dialog
            );

    connect(
        closeButton,
        &QPushButton::clicked,
        &dialog,
        &QDialog::reject
        );

    auto* dialogButtonLayout =
        new QHBoxLayout;
    dialogButtonLayout->addStretch();
    dialogButtonLayout->addWidget(closeButton);

    layout->addWidget(textEdit);
    layout->addLayout(dialogButtonLayout);

    dialog.exec();
}
