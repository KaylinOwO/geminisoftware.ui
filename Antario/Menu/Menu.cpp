#include "Menu.h"
#include "TGFCfg.h"
#include "../SDK/Vector.h"
#include "../SDK/ISurface.h"
#include "../Utils/Color.h"
#include "../Utils/GlobalVars.h"
#include "config.h"
#include "../Hooks.h"

Menu g_Menu;

void Menu::Render()
{
	
}

void Menu::GUI_Init(IDirect3DDevice9* pDevice)
{
	ImGui_ImplDX9_Init(g_Hooks.hCSGOWindow, pDevice);
	D3DInit = true;
}

void Menu::HandleMenuVisibility(){
	static bool is_down = false;
	static bool is_clicked = false;

	if (Globals::PressedKeys[VK_INSERT])
	{
		is_clicked = false;
		is_down = true;
	}
	else if (!Globals::PressedKeys[VK_INSERT] && is_down)
	{
		is_clicked = true;
		is_down = false;
	}
	else
	{
		is_clicked = false;
		is_down = false;
	}

	if (is_clicked)
	{
		menuOpened = !menuOpened;
	}
}
