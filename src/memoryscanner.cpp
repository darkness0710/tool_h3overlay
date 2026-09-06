#include "memoryscanner.h"

#include <tlhelp32.h>
#include <psapi.h>
#include <wchar.h>
#include <stdint.h>
#include <memory>

#include <QObject>
#include <QDebug>
#include <QRegularExpression>
#include <QRegularExpressionMatch>


#define POINTER_CASTING(addr) reinterpret_cast<LPCVOID>(static_cast<intptr_t>(addr))

MemoryScanner::MemoryScanner(Settings *settings, QObject *parent):
    QObject(parent),
    winLossCounted(false),
    mapNameTried(false),
    mapNameGeneratedTried(false),
    profileTried(false),
    multiplayerType(UNKNOWN_MULTIPLAYER),
    frameCounterNotInMatch(0),
    settings(settings)
{
    this->pollRateTimer.setInterval(100);
    connect(&this->pollRateTimer, &QTimer::timeout, this, &MemoryScanner::updateState);

    this->processCheckTimer.setInterval(2000);
    connect(&this->processCheckTimer, &QTimer::timeout, this, &MemoryScanner::checkProcess);

    clearBuffers();
    memset(&this->lastOpponent, 0, MAX_PLAYER_NAME_LENGTH);
}

bool MemoryScanner::init()
{
    clearBuffers();
    this->pollRateTimer.start();
    this->processCheckTimer.start();
    return this->proc.init();
}

void MemoryScanner::clearBuffers()
{
    memset(&this->localPlayer, 0, sizeof(PlayerStruct));
    memset(&this->opponentPlayer, 0, sizeof(PlayerStruct));
    memset(&this->matchInfo, 0, sizeof(MatchInfoStruct));
    this->localPlayer.tavernHero = 0xFFFFFFFF;
    this->opponentPlayer.tavernHero = 0xFFFFFFFF;
    this->proc.reset();

    emit playerUpdated(this->displayInfo);
}

void MemoryScanner::checkProcess()
{
    this->proc.attachToProcess();
}

bool MemoryScanner::isNameSameWhenLowered(const std::array <char, MAX_PLAYER_NAME_LENGTH > &name1,
                                          const std::array <char, MAX_PLAYER_NAME_LENGTH > &name2)
{
    for (size_t i = 0; i < name1.size();i++)
    {
        // They match if we find the string null terminator
        if(name1[i] == 0 && name2[i] == 0)
        {
            return true;
        }
        if(tolower(name1[i]) != tolower(name2[i]))
        {
            return false;
        }
    }
    return true;
}


void MemoryScanner::extractColoredName(PlayerStruct &player)
{
    strncpy(&player.nameColor[0], "#E0CE75", 8);
    std::array <char, MAX_PLAYER_NAME_LENGTH > tmp;
    memcpy(&tmp[0], &player.info.playerName[0], MAX_PLAYER_NAME_LENGTH);
    for(int i = 0; i < MAX_PLAYER_NAME_LENGTH - 1; i++)
    {
        if(player.info.playerName[i] >= 0x17 && player.info.playerName[i] < 0x20)
        {
            switch(player.info.playerName[i])
            {
            case GameTextColor::Green:
                strncpy(&player.nameColor[0], "#83E16F", 8);
                break;
            case GameTextColor::Red:
                strncpy(&player.nameColor[0], "#F15D5A", 8);
                break;
            case GameTextColor::Pink:
                strncpy(&player.nameColor[0], "#F68D8A", 8);
                break;
            case GameTextColor::Gray:
                strncpy(&player.nameColor[0], "#8D8C8A", 8);
                break;
            /* Black is quite hard to see...
             * case GameTextColor::Black:
                strncpy(&player.nameColor[0], "#000201", 8);
                break;*/
            case GameTextColor::Blue:
                strncpy(&player.nameColor[0], "#5987B7", 8);
                break;
            case GameTextColor::Purple:
                strncpy(&player.nameColor[0], "#C055FF", 8);
                break;
            }

            // Extract the name from the color encoding.
            memcpy(&player.info.playerName[0],
                   &tmp[i+1],
                   MAX_PLAYER_NAME_LENGTH - i);
        }
    }
    for(int i = 0; i < MAX_PLAYER_NAME_LENGTH; i++)
    {
        // The color coding ends with a "}" (ACSII 0x7D) after the text.
        // Replace it with 0 so it doesn't show up in the display.
        if(player.info.playerName[i] == '}')
        {
            player.info.playerName[i] = 0;
        }
    }
    if(this->settings->getDisplayNameColor() == false)
    {
        strncpy(&player.nameColor[0], "#E0CE75", player.nameColor.size());
    }
}

