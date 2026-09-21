#include "offsetscanner.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include <QDebug>
#include <QHash>
#include <QSettings>

#include "gamestructs.h"

namespace {

// Same organisation/application as Settings so everything lands in one place.
constexpr char SETTINGS_ORGANISATION[] = "H3Overlay";
constexpr char SETTINGS_APPLICATION[] = "H3Overlay";

// Enough of the mapped image to cover the DOS stub, the PE headers and the
// section table.
constexpr uint32_t HEADER_READ_SIZE = 0x1000;

// The .text of hota.dll is a couple of megabytes. Anything far beyond that
// means we misparsed the headers, and we should not try to allocate it.
constexpr uint32_t MAX_TEXT_SIZE = 64u * 1024u * 1024u;

// Mirrors scripts/find_hota_offsets.py: the instruction that touches the hero
// array pointer sits within a few bytes of the sizeof(GuildHeroStruct)
// immediate that is used to stride through it.
constexpr int WINDOW_BEFORE = 8;
constexpr int WINDOW_AFTER = 24;

// modrm bytes whose mod/rm part selects "disp32 only", i.e. an absolute address.
constexpr uint8_t ABSOLUTE_MODRM_MASK = 0xC7;
constexpr uint8_t ABSOLUTE_MODRM_VALUE = 0x05;

constexpr uint8_t OPCODE_MOV_EAX_ABSOLUTE = 0xA1;   // mov eax, [imm32]
constexpr uint8_t OPCODE_MOV_ABSOLUTE_EAX = 0xA3;   // mov [imm32], eax
constexpr uint8_t OPCODE_MOV_TO_REGISTER = 0x8B;    // mov r32, [imm32]
constexpr uint8_t OPCODE_MOV_FROM_REGISTER = 0x89;  // mov [imm32], r32
constexpr uint8_t OPCODE_ADD_TO_REGISTER = 0x03;    // add r32, [imm32]
constexpr uint8_t OPCODE_GROUP5 = 0xFF;             // push [imm32] and friends

struct AbsoluteForm
{
    uint8_t opcode;
    bool hasModrm;
    int immediateAt;
};

// Instruction forms that load from or store to a global through an absolute
// address. The shortest of these is 5 bytes, the longest 6.
constexpr AbsoluteForm ABSOLUTE_FORMS[] = {
    { OPCODE_MOV_EAX_ABSOLUTE,   false, 1 },
    { OPCODE_MOV_ABSOLUTE_EAX,   false, 1 },
    { OPCODE_ADD_TO_REGISTER,    true,  2 },
    { OPCODE_MOV_TO_REGISTER,    true,  2 },
    { OPCODE_MOV_FROM_REGISTER,  true,  2 },
    { OPCODE_GROUP5,             true,  2 },
};

constexpr int SHORTEST_ABSOLUTE_FORM = 5;

// How many of the ranked candidates are kept for confirmation. The right one
// has always been first in practice; the rest are insurance.
constexpr int MAX_CANDIDATES = 8;

// The first heroes in the array are the Castle heroes, which exist in every
// map, so sampling the start of the array is enough to recognise it.
constexpr uint32_t HEROES_TO_SAMPLE = 16;

// A 32 bit user mode heap pointer.
constexpr uint32_t MIN_USER_ADDRESS = 0x00010000;
constexpr uint32_t MAX_USER_ADDRESS = 0x7FFEFFFF;

constexpr uint8_t MAX_PLAYER_COLOR = 7;
constexpr uint8_t UNOWNED_COLOR = 0xFF;

constexpr int HERO_NAME_SIZE = static_cast<int>(sizeof(GuildHeroStruct::heroName));
constexpr int MIN_HERO_NAME_LENGTH = 2;

// Two independent ways to accept a candidate. The first leans on heroID
// matching the array index, which is what updatePlayer() already relies on to
// find the selected hero. The second one does not, so a HotA build that stops
// numbering heroes that way can still be recognised by its names and colors.
constexpr int MIN_ID_MATCHES = 12;
constexpr int MIN_NAME_MATCHES_WITH_IDS = 8;
constexpr int MIN_NAME_MATCHES_ALONE = 15;
constexpr int MIN_COLOR_MATCHES_ALONE = 15;

/**
 * @brief peek Reads a little endian value out of a buffer, bounds checked.
 * Used instead of a pointer cast because the PE offsets are not aligned.
 */
template<typename T>
bool peek(const QByteArray &blob, qsizetype at, T &out)
{
    if (at < 0 || at + static_cast<qsizetype>(sizeof(T)) > blob.size())
    {
        return false;
    }
    memcpy(&out, blob.constData() + at, sizeof(T));
    return true;
}

/**
 * @brief looksLikeHeroName Whether the 13 byte name field holds a plausible
 * hero name: NUL terminated, printable, and starting with a capital or with a
 * byte from a localised code page.
 */
bool looksLikeHeroName(const char * const name)
{
    int length = 0;
    while (length < HERO_NAME_SIZE && name[length] != '\0')
    {
        ++length;
    }
    if (length < MIN_HERO_NAME_LENGTH || length >= HERO_NAME_SIZE)
    {
        return false;
    }
    for (int index = 0; index < length; ++index)
    {
        const unsigned char character = static_cast<unsigned char>(name[index]);
        if (character < 0x20 || character == 0x7F)
        {
            return false;
        }
    }
    const unsigned char first = static_cast<unsigned char>(name[0]);
    return (first >= 'A' && first <= 'Z') || first >= 0x80;
}

} // namespace


