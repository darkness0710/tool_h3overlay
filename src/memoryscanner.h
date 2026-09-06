#ifndef MEMORYSCANNER_H
#define MEMORYSCANNER_H

#include <windows.h>
#include <tlhelp32.h>
#include <array>

#include <QObject>
#include <QTimer>
#include <QList>

#include "gamestructs.h"
#include "processhandler.h"
#include "settings.h"


class MemoryScanner : public QObject
{
    Q_OBJECT

public:

    MemoryScanner(Settings * settings, QObject *parent = nullptr);
    /**
     * @brief init .
     * @return true if it could get admin privilages
     */
    bool init();

private slots:
    /**
     * @brief updateState Called cyclically based on the processCheckTimer.
     * It will update this applications information about the game and trigger
     * a signal if any new data is available.
     */
    void updateState();

    /**
     * @brief checkProcess Called cyclically based on the processCheckTimer.
     * Will call for the process handler to check if the process is still alive
     * or if not, try to find and attach to a new process.
     */
    void checkProcess();

private:
    enum MultiplayerType
    {
        UNKNOWN_MULTIPLAYER,
        LOCAL_MULTIPLAYER,
        ONLINE_MULTIPLAYER
    };

    /**
     * @brief updatePlayer Reads the memory for the player info.
     * @return True if player data could be fetch from the game,
     * otherwise returns false.
     */
    bool updatePlayersInfo();

    /**
     * @brief clearBuffers Clears the internal buffers which contains player
     * and match info which was copied from the game.
     */
    void clearBuffers();

    /**
     * @brief checkIfInMatch There is a pointer which is only not null when a
     * match is active. As soon as the player goes back to the lobby, that
     * pointer is set to null. This function uses that pointer to tell if we
     * are in a match or not. This also updates the internal view if a match
     * is running or not.
     * If we have just left a match (the internal state is match is running),
     * this function will also reset trade result.
     * TODO is to remove trade reset from this function
     * @return true if we are in a match, otherwise returns false.
     */
    bool checkIfInMatch();

    /**
     * @brief setDisplayInfo Populates the struct which will be sent to the
     * display class.
     * @param player The player struct which contains the data read from the
     * game.
     * @param playerNumber Which element in the display struct to stored the
     * converted data into. The only valid values are 0 and 1
     */
    void setDisplayInfo(PlayerStruct &player, size_t playerNumber);

    /**
     * @brief checkWinLossPopup Checks if the WIN/LOSS popup has appeard which
     * tells us if the match is over and someone has won. Follows some pointers
     * to arrive at the text of the checkbox. The first byte will be the color
     * of the text, the color depends on if the local player won or lost.
     * Populats a member variable if a win/loss occured.
     */
    void updateMatchResult();

    /**
     * @brief enumAllVirtualMemory Create an list of all readable virtual
     * memory of the game.
     * @return A list of all readable virtual memory addresses, described in
     * MEMORY_BASIC_INFORMATION structs.
     */
    QList<MEMORY_BASIC_INFORMATION> enumAllVirtualMemory();

    /**
     * @brief enumModuleVirtualMemory Create an list of all readable virtual
     * memory of a specific module.
     * @param name the name of the module
     * @return A list of all readable virtual memory addresses, described in
     * MEMORY_BASIC_INFORMATION structs.
     */
    QList<MEMORY_BASIC_INFORMATION> enumModuleVirtualMemory(const wchar_t *name);

    /**
     * @brief findProfileAddresses Scans the entire game memory for a
     * specific pattern which matches the location where the local player
     * profile is stored. Only valid a few seconds into an active match.
     */
    void findProfileAddresses();

    /**
     * @brief getPlayerProfile A wrapper function which will call other
     * functions to fetch profile information based on if the player is local
     * or remote.
     * @param player The player name to search for.
     * @return The online profile for the matching player. If no player was
     * found, it will return an profile with everything set to 0.
     */
    lobbyProfile getPlayerProfile(const PlayerStruct &player);

    /**
     * @brief readPlayerProfile Fetches the local player online profile if
     * the player address has previously been identified (findLocalProfileAddress)
     * @return The online profile for the local player. If no player was
     * found, it will return an profile with everything set to 0.
     */
    lobbyProfile readPlayerProfile(uint32_t address);

    /**
     * @brief scanForTradeMessages Scan the online match lobby for chat
     * messages which looks like trade messages
     * @return true if it could read chat logs, that does not mean it found
     * any trade messages.
     */
    bool scanForTradeMessages();
    
    /**
     * @brief findMapNameFromGeneratedMap Will try to find the map name based
     * on the generated map string (<players><date><time><mapname>) from
     * the scenario data.
     * @return True if the function had any errors. The map name, if found, will
     * be stored in the member variable "mapName".
     */
    bool findMapNameFromGeneratedMap();
    
    /**
     * @brief findMapNameFromDescription Will try to find the map name based
     * on the map description from the scenario data. Only works for random
     * generated maps.
     * @return True if the function had any errors. The map name, if found, will
     * be stored in the member variable "mapName".
     */
    bool findMapNameFromDescription();
    
    /**
     * @brief updateMatchInfo Reads the match info (starter town/hero).
     * @return true if the data could be read.
     */
    bool updateMatchInfo();

