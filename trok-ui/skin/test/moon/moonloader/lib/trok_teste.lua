-- Teste do Trok Skin: os mesmos controles em cada painel, com o retangulo de cada um gravado pelo host.
local imgui = require 'imgui'

local M = {}

function M.rect(script, what)
    local a, b = imgui.GetItemRectMin(), imgui.GetItemRectMax()
    host_rect(script, what, a.x, a.y, b.x, b.y)
end

-- Valores do painel para o host (modo cliques): caixa, volume e se a janela esta aberta.
function M.values(s, open)
    return string.format('check=%d volume=%.0f aberto=%d', s.check.v and 1 or 0, s.volume.v, open.v and 1 or 0)
end

function M.state()
    return {check = imgui.ImBool(true), volume = imgui.ImFloat(65), nome = imgui.ImBuffer('Victor_Trok', 64),
            modo = imgui.ImInt(1)}
end

function M.widgets(name, s)
    imgui.Text('Configura\195\167\195\181es do menu (' .. name .. ')')
    M.rect(name, 'texto')
    imgui.Button('Salvar')
    M.rect(name, 'salvar')
    imgui.SameLine()
    imgui.Button('Fechar')
    M.rect(name, 'fechar')
    imgui.Checkbox('Ativar som', s.check)
    M.rect(name, 'checkbox')
    imgui.SliderFloat('Volume', s.volume, 0, 100, '%.0f')
    M.rect(name, 'slider')
    imgui.InputText('Nome', s.nome)
    M.rect(name, 'campo')
    imgui.Combo('Modo', s.modo, {'Um', 'Dois', 'Tr\195\170s'})
    M.rect(name, 'combo')
    imgui.Separator()
    imgui.Selectable('Item selecionado', true)
    M.rect(name, 'selecionavel')
    imgui.CollapsingHeader('Avan\195\167\097do')
    M.rect(name, 'cabecalho')
    imgui.Text('\208\159\209\128\208\184\208\178\208\181\209\130, \208\188\208\184\209\128 (cir\195\173lico)')
    M.rect(name, 'cirilico')
end

return M
