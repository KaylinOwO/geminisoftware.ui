#pragma once
#include "..\..\Utils\GlobalVars.h"
#include "..\..\SDK\singleton.h"
#include "..\..\SDK\IEngineTrace.h"
#include "..\Aimbot\Autowall.h"


class c_visuals : public singleton< c_visuals > {
private:

public:
	void CollectFootstep(int iEntIndex, const char * pSample);
	void run();
	void DrawSpreadCircle();
	void run_viewmodel();
	void spectators();
	void force_crosshair();
	void flags(C_BaseEntity * player);
	void BoxESP(C_BaseEntity * Entity);
	void NameESP(C_BaseEntity * Entity);

	void PlayerIndicator(C_BaseEntity * entity);
	void WeaponESP(C_BaseEntity * Entity);
	void Ammo(C_BaseEntity * Entity);
	void HealthESP(C_BaseEntity * Entity);
	void DrawPlayers();
	void draw_scope();
	void DrawRadarPoint(Vector vOriginX, Vector vOriginY, QAngle qAngle, C_BaseEntity * pBaseEntity, Color col);
	void DrawRadarBackground();
	void DrawRadar(C_BaseEntity * pEntity);
	void worldWeapon(C_BaseEntity * pEntity);
	void spectator_list(C_BaseEntity * entity, C_BaseEntity * local_player, Color col);
	void DrawRadar();
	void run_aspect_ratio_changer();
	void draw_watermark();
	void do_nightmode();
	void do_fullbright();
};

