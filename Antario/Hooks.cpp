﻿#include <thread>
#include "Hooks.h"
#include "Utils\Utils.h"
#include "Features\Features.h"
#include "Menu\Menu.h"
#include "Features/Visuals/EventLogging.h"
#include "Features\SkinChanger\Skinchanger.h"
#include "ImGui/dx9/imgui_dx9.h"

Misc     g_Misc;
Hooks    g_Hooks;
Event    g_Event;
c_nade_prediction g_nadepred;

std::vector<trace_info> trace_logs;

void SinCos(float radian, float* sin, float* cos)
{
	*sin = std::sin(radian);
	*cos = std::cos(radian);
}
void angle_to_vector(const Vector &angles, Vector &forward) {
	float sp, sy, cp, cy;

	SinCos(DEG2RAD(angles.y), &sy, &cy);
	SinCos(DEG2RAD(angles.x), &sp, &cp);

	forward.x = cp * cy;
	forward.y = cp * sy;
	forward.z = -sp;
}
void c_nade_prediction::predict(CUserCmd *ucmd) {
	//	readability
	constexpr float restitution = 0.45f;
	constexpr float power[] = { 1.0f, 1.0f, 0.5f, 0.0f };
	constexpr float velocity = 403.0f * 0.9f;

	float step, gravity, new_velocity, unk01;
	int index{}, grenade_act{ 1 };
	Vector pos, thrown_direction, start, eye_origin;
	Vector angles, thrown;

	//	first time setup
	static auto sv_gravity = g_pCvar->FindVar("sv_gravity");

	//	calculate step and actual gravity value
	gravity = sv_gravity->GetFloat() / 8.0f;
	step = g_pGlobalVars->intervalPerTick;

	//	get local view and eye origin
	eye_origin = Globals::LocalPlayer->GetEyePosition();
	angles = ucmd->viewangles;

	//	copy current angles and normalise pitch
	thrown = angles;

	if (thrown.x < 0) {
		thrown.x = -10 + thrown.x * ((90 - 10) / 90.0f);
	}
	else {
		thrown.x = -10 + thrown.x * ((90 + 10) / 90.0f);
	}

	//	find out how we're throwing the grenade
	auto primary_attack = ucmd->buttons & IN_ATTACK;
	auto secondary_attack = ucmd->buttons & IN_ATTACK2;


	//	apply 'magic' and modulate by velocity
	unk01 = power[grenade_act];

	unk01 = unk01 * 0.7f;
	unk01 = unk01 + 0.3f;

	new_velocity = velocity * unk01;

	//	here's where the fun begins
	angle_to_vector(thrown, thrown_direction);

	start = eye_origin + thrown_direction * 16.0f;
	thrown_direction = (thrown_direction * new_velocity) + Globals::LocalPlayer->GetVelocity();

	//	let's go ahead and predict
	for (auto time = 0.0f; index < 500; time += step) {
		pos = start + thrown_direction * step;

		//	setup trace
		C_Trace trace;
		CTraceFilterSkipEntity filter(Globals::LocalPlayer);

		g_pTrace->TraceRay(C_Ray{ start, pos }, mask_solid, &filter, &trace);

		//	modify path if we have hit something
		if (trace.flFraction != 1.0f) {
			thrown_direction = trace.plane.normal * -2.0f * thrown_direction.Dot(trace.plane.normal) + thrown_direction;

			thrown_direction *= restitution;

			pos = start + thrown_direction * trace.flFraction * step;

			time += (step * (1.0f - trace.flFraction));
		}

		//	check for detonation
		auto detonate = true;

		//	emplace nade point
		_points.at(index++) = c_nadepoint(start, pos, trace.flFraction != 1.0f, true, trace.plane.normal, detonate);
		start = pos;

		//	apply gravity modifier
		thrown_direction.z -= gravity * trace.flFraction * step;

		if (detonate) {
			break;
		}
	}

	//	invalidate all empty points and finish prediction
	for (auto n = index; n < 500; ++n) {
		_points.at(n).m_valid = false;
	}

	_predicted = true;
}

bool c_nade_prediction::detonated() {
	return true;
}
bool PredictGrenade = true;
void c_nade_prediction::trace(CUserCmd *ucmd) {
	if (!PredictGrenade)
		return;

	if (!(ucmd->buttons & IN_ATTACK) && !(ucmd->buttons & IN_ATTACK2)) {
		_predicted = false;
		return;
	}

	const static std::vector< ItemDefinitionIndex > nades{
		ItemDefinitionIndex::WEAPON_FLASHBANG,
		ItemDefinitionIndex::WEAPON_SMOKEGRENADE,
		ItemDefinitionIndex::WEAPON_HEGRENADE,
		ItemDefinitionIndex::WEAPON_MOLOTOV,
		ItemDefinitionIndex::WEAPON_DECOY,
		ItemDefinitionIndex::WEAPON_INCGRENADE
	};

	//	grab local weapon
	auto weapon = Globals::LocalPlayer->GetActiveWeapon();

	if (!weapon) {
		return;
	}

	if (std::find(nades.begin(), nades.end(), weapon->GetItemDefinitionIndex()) != nades.end()) {
		return predict(ucmd);
	}

	_predicted = false;
}

void c_nade_prediction::draw() {
	if (!PredictGrenade)
		return;

	if (!g_pEngine->IsInGame() || !Globals::LocalPlayer || !Globals::LocalPlayer->IsAlive())
		return;

	Vector start, end;

	//	draw nade path
	if (_predicted) {
		for (auto &p : _points) {
			if (!p.m_valid) {
				break;
			}

			if (Utils::WorldToScreen2(p.m_start, start) && Utils::WorldToScreen2(p.m_end, end)) {
				//	draw line
				g_pSurface->Line(start.x, start.y, end.x, end.y, Color(255, 255, 255, 255));

				//	draw small box if detonated or hit a wall
				if (p.m_detonate || p.m_plane) {
					g_pSurface->FilledRect(start.x - 2, start.y - 2, 5, 5, Color(255, 255, 255, 255));
				}
			}
		}
	}
}


#define SEQUENCE_DEFAULT_DRAW 0
#define SEQUENCE_DEFAULT_IDLE1 1
#define SEQUENCE_DEFAULT_IDLE2 2
#define SEQUENCE_DEFAULT_LIGHT_MISS1 3
#define SEQUENCE_DEFAULT_LIGHT_MISS2 4
#define SEQUENCE_DEFAULT_HEAVY_MISS1 9
#define SEQUENCE_DEFAULT_HEAVY_HIT1 10
#define SEQUENCE_DEFAULT_HEAVY_BACKSTAB 11
#define SEQUENCE_DEFAULT_LOOKAT01 12

#define SEQUENCE_BUTTERFLY_DRAW 0
#define SEQUENCE_BUTTERFLY_DRAW2 1
#define SEQUENCE_BUTTERFLY_LOOKAT01 13
#define SEQUENCE_BUTTERFLY_LOOKAT03 15

#define SEQUENCE_FALCHION_IDLE1 1
#define SEQUENCE_FALCHION_HEAVY_MISS1 8
#define SEQUENCE_FALCHION_HEAVY_MISS1_NOFLIP 9
#define SEQUENCE_FALCHION_LOOKAT01 12
#define SEQUENCE_FALCHION_LOOKAT02 13

#define SEQUENCE_DAGGERS_IDLE1 1
#define SEQUENCE_DAGGERS_LIGHT_MISS1 2
#define SEQUENCE_DAGGERS_LIGHT_MISS5 6
#define SEQUENCE_DAGGERS_HEAVY_MISS2 11
#define SEQUENCE_DAGGERS_HEAVY_MISS1 12

#define SEQUENCE_BOWIE_IDLE1 1


int Hitmarkertime;
int HitmarkerExpansion;

std::vector<std::pair<std::string, Color>> event_logs;

struct BulletImpact_t
{
	float			flImpactTime;
	Vector			vecImpactPos;
	Color			color;
	C_BaseEntity*	pPlayer;

	__forceinline BulletImpact_t()
	{
		vecImpactPos = { 0.0f, 0.0f, 0.0f };
		flImpactTime = 0.0f;
		color = Color(255, 255, 255, 255);
		pPlayer = nullptr;
	}

	__forceinline BulletImpact_t(C_BaseEntity* player, Vector pos, float time, Color col = Color(255, 255, 255, 255))
	{
		pPlayer = player;
		flImpactTime = time;
		vecImpactPos = pos;
		color = col;
	}
};
std::deque<BulletImpact_t>				Impacts;
RecvVarProxyFn oRecvnModelIndex;
RecvVarProxyFn fnSequenceProxyFn = NULL;
RecvVarProxyFn ovecForce = nullptr;
#define RandomInt(nMin, nMax) (rand() % (nMax - nMin + 1) + nMin);
void SetViewModelSequence(const CRecvProxyData *pDataConst, void *pStruct, void *pOut) {
	// Make the incoming data editable.
	CRecvProxyData* pData = const_cast<CRecvProxyData*>(pDataConst);

	// Confirm that we are replacing our view model and not someone elses.
	C_BaseViewModel* pViewModel = (C_BaseViewModel*)pStruct;

	if (pViewModel) {
		IClientEntity* pOwner = g_pEntityList->GetClientEntityFromHandle(CBaseHandle(pViewModel->GetOwner()));

		// Compare the owner entity of this view model to the local player entity.
		if (pOwner && pOwner->EntIndex() == g_pEngine->GetLocalPlayer()) {
			// Get the filename of the current view model.
			void* pModel = g_pModelInfo->GetModel(pViewModel->GetModelIndex());

			const char* szModel = g_pModelInfo->GetModelName((model_t*)pModel);

			// Store the current sequence.
			int m_nSequence = pData->m_Value.m_Int;

			if (!strcmp(szModel, "models/weapons/v_knife_butterfly.mdl")) {
				// Fix animations for the Butterfly Knife.
				switch (m_nSequence) {
				case SEQUENCE_DEFAULT_DRAW:
					m_nSequence = RandomInt(SEQUENCE_BUTTERFLY_DRAW, SEQUENCE_BUTTERFLY_DRAW2); break;
				case SEQUENCE_DEFAULT_LOOKAT01:
					m_nSequence = RandomInt(SEQUENCE_BUTTERFLY_LOOKAT01, SEQUENCE_BUTTERFLY_LOOKAT03); break;
				default:
					m_nSequence++;
				}
			}
			else if (!strcmp(szModel, "models/weapons/v_knife_falchion_advanced.mdl")) {
				// Fix animations for the Falchion Knife.
				switch (m_nSequence) {
				case SEQUENCE_DEFAULT_IDLE2:
					m_nSequence = SEQUENCE_FALCHION_IDLE1; break;
				case SEQUENCE_DEFAULT_HEAVY_MISS1:
					m_nSequence = RandomInt(SEQUENCE_FALCHION_HEAVY_MISS1, SEQUENCE_FALCHION_HEAVY_MISS1_NOFLIP); break;
				case SEQUENCE_DEFAULT_LOOKAT01:
					m_nSequence = RandomInt(SEQUENCE_FALCHION_LOOKAT01, SEQUENCE_FALCHION_LOOKAT02); break;
				case SEQUENCE_DEFAULT_DRAW:
				case SEQUENCE_DEFAULT_IDLE1:
					break;
				default:
					m_nSequence--;
				}
			}
			else if (!strcmp(szModel, "models/weapons/v_knife_push.mdl")) {
				// Fix animations for the Shadow Daggers.
				switch (m_nSequence) {
				case SEQUENCE_DEFAULT_IDLE2:
					m_nSequence = SEQUENCE_DAGGERS_IDLE1; break;
				case SEQUENCE_DEFAULT_LIGHT_MISS1:
				case SEQUENCE_DEFAULT_LIGHT_MISS2:
					m_nSequence = RandomInt(SEQUENCE_DAGGERS_LIGHT_MISS1, SEQUENCE_DAGGERS_LIGHT_MISS5); break;
				case SEQUENCE_DEFAULT_HEAVY_MISS1:
					m_nSequence = RandomInt(SEQUENCE_DAGGERS_HEAVY_MISS2, SEQUENCE_DAGGERS_HEAVY_MISS1); break;
				case SEQUENCE_DEFAULT_HEAVY_HIT1:
				case SEQUENCE_DEFAULT_HEAVY_BACKSTAB:
				case SEQUENCE_DEFAULT_LOOKAT01:
					m_nSequence += 3; break;
				case SEQUENCE_DEFAULT_DRAW:
				case SEQUENCE_DEFAULT_IDLE1:
					break;
				default:
					m_nSequence += 2;
				}
			}
			else if (!strcmp(szModel, "models/weapons/v_knife_survival_bowie.mdl")) {
				// Fix animations for the Bowie Knife.
				switch (m_nSequence) {
				case SEQUENCE_DEFAULT_DRAW:
				case SEQUENCE_DEFAULT_IDLE1:
					break;
				case SEQUENCE_DEFAULT_IDLE2:
					m_nSequence = SEQUENCE_BOWIE_IDLE1; break;
				default:
					m_nSequence--;
				}
			}
			else if (!strcmp(szModel, "models/weapons/v_knife_ursus.mdl")) {
				switch (m_nSequence)
				{
				case SEQUENCE_DEFAULT_DRAW:
					m_nSequence = RandomInt(SEQUENCE_BUTTERFLY_DRAW, SEQUENCE_BUTTERFLY_DRAW2);
					break;
				case SEQUENCE_DEFAULT_LOOKAT01:
					m_nSequence = RandomInt(SEQUENCE_BUTTERFLY_LOOKAT01, SEQUENCE_BUTTERFLY_LOOKAT03);
					break;
				default:
					m_nSequence++;
					break;
				}
			}
			else if (!strcmp(szModel, "models/weapons/v_knife_stiletto.mdl")) {
				switch (m_nSequence)
				{
				case SEQUENCE_DEFAULT_LOOKAT01:
					m_nSequence = RandomInt(12, 13);
					break;
				}
			}
			else if (!strcmp(szModel, "models/weapons/v_knife_widowmaker.mdl")) {
				switch (m_nSequence)
				{
				case SEQUENCE_DEFAULT_LOOKAT01:
					m_nSequence = RandomInt(14, 15);
					break;
				}
			}
			// Set the fixed sequence.
			pData->m_Value.m_Int = m_nSequence;
		}
	}

	// Call original function with the modified data.
	fnSequenceProxyFn(pData, pStruct, pOut);
}



