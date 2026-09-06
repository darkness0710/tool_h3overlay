#ifndef GAMESTRUCTS_H
#define GAMESTRUCTS_H

#include <stdint.h>
#include <windows.h>

#include <array>
#include <QString>

constexpr int MAX_PLAYER_NAME_LENGTH = 20;


/** This is the struct of the "lobby profile" which is the data that is
 *  downloaded and displayed in the "popup window" when hovering a user
 *  in the online lobby.
 *  NOTE: This is packed to match the game memory */
typedef struct
{
    uint32_t identifier1;
    uint8_t padding1[8];
    uint32_t rating;
    uint32_t likes;
    std::array <char, MAX_PLAYER_NAME_LENGTH> name;
    uint8_t padding2[64 - 48];
    uint32_t identifier2;
    uint8_t padding3[4];
}__attribute__((packed, aligned(1))) lobbyProfile;

/** Just a collection game states and game addresses */
typedef struct
{
    DWORD pid;
    HANDLE handle;
    uint32_t dllBaseAddress;
    uint32_t hdDLLBaseAddress;
    uint32_t exeBaseAddress;
    uint32_t playerSectionAddress;
    uint32_t statusStruktAddress;
    uint32_t localPlayerProfileAddress;
    bool isInMatch;
    bool isFinishedLoading;
    bool activeMap;
} ProcessInfoStruct;

/** This is the struct which contains player match info.
 *  NOTE: This is packed to match the game memory */
typedef struct
{
    // Aligned with game memory
    uint8_t color;
    uint8_t nrOfHeroes;
    uint8_t padding[2];
    uint32_t selectedHeroID;
    uint32_t heroIDSlot[8];
    uint8_t padding2[22];
    uint8_t nrOfTowns;
    uint8_t selectedTown;
    uint8_t ownedTownID[8];
    uint8_t padding3[84];
    uint32_t wood;
    uint32_t mercury;
    uint32_t ore;
    uint32_t sulfur;
    uint32_t crystal;
    uint32_t gems;
    uint32_t gold;
    uint8_t padding4[20];
    std::array <char, MAX_PLAYER_NAME_LENGTH > playerName;
    uint8_t padding5;
    uint8_t isLocal;
    uint8_t isHuman;
    uint8_t padding6;
    uint8_t isRemote;
    uint8_t padding7[131];

}__attribute__((packed, aligned(1))) PlayerBaseStruct ;


/** Just a collection info related to a player, stored together for convenience.
 *  NOTE: The "info" member is packed to match the game memory */
struct PlayerStruct
{
    PlayerBaseStruct info;
    std::array <char, 8> nameColor;
    uint32_t wins;
    int32_t rating;
    int32_t ratingDelta;
    uint32_t tavernHero;
    uint32_t playerNumber;
    std::array <char, 20> winningColor;
    bool newMatchToRegister;
    bool displayTavernHero;
};

typedef struct
{
    uint32_t startTown[8];
    uint8_t padding1[372];
    uint32_t startHero[8];
}__attribute__((packed, aligned(1))) MatchInfoStruct ;

struct messageStruct
{
    std::array <char, 80> message;
    uint8_t padding1[48];
    uint32_t time;
    uint8_t padding2[4];
}__attribute__((packed, aligned(1)));

struct chatStruct
{
    uint32_t messagesPointer;
    uint32_t messageStart;
    uint32_t messageCount;
    uint8_t padding[76];
    uint32_t isInMatchLobbyPointer;
}__attribute__((packed, aligned(1)));

enum DisplayPosition
{
    Left,
    Right
};

/**
 * @brief The GameTextColor enum These values are defined in the game and
 * are used to set text colors
 */
enum GameTextColor
{
    Green = 0x17,
    Red = 0x18,
    Pink = 0x19,
    Gray = 0x1A,
    Black = 0x1B,
    Blue = 0x1D,
    Purple = 0x1E
};


struct StatusWindowStruct
{
    uint8_t padding1[4];          // 0
    uint32_t isInbattlePointer;   // 4
    uint32_t isInMatchPointer;    // 8
    uint8_t padding2[244];        // 12
    uint32_t frameCounter;        // 256
    uint8_t padding3[656];        // 260
    uint8_t interactionDisplayID; // 916
    uint8_t padding4[7];          // 917
    uint16_t interactionID;       // 924
}__attribute__((packed, aligned(1)));

struct displayPlayerInfoStruct
{
    QString name;
    QString nameColor;
    QString playerColor;
    uint8_t playerNumer = 0;
    QString money;
    QString rating;
    QString hero;
    QString town;
    QString wins;
    bool isLocalPlayer = false;

    /** Head to head record against the other player, and today's record for
     *  the local player. These come from the hotameta.com API rather than from
     *  game memory, so they stay empty until that is wired up and the display
     *  falls back to placeholders.
     *  Only the local player carries the "today" values, the API cannot be
     *  trusted for an opponent it has not crawled. */
    QString h2hWins;
    QString h2hLosses;
    QString todayWins;
    QString todayLosses;
    QString todayRatingChange;
};


