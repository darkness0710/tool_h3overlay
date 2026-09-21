#include "processhandler.h"

#include <windows.h>
#include <tlhelp32.h>
#include <wchar.h>
#include <chrono>
#include <thread>

#include <QDebug>


#define HOTA_PROCESS_NAME L"h3hota.exe"
#define HOTA_HD_PROCESS_NAME L"h3hota HD.exe"


// Same global is used for two different things: the status struct is stored
// directly in it, and the player section is reached by following it.
// Verified still referenced by HotA 1.8.1 (h3hota.exe 240 refs, HotA.dll 244).
#define EXE_TO_UNKNOWN_OFFSET 0x2992B8

#define HOTA_EXE_TO_PLAYER_SECTION 0x002992B8
#define HOTA_EXE_TO_PLAYER_SECTION_OFFSET1 0x5C
#define HOTA_EXE_TO_PLAYER_SECTION_OFFSET2 0xF60

#define POINTER_CASTING(addr) reinterpret_cast<LPCVOID>(static_cast<intptr_t>(addr))

ProcessHandler::ProcessHandler()
{
    reset();
}

bool ProcessHandler::init()
{
    reset();
    // Extra permissions is required for this process to be
    // allowed to attach to HoMM HotA process
    bool gotExtraPermission = askForExtraPermissions();
    if (gotExtraPermission == false)
    {
        qWarning() << "Could not get extra permissions. Can't attach to HotA";
    }

    this->attachToProcess();
    return gotExtraPermission;
}

void ProcessHandler::reset()
{
    if(this->processInfo.handle)
    {
        qInfo() << "Closing process handle.";
        CloseHandle(this->processInfo.handle);
    }
    memset(&this->processInfo, 0, sizeof(ProcessInfoStruct));
}

bool ProcessHandler::askForExtraPermissions()
{
    qDebug() << "Asking for elevated permissions";
    bool bRtn = false;
    TOKEN_PRIVILEGES tp;
    HANDLE tokenhandle = nullptr;
    HANDLE ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, false, GetCurrentProcessId());
    if (!ProcessHandle)
    {
        DWORD lastError = GetLastError();
        qWarning() << "Could not open own process to set permissions. Error code: " << lastError;
        return false;
    }

    bRtn = OpenProcessToken(ProcessHandle, TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &tokenhandle);
    if (bRtn == 0)
    {
        DWORD lastError = GetLastError();
        qWarning() << "Could not open process tokens. Error code: " << lastError;
        return false;
    }

    bRtn = LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &(tp.Privileges[0].Luid));
    if (bRtn)
    {
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        tp.PrivilegeCount = 1;
        bRtn = AdjustTokenPrivileges(tokenhandle, false, &tp, sizeof(tp), nullptr, nullptr);
        if(!bRtn)
        {
            DWORD lastError = GetLastError();
            qWarning() << "Could not adjust privilages for this process. Error code: " << lastError;
            return false;
        }
    }
    else
    {
        DWORD lastError = GetLastError();
        qWarning() << "Could not look up privilage for this process. Error code: " << lastError;
        return false;
    }
    return true;
}

DWORD ProcessHandler::GetHommPid()
{
    if(isGameStillRunning())
    {
        return this->processInfo.pid;
    }

    qDebug() << "Searching for HoMM PID.";

    this->exeName = nullptr;

    DWORD pid = 0;
    PROCESSENTRY32 pe32;
    HANDLE hSnapshot = nullptr;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if(Process32First( hSnapshot, &pe32))
    {
        do
        {
            if(wcscmp( pe32.szExeFile, HOTA_PROCESS_NAME) == 0)
            {
                this->exeName = HOTA_PROCESS_NAME;
                pid = pe32.th32ProcessID;
                break;
            }
            else if(wcscmp( pe32.szExeFile, HOTA_HD_PROCESS_NAME) == 0)
            {
                this->exeName = HOTA_HD_PROCESS_NAME;
                pid = pe32.th32ProcessID;
                break;
            }

        } while(Process32Next(hSnapshot, &pe32));
    }

    if(hSnapshot != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hSnapshot);
    }

    return pid;
}

bool ProcessHandler::attachToProcess()
{
    if(isGameStillRunning())
    {
        if(this->processInfo.exeBaseAddress == 0 ||
                this->processInfo.statusStruktAddress == 0 ||
                this->processInfo.playerSectionAddress == 0)
        {
            updateGameAddresses();
        }
        qDebug() << "Process is still active.";
        return true;
    }

    if(this->processInfo.handle)
    {
        reset();
    }

    this->processInfo.pid = GetHommPid();
    if (this->processInfo.pid == 0)
    {
        // If no pid was found, clearing buffers and states
        qDebug() << "No HoMM process found...";
        reset();

        return false;
    }

    if(!this->processInfo.handle)
    {
        qDebug() << "Trying to attatch to HoMM...";
        this->processInfo.handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                                               false,
                                               this->processInfo.pid);
        if (!this->processInfo.handle)
        {
            DWORD errorCode = GetLastError();
            qWarning() << "Failed to attach to process, error code: " << errorCode;
            return false;
        }
    }

    qInfo() << "Attached to H3 pid:" << this->processInfo.pid;
    qInfo() << "Process name:" << QString::fromWCharArray(this->exeName);

    return updateGameAddresses();
}