void Hooked_RecvProxy_Viewmodel(CRecvProxyData *pData, void *pStruct, void *pOut)
{
	// Get the knife view model id's
	int default_t = g_pModelInfo->GetModelIndex("models/weapons/v_knife_default_t.mdl");
	int default_ct = g_pModelInfo->GetModelIndex("models/weapons/v_knife_default_ct.mdl");
	int iBayonet = g_pModelInfo->GetModelIndex("models/weapons/v_knife_bayonet.mdl");
	int iButterfly = g_pModelInfo->GetModelIndex("models/weapons/v_knife_butterfly.mdl");
	int iFlip = g_pModelInfo->GetModelIndex("models/weapons/v_knife_flip.mdl");
	int iGut = g_pModelInfo->GetModelIndex("models/weapons/v_knife_gut.mdl");
	int iKarambit = g_pModelInfo->GetModelIndex("models/weapons/v_knife_karam.mdl");
	int iM9Bayonet = g_pModelInfo->GetModelIndex("models/weapons/v_knife_m9_bay.mdl");
	int iHuntsman = g_pModelInfo->GetModelIndex("models/weapons/v_knife_tactical.mdl");
	int iFalchion = g_pModelInfo->GetModelIndex("models/weapons/v_knife_falchion_advanced.mdl");
	int iDagger = g_pModelInfo->GetModelIndex("models/weapons/v_knife_push.mdl");
	int iBowie = g_pModelInfo->GetModelIndex("models/weapons/v_knife_survival_bowie.mdl");
	int iGunGame = g_pModelInfo->GetModelIndex("models/weapons/v_knife_gg.mdl");

	// Get local player (just to stop replacing spectators knifes)
	auto pLocal = Globals::LocalPlayer;
	if (pLocal && c_config::get().knife_model > 0)
	{
		// If we are alive and holding a default knife(if we already have a knife don't worry about changing)
		if (pLocal->IsAlive() && (
			pData->m_Value.m_Int == default_t ||
			pData->m_Value.m_Int == default_ct ||
			pData->m_Value.m_Int == iBayonet ||
			pData->m_Value.m_Int == iButterfly ||
			pData->m_Value.m_Int == iFlip ||
			pData->m_Value.m_Int == iGunGame ||
			pData->m_Value.m_Int == iGut ||
			pData->m_Value.m_Int == iKarambit ||
			pData->m_Value.m_Int == iM9Bayonet ||
			pData->m_Value.m_Int == iHuntsman ||
			pData->m_Value.m_Int == iFalchion ||
			pData->m_Value.m_Int == iDagger ||
			pData->m_Value.m_Int == iBowie))
		{
				pData->m_Value.m_Int = g_pModelInfo->GetModelIndex(gSkins.getKnifeModel());
		}
	}

	// Carry on the to original proxy
	oRecvnModelIndex(pData, pStruct, pOut);
}

void vec_force(CRecvProxyData* data, void* pStruct, void* out) {
	Vector v;

	v.x = data->m_Value.m_Vector[0];
	v.y = data->m_Value.m_Vector[1];
	v.z = data->m_Value.m_Vector[2];
	if (c_config::get().ragdoll_launcher) {
		Vector pp;
		Vector angles{ -45, g_Math.RandomFloat(-180, 180), 0 };
		g_Math.AngleVectors(angles, &pp, nullptr, nullptr);

		v = pp * 9999999999.0f;
	}
	*(Vector*)out = v;
}

void ApplyAAAHooks()
{
	ClientClass *pClass = g_pClientDll->GetAllClasses();
	while (pClass)
	{
		const char *pszName = pClass->pRecvTable->pNetTableName;

		if (!strcmp(pszName, "DT_BaseViewModel")) {
			// Search for the 'm_nModelIndex' property.
			RecvTable* pClassTable = pClass->pRecvTable;

			for (int nIndex = 0; nIndex < pClass->pRecvTable->nProps; nIndex++) {
				RecvProp *pProp = &(pClass->pRecvTable->pProps[nIndex]);

				if (!pProp || strcmp(pProp->pVarName, "m_nSequence"))
					continue;

				// Store the original proxy function.
				fnSequenceProxyFn = (RecvVarProxyFn)pProp->ProxyFn;

				// Replace the proxy function with our sequence changer.
				pProp->ProxyFn = SetViewModelSequence;
			}
		}

		if (!strcmp(pszName, "DT_BaseViewModel"))
		{
			for (int i = 0; i < pClass->pRecvTable->nProps; i++)
			{
				RecvProp *pProp = &(pClass->pRecvTable->pProps[i]);
				const char *name = pProp->pVarName;

				// Knives
				if (!strcmp(name, "m_nModelIndex"))
				{
					oRecvnModelIndex = (RecvVarProxyFn)pProp->ProxyFn;
					pProp->ProxyFn = (RecvVarProxyFn)Hooked_RecvProxy_Viewmodel;
				}
			}
		}

		if (!strcmp(pszName, "DT_CSRagdoll")) {
			for (int i = 0; i < pClass->pRecvTable->nProps; i++)
			{
				RecvProp* pProp = &(pClass->pRecvTable->pProps[i]);
				const char* name = pProp->pVarName;
				if (!strcmp(name, "m_vecForce")) {
					ovecForce = (RecvVarProxyFn)pProp->ProxyFn;
					pProp->ProxyFn = (RecvVarProxyFn)vec_force;
				}
			}
		}

		pClass = pClass->pNext;
	}
}

namespace StoredLocalPlayer {
	Vector StoredOrigin;
}

void Hooks::Init()
{
    // Get window handle
    while (!(g_Hooks.hCSGOWindow = FindWindowA("Valve001", nullptr)))
    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(50ms);
    }

    interfaces::Init();                         // Get interfaces
    g_pNetvars = std::make_unique<NetvarTree>();// Get netvars after getting interfaces as we use them

    Utils::Log("Hooking in progress...");

	DWORD shaderapidx9 = **(DWORD * *)(Utils::FindSignature("shaderapidx9.dll", "A1 ?? ?? ?? ?? 50 8B 08 FF 51 0C") + 1);

	if(g_Hooks.hCSGOWindow)        // Hook WNDProc to capture mouse / keyboard input
		g_Hooks.pOriginalWNDProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(g_Hooks.hCSGOWindow, GWLP_WNDPROC,
			reinterpret_cast<LONG_PTR>(g_Hooks.WndProc)));

    // VMTHooks
	g_Hooks.pClientHook     = std::make_unique<VMTHook>(g_pClientDll);
    g_Hooks.pClientModeHook = std::make_unique<VMTHook>(g_pClientMode);
    g_Hooks.pSurfaceHook	= std::make_unique<VMTHook>(g_pSurface);
	g_Hooks.pPanelHook		= std::make_unique<VMTHook>(g_pPanel);
	g_Hooks.pRenderViewHook = std::make_unique<VMTHook>(g_pRenderView);
	g_Hooks.pModelHook = std::make_unique<VMTHook>(g_pModelRender);
	g_Hooks.D3DHook = std::make_unique<VMTHook>((DWORD**)shaderapidx9);

    // Hook the table functions
	g_Hooks.pClientHook    ->Hook(vtable_indexes::frameStage, Hooks::FrameStageNotify);
    g_Hooks.pClientModeHook->Hook(vtable_indexes::createMove, Hooks::CreateMove);
	g_Hooks.pClientModeHook->Hook(vtable_indexes::view, Hooks::OverrideView);
	g_Hooks.pClientModeHook->Hook(vtable_indexes::getviewmodelfov, Hooks::GetViewmodelFOV);
    g_Hooks.pSurfaceHook   ->Hook(vtable_indexes::lockCursor, Hooks::LockCursor);
	g_Hooks.pPanelHook	   ->Hook(vtable_indexes::paint, Hooks::PaintTraverse);
	g_Hooks.pModelHook->Hook(vtable_indexes::dme, Hooks::DrawModelExecute);
	g_Hooks.pRenderViewHook->Hook(vtable_indexes::sceneEnd, Hooks::SceneEnd);
	g_Hooks.D3DHook->Hook(vtable_indexes::end_scene, Hooks::Hooked_EndScene);
	g_Hooks.D3DHook->Hook(vtable_indexes::end_scene_reset, Hooks::Hooked_EndScene_Reset);


	

	g_Event.Init();

	Globals::CourierNew = g_pSurface->FontCreate();
	Globals::WeaponESP = g_pSurface->FontCreate();
	Globals::SmallText = g_pSurface->FontCreate();
	Globals::WeaponIcon = g_pSurface->FontCreate();
	Globals::TabFont = g_pSurface->FontCreate();
	Globals::IndicatorFont = g_pSurface->FontCreate();

	g_pSurface->SetFontGlyphSet(Globals::CourierNew, "Verdana", 13, 800, 0, 0, FONTFLAG_DROPSHADOW);
	g_pSurface->SetFontGlyphSet(Globals::SmallText, "Tahoma", 12, 400, 0, 0, FONTFLAG_DROPSHADOW);
	g_pSurface->SetFontGlyphSet(Globals::WeaponESP, "Small Fonts", 8, 400, 0, 0, FONTFLAG_OUTLINE);
	g_pSurface->SetFontGlyphSet(Globals::WeaponIcon, "Counter-Strike", 24, 400, 0, 0, FONTFLAG_ANTIALIAS);
	g_pSurface->SetFontGlyphSet(Globals::TabFont, "Calibri Bold", 18, 200, 0, 0, FONTFLAG_DROPSHADOW);
	g_pSurface->SetFontGlyphSet(Globals::IndicatorFont, "Verdana", 24, 900, 0, 0, FONTFLAG_ANTIALIAS | FONTFLAG_DROPSHADOW);

	ApplyAAAHooks();
//	g_pEngine->ExecuteClientCmd("play ambient\\playonce\\weather\\thunder4.wav");
    Utils::Log("Hooking completed!");
}

IDirect3DStateBlock9* m_pStateBlockDraw;
IDirect3DStateBlock9* m_pStateBlockText;
DWORD					dwOld_D3DRS_COLORWRITEENABLE;

bool ColorPicker(float* col, bool alphabar)
{
	const int EDGE_SIZE = 200; // = int(ImGui::GetWindowWidth() * 0.75f);
	const ImVec2 SV_PICKER_SIZE = ImVec2(EDGE_SIZE, EDGE_SIZE);
	const float SPACING = ImGui::GetStyle().ItemInnerSpacing.x;
	const float HUE_PICKER_WIDTH = 20.f;
	const float CROSSHAIR_SIZE = 7.0f;

	ImColor color(col[0], col[1], col[2]);
	bool value_changed = false;

	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	ImVec2 picker_pos = ImGui::GetCursorScreenPos();

	float hue, saturation, value;
	ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, hue, saturation, value);

	ImColor colors[] = {
			ImColor(255, 0, 0),
			ImColor(255, 255, 0),
			ImColor(0, 255, 0),
			ImColor(0, 255, 255),
			ImColor(0, 0, 255),
			ImColor(255, 0, 255),
			ImColor(255, 0, 0)
	};

	for (int i = 0; i < 6; i++)
	{
		draw_list->AddRectFilledMultiColor(
			ImVec2(picker_pos.x + SV_PICKER_SIZE.x + SPACING, picker_pos.y + i * (SV_PICKER_SIZE.y / 6)),
			ImVec2(picker_pos.x + SV_PICKER_SIZE.x + SPACING + HUE_PICKER_WIDTH,
				picker_pos.y + (i + 1) * (SV_PICKER_SIZE.y / 6)),
			colors[i],
			colors[i],
			colors[i + 1],
			colors[i + 1]
		);
	}

	draw_list->AddLine(
		ImVec2(picker_pos.x + SV_PICKER_SIZE.x + SPACING - 2, picker_pos.y + hue * SV_PICKER_SIZE.y),
		ImVec2(picker_pos.x + SV_PICKER_SIZE.x + SPACING + 2 + HUE_PICKER_WIDTH, picker_pos.y + hue * SV_PICKER_SIZE.y),
		ImColor(255, 255, 255)
	);

	if (alphabar)
	{
		float alpha = col[3];

		draw_list->AddRectFilledMultiColor(
			ImVec2(picker_pos.x + SV_PICKER_SIZE.x + 2 * SPACING + HUE_PICKER_WIDTH, picker_pos.y),
			ImVec2(picker_pos.x + SV_PICKER_SIZE.x + 2 * SPACING + 2 * HUE_PICKER_WIDTH, picker_pos.y + SV_PICKER_SIZE.y),
			ImColor(0, 0, 0), ImColor(0, 0, 0), ImColor(255, 255, 255), ImColor(255, 255, 255)
		);

		draw_list->AddLine(
			ImVec2(picker_pos.x + SV_PICKER_SIZE.x + 2 * (SPACING - 2) + HUE_PICKER_WIDTH, picker_pos.y + alpha * SV_PICKER_SIZE.y),
			ImVec2(picker_pos.x + SV_PICKER_SIZE.x + 2 * (SPACING + 2) + 2 * HUE_PICKER_WIDTH, picker_pos.y + alpha * SV_PICKER_SIZE.y),
			ImColor(255.f - alpha, 255.f, 255.f)
		);
	}

	const ImU32 c_oColorBlack = ImGui::ColorConvertFloat4ToU32(ImVec4(0.f, 0.f, 0.f, 1.f));
	const ImU32 c_oColorBlackTransparent = ImGui::ColorConvertFloat4ToU32(ImVec4(0.f, 0.f, 0.f, 0.f));
	const ImU32 c_oColorWhite = ImGui::ColorConvertFloat4ToU32(ImVec4(1.f, 1.f, 1.f, 1.f));

	ImVec4 cHueValue(1, 1, 1, 1);
	ImGui::ColorConvertHSVtoRGB(hue, 1, 1, cHueValue.x, cHueValue.y, cHueValue.z);
	ImU32 oHueColor = ImGui::ColorConvertFloat4ToU32(cHueValue);

	draw_list->AddRectFilledMultiColor(
		ImVec2(picker_pos.x, picker_pos.y),
		ImVec2(picker_pos.x + SV_PICKER_SIZE.x, picker_pos.y + SV_PICKER_SIZE.y),
		c_oColorWhite,
		oHueColor,
		oHueColor,
		c_oColorWhite
	);

	draw_list->AddRectFilledMultiColor(
		ImVec2(picker_pos.x, picker_pos.y),
		ImVec2(picker_pos.x + SV_PICKER_SIZE.x, picker_pos.y + SV_PICKER_SIZE.y),
		c_oColorBlackTransparent,
		c_oColorBlackTransparent,
		c_oColorBlack,
		c_oColorBlack
	);

	float x = saturation * SV_PICKER_SIZE.x;
	float y = (1 - value) * SV_PICKER_SIZE.y;
	ImVec2 p(picker_pos.x + x, picker_pos.y + y);
	draw_list->AddLine(ImVec2(p.x - CROSSHAIR_SIZE, p.y), ImVec2(p.x - 2, p.y), ImColor(255, 255, 255));
	draw_list->AddLine(ImVec2(p.x + CROSSHAIR_SIZE, p.y), ImVec2(p.x + 2, p.y), ImColor(255, 255, 255));
	draw_list->AddLine(ImVec2(p.x, p.y + CROSSHAIR_SIZE), ImVec2(p.x, p.y + 2), ImColor(255, 255, 255));
	draw_list->AddLine(ImVec2(p.x, p.y - CROSSHAIR_SIZE), ImVec2(p.x, p.y - 2), ImColor(255, 255, 255));

	ImGui::InvisibleButton("saturation_value_selector", SV_PICKER_SIZE);

	if (ImGui::IsItemActive() && ImGui::GetIO().MouseDown[0])
	{
		ImVec2 mouse_pos_in_canvas = ImVec2(ImGui::GetIO().MousePos.x - picker_pos.x, ImGui::GetIO().MousePos.y - picker_pos.y);

		if (mouse_pos_in_canvas.x < 0)
			mouse_pos_in_canvas.x = 0;
		else if (mouse_pos_in_canvas.x >= SV_PICKER_SIZE.x - 1)
			mouse_pos_in_canvas.x = SV_PICKER_SIZE.x - 1;

		if (mouse_pos_in_canvas.y < 0)
			mouse_pos_in_canvas.y = 0;
		else if (mouse_pos_in_canvas.y >= SV_PICKER_SIZE.y - 1)
			mouse_pos_in_canvas.y = SV_PICKER_SIZE.y - 1;

		value = 1 - (mouse_pos_in_canvas.y / (SV_PICKER_SIZE.y - 1));
		saturation = mouse_pos_in_canvas.x / (SV_PICKER_SIZE.x - 1);
		value_changed = true;
	}

	// hue bar logic
	ImGui::SetCursorScreenPos(ImVec2(picker_pos.x + SPACING + SV_PICKER_SIZE.x, picker_pos.y));
	ImGui::InvisibleButton("hue_selector", ImVec2(HUE_PICKER_WIDTH, SV_PICKER_SIZE.y));

	if (ImGui::GetIO().MouseDown[0] && (ImGui::IsItemHovered() || ImGui::IsItemActive()))
	{
		ImVec2 mouse_pos_in_canvas = ImVec2(ImGui::GetIO().MousePos.x - picker_pos.x, ImGui::GetIO().MousePos.y - picker_pos.y);

		if (mouse_pos_in_canvas.y < 0)
			mouse_pos_in_canvas.y = 0;
		else if (mouse_pos_in_canvas.y >= SV_PICKER_SIZE.y - 1)
			mouse_pos_in_canvas.y = SV_PICKER_SIZE.y - 1;

		hue = mouse_pos_in_canvas.y / (SV_PICKER_SIZE.y - 1);
		value_changed = true;
	}

	if (alphabar)
	{
		ImGui::SetCursorScreenPos(ImVec2(picker_pos.x + SPACING * 2 + HUE_PICKER_WIDTH + SV_PICKER_SIZE.x, picker_pos.y));
		ImGui::InvisibleButton("alpha_selector", ImVec2(HUE_PICKER_WIDTH, SV_PICKER_SIZE.y));

		if (ImGui::GetIO().MouseDown[0] && (ImGui::IsItemHovered() || ImGui::IsItemActive()))
		{
			ImVec2 mouse_pos_in_canvas = ImVec2(ImGui::GetIO().MousePos.x - picker_pos.x, ImGui::GetIO().MousePos.y - picker_pos.y);

			if (mouse_pos_in_canvas.y < 0)
				mouse_pos_in_canvas.y = 0;
			else if (mouse_pos_in_canvas.y >= SV_PICKER_SIZE.y - 1)
				mouse_pos_in_canvas.y = SV_PICKER_SIZE.y - 1;

			float alpha = mouse_pos_in_canvas.y / (SV_PICKER_SIZE.y - 1);
			col[3] = alpha;
			value_changed = true;
		}
	}

	color = ImColor::HSV(hue >= 1 ? hue - 10 * 1e-6 : hue, saturation > 0 ? saturation : 10 * 1e-6, value > 0 ? value : 1e-6);
	col[0] = color.Value.x;
	col[1] = color.Value.y;
	col[2] = color.Value.z;

	bool widget_used;
	ImGui::PushItemWidth((alphabar ? SPACING + HUE_PICKER_WIDTH : 0) + SV_PICKER_SIZE.x + SPACING + HUE_PICKER_WIDTH - 2 * ImGui::GetStyle().FramePadding.x);
	widget_used = alphabar ? ImGui::ColorEdit4("", col) : ImGui::ColorEdit3("", col);
	ImGui::PopItemWidth();

	float new_hue, new_sat, new_val;
	ImGui::ColorConvertRGBtoHSV(col[0], col[1], col[2], new_hue, new_sat, new_val);

	if (new_hue <= 0 && hue > 0)
	{
		if (new_val <= 0 && value != new_val)
		{
			color = ImColor::HSV(hue, saturation, new_val <= 0 ? value * 0.5f : new_val);
			col[0] = color.Value.x;
			col[1] = color.Value.y;
			col[2] = color.Value.z;
		}
		else if (new_sat <= 0)
		{
			color = ImColor::HSV(hue, new_sat <= 0 ? saturation * 0.5f : new_sat, new_val);
			col[0] = color.Value.x;
			col[1] = color.Value.y;
			col[2] = color.Value.z;
		}
	}

	return value_changed | widget_used;
}