HeroPointerLocator::HeroPointerLocator():
    chosenOffset(0),
    scanned(false),
    confirmed(false)
{
}

void HeroPointerLocator::reset()
{
    this->identity.clear();
    this->candidates.clear();
    this->chosenOffset = 0;
    this->scanned = false;
    this->confirmed = false;
}

bool HeroPointerLocator::isConfirmed() const
{
    return this->confirmed;
}

uint32_t HeroPointerLocator::offset() const
{
    return this->chosenOffset;
}

QString HeroPointerLocator::moduleIdentity() const
{
    return this->identity;
}

QString HeroPointerLocator::settingsKey() const
{
    return QStringLiteral("memoryOffsets/heroSectionPointer/") + this->identity;
}

void HeroPointerLocator::forgetCachedOffset()
{
    if (!this->identity.isEmpty())
    {
        QSettings stored(SETTINGS_ORGANISATION, SETTINGS_APPLICATION);
        stored.remove(settingsKey());
    }
    reset();
}

bool HeroPointerLocator::readMemory(HANDLE process,
                                    uintptr_t address,
                                    void *destination,
                                    size_t size) const
{
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(process,
                           reinterpret_cast<LPCVOID>(address),
                           destination,
                           size,
                           &bytesRead))
    {
        return false;
    }
    return bytesRead == size;
}

