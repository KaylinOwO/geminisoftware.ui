#include "Skinchanger.h"

Skinchanger gSkins;

int Skinchanger::getSkin(int shit_to_switch)
{
	switch (shit_to_switch)
	{
	case 0:
		return 0;
		break;
	case 1:
		return 3;
		break;
	case 2:
		return 11;
		break;
	case 3:
		return 15;
		break;
	case 4:
		return 39;
		break;
	case 5:
		return 70;
		break;
	case 6:
		return 98;
		break;
	case 7:
		return 156;
		break;
	case 8:
		return 196;
		break;
	case 9:
		return 400;
		break;
	case 10:
		return 419;
		break;
	case 11:
		return 427;
		break;
	case 12:
		return 451;
		break;
	case 13:
		return 572;
		break;
	case 14:
		return 702;
		break;
	case 15:
		return 102;
		break;
	case 16:
		return 312;
		break;
	default:
		return 0;
		break;
	}
}

int Skinchanger::getKnifeItemDefinitionIndex()
{
	int gay = 9;
	//

	if (!Globals::LocalPlayer)
		return 59;


	switch (c_config::get().knife_model)
	{
	case 0:
		return Globals::LocalPlayer->GetTeam() == 1 ? 42 : 26;
		break;

	case 1:
		return 508;
		break;

	case 2:
		return 500;
		break;

	case 3:
		return 505;
		break;

	case 4:
		return 506;
		break;

	case 5:
		return 507;
		break;

	case 6:
		return 509; // Huntsman
		break;

	case 7:
		return 512;
		break;

	case 8:
		return 514;
		break;

	case 9:
		return 515;
		break;
	case 10:
		return 516;
		break;
	case 11:
		return 520;
		break;
	case 12:
		return 522;
		break;
	case 13:
		return 519;
		break;
	default:
		return 523;
		break;
	}
}

const char* Skinchanger::getKnifeModel(bool viewmodel)
{
	int gay = 9;
	//

	if (!Globals::LocalPlayer)
		return "models/weapons/v_knife_default_t.mdl";


	switch (c_config::get().knife_model)
	{
	case 0:
		return viewmodel ? Globals::LocalPlayer->GetTeam() == 2 ? "models/weapons/v_knife_default_t.mdl" : "models/weapons/v_knife_default_ct.mdl" : Globals::LocalPlayer->GetTeam() == 2 ? "models/weapons/w_knife_default_t.mdl" : "models/weapons/w_knife_default_ct.mdl";
		break;

	case 1:
		return viewmodel ? "models/weapons/v_knife_m9_bay.mdl" : "models/weapons/w_knife_m9_bay.mdl";
		break;

	case 2:
		return viewmodel ? "models/weapons/v_knife_bayonet.mdl" : "models/weapons/w_knife_bayonet.mdl";
		break;

	case 3:
		return viewmodel ? "models/weapons/v_knife_flip.mdl" : "models/weapons/w_knife_flip.mdl";
		break;

	case 4:
		return viewmodel ? "models/weapons/v_knife_gut.mdl" : "models/weapons/w_knife_gut.mdl";
		break;

	case 5:
		return viewmodel ? "models/weapons/v_knife_karam.mdl" : "models/weapons/w_knife_karam.mdl";
		break;

	case 6:
		return viewmodel ? "models/weapons/v_knife_tactical.mdl" : "models/weapons/w_knife_tactical.mdl";
		break;

	case 7:
		return viewmodel ? "models/weapons/v_knife_falchion_advanced.mdl" : "models/weapons/w_knife_falchion_advanced.mdl";
		break;

	case 8:
		return viewmodel ? "models/weapons/v_knife_survival_bowie.mdl" : "models/weapons/w_knife_survival_bowie.mdl";
		break;
	case 9:
		return viewmodel ? "models/weapons/v_knife_butterfly.mdl" : "models/weapons/w_knife_butterfly.mdl";
		break;
	case 10:
		return viewmodel ? "models/weapons/v_knife_push.mdl" : "models/weapons/w_knife_push.mdl";
		break;
	case 11:
		return viewmodel ? "models/weapons/v_knife_gypsy_jackknife.mdl" : "models/weapons/w_knife_gypsy_jackknife.mdl";
		break;
	case 12:
		return viewmodel ? "models/weapons/v_knife_stiletto.mdl" : "models/weapons/w_knife_stiletto.mdl";
		break;
	case 13:
		return viewmodel ? "models/weapons/v_knife_ursus.mdl" : "models/weapons/w_knife_ursus.mdl";
		break;
	case 14:
		return viewmodel ? "models/weapons/v_knife_widowmaker.mdl" : "models/weapons/2_knife_widowmaker.mdl";
		break;
	default:
		return "";
		break;
	}
}