constexpr uint32_t HD_EXE_TO_ACTIVE_TAVERN_OFFSET = 0x2AA694;
constexpr uint32_t HD_EXE_TO_TAVERN_BEST_HERO = 0x2AAA20;


bool MemoryScanner::populateTavernHero(PlayerStruct &player)
{
    if(player.info.heroIDSlot[0] == 0xFFFFFFFF)
    {
        return false;
    }
    uint32_t baseAddr = this->proc.processInfo.exeBaseAddress + HD_EXE_TO_TAVERN_BEST_HERO;

    uint32_t addr = baseAddr + player.playerNumber * sizeof(uint32_t);
    if(readMemory(POINTER_CASTING(addr),
                  &player.tavernHero,
                  sizeof(player.tavernHero)) == false)
    {
        return false;
    }
    return true;
}

void MemoryScanner::updateTavernInfo()
{
    uint32_t pointer;
    if(readMemory(POINTER_CASTING(this->proc.processInfo.exeBaseAddress + HD_EXE_TO_ACTIVE_TAVERN_OFFSET),
                  &pointer,
                  sizeof(pointer)) == false)
    {
        return;
    }

    if(pointer == 0)
    {
        return;
    }
    populateTavernHero(this->localPlayer);
    populateTavernHero(this->opponentPlayer);

    return;
}

bool MemoryScanner::updatePlayersInfo()
{
    if (!this->proc.processInfo.handle)
    {
        return false;
    }

    PlayerBaseStruct players[8];
    bool localAlreadySet = false;
    if(!readMemory(POINTER_CASTING(this->proc.processInfo.playerSectionAddress),
                    &players[0],
                    sizeof(players)))
    {
        qWarning() << "Could not read Player id";
        return false;
    }

    // Player number != player color. E.g in a two player map, the player number
    // will be 0 and 1, but the colors might med 0 (red) and 2 (tan)
    uint32_t playerNumber = 0;
    for(PlayerBaseStruct &player: players)
    {
        if(player.isHuman && player.isLocal && localAlreadySet == false)
        {
            localAlreadySet = true;
            memcpy(&this->localPlayer.info, &player, sizeof(PlayerBaseStruct));
            this->localPlayer.playerNumber = playerNumber;

        }
        else if(player.isHuman)
        {
            // Don't copy data if this is local multiplayer and the setting to
            // read local multiplayer games is set to false
            if(player.isLocal && this->settings->getDisplayLocalMatch() == false)
            {
                if(this->multiplayerType == UNKNOWN_MULTIPLAYER)
                {
                    this->multiplayerType = LOCAL_MULTIPLAYER;
                }
                return false;
            }
            memcpy(&this->opponentPlayer.info, &player, sizeof(PlayerBaseStruct));
            this->opponentPlayer.playerNumber = playerNumber;
        }
        // if a player has at least a town or a hero, they are still in the game.
        if(player.nrOfHeroes > 0 || player.nrOfTowns > 0)
        {
            playerNumber++;
        }
    }

    if(this->multiplayerType == UNKNOWN_MULTIPLAYER)
    {
        this->multiplayerType = ONLINE_MULTIPLAYER;
    }

    extractColoredName(this->localPlayer);
    extractColoredName(this->opponentPlayer);
    this->localPlayer.rating = getPlayerProfile(this->localPlayer).rating;
    this->opponentPlayer.rating = getPlayerProfile(this->opponentPlayer).rating;

    return true;
}


bool MemoryScanner::checkIfInMatch()
{
    bool wasInMatch = this->proc.processInfo.isInMatch;

    if (this->proc.processInfo.statusStruktAddress == 0)
    {
        return false;
    }
    if (readMemory(POINTER_CASTING(this->proc.processInfo.statusStruktAddress),
                   &status,
                   sizeof(StatusWindowStruct)) == false)
    {
        return false;
    }

    this->proc.processInfo.isInMatch = static_cast<bool>(status.isInMatchPointer);

    if(this->proc.processInfo.isInMatch == false && wasInMatch == true)
    {
        this->tradeResult[Left] = "0";
        this->tradeResult[Right] = "0";
        this->winLossCounted = false;
        this->frameCounterNotInMatch = this->status.frameCounter;
    }

    if(this->frameCounterNotInMatch != this->status.frameCounter)
    {
        this->frameCounterNotInMatch = 0;
    }
    this->proc.processInfo.isFinishedLoading = this->frameCounterNotInMatch != status.frameCounter;

    return this->proc.processInfo.isInMatch;
}