bool HeroPointerLocator::readModuleLayout(HANDLE process,
                                          uintptr_t dllBaseAddress,
                                          ModuleLayout &layout) const
{
    QByteArray headers;
    headers.resize(static_cast<qsizetype>(HEADER_READ_SIZE));
    if (!readMemory(process, dllBaseAddress, headers.data(), HEADER_READ_SIZE))
    {
        return false;
    }

    if (headers[0] != 'M' || headers[1] != 'Z')
    {
        qWarning() << "hota.dll is not mapped where we expected it.";
        return false;
    }

    uint32_t peOffset = 0;
    if (!peek(headers, 0x3C, peOffset))
    {
        return false;
    }

    const qsizetype pe = static_cast<qsizetype>(peOffset);
    static const char peSignature[4] = { 'P', 'E', '\0', '\0' };
    if (pe + 24 > headers.size() ||
            memcmp(headers.constData() + pe, peSignature, sizeof(peSignature)) != 0)
    {
        qWarning() << "hota.dll has no PE signature.";
        return false;
    }

    const qsizetype coff = pe + 4;
    uint16_t sectionCount = 0;
    uint16_t optionalSize = 0;
    uint16_t magic = 0;
    if (!peek(headers, coff + 2, sectionCount) ||
            !peek(headers, coff + 4, layout.timestamp) ||
            !peek(headers, coff + 16, optionalSize))
    {
        return false;
    }

    const qsizetype optional = coff + 20;
    if (!peek(headers, optional, magic) || magic != 0x10B)
    {
        qWarning() << "hota.dll is not a PE32 image.";
        return false;
    }
    if (!peek(headers, optional + 56, layout.imageSize))
    {
        return false;
    }

    layout.textRva = 0;
    layout.textSize = 0;
    layout.dataRva = 0;
    layout.dataSize = 0;

    const qsizetype sectionTable = optional + static_cast<qsizetype>(optionalSize);
    for (uint16_t index = 0; index < sectionCount; ++index)
    {
        const qsizetype entry = sectionTable + static_cast<qsizetype>(index) * 40;
        if (entry + 40 > headers.size())
        {
            break;
        }

        char name[9] = { 0 };
        memcpy(name, headers.constData() + entry, 8);

        uint32_t virtualSize = 0;
        uint32_t rva = 0;
        if (!peek(headers, entry + 8, virtualSize) || !peek(headers, entry + 12, rva))
        {
            break;
        }

        if (strcmp(name, ".text") == 0)
        {
            layout.textRva = rva;
            layout.textSize = virtualSize;
        }
        else if (strcmp(name, ".data") == 0)
        {
            layout.dataRva = rva;
            layout.dataSize = virtualSize;
        }
    }

    if (layout.textSize == 0 || layout.dataSize == 0)
    {
        qWarning() << "hota.dll is missing a .text or .data section.";
        return false;
    }
    return true;
}

