#include "Legit Aimbot.h"
#include "../../Menu/Menu.h"
#include "../../Menu/config.h"
float m_fbestfov = 20.0f;
int m_ibesttargetlegit = -1;

float deltaTime;
float curAimTime;

LegitAimbot g_LegitAimbot;

void LegitAimbot::Aim_CreateMove(CUserCmd * cmd)
{

	DropTarget();

	static float oldServerTime = Globals::LocalPlayer->GetTickBase() * g_pGlobalVars->intervalPerTick;
	float serverTime = Globals::LocalPlayer->GetTickBase() * g_pGlobalVars->intervalPerTick;
	deltaTime = serverTime - oldServerTime;
	oldServerTime = serverTime;

	if (!Globals::LocalPlayer->GetActiveWeapon())
		return;

	if (Globals::LocalPlayer->GetActiveWeapon()->IsKnifeorNade() || Globals::LocalPlayer->GetActiveWeapon()->GetAmmo() == 0)
		return;

	if ((!(Globals::pCmd->buttons & IN_ATTACK) || m_ibesttargetlegit == -1) && c_config::get().legit_aimbot_enabled)
		FindTarget();


	if (m_ibesttargetlegit != -1 && GetAsyncKeyState(VK_LBUTTON))
		GoToTarget();

	if (c_config::get().rcs && c_config::get().legit_aimbot_enabled)
		RCS();
}

void LegitAimbot::FindTarget()
{
	m_fbestfov = c_config::get().fov;

	for (int i = 0; i <= g_pGlobalVars->maxClients; i++)
	{
		auto Entity = g_pEntityList->GetClientEntity(i);

		if (!Entity->is_valid(Globals::LocalPlayer))
			continue;

		Vector viewangles = Globals::pCmd->viewangles + Globals::LocalPlayer->GetAimPunchAngle() * 2.f;

		float fov = g_Math.GetFov(Globals::pCmd->viewangles,
		                   g_Math.CalcAngle(Globals::LocalPlayer->GetEyePosition(),
		                            Entity->GetBonePosition(8)));
		if (fov < m_fbestfov)
		{
			m_fbestfov = fov;
			m_ibesttargetlegit = i;
		}
	}
}

void LegitAimbot::GoToTarget()
{
	auto Entity = g_pEntityList->GetClientEntity(m_ibesttargetlegit);

	if (!Entity)
		return;

	Vector predicted = Entity->GetPredicted(Entity->GetBonePosition(8));

	Vector dst = g_Math.CalcAngle(Globals::LocalPlayer->GetEyePosition(), predicted);
	Vector src = Globals::pCmd->viewangles;

	dst.x -= Globals::LocalPlayer->GetAimPunchAngle().x * (c_config::get().rcs_x / 50.f);
	dst.y -= Globals::LocalPlayer->GetAimPunchAngle().y * (c_config::get().rcs_y / 50.f);

	Vector delta = dst - src;

	delta.Clamp();

	if (!delta.IsZero())
	{
		float finalTime = delta.Length() / (c_config::get().linear_progression_threshold / 5);

		curAimTime += deltaTime;

		if (curAimTime > finalTime)
			curAimTime = finalTime;

		float percent = curAimTime / finalTime;

		delta *= percent;
		dst = src + delta;
	}

	g_pEngine->SetViewAngles(dst.Clamp());
}

void LegitAimbot::DropTarget()
{
	if (!EntityIsValid(m_ibesttargetlegit))
	{
		m_ibesttargetlegit = -1;
		curAimTime = 0.f;
	}
}

bool LegitAimbot::EntityIsValid(int i)
{
	auto Entity = g_pEntityList->GetClientEntity(i);

	if (!Entity->is_valid())
		return false;
	
	if (g_Math.GetFov(Globals::pCmd->viewangles, g_Math.CalcAngle(Globals::LocalPlayer->GetEyePosition(), Entity->GetBonePosition(c_config::get().legit_aimbot_priority_hitbox))) > c_config::get().legit_aimbot_fov)
		return false;

	return true;
}

Vector m_oldangle;

void LegitAimbot::RCS()
{
	auto weap = Globals::LocalPlayer->GetActiveWeapon();
	if (weap->IsPistol() || weap->IsSniper() || weap->IsShotgun())
		return;

	if (Globals::pCmd->buttons & IN_ATTACK)
	{
		Vector aimpunch = Globals::LocalPlayer->GetAimPunchAngle() * 2.f;

		Globals::pCmd->viewangles += (m_oldangle - aimpunch);

		m_oldangle = aimpunch;
	}
	else
	{
		m_oldangle.x = m_oldangle.y = m_oldangle.z = 0;
	}
}