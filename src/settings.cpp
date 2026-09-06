#include "settings.h"

#include <QDebug>

#define DISPLAY_PLAYER_NAME_COLOR "displayPlayerNameColor"
#define DISPLAY_LOCAL_MULTIPLAYER "displayLocalMultiplayer"
#define DISPLAY_BEST_TAVERN_HEROES "displayBestTavernHeroes"
#define DISPLAY_TODAY_LINE "displayTodayLine"

Settings::Settings(QObject *parent) :
    QObject(parent),
    settings("H3Overlay","H3Overlay")
{
    enablePlayerNameColor(loadSetting(DISPLAY_PLAYER_NAME_COLOR, false));
    enableLocalMultiplayer(loadSetting(DISPLAY_LOCAL_MULTIPLAYER, false));
    enableBestTavernHero(loadSetting(DISPLAY_BEST_TAVERN_HEROES, false));
    // Off by default: the second row makes the overlay taller, so it is opt in.
    enableTodayLine(loadSetting(DISPLAY_TODAY_LINE, false));
}

Settings::~Settings()
{
    settings.sync();
}

bool Settings::loadSetting(const QString &setting, bool defaultValue) const
{
    bool loadedSetting = defaultValue;
    if(settings.contains(setting))
    {
        loadedSetting = settings.value(setting).toBool();
    }
    else
    {
        qWarning() << "Settings" << setting << "not found.";
    }
    return loadedSetting;
}

bool Settings::getDisplayLocalMatch() const
{
    return displayLocalMultiplayer;
}

bool Settings::getDisplayNameColor() const
{
    return displayNameColor;
}

bool Settings::getDisplayTavernHeroes() const
{
    return displayBestTavernHero;
}

bool Settings::getDisplayTodayLine() const
{
    return displayTodayLine;
}

void Settings::enableTodayLine(bool enable)
{
    this->displayTodayLine = enable;
    settings.setValue(DISPLAY_TODAY_LINE, enable);
    emit settingsUpdated();
}

void Settings::enablePlayerNameColor(bool enable)
{
    this->displayNameColor = enable;
    settings.setValue(DISPLAY_PLAYER_NAME_COLOR, enable);
    emit settingsUpdated();
}

void Settings::enableLocalMultiplayer(bool enable)
{
    this->displayLocalMultiplayer = enable;
    settings.setValue(DISPLAY_LOCAL_MULTIPLAYER, enable);
    emit settingsUpdated();
}

void Settings::enableBestTavernHero(bool enable)
{
    this->displayBestTavernHero = enable;
    settings.setValue(DISPLAY_BEST_TAVERN_HEROES, enable);
    emit settingsUpdated();
}
