#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <memory> // For smart pointers
#include "Resource.h"

// Centralized logging function
void LogError(const std::string& message) {
    std::cerr << message << " Error: " << GetLastError() << std::endl;
}

void ShowSpinner(const char* message, bool& stopFlag) {
    const char spinnerChars[] = "|/-\\";
    int spinnerIndex = 0;

    while (!stopFlag) {
        std::cout << "\r" << message << " " << spinnerChars[spinnerIndex] << " ";
        std::cout.flush();
        spinnerIndex = (spinnerIndex + 1) % 4;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Hapus spinner setelah selesai
    std::cout << "\r" << std::string(strlen(message) + 4, ' ') << "\r";
}

// RAII wrapper for HANDLE
class HandleWrapper {
public:
    explicit HandleWrapper(HANDLE handle) : handle_(handle) {}
    ~HandleWrapper() {
        if (handle_ && handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
    }
    HANDLE get() const { return handle_; }
private:
    HANDLE handle_;
};

// Extract embedded DLL from resources
bool ExtractEmbeddedDLL(const char* outputPath) {
    HRSRC hResource = FindResource(nullptr, MAKEINTRESOURCE(IDR_DLL1), RT_RCDATA);
    if (!hResource) {
        LogError("Failed to find embedded DLL resource.");
        return false;
    }

    HGLOBAL hLoadedResource = LoadResource(nullptr, hResource);
    if (!hLoadedResource) {
        LogError("Failed to load embedded DLL resource.");
        return false;
    }

    void* pResourceData = LockResource(hLoadedResource);
    if (!pResourceData) {
        LogError("Failed to lock embedded DLL resource.");
        return false;
    }

    DWORD resourceSize = SizeofResource(nullptr, hResource);
    if (resourceSize == 0) {
        LogError("Failed to get size of embedded DLL resource.");
        return false;
    }

    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile) {
        LogError("Failed to create temporary file for DLL.");
        return false;
    }

    outFile.write(reinterpret_cast<const char*>(pResourceData), resourceSize);
    return true;
}

// Inject DLL into target process
bool InjectDLL(DWORD processID, const char* dllPath) {
    HandleWrapper hProcess(OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID));
    if (!hProcess.get()) {
        LogError("Failed to open process.");
        return false;
    }

    size_t dllPathLen = strlen(dllPath) + 1;
    LPVOID pRemoteMemory = VirtualAllocEx(hProcess.get(), nullptr, dllPathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMemory) {
        LogError("Failed to allocate memory in target process.");
        return false;
    }

    if (!WriteProcessMemory(hProcess.get(), pRemoteMemory, dllPath, dllPathLen, nullptr)) {
        LogError("Failed to write to process memory.");
        VirtualFreeEx(hProcess.get(), pRemoteMemory, 0, MEM_RELEASE);
        return false;
    }

    LPVOID pLoadLibrary = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (!pLoadLibrary) {
        LogError("Failed to get address of LoadLibraryA.");
        VirtualFreeEx(hProcess.get(), pRemoteMemory, 0, MEM_RELEASE);
        return false;
    }

    HandleWrapper hThread(CreateRemoteThread(hProcess.get(), nullptr, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemoteMemory, 0, nullptr));
    if (!hThread.get()) {
        LogError("Failed to create remote thread.");
        VirtualFreeEx(hProcess.get(), pRemoteMemory, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(hThread.get(), INFINITE);
    VirtualFreeEx(hProcess.get(), pRemoteMemory, 0, MEM_RELEASE);
    return true;
}

// Find process ID by name
DWORD FindProcessID(const char* processName) {
    HandleWrapper hSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (hSnapshot.get() == INVALID_HANDLE_VALUE) {
        LogError("Failed to create process snapshot.");
        return 0;
    }

    PROCESSENTRY32W processEntry = { sizeof(PROCESSENTRY32W) };
    if (Process32FirstW(hSnapshot.get(), &processEntry)) {
        do {
            std::wstring exeFileName = processEntry.szExeFile;
            if (_wcsicmp(exeFileName.c_str(), std::wstring(processName, processName + strlen(processName)).c_str()) == 0) {
                return processEntry.th32ProcessID;
            }
        } while (Process32NextW(hSnapshot.get(), &processEntry));
    }

    return 0;
}

// Fungsi untuk menampilkan ASCII art
void DisplayASCIIArt() {
    std::cout << R"( ___   ________    ___   _________   ___   ________   ___                       ________     
|\  \ |\   ___  \ |\  \ |\___   ___\|\  \ |\   __  \ |\  \                     |\   __  \    
\ \  \\ \  \\ \  \\ \  \\|___ \  \_|\ \  \\ \  \|\  \\ \  \       ____________ \ \  \|\ /_   
 \ \  \\ \  \\ \  \\ \  \    \ \  \  \ \  \\ \   __  \\ \  \     |\____________\\ \   __  \  
  \ \  \\ \  \\ \  \\ \  \    \ \  \  \ \  \\ \  \ \  \\ \  \____\|____________| \ \  \|\  \ 
   \ \__\\ \__\\ \__\\ \__\    \ \__\  \ \__\\ \__\ \__\\ \_______\               \ \_______\
    \|__| \|__| \|__| \|__|     \|__|   \|__| \|__|\|__| \|_______|                \|_______|)" << std::endl;
    std::cout << "----------------------------------------------------------------------------------- YimMenuV2" << std::endl;
}

// Fungsi untuk mengatur warna teks
void SetConsoleColor(WORD color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

void SetConsoleIcon(HINSTANCE hInstance) {
    HWND consoleWindow = GetConsoleWindow();
    if (consoleWindow) {
        HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CUSTOM_ICON));
        if (hIcon) {
            SendMessage(consoleWindow, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            SendMessage(consoleWindow, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        }
    }
}

void SetCursorPosition(int x, int y) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD position = { static_cast<SHORT>(x), static_cast<SHORT>(y) };
    SetConsoleCursorPosition(hConsole, position);
}

bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;

    // Alokasikan SID untuk grup administrator
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(
        &ntAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &adminGroup)) {
        // Periksa apakah token proses saat ini adalah bagian dari grup administrator
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin;
}