#include <intrin.h>
#include "../Font.h"
#include "../ImGuiFonts.h"
CIFonts gIFonts;
void __stdcall Hooks::Hooked_EndScene(IDirect3DDevice9* pDevice)
{
	static auto oEndScene = g_Hooks.D3DHook->GetOriginal<EndSceneFn>(vtable_indexes::end_scene);

	if (!g_Menu.D3DInit)
		g_Menu.GUI_Init(pDevice);

	ImGui::GetIO().MouseDrawCursor = false;

	static void* dwReturnAddress = _ReturnAddress();

	if (dwReturnAddress == _ReturnAddress())
	{
		pDevice->CreateStateBlock(D3DSBT_PIXELSTATE, &m_pStateBlockDraw);
		pDevice->CreateStateBlock(D3DSBT_PIXELSTATE, &m_pStateBlockText);

		if (m_pStateBlockDraw)
			m_pStateBlockDraw->Capture();

		pDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &dwOld_D3DRS_COLORWRITEENABLE);
		pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xffffffff);

		ImGui_ImplDX9_NewFrame();


		if (g_Menu.menuOpened)
		{
			static int curTab = 0;
			static int legit_aimbot_sub_curTab = 0;
			static int rage_aimbot_sub_curTab = 0;
			static int visuals_sub_curTab = 0;

			/*Aim*/
			const char* pitch[] = { "Disabled", "Down", "Up", "Origin", "Offset" };
			const char* yaw[] = { "Disabled", "Backwards", "Origin", "Spin", "Manual" };
			const char* BodyAimOptions[] = { "Bodyaim Players In Air", "Bodyaim Players Who Are Slow Walking", "Bodyaim With High Inaccuracy", "Bodyaim Vulnerable Players", "Bodyaim Lethal Shots" };
			const char* HitboxOptionz[] = { "Hitscan", "Head Only" };
			const char* AutoStopOptions[] = { "Minimum Speed", "Full Stop" };
			const char* BodyAimModeOptions[] = { "Prefer", "Force" };
			const char* WeaponGroupOptions[] = { "Auto", "Scout", "Awp", "Heavy Pistol (Deagle, Revolver)", "Other" };
			const char* freestand_mode[] = { "Threat", "Crosshair", "Distance" };
			const char* desync[] = { "Disabled", "Ephemeral", "Cromulent", "Ornery", "Jitter" };

			/*Visuals*/
			const char* WireFrameHands[] = { "Disabled", "Invisible", "Chams" };
			const char* EventLogsOptions[] = { "Weapon Purchase", "Player Hurt", "Player Killed" };
			const char* Health_Type[] = { "Disabled", "Normal", "Battery" };
			const char* Hitmarker_Type[] = { "Disabled", "Arena Switch", "Custom" };

			static float name_col[4] = { c_config::get().name_esp_color_r / 255, c_config::get().name_esp_color_g / 255, c_config::get().name_esp_color_b / 255, c_config::get().name_esp_color_a / 255 };

			ImGuiIO& io = ImGui::GetIO();

			int pX, pY;
			POINT mp;
			GetCursorPos(&mp);

			io.MousePos.x = mp.x;
			io.MousePos.y = mp.y;

			ImGui::Begin("xy0", &g_Menu.menuOpened, ImVec2(800, 600), 1.f, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
			{
				if (ImGui::BeginChild("##Tabs", ImVec2(75, 0))) {
					if (ImGui::Button("Legit Aim", ImVec2(75, 0))) {
						curTab = 0;
					}
					if (ImGui::Button("Rage Aim", ImVec2(75, 0))) {
						curTab = 1;
					}
					if (ImGui::Button("Visuals", ImVec2(75, 0))) {
						curTab = 2;
					}
					ImGui::EndChild();
				}

				switch (curTab) {
				case 0: {
					ImGui::SetCursorPos(ImVec2(106, 40));

					if (ImGui::BeginChild("##SubTabs", ImVec2(0, 25))) {

						if (ImGui::Button("Main")) {
							legit_aimbot_sub_curTab = 0;
						}

						ImGui::EndChild();
					}

					ImGui::SetCursorPos(ImVec2(106, 80));

					if (ImGui::BeginChild("##Controls", ImVec2(0, 0))) {

						switch (legit_aimbot_sub_curTab) {
						case 0: {
							ImGui::Checkbox("Enabled", &c_config::get().legit_aimbot_enabled);
							ImGui::Checkbox("Position Adjustment", &c_config::get().legit_aimbot_backtrack);
							ImGui::SliderInt("FOV", &c_config::get().legit_aimbot_fov, 0, 100);
							ImGui::SliderInt("Speed", &c_config::get().linear_progression_threshold, 0, 100);
							ImGui::Checkbox("RCS", &c_config::get().rcs);
							ImGui::SliderInt("RCS X", &c_config::get().rcs_x, 0, 100);
							ImGui::SliderInt("RCS Y", &c_config::get().rcs_y, 0, 100);
						}break;
						}
						ImGui::EndChild();
					}
				} break;
				case 1: {

					ImGui::SetCursorPos(ImVec2(106, 40));

					if (ImGui::BeginChild("##SubTabs", ImVec2(0, 25))) {

						if (ImGui::Button("Main")) {
							rage_aimbot_sub_curTab = 0;
						}

						ImGui::SameLine();

						if (ImGui::Button("Target")) {
							rage_aimbot_sub_curTab = 1;
						}

						ImGui::SameLine();

						if (ImGui::Button("Accuracy")) {
							rage_aimbot_sub_curTab = 2;
						}

						ImGui::SameLine();

						if (ImGui::Button("Body-Aim")) {
							rage_aimbot_sub_curTab = 3;
						}

						ImGui::SameLine();

						if (ImGui::Button("Anti-Aim")) {
							rage_aimbot_sub_curTab = 4;
						}

						ImGui::EndChild();
					}

					ImGui::SetCursorPos(ImVec2(106, 80));

					if (ImGui::BeginChild("##Controls", ImVec2(0, 0))) {
						
						switch (rage_aimbot_sub_curTab) {
						case 0: {
							ImGui::Checkbox("Enabled", &c_config::get().aimbot_enabled);
							ImGui::Checkbox("Silent Aim", &c_config::get().aimbot_silentaim);
							ImGui::Checkbox("Remove Recoil", &c_config::get().aimbot_norecoil);
							ImGui::Checkbox("Fakelag Prediction", &c_config::get().fakelag_prediction);
							ImGui::Checkbox("Auto Stop", &c_config::get().autostop);
							if (c_config::get().autostop) 
							{
								ImGui::SameLine();
								ImGui::Combo("##autostopopt", &c_config::get().autostop_mode, AutoStopOptions, ARRAYSIZE(AutoStopOptions));
								ImGui::Checkbox("Stop Between Shots", &c_config::get().stop_inbetween_shots);
							}
							ImGui::Checkbox("Auto Scope", &c_config::get().auto_scope);
							ImGui::Checkbox("Auto Revolver", &c_config::get().autorevolver);
							ImGui::Checkbox("Accuracy Boost", &c_config::get().accuracy_boost);
							
						}break;
						case 1: {
							ImGui::SliderInt("Head Scale", &c_config::get().aimbot_headpointscale, 0, 100);
							ImGui::SliderInt("Body Scale", &c_config::get().aimbot_bodypointscale, 0, 100);
						}break;
						case 2: {
							ImGui::Checkbox("Resolver", &c_config::get().aimbot_resolver);
							static int weapon_group;

							ImGui::Combo("Weapon Group", &weapon_group, WeaponGroupOptions, ARRAYSIZE(WeaponGroupOptions));

							switch (weapon_group) {
							case 0:
							{
								ImGui::SliderInt("Hitchance", &c_config::get().auto_hitchance, 0, 100);
								ImGui::SliderInt("Minimum Damage", &c_config::get().auto_mindamage, 0, 100);
							}
							break;
							case 1:
							{
								ImGui::SliderInt("Hitchance", &c_config::get().scout_hitchance, 0, 100);
								ImGui::SliderInt("Minimum Damage", &c_config::get().scout_mindamage, 0, 100);
							}
							break;
							case 2:
							{
								ImGui::SliderInt("Hitchance", &c_config::get().awp_hitchance, 0, 100);
								ImGui::SliderInt("Minimum Damage", &c_config::get().awp_mindamage, 0, 100);
							}
							break;
							case 3:
							{
								ImGui::SliderInt("Hitchance", &c_config::get().heavy_pistol_hitchance, 0, 100);
								ImGui::SliderInt("Minimum Damage", &c_config::get().heavy_pistol_mindamage, 0, 100);
							}
							break;
							case 4:
							{
								ImGui::SliderInt("Hitchance", &c_config::get().other_hitchance, 0, 100);
								ImGui::SliderInt("Minimum Damage", &c_config::get().other_mindamage, 0, 100);
							}
							break;
							}
						}break;
						case 3: {

							ImGui::Combo("Baim Mode", &c_config::get().bodyaim_mode, BodyAimModeOptions, ARRAYSIZE(BodyAimModeOptions));
							ImGui::SliderInt("Baim < X Health", &c_config::get().bodyaim_health, 0, 101);
							ImGui::SliderInt("Baim After X Missed Shots", &c_config::get().bodyaim_shots, 0, 10);
							ImGui::Checkbox(BodyAimOptions[0], &c_config::get().prefer_bodyaim[0]);
							ImGui::Checkbox(BodyAimOptions[1], &c_config::get().prefer_bodyaim[1]);
							ImGui::Checkbox(BodyAimOptions[2], &c_config::get().prefer_bodyaim[2]);
							ImGui::Checkbox(BodyAimOptions[3], &c_config::get().prefer_bodyaim[3]);
							ImGui::Checkbox(BodyAimOptions[4], &c_config::get().prefer_bodyaim[4]);
						}break;
						case 4: {
							ImGui::Checkbox("Enabled", &c_config::get().antiaim_enabled);

							static int aa_type;

							ImGui::Combo("Anti-Aim Type", &aa_type, "Standing\0Moving\0In Air", 3);

							switch (aa_type) {
							case 0: {
								ImGui::Combo("X", &c_config::get().pitch_standing, pitch, ARRAYSIZE(pitch));
								ImGui::Combo("Y", &c_config::get().yaw_standing, yaw, ARRAYSIZE(yaw));
							}break;
							case 1: {
								ImGui::Combo("X", &c_config::get().pitch_moving, pitch, ARRAYSIZE(pitch));
								ImGui::Combo("Y", &c_config::get().yaw_moving, yaw, ARRAYSIZE(yaw));
							}break;
							case 2: {
								ImGui::Combo("X", &c_config::get().pitch_air, pitch, ARRAYSIZE(pitch));
								ImGui::Combo("Y", &c_config::get().yaw_air, yaw, ARRAYSIZE(yaw));
							}break;
							}

							ImGui::Checkbox("Auto Direction", &c_config::get().antiaim_freestanding);
							if (c_config::get().antiaim_freestanding) {
								ImGui::SameLine();
								ImGui::Combo("##freestand", &c_config::get().antiaim_freestanding_mode, freestand_mode, ARRAYSIZE(freestand_mode));
								ImGui::SameLine();
								ImGui::Checkbox("Extrapolated", &c_config::get().extrapolated);
							}
							ImGui::Combo("Desync", &c_config::get().desync, desync, ARRAYSIZE(desync));
							ImGui::SliderInt("Angle Jitter", &c_config::get().aa_jitter, 0, 180);
						}break;
						}
						ImGui::EndChild();
					}
				}break;
				case 2: {

					ImGui::SetCursorPos(ImVec2(106, 40));

					if (ImGui::BeginChild("##SubTabs", ImVec2(0, 25))) {

						if (ImGui::Button("Players")) {
							visuals_sub_curTab = 0;
						}

						//ImGui::SameLine();

						ImGui::EndChild();
					}

					ImGui::SetCursorPos(ImVec2(106, 80));

					if (ImGui::BeginChild("##Controls", ImVec2(0, 0))) {

						switch (visuals_sub_curTab) {
						case 0: {
							ImGui::Checkbox("Enabled", &c_config::get().visuals_enabled);

						
							
							ImGui::Checkbox("Name", &c_config::get().name_enemies);
							ImGui::SameLine();
							ColorPicker(name_col, true);
							//color_selector("box_col", &c_config::get().box_esp_color_r, &c_config::get().box_esp_color_g,
							//	&c_config::get().box_esp_color_b, &c_config::get().box_esp_color_a);
							ImGui::Checkbox("Bounding Box", &c_config::get().box_enemies);
							ImGui::Combo("Health", &c_config::get().health_enemies, Health_Type, ARRAYSIZE(Health_Type));
							//	color_selector("weapon_col", &c_config::get().weapon_esp_color_r, &c_config::get().weapon_esp_color_g,
							//		&c_config::get().weapon_esp_color_b, &c_config::get().weapon_esp_color_a);
							ImGui::Checkbox("Weapon", &c_config::get().weapon_enemies);
							if (c_config::get().weapon_enemies) ImGui::Checkbox("Show Icon When Possible", &c_config::get().show_icon_when_possible_enemies);
							ImGui::Checkbox("Flags", &c_config::get().flags_enemy);
							ImGui::Checkbox("Ammo", &c_config::get().ammo_enemy);
						}break;
						}

						ImGui::EndChild();
					}
				}break;
				}
				ImGui::End();
			}
		}

		pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, dwOld_D3DRS_COLORWRITEENABLE);

		if (m_pStateBlockDraw)
		{
			m_pStateBlockDraw->Apply();
			m_pStateBlockDraw->Release();
		}

		if (m_pStateBlockText)
			m_pStateBlockText->Release();
	}
	ImGui::Render();
	oEndScene(pDevice);
}

