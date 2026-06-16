#include "campus_map_preview.h"

#include <QFileInfo>
#include <QLabel>
#include <QObject>
#include <QPixmap>

void CampusMapPreview::update(
    QLabel* label,
    const QString& imagePath
    )
{
    if (!label)
    {
        return;
    }

    const QString trimmedImagePath =
        imagePath.trimmed();

    if (
        trimmedImagePath.isEmpty()
        || !QFileInfo::exists(trimmedImagePath)
        )
    {
        label->setPixmap(QPixmap());
        label->setText(QObject::tr("No map available"));
        return;
    }

    QPixmap pixmap(trimmedImagePath);

    if (pixmap.isNull())
    {
        label->setPixmap(QPixmap());
        label->setText(QObject::tr("No map available"));
        return;
    }

    label->setText(QString());
    label->setPixmap(
        pixmap.scaled(
            800,
            520,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
            )
        );
}
