//=============================================================================
// Trok: menu próprio no lugar do menu do GameUI. Ver trok_menu.cpp.
//=============================================================================
#ifndef TROK_MENU_H
#define TROK_MENU_H
#ifdef _WIN32
#pragma once
#endif

// Cria o painel e registra no GameUI (chamado em VGui_CreateGlobalPanels; patch do sdk.patch).
void TrokMenu_Create();
void TrokMenu_Destroy();

// true quando o menu está numa subtela: o ESC volta para a tela inicial em vez de
// fechar o menu (chamado em CHLClient::HandleUiToggle; patch do sdk.patch).
bool TrokMenu_CaptureESC();

#endif // TROK_MENU_H