void __stdcall Hooks::Hooked_EndScene_Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	static auto oEndSceneReset = g_Hooks.D3DHook->GetOriginal<EndSceneResetFn>(vtable_indexes::end_scene_reset);

	if (!g_Menu.D3DInit)
		return oEndSceneReset(pDevice, pPresentationParameters);

	ImGui_ImplDX9_InvalidateDeviceObjects();

	auto hr = oEndSceneReset;
	ImGui_ImplDX9_CreateDeviceObjects();

	return hr(pDevice, pPresentationParameters);
}

#include "SDK/Hitboxes.h"
void RenderSkeleton(C_BaseEntity* pEnt, matrix3x4_t *matrix, Color skelecolor) // the best
{
		Vector Hitbox[19];
		Vector2D Hitboxw2s[19];

		for (int hitbox = 0; hitbox < 19; hitbox++)
		{
			Hitbox[hitbox] = pEnt->GetHitboxPosition(hitbox, matrix);
			Utils::WorldToScreen(Hitbox[hitbox], Hitboxw2s[hitbox]);
		}

		//spine
		g_pSurface->Line(Hitboxw2s[HITBOX_HEAD].x, Hitboxw2s[HITBOX_HEAD].y, Hitboxw2s[HITBOX_NECK].x, Hitboxw2s[HITBOX_NECK].y, skelecolor);
		g_pSurface->Line(Hitboxw2s[HITBOX_NECK].x, Hitboxw2s[HITBOX_NECK].y, Hitboxw2s[HITBOX_UPPER_CHEST].x, Hitboxw2s[HITBOX_UPPER_CHEST].y, skelecolor);
		g_pSurface->Line(Hitboxw2s[HITBOX_UPPER_CHEST].x, Hitboxw2s[HITBOX_UPPER_CHEST].y, Hitboxw2s[HITBOX_LOWER_CHEST].x, Hitboxw2s[HITBOX_LOWER_CHEST].y, skelecolor);
		g_pSurface->Line(Hitboxw2s[HITBOX_LOWER_CHEST].x, Hitboxw2s[HITBOX_LOWER_CHEST].y, Hitboxw2s[HITBOX_THORAX].x, Hitboxw2s[HITBOX_THORAX].y, skelecolor);
		g_pSurface->Line(Hitboxw2s[HITBOX_THORAX].x, Hitboxw2s[HITBOX_THORAX].y, Hitboxw2s[HITBOX_BELLY].x, Hitboxw2s[HITBOX_BELLY].y, skelecolor);
		g_pSurface->Line(Hitboxw2s[HITBOX_BELLY].x, Hitboxw2s[HITBOX_BELLY].y, Hitboxw2s[HITBOX_PELVIS].x, Hitboxw2s[HITBOX_PELVIS].y, skelecolor);

		//right leg
		g_pSurface->Line(Hitboxw2s[HITBOX_PELVIS].x, Hitboxw2s[HITBOX_PELVIS].y, Hitboxw2s[HITBOX_RIGHT_THIGH].x, Hitboxw2s[HITBOX_RIGHT_THIGH].y, skelecolor);
		g_pSurface->Line(Hitboxw2s[HITBOX_RIGHT_THIGH].x, Hitboxw2s[HITBOX_RIGHT_THIGH].y, Hitboxw2s[HITBOX_RIGHT_CALF].x, Hitboxw2s[HITBOX_RIGHT_CALF].y, skelecolor);
		g_pSurface->Line(Hitboxw2s[HITBOX_RIGHT_CALF].x, Hitboxw2s[HITBOX_RIGHT_CALF].y, Hitboxw2s[HITBOX_RIGHT_FOOT].x, Hitboxw2s[HITBOX_RIGHT_FOOT].y, skelecolor);

		//right arm
		g_pSurface->Line(Hitboxw2s[HITBOX_NECK].x, Hitboxw2s[HITBOX_NECK].y, Hitboxw2s[HITBOX_RIGHT_UPPER_ARM].x, Hitboxw2s[HITBOX_RIGHT_UPPER_ARM].y, skelecolor);
		g_pSurface->Line(Hitboxw2s[HITBOX_RIGHT_UPPER_ARM].x, Hitboxw2s[HITBOX_RIGHT_UPPER_ARM].y, Hitboxw2s[HITBOX_RIGHT_FOREARM].x, Hitboxw2s[HITBOX_RIGHT_FOREARM].y, skelecolor);
		g_pSurface->Line(Hitboxw2s[HITBOX_RIGHT_FOREARM].x, Hitboxw2s[HITBOX_RIGHT_FOREARM].y, Hitboxw2s[HITBOX_RIGHT_HAND].x, Hitboxw2s[HITBOX_RIGHT_HAND].y, skelecolor);

		//left leg
		g_pSurface->Line(Hitboxw2s[HITBOX_PELVIS].x, Hitboxw2s[HITBOX_PELVIS].y, Hitboxw2s[HITBOX_LEFT_THIGH].x, Hitboxw2s[HITBOX_LEFT_THIGH].y, skelecolor);
		g_pSurface->Line(Hitboxw2s[HITBOX_LEFT_THIGH].x, Hitboxw2s[HITBOX_LEFT_THIGH].y, Hitboxw2s[HITBOX_LEFT_CALF].x, Hitboxw2s[HITBOX_LEFT_CALF].y, skelecolor);
		g_pSurface->Line(Hitboxw2s[HITBOX_LEFT_CALF].x, Hitboxw2s[HITBOX_LEFT_CALF].y, Hitboxw2s[HITBOX_LEFT_FOOT].x, Hitboxw2s[HITBOX_LEFT_FOOT].y, skelecolor);

		//left arm
		g_pSurface->Line(Hitboxw2s[HITBOX_NECK].x, Hitboxw2s[HITBOX_NECK].y, Hitboxw2s[HITBOX_LEFT_UPPER_ARM].x, Hitboxw2s[HITBOX_LEFT_UPPER_ARM].y, skelecolor);
		g_pSurface->Line(Hitboxw2s[HITBOX_LEFT_UPPER_ARM].x, Hitboxw2s[HITBOX_LEFT_UPPER_ARM].y, Hitboxw2s[HITBOX_LEFT_FOREARM].x, Hitboxw2s[HITBOX_LEFT_FOREARM].y, skelecolor);
		g_pSurface->Line(Hitboxw2s[HITBOX_LEFT_FOREARM].x, Hitboxw2s[HITBOX_LEFT_FOREARM].y, Hitboxw2s[HITBOX_LEFT_HAND].x, Hitboxw2s[HITBOX_LEFT_HAND].y, skelecolor);

}
void Hooks::Restore()
{
	Utils::Log("Unhooking in progress...");
    {   // Unhook every function we hooked and restore original one
        g_Hooks.pClientModeHook->Unhook(vtable_indexes::createMove);
        g_Hooks.pSurfaceHook->Unhook(vtable_indexes::lockCursor);
        SetWindowLongPtr(g_Hooks.hCSGOWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_Hooks.pOriginalWNDProc));

        g_pNetvars.reset();   /* Need to reset by-hand, global pointer so doesnt go out-of-scope */
    }
    Utils::Log("Unhooking succeded!");
}

bool __fastcall Hooks::SendNetMsg(NetChannel* pNetChan, void* edx, INetMessage& msg, bool bForceReliable, bool bVoice)
{
	static auto oCreateMove = g_Hooks.pClientModeHook->GetOriginal<CreateMove_t>(vtable_indexes::createMove);

/*	if (msg.GetType() == 14) // Return and don't send messsage if its FileCRCCheck
		return false;

	if (msg.GetGroup() == 9) // Fix lag when transmitting voice and fakelagging
		bVoice = true;*/

	//return g_Hooks.oSendNetMsg(pNetChan, msg, bForceReliable, bVoice);
	return false;
}

#include "Features/GrenadePrediction/GrenadePrediction.h"
#include "Features/Legit Aimbot/Legit Aimbot.h"
bool __fastcall Hooks::CreateMove(IClientMode* thisptr, void* edx, float sample_frametime, CUserCmd* pCmd)
{
    // Call original createmove before we start screwing with it
    static auto oCreateMove = g_Hooks.pClientModeHook->GetOriginal<CreateMove_t>(vtable_indexes::createMove);
    oCreateMove(thisptr, edx, sample_frametime, pCmd);

    if (!pCmd || !pCmd->command_number)
        return false;

    // Get globals
    Globals::pCmd         = pCmd;
    Globals::LocalPlayer = g_pEntityList->GetClientEntity(g_pEngine->GetLocalPlayer());
	Globals::bSendPacket  = true;
    if (!Globals::LocalPlayer)
        return false;

	uintptr_t *framePtr;
	__asm mov framePtr, ebp;

	Globals::OriginalView = Globals::pCmd->viewangles;

	if (Globals::LocalPlayer && Globals::LocalPlayer->IsAlive()) {
		backtracking->legitBackTrack(pCmd, Globals::LocalPlayer);
	}

    g_Misc.OnCreateMove();
	g_Resolver.OnCreateMove();

	if (c_config::get().misc_fastcrouch)
		pCmd->buttons |= IN_BULLRUSH;

    engine_prediction::RunEnginePred();
	g_AntiAim.OnCreateMove();
	g_Aimbot.OnCreateMove();
	Vector OrigAng = Globals::pCmd->viewangles;
	//CircleStrafer(OrigAng.y);

	if(Globals::LocalPlayer && Globals::LocalPlayer->IsAlive())
	{
		if (c_config::get().grenade_prediction) {
			GrenadePrediction::get().Tick(pCmd->buttons);
		}
		


		g_LegitAimbot.Aim_CreateMove(pCmd);
	}
	
    engine_prediction::EndEnginePred();
	g_Misc.NoRecoil(pCmd);
	//g_nadepred.trace(pCmd);
	//g_Misc.slow_walk(pCmd);
	//g_Misc.LinearExtrapolations();
	//FakeCrouch(pCmd, Globals::bSendPacket);
	g_Misc.MovementFix(Globals::OriginalView);
	g_Math.Clamp(Globals::pCmd->viewangles);
	

	if (Globals::bSendPacket && Globals::LocalPlayer) {
		if (Globals::LocalPlayer->IsAlive()) {
			StoredLocalPlayer::StoredOrigin = Globals::LocalPlayer->GetOrigin();
		}
	}

	if (Globals::bSendPacket)
		Globals::RealAngle = Globals::pCmd->viewangles;
	else
		Globals::FakeAngle = Globals::pCmd->viewangles;

	*(bool*)(*framePtr - 0x1C) = Globals::bSendPacket;

    return false;
}
#include <functional>
#include <utility>


struct hud_weapons_t {
	std::int32_t* get_weapon_count() {
		return reinterpret_cast<std::int32_t*>(std::uintptr_t(this) + 0x80);
	}
};
template<class T>
static T* FindHudElement(const char* name)
{
	static auto pThis = *reinterpret_cast<DWORD**>(Utils::FindSignature("client_panorama.dll", "B9 ? ? ? ? E8 ? ? ? ? 8B 5D 08") + 1);

	static auto find_hud_element = reinterpret_cast<DWORD(__thiscall*)(void*, const char*)>(Utils::FindSignature("client_panorama.dll", "55 8B EC 53 8B 5D 08 56 57 8B F9 33 F6 39"));
	return (T*)find_hud_element(pThis, name);
}
void clearRefCountedVector(CUtlVector<IRefCounted*>& vec)
{
	for (auto &elem : vec)
	{
		if (elem)
		{
			elem->unreference();
			elem = nullptr;
		}
	}
	vec.RemoveAll();
}
void ForceItemUpdate(C_WeaponCSBase *item)
{
	if (!item)
		return;

	C_EconItemView &view = item->m_AttributeManager.m_Item;

	item->m_bCustomMaterialInitialized = (reinterpret_cast<C_BaseCombatWeapon*>(item)->FallbackPaintKit() <= 0);

	item->m_CustomMaterials.RemoveAll(); // clear vector, but don't unreference items
	view.m_CustomMaterials.RemoveAll();
	clearRefCountedVector(view.m_VisualsDataProcessors); // prevent memory leak

	item->PostDataUpdate(0);
	item->OnDataChanged(0);
}


void ForceItemUpdates()
{
	auto local = Globals::LocalPlayer;
	if (local)
	{
		auto weapons = local->m_hMyWeapons();

		for (size_t i = 0; weapons[i].IsValid(); i++)
		{
			//auto weapon = weapons[i].Get();

			//ForceItemUpdate(reinterpret_cast<C_WeaponCSBase*>(weapon));
		}
	}
}
typedef void(*CL_FullUpdate_t) (void);
CL_FullUpdate_t CL_FullUpdate = nullptr;

std::uint8_t* pattern_scan(void* module, const char* signature) {
	static auto pattern_to_byte = [](const char* pattern) {
		auto bytes = std::vector<int>{};
		auto start = const_cast<char*>(pattern);
		auto end = const_cast<char*>(pattern) + strlen(pattern);

		for (auto current = start; current < end; ++current) {
			if (*current == '?') {
				++current;
				if (*current == '?')
					++current;
				bytes.push_back(-1);
			}
			else {
				bytes.push_back(strtoul(current, &current, 16));
			}
		}
		return bytes;
	};

	auto dos_headers = (PIMAGE_DOS_HEADER)module;
	auto nt_headers = (PIMAGE_NT_HEADERS)((std::uint8_t*)module + dos_headers->e_lfanew);

	auto size_of_image = nt_headers->OptionalHeader.SizeOfImage;
	auto pattern_bytes = pattern_to_byte(signature);
	auto scan_bytes = reinterpret_cast<std::uint8_t*>(module);

	auto s = pattern_bytes.size();
	auto d = pattern_bytes.data();

	for (auto i = 0ul; i < size_of_image - s; ++i) {
		bool found = true;
		for (auto j = 0ul; j < s; ++j) {
			if (scan_bytes[i + j] != d[j] && d[j] != -1) {
				found = false;
				break;
			}
		}
		if (found) {
			return &scan_bytes[i];
		}
	}
	return nullptr;
}

template<class T>
static T* find_hud_element(const char* name) {
	static auto fn = *reinterpret_cast<DWORD**>(pattern_scan(GetModuleHandleA("client_panorama.dll"), ("B9 ? ? ? ? E8 ? ? ? ? 8B 5D 08")) + 1);

	static auto find_hud_element = reinterpret_cast<DWORD(__thiscall*)(void*, const char*)>(pattern_scan(GetModuleHandleA("client_panorama.dll"), ("55 8B EC 53 8B 5D 08 56 57 8B F9 33 F6 39 77 28")));
	return (T*)find_hud_element(fn, name);
}