void RelaunchAsAdmin() {
    if (!IsRunningAsAdmin()) {
        // Get the path to the current executable
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);

        // Set up the structure for ShellExecute
        SHELLEXECUTEINFOA sei = { sizeof(SHELLEXECUTEINFOA) };
        sei.lpVerb = "runas"; // Request administrator privileges
        sei.lpFile = exePath; // Path to executable
        sei.nShow = SW_SHOWNORMAL;

        // Re-run the program with administrator privileges
        if (!ShellExecuteExA(&sei)) {
            std::cerr << "Failed to relaunch as administrator. Error: " << GetLastError() << std::endl;
        }

        // Exit the current process
        ExitProcess(0);
    }
}

int main() {
    RelaunchAsAdmin();
    AllocConsole();
    SetConsoleTitle(L"YimMenuV2");
    SetConsoleIcon(GetModuleHandle(nullptr));

    // Display ASCII art with color
    SetConsoleColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    DisplayASCIIArt();
    SetConsoleColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // Reset warna

    // Loading simulation
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    int cursorX = csbi.dwCursorPosition.X;
    int cursorY = csbi.dwCursorPosition.Y;

    std::cout << "Initializing";
    for (int i = 0; i < 3; ++i) {
        std::cout << ".";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Move the cursor to the starting position of “Initializing” and delete the text
    SetCursorPosition(cursorX, cursorY);
    std::cout << std::string(50, ' '); // Delete lines with blank spaces
    SetCursorPosition(cursorX, cursorY); // Return the cursor to the starting position

    // Proceed to the main logic of the program
    const char* targetProcess = "GTA5_Enhanced.exe";
    // Buffer to store folder path %TEMP%
    char tempPath[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempPath) == 0) {
        LogError("Failed to get TEMP folder path.");
        std::cin.get();
        return 1;
    }

    // Add the DLL filename to the path %TEMP%
    std::string tempDllPath = std::string(tempPath) + "YimMenuV2.dll";

    DWORD processID = 0;

    if (!ExtractEmbeddedDLL(tempDllPath.c_str())) {
        LogError("Failed to extract embedded DLL.");
        std::cin.get();
        return 1;
    }

    // Spinner animation
    bool stopSpinner = false;
    std::thread spinnerThread(ShowSpinner, "Waiting GTA5_Enhanced.exe to start", std::ref(stopSpinner));

	// Wait for the process to start
    while ((processID = FindProcessID(targetProcess)) == 0) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

	// Stop spinner animation
    stopSpinner = true;
    spinnerThread.join();

    std::cout << "GTA 5 Enhanced Loaded! Press Enter to start inject..." << std::endl;

	// Wait for user input before proceeding
    std::cin.get();

    if (InjectDLL(processID, tempDllPath.c_str())) {
        std::cout << "DLL injection successfully." << std::endl;
    }
    else {
		std::cout << "DLL injection failed!\nPress Enter to exit." << std::endl;
		std::cin.get();
    }

    return 0;
}
