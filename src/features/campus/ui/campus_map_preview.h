#pragma once

#include <QString>

class QLabel;

class CampusMapPreview
{
public:
    static void update(
        QLabel* label,
        const QString& imagePath
        );
};
