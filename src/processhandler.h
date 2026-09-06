#ifndef PROCESSHANDLER_H
#define PROCESSHANDLER_H

#include <QObject>
#include <wchar.h>

#include "gamestructs.h"
#include <tlhelp32.h>

class ProcessHandler
{
public:
    ProcessHandler();

    /**
     * @brief init Setup for the memory scanner, will start trying to attatch
     * the the game process
     * @return true if privilages permissions are granted which is required to
     * be allowed to attach to the game
     */
    bool init();

    /**
     * @brief reset Clears the internal state and detaches fromthe game
     */
    void reset();

    /**
     * @brief attachToProcess Tries to attach to the the HoMM process.
     * @return True if successful or already attached, otherwise returns false.
     */
    bool attachToProcess();

    /**
     * @brief getPointerValue Reads a memory location in the HoMMm and assumes
     * it is a 32 bit pointer and returns the value of what it is pointing to.
     * @param address The address to read the pointer value from.
     * @return The value of what the pointer is pointing to.
     */
    uintptr_t followPointer(const uintptr_t address);

    /**
     * @brief isGameStillRunning Checks if the game we might have attached to
     * is still running.
     * @return true if we are attached to the game and it is still running.
     */
    bool isGameStillRunning();

    /**
     * @brief findMapAddress Finds and stores the memory location for the
     * descirption of the map. This can only be found when a match is active.
     */
    void findMapAddress();

    /**
     * @brief getModuleEntry Fetches the module entry which contains meta data
     * about a given module, like baseaddress and size
     * @param moduleName The module name to fetch
     * @return The module object if it is found, otherwise returns a module
     * object with all values set to 0.
     */
    MODULEENTRY32W getModuleEntry(const wchar_t *moduleName);

    ProcessInfoStruct processInfo;
private:

    /**
     * @brief askForExtraPermissions Asks Windows for extra permissions. Required
     * to be allowed to attach to HoMM HotA.
     * @return True if successful, otherwise returns false.
     */
    bool askForExtraPermissions();


    /**
     * @brief GetHommPid Searches trough all processes on the computer to find
     * the pid of h3hota.exe, h3hota HD.exe, Heroes3.exe or Heroes3 HD.exe.
     * If the game is already tracked, it will just return the known pid.
     * @return The pid of the HoMM3 PID.
     */
    DWORD GetHommPid();

    /**
     * @brief GetModuleBaseAddress Gets the start address of a given module
     * (.dll/.exe)
     * @param moduleName The full file name of the module
     * @return The memory address of the module, if not found, will return NULL.
     */
    uintptr_t getModuleBaseAddress(const wchar_t *moduleName);

    /**
     * @brief getModuleSize Gets the size of a module in bytes.
     * @param moduleName The full file name of the module
     * @return The number of bytes of the module, returns 0 if the module is
     * not found
     */
    DWORD getModuleSize(const wchar_t *moduleName);

    wchar_t const *exeName;

private slots:
    /**
     * @brief updateGameAddresses Figures out a few game addresses which are
     * static for each instance of the game.
     * @return True if all required memory locations could be found. Returns
     * false otherwise.
     */
    bool updateGameAddresses();
};

#endif // PROCESSHANDLER_H