void MemoryScanner::setDisplayInfo(PlayerStruct &player, size_t playerNumber)
{
    if(playerNumber >= this->displayInfo.player.size())
    {
        qCritical() << "Player number value can't exceed " << this->displayInfo.player.size();
        return;
    }

    if(this->displayInfo.player[playerNumber].money.isEmpty())
    {
        this->displayInfo.player[playerNumber].money = "0";
    }

    this->displayInfo.player[playerNumber].name = &player.info.playerName[0];

    uint32_t hero = matchInfo.startHero[player.info.color];

    if(this->settings->getDisplayTavernHeroes() &&
            (player.tavernHero != 0xFFFFFFFF))
    {
        hero = player.tavernHero;
    }

    if(heroMap.size() > hero)
    {
        this->displayInfo.player[playerNumber].hero = heroMap[hero];
    }
    if(townMap.size() > this->matchInfo.startTown[player.info.color])
    {
        this->displayInfo.player[playerNumber].town = townMap[matchInfo.startTown[player.info.color]];
    }
    this->displayInfo.player[playerNumber].rating = QString::number(player.rating);
    this->displayInfo.player[playerNumber].wins = QString::number(player.wins);
    this->displayInfo.player[playerNumber].nameColor = QString(&player.nameColor[0]);

    this->displayInfo.winner = &player.winningColor[0];
    if(colorMap.size() > player.info.color)
    {
        this->displayInfo.player[playerNumber].playerColor = colorMap[player.info.color];
    }
    this->displayInfo.player[playerNumber].playerNumer = player.info.color;
    // Travels with the player through the manual swap in the controller, so
    // both the statistics database and the head to head line can tell which
    // side is the local player.
    this->displayInfo.player[playerNumber].isLocalPlayer = player.info.isLocal;

    if(player.info.isLocal)
    {
        this->displayInfo.ratingDelta = QString::number(player.ratingDelta);
    }

    this->displayInfo.mapName = this->mapName;

    this->displayInfo.newMatchToRegister = player.newMatchToRegister;
}


constexpr uint32_t HOTA_EXE_TO_CHAT_OFFSET = 0x29d800;
constexpr uint32_t MAX_CHAT_MESSAGE_COUNT = 20;

bool MemoryScanner::scanForTradeMessages()
{
    chatStruct chat;
    if(!readMemory(POINTER_CASTING(this->proc.processInfo.exeBaseAddress + HOTA_EXE_TO_CHAT_OFFSET),
                    &chat,
                    sizeof(chat)))
    {
        qWarning() << "Could not read chat log.";
        return false;
    }

    // We are not in a lobby, reset trade result.
    if(chat.isInMatchLobbyPointer == 0)
    {
        this->displayInfo.player[Left].money = "0";
        this->displayInfo.player[Right].money = "0";
        return true;
    }

    messageStruct messages[MAX_CHAT_MESSAGE_COUNT];

    if(!readMemory(POINTER_CASTING(chat.messagesPointer),
                    messages,
                    sizeof(messages)))
    {
        qWarning() << "Could not read chat messages";
        return false;
    }

    QRegularExpressionMatch match;

    // The text we are searching for is in the lobby chat windows. In english
    // the text looks like the following row:
    //    <SYSTEM> RED: +1234 Blue: -1234
    // To support other languages, we don't search for the text, just the
    // symbols and numbers.
    static QRegularExpression re("^<.*>: .+: ([+-]?[0-9]+) +.+: ([+-]?[0-9]+)");
    QStringList money = QStringList() << "0" << "0" << "0";

    // In case the system message regarding trade has been overwritten in
    // the circular buffer, we default to the last known trade amount.
    if(chat.messageCount > 0)
    {
        money[1] = tradeResult[Left];
        money[2] = tradeResult[Right];
    }

    for(uint32_t i = chat.messageCount; i > 0; --i)
    {
        uint32_t circularBufferIndex = (i - 1 + chat.messageStart) % MAX_CHAT_MESSAGE_COUNT;
        QString messagePayload(&messages[circularBufferIndex].message[0]);
        match = re.match(messagePayload);
        if (match.hasMatch())
        {
            money = match.capturedTexts();
            break;
        }
    }
    if(chat.messageCount > 0)
    {
        this->tradeResult[Left] = money[1];
        this->tradeResult[Right]= money[2];
    }

    return true;
}


