#pragma once
#include "..\..\Utils\GlobalVars.h"
#include "..\..\SDK\CGlobalVarsBase.h"
#include "..\..\Utils/Math.h"
#include <deque>

class LegitAimbot
{
public:
	void Aim_CreateMove(CUserCmd* cmd);
	
private:
	void FindTarget();
	void GoToTarget();
	void DropTarget();
	bool EntityIsValid(int i);
	void RCS();
};
extern LegitAimbot g_LegitAimbot;