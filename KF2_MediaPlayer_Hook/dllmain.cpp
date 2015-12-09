// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

#if defined _M_X64
#pragma comment(lib, "libMinHook.x64.lib")
#elif defined _M_IX86
#pragma comment(lib, "libMinHook.x86.lib")
#endif

#define BUFSIZE MAX_PATH

using namespace std;

typedef HANDLE(WINAPI *FP_CreateFile)(
	LPCTSTR               lpFileName,
	DWORD                 dwDesiredAccess,
	DWORD                 dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD                 dwCreationDisposition,
	DWORD                 dwFlagsAndAttributes,
	HANDLE                hTemplateFile);

enum class MusicType
{
	Ambient,
	Action
};

unordered_map<string, MusicType> musicFileMap;

FP_CreateFile fpCreateFile;

bool isPlayingMusic = false;

void firePlayPauseKey()
{
	INPUT ip;
	ip.type = INPUT_KEYBOARD;
	ip.ki.wScan = 0;
	ip.ki.time = 0;
	ip.ki.dwExtraInfo = 0;

	ip.ki.wVk = VK_MEDIA_PLAY_PAUSE;
	ip.ki.dwFlags = 0;
	SendInput(1, &ip, sizeof(INPUT));

	ip.ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(1, &ip, sizeof(INPUT));
}

HANDLE WINAPI CreateFile_Hook(
	_In_     LPCTSTR               lpFileName,
	_In_     DWORD                 dwDesiredAccess,
	_In_     DWORD                 dwShareMode,
	_In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	_In_     DWORD                 dwCreationDisposition,
	_In_     DWORD                 dwFlagsAndAttributes,
	_In_opt_ HANDLE                hTemplateFile
	)
{
	if (dwDesiredAccess == GENERIC_READ)
	{
		TCHAR filename_w[BUFSIZE];
		TCHAR ext_w[BUFSIZE];
		if (_wsplitpath_s(lpFileName, nullptr, 0, nullptr, 0, filename_w, BUFSIZE, ext_w, BUFSIZE) == 0)
		{
			if (wcscat_s(filename_w, BUFSIZE, ext_w) != 0)
			{
				MessageBox(nullptr, L"Fail to join filename with extension.", L"KF2_MediaPlayer", MB_OK);
			}

			char filename[BUFSIZE];
			size_t numChars;
			wcstombs_s(&numChars, filename, filename_w, BUFSIZE - 1);

			// Check if file is one we know.
			const auto it = musicFileMap.find(filename);
			if (it != musicFileMap.end())
			{
				if (!isPlayingMusic && it->second == MusicType::Action)
				{
					firePlayPauseKey();
					isPlayingMusic = true;
				}
				else if (isPlayingMusic && it->second == MusicType::Ambient)
				{
					firePlayPauseKey();
					isPlayingMusic = false;
				}
			}
		}
	}

	return fpCreateFile(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

bool hasEnding(std::string const &fullString, std::string const &ending)
{
	if (fullString.length() >= ending.length())
		return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
	else
		return false;
}

vector<string> findWEMFile(const char* filename)
{
	vector<string> files;

	ifstream f(filename);
	if (!f.is_open())
		return files;

	string line;
	while (getline(f, line))
	{
		istringstream iss(line);
		vector<string> tokens{ istream_iterator<string>{iss}, istream_iterator<string>{} };
		for (const auto &token : tokens)
		{
			if (token.find_first_of('\\') == string::npos && hasEnding(token, ".wem"))
			{
				files.emplace_back(token);
			}
		}
	}

	return files;
}

void CreateMusicFileHashTable()
{
#ifdef _DEBUG
	const char* musicActionFileName = "D:\\SteamLibrary\\steamapps\\common\\killingfloor2\\KFGame\\BrewedPC\\WwiseAudio\\Windows\\WwiseDefaultBank_WW_MACT_Default.txt";
	const char* musicAmbientFilesName = "D:\\SteamLibrary\\steamapps\\common\\killingfloor2\\KFGame\\BrewedPC\\WwiseAudio\\Windows\\WwiseDefaultBank_WW_MAMB_Default.txt";
#else
	const char* musicActionFileName = "..\\..\\KFGame\\BrewedPC\\WwiseAudio\\Windows\\WwiseDefaultBank_WW_MACT_Default.txt";
	const char* musicAmbientFilesName = "..\\..\\KFGame\\BrewedPC\\WwiseAudio\\Windows\\WwiseDefaultBank_WW_MAMB_Default.txt";
#endif

	const auto musicActionFiles = findWEMFile(musicActionFileName);
	const auto musicAmbientFiles = findWEMFile(musicAmbientFilesName);

	musicFileMap.reserve(musicActionFiles.size() + musicAmbientFiles.size());
	for (const auto &f : musicActionFiles)
	{
		musicFileMap[f] = MusicType::Action;
	}

	for (const auto &f : musicAmbientFiles)
	{
		musicFileMap[f] = MusicType::Ambient;
	}
}

bool HookReadFile()
{
	// Initialize MinHook.
	if (MH_Initialize() != MH_OK)
	{
		return false;
	}

	HMODULE hModule = GetModuleHandleA("Kernel32.dll");
	if (hModule == nullptr)
	{
		MessageBox(nullptr, L"Fail to get module handle!", L"KF2_MediaPlayer", MB_OK);
		return false;
	}

	LPVOID pTarget = GetProcAddress(hModule, "CreateFileW");
	if (pTarget == nullptr)
	{
		MessageBox(nullptr, L"Fail to get function address!", L"KF2_MediaPlayer", MB_OK);
		return false;
	}

	if (MH_CreateHook(pTarget, &CreateFile_Hook, reinterpret_cast<void**>(&fpCreateFile)) != MH_OK)
	{
		MessageBox(nullptr, L"Fail to create hook!", L"KF2_MediaPlayer", MB_OK);
		return false;
	}

	if (MH_EnableHook(pTarget) != MH_OK)
	{
		MessageBox(nullptr, L"Fail to enable hook!", L"KF2_MediaPlayer", MB_OK);
		return false;
	}

	return true;
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
	)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		CreateMusicFileHashTable();
		HookReadFile();
		break;

	case DLL_PROCESS_DETACH:
	case DLL_THREAD_DETACH:
		if (isPlayingMusic)
			firePlayPauseKey();

		break;
	}
	return TRUE;
}