QVector<uint32_t> HeroPointerLocator::scanForCandidates(HANDLE process,
                                                        uintptr_t dllBaseAddress,
                                                        const ModuleLayout &layout) const
{
    QVector<uint32_t> ranked;

    if (layout.textSize > MAX_TEXT_SIZE)
    {
        qWarning() << "The .text of hota.dll has an implausible size:" << layout.textSize;
        return ranked;
    }

    QByteArray text;
    text.resize(static_cast<qsizetype>(layout.textSize));
    if (!readMemory(process, dllBaseAddress + layout.textRva, text.data(), layout.textSize))
    {
        qWarning() << "Could not read the .text of hota.dll to scan for the hero pointer.";
        return ranked;
    }

    const uint8_t * const code = reinterpret_cast<const uint8_t *>(text.constData());
    const qsizetype size = text.size();

    const uint32_t stride = static_cast<uint32_t>(sizeof(GuildHeroStruct));
    uint8_t needle[sizeof(uint32_t)];
    memcpy(needle, &stride, sizeof(needle));

    const uintptr_t moduleLow = dllBaseAddress;
    const uintptr_t moduleHigh = dllBaseAddress + layout.imageSize;
    const uint32_t dataLow = layout.dataRva;
    const uint32_t dataHigh = layout.dataRva + layout.dataSize;

    QHash<uint32_t, int> votes;
    QHash<uint32_t, int> stores;

    for (qsizetype position = 0; position + 4 <= size; ++position)
    {
        if (code[position] != needle[0] ||
                memcmp(code + position, needle, sizeof(needle)) != 0)
        {
            continue;
        }

        // The pointer is loaded just before or just after the stride is used.
        const qsizetype centre = position + 4;
        const qsizetype low = std::max<qsizetype>(0, centre - WINDOW_BEFORE);
        const qsizetype high = std::min<qsizetype>(size, centre + WINDOW_AFTER);

        for (qsizetype at = low; at + SHORTEST_ABSOLUTE_FORM + 1 <= high; ++at)
        {
            for (const AbsoluteForm &form : ABSOLUTE_FORMS)
            {
                if (code[at] != form.opcode)
                {
                    continue;
                }
                if (form.hasModrm &&
                        (code[at + 1] & ABSOLUTE_MODRM_MASK) != ABSOLUTE_MODRM_VALUE)
                {
                    break;
                }
                if (at + form.immediateAt + 4 > high)
                {
                    break;
                }

                uint32_t absolute = 0;
                memcpy(&absolute, code + at + form.immediateAt, sizeof(absolute));
                const uintptr_t absoluteAddress = static_cast<uintptr_t>(absolute);
                if (absoluteAddress < moduleLow || absoluteAddress >= moduleHigh)
                {
                    break;
                }

                // A pointer variable is dword aligned and lives in .data;
                // anything else is a misaligned read that happened to land
                // inside the section.
                const uint32_t rva = static_cast<uint32_t>(absoluteAddress - moduleLow);
                if ((rva % 4) != 0 || rva < dataLow || rva >= dataHigh)
                {
                    break;
                }

                ++votes[rva];
                if (form.opcode == OPCODE_MOV_ABSOLUTE_EAX ||
                        form.opcode == OPCODE_MOV_FROM_REGISTER)
                {
                    ++stores[rva];
                }
                break;
            }
        }
    }

    // A store to the global anywhere in .text proves it is a pointer variable
    // that gets assigned at runtime, which is what we need to be able to
    // follow. Look outside the stride window too, for candidates that had none.
    const QList<uint32_t> voted = votes.keys();
    for (const uint32_t rva : voted)
    {
        if (stores.value(rva) > 0)
        {
            continue;
        }

        const uint32_t absolute = static_cast<uint32_t>(dllBaseAddress + rva);
        uint8_t pattern[sizeof(uint32_t)];
        memcpy(pattern, &absolute, sizeof(pattern));

        for (qsizetype at = 1; at + 4 <= size; ++at)
        {
            if (code[at] != pattern[0] ||
                    memcmp(code + at, pattern, sizeof(pattern)) != 0)
            {
                continue;
            }
            if (at >= 2 &&
                    code[at - 2] == OPCODE_MOV_FROM_REGISTER &&
                    (code[at - 1] & ABSOLUTE_MODRM_MASK) == ABSOLUTE_MODRM_VALUE)
            {
                ++stores[rva];
            }
            else if (code[at - 1] == OPCODE_MOV_ABSOLUTE_EAX)
            {
                ++stores[rva];
            }
        }
    }

    struct Candidate
    {
        uint32_t rva;
        int sites;
        int stores;
    };

    QVector<Candidate> scored;
    scored.reserve(voted.size());
    for (const uint32_t rva : voted)
    {
        scored.append(Candidate{ rva, votes.value(rva), stores.value(rva) });
    }
    std::sort(scored.begin(), scored.end(), [](const Candidate &left, const Candidate &right) {
        if (left.sites != right.sites)
        {
            return left.sites > right.sites;
        }
        if (left.stores != right.stores)
        {
            return left.stores > right.stores;
        }
        return left.rva < right.rva;
    });

    const int keep = std::min(MAX_CANDIDATES, static_cast<int>(scored.size()));
    for (int index = 0; index < keep; ++index)
    {
        qInfo("Hero pointer candidate 0x%X: %d sites, %d stores.",
              scored[index].rva, scored[index].sites, scored[index].stores);
        ranked.append(scored[index].rva);
    }
    return ranked;
}