void KnifeApplyCallbk()
{
	static auto fn = reinterpret_cast<std::int32_t(__thiscall*)(void*, std::int32_t)>(pattern_scan(GetModuleHandleA("client_panorama.dll"), ("55 8B EC 51 53 56 8B 75 08 8B D9 57 6B FE 2C")));

	auto element = find_hud_element<std::uintptr_t*>(("CCSGO_HudWeaponSelection"));

	auto hud_weapons = reinterpret_cast<hud_weapons_t*>(std::uintptr_t(element) - 0xA0);
	if (hud_weapons == nullptr)
		return;

	if (!*hud_weapons->get_weapon_count())
		return;

	for (std::int32_t i = 0; i < *hud_weapons->get_weapon_count(); i++)
		i = fn(hud_weapons, i);

	static auto CL_FullUpdate = reinterpret_cast<CL_FullUpdate_t>(Utils::FindPattern("engine.dll", reinterpret_cast<PBYTE>("\xA1\x00\x00\x00\x00\xB9\x00\x00\x00\x00\x56\xFF\x50\x14\x8B\x34\x85"), "x????x????xxxxxxx"));
	CL_FullUpdate();
}

void DrawBeam(Vector src, Vector end, Color color)
{
	BeamInfo_t beamInfo;
	beamInfo.m_nType = TE_BEAMTESLA;
	beamInfo.m_pszModelName = "sprites/purplelaser1.vmt";
	beamInfo.m_nModelIndex = -1;
	beamInfo.m_flHaloScale = 0.0f;
	beamInfo.m_flLife = 3.0f;
	beamInfo.m_flWidth = 2.0f;
	beamInfo.m_flEndWidth = 2.0f;
	beamInfo.m_flFadeLength = 0.0f;
	beamInfo.m_flAmplitude = 2.0f;
	beamInfo.m_flBrightness = color.alpha;
	beamInfo.m_flSpeed = 0.2f;
	beamInfo.m_nStartFrame = 0;
	beamInfo.m_flFrameRate = 0.f;
	beamInfo.m_flRed = color.red;
	beamInfo.m_flGreen = color.green;
	beamInfo.m_flBlue = color.blue;
	beamInfo.m_nSegments = 2;
	beamInfo.m_bRenderable = true;
	beamInfo.m_vecStart = src;
	beamInfo.m_vecEnd = end;
	Beam_t* myBeam = g_pRenderBeams->CreateBeamPoints(beamInfo);
	if (myBeam)
		g_pRenderBeams->DrawBeam(myBeam);
}

void __stdcall Hooks::FrameStageNotify(ClientFrameStage_t curStage)
{
	static auto oFrameStage = g_Hooks.pClientHook->GetOriginal<FrameStageNotify_t>(vtable_indexes::frameStage);

	g_Misc.Thirdperson_FSN(curStage);

	g_Resolver.FrameStage(curStage);

	g_Misc.NightMode();

	g_Misc.AsusProps();

	
	static int stored_shit = 0;
	static int stored_skin_knife_shot = 0;
	static int stored_ssg_shit = 0;
	static int stored_scar20_shit = 0;
	static int stored_g3sg1_shit = 0;
	static int stored_ak47_shit = 0;
	static int stored_cz75_shit = 0;
	static int stored_m4a1_shit = 0;
	static int stored_m4a1s_shit = 0;
	static int stored_deagle_shit = 0;
	static int stored_revolver_shit = 0;
	if (stored_shit != c_config::get().knife_model || stored_skin_knife_shot != c_config::get().knife_skin || stored_ssg_shit != c_config::get().ssg08_skin || stored_scar20_shit != c_config::get().scar20_skin || stored_g3sg1_shit != c_config::get().g3sg1_skin || stored_ak47_shit != c_config::get().ak47_skin || stored_cz75_shit != c_config::get().cz75_skin || stored_deagle_shit != c_config::get().deagle_skin || stored_revolver_shit != c_config::get().r8_skin | stored_m4a1_shit != c_config::get().m4a4_skin | stored_m4a1s_shit != c_config::get().m4a1_s_skin) {
		if (g_pEngine->IsInGame() && g_pEngine->IsConnected() && Globals::LocalPlayer && Globals::LocalPlayer->IsAlive() && Globals::LocalPlayer->GetActiveWeapon())
			KnifeApplyCallbk();

		stored_shit = c_config::get().knife_model;
		stored_skin_knife_shot = c_config::get().knife_skin;
		stored_ssg_shit = c_config::get().ssg08_skin;
		stored_scar20_shit = c_config::get().scar20_skin;
		stored_g3sg1_shit = c_config::get().g3sg1_skin;
		stored_ak47_shit = c_config::get().ak47_skin;
		stored_deagle_shit = c_config::get().deagle_skin;
		stored_revolver_shit = c_config::get().r8_skin;
		stored_m4a1_shit = c_config::get().m4a4_skin;
		stored_m4a1s_shit = c_config::get().m4a1_s_skin;
		stored_cz75_shit = c_config::get().cz75_skin;
	}
	if (curStage == FRAME_NET_UPDATE_POSTDATAUPDATE_START)
	{
		gSkins.Run();

		if (Globals::LocalPlayer && Globals::LocalPlayer->IsAlive()) {
			backtracking->Update(g_pGlobalVars->tickcount);
		}
	}
	if (curStage == FRAME_NET_UPDATE_START)
	{
		if (g_pEngine->IsInGame() && g_pEngine->IsConnected())
		{
			if (c_config::get().visuals_bullet_tracers)
			{
			//	float Red, Green, Blue;
			//	Red = [0] * 255;
			//	Green = Vars.Visuals.Colors.BulletTracers[1] * 255;
			//	Blue = Vars.Visuals.Colors.BulletTracers[2] * 255;
				Color bullet_tracer_color = Color(c_config::get().bullet_tracers_color_r, c_config::get().bullet_tracers_color_g, c_config::get().bullet_tracers_color_b, c_config::get().bullet_tracers_color_a);
				for (unsigned int i = 0; i < trace_logs.size(); i++)
				{
					auto *shooter = g_pEntityList->GetClientEntity(g_pEngine->GetPlayerForUserID(trace_logs[i].userid));
					if (!shooter)
						return;
					DrawBeam(trace_logs[i].start, trace_logs[i].position, bullet_tracer_color);
					trace_logs.erase(trace_logs.begin() + i);
				}
			}
		}
	}

	oFrameStage(curStage);

}
#define LOAD_FROM_BUFFER "test"

void DesyncPercentageColor(float Percentage, Color& Color)
{
	Percentage = max(0, min(Percentage, 58));

	Color.red = min((510 * (58 - Percentage)) / 58, 255);
	Color.green = min((510 * Percentage) / 58, 255);
	Color.blue = 0;
	Color.alpha = 255;
}

#include <Windows.h>
#include <Psapi.h>
#include <lmcons.h>
#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <shlobj.h>
#include <time.h>
#include <random>
#include <sstream>
#include <fstream>
#include <shlwapi.h>
#include <iomanip>
#include <ctime>
#include <cstdint>

void Indicators() {
	if (Globals::LocalPlayer) {
		if (Globals::LocalPlayer->IsAlive()) {

			if (!Globals::LocalPlayer->AnimState())
				return;

			int screenSizeX, screenCenterX;
			int screenSizeY, screenCenterY;
			g_pEngine->GetScreenSize(screenSizeX, screenSizeY);



			/*
			for (int j = 0; j < 13; j++)
			{
				auto AnimShit = Globals::LocalPlayer->GetAnimOverlay4(j);

				std::string Gay;

				float Weight = AnimShit->m_flWeight;

				float Cycle = AnimShit->m_flCycle;

				Gay += std::to_string(j);
				Gay += " - ";
				Gay += std::to_string(Weight);
				Gay += ", ";
				Gay += std::to_string(Cycle);

				g_pSurface->DrawT(30, (screenSizeY / 2) + (j * 13), Color(255, 255, 255), Globals::CourierNew, false, Gay.c_str());
			}
			*/
			auto chocked_commands = *(int*)(uintptr_t(g_pClientState) + 0x4D28);

			RECT FakeSize = g_pSurface->GetTextSizeRect(Globals::CourierNew, "Fake");
			RECT CHOKESIZE = g_pSurface->GetTextSizeRect(Globals::CourierNew, "Choke");
			RECT ManualSize = g_pSurface->GetTextSizeRect(Globals::CourierNew, "Right");
			float DesyncWidth;
			if (Globals::DesyncEnabledSpecialChecks) {
				DesyncWidth = (((Globals::LocalPlayer->DesyncValue() * (FakeSize.right / -58.f))));
			}
			else {
				DesyncWidth = (((0 * (FakeSize.right / -58.f))));
			}
		

			float ChokedWidth;
			ChokedWidth = (((chocked_commands * (CHOKESIZE.right / 14.f))));



			bool Show_Manual = c_config::get().yaw_standing == 4 || c_config::get().yaw_moving == 4 || c_config::get().yaw_air == 4;

			screenCenterX = screenSizeX - 90;
			screenCenterY = 40;

			int FAKE_G = Globals::LocalPlayer->DesyncValue() / -58.f * 255.f;
			int FAKE_R = 255.f - FAKE_G;
			int FAKE_B = 0.f;

			int CHOKE_G = chocked_commands / 14.f * 255.f;
			int CHOKE_R = 255.f - CHOKE_G;
			int CHOKE_B = 0.f;

			static bool Show_Fake = true;
			static bool Show_Choke = true;
			
			int IndicatorPos = 0;

			if (Show_Manual) {
				Color theme = Color(c_config::get().menu_color_r, c_config::get().menu_color_g, c_config::get().menu_color_b);
				std::array< vec2_t, 3 >right_facing{ vec2_t(((screenSizeX / 2) + 32) - 8, screenSizeY / 2 - 8), vec2_t(((screenSizeX / 2) + 32) + 8, screenSizeY / 2), vec2_t(((screenSizeX / 2) + 32) - 8, screenSizeY / 2 + 8) };

				std::array< vec2_t, 3 >left_facing{ vec2_t(((screenSizeX / 2) - 31) + 8, screenSizeY / 2 + 8), vec2_t(((screenSizeX / 2) - 31) - 8, screenSizeY / 2),  vec2_t(((screenSizeX / 2) - 31) + 8, screenSizeY / 2 - 8) };
				g_pSurface->draw_filled_triangle(left_facing, Globals::Manual_Side ? theme : Color(125, 125, 125, 185));
				g_pSurface->PolygonOutline(3, left_facing, Color(0, 0, 0, 255));
				g_pSurface->draw_filled_triangle(right_facing, Globals::Manual_Side ? Color(125, 125, 125, 185) : theme);
				g_pSurface->PolygonOutline(3, left_facing, Color(0, 0, 0, 255));
			}

			if (Show_Choke) {
				g_pSurface->DrawT(screenCenterX - 5 - IndicatorPos + (CHOKESIZE.right / 2), screenCenterY - 14, Color(255, 255, 255, 255), Globals::CourierNew, true, "Choke");

				g_pSurface->FilledRect(screenCenterX - 5 - IndicatorPos, screenCenterY, CHOKESIZE.right, 4, Color(0, 0, 0, 185));
				g_pSurface->FilledRect(screenCenterX - 5 - IndicatorPos, screenCenterY, ChokedWidth, 4, Color(CHOKE_R, CHOKE_G, CHOKE_B, 255));
				g_pSurface->OutlinedRect(screenCenterX - 5 - IndicatorPos, screenCenterY, CHOKESIZE.right, 4, Color(0, 0, 0, 255));;

				IndicatorPos += CHOKESIZE.right;
			}

			if (Show_Fake) {
				g_pSurface->DrawT(screenCenterX - 5 - IndicatorPos + (FakeSize.right / 2), screenCenterY - 14, Color(255, 255, 255, 255), Globals::CourierNew, true, "Fake");

				g_pSurface->FilledRect(screenCenterX - 5 - IndicatorPos, screenCenterY, FakeSize.right, 4, Color(0, 0, 0, 185));
				g_pSurface->FilledRect(screenCenterX - 5 - IndicatorPos, screenCenterY, DesyncWidth, 4, Color(FAKE_R, FAKE_G, FAKE_B, 255));
				g_pSurface->OutlinedRect(screenCenterX - 5 - IndicatorPos, screenCenterY, FakeSize.right, 4, Color(0, 0, 0, 255));
				IndicatorPos += FakeSize.right;
			}

		}
	}
}

bool WorldToScreen223(const Vector& world, Vector& screen) {
	auto screen_transform = [&world, &screen]() -> bool {
		static auto &matrix = g_pEngine->WorldToScreenMatrix();

		screen.x = matrix[0][0] * world.x + matrix[0][1] * world.y + matrix[0][2] * world.z + matrix[0][3];
		screen.y = matrix[1][0] * world.x + matrix[1][1] * world.y + matrix[1][2] * world.z + matrix[1][3];
		screen.z = 0.0f;

		float w = matrix[3][0] * world.x + matrix[3][1] * world.y + matrix[3][2] * world.z + matrix[3][3];

		if (w < 0.001f) {
			screen.x *= 100000;
			screen.y *= 100000;
			return false;
		}

		screen.x /= w;
		screen.y /= w;

		return true;
	};

	if (screen_transform()) {
		int w, h;
		g_pEngine->GetScreenSize(w, h);

		screen.x = (w / 2.0f) + (screen.x * w) / 2.0f;
		screen.y = (h / 2.0f) - (screen.y * h) / 2.0f;

		return true;
	}

	return false;
}

