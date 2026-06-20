#pragma once

enum class SaveMode
{
    Automatic,
    Manual
};

enum class Theme
{
    Dark,
    Light
};

enum class Language
{
    SystemDefault = 0,
    English = 1,
    // Keep Korean's stored value stable. Values 2-4 were the old
    // per-region English options and are migrated to English on startup.
    Korean = 5
};
