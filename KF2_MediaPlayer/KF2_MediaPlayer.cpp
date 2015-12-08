// KF2_MediaPlayer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

using namespace std;

#ifdef _DEBUG
const wchar_t* dllName = L"C:\\Users\\Pedro\\Documents\\Projects_Sync\\KF2_MediaPlayer\\x64\\Debug\\KF2_MediaPlayer_Hook.dll";	// Change for you environment.
const wchar_t* gameExe = L"D:\\SteamLibrary\\steamapps\\common\\killingfloor2\\Binaries\\Win64\\KFGame.exe";	// Change for you environment.
#else
const wchar_t* dllName = L"KF2_MediaPlayer_Hook.dll";
const wchar_t* gameExe = L"KFGame.exe";
#endif

vector<HANDLE> find_game_pid(const std::wstring &name, const int tries = -1)
{
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	vector<HANDLE> processId;

	int triesCount = 0;
	for (;;)
	{
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

		if (Process32First(snapshot, &entry) == TRUE)
		{
			while (Process32Next(snapshot, &entry) == TRUE)
			{
				if (wcscmp(entry.szExeFile, name.c_str()) == 0)
				{
					HANDLE remoteProc = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
					if (remoteProc)
						processId.push_back(remoteProc);

					continue;
				}
			}
		}

		CloseHandle(snapshot);

		if (!processId.empty())
			break;

		triesCount++;
		if (tries >= triesCount)
			break;

		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	return processId;
}

bool inject_dll(vector<HANDLE> processesId)
{
	bool wasSuccess = false;

	const size_t dllNameSize = (wcslen(dllName) + 1) * sizeof(wchar_t);

	for (auto remoteProc : processesId)
	{
		LPVOID loadLibAddress = (LPVOID) GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW");
		LPVOID memAlloc = (LPVOID) VirtualAllocEx(remoteProc, nullptr, dllNameSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

		if (!WriteProcessMemory(remoteProc, (LPVOID) memAlloc, dllName, dllNameSize, nullptr))
			continue;

		if (CreateRemoteThread(remoteProc, nullptr, 0, (LPTHREAD_START_ROUTINE) loadLibAddress, (LPVOID) memAlloc, 0, nullptr) == nullptr)
			continue;

		wasSuccess = true;

		if (!VirtualFreeEx(remoteProc, (LPVOID) memAlloc, 0, MEM_RELEASE | MEM_DECOMMIT))
			continue;

		if (!CloseHandle(remoteProc))
			continue;
	}

	return wasSuccess;
}

int main()
{
	std::wstring exeToHook = L"KFGame.exe";

	// Check if the game is already running.
	vector<HANDLE> gamePid = find_game_pid(exeToHook, 3);
	if (gamePid.empty())
	{
		// Launch game.
		ShellExecute(nullptr, L"open", gameExe, nullptr, nullptr, SW_SHOWDEFAULT);
	}

	std::wcout << "Waiting for " << exeToHook.c_str() << " to start..." << std::endl;

	gamePid = find_game_pid(exeToHook);
	if (gamePid.empty())
	{	
		std::wcout << "Failed to find game [" << exeToHook.c_str() << "]..." << std::endl;
		std::cin.get();
	}
	else
	{
		if (inject_dll(gamePid))
			std::cout << "Injection completed... Have fun!" << std::endl;
		else
		{	
			std::cout << "Failed to inject DLL!" << std::endl;
			std::cin.get();
		}
	}

	return 0;
}
