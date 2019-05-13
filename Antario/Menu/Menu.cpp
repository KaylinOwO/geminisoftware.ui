#include "Menu.h"
#include "TGFCfg.h"
#include "../SDK/Vector.h"
#include "../SDK/ISurface.h"
#include "../Utils/Color.h"
#include "../Utils/GlobalVars.h"
#include "config.h"
#include "../../Antario/ImGui/imgui.h"
#include "../../Antario/ImGui/imconfig.h"
#include "../../Antario/ImGui/imgui_internal.h"
#include "../../Antario/ImGui/imstb_rectpack.h"
#include "../ImGui/imstb_textedit.h"
#include "../ImGui/imstb_truetype.h"
#include "../Hooks.h"
Menu g_Menu;

void Menu::Render()
{
	
}

void Menu::GUI_Init(IDirect3DDevice9* pDevice)
{
	ImGui_ImplDX11_Init(g_Hooks.hCSGOWindow, pDevice);
	D3DInit = true;
}
