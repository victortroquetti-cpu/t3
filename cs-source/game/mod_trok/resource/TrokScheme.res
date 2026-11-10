// Trok: fontes do menu próprio (client/trok/ui/trok_menu.cpp).
// Tamanhos em pixels de uma tela de 480 de altura: o VGUI escala pela altura real
// (fontes "proporcionais"). Referência do concept: 1280x720, então tall = px * 480 / 720.
// Fontes: Barlow e Barlow Condensed, SIL Open Font License (resource/fonts/OFL.txt).
Scheme
{
	Colors
	{
	}
	// Cores dos controles VGUI que usam este esquema (o chat da partida: histórico RichText e
	// campo TextEntry; hl2mp_hud_chat.cpp). Paleta de design/ui.md.
	BaseSettings
	{
		"Panel.FgColor"					"236 238 241 255"
		"Panel.BgColor"					"0 0 0 0"
		"Label.TextColor"				"236 238 241 255"
		"Label.BgColor"					"0 0 0 0"
		"Label.DisabledFgColor1"		"128 134 142 255"
		"Label.DisabledFgColor2"		"0 0 0 0"
		"Label.SelectedTextColor"		"236 238 241 255"
		"RichText.TextColor"			"236 238 241 255"
		"RichText.BgColor"				"0 0 0 0"
		"RichText.SelectedTextColor"	"11 13 16 255"
		"RichText.SelectedBgColor"		"242 181 68 255"
		"TextEntry.TextColor"			"236 238 241 255"
		"TextEntry.BgColor"				"0 0 0 0"
		"TextEntry.CursorColor"			"242 181 68 255"
		"TextEntry.DisabledTextColor"	"128 134 142 255"
		"TextEntry.DisabledBgColor"		"0 0 0 0"
		"TextEntry.SelectedTextColor"	"11 13 16 255"
		"TextEntry.SelectedBgColor"		"242 181 68 255"
		"TextEntry.OutOfFocusSelectedBgColor"	"163 169 177 120"
		"TextEntry.FocusEdgeColor"		"0 0 0 0"
		"ScrollBarButton.FgColor"		"163 169 177 255"
		"ScrollBarButton.BgColor"		"0 0 0 0"
		"ScrollBarButton.ArmedFgColor"	"236 238 241 255"
		"ScrollBarButton.ArmedBgColor"	"0 0 0 0"
		"ScrollBarButton.DepressedFgColor"	"242 181 68 255"
		"ScrollBarButton.DepressedBgColor"	"0 0 0 0"
		"ScrollBarSlider.FgColor"		"255 255 255 60"
		"ScrollBarSlider.BgColor"		"255 255 255 15"
		"Chat.TypingText"				"236 238 241 255"
	}
	Fonts
	{
		"TrokLogo"
		{
			"1"	{ "name" "Barlow Condensed" "tall" "27" "weight" "700" "antialias" "1" }
		}
		"TrokTitle"
		{
			"1"	{ "name" "Barlow Condensed" "tall" "35" "weight" "700" "antialias" "1" }
		}
		"TrokItem"
		{
			"1"	{ "name" "Barlow Condensed SemiBold" "tall" "23" "weight" "400" "antialias" "1" }
		}
		"TrokHeading"
		{
			"1"	{ "name" "Barlow Condensed SemiBold" "tall" "16" "weight" "400" "antialias" "1" }
		}
		"TrokButton"
		{
			"1"	{ "name" "Barlow Condensed SemiBold" "tall" "12" "weight" "400" "antialias" "1" }
		}
		"TrokLoadingMap"
		{
			"1"	{ "name" "Barlow Condensed" "tall" "58" "weight" "700" "antialias" "1" }
		}
		"TrokHudNumber"
		{
			"1"	{ "name" "Barlow Condensed" "tall" "29" "weight" "700" "antialias" "1" "dropshadow" "1" }
		}
		"TrokHudScore"
		{
			"1"	{ "name" "Barlow Condensed" "tall" "20" "weight" "700" "antialias" "1" "dropshadow" "1" }
		}
		"TrokHudText"
		{
			"1"	{ "name" "Barlow Condensed SemiBold" "tall" "10" "weight" "400" "antialias" "1" "dropshadow" "1" }
		}
		"TrokHudSmall"
		{
			"1"	{ "name" "Barlow Medium" "tall" "8" "weight" "400" "antialias" "1" "dropshadow" "1" }
		}
		"TrokBody"
		{
			"1"	{ "name" "Barlow" "tall" "10" "weight" "400" "antialias" "1" }
		}
		"TrokSmall"
		{
			"1"	{ "name" "Barlow Medium" "tall" "8" "weight" "400" "antialias" "1" }
		}
		// Chat da partida (hl2mp_hud_chat.cpp): histórico, campo de texto e o prompt TODOS:/TIME:.
		// Barlow tem os acentos do português.
		"ChatFont"
		{
			"1"	{ "name" "Barlow Medium" "tall" "9" "weight" "400" "antialias" "1" "dropshadow" "1" }
		}
		"TrokChatPrompt"
		{
			"1"	{ "name" "Barlow Condensed SemiBold" "tall" "9" "weight" "400" "antialias" "1" "dropshadow" "1" }
		}
		// Usadas pelos controles VGUI deste esquema (rótulo e setas da rolagem do chat).
		"Default"
		{
			"1"	{ "name" "Barlow" "tall" "10" "weight" "400" "antialias" "1" }
		}
		"Marlett"
		{
			"1"	{ "name" "Marlett" "tall" "14" "weight" "0" "symbol" "1" }
		}
		// Mensagens do centro da tela e dicas (vgui_centerprint.cpp, hud_hintdisplay.cpp).
		"TrokCentro"
		{
			"1"	{ "name" "Barlow Condensed SemiBold" "tall" "14" "weight" "400" "antialias" "1" "dropshadow" "1" }
		}
	}
	Borders
	{
	}
	CustomFontFiles
	{
		"1"	"resource/fonts/BarlowCondensed-Bold.ttf"
		"2"	"resource/fonts/BarlowCondensed-SemiBold.ttf"
		"3"	"resource/fonts/Barlow-Regular.ttf"
		"4"	"resource/fonts/Barlow-Medium.ttf"
	}
}
