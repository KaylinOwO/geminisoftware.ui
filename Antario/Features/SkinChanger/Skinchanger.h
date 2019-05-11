#pragma once
#include "../../Utils/GlobalVars.h"
#include "../../SDK/ISurface.h"
#include "../../Utils/Math.h"
#include "../../SDK/IVModelInfo.h"
#include "../../SDK/IClientMode.h"
#include "../../SDK/CGlobalVarsBase.h"
#include "../../SDK/IBaseClientDll.h"
#include "../../Menu/config.h"
#include <deque>

class Skinchanger
{
public:
	int getSkin(int shit_to_switch);
	int getKnifeItemDefinitionIndex();
	const char* getKnifeModel(bool viewmodel = true);
	void Run();
};

extern Skinchanger gSkins;