bool HeroPointerLocator::confirmCandidate(HANDLE process,
                                          uintptr_t dllBaseAddress,
                                          uint32_t candidateRva) const
{
    uint32_t arrayAddress = 0;
    if (!readMemory(process, dllBaseAddress + candidateRva, &arrayAddress, sizeof(arrayAddress)))
    {
        return false;
    }
    // Null until a map is loaded, which is the normal state in the main menu.
    if (arrayAddress < MIN_USER_ADDRESS || arrayAddress > MAX_USER_ADDRESS)
    {
        return false;
    }

    int idMatches = 0;
    int nameMatches = 0;
    int colorMatches = 0;

    for (uint32_t index = 0; index < HEROES_TO_SAMPLE; ++index)
    {
        GuildHeroStruct hero;
        const uintptr_t heroAddress = static_cast<uintptr_t>(arrayAddress) +
                sizeof(GuildHeroStruct) * index;
        if (!readMemory(process, heroAddress, &hero, sizeof(hero)))
        {
            return false;
        }

        if (static_cast<uint32_t>(hero.heroID) == index)
        {
            ++idMatches;
        }
        if (looksLikeHeroName(hero.heroName))
        {
            ++nameMatches;
        }
        if (hero.color <= MAX_PLAYER_COLOR || hero.color == UNOWNED_COLOR)
        {
            ++colorMatches;
        }
    }

    const bool byHeroIds = idMatches >= MIN_ID_MATCHES &&
            nameMatches >= MIN_NAME_MATCHES_WITH_IDS;
    const bool byHeroData = nameMatches >= MIN_NAME_MATCHES_ALONE &&
            colorMatches >= MIN_COLOR_MATCHES_ALONE;

    if (byHeroIds || byHeroData)
    {
        qInfo("Candidate 0x%X points at %u heroes: %d ids, %d names, %d colors matched.",
              candidateRva, HEROES_TO_SAMPLE, idMatches, nameMatches, colorMatches);
        return true;
    }
    return false;
}

uint32_t HeroPointerLocator::resolve(HANDLE process,
                                     uintptr_t dllBaseAddress,
                                     uint32_t fallbackOffset)
{
    if (process == nullptr || dllBaseAddress == 0)
    {
        return fallbackOffset;
    }

    ModuleLayout layout;
    if (!readModuleLayout(process, dllBaseAddress, layout))
    {
        return this->chosenOffset != 0 ? this->chosenOffset : fallbackOffset;
    }

    // Keyed by build rather than by the HotA version string: HD Mod ships its
    // own hota.dll, so two installs calling themselves the same version can
    // still need different offsets.
    const QString currentIdentity = QStringLiteral("%1_%2")
            .arg(layout.timestamp, 8, 16, QLatin1Char('0'))
            .arg(layout.imageSize, 8, 16, QLatin1Char('0'));
    if (currentIdentity != this->identity)
    {
        reset();
        this->identity = currentIdentity;
    }

    if (this->confirmed)
    {
        return this->chosenOffset;
    }

    if (!this->scanned)
    {
        this->scanned = true;
        this->chosenOffset = fallbackOffset;

        QSettings stored(SETTINGS_ORGANISATION, SETTINGS_APPLICATION);
        const QVariant cached = stored.value(settingsKey());
        if (cached.isValid())
        {
            this->chosenOffset = cached.toUInt();
            this->candidates.append(this->chosenOffset);
            qInfo("Cached hero pointer offset for hota.dll %s is 0x%X.",
                  qUtf8Printable(this->identity), this->chosenOffset);
        }

        const QVector<uint32_t> found = scanForCandidates(process, dllBaseAddress, layout);
        for (const uint32_t rva : found)
        {
            if (!this->candidates.contains(rva))
            {
                this->candidates.append(rva);
            }
        }

        // Worth trying even when the scan found nothing: on a build we already
        // ship an offset for, this confirms it and stops the retries.
        if (!this->candidates.contains(fallbackOffset))
        {
            this->candidates.append(fallbackOffset);
        }

        if (found.isEmpty())
        {
            qWarning() << "No hero array pointer candidate found in hota.dll" << this->identity
                       << "- GuildHeroStruct in gamestructs.h no longer matches the game.";
        }
        else if (!cached.isValid())
        {
            this->chosenOffset = found.first();
        }
    }

    // Retried on every call until one sticks, because the hero array does not
    // exist until a map has been loaded.
    for (const uint32_t candidate : std::as_const(this->candidates))
    {
        if (!confirmCandidate(process, dllBaseAddress, candidate))
        {
            continue;
        }

        this->chosenOffset = candidate;
        this->confirmed = true;

        QSettings stored(SETTINGS_ORGANISATION, SETTINGS_APPLICATION);
        stored.setValue(settingsKey(), candidate);

        qInfo("Confirmed hero pointer offset 0x%X for hota.dll %s.",
              candidate, qUtf8Printable(this->identity));
        break;
    }

    return this->chosenOffset;
}