void DrawTaserRange()
{
	Vector prev_scr_pos;
	Vector scr_pos;
	C_BaseCombatWeapon *local_weapon = Globals::LocalPlayer->GetActiveWeapon();
	if (!local_weapon || local_weapon->GetItemDefinitionIndex() != ItemDefinitionIndex::WEAPON_TASER)
		return;

	float step = M_PI * 2.0 / 2047; //adjust if you need 1-5 extra fps lol
	float rad = local_weapon->GetCSWpnData()->range;
	Vector origin = Globals::LocalPlayer->GetEyePosition();
	static float hue_offset = 0;
	for (float rotation = 0; rotation < (M_PI * 2.0); rotation += step)
	{
		Vector pos(rad * cos(rotation) + origin.x, rad * sin(rotation) + origin.y, origin.z);

		ray_t ray;
		trace_t trace;
		trace_filter  filter;

		filter.skip = Globals::LocalPlayer;
		ray.initialize(origin, pos);

		g_pTraceV2->trace_ray(ray, mask_shot_brushonly, &filter, &trace);

		if (Utils::WorldToScreen2(trace.end, scr_pos))
		{
			if (prev_scr_pos.IsValidV2())
			{
				int hue = RAD2DEG(rotation) + hue_offset;
				Color color = Color(255, 255, 255, 255);

				g_pSurface->Line(prev_scr_pos.x, prev_scr_pos.y, scr_pos.x, scr_pos.y, Color(255, 0, 0, 255));
			}
			prev_scr_pos = scr_pos;
		}
	}
	hue_offset += 0.25;
}
bool ExplectedWorldToScreanv2(Vector vOrigin, Vector &vScreen)
{
	float w = Globals::w2s_matrix[3][0] * vOrigin.x + Globals::w2s_matrix[3][1] * vOrigin.y + Globals::w2s_matrix[3][2] * vOrigin.z + Globals::w2s_matrix[3][3];
	int x, y;
	g_pEngine->GetScreenSize(x, y);
	float ScreenWidth = (float)x;
	float ScreenHeight = (float)y;

	if (w > 0.01f)
	{
		float inverseWidth = 1.f / w;
		vScreen.x = (float)((ScreenWidth / 2) + (0.5 * ((Globals::w2s_matrix[0][0] * vOrigin.x + Globals::w2s_matrix[0][1] * vOrigin.y + Globals::w2s_matrix[0][2] * vOrigin.z + Globals::w2s_matrix[0][3]) * inverseWidth) * ScreenWidth + 0.5));
		vScreen.y = (float)((ScreenHeight / 2) - (0.5 * ((Globals::w2s_matrix[1][0] * vOrigin.x + Globals::w2s_matrix[1][1] * vOrigin.y + Globals::w2s_matrix[1][2] * vOrigin.z + Globals::w2s_matrix[1][3]) * inverseWidth) * ScreenHeight + 0.5));
		return true;
	}

	return false;
}
void DrawLinev2(int x1, int y1, int x2, int y2, Color color)
{
	g_pSurface->DrawSetColor(color);
	g_pSurface->Line(x1, y1, x2, y2, color);
}
void DrawFilled3DBox(Vector origin, int width, int height, Color col)
{
	float difw = float(width / 2);
	float difh = float(height / 2);
	Vector boxVectors[8] =
	{
		Vector(origin.x - difw, origin.y - difh, origin.z - difw),
		Vector(origin.x - difw, origin.y - difh, origin.z + difw),
		Vector(origin.x + difw, origin.y - difh, origin.z + difw),
		Vector(origin.x + difw, origin.y - difh, origin.z - difw),
		Vector(origin.x - difw, origin.y + difh, origin.z - difw),
		Vector(origin.x - difw, origin.y + difh, origin.z + difw),
		Vector(origin.x + difw, origin.y + difh, origin.z + difw),
		Vector(origin.x + difw, origin.y + difh, origin.z - difw),
	};

	static Vector vec0, vec1, vec2, vec3,
		vec4, vec5, vec6, vec7;

	if (ExplectedWorldToScreanv2(boxVectors[0], vec0) &&
		ExplectedWorldToScreanv2(boxVectors[1], vec1) &&
		ExplectedWorldToScreanv2(boxVectors[2], vec2) &&
		ExplectedWorldToScreanv2(boxVectors[3], vec3) &&
		ExplectedWorldToScreanv2(boxVectors[4], vec4) &&
		ExplectedWorldToScreanv2(boxVectors[5], vec5) &&
		ExplectedWorldToScreanv2(boxVectors[6], vec6) &&
		ExplectedWorldToScreanv2(boxVectors[7], vec7))
	{
		Vector lines[12][2];
		lines[0][0] = vec0;
		lines[0][1] = vec1;
		lines[1][0] = vec1;
		lines[1][1] = vec2;
		lines[2][0] = vec2;
		lines[2][1] = vec3;
		lines[3][0] = vec3;
		lines[3][1] = vec0;

		// top of box
		lines[4][0] = vec4;
		lines[4][1] = vec5;
		lines[5][0] = vec5;
		lines[5][1] = vec6;
		lines[6][0] = vec6;
		lines[6][1] = vec7;
		lines[7][0] = vec7;
		lines[7][1] = vec4;

		lines[8][0] = vec0;
		lines[8][1] = vec4;

		lines[9][0] = vec1;
		lines[9][1] = vec5;

		lines[10][0] = vec2;
		lines[10][1] = vec6;

		lines[11][0] = vec3;
		lines[11][1] = vec7;

		for (int i = 0; i < 12; i++)
			DrawLinev2(lines[i][0].x, lines[i][0].y, lines[i][1].x, lines[i][1].y, col);
	}
}

void draw_gay_circle(Color* p, size_t resolution, float alpha_power = 1.f)
{
	const auto max_radius = resolution / 2;
	const auto max_relative_alpha = pow(max_radius, alpha_power);
	for (auto i = 0; i < resolution; ++i)
		for (auto j = 0; j < resolution; ++j)
		{
			const auto cy = i - max_radius;
			const auto cx = j - max_radius;
			const auto radius = std::sqrt(cx * cx + cy * cy);
			p[i * resolution + j] = {
				0x00u,
				0x00u,
				0x00u,
				radius > max_radius
				? 0x00u
				: uint8_t(pow(radius, alpha_power) / max_relative_alpha * 0xFF)
			};
		}
}

void __fastcall Hooks::PaintTraverse(PVOID pPanels, int edx, unsigned int vguiPanel, bool forceRepaint, bool allowForce)
{
	static auto oPaintTraverse = g_Hooks.pPanelHook->GetOriginal<PaintTraverse_t>(vtable_indexes::paint);
	static unsigned int panelID = 0;
	if (c_config::get().remove_scope && c_config::get().visuals_enabled && !strcmp("HudZoom", g_pPanel->GetName(vguiPanel)))
		return;
	oPaintTraverse(pPanels, vguiPanel, forceRepaint, allowForce);

	if (!panelID) 
	{
		const auto panelName = g_pPanel->GetName(vguiPanel);
		if (strstr(panelName, "MatSystemTopPanel"))
			panelID = vguiPanel;
		g_Hooks.bInitializedDrawManager = true;
	}


	if (panelID == vguiPanel)
	{
		if (c_config::get().visuals_enabled) {
			c_visuals::get().draw_scope();
			c_visuals::get().DrawPlayers();
			c_event_logs::get().run();
			//LogEvents::Draw();
		}

		static bool stored_menu_open = false;

		if (stored_menu_open != g_Menu.menuOpened) { /// optimization $$
			g_InputSystem->EnableInput(!g_Menu.menuOpened);
			stored_menu_open = g_Menu.menuOpened;
		}

		g_Misc.SwapManual();

		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++)
			{
				Globals::w2s_matrix[i][j] = g_pEngine->WorldToScreenMatrix()[i][j];
			}
		}

		if (c_config::get().grenade_prediction) {
			GrenadePrediction::get().Paint();
		}


		int screenSizeX, screenCenterX;
		int screenSizeY, screenCenterY;
		auto lineSize = 8;
		g_pEngine->GetScreenSize(screenSizeX, screenSizeY);

		//std::array< vec2_t, 3 >points{ vec2_t(screenSizeX / 2 - 16, screenSizeY / 2 - 16),
		//vec2_t(screenSizeX / 2 + 16, screenSizeY / 2),
		//vec2_t(screenSizeX / 2 - 16, screenSizeY / 2 + 16) };


		screenCenterX = screenSizeX / 2;
		screenCenterY = screenSizeY / 2;

		/*if (g_Menu.menuOpened) {
			g_pSurface->FilledRect(0, 0, 50, 50, Color(0, 255, 0));
		}
		else {
			g_pSurface->FilledRect(0, 0, 50, 50, Color(255, 0, 0));
		}*/


		if (Hitmarkertime > 0) {
		

		

			Hitmarkertime = max(Hitmarkertime - 2, 0);

			g_pSurface->Line(screenCenterX + 6, screenCenterY + 6, screenCenterX + 3, screenCenterY + 3, Color::White(Hitmarkertime));// Bottom right to center
			g_pSurface->Line(screenCenterX - 6, screenCenterY + 6, screenCenterX - 3, screenCenterY + 3, Color::White(Hitmarkertime));//Bottom left to center
			g_pSurface->Line(screenCenterX + 6, screenCenterY - 6, screenCenterX + 3, screenCenterY - 3, Color::White(Hitmarkertime));//Top right to center
			g_pSurface->Line(screenCenterX - 6, screenCenterY - 6, screenCenterX - 3, screenCenterY - 3, Color::White(Hitmarkertime));//Top left to center
		}
		Vector ViewAngs;

		//g_nadepred.draw();
		if (g_pEngine->IsConnected() && g_pEngine->IsInGame()) {

			if (Globals::LocalPlayer && Globals::LocalPlayer->IsAlive()) {
				///DrawTaserRange();
				float Damage;

				if (!Globals::LocalPlayer->GetActiveWeapon())
					return;

				Vector angles; g_pEngine->GetViewAngles(angles);

				int mode = 0;
				if (c_config::get().penetration_crosshair && !Globals::LocalPlayer->IsKnifeorNade()) {
					if (g_Autowall.CanWallbang(Damage)) {
						if (mode == 2) {
							//DrawFilled3DBox(angles, 5, 5, Color(0, 255, 0, 255));

							
						}
						else {
							g_pSurface->OutlinedRect(screenCenterX - 2, screenCenterY - 2, 5, 5, Color::Green());//Circle
							g_pSurface->FilledRect(screenCenterX - 8, screenCenterY, 5, 1, Color::Green());//Left line
							g_pSurface->FilledRect(screenCenterX + 4, screenCenterY, 5, 1, Color::Green());//Right line
							g_pSurface->FilledRect(screenCenterX, screenCenterY - 8, 1, 5, Color::Green());//Top line
							g_pSurface->FilledRect(screenCenterX, screenCenterY + 4, 1, 5, Color::Green());//Bottom line
						}
					}
					else {
						if (mode == 2) {
							DrawFilled3DBox(angles, 5, 5, Color(255, 0, 0, 255));
						}
						else {
							g_pSurface->OutlinedRect(screenCenterX - 2, screenCenterY - 2, 5, 5, Color::Red());
							g_pSurface->FilledRect(screenCenterX - 8, screenCenterY, 5, 1, Color::Red());//Left line
							g_pSurface->FilledRect(screenCenterX + 4, screenCenterY, 5, 1, Color::Red());//Right line
							g_pSurface->FilledRect(screenCenterX, screenCenterY - 8, 1, 5, Color::Red());//Top line
							g_pSurface->FilledRect(screenCenterX, screenCenterY + 4, 1, 5, Color::Red());//Bottom line
						}
					}
				}
			}

			Indicators();
		}

		//g_pSurface->FilledRect(screenCenterX - 1, screenCenterY - 1, 4, 4, Color(0, 0, 0, 185));
		//g_pSurface->DrawT(30, 30, Color(255, 255, 255), 5, false, std::to_string(Globals::Manual_Side).c_str());
		//g_pSurface->Line(screenCenterX, screenCenterY, screenCenterX + 4, screenCenterY, Color(255, 0, 0, 255));
		//g_pSurface->Line(screenCenterX + 2, screenCenterY - 1, screenCenterX + 2, screenCenterY + 3, Color(255, 0, 0, 255));
		//g_pSurface->Line(screenCenterX - 1 + 2, screenCenterY - 1 - 1, screenCenterX - 1 + 2, screenCenterY - 1 + 2, Color(255, 0, 0, 255));
		if (Globals::LocalPlayer) {
			if (Globals::LocalPlayer->IsAlive()) {

				if (!Globals::LocalPlayer->AnimState())
					return;

				if (c_config::get().remove_flash)
					*Globals::LocalPlayer->m_flFlashMaxAlpha() = 0;
			}
		}

		g_Menu.Render();
	}
}

/*
	if (Globals::LocalPlayer) {
				if (Globals::LocalPlayer->IsAlive()) {

					if (!Globals::LocalPlayer->AnimState())
						return;

					if(c_config::get().remove_flash)
						*Globals::LocalPlayer->m_flFlashMaxAlpha() = 0;

					int subtraction_pos = 0;

					int FAKE_G = Globals::LocalPlayer->DesyncValue() / -58.f * 255.f;
					int FAKE_R = 255.f - FAKE_G;
					int FAKE_B = 0.f;

					int LAG_COMP_GREEN = (Globals::LocalPlayer->GetOrigin() - StoredLocalPlayer::StoredOrigin).LengthSqr() / 4096.f * 255.f;
					int LAG_COMP_RED = 255.f - LAG_COMP_GREEN;
					int LAG_COMP_BLUE = 0.f;
					auto chocked_commands = *(int*)(uintptr_t(g_pClientState) + 0x4D28);

					bool show_choked = true;

					bool show_fake = true;

					static int frames_on_ground = 0;




					auto GetFPS = []() -> int
					{
						static float m_Framerate = 0;
						m_Framerate = 0.9 * m_Framerate + (1.0 - 0.9) * g_pGlobalVars->absoluteframetime; // https://github.com/ValveSoftware/source-sdk-2013/blob/master/mp/src/game/client/vgui_netgraphpanel.cpp#L744

						return int(1.f / m_Framerate);
					};

					auto tickRate = 1.0f / g_pGlobalVars->intervalPerTick;


					if (GetFPS() < tickRate) {
						g_pSurface->DrawT(10, (screenSizeY - 75 - subtraction_pos), Color(255, 0, 0, 255), Globals::IndicatorFont, false, "FPS");
						subtraction_pos += 25;
					}

					std::string choke_meme;
					choke_meme += "choked: ";
					choke_meme += std::to_string(chocked_commands);

					if (show_choked) {
						g_pSurface->DrawT(10, (screenSizeY - 75 - subtraction_pos), Color(255, 255, 255, 255), Globals::IndicatorFont, false, choke_meme.c_str());
						subtraction_pos += 25;
					}

					if (Globals::LocalPlayer->GetFlags() & FL_ONGROUND) {
						frames_on_ground += 10;
					}

					if (!(Globals::LocalPlayer->GetFlags() & FL_ONGROUND) && Globals::LocalPlayer->GetVelocity().Length2D() > 75) {
						frames_on_ground = 0;
					}

					bool Breaking_Lag_Compensation = (Globals::LocalPlayer->GetOrigin() - StoredLocalPlayer::StoredOrigin).Length2DSqr() > (4096.f);

					if (!(Globals::LocalPlayer->GetFlags() & FL_ONGROUND) && Globals::LocalPlayer->GetVelocity().Length2D() > 75 || frames_on_ground < 100)
					{
						g_pSurface->DrawT(10, (screenSizeY - 75 - subtraction_pos), Breaking_Lag_Compensation ? Color(78, 177, 0, 255) : Color(255, 0, 0, 255), Globals::IndicatorFont, false, "LC");
						subtraction_pos += 25;
					}

					if (show_fake) {
						g_pSurface->DrawT(10, (screenSizeY - 75 - subtraction_pos), Color(FAKE_R, FAKE_G, FAKE_B, 255), Globals::IndicatorFont, false, "FAKE: %.0f", Globals::LocalPlayer->DesyncValue());
						subtraction_pos += 25;
					}

				}
			}
*/
template<class T2>
static T2* FindHudElement2(const char* name)
{
	static auto pThis = *reinterpret_cast<DWORD**>(Utils::FindSignature("client_panorama.dll", "B9 ? ? ? ? E8 ? ? ? ? 8B 5D 08") + 1);

	static auto find_hud_element = reinterpret_cast<DWORD(__thiscall*)(void*, const char*)>(Utils::FindSignature("client_panorama.dll", "55 8B EC 53 8B 5D 08 56 57 8B F9 33 F6 39 77 28"));
	return (T2*)find_hud_element(pThis, name);
}

float dot_product(const Vector& a, const Vector& b) {
	return (a.x * b.x
		+ a.y * b.y
		+ a.z * b.z);
}

