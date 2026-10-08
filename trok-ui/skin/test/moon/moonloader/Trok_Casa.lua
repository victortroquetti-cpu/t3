-- Script da casa ("trok" no nome) com janela ##trokCasa e estilo proprio: a skin nao muda nada nele.
local imgui = require 'imgui'
local teste = require 'trok_teste'
local check = imgui.ImBool(true)
imgui.Process = true

imgui.SwitchContext()
imgui.GetStyle().WindowRounding = 0
imgui.GetStyle().FrameRounding = 2

function imgui.OnDrawFrame()
    imgui.SetNextWindowPos(imgui.ImVec2(940, 200), imgui.Cond.Always)
    imgui.SetNextWindowSize(imgui.ImVec2(300, 300), imgui.Cond.Always)
    imgui.PushStyleColor(imgui.Col.WindowBg, imgui.ImVec4(0.1, 0.2, 0.6, 1))
    imgui.Begin('##trokCasa', nil, imgui.WindowFlags.NoTitleBar)
    imgui.Text('Janela da casa')
    teste.rect('Casa', 'texto')
    imgui.Button('Botao padrao')
    teste.rect('Casa', 'botao')
    imgui.Checkbox('Caixa', check)
    teste.rect('Casa', 'caixa')
    imgui.End()
    imgui.PopStyleColor()
end