bool MemoryScanner::findMapNameFromGeneratedMap()
{

    if(this->mapNameGeneratedTried)
    {
        return true;
    }
    this->mapNameGeneratedTried = true;
    uint32_t mapInfo = this->proc.followPointer(this->proc.processInfo.statusStruktAddress + 0x5C);
    mapInfo = mapInfo - 1175; // A bit weird to go negative, best I found...

    QString latestMap;

    static QRegularExpression rePack("(?:.*\\d \\d{2};\\d{2} )?(.*)\\.h3m");
    QRegularExpressionMatch match;

    char buffer[150];
    if(!readMemory(POINTER_CASTING(mapInfo),
                    &buffer[0],
                    sizeof(buffer)))
    {
        qWarning() << "Could not scan memory for map name.";
        return false;
    }
    // Make sure we have a null termination.
    buffer[149] = 0;
    QString text(&buffer[0]);

    match = rePack.match(text);

    if (match.hasMatch() && match.capturedTexts().length() > 1)
    {
        QStringList matches(match.capturedTexts());
        latestMap = matches[1]; // First match (0) is the date, second (1) is the map
    }
    this->mapName = latestMap.replace("_"," ");
    return true;
}

bool MemoryScanner::findMapNameFromDescription()
{
    if(mapNameTried)
    {
        return true;
    }

    this->mapNameTried = true;
    uint32_t step = this->proc.followPointer(this->proc.processInfo.statusStruktAddress + 0x5C);
    step = this->proc.followPointer(step - 32);;

    QString foundMapName;

    static QRegularExpression rePack("from pack ([^[,]*)");
    static QRegularExpression reTemp("Template was ([^[,]*)");
    QRegularExpressionMatch matchVariant;

    char buffer[300];
    static char prevBuffer[sizeof(buffer)] = {0};
    if(!readMemory(POINTER_CASTING(step),
                    &buffer[0],
                    sizeof(buffer)))
    {
        qWarning() << "Could not scan memory for map name.";
        return false;
    }

    // Make sure we have a null termination.
    buffer[299] = 0;


    if(memcmp(prevBuffer, buffer, sizeof(buffer)) == 0)
    {
        return true;
    }
    memcpy(prevBuffer, buffer, sizeof(buffer));

    QString text(&buffer[0]);

    // There seems to be two ways the description is generated, depending
    // on the map.
    if(text.contains("from pack"))
    {
        matchVariant = rePack.match(text);
    }
    else if(text.contains("Template was"))
    {
        matchVariant = reTemp.match(text);
    }
    else
    {
        return false;
    }

    if (matchVariant.hasMatch() && matchVariant.capturedTexts().length() > 1)
    {
        QStringList matches(matchVariant.capturedTexts());
        foundMapName = matches[1];
    }

    static QRegularExpression variant("(?:VERSION|VARIANT) ([A-Z0-9])");
    matchVariant = variant.match(text);
    if (matchVariant.hasMatch() && matchVariant.capturedTexts().length() > 1)
    {
        QStringList matches(matchVariant.capturedTexts());
        foundMapName.append(" V:" + matches[1]);
    }

    this->mapName = foundMapName.replace("_"," ");
    return true;
}


constexpr uint32_t HOTA_EXE_TO_MAP_INFO_POINTER = 0x299538;
constexpr uint32_t MAP_INFO_TO_ACTIVE_MAP = 0x83;
constexpr uint32_t MAP_INFO_TO_START_HERO_TOWN_OFFSET = 0x1F6B0;

bool MemoryScanner::updateMatchInfo()
{
    uint64_t mapInfoAddr = this->proc.followPointer(this->proc.processInfo.exeBaseAddress + HOTA_EXE_TO_MAP_INFO_POINTER);

    if(this->proc.processInfo.isInMatch == false)
    {
        return false;
    }
    if(!readMemory(POINTER_CASTING(mapInfoAddr + MAP_INFO_TO_ACTIVE_MAP),
                    &this->proc.processInfo.activeMap,
                    sizeof(this->proc.processInfo.activeMap)))
    {
        qWarning() << "Could not read if map is loaded.";
        return false;
    }
    if(this->proc.processInfo.activeMap == false)
    {
        this->localPlayer.tavernHero = 0xFFFFFFFF;
        this->opponentPlayer.tavernHero = 0xFFFFFFFF;
    }

    if(!readMemory(POINTER_CASTING(mapInfoAddr + MAP_INFO_TO_START_HERO_TOWN_OFFSET),
                    &this->matchInfo,
                    sizeof(this->matchInfo)))
    {
        qWarning() << "Could not read match info.";
        return false;
    }
    return true;
}