    /**
     * @brief updateMatchAddresses Searches for usfull pointers to a bunch
     * of structs.
     */
    void updateMatchAddresses();

    /**
     * @brief resetWinsIfAppropriate If a new online game has started with a
     * new opponent, the win counters are reset.
     */
    void resetWinsIfAppropriate();

    /**
     * @brief extractColoredName Replaces the color encoded player name with
     * the plain player name. Stores the name color in a seperate member
     * variable
     * @param player The player struct to replace the player name in.
     */
    void extractColoredName(PlayerStruct &player);

    /**
     * @brief isNameSameWhenLowered Compares the two names if they are the
     * same, case insensitive
     * @param name1 first name to compare
     * @param name2 second name to compare
     * @return true if the names are the same, returns false otherwise
     */
    bool isNameSameWhenLowered(const std::array<char, MAX_PLAYER_NAME_LENGTH> &name1,
                               const std::array<char, MAX_PLAYER_NAME_LENGTH> &name2);


    /**
     * @brief readMemory Wrapper function to read the virtual memory of the game
     * @param address The address to read
     * @param buffer A pointer to the buffer where the read data should be stored
     * @param size The amount of data to read, should also match buffer size
     * @return true if the reading was successful, returns false otherwise
     */
    bool readMemory(LPCVOID address, LPVOID buffer, size_t size);

    /**
     * @brief isAddressReadable Checks if the page associated with the address
     * is readable
     * @param address The address to check for readability
     * @return true if the address is readable, false otherwise
     */
    bool isAddressReadable(LPCVOID address);

    /**
     * @brief getMemoryRegion Finds the start bound and size of the memory
     * region the given memory address is in.
     * @param addr The address to receive the memory region for
     * @return An MBI object which contains the start address and size of
     * the memory region. The size is 0 if the address is invalid.
     */
    MEMORY_BASIC_INFORMATION getMemoryRegion(LPCVOID addr);

    /**
     * @brief readMemoryRegion Fetches the data for the entire memory region
     * where the region is specified by the given address.
     * This function will allocate memory which matches the amount of data
     * in the region.
     * @param regionAddress The address specifies which region to read.
     * Any address within a memory region will return the same data.
     * @param buffer The pointer which will hold the allocated data.
     * @return The size of the memory region which is also the size of
     * the allocated data pointet do by "byffer".
     */
    size_t readMemoryRegion(LPCVOID regionAddress, std::unique_ptr<uint8_t[]> &buffer);

    /**
     * @brief searchForLocalProfile Searches the given buffer for the lobby
     * profile which matches the local player. If a profile is found, sets
     * the lobby address of the local player.
     * @param buffer The buffer to search in.
     * @param bufferSize The size of the buffer to search in
     * @return True if the local player lobby profile is found.
     */
    bool searchForLocalProfile(std::unique_ptr<uint8_t[]> &buffer, size_t bufferSize);

    /**
     * @brief findLocalPlayerProfileAddresses Scans the memory to find the
     * memory address of the local player. Will only search for profile
     * info if we are in an online match.
     */
    void findLocalPlayerProfileAddresses();


    /**
     * @brief checkForProfilePointer Checks if the data might be a pointer,
     * if so tries to follow the pointer and treat the result as a
     * lobby profile struct. Often returns false positive.
     * @param profilePointer The data which may hold a pointer to a profile
     * structure. Assumes that there is at least 4 bytes available.
     * @return If the pointer could not be followed, all fields are set to 0.
     * Otherwise, the data will be populated with whatever was stored at
     * that address.
     */
    lobbyProfile checkForProfilePointer(uint32_t profilePointer);

    /**
     * @brief updateTavernInfo Checks if the Thieves' Guild window is
     * open, if so, it tries to look for the "best hero" for both players.
     */
    void updateTavernInfo();

    /**
     * @brief populateTavernHero Checks if the Thieves' Guild window
     * is open, if so updates the "known best hero".
     * Requires player.playerNumber to be set to match the
     * "active player number", which is not necessarily the color.
     * @param player The player to update the Thieves' Guild hero for
     * @return If the "seen" hero could be found, returns true.
     */
    bool populateTavernHero(PlayerStruct &player);

    /** * * * * * * * * * *
     *  Member variables  *
     ** * * * * * * * * * */

    QTimer pollRateTimer;
    QTimer processCheckTimer;

    PlayerStruct localPlayer;
    PlayerStruct opponentPlayer;
    PlayerStruct prevLocalPlayer;
    PlayerStruct prevRemotePlayer;
    StatusWindowStruct status;
    ProcessHandler proc;

    MatchInfoStruct matchInfo;

    std::array <char, MAX_PLAYER_NAME_LENGTH> lastOpponent;

    displayInfoStruct displayInfo;
    std::array <QString, 2> tradeResult;

    bool winLossCounted;
    bool mapNameTried;
    bool mapNameGeneratedTried;
    bool profileTried;
    QString mapName;

    MultiplayerType multiplayerType;
    uint32_t frameCounterNotInMatch;

    Settings *settings;

signals:
    /**
     * @brief playerUpdated Signal which is emitted when any of the tracked
     * data has been updated
     * @param displayInfo A struct which contain all the current information to
     * be displayed
     */
    void playerUpdated(displayInfoStruct displayInfo);
};

#endif // MEMORYSCANNER_H