static void CapsuleOverlay(C_BaseEntity* pPlayer, Color col, float duration)
{
	if (!pPlayer)
		return;

	studiohdr_t* pStudioModel = g_pModelInfo->GetStudiomodel((model_t*)pPlayer->GetModel());
	if (!pStudioModel)
		return;

	static matrix3x4_t pBoneToWorldOut[128];
	if (!pPlayer->SetupBones(pBoneToWorldOut, 128, 256, 0))
		return;

	mstudiohitboxset_t* pHitboxSet = pStudioModel->GetHitboxSet(0);
	if (!pHitboxSet)
		return;

	auto VectorTransform2 = [](const Vector in1, matrix3x4_t in2, Vector & out)
	{

		out[0] = dot_product(in1, Vector(in2[0][0], in2[0][1], in2[0][2])) + in2[0][3];
		out[1] = dot_product(in1, Vector(in2[1][0], in2[1][1], in2[1][2])) + in2[1][3];
		out[2] = dot_product(in1, Vector(in2[2][0], in2[2][1], in2[2][2])) + in2[2][3];
	};

	for (int i = 0; i < pHitboxSet->numhitboxes; i++)
	{
		mstudiobbox_t* pHitbox = pHitboxSet->GetHitbox(i);
		if (!pHitbox)
			continue;

		Vector vMin, vMax;
		VectorTransform2(pHitbox->min, pBoneToWorldOut[pHitbox->bone], vMin); //nullptr???
		VectorTransform2(pHitbox->max, pBoneToWorldOut[pHitbox->bone], vMax);

		if (pHitbox->radius > -1)
		{
			g_pIVDebugOverlay->AddCapsuleOverlay(vMin, vMax, pHitbox->radius, col.red, col.green, col.blue, col.alpha, duration);
		}
	}
}



void Event::FireGameEvent(IGameEvent* event)
{
	if (!event)
		return;

	if (!Globals::LocalPlayer)
		return;

	//LogEvents::FireGameEvent(event);
	auto name = event->GetName();
	if (!strcmp(name, "player_hurt")) {
		c_event_logs::get().event_player_hurt(event);
		auto attacker = g_pEntityList->GetClientEntity(g_pEngine->GetPlayerForUserID(event->GetInt("attacker")));
		if (!attacker)
			return;

		if (attacker != Globals::LocalPlayer)
			return;

		int index = g_pEngine->GetPlayerForUserID(event->GetInt("userid"));
		int hitgroup = event->GetInt("hitgroup");

		PlayerInfo_t pInfo;
		g_pEngine->GetPlayerInfo(index, &pInfo);

		Globals::Hit[index] = true;

		if (c_config::get().draw_hit_player) {
			C_BaseEntity* hittedplayer = (C_BaseEntity*)g_pEntityList->GetClientEntity(g_pEngine->GetPlayerForUserID(event->GetInt("userid")));

			if (hittedplayer && hittedplayer->EntIndex() > 0 && hittedplayer->EntIndex() < 64)
			{
				if (Globals::LocalPlayer && hittedplayer != Globals::LocalPlayer)
					CapsuleOverlay(hittedplayer, Color(c_config::get().draw_hit_player_esp_color_r, c_config::get().draw_hit_player_esp_color_g, c_config::get().draw_hit_player_esp_color_b, 255), c_config::get().hit_player_duration);
			}
		}

		if (c_config::get().hitmarker) {

			//if (hitgroup == 1) {
			//	g_pEngine->ExecuteClientCmd("play commander\\train_bodydamageheadshot_01b");
			//}
			//else {
			switch (c_config::get().hitmarker_sound) {
			case 1: g_pSurface->PlaySoundA("buttons\\arena_switch_press_02.wav"); break;
			case 2: g_pSurface->PlaySoundA("hitmarker.wav"); break;
			}
				
			//}


			Hitmarkertime = 255;
			HitmarkerExpansion = 30 * 3;
		}
	}
	else if (!strcmp(name, "player_death")) {
		c_event_logs::get().event_player_death(event);
		if (!g_pEngine->IsConnected() || !g_pEngine->IsConnected())
			return;

		if (!Globals::LocalPlayer)
			return;

		auto attacker = g_pEntityList->GetClientEntity(g_pEngine->GetPlayerForUserID(event->GetInt("attacker")));
		if (!attacker)
			return;

		if (attacker != Globals::LocalPlayer)
			return;

		int index = g_pEngine->GetPlayerForUserID(event->GetInt("userid"));

		if (!index)
			return;

		static DWORD* deathNotice = FindHudElement2<DWORD>("CCSGO_HudDeathNotice");

		static DWORD* _death_notice = FindHudElement2<DWORD>("CCSGO_HudDeathNotice");
		static void(__thiscall *_clear_notices)(DWORD) = (void(__thiscall*)(DWORD))Utils::FindSignature("client_panorama.dll", "55 8B EC 83 EC 0C 53 56 8B 71 58");


		if (_death_notice)
			*(float*)((DWORD)_death_notice + 0x50) = c_config::get().visual_preserve_killfeed ? 500 : 1.5;

		//g_pEngine->ClientCmd("say Owned by xy0, buy it today at geminisoftware.us!");
	}
	else if (strstr(event->GetName(), "round_start"))
	{
		if (!g_pEngine->IsConnected() || !g_pEngine->IsConnected())
			return;

		if (!Globals::LocalPlayer)
			return;

		static DWORD* deathNotice = FindHudElement2<DWORD>("CCSGO_HudDeathNotice");

		static DWORD* _death_notice = FindHudElement2<DWORD>("CCSGO_HudDeathNotice");
		static void(__thiscall *_clear_notices)(DWORD) = (void(__thiscall*)(DWORD))Utils::FindSignature("client_panorama.dll", "55 8B EC 83 EC 0C 53 56 8B 71 58");

		_death_notice = FindHudElement2<DWORD>("CCSGO_HudDeathNotice");
		_clear_notices(((DWORD)_death_notice - 20));
	}
	else if (strstr(event->GetName(), "item_purchase")) {
		c_event_logs::get().event_item_purchase(event);
	}
	else if (strstr(event->GetName(), "bullet_impact"))
	{
		if (c_config::get().visuals_bullet_tracers)
		{
			auto* index = g_pEntityList->GetClientEntity(g_pEngine->GetPlayerForUserID(event->GetInt("userid")));

			if (Globals::LocalPlayer)
			{
				Vector position(event->GetFloat("x"), event->GetFloat("y"), event->GetFloat("z"));

				if (index)
					trace_logs.push_back(trace_info(index->GetEyePosition(), position, g_pGlobalVars->curtime, event->GetInt("userid")));
			}
		}
	}
};

void GetViewModelFOV2(float& fov)
{

	if (g_pEngine->IsConnected() && g_pEngine->IsInGame())
	{

		if (!Globals::LocalPlayer)
			return;

		if (c_config::get().viewmodel_fov != 0) {
			fov = c_config::get().viewmodel_fov;
		}
		else {
			fov = 68;
		}
			
	}
}

float __stdcall  Hooks::GetViewmodelFOV()
{
	float fov = g_Hooks.pClientModeHook->GetOriginal<oGetViewModelFOV>(35)();

	GetViewModelFOV2(fov);

	return fov;
}

void __fastcall Hooks::OverrideView(void* ecx, void* edx, CViewSetup* pSetup)
{
	static auto oOverrideView = g_Hooks.pClientModeHook->GetOriginal<OverrideView_t>(vtable_indexes::view);

	if (g_pEngine->IsConnected() && g_pEngine->IsInGame())
	{
		if (!Globals::LocalPlayer)
			return;

		if (!Globals::LocalPlayer->IsAlive())
			return;

		if (c_config::get().grenade_prediction) {
			GrenadePrediction::get().View(pSetup);
		}

		g_Misc.ThirdPerson();

		if (c_config::get().fov != 0 && !Globals::LocalPlayer->IsScoped())
			pSetup->fov = c_config::get().fov;

		if (Globals::LocalPlayer->IsScoped() && c_config::get().fov_while_zoomed) {
			pSetup->fov = c_config::get().fov;
		}


		if (c_config::get().remove_visual_recoil)
		{
			Vector viewPunch = Globals::LocalPlayer->GetViewPunchAngle();
			Vector aimPunch = Globals::LocalPlayer->GetAimPunchAngle();

			pSetup->angles[0] -= (viewPunch[0] + (aimPunch[0] * 2 * 0.4499999f));
			pSetup->angles[1] -= (viewPunch[1] + (aimPunch[1] * 2 * 0.4499999f));
			pSetup->angles[2] -= (viewPunch[2] + (aimPunch[2] * 2 * 0.4499999f));
		}
	}

	oOverrideView(ecx, edx, pSetup);
}

void __fastcall Hooks::LockCursor(ISurface* thisptr, void* edx)
{
    static auto oLockCursor = g_Hooks.pSurfaceHook->GetOriginal<LockCursor_t>(vtable_indexes::lockCursor);

    if (!g_Menu.menuOpened)
        return oLockCursor(thisptr, edx);

    g_pSurface->UnLockCursor();
}
extern LRESULT ImGui_ImplDX9_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT Hooks::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // for now as a lambda, to be transfered somewhere
    // Thanks uc/WasserEsser for pointing out my mistake!
    // Working when you HOLD th button, not when you press it.
    const auto getButtonHeld = [uMsg, wParam](bool& bButton, int vKey)
    {
		if (wParam != vKey) return;

        if (uMsg == WM_KEYDOWN)
            bButton = true;
        else if (uMsg == WM_KEYUP)
            bButton = false;
    };

	const auto getButtonToggle = [uMsg, wParam](bool& bButton, int vKey)
	{
		if (wParam != vKey) return;

		if (uMsg == WM_KEYUP)
			bButton = !bButton;
	};

	switch (uMsg)
	{
	case WM_LBUTTONDOWN:
		Globals::PressedKeys[VK_LBUTTON] = true;
		break;
	case WM_LBUTTONUP:
		Globals::PressedKeys[VK_LBUTTON] = false;
		break;
	case WM_RBUTTONDOWN:
		Globals::PressedKeys[VK_RBUTTON] = true;
		break;
	case WM_RBUTTONUP:
		Globals::PressedKeys[VK_RBUTTON] = false;
		break;
	case WM_MBUTTONDOWN:
		Globals::PressedKeys[VK_MBUTTON] = true;
		break;
	case WM_MBUTTONUP:
		Globals::PressedKeys[VK_MBUTTON] = false;
		break;
	case WM_XBUTTONDOWN:
	{
		UINT button = GET_XBUTTON_WPARAM(wParam);
		if (button == XBUTTON1)
		{
			Globals::PressedKeys[VK_XBUTTON1] = true;
		}
		else if (button == XBUTTON2)
		{
			Globals::PressedKeys[VK_XBUTTON2] = true;
		}
		break;
	}
	case WM_XBUTTONUP:
	{
		UINT button = GET_XBUTTON_WPARAM(wParam);
		if (button == XBUTTON1)
		{
			Globals::PressedKeys[VK_XBUTTON1] = false;
		}
		else if (button == XBUTTON2)
		{
			Globals::PressedKeys[VK_XBUTTON2] = false;
		}
		break;
	}
	case WM_KEYDOWN:
		Globals::PressedKeys[wParam] = true;
		break;
	case WM_KEYUP:
		Globals::PressedKeys[wParam] = false;
		break;
	case WM_SYSKEYDOWN:
		if (wParam == VK_MENU)
			Globals::PressedKeys[VK_MENU] = true;
		break;
	case WM_SYSKEYUP:
		if (wParam == VK_MENU)
			Globals::PressedKeys[VK_MENU] = false;
		break;
	default: break;
	}


	g_Menu.HandleMenuVisibility();
	if (g_Menu.D3DInit && g_Menu.menuOpened && ImGui_ImplDX9_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;


    // Call original wndproc to make game use input again
    return CallWindowProcA(g_Hooks.pOriginalWNDProc, hWnd, uMsg, wParam, lParam);
}
#include "Features/Aimbot/LagComp.h"
static auto linegoesthrusmoke = Utils::FindPattern("client_panorama.dll", (PBYTE)"\x55\x8B\xEC\x83\xEC\x08\x8B\x15\x00\x00\x00\x00\x0F\x57\xC0", "xxxxxxxx????xxx");