// The dword stored at this address is the address of the game global which
// holds the currently active popup window. Verified unchanged in HotA 1.8.1.
constexpr uint32_t HOTA_EXE_TO_POPUP_ACTIVE = 0xF1728;

// How much of the popup window object to search for the result text, and the
// longest text we care about. The text has always been well within the first
// few hundred bytes of the object, the margin is only there so a layout change
// does not silently break the search again.
constexpr uint32_t POPUP_OBJECT_SCAN_SIZE = 0x2000;
constexpr uint32_t POPUP_OBJECT_CHUNK_SIZE = 0x400;
constexpr size_t POPUP_TEXT_MAX_LENGTH = 200;

void MemoryScanner::updateMatchResult()
{
    if(this->winLossCounted || this->proc.processInfo.isInMatch == false)
    {
        return;
    }

    // This is just a pointer which seems to be set as soon as a popup window
    // appers. Use it to figure out when we should start searching for the
    // Win/Loss popup.
    uint32_t anyActivePopup = this->proc.followPointer(this->proc.processInfo.exeBaseAddress + HOTA_EXE_TO_POPUP_ACTIVE);

    anyActivePopup = this->proc.followPointer(anyActivePopup);
    if (anyActivePopup == 0)
    {
        return;
    }

    // The result text lives inside the popup window object itself. It used to
    // be read from a fixed address plus an offset which had to be re-tuned for
    // every single game update, so search the object instead.
    std::array<char, POPUP_OBJECT_SCAN_SIZE> popupObject{};
    size_t readableBytes = 0;
    while(readableBytes < popupObject.size())
    {
        if(!readMemory(POINTER_CASTING(anyActivePopup + readableBytes),
                       &popupObject[readableBytes],
                       POPUP_OBJECT_CHUNK_SIZE))
        {
            // The object is smaller than the scan window, keep what we got.
            break;
        }
        readableBytes += POPUP_OBJECT_CHUNK_SIZE;
    }
    if(readableBytes == 0)
    {
        return;
    }

    // Example of english popup:
    //   YOU WIN!
    //   Rating: +25
    // Not searching for english character to support non-english game clients.
    static QRegularExpression re("\\!.*: ([+-][0-9]+)$",
                                 QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch match;
    char resultColor = 0;

    // The text is prefixed by its color, which is what tells a win from a loss.
    // 0x17 == green color == win
    // 0x18 == red color == loss
    for(size_t i = 0; i + 1 < readableBytes; i++)
    {
        if(popupObject[i] != GameTextColor::Green &&
                popupObject[i] != GameTextColor::Red)
        {
            continue;
        }
        // Skipping the color byte itself. The text is null terminated, but a
        // length limit is needed in case the object holds no terminator.
        size_t available = readableBytes - i - 1;
        QString candidate = QString::fromLatin1(&popupObject[i + 1],
                                                static_cast<qsizetype>(qMin(available,
                                                                            POPUP_TEXT_MAX_LENGTH)));
        candidate.truncate(candidate.indexOf(QChar('\0')) < 0
                               ? candidate.size()
                               : candidate.indexOf(QChar('\0')));
        match = re.match(candidate);
        if(match.hasMatch() && match.capturedTexts().length() > 1)
        {
            resultColor = popupObject[i];
            break;
        }
    }

    if (resultColor != 0)
    {
        QStringList matches(match.capturedTexts());
        this->localPlayer.ratingDelta = matches[1].toInt();
        this->localPlayer.newMatchToRegister = true;
        this->opponentPlayer.newMatchToRegister = true;
        this->winLossCounted = true;

        memset(&this->localPlayer.winningColor[0], 0, this->localPlayer.winningColor.size());
        memset(&this->opponentPlayer.winningColor[0], 0, this->localPlayer.winningColor.size());

        // Did the local player win (green text)?...
        if(resultColor == GameTextColor::Green)
        {
            strncpy(&this->localPlayer.winningColor[0],
                    colorMap[this->localPlayer.info.color],
                    this->localPlayer.winningColor.size() - 1);
            this->localPlayer.wins++;
        }
        else
        {
            strncpy(&this->localPlayer.winningColor[0],
                    colorMap[this->opponentPlayer.info.color],
                    this->localPlayer.winningColor.size() - 1);
            this->opponentPlayer.wins++;
        }
        this->opponentPlayer.winningColor = this->localPlayer.winningColor;
    }
}


QList<MEMORY_BASIC_INFORMATION> MemoryScanner::enumAllVirtualMemory()
{
    size_t start = 0;
    QList<MEMORY_BASIC_INFORMATION> mbis;
    MEMORY_BASIC_INFORMATION mbi;

    // H3 is 32 bit application, so check that range.
    while ((start < pow(2,32)) &&
           (VirtualQueryEx(this->proc.processInfo.handle,
                           POINTER_CASTING(start),
                           &mbi,
                           sizeof(mbi)) == sizeof(mbi)))
    {
        if((mbi.State == MEM_COMMIT) &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY)) &&
            (mbi.Protect & PAGE_GUARD) == 0)
        {
            mbis.append(mbi);
        }

        start = reinterpret_cast<size_t>(mbi.BaseAddress) + reinterpret_cast<size_t>(mbi.RegionSize);
        if(mbi.RegionSize == 0)
        {
            break;
        }
    }
    return mbis;
}