void Skinchanger::Run()
{
	if (!Globals::LocalPlayer)
		return;

	if (Globals::LocalPlayer->IsAlive())
	{
		int nLocalPlayerID = g_pEngine->GetLocalPlayer();
		C_BaseEntity* pLocal = (C_BaseEntity*)g_pEntityList->GetClientEntity(nLocalPlayerID);


		auto Weapons = Globals::LocalPlayer->GetWeapons();

		for (int i = 0; i < 64; i++) {
			if (Weapons[i] == -1)
				continue;

			C_BaseCombatWeapon* Weap = (C_BaseCombatWeapon*)g_pEntityList->GetClientEntityFromHandle(Weapons[i]);

			if (!Weap || Weap == nullptr)
				continue;


			ClientClass *pClass = g_pClientDll->GetAllClasses();

			auto world_model_handle = Weap->m_hWeaponWorldModel_h();

			const auto world_model = (CBaseWeaponWorldModel*)(g_pEntityList->GetClientEntityFromHandle(world_model_handle));


			if (Weap->GetClientClass()->ClassID == (int)EClassIds::CKnife && c_config::get().knife_model > 0)
			{
				*Weap->ItemDefinitionIndex2() = getKnifeItemDefinitionIndex();
				Weap->SetModelIndex(g_pModelInfo->GetModelIndex(getKnifeModel()));

				if (world_model) {
					*world_model->GetModelIndex() = g_pModelInfo->GetModelIndex(getKnifeModel(false));
				}

				Weap->GetItemIDLow() = -1;
				Weap->GetItemIDHigh() = -1;
				*Weap->GetFallbackPaintKit() = getSkin(c_config::get().knife_skin);
				*Weap->FallbackWear() = 0.00000000001;
			}

			switch (Weap->GetItemDefinitionIndex()) {
			case ItemDefinitionIndex::WEAPON_SSG08: {
				Weap->GetItemIDHigh() = -1;
				*Weap->GetFallbackPaintKit() = getSkin(c_config::get().ssg08_skin);
				*Weap->FallbackSeed() = 0;
				*Weap->OwnerXuidLow() = 0;
				*Weap->OwnerXuidHigh() = 0;
				*Weap->FallbackWear() = 0.00000000001;
			}break;
			case ItemDefinitionIndex::WEAPON_SCAR20: {
				Weap->GetItemIDHigh() = -1;
				*Weap->GetFallbackPaintKit() = getSkin(c_config::get().scar20_skin);
				*Weap->FallbackSeed() = 0;
				*Weap->OwnerXuidLow() = 0;
				*Weap->OwnerXuidHigh() = 0;
				*Weap->FallbackWear() = 0.00000000001;
			} break;
			case ItemDefinitionIndex::WEAPON_G3SG1: {
				Weap->GetItemIDHigh() = -1;
				*Weap->GetFallbackPaintKit() = getSkin(c_config::get().g3sg1_skin);
				*Weap->FallbackSeed() = 0;
				*Weap->OwnerXuidLow() = 0;
				*Weap->OwnerXuidHigh() = 0;
				*Weap->FallbackWear() = 0.00000000001;
			} break;
			case ItemDefinitionIndex::WEAPON_AK47: {
				Weap->GetItemIDHigh() = -1;
				*Weap->GetFallbackPaintKit() = getSkin(c_config::get().ak47_skin);
				*Weap->FallbackSeed() = 0;
				*Weap->OwnerXuidLow() = 0;
				*Weap->OwnerXuidHigh() = 0;
				*Weap->FallbackWear() = 0.00000000001;
			} break;
			case ItemDefinitionIndex::WEAPON_M4A1: {
				Weap->GetItemIDHigh() = -1;
				*Weap->GetFallbackPaintKit() = getSkin(c_config::get().m4a4_skin);
				*Weap->FallbackSeed() = 0;
				*Weap->OwnerXuidLow() = 0;
				*Weap->OwnerXuidHigh() = 0;
				*Weap->FallbackWear() = 0.00000000001;
			} break;
			case ItemDefinitionIndex::WEAPON_M4A1_SILENCER: {
				Weap->GetItemIDHigh() = -1;
				*Weap->GetFallbackPaintKit() = getSkin(c_config::get().m4a1_s_skin);
				*Weap->FallbackSeed() = 0;
				*Weap->OwnerXuidLow() = 0;
				*Weap->OwnerXuidHigh() = 0;
				*Weap->FallbackWear() = 0.00000000001;
			} break;
			case ItemDefinitionIndex::WEAPON_CZ75A: {
				Weap->GetItemIDHigh() = -1;
				*Weap->GetFallbackPaintKit() = getSkin(c_config::get().cz75_skin);
				*Weap->FallbackSeed() = 0;
				*Weap->OwnerXuidLow() = 0;
				*Weap->OwnerXuidHigh() = 0;
				*Weap->FallbackWear() = 0.00000000001;
			} break;
			case ItemDefinitionIndex::WEAPON_DEAGLE: {
				Weap->GetItemIDHigh() = -1;
				*Weap->GetFallbackPaintKit() = getSkin(c_config::get().deagle_skin);
				*Weap->FallbackSeed() = 0;
				*Weap->OwnerXuidLow() = 0;
				*Weap->OwnerXuidHigh() = 0;
				*Weap->FallbackWear() = 0.00000000001;
			} break;
			case ItemDefinitionIndex::WEAPON_REVOLVER: {
				Weap->GetItemIDHigh() = -1;
				*Weap->GetFallbackPaintKit() = getSkin(c_config::get().r8_skin);
				*Weap->FallbackSeed() = 0;
				*Weap->OwnerXuidLow() = 0;
				*Weap->OwnerXuidHigh() = 0;
				*Weap->FallbackWear() = 0.00000000001;
			} break;
			}

		}
	}
}