MODULEENTRY32W ProcessHandler::getModuleEntry(const wchar_t * moduleName)
{

    MODULEENTRY32W module;
    memset(&module, 0, sizeof(MODULEENTRY32W));
    if(this->processInfo.pid == 0 || moduleName == nullptr)
    {
        return module;
    }

    qDebug() << "Gettings module:" << QString::fromWCharArray(moduleName);
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, this->processInfo.pid);
    if (hSnapshot != INVALID_HANDLE_VALUE)
    {
        MODULEENTRY32 ModuleEntry32;
        ModuleEntry32.dwSize = sizeof(MODULEENTRY32);
        if (Module32First(hSnapshot, &ModuleEntry32))
        {
            do
            {
                if (_wcsicmp(ModuleEntry32.szModule, moduleName) == 0)
                {
                    return ModuleEntry32;
                }
            } while (Module32Next(hSnapshot, &ModuleEntry32));
        }
        CloseHandle(hSnapshot);
    }
    return module;
}

uintptr_t ProcessHandler::getModuleBaseAddress(const wchar_t * moduleName)
{
    if(this->processInfo.pid == 0 || moduleName == nullptr)
    {
        return 0;
    }
    qDebug() << "Gettings module offset address for:" << QString::fromWCharArray(moduleName);
    return reinterpret_cast<uintptr_t>(getModuleEntry(moduleName).modBaseAddr);
}


DWORD ProcessHandler::getModuleSize(const wchar_t * moduleName)
{
    if(this->processInfo.pid == 0 || moduleName == nullptr)
    {
        return 0;
    }
    qDebug() << "Gettings module size for:" << QString::fromWCharArray(moduleName);
    return reinterpret_cast<DWORD>(getModuleEntry(moduleName).modBaseSize);
}


bool ProcessHandler::isGameStillRunning()
{
    DWORD returnCode;
    WINBOOL success = GetExitCodeProcess(this->processInfo.handle, &returnCode);
    bool isRunning = success && returnCode == STILL_ACTIVE && this->processInfo.pid && this->processInfo.handle;
    if(isRunning == false)
    {
        reset();
    }

    return isRunning;
}


bool ProcessHandler::updateGameAddresses()
{
    this->processInfo.exeBaseAddress = getModuleBaseAddress(this->exeName);

    if(this->processInfo.exeBaseAddress == 0)
    {
        qWarning() << "Could not get base game address.";
        return false;
    }
    this->processInfo.hdDLLBaseAddress = getModuleBaseAddress(L"HD_HOTA.dll");
    this->processInfo.dllBaseAddress = getModuleBaseAddress(L"hota.dll");

    // Gets a pointer which points to an unmapped struct, but it contains
    // values which are useful
    this->processInfo.statusStruktAddress = followPointer(this->processInfo.exeBaseAddress + EXE_TO_UNKNOWN_OFFSET);
    if(this->processInfo.statusStruktAddress == 0)
    {
        qWarning() << "Could not get status struct address.";
        return false;
    }


    uint32_t step1 = followPointer(this->processInfo.exeBaseAddress + HOTA_EXE_TO_PLAYER_SECTION);
    uint32_t step2 = followPointer(step1 + HOTA_EXE_TO_PLAYER_SECTION_OFFSET1) ;
    this->processInfo.playerSectionAddress = step2 + HOTA_EXE_TO_PLAYER_SECTION_OFFSET2;

    if(this->processInfo.playerSectionAddress == 0)
    {
        qWarning() << "Could not get player section struct address.";
        return false;
    }
    qInfo() << "Game addresses updated.";
    return true;
}


uintptr_t ProcessHandler::followPointer(const uintptr_t address)
{
    uintptr_t pointerValue = 0;
    size_t bytesRead = 0;
    if(!ReadProcessMemory(this->processInfo.handle,
                           reinterpret_cast<LPCVOID>(address),
                           &pointerValue,
                           sizeof(uint32_t),
                           &bytesRead))
    {
        DWORD lastError = GetLastError();
        if(lastError != 299)
            qWarning() << "Could not read pointer at " << address <<  " . Error code: " << lastError;
        return 0;
    }
    return pointerValue;
}