MEMORY_BASIC_INFORMATION MemoryScanner::getMemoryRegion(LPCVOID addr)
{
    MEMORY_BASIC_INFORMATION mbi;

    // H3 is 32 bit application, so check that range.
    if(VirtualQueryEx(this->proc.processInfo.handle,
                    addr,
                    &mbi,
                    sizeof(mbi)) == sizeof(mbi))
    {
        // PAGE_WRITECOPY must be included: the DLLs are relocated, so any of
        // their data pages which have not been written to yet stay WRITECOPY.
        // ReadProcessMemory reads them fine, and leaving them out made this
        // function report "size 0" for a perfectly readable region, which in
        // turn made the region walk in findLocalPlayerProfileAddresses() give
        // up at the first such page.
        if((mbi.State == MEM_COMMIT) &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_WRITECOPY |
                            PAGE_EXECUTE | PAGE_EXECUTE_READ |
                            PAGE_EXECUTE_WRITECOPY)) &&
            (mbi.Protect & PAGE_GUARD) == 0)
        {
            return mbi;
        }
    }
    memset(&mbi, 0, sizeof(mbi));
    return mbi;
}

lobbyProfile MemoryScanner::checkForProfilePointer(uint32_t profilePointer)
{
    lobbyProfile profile = {};

    // Not a pointer we want if the value is < 1000
    if(profilePointer < 1000 || isAddressReadable(POINTER_CASTING(profilePointer)) == false)
    {
        return profile;
    }
    if(!readMemory(POINTER_CASTING(profilePointer),
                   &profile,
                   sizeof(profile)))
    {
        qWarning() << "Could not scan addr " << Qt::hex <<  profilePointer << Qt::dec<< " for profile.";
    }
    return profile;
}

size_t MemoryScanner::readMemoryRegion(LPCVOID regionAddress,
                                       std::unique_ptr<uint8_t[]> &buffer)
{
    MEMORY_BASIC_INFORMATION mbi = getMemoryRegion(regionAddress);

    if(mbi.RegionSize == 0)
    {
        return 0;
    }

    buffer.reset(new uint8_t[mbi.RegionSize]);
    if(!readMemory(mbi.BaseAddress,
                   &buffer.get()[0],
                   mbi.RegionSize))
    {
        qWarning() << "Could not scan memory for local profile.";
        return 0;
    }
    return mbi.RegionSize;
}

// Windows headers do not define a page size constant; 4 KiB on x86/x64.
constexpr uint32_t MEMORY_PAGE_SIZE = 0x1000;
constexpr uint32_t POINTER_TO_PROFILE_OFFSET = 24;