struct displayInfoStruct
{
    std::array <displayPlayerInfoStruct, 2> player;
    QString winner;
    QString ratingDelta;
    QString mapName;
    bool newMatchToRegister;
};



// List of heroes, listed in the same order as in memory
static const std::array heroMap={ "Orrin",          // 0
                                  "Valeska",        // 1
                                  "Edric",          // 2
                                  "Sylvia",         // 3
                                  "Lord Haart",     // 4
                                  "Sorsha",         // 5
                                  "Christian",      // 6
                                  "Tyris",          // 7
                                  "Rion",           // 8
                                  "Adela",          // 9
                                  "Cuthbert",       // 10
                                  "Adelaide",       // 11
                                  "Ingham",         // 12
                                  "Sanya",          // 13
                                  "Loynis",         // 14
                                  "Caitlin",        // 15
                                  "Mephala",        // 16
                                  "Ufretin",        // 17
                                  "Jenova",         // 18
                                  "Ryland",         // 19
                                  "Thorgrim",       // 20
                                  "Ivor",           // 21
                                  "Clancy",         // 22
                                  "Kyrre",          // 23
                                  "Coronius",       // 24
                                  "Uland",          // 25
                                  "Elleshar",       // 26
                                  "Gem",            // 27
                                  "Malcom",         // 28
                                  "Melodia",        // 29
                                  "Alagar",         // 30
                                  "Aeris",          // 31
                                  "Piquedram",      // 32
                                  "Thane",          // 33
                                  "Josephine",      // 34
                                  "Neela",          // 35
                                  "Torosar",        // 36
                                  "Fafner",         // 37
                                  "Rissa",          // 38
                                  "Iona",           // 39
                                  "Astral",         // 40
                                  "Halon",          // 41
                                  "Serena",         // 42
                                  "Daremyth",       // 43
                                  "Theodorus",      // 44
                                  "Solmyr",         // 45
                                  "Cyra",           // 46
                                  "Aine",           // 47
                                  "Fiona",          // 48
                                  "Rashka",         // 49
                                  "Marius",         // 50
                                  "Ignatius",       // 51
                                  "Octavia",        // 52
                                  "Calh",           // 53
                                  "Pyre",           // 54
                                  "Nymus",          // 55
                                  "Ayden",          // 56
                                  "Xyron",          // 57
                                  "Axsis",          // 58
                                  "Olema",          // 59
                                  "Calid",          // 60
                                  "Ash",            // 61
                                  "Zydar",          // 62
                                  "Xarfax",         // 63
                                  "Straker",        // 64
                                  "Vokial",         // 65
                                  "Moandor",        // 66
                                  "Charna",         // 67
                                  "Tamika",         // 68
                                  "Isra",           // 69
                                  "Clavius",        // 70
                                  "Galthran",       // 71
                                  "Septienna",      // 72
                                  "Aislinn",        // 73
                                  "Sandro",         // 74
                                  "Nimbus",         // 75
                                  "Thant",          // 76
                                  "Xsi",            // 77
                                  "Vidomina",       // 78
                                  "Nagash",         // 79
                                  "Lorelei",        // 80
                                  "Arlach",         // 81
                                  "Dace",           // 82
                                  "Ajit",           // 83
                                  "Damacon",        // 84
                                  "Gunnar",         // 85
                                  "Synca",          // 86
                                  "Shakti",         // 87
                                  "Alamar",         // 88
                                  "Jaegar",         // 89
                                  "Malekith",       // 90
                                  "Jeddite",        // 91
                                  "Geon",           // 92
                                  "Deemer",         // 93
                                  "Sephinroth",     // 94
                                  "Darkstorn",      // 95
                                  "Yog",            // 96
                                  "Gurnisson",      // 97
                                  "Jabarkas",       // 98
                                  "Shiva",          // 99
                                  "Gretchin",       // 100
                                  "Krellion",       // 101
                                  "Crag Hack",      // 102
                                  "Tyraxor",        // 103
                                  "Gird",           // 104
                                  "Vey",            // 105
                                  "Dessa",          // 106
                                  "Terek",          // 107
                                  "Zubin",          // 108
                                  "Gundula",        // 109
                                  "Oris",           // 110
                                  "Saurug",         // 111
                                  "Bron",           // 112
                                  "Drakon",         // 113
                                  "Wystan",         // 114
                                  "Tazar",          // 115
                                  "Alkin",          // 116
                                  "Korbac",         // 117
                                  "Gerwulf",        // 118
                                  "Broghild",       // 119
                                  "Mirlanda",       // 120
                                  "Rosic",          // 121
                                  "Voy",            // 122
                                  "Verdish",        // 123
                                  "Merist",         // 124
                                  "Styg",           // 125
                                  "Andra",          // 126
                                  "Tiva",           // 127
                                  "Pasis",          // 128
                                  "Thunar",         // 129
                                  "Ignissa",        // 130
                                  "Lacus",          // 131
                                  "Monere",         // 132
                                  "Erdamon",        // 133
                                  "Fiur",           // 134
                                  "Kalt",           // 135
                                  "Luna",           // 136
                                  "Brissa",         // 137
                                  "Ciele",          // 138
                                  "Labetha",        // 139
                                  "Inteus",         // 140
                                  "Aenain",         // 141
                                  "Gelare",         // 142
                                  "Grindan",        // 143
                                  "Sir Mullich",    // 144
                                  "Adrienne",       // 145
                                  "Catherine",      // 146
                                  "Dracon",         // 147
                                  "Gelu",           // 148
                                  "Kilgor",         // 149
                                  "Haart Lich",     // 150
                                  "Mutare",         // 151
                                  "Roland",         // 152
                                  "Mutare Drake",   // 153
                                  "Boragus",        // 154
                                  "Xeron",          // 155
                                  "Corkes",         // 156
                                  "Jeremy",         // 157
                                  "Illor",          // 158
                                  "Derek",          // 109
                                  "Leena",          // 160
                                  "Anabel",         // 161
                                  "Cassiopeia",     // 162
                                  "Miriam",         // 163
                                  "Casmetra",       // 164
                                  "Eovacius",       // 165
                                  "Spint",          // 166
                                  "Andal",          // 167
                                  "Manfred",        // 168
                                  "Zilare",         // 169
                                  "Astra",          // 170
                                  "Dargem",         // 171
                                  "Bidley",         // 172
                                  "Tark",           // 173
                                  "Elmore",         // 174
                                  "Beatrice",       // 175
                                  "Kinkeria",       // 176
                                  "Ranloo",         // 177
                                  "Giselle",        // 178
                                  "Henrietta",      // 179
                                  "Sam",            // 180
                                  "Tancred",        // 181
                                  "Melchior",       // 182
                                  "Floribert",      // 183
                                  "Wynona",         // 184
                                  "Dury",           // 185
                                  "Morton",         // 186
                                  "Celestine",      // 187
                                  "Todd",           // 188
                                  "Agar",           // 189
                                  "Bertram",        // 190
                                  "Wrathmont",      // 191
                                  "Ziph",           // 192
                                  "Victoria",       // 193
                                  "Eanswythe",      // 194
                                  "Frederick",      // 195
                                  "Tavin",          // 196
                                  "Murdoch",        // 197
                                  "Dhuin",          // 198
                                  "Oidana",         // 199
                                  "Neia",           // 200
                                  "Eikthurn",       // 201
                                  "Creyle",         // 202
                                  "Spadum",         // 203
                                  "Kynr",           // 204
                                  "Ergon",          // 205
                                  "Kriv",           // 206
                                  "Glacius",        // 207
                                  "Sial",           // 208
                                  "Dalton",         // 209
                                  "Biarma",         // 210
                                  "Akka",           // 211
                                  "Vehr",           // 212
                                  "Allora",         // 213
                                  "Haugir"};        // 214

