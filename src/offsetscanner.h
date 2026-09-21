#ifndef OFFSETSCANNER_H
#define OFFSETSCANNER_H

#include <windows.h>
#include <stdint.h>

#include <QString>
#include <QVector>

/**
 * @brief The HeroPointerLocator class finds the RVA of the hero array pointer
 * inside hota.dll while the game is running.
 *
 * Every HotA release rebuilds hota.dll, which shifts that RVA and used to
 * require rebuilding H3roViewer with a new hardcoded
 * HOTA_DLL_TO_HERO_SECTION_POINTER_OFFSET. This class runs the same signature
 * scan that scripts/find_hota_offsets.py does, except it reads the module that
 * is already mapped into the game process and then *confirms* the candidate by
 * following it and checking that the memory behind it really looks like the
 * hero array. The hardcoded constant is only used as a fallback.
 *
 * The confirmed value is cached in QSettings, keyed by the PE timestamp and
 * image size of hota.dll, so swapping in a different build (HD Mod ships its
 * own) invalidates the cache on its own.
 */
class HeroPointerLocator
{
public:
    HeroPointerLocator();

    /**
     * @brief resolve Returns the offset to add to the hota.dll base address to
     * reach the hero array pointer. Cheap to call repeatedly: the signature
     * scan runs once per hota.dll build, and confirmation stops once a
     * candidate has been verified against the running game.
     * @param process An open handle to the game, with PROCESS_VM_READ.
     * @param dllBaseAddress Base address of hota.dll inside that process.
     * @param fallbackOffset The compiled-in offset to use when the scan or the
     * confirmation cannot produce an answer.
     * @return The offset to use right now.
     */
    uint32_t resolve(HANDLE process, uintptr_t dllBaseAddress, uint32_t fallbackOffset);

    /**
     * @brief isConfirmed Whether the returned offset was verified against the
     * running game rather than guessed or taken from the fallback.
     */
    bool isConfirmed() const;

    /**
     * @brief offset The offset resolve() last settled on.
     */
    uint32_t offset() const;

    /**
     * @brief moduleIdentity "<PE timestamp>_<image size>" of the hota.dll that
     * is currently loaded, or an empty string if it has not been read yet.
     */
    QString moduleIdentity() const;

    /**
     * @brief forgetCachedOffset Drops the stored value for the current hota.dll
     * and forces a full rescan on the next resolve(). Backs the "re-detect"
     * button in the settings window.
     */
    void forgetCachedOffset();

    /**
     * @brief reset Throws away everything learned about the current hota.dll.
     */
    void reset();

private:
    struct ModuleLayout
    {
        uint32_t timestamp;
        uint32_t imageSize;
        uint32_t textRva;
        uint32_t textSize;
        uint32_t dataRva;
        uint32_t dataSize;
    };

    /**
     * @brief readMemory Reads from the game process, requiring a full read.
     * @return true only if every requested byte was read.
     */
    bool readMemory(HANDLE process, uintptr_t address, void *destination, size_t size) const;

    /**
     * @brief readModuleLayout Parses the PE headers of the mapped hota.dll to
     * find the .text and .data sections and to identify the build.
     * @return true if the headers could be read and look like PE32.
     */
    bool readModuleLayout(HANDLE process, uintptr_t dllBaseAddress, ModuleLayout &layout) const;

    /**
     * @brief scanForCandidates Looks for globals in .data that are touched by
     * code near a sizeof(GuildHeroStruct) immediate, best first.
     */
    QVector<uint32_t> scanForCandidates(HANDLE process,
                                        uintptr_t dllBaseAddress,
                                        const ModuleLayout &layout) const;

    /**
     * @brief confirmCandidate Follows the candidate pointer and checks that the
     * heroes behind it have sane ids, names and colors.
     * @return true if the memory really is the hero array.
     */
    bool confirmCandidate(HANDLE process, uintptr_t dllBaseAddress, uint32_t candidateRva) const;

    /**
     * @brief settingsKey The QSettings key holding the offset for the hota.dll
     * build that is currently loaded.
     */
    QString settingsKey() const;

    QString identity;
    QVector<uint32_t> candidates;
    uint32_t chosenOffset;
    bool scanned;
    bool confirmed;
};

#endif // OFFSETSCANNER_H
