-- "HUD": fundo transparente por cima do jogo, sem titulo.
local imgui = require 'imgui'
local teste = require 'trok_teste'
imgui.Process = true

imgui.SwitchContext()
imgui.GetStyle().Colors[imgui.Col.WindowBg] = imgui.ImVec4(0, 0, 0, 0)

function imgui.OnDrawFrame()
    imgui.SetNextWindowPos(imgui.ImVec2(940, 40), imgui.Cond.Always)
    imgui.SetNextWindowSize(imgui.ImVec2(300, 120), imgui.Cond.Always)
    local flags = imgui.WindowFlags
    imgui.Begin('HUD', nil, flags.NoTitleBar + flags.NoResize + flags.NoInputs)
    imgui.Text('Velocidade 120 km/h')
    teste.rect('HUD', 'texto')
    imgui.End()
end
