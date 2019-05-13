#include "Menu.h"
#include "TGFCfg.h"
#include "../SDK/Vector.h"
#include "../SDK/ISurface.h"
#include "../Utils/Color.h"
#include "../Utils/GlobalVars.h"
#include "config.h"
#include "../Hooks.h"
#include "../ImGui/imgui.h"
#include "../ImGui/dx9/imgui_dx9.h"
Menu g_Menu;

void Menu::Render()
{
	
}

void Menu::GUI_Init(IDirect3DDevice9* pDevice)
{
	ImGui_ImplDX9_Init(g_Hooks.hCSGOWindow, pDevice);
	D3DInit = true;
}