IMaterial *create_material(bool shade, bool wireframe, bool ignorez, int rimlight_boost = 0) {
	static const std::string material_name = "game_material.vmt";
	const std::string material_type = shade ? "VertexLitGeneric" : "UnlitGeneric";

	std::string material_data;

	material_data += "\"" + material_type + "\"\n{\n";
	material_data += "\t\"$basetexture\" \"vgui/white_additive\"\n";
	material_data += "\t\"$envmap\" \"\"\n";
	material_data += "\t\"$model\" \"1\"\n";
	material_data += "\t\"$flat\" \"1\"\n";
	material_data += "\t\"$nocull\" \"0\"\n";
	material_data += "\t\"$selfillum\" \"1\"\n";
	material_data += "\t\"$halflambert\" \"1\"\n";
	material_data += "\t\"$znearer\" \"0\"\n";
	material_data += "\t\"$rimlightboost\" \"" + std::to_string(rimlight_boost) + "\"\n";
	material_data += "\t\"$rimlightexponent\" \"" + std::to_string(4) + "\"\n";
	material_data += "\t\"$ambientreflectionboost\" \"0.2\"\n";
	material_data += "\t\"$envmaplightscale\" \"" + std::to_string(0.1) + "\"\n";
	material_data += "\t\"$wireframe\" \"" + std::to_string(wireframe) + "\"\n";
	material_data += "\t\"$ignorez\" \"" + std::to_string(ignorez) + "\"\n";
	material_data += "}\n";

	auto init_key_values = [](KeyValuesv2 *keyvalues, const char *key_name) -> void {
		using InitKeyValues_t = void(__thiscall *)(void *, const char *);
		static InitKeyValues_t InitKeyValuesEx = nullptr;
		static auto pThis = *reinterpret_cast<DWORD**>(Utils::FindSignature("client_panorama.dll", "B9 ? ? ? ? E8 ? ? ? ? 8B 5D 08") + 1);
		if (!InitKeyValuesEx)
			InitKeyValuesEx = *reinterpret_cast<InitKeyValues_t>(Utils::FindSignature("client_panorama.dll", "55 8B EC 51 33 C0 C7 45"));

		InitKeyValuesEx(keyvalues, key_name);
	};
	auto load_from_buffer = [](KeyValuesv2 *key_values, const char *resource_name, const char *buf, void *file_sys = nullptr, const char *path_id = nullptr, void *eval_sym_proc = nullptr, void *unk = nullptr) -> void {
		using LoadFromBuffer_t = void(__thiscall *)(void *, const char *, const char *, void *, const char *, void *, void *);
		static LoadFromBuffer_t LoadFromBufferEx = nullptr;

		if (!LoadFromBufferEx)
			LoadFromBufferEx = *reinterpret_cast<LoadFromBuffer_t>(Utils::FindSignature("client_panorama.dll", "55 8B EC 83 E4 F8 83 EC 34 53 8B 5D 0C 89"));

		LoadFromBufferEx(key_values, resource_name, buf, file_sys, path_id, eval_sym_proc, unk);
	};

	auto *key_values = new KeyValuesv2();
	init_key_values(key_values, material_type.c_str());
	load_from_buffer(key_values, material_name.c_str(), material_data.c_str());

	return g_pMaterialSys->CreateMaterial(material_name.c_str(), key_values);
}
void __fastcall Hooks::SceneEnd(void *ecx, void *edx)
{
	static auto oSceneEnd = g_Hooks.pRenderViewHook->GetOriginal<SceneEnd_t>(vtable_indexes::sceneEnd);
	oSceneEnd(ecx, edx);

	

	for (int i = 1; i < g_pEngine->GetMaxClients(); ++i)
	{
		C_BaseEntity* pPlayerEntity = g_pEntityList->GetClientEntity(i);

		if (!pPlayerEntity
			|| !pPlayerEntity->IsAlive()
			|| pPlayerEntity->IsDormant()
			|| !Globals::LocalPlayer)
			continue;

		bool IsTeammate = pPlayerEntity->GetTeam() == Globals::LocalPlayer->GetTeam();
		bool IsEnemy = pPlayerEntity->GetTeam() != Globals::LocalPlayer->GetTeam();
		bool IsLocal = pPlayerEntity == Globals::LocalPlayer;


		static std::vector< IMaterial * > materials;

		static bool once{ false };
		if (!once) {
			materials.push_back(create_material(true, false, false)); // material.
			materials.push_back(create_material(false, false, false, 0)); // flat.
			materials.push_back(create_material(true, false, true)); // material zignore.
			materials.push_back(create_material(false, false, true)); // flat zignore.
			once = true;
		}

		/*
		if (Globals::LocalPlayer) {
			float NormalColor[3] = { 1, 1, 1 };
			g_pRenderView->SetColorModulation(NormalColor);
			g_pModelRender->ForcedMaterialOverride(materials.at(0));
			Globals::LocalPlayer->DrawModel(1, 255);
			g_pModelRender->ForcedMaterialOverride(nullptr);
		}
		*/
		float vis_color[3] = { c_config::get().chams_esp_color_r / 255.f, c_config::get().chams_esp_color_g / 255.f, c_config::get().chams_esp_color_b / 255.f };
		float hid_color[3] = { c_config::get().chams_xqz_esp_color_r / 255.f, c_config::get().chams_xqz_esp_color_g / 255.f, c_config::get().chams_xqz_esp_color_b / 255.f };
		float local_color[3] = { c_config::get().local_chams_color_r / 255.f, c_config::get().local_chams_color_g / 255.f, c_config::get().local_chams_color_b / 255.f };

		if (IsEnemy) {
			if (c_config::get().chams_enemies) {
				if (c_config::get().chams_xqz_enemies) {
					g_pRenderView->SetBlend(c_config::get().chams_esp_color_a / 100.f);
					g_pModelRender->ForcedMaterialOverride(materials.at(2));
					g_pRenderView->SetColorModulation(hid_color);
					pPlayerEntity->DrawModel(0x00000001, 255);
				}

				g_pRenderView->SetBlend(c_config::get().chams_xqz_esp_color_a / 100.f);
				g_pModelRender->ForcedMaterialOverride(materials.at(0));
				g_pRenderView->SetColorModulation(vis_color);
				pPlayerEntity->DrawModel(0x00000001, 255);
			}
		}
		else if (IsLocal && !Globals::LocalPlayer->IsScoped() && c_config::get().local_chams) {
			g_pRenderView->SetBlend(c_config::get().local_chams_color_a / 100.f);
			g_pModelRender->ForcedMaterialOverride(materials.at(0));
			g_pRenderView->SetColorModulation(local_color);
			pPlayerEntity->DrawModel(0x00000001, 255);
		}

		g_pModelRender->ForcedMaterialOverride(nullptr);
	}


	for (auto i = 0; i < g_GlowManager->GetSize(); i++)
	{
		auto &glowObject = g_GlowManager->m_GlowObjectDefinitions[i];
		auto entity = reinterpret_cast<C_BaseEntity*>(glowObject.m_pEntity);
		auto m_pLocalPlayer = reinterpret_cast<C_BaseEntity*>(g_pEntityList->GetClientEntity(g_pEngine->GetLocalPlayer()));

		if (!entity)
			continue;

		if (!m_pLocalPlayer)
			continue;

		if (glowObject.IsUnused())
			continue;

		bool is_local_player = entity == m_pLocalPlayer;
		bool is_teammate = m_pLocalPlayer->GetTeam() == entity->GetTeam() && !is_local_player;
		bool IsEnemy = entity->GetTeam() != m_pLocalPlayer->GetTeam();

		if (is_local_player) {
			if (c_config::get().local_glow && !m_pLocalPlayer->IsScoped()) {
				auto class_id = entity->GetClientClass()->ClassID;


				switch (class_id)
				{
				default:
					glowObject.m_flAlpha = 0.0f;
					break;
				case 40:
					glowObject.m_nGlowStyle = 0;
					glowObject.m_flAlpha = c_config::get().local_glow_color_a / 255.f;
					break;
				}

				glowObject.m_flRed = c_config::get().local_glow_color_r / 255.0f;
				glowObject.m_flGreen = c_config::get().local_glow_color_g / 255.0f;
				glowObject.m_flBlue = c_config::get().local_glow_color_b / 255.0f;
				glowObject.m_bRenderWhenOccluded = true;
				glowObject.m_bRenderWhenUnoccluded = false;
			}
		}
		else if (!is_teammate) {
			if (c_config::get().glow_enemies) {
				auto class_id = entity->GetClientClass()->ClassID;


				switch (class_id)
				{
				default:
					glowObject.m_flAlpha = 0.0f;
					break;
				case 40:
					glowObject.m_nGlowStyle = 0;
					glowObject.m_flAlpha = c_config::get().glow_esp_color_a / 255.f;
					break;
				}

				glowObject.m_flRed = c_config::get().glow_esp_color_r / 255.0f;
				glowObject.m_flGreen = c_config::get().glow_esp_color_g / 255.0f;
				glowObject.m_flBlue = c_config::get().glow_esp_color_b / 255.0f;
				glowObject.m_bRenderWhenOccluded = true;
				glowObject.m_bRenderWhenUnoccluded = false;
			}
		}
		else if (is_teammate) {
			if (c_config::get().glow_teammates) {
				auto class_id = entity->GetClientClass()->ClassID;


				switch (class_id)
				{
				default:
					glowObject.m_flAlpha = 0.0f;
					break;
				case 40:
					glowObject.m_nGlowStyle = 0;
					glowObject.m_flAlpha = c_config::get().glow_alpha_teammates / 255.f;
					break;
				}

				glowObject.m_flRed = 180 / 255.0f;
				glowObject.m_flGreen = 60 / 255.0f;
				glowObject.m_flBlue = 120 / 255.0f;
				glowObject.m_bRenderWhenOccluded = true;
				glowObject.m_bRenderWhenUnoccluded = false;
			}
		}


		
	}
	if (c_config::get().no_smoke) {
		std::vector<const char*> vistasmoke_wireframe =
		{
			"particle/vistasmokev1/vistasmokev1_smokegrenade",
		};

		std::vector<const char*> vistasmoke_nodraw =
		{
			"particle/vistasmokev1/vistasmokev1_fire",
			"particle/vistasmokev1/vistasmokev1_emods",
			"particle/vistasmokev1/vistasmokev1_emods_impactdust",
		};

		for (auto mat_s : vistasmoke_wireframe)
		{
			IMaterial* mat = g_pMaterialSys->FindMaterial(mat_s, "Other textures");
			mat->SetMaterialVarFlag(MATERIAL_VAR_WIREFRAME, true); //wireframe
		}

		for (auto mat_n : vistasmoke_nodraw)
		{
			IMaterial* mat = g_pMaterialSys->FindMaterial(mat_n, "Other textures");
			mat->SetMaterialVarFlag(MATERIAL_VAR_NO_DRAW, true);
		}

		static auto smokecout = *(DWORD*)(linegoesthrusmoke + 0x8);
		*(int*)(smokecout) = 0;
	}

}
inline bool isHand(std::string& modelName) {
	if ((modelName.find("arms") != std::string::npos || modelName.find("v_models") != std::string::npos) && !(modelName.find("uid") != std::string::npos || modelName.find("stattrack") != std::string::npos)) {
		return true;
	}

	return false;
}
void angleMatrixv2(const Vector angles, matrix3x4_t& matrix)
{
	float sr, sp, sy, cr, cp, cy;

	sy = sin(DEG2RAD(angles[1]));
	cy = cos(DEG2RAD(angles[1]));

	sp = sin(DEG2RAD(angles[0]));
	cp = cos(DEG2RAD(angles[0]));

	sr = sin(DEG2RAD(angles[2]));
	cr = cos(DEG2RAD(angles[2]));

	//matrix = (YAW * PITCH) * ROLL
	matrix[0][0] = cp * cy;
	matrix[1][0] = cp * sy;
	matrix[2][0] = -sp;

	float crcy = cr * cy;
	float crsy = cr * sy;
	float srcy = sr * cy;
	float srsy = sr * sy;

	matrix[0][1] = sp * srcy - crsy;
	matrix[1][1] = sp * srsy + crcy;
	matrix[2][1] = sr * cp;

	matrix[0][2] = (sp * crcy + srsy);
	matrix[1][2] = (sp * crsy - srcy);
	matrix[2][2] = cr * cp;

	matrix[0][3] = 0.0f;
	matrix[1][3] = 0.0f;
	matrix[2][3] = 0.0f;
}
void angleMatrix(const Vector angles, matrix3x4_t& matrix)
{
	float sr, sp, sy, cr, cp, cy;

	sy = sin(DEG2RAD(angles[1]));
	cy = cos(DEG2RAD(angles[1]));

	sp = sin(DEG2RAD(angles[0]));
	cp = cos(DEG2RAD(angles[0]));

	sr = sin(DEG2RAD(angles[2]));
	cr = cos(DEG2RAD(angles[2]));

	//matrix = (YAW * PITCH) * ROLL
	matrix[0][0] = cp * cy;
	matrix[1][0] = cp * sy;
	matrix[2][0] = -sp;

	float crcy = cr * cy;
	float crsy = cr * sy;
	float srcy = sr * cy;
	float srsy = sr * sy;

	matrix[0][1] = sp * srcy - crsy;
	matrix[1][1] = sp * srsy + crcy;
	matrix[2][1] = sr * cp;

	matrix[0][2] = (sp * crcy + srsy);
	matrix[1][2] = (sp * crsy - srcy);
	matrix[2][2] = cr * cp;

	matrix[0][3] = 0.0f;
	matrix[1][3] = 0.0f;
	matrix[2][3] = 0.0f;
}
FORCEINLINE float DotProductv2(const float *v1, const float *v2)
{
	return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}
void vectorRotate(const float *in1, const matrix3x4_t& in2, float *out)
{
	out[0] = DotProductv2(in1, in2[0]);
	out[1] = DotProductv2(in1, in2[1]);
	out[2] = DotProductv2(in1, in2[2]);
}

void vectorRotate(const Vector& in1, const matrix3x4_t &in2, Vector &out)
{
	vectorRotate(&in1.x, in2, &out.x);
}

void vectorRotate(const Vector &in1, const Vector &in2, Vector &out)
{
	matrix3x4_t matRotate;
	angleMatrix(in2, matRotate);
	vectorRotate(in1, matRotate, out);
}
void matrixCopy(const matrix3x4_t& source, matrix3x4_t& target)
{
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			target[i][j] = source[i][j];
		}
	}
}

void matrixMultiply(matrix3x4_t& in1, const matrix3x4_t& in2)
{
	matrix3x4_t out;
	if (&in1 == &out)
	{
		matrix3x4_t in1b;
		matrixCopy(in1, in1b);
		matrixMultiply(in1b, in2);
		return;
	}
	if (&in2 == &out)
	{
		matrix3x4_t in2b;
		matrixCopy(in2, in2b);
		matrixMultiply(in1, in2b);
		return;
	}
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
		in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
		in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
		in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
		in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
		in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
		in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
		in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
		in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
		in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
		in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
		in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
		in1[2][2] * in2[2][3] + in1[2][3];

	in1 = out;
}

void __fastcall Hooks::DrawModelExecute(void* ecx, void* edx, IMatRenderContext* context, const DrawModelState_t& state, const ModelRenderInfo_t& info, matrix3x4_t* matrix)
{
	static auto oDrawModelExecute = g_Hooks.pModelHook->GetOriginal<DrawModelExecute_t>(vtable_indexes::dme);

	const char* ModelName = g_pModelInfo->GetModelName((model_t*)info.pModel);

	C_BaseEntity* pPlayerEntity = g_pEntityList->GetClientEntity(info.index);


	if (pPlayerEntity && pPlayerEntity->IsAlive() && !pPlayerEntity->IsDormant() && strstr(ModelName, "models/player") && Globals::LocalPlayer && pPlayerEntity == Globals::LocalPlayer) {
		if (Globals::LocalPlayer->IsScoped())
		{
			g_pRenderView->SetBlend(0.4f);
		}
	}

	static std::vector< IMaterial * > materials;

	static bool once{ false };
	if (!once) {
		materials.push_back(create_material(true, false, false)); // material.
		materials.push_back(create_material(false, false, false, 0)); // flat.
		materials.push_back(create_material(true, false, true)); // material zignore.
		materials.push_back(create_material(false, false, true)); // flat zignore.
		once = true;
	}
	/*
			IMaterial *materials[32];
			studiohdr_t* hdr = g_pModelInfo->GetStudiomodel(info.pModel);
			g_pModelInfo->GetModelMaterials(info.pModel, hdr->numtextures, materials);

			for (int i = 0; i < hdr->numtextures; i++)
			{
				IMaterial* mathands = materials[i];
					if (!mathands)
						continue;

					mathands->SetMaterialVarFlag(MATERIAL_VAR_WIREFRAME, c_config::get().wireframe_arms);
			}
	*/
	float hand_chams[3] = { c_config::get().hand_chams_color_r / 255.f, c_config::get().hand_chams_color_g / 255.f, c_config::get().hand_chams_color_b / 255.f };

	auto HandChams = c_config::get().hand_chams;

	if (strstr(ModelName, "arms") && HandChams > 0) {

		if (HandChams == 1) {
			g_pRenderView->SetBlend(0.0);
		}
		else if (HandChams == 2) {
			g_pRenderView->SetBlend(c_config::get().hand_chams_color_a / 100.f);
			g_pRenderView->SetColorModulation(hand_chams);
			g_pModelRender->ForcedMaterialOverride(materials.at(0));
		}
	}

	oDrawModelExecute(ecx, context, state, info, matrix);
}


void Hooks::HookPlayers()
{
	static bool Init[65];
	static bool Hooked[65];

	for (int i = 1; i < g_pEngine->GetMaxClients(); ++i)
	{
		C_BaseEntity* pPlayerEntity = g_pEntityList->GetClientEntity(i);

		if (!pPlayerEntity
			|| !pPlayerEntity->IsAlive()
			|| pPlayerEntity->IsDormant())
		{
			if (Hooked[i])
				g_Hooks.pPlayerHook[i]->Unhook(vtable_indexes::extraBonePro);

			Hooked[i] = false;
			continue;
		}

		if (!Init[i])
		{
			g_Hooks.pPlayerHook[i] = std::make_unique<ShadowVTManager>();
			Init[i] = true;
		}

		if (Hooked[i])
			g_Hooks.pPlayerHook[i]->Unhook(vtable_indexes::extraBonePro);

		if (!Hooked[i])
		{
			g_Hooks.pPlayerHook[i]->Setup(pPlayerEntity);
			g_Hooks.pPlayerHook[i]->Hook(vtable_indexes::extraBonePro, Hooks::DoExtraBonesProcessing);

			Hooked[i] = true;
		}
	}
}


void __fastcall Hooks::DoExtraBonesProcessing(void * ECX, void * EDX, void * unkn1, void * unkn2, void * unkn3, void * unkn4, CBoneBitList & unkn5, void * unkn6)
{
	C_BaseEntity* pPlayerEntity = (C_BaseEntity*)ECX;

	if (!pPlayerEntity || pPlayerEntity == nullptr)
		return;

	if (!pPlayerEntity->IsAlive() || pPlayerEntity->IsDormant())
		return;

	if (!pPlayerEntity->AnimState())
		return;

	auto oDoExtraBonesProcessing = g_Hooks.pPlayerHook[pPlayerEntity->EntIndex()]->GetOriginal<ExtraBoneProcess_t>(vtable_indexes::extraBonePro);

	float Backup = pPlayerEntity->AnimState()->m_flStopToFullRunningFraction;
	pPlayerEntity->AnimState()->m_flStopToFullRunningFraction = 0;

	oDoExtraBonesProcessing(ECX, unkn1, unkn2, unkn3, unkn4, unkn5, unkn6);

	pPlayerEntity->AnimState()->m_flStopToFullRunningFraction = Backup;
}
