// Music Room BGM Library
// ----------------------
// bgmlib.cpp - BGM Library main class
// ----------------------
// "©" Nmlgc, 2011

#include "platform.h"
#include "list.h"
#include "config.h"
#include "infostruct.h"
#include "bgmlib.h"
#include "ui.h"
#include "packmethod.h"
#include "utils.h"

#include <FXPath.h>
#include <FXIO.h>
#include <FXDir.h>
#include <FXSystem.h>

ushort Lang;	// Current language

namespace BGMLib
{
	// String constants
	// ----------------
	const FXString Trial[LANG_COUNT] = {L" 体験版", " (Trial)", L" (체험판)"};
	const FXString GNDelim[2] = {L"「",  L"」"};
	const FXString WriteError = L"쓸 수 없습니다: ";
	// ----------------

	// Values read from a config file
	// -----
	// [bgmlib]
	FXString InfoPath;	// BGM info file directory
	// -----

	List<PackMethod*>	PM;	// Supported pack methods
	List<GameInfo>	Game;	// Supported games
}

bool BGMLib::Init(ConfigFile *Cfg, FXString CfgPath, bool DefaultPM)
{
	ConfigParser* Lib;
	
	if(DefaultPM)
	{
		// Invoke PM_None
		PM_None& Invoke = PM_None::Inst();
	}

	SetupLang();

	if(!Cfg)	return false;

	if(Lib = Cfg->FindSection("bgmlib"))
	{
		Lib->LinkValue("lang", TYPE_USHORT, &Lang);
		Lib->GetValue("infopath", TYPE_STRING, &InfoPath);
	}
	InfoPath = absolutePath(CfgPath, InfoPath);
	
	return true;
}

bool BGMLib::LoadBGMInfo()
{
	FXString* BGM;
	FXint	BGMCount;
	FXString PrevPath;
	GameInfo* New;

	if(InfoPath.empty())	return false;

	BGMCount = FXDir::listFiles(BGM, InfoPath, "*.bgm");

	if(BGMCount == 0)
	{
		UI_Error(InfoPath + L" 폴더에서 파일을 찾을 수 없습니다!\nBGM 파일을 넣은 후 프로그램을 재시작해주세요.\n");
		return false;
	}

	Game.Clear();

	PrevPath = FXSystem::getCurrentDirectory();
	FXSystem::setCurrentDirectory(InfoPath);

	UI_Stat(L"---------------------------\n지원 목록:\n");

	for(FXint c = 0; c < BGMCount; c++)
	{
		New = &(Game.Add()->Data);
		if(!New->ParseGameData(BGM[c]))	Game.PopLast();
		BGM[c].clear();
	}
	SAFE_DELETE_ARRAY(BGM);

	UI_Stat("---------------------------\n");

	FXSystem::setCurrentDirectory(PrevPath);

	return true;
}

GameInfo* BGMLib::ScanGame(const FXString& Path)
{
	GameInfo* NewGame = NULL;
	FXString Str;
	FXString PrevPath;

	PrevPath = FXSystem::getCurrentDirectory();

	FXSystem::setCurrentDirectory(Path);

	UI_Stat(L"" + Path + L"을(를) 탐색하는 중...\n");
	UI_Stat("------------------------\n");

	// Scan the pack methods in reverse order.
	// The latter ones tend to be more advanced.
	ListEntry<PackMethod*>* CurPM = PM.Last();

	while(CurPM)
	{
		if(NewGame = CurPM->Data->Scan(Path))	break;
		CurPM = CurPM->Prev();
	}

	if(NewGame != NULL)
	{
		Str = (NewGame->DelimName(Lang) + L"을(를) 확인함\n");
		UI_Stat(Str);
	}
	else
	{
		BGMLib::UI_Stat("사용 가능한 게임을 찾을 수 없습니다!\n");
	}

	BGMLib::UI_Stat("------------------------\n");

	FXSystem::setCurrentDirectory(PrevPath);

	return NewGame;
}

void BGMLib::Clear()
{
	for(ushort c = 0; c < LANG_COUNT; c++)
	{
		LI[c].Clear();
	}
	PM.Clear();
	Game.Clear();
}