bool MemoryScanner::searchForLocalProfile(std::unique_ptr<uint8_t[]> &buffer, size_t bufferSize)
{
    // A region too small for one profile would make the bound below wrap
    // around, so the loop would read past the end of the buffer.
    if(buffer == nullptr ||
            bufferSize < sizeof(lobbyProfile) + POINTER_TO_PROFILE_OFFSET)
    {
        return false;
    }
    const size_t lastRequiredSize = bufferSize - sizeof(lobbyProfile) - POINTER_TO_PROFILE_OFFSET;
    // We loop through this memory region and look for pointers.
    // We assume that the pointer is aligned to 4 byte boundery.
    for (size_t byteOffset = 0;byteOffset <= lastRequiredSize;byteOffset+=4)
    {
        // Key address is just a pointer to some value which
        // looks to be static. Used to check improve the chanse
        // that we are in the correct place.
        uint32_t keyAddr = *reinterpret_cast<uint32_t*>(&buffer.get()[byteOffset]);
        uint32_t profileAddr = *reinterpret_cast<uint32_t*>(&buffer.get()[byteOffset + POINTER_TO_PROFILE_OFFSET]);

        // A set of data which seems to be static and is its pointer is located
        // close to the pointer to the local player lobby struct.
        constexpr uint8_t cmpArray[] = {0x14, 0xBA, 0x63, 0x00, 0x63, 0x68,
                                        0x61, 0x74, 0x61, 0x62, 0x6B, 0x2E,
                                        0x70, 0x63, 0x78};
        uint8_t cmpBuffer[sizeof(cmpArray)];

        lobbyProfile profile = checkForProfilePointer(profileAddr);
        if(profile.name[0] == 0)
        {
            continue;
        }
        if(isNameSameWhenLowered(this->localPlayer.info.playerName, profile.name) == false)
        {
            continue;
        }

        // Extra check to see if we are in the correct place.
        readMemory(POINTER_CASTING(keyAddr), &cmpBuffer[0], sizeof(cmpBuffer));
        if(memcmp(cmpArray, cmpBuffer, sizeof(cmpArray)) != 0)
        {
            continue;
        }
        this->proc.processInfo.localPlayerProfileAddress = profileAddr;
        return true;
    }
    return false;
}


void MemoryScanner::findLocalPlayerProfileAddresses()
{
    // If opponent is local, we might not be logged in to the online lobby so
    // it might be a bad idea to try and find the lobby data.
    if(this->opponentPlayer.info.isLocal)
    {
        return;
    }
    if(this->proc.processInfo.localPlayerProfileAddress)
    {
        return;
    }

    MODULEENTRY32W module = proc.getModuleEntry(L"HD_HOTA.dll");
    // Keep the max scan size reasonable
    if(module.modBaseSize > 3000000)
    {
        module.modBaseSize = 3000000;
    }

    uint32_t endAddr = this->proc.processInfo.hdDLLBaseAddress + module.modBaseSize;;
    uint32_t regionStartAddr = this->proc.processInfo.hdDLLBaseAddress;

    while(regionStartAddr <= endAddr)
    {
        std::unique_ptr<uint8_t[]> buffer;
        MEMORY_BASIC_INFORMATION mbi = getMemoryRegion(POINTER_CASTING(regionStartAddr));
        uint32_t nextRegionAddr = reinterpret_cast<size_t>(mbi.BaseAddress) + reinterpret_cast<size_t>(mbi.RegionSize);

        // An unreadable region (reserved, no access) is not the end of the
        // module, so step over it instead of giving up on the whole scan.
        if(mbi.RegionSize == 0)
        {
            regionStartAddr += MEMORY_PAGE_SIZE;
            continue;
        }
        // The ponter we are looking for is in a read/write region. Pages which
        // are still WRITECOPY have never been written to, so they cannot hold
        // a pointer the game produced at runtime.
        if((mbi.Protect & PAGE_READWRITE) == 0)
        {
            regionStartAddr = nextRegionAddr;
            continue;
        }
        size_t bufferSize = readMemoryRegion(mbi.BaseAddress, buffer);
        // If we find the local player, we are done.
        if(bufferSize != 0 && searchForLocalProfile(buffer, bufferSize))
        {
            return;
        }
        regionStartAddr = nextRegionAddr;
    }
}


void MemoryScanner::findProfileAddresses()
{
    if(this->profileTried)
    {
        return;
    }
    this->profileTried = true;

    findLocalPlayerProfileAddresses();
}


lobbyProfile MemoryScanner::readPlayerProfile(uint32_t address)
{
    lobbyProfile profile;
    memset(&profile, 0, sizeof(profile));

    if(address == 0)
    {
        return profile;
    }
    memset(&profile, 0, sizeof(lobbyProfile));
    if(!readMemory(POINTER_CASTING(address),
                    &profile,
                    sizeof(lobbyProfile)))
    {
        qWarning() << "Could not read player profile.";
        return profile;
    }
    return profile;
}


void MemoryScanner::resetWinsIfAppropriate()
{
    // Only reset score if it is online multiplayer, local multiplayer is
    // often used after an online match.
    if (this->opponentPlayer.info.isLocal == false)
    {
        // Only reset score if there is a new opponent
        if (isNameSameWhenLowered(this->opponentPlayer.info.playerName,
                                  this->lastOpponent) == false)
        {
            this->localPlayer.wins = 0;
            this->opponentPlayer.wins = 0;
            this->lastOpponent = this->opponentPlayer.info.playerName;
        }
    }
}


