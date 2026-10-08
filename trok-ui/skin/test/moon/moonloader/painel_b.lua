-- "Painel B": tema padrao; fundo vermelho empurrado so para a janela; texto e botao vermelhos dentro.
local imgui = require 'imgui'
local teste = require 'trok_teste'
local s = teste.state()
local open = imgui.ImBool(true)
imgui.Process = true

function imgui.OnDrawFrame()
    imgui.SetNextWindowPos(imgui.ImVec2(480, 40), imgui.Cond.Always)
    imgui.SetNextWindowSize(imgui.ImVec2(400, 520), imgui.Cond.Always)
    -- No ImGui 1.52 o push feito antes do Begin so pode sair depois do End (senao o End dispara um assert).
    imgui.PushStyleColor(imgui.Col.WindowBg, imgui.ImVec4(0.55, 0.08, 0.08, 1))
    imgui.Begin('Painel B', open, 0)
    teste.widgets('Painel B', s)
    imgui.PushStyleColor(imgui.Col.Text, imgui.ImVec4(1, 0.3, 0.3, 1))
    imgui.Text('Erro: senha incorreta')
    teste.rect('Painel B', 'texto_vermelho')
    imgui.PopStyleColor()
    imgui.PushStyleColor(imgui.Col.Button, imgui.ImVec4(0.8, 0.1, 0.1, 1))
    imgui.Button('Apagar tudo')
    teste.rect('Painel B', 'botao_vermelho')
    imgui.PopStyleColor()
    imgui.End()
    imgui.PopStyleColor()
end
