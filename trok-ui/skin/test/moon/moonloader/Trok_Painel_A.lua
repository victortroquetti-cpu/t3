-- "Painel A": tema claro do proprio script e cantos retos (aplicado uma vez, ao carregar). Tem "trok" no nome do
-- arquivo, mas nao usa o kit da casa: tem que ser padronizado (o nome nao conta). Abre tambem o "Painel A2", sem X.
local imgui = require 'imgui'
local teste = require 'trok_teste'
local s = teste.state()
local open = imgui.ImBool(true)
imgui.Process = true

imgui.SwitchContext()
local style = imgui.GetStyle()
local colors, clr, ImVec4 = style.Colors, imgui.Col, imgui.ImVec4
style.WindowRounding = 0
style.FrameRounding = 0
colors[clr.Text] = ImVec4(0, 0, 0, 1)
colors[clr.WindowBg] = ImVec4(0.94, 0.94, 0.94, 1)
colors[clr.TitleBg] = ImVec4(0.96, 0.96, 0.96, 1)
colors[clr.TitleBgActive] = ImVec4(0.82, 0.82, 0.82, 1)
colors[clr.FrameBg] = ImVec4(1, 1, 1, 1)
colors[clr.Button] = ImVec4(0.26, 0.59, 0.98, 0.4)
colors[clr.Border] = ImVec4(0, 0, 0, 0.3)

HOST_VALUES = function() return teste.values(s, open) end

function imgui.OnDrawFrame()
    -- Segunda janela do script, sem botao de fechar (o Esc nao fecha janela sem X).
    imgui.SetNextWindowPos(imgui.ImVec2(40, 600), imgui.Cond.Always)
    imgui.SetNextWindowSize(imgui.ImVec2(400, 150), imgui.Cond.Always)
    imgui.Begin('Painel A2')
    imgui.Text('Janela sem X')
    imgui.End()
    if not open.v then return end -- a janela so e desenhada enquanto esta aberta (o X ou o Esc fecham)
    imgui.SetNextWindowPos(imgui.ImVec2(40, 40), imgui.Cond.Always)
    imgui.SetNextWindowSize(imgui.ImVec2(400, 520), imgui.Cond.Always)
    imgui.Begin('Painel A', open)
    teste.widgets('Painel A', s)
    imgui.End()
end