lobbyProfile MemoryScanner::getPlayerProfile(const PlayerStruct &player)
{
    lobbyProfile profile;
    // Only the local player's profile is read from the game. The opponent's
    // rating used to be hunted for in the HD_HOTA.dll data, which needed the
    // player to hover the opponent in the lobby first and broke on game
    // updates. It comes from the hotameta.com API instead now.
    if(player.info.isLocal == false)
    {
        memset(&profile, 0, sizeof(lobbyProfile));
        return profile;
    }

    profile = readPlayerProfile(this->proc.processInfo.localPlayerProfileAddress);
    if(isNameSameWhenLowered(player.info.playerName, profile.name))
    {
        return profile;
    }

    memset(&profile, 0, sizeof(lobbyProfile));
    return profile;
}


bool MemoryScanner::isAddressReadable(LPCVOID address)
{
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQueryEx(this->proc.processInfo.handle, address, &mbi, sizeof(mbi));
    return ((mbi.State == MEM_COMMIT) &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY)) &&
            (mbi.Protect & PAGE_GUARD) == 0);
}


bool MemoryScanner::readMemory(LPCVOID address, LPVOID buffer, size_t size)
{
    size_t bytesRead = 0;
    bool success = ReadProcessMemory(this->proc.processInfo.handle,
                                     address,
                                     buffer,
                                     size,
                                     &bytesRead);
    if(success == false)
    {
        DWORD lastError = GetLastError();
        qWarning() << "Could not scan memory. Error code: " << lastError;
    }
    if(bytesRead != size)
    {
        qWarning() << "Expected bytes: " << size << ", bytes read:" << bytesRead;
    }
    return success;
}

void MemoryScanner::updateState()
{
    if (this->proc.processInfo.handle == 0 || this->proc.isGameStillRunning() == false)
    {
        return;
    }

    // Ugly way to trigger a redraw event if the user changed this setting.
    this->localPlayer.displayTavernHero = settings->getDisplayTavernHeroes();

    if(checkIfInMatch() == false)
    {
        this->proc.processInfo.localPlayerProfileAddress = 0;
        this->multiplayerType = UNKNOWN_MULTIPLAYER;
        this->mapNameTried = false;
        this->mapNameGeneratedTried = false;
        this->profileTried = false;
        this->localPlayer.tavernHero = 0xFFFFFFFF;
        this->opponentPlayer.tavernHero = 0xFFFFFFFF;
        // We are not in a match so we might be in the lobby,
        // scan for trade info in chat.
        if(scanForTradeMessages() == false)
        {
            // Failing the scan for trade means that we might no longer be
            // attached to the game (perhaps game was closed). We try
            // to attach again.
            this->proc.attachToProcess();
        }
        return;
    }

    if(updatePlayersInfo())
    {
        if(this->multiplayerType == LOCAL_MULTIPLAYER &&
                this->settings->getDisplayLocalMatch() == false)
        {
            return;
        }
        if(this->proc.processInfo.isFinishedLoading)
        {
            findProfileAddresses();
            findMapNameFromDescription();
            if (this->mapName.isEmpty())
            {
                findMapNameFromGeneratedMap();
            }
            if(this->displayInfo.mapName != this->mapName)
            {
                this->displayInfo.mapName = this->mapName;
                emit playerUpdated(this->displayInfo);
            }
        }

        resetWinsIfAppropriate();
        updateMatchInfo();
        if(this->proc.processInfo.activeMap == false)
        {
            this->mapNameTried = false;
        }
        updateMatchResult();
        updateTavernInfo();

        this->displayInfo.player[0].money = this->tradeResult[localPlayer.info.color > opponentPlayer.info.color];
        this->displayInfo.player[1].money = this->tradeResult[opponentPlayer.info.color > localPlayer.info.color];
        setDisplayInfo(this->localPlayer, 0);
        setDisplayInfo(this->opponentPlayer, 1);

        // Only trigger a GUI update if new data is available to reduce CPU usage.
        if(memcmp(&this->localPlayer, &this->prevLocalPlayer, sizeof(PlayerStruct)) != 0 ||
           memcmp(&this->opponentPlayer, &this->prevRemotePlayer, sizeof(PlayerStruct)) != 0)
        {
            emit playerUpdated(this->displayInfo);
            this->localPlayer.newMatchToRegister = false;
            this->opponentPlayer.newMatchToRegister = false;
        }
        // Copy last "frame" so we can check if something was updated next frame.
        this->prevLocalPlayer = this->localPlayer;
        this->prevRemotePlayer = this->opponentPlayer;
    }
}
