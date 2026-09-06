#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QSettings>


class Settings : public QObject
{
    Q_OBJECT
public:
    Settings(QObject *parent = nullptr);
    ~Settings();

    /**
     * @brief getDisplayNameColor Settings for if special player name color
     * should be displayed.
     * @return True if special player name should be displayed. Returns false
     * otherwise
     */
    bool getDisplayNameColor() const;

    /**
     * @brief getDisplayLocalMultiplayer Settings for the display to update
     * information when in a local single/multiplayer match.
     * @return True if local games should cause an update in the display,
     * false otherwise.
     */
    bool getDisplayLocalMatch() const;

    /**
     * @brief getDisplayTavernHeroes Settings for the display to update
     * hero information when opening the tavern view
     * @return True if the hero display should match what is in
     * the tavern.
     */
    bool getDisplayTavernHeroes() const;

    /**
     * @brief getDisplayTodayLine Settings for the line under the title which
     * shows today's record and the head to head score.
     * @return True if that line should be shown.
     */
    bool getDisplayTodayLine() const;

    /**
     * @brief enablePlayerNameColor Sets the setting for if the color of the
     * player name should match in game color or not
     * @param enable If set to true, color matching will be activated
     */
    void enablePlayerNameColor(bool enable);

    /**
     * @brief enableLocalMultiplayer Sets the setting for if local games should
     * trigger updates for the display
     * @param enable If set to true, local games will cause updates for the
     * main display
     */
    void enableLocalMultiplayer(bool enable);

    /**
     * @brief enableBestTavernHero Sets the setting to enable/disable
     * tracking the best tavern heroes instead of starting heroes.
     * @param enable If set to true, the tavern hero will tracked as soon
     * as the player opens the tavern screen.
     */
    void enableBestTavernHero(bool enable);

    /**
     * @brief enableTodayLine Shows or hides the line under the title. Off by
     * default, since showing it makes the overlay taller. Hiding it gives the
     * screen space back, the overlay shrinks to the single row.
     * @param enable If set to true, the line is shown.
     */
    void enableTodayLine(bool enable);

signals:
    /**
     * @brief settingsUpdated Signal which is triggered as soon as any setting
     * has been changed
     */
    void settingsUpdated();

private:
    /**
     * @brief loadSetting Loades the setting from the file backed settings.
     * @param setting The name of the setting to read
     * @param defaultValue The default value to set if no setting was found
     * @return The value of the setting. Will return default value if the
     * setting was not found.
     */
    bool loadSetting(const QString &setting, bool defaultValue = false) const;

    bool displayNameColor;
    bool displayLocalMultiplayer;
    bool displayBestTavernHero;
    bool displayTodayLine;
    QSettings settings;
};

#endif // SETTINGS_H