static const std::array townMap={"Castle",     // 0
                                 "Rampart",    // 1
                                 "Tower",      // 2
                                 "Inferno",    // 3
                                 "Necropolis", // 4
                                 "Dungeon",    // 5
                                 "Stronghold", // 6
                                 "Fortress",   // 7
                                 "Conflux",    // 8
                                 "Cove",       // 9
                                 "Factory",    // 10
                                 "Bulwark"};   // 11

/** Faction identity colours, in the same order as townMap, used to tint the
 *  overlay frame to the local player's town.
 *  These are picked by hand on purpose. Deriving them from the town artwork
 *  does not work: those pictures are buildings standing on soil, so seven of
 *  the twelve average out to the same shade of tan. */
static const std::array townAccentMap={"#4A7EBB",    // Castle
                                       "#4E9A3E",    // Rampart
                                       "#8FC7E8",    // Tower
                                       "#D4451E",    // Inferno
                                       "#9B93B5",    // Necropolis
                                       "#A33447",    // Dungeon
                                       "#D07C2A",    // Stronghold
                                       "#7A8C3C",    // Fortress
                                       "#7ED0E8",    // Conflux
                                       "#2FA3A0",    // Cove
                                       "#B4703C",    // Factory
                                       "#3FB0BF"};   // Bulwark

// A new town arriving with a game update must not silently end up without a
// colour, which would leave the frame stuck on the previous faction's.
static_assert(townMap.size() == townAccentMap.size(),
              "every entry in townMap needs an accent colour in townAccentMap");

static const std::array colorMap={"Red",    // 0
                                  "Blue",   // 1
                                  "Tan",    // 2
                                  "Green",  // 3
                                  "Orange", // 4
                                  "Purple", // 5
                                  "Teal",   // 6
                                  "Pink"};  // 7


#endif // GAMESTRUCTS_H
