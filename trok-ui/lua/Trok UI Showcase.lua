script_name('Trok UI Showcase')
script_author('Victor_Trok')
script_version('1.0.0')
script_description('Vitrine de todos os controles de menu da casa. Digite /trokuilua no chat.')

require 'moonloader'

local imgui = require 'mimgui'
local vkeys = require 'vkeys'
local bit = require 'bit'
local ffi = require 'ffi'

ffi.cdef[[
    int __stdcall IsWindow(void* hWnd);
    int __stdcall SetPropA(void* hWnd, const char* lpString, void* hData);
    void* __stdcall RemovePropA(void* hWnd, const char* lpString);
]]

local user32 = ffi.load('user32')

local VERSION_TAG = 'v1.0.0'

-- =====================================================================================
-- TROK UI -- kit de componentes da casa para mods Lua (mimgui).
-- Copie este bloco inteiro para um mod novo. Tudo e desenhado a mao no draw list, sem os
-- widgets cinza padrao do ImGui. As medidas saem dos mods publicados (Trok Kill List,
-- Trok Dialogs e Trok Radar) e escalam com u = altura/1080*0.7225: 15% menor que a conta do Kill List.
-- Fontes da casa: resource\trok\font.ttf (Gotham Medium) e resource\trok\lucide.ttf (icones).
-- =====================================================================================

local ui = {
    u = 1.0,
    fonts = {},
    anim = {},
    reveal = {},
    capturing = nil, -- id da linha de tecla que espera o jogador apertar algo
    captured = nil,  -- o que o laco principal pegou: numero da tecla, 'cancelar' ou 'limpar'
    popupDrawn = false,
    popupOpen = false,
    closePopup = false,
    tipId = nil,
    tipSince = 0,
    tipSeen = false,
    toasts = {},
}

-- Paleta da casa: monocromatica. "strong" e o branco do toggle ligado; "ink" e o texto sobre ele;
-- "marker" marca o valor padrao.
local PAL = {
    text = { 240, 240, 240, 255 },
    column = { 200, 200, 200, 255 },
    hint = { 255, 255, 255, 118 },
    disabled = { 255, 255, 255, 42 },
    version = { 140, 140, 140, 255 },
    separator = { 255, 255, 255, 10 },
    selection = { 255, 255, 255, 10 },
    hover = { 255, 255, 255, 6 },
    number = { 255, 255, 255, 78 },
    numberHover = { 255, 255, 255, 110 },
    numberSel = { 255, 255, 255, 150 },
    strong = { 226, 226, 226, 255 },
    ink = { 18, 18, 18, 255 },
    marker = { 255, 255, 255, 70 },
}

-- Codigos oficiais do Lucide (pacote lucide-static). O lucide.ttf da casa e um recorte com o X em
-- 'x' (U+0078), seta, chevron, chevron duplo, olho e olho cortado; os outros entram se o arquivo tiver.
local GLYPH = {
    X_ASCII = 0x0078, X = 0xE1B2,
    ARROW_RIGHT = 0xE049,
    CHECK = 0xE06C, CHEVRON_DOWN = 0xE06D, CHEVRON_LEFT = 0xE06E, CHEVRON_RIGHT = 0xE06F, CHEVRON_UP = 0xE070,
    CHEVRONS_RIGHT = 0xE073,
    EYE = 0xE0BA, EYE_OFF = 0xE0BB, INFO = 0xE0F9, SEARCH = 0xE151,
}

local function rgba(r, g, b, a)
    return imgui.ColorConvertFloat4ToU32(imgui.ImVec4(r / 255, g / 255, b / 255, (a or 255) / 255))
end

local function white(a)
    return rgba(255, 255, 255, a)
end

local function gray(v, a)
    return rgba(v, v, v, a or 255)
end

local function pal(name, alpha)
    local t = PAL[name]
    return rgba(t[1], t[2], t[3], t[4] * (alpha or 1))
end

local function vec(x, y)
    return imgui.ImVec2(x, y)
end

-- 15% menor que a conta do Kill List (altura/1080*0.85, minimo 0.55): tudo vezes 0.85.
function ui.scale(screenY)
    return math.max(0.4675, (screenY / 1080) * 0.7225)
end

-- Chamar dentro do imgui.OnInitialize.
function ui.buildFonts()
    local io = imgui.GetIO()
    local _, screenY = getScreenResolution()
    ui.u = ui.scale(screenY)
    local u = ui.u

    local arial = 'C:\\Windows\\Fonts\\arial.ttf'
    local house = getWorkingDirectory() .. '\\resource\\trok\\font.ttf'
    local icons = getWorkingDirectory() .. '\\resource\\trok\\lucide.ttf'
    local body = doesFileExist(house) and house or arial
    local ranges = io.Fonts:GetGlyphRangesCyrillic()

    if doesFileExist(body) then
        local cfg = imgui.ImFontConfig()
        cfg.OversampleH = 2
        cfg.OversampleV = 2
        ui.fonts.body = io.Fonts:AddFontFromFileTTF(body, 16 * u, cfg, ranges)
        ui.fonts.title = io.Fonts:AddFontFromFileTTF(body, 20 * u, cfg, ranges)
        ui.fonts.desc = io.Fonts:AddFontFromFileTTF(body, 13.5 * u, cfg, ranges)
    else
        local default = io.Fonts:AddFontDefault()
        ui.fonts.body, ui.fonts.title, ui.fonts.desc = default, default, default
    end

    if doesFileExist(icons) then
        -- So os icones que o kit usa (o atlas fica pequeno mesmo com o lucide completo). A tabela
        -- precisa viver enquanto o atlas existir, por isso fica guardada em ui.
        ui.iconRanges = imgui.new.ImWchar[17](
            GLYPH.X_ASCII, GLYPH.X_ASCII, GLYPH.ARROW_RIGHT, GLYPH.ARROW_RIGHT, GLYPH.CHECK, GLYPH.CHEVRON_UP,
            GLYPH.CHEVRONS_RIGHT, GLYPH.CHEVRONS_RIGHT, GLYPH.EYE, GLYPH.EYE_OFF, GLYPH.INFO, GLYPH.INFO,
            GLYPH.SEARCH, GLYPH.SEARCH, GLYPH.X, GLYPH.X, 0)
        local cfg = imgui.ImFontConfig()
        cfg.OversampleH = 2
        cfg.OversampleV = 2
        ui.fonts.icon = io.Fonts:AddFontFromFileTTF(icons, 18 * u, cfg, ui.iconRanges)
        -- A 13u o traco do lucide fica abaixo de 1 px e o icone sai cinza: reforca a cobertura para ele
        -- ter o mesmo peso dos icones de 18u.
        local smallCfg = imgui.ImFontConfig()
        smallCfg.OversampleH = 2
        smallCfg.OversampleV = 2
        smallCfg.RasterizerMultiply = 1.6
        ui.fonts.iconSmall = io.Fonts:AddFontFromFileTTF(icons, 13 * u, smallCfg, ui.iconRanges)
    end
end

local function textSize(font, text)
    return font:CalcTextSizeA(font.FontSize, 10000, 0, text)
end

local function drawText(dl, font, x, y, color, text)
    dl:AddTextFontPtr(font, font.FontSize, vec(math.floor(x), math.floor(y)), color, text)
end

-- Quebra de linha propria (por palavras), sem depender do wrap do AddText.
local function wrapLines(font, text, maxWidth)
    local lines = {}
    for paragraph in (text .. '\n'):gmatch('(.-)\n') do
        local line = ''
        for word in paragraph:gmatch('%S+') do
            local candidate = line == '' and word or (line .. ' ' .. word)
            if line ~= '' and textSize(font, candidate).x > maxWidth then
                lines[#lines + 1] = line
                line = word
            else
                line = candidate
            end
        end
        lines[#lines + 1] = line
    end
    return lines
end

local function anim(id, target, speed)
    local value = ui.anim[id]
    if value == nil then
        ui.anim[id] = target
        return target
    end
    local k = 1 - math.exp(-(speed or 14) * imgui.GetIO().DeltaTime)
    value = value + (target - value) * k
    if math.abs(target - value) < 0.001 then
        value = target
    end
    ui.anim[id] = value
    return value
end

local function hand()
    imgui.SetMouseCursor(imgui.MouseCursor.Hand)
end

local function box(dl, x1, y1, x2, y2, fill, border, rounding)
    if fill then
        dl:AddRectFilled(vec(x1, y1), vec(x2, y2), fill, rounding)
    end
    if border then
        dl:AddRect(vec(x1 + 0.5, y1 + 0.5), vec(x2 - 0.5, y2 - 0.5), border, rounding, nil, 1)
    end
end

-- ---------------------------------------------------------------- icones (lucide)

-- Desenha o glifo do lucide centrado em (cx, cy). O centro e o do grid de 24 do lucide (a caixa em: metade
-- do avanco e metade da altura da fonte), nao o da caixa do glifo: o lucide.ttf grava a caixa de cada
-- glifo a partir de (0,0), entao a caixa que o mimgui monta sobra a esquerda e embaixo, e centraliza-la
-- jogaria o icone para cima e para a direita. turns gira 90 graus no sentido horario e mirror espelha na
-- horizontal, os dois em volta desse centro. Devolve false se a fonte nao tem o glifo.
local function glyphQuad(dl, font, cp, cx, cy, color, turns, mirror)
    if not font then
        return false
    end
    local g = font:FindGlyphNoFallback(cp)
    if g == nil then
        return false
    end
    turns = turns or 0
    local ax, ay = g.AdvanceX * 0.5, font.FontSize * 0.5
    -- Origem do glifo em pixel inteiro, como no texto, para o icone nao borrar.
    cx = math.floor(cx - ax + 0.5) + ax
    cy = math.floor(cy - ay + 0.5) + ay
    local l, r, t, b = g.X0 - ax, g.X1 - ax, g.Y0 - ay, g.Y1 - ay
    local u0, u1 = g.U0, g.U1
    if mirror then
        l, r = -r, -l
        u0, u1 = u1, u0
    end
    local corners = { { l, t }, { r, t }, { r, b }, { l, b } }
    local p = {}
    for i = 1, 4 do
        local x, y = corners[i][1], corners[i][2]
        for _ = 1, turns do
            x, y = -y, x
        end
        p[i] = vec(cx + x, cy + y)
    end
    dl:AddImageQuad(font.ContainerAtlas.TexID, p[1], p[2], p[3], p[4], vec(u0, g.V0), vec(u1, g.V0), vec(u1, g.V1),
        vec(u0, g.V1), color)
    return true
end

local function polyline(dl, points, color, thickness)
    for i = 1, #points - 1 do
        dl:AddLine(points[i], points[i + 1], color, thickness)
    end
end

local function quadratic(x0, y0, x1, y1, x2, y2, segments)
    local points = {}
    for i = 0, segments do
        local t = i / segments
        local a, b, c = (1 - t) * (1 - t), 2 * (1 - t) * t, t * t
        points[#points + 1] = vec(a * x0 + b * x1 + c * x2, a * y0 + b * y1 + c * y2)
    end
    return points
end

local function arc(dl, cx, cy, radius, a0, a1, color, thickness, segments)
    local points = {}
    for i = 0, segments do
        local a = a0 + (a1 - a0) * i / segments
        points[#points + 1] = vec(cx + math.cos(a) * radius, cy + math.sin(a) * radius)
    end
    polyline(dl, points, color, thickness)
end

-- Desenhos vetoriais no mesmo traco do lucide, para quando o glifo nao existe na fonte.
local VECTOR = {}

function VECTOR.chevron(dl, x, y, dir, color)
    local u = ui.u
    local a, b, t = 2.5 * u, 5 * u, 1.6 * u
    if dir == 'esquerda' then
        dl:AddLine(vec(x + a, y - b), vec(x - a, y), color, t)
        dl:AddLine(vec(x - a, y), vec(x + a, y + b), color, t)
    elseif dir == 'direita' then
        dl:AddLine(vec(x - a, y - b), vec(x + a, y), color, t)
        dl:AddLine(vec(x + a, y), vec(x - a, y + b), color, t)
    elseif dir == 'baixo' then
        dl:AddLine(vec(x - b, y - a), vec(x, y + a), color, t)
        dl:AddLine(vec(x, y + a), vec(x + b, y - a), color, t)
    else
        dl:AddLine(vec(x - b, y + a), vec(x, y - a), color, t)
        dl:AddLine(vec(x, y - a), vec(x + b, y + a), color, t)
    end
end

function VECTOR.eye(dl, cx, cy, s, slashed, color)
    local t = 1.4 * ui.u
    polyline(dl, quadratic(cx - s, cy, cx, cy - s * 1.15, cx + s, cy, 10), color, t)
    polyline(dl, quadratic(cx - s, cy, cx, cy + s * 1.15, cx + s, cy, 10), color, t)
    dl:AddCircle(vec(cx, cy), s * 0.32, color, 12, t)
    if slashed then
        dl:AddLine(vec(cx - s * 0.85, cy + s * 0.85), vec(cx + s * 0.85, cy - s * 0.85), color, t)
    end
end

function VECTOR.check(dl, cx, cy, s, color)
    local t = 1.8 * ui.u
    dl:AddLine(vec(cx - s, cy), vec(cx - s * 0.3, cy + s * 0.7), color, t)
    dl:AddLine(vec(cx - s * 0.3, cy + s * 0.7), vec(cx + s, cy - s * 0.75), color, t)
end

-- name: 'fechar', 'esquerda', 'direita', 'cima', 'baixo', 'duplo', 'seta', 'olho', 'olhoCortado',
-- 'check', 'busca', 'info'. small usa o lucide de 13u.
function ui.icon(dl, name, cx, cy, color, small)
    local u = ui.u
    local font = small and ui.fonts.iconSmall or ui.fonts.icon
    if name == 'fechar' then
        if glyphQuad(dl, font, GLYPH.X_ASCII, cx, cy, color) or glyphQuad(dl, font, GLYPH.X, cx, cy, color) then
            return
        end
        dl:AddLine(vec(cx - 4.5 * u, cy - 4.5 * u), vec(cx + 4.5 * u, cy + 4.5 * u), color, 1.6 * u)
        dl:AddLine(vec(cx + 4.5 * u, cy - 4.5 * u), vec(cx - 4.5 * u, cy + 4.5 * u), color, 1.6 * u)
    elseif name == 'direita' then
        if not glyphQuad(dl, font, GLYPH.CHEVRON_RIGHT, cx, cy, color) then
            VECTOR.chevron(dl, cx, cy, 'direita', color)
        end
    elseif name == 'esquerda' then
        -- Espelhado (nao girado) para ficar na mesma altura do chevron da direita.
        if not glyphQuad(dl, font, GLYPH.CHEVRON_LEFT, cx, cy, color)
            and not glyphQuad(dl, font, GLYPH.CHEVRON_RIGHT, cx, cy, color, 0, true) then
            VECTOR.chevron(dl, cx, cy, 'esquerda', color)
        end
    elseif name == 'baixo' then
        if not glyphQuad(dl, font, GLYPH.CHEVRON_DOWN, cx, cy, color)
            and not glyphQuad(dl, font, GLYPH.CHEVRON_RIGHT, cx, cy, color, 1) then
            VECTOR.chevron(dl, cx, cy, 'baixo', color)
        end
    elseif name == 'cima' then
        if not glyphQuad(dl, font, GLYPH.CHEVRON_UP, cx, cy, color)
            and not glyphQuad(dl, font, GLYPH.CHEVRON_RIGHT, cx, cy, color, 3) then
            VECTOR.chevron(dl, cx, cy, 'cima', color)
        end
    elseif name == 'duplo' then
        if not glyphQuad(dl, font, GLYPH.CHEVRONS_RIGHT, cx, cy, color) then
            VECTOR.chevron(dl, cx - 3 * u, cy, 'direita', color)
            VECTOR.chevron(dl, cx + 3 * u, cy, 'direita', color)
        end
    elseif name == 'seta' then
        if not glyphQuad(dl, font, GLYPH.ARROW_RIGHT, cx, cy, color) then
            dl:AddLine(vec(cx - 6 * u, cy), vec(cx + 6 * u, cy), color, 1.6 * u)
            VECTOR.chevron(dl, cx + 3.5 * u, cy, 'direita', color)
        end
    elseif name == 'olho' or name == 'olhoCortado' then
        if not glyphQuad(dl, font, name == 'olho' and GLYPH.EYE or GLYPH.EYE_OFF, cx, cy, color) then
            VECTOR.eye(dl, cx, cy, (small and 5 or 6.5) * u, name == 'olhoCortado', color)
        end
    elseif name == 'check' then
        if not glyphQuad(dl, font, GLYPH.CHECK, cx, cy, color) then
            VECTOR.check(dl, cx, cy, (small and 4.5 or 6) * u, color)
        end
    elseif name == 'busca' then
        if not glyphQuad(dl, font, GLYPH.SEARCH, cx, cy, color) then
            local s, t = (small and 7 or 9) * u, 1.5 * u
            dl:AddCircle(vec(cx - s * 0.15, cy - s * 0.15), s * 0.55, color, 16, t)
            dl:AddLine(vec(cx + s * 0.25, cy + s * 0.25), vec(cx + s * 0.75, cy + s * 0.75), color, t)
        end
    elseif name == 'info' then
        if not glyphQuad(dl, font, GLYPH.INFO, cx, cy, color) then
            local r = (small and 6 or 8) * u
            dl:AddCircle(vec(cx, cy), r, color, 20, 1.4 * u)
            dl:AddCircleFilled(vec(cx, cy - r * 0.42), 1.2 * u, color)
            dl:AddLine(vec(cx, cy - r * 0.1), vec(cx, cy + r * 0.5), color, 1.4 * u)
        end
    end
end

-- Largura das setas no meio do texto: a tinta no grid de 24 do lucide (com as pontas redondas) mais 1u de
-- folga de cada lado. Vem do desenho do lucide, nao da caixa do glifo (que no lucide.ttf comeca em 0).
local ICON_UNITS = { seta = 16, direita = 8, duplo = 14 }

function ui.iconWidth(name, small)
    return (small and 13 or 18) * ui.u * (ICON_UNITS[name] or 8) / 24 + 2 * ui.u
end

local function spinner(dl, cx, cy, radius, time)
    local u = ui.u
    dl:AddCircle(vec(cx, cy), radius, white(22), 24, 2 * u)
    local start = time * 5
    arc(dl, cx, cy, radius, start, start + math.pi * 1.4, pal('strong'), 2 * u, 20)
end

-- ---------------------------------------------------------------- texto rico (SA-MP)

-- Setas do servidor viram icones do lucide, como no Trok Dialogs.
local ARROW_TOKENS = { ['>'] = 'direita', ['>>'] = 'duplo', ['\194\187'] = 'duplo', ['->'] = 'seta', ['=>'] = 'seta' }

-- Cores {RRGGBB} do SA-MP e as setas (">", ">>", "\194\187", "->", "=>" viram icones). Uma linha por \n.
-- Devolve largura e altura; draw = false so mede.
function ui.richText(dl, font, x, y, color, text, draw)
    local u = ui.u
    local lineH = font.FontSize + 4 * u
    local spaceW = textSize(font, ' ').x
    -- Icones alinhados pelo meio das maiusculas (o centro da caixa da linha fica alto demais).
    local capital = font:FindGlyph(72)
    local iconMid = capital ~= nil and (capital.Y0 + capital.Y1) * 0.5 or font.FontSize * 0.5
    local current = color
    local maxW, lineIndex = 0, 0
    for line in (text .. '\n'):gmatch('(.-)\n') do
        local cx = x
        local cy = y + lineIndex * lineH
        lineIndex = lineIndex + 1
        local rest = line
        while #rest > 0 do
            local before, hex, after = rest:match('^(.-){(%x%x%x%x%x%x)}(.*)$')
            local chunk = before or rest
            -- Palavra a palavra (os espacos ficam), para trocar as setas por icones.
            for space, word in chunk:gmatch('(%s*)(%S*)') do
                cx = cx + #space * spaceW
                if word ~= '' then
                    local icon = ARROW_TOKENS[word]
                    if icon then
                        local w = ui.iconWidth(icon)
                        if draw ~= false then
                            ui.icon(dl, icon, cx + w * 0.5, cy + iconMid, current)
                        end
                        cx = cx + w
                    else
                        if draw ~= false then
                            drawText(dl, font, cx, cy, current, word)
                        end
                        cx = cx + textSize(font, word).x
                    end
                end
            end
            if hex then
                current = rgba(tonumber(hex:sub(1, 2), 16), tonumber(hex:sub(3, 4), 16), tonumber(hex:sub(5, 6), 16))
                rest = after
            else
                rest = ''
            end
        end
        maxW = math.max(maxW, cx - x)
    end
    return maxW, lineIndex * lineH
end

-- ---------------------------------------------------------------- cores (HSV)

local function hsvToRgb(h, s, v)
    local i = math.floor(h * 6)
    local f = h * 6 - i
    local p, q, t = v * (1 - s), v * (1 - f * s), v * (1 - (1 - f) * s)
    i = i % 6
    if i == 0 then return v, t, p end
    if i == 1 then return q, v, p end
    if i == 2 then return p, v, t end
    if i == 3 then return p, q, v end
    if i == 4 then return t, p, v end
    return v, p, q
end

local function rgbToHsv(r, g, b)
    local max, min = math.max(r, g, b), math.min(r, g, b)
    local d = max - min
    local h = 0
    if d > 0 then
        if max == r then
            h = ((g - b) / d) % 6
        elseif max == g then
            h = (b - r) / d + 2
        else
            h = (r - g) / d + 4
        end
        h = h / 6
    end
    return h, max > 0 and d / max or 0, max
end

local function sameColor(a, b)
    return a[1] == b[1] and a[2] == b[2] and a[3] == b[3]
end

local function hexColor(c)
    return string.format('#%02X%02X%02X', c[1], c[2], c[3])
end

-- ---------------------------------------------------------------- dica (tooltip)

function ui.tooltip(id, text)
    ui.tipSeen = true
    local now = imgui.GetTime()
    if ui.tipId ~= id then
        ui.tipId = id
        ui.tipSince = now
    end
    if now - ui.tipSince < 0.35 then
        return
    end
    local u = ui.u
    local font = ui.fonts.desc
    local dl = imgui.GetForegroundDrawList()
    local mouse = imgui.GetIO().MousePos
    local screenX = getScreenResolution()
    local lines = wrapLines(font, text, 260 * u)
    local width, lineH = 0, font.FontSize + 2 * u
    for _, line in ipairs(lines) do
        width = math.max(width, textSize(font, line).x)
    end
    local pad = 8 * u
    local x1, y1 = mouse.x + 14 * u, mouse.y + 18 * u
    local x2, y2 = x1 + width + pad * 2, y1 + lineH * #lines + pad * 1.5
    if x2 > screenX - 4 then
        x1, x2 = x1 - (x2 - screenX + 4), screenX - 4
    end
    dl:AddRectFilled(vec(x1, y1), vec(x2, y2), rgba(22, 22, 22, 250), 6 * u)
    dl:AddRect(vec(x1, y1), vec(x2, y2), white(18), 6 * u, nil, 1)
    for i, line in ipairs(lines) do
        drawText(dl, font, x1 + pad, y1 + pad * 0.75 + (i - 1) * lineH, pal('column'), line)
    end
end

-- ---------------------------------------------------------------- notificacoes

local TOAST_LIFE = 4.0

function ui.toast(title, message)
    ui.toasts[#ui.toasts + 1] = { title = title, message = message, born = os.clock() }
    while #ui.toasts > 4 do
        table.remove(ui.toasts, 1)
    end
end

function ui.drawToasts()
    if #ui.toasts == 0 then
        return
    end
    local u = ui.u
    local dl = imgui.GetForegroundDrawList()
    local io = imgui.GetIO()
    local screenX, screenY = getScreenResolution()
    local now = os.clock()
    local width, margin, gap = 320 * u, 24 * u, 8 * u
    local y = screenY - margin
    for i = #ui.toasts, 1, -1 do
        local toast = ui.toasts[i]
        local age = now - toast.born
        if age > TOAST_LIFE then
            table.remove(ui.toasts, i)
        else
            local fadeIn = math.min(1, age / 0.18)
            local alpha = math.min(fadeIn, math.min(1, (TOAST_LIFE - age) / 0.3))
            local x = screenX - margin - width + (1 - fadeIn) * 16 * u
            local lines = wrapLines(ui.fonts.desc, toast.message, width - 32 * u)
            local lineH = ui.fonts.desc.FontSize + 2 * u
            local height = 14 * u + ui.fonts.body.FontSize + 4 * u + lineH * #lines + 14 * u
            local x1, y1, x2, y2 = x, y - height, x + width, y

            local m = io.MousePos
            local hovered = m.x >= x1 and m.x <= x2 and m.y >= y1 and m.y <= y2
            if hovered and imgui.IsMouseClicked(0) then
                toast.born = now - TOAST_LIFE + 0.3
            end

            dl:AddRectFilled(vec(x1, y1), vec(x2, y2), rgba(16, 16, 16, 245 * alpha), 8 * u)
            dl:AddRect(vec(x1, y1), vec(x2, y2), white((hovered and 30 or 16) * alpha), 8 * u, nil, 1)
            drawText(dl, ui.fonts.body, x1 + 16 * u, y1 + 14 * u, pal('text', alpha), toast.title)
            for li, line in ipairs(lines) do
                drawText(dl, ui.fonts.desc, x1 + 16 * u, y1 + 18 * u + ui.fonts.body.FontSize + (li - 1) * lineH,
                    pal('hint', alpha), line)
            end
            local life = 1 - age / TOAST_LIFE
            dl:AddRectFilled(vec(x1 + 8 * u, y2 - 3 * u), vec(x1 + 8 * u + (width - 16 * u) * life, y2 - 1 * u),
                white(60 * alpha), 1 * u)
            y = y - height - gap
        end
    end
end

-- ---------------------------------------------------------------- janela (casca)

function ui.headerHeight()
    return 44 * ui.u
end

function ui.footerHeight()
    return 36 * ui.u
end

function ui.pushStyle()
    local u = ui.u
    imgui.PushStyleVarFloat(imgui.StyleVar.WindowRounding, 10 * u)
    imgui.PushStyleVarFloat(imgui.StyleVar.FrameRounding, 9 * u)
    imgui.PushStyleVarFloat(imgui.StyleVar.WindowBorderSize, 1)
    imgui.PushStyleVarVec2(imgui.StyleVar.WindowPadding, vec(18 * u, 16 * u))
    imgui.PushStyleVarVec2(imgui.StyleVar.ItemSpacing, vec(0, 0))
    imgui.PushStyleVarFloat(imgui.StyleVar.ScrollbarSize, 4 * u)
    imgui.PushStyleVarFloat(imgui.StyleVar.ScrollbarRounding, 2 * u)
    imgui.PushStyleVarFloat(imgui.StyleVar.PopupRounding, 8 * u)
    imgui.PushStyleVarFloat(imgui.StyleVar.PopupBorderSize, 1)

    imgui.PushStyleColor(imgui.Col.Text, imgui.ImVec4(0.94, 0.94, 0.94, 1.00))
    imgui.PushStyleColor(imgui.Col.TextDisabled, imgui.ImVec4(0.47, 0.47, 0.47, 1.00))
    imgui.PushStyleColor(imgui.Col.WindowBg, imgui.ImVec4(0.047, 0.047, 0.047, 0.988))
    imgui.PushStyleColor(imgui.Col.PopupBg, imgui.ImVec4(0.055, 0.055, 0.055, 0.988))
    imgui.PushStyleColor(imgui.Col.Border, imgui.ImVec4(1.00, 1.00, 1.00, 0.055))
    imgui.PushStyleColor(imgui.Col.BorderShadow, imgui.ImVec4(0, 0, 0, 0))
    imgui.PushStyleColor(imgui.Col.NavHighlight, imgui.ImVec4(0, 0, 0, 0))
    imgui.PushStyleColor(imgui.Col.ScrollbarBg, imgui.ImVec4(0, 0, 0, 0))
    imgui.PushStyleColor(imgui.Col.ScrollbarGrab, imgui.ImVec4(1, 1, 1, 0.16))
    imgui.PushStyleColor(imgui.Col.ScrollbarGrabHovered, imgui.ImVec4(1, 1, 1, 0.27))
    imgui.PushStyleColor(imgui.Col.ScrollbarGrabActive, imgui.ImVec4(1, 1, 1, 0.39))
    imgui.PushStyleColor(imgui.Col.TextSelectedBg, imgui.ImVec4(1, 1, 1, 0.18))
    imgui.PushStyleColor(imgui.Col.FrameBg, imgui.ImVec4(0, 0, 0, 0))
    imgui.PushStyleColor(imgui.Col.FrameBgHovered, imgui.ImVec4(0, 0, 0, 0))
    imgui.PushStyleColor(imgui.Col.FrameBgActive, imgui.ImVec4(0, 0, 0, 0))
end

function ui.popStyle()
    imgui.PopStyleColor(15)
    imgui.PopStyleVar(9)
end

-- shell = { moved = false, x = 0, y = 0, dragging = false } guarda a posicao arrastada.
-- Devolve false quando o X foi clicado.
function ui.beginShell(id, title, version, width, height, shell, extraFlags)
    local u = ui.u
    local io = imgui.GetIO()
    local screenX, screenY = getScreenResolution()
    width, height = math.ceil(width), math.ceil(height)

    if shell.moved then
        shell.x = math.max(0, math.min(screenX - width, shell.x))
        shell.y = math.max(0, math.min(screenY - height, shell.y))
        imgui.SetNextWindowPos(vec(math.floor(shell.x), math.floor(shell.y)), imgui.Cond.Always)
    else
        imgui.SetNextWindowPos(vec(screenX * 0.5, screenY * 0.5), imgui.Cond.Always, vec(0.5, 0.5))
    end
    imgui.SetNextWindowSize(vec(width, height), imgui.Cond.Always)

    local flags = imgui.WindowFlags.NoTitleBar + imgui.WindowFlags.NoResize + imgui.WindowFlags.NoMove +
        imgui.WindowFlags.NoCollapse + imgui.WindowFlags.NoScrollbar + imgui.WindowFlags.NoScrollWithMouse +
        imgui.WindowFlags.NoSavedSettings + (extraFlags or 0)
    imgui.PushFont(ui.fonts.body)
    imgui.Begin(id, nil, flags)

    local dl = imgui.GetWindowDrawList()
    local pos = imgui.GetWindowPos()
    local size = imgui.GetWindowSize()
    ui.shellPos, ui.shellSize = pos, size
    local headerH = ui.headerHeight()
    local padX = 18 * u

    dl:AddLine(vec(pos.x + padX, pos.y + headerH - 0.5), vec(pos.x + size.x - padX, pos.y + headerH - 0.5),
        pal('separator'), 1)

    local titleSize = textSize(ui.fonts.title, title)
    local versionSize = version and textSize(ui.fonts.desc, version) or vec(0, 0)
    local gap = version and 8 * u or 0
    local titleX = pos.x + (size.x - titleSize.x - gap - versionSize.x) * 0.5
    drawText(dl, ui.fonts.title, titleX, pos.y + (headerH - titleSize.y) * 0.5, pal('text'), title)
    if version then
        drawText(dl, ui.fonts.desc, titleX + titleSize.x + gap, pos.y + (headerH - versionSize.y) * 0.5 + 2 * u,
            pal('version'), version)
    end

    local keepOpen = true
    local closeSide = 28 * u
    local closeX = pos.x + size.x - padX - closeSide + 6 * u
    local closeY = pos.y + (headerH - closeSide) * 0.5
    imgui.SetCursorScreenPos(vec(closeX, closeY))
    if imgui.InvisibleButton('##fecharX', vec(closeSide, closeSide)) then
        keepOpen = false
    end
    local closeHovered = imgui.IsItemHovered()
    if closeHovered then
        hand()
    end
    ui.icon(dl, 'fechar', closeX + closeSide * 0.5, closeY + closeSide * 0.5,
        closeHovered and pal('text') or pal('hint'))

    imgui.SetCursorScreenPos(pos)
    imgui.InvisibleButton('##alca', vec(size.x, headerH))
    if imgui.IsItemActive() and imgui.IsMouseDragging(0) then
        if not shell.moved then
            shell.moved = true
            shell.x, shell.y = pos.x, pos.y
        end
        shell.x = shell.x + io.MouseDelta.x
        shell.y = shell.y + io.MouseDelta.y
        shell.dragging = true
        imgui.SetMouseCursor(imgui.MouseCursor.ResizeAll)
    elseif shell.dragging and not imgui.IsMouseDown(0) then
        shell.dragging = false
    end
    return keepOpen
end

-- hints = { { 'Setas', 'Ajustar' }, { 'Esc', 'Fechar', true } } -- o terceiro campo deixa a dica clicavel.
-- alignRight = estilo dos dialogos (Enter/Esc a direita). Devolve o indice da dica clicada ou nil.
function ui.endShell(hints, alignRight)
    local u = ui.u
    local dl = imgui.GetWindowDrawList()
    local pos, size = ui.shellPos, ui.shellSize
    local font = ui.fonts.desc
    local footerH = ui.footerHeight()
    local footerY = pos.y + size.y - footerH
    local padX = 18 * u

    -- O ImGui recorta o conteudo da janela a meio WindowPadding (9u) das laterais; a faixa do rodape
    -- e o divisor vao de borda a borda, entao saem desse recorte.
    dl:PushClipRect(pos, vec(pos.x + size.x, pos.y + size.y), false)
    dl:AddRectFilled(vec(pos.x + 1, footerY), vec(pos.x + size.x - 1, pos.y + size.y - 1), rgba(0, 0, 0, 46),
        10 * u, imgui.DrawCornerFlags.Bot)
    dl:AddLine(vec(pos.x + 1, footerY + 0.5), vec(pos.x + size.x - 1, footerY + 0.5), pal('separator'), 1)
    dl:PopClipRect()

    local keyGap, hintGap = 6 * u, 22 * u
    local total = 0
    for i, hint in ipairs(hints) do
        total = total + textSize(font, hint[1]).x + keyGap + textSize(font, hint[2]).x
        if i < #hints then
            total = total + hintGap
        end
    end
    local x = alignRight and pos.x + size.x - padX - total or pos.x + padX
    local y = footerY + (footerH - font.FontSize) * 0.5

    local clicked = nil
    for i, hint in ipairs(hints) do
        local keyW, actionW = textSize(font, hint[1]).x, textSize(font, hint[2]).x
        local hovered = false
        if hint[3] then
            imgui.SetCursorScreenPos(vec(x - 4 * u, footerY))
            if imgui.InvisibleButton('##cfgDica' .. i, vec(keyW + keyGap + actionW + 8 * u, footerH)) then
                clicked = i
            end
            hovered = imgui.IsItemHovered()
            if hovered then
                hand()
            end
        end
        drawText(dl, font, x, y, hovered and pal('text') or pal('column'), hint[1])
        drawText(dl, font, x + keyW + keyGap, y, hovered and pal('column') or pal('hint'), hint[2])
        x = x + keyW + keyGap + actionW + hintGap
    end

    imgui.End()
    imgui.PopFont()
    return clicked
end

-- ---------------------------------------------------------------- abas (Trok Radar)

function ui.tabsHeight()
    return 34 * ui.u
end

-- Devolve a aba atual (1..#labels).
function ui.tabs(id, labels, current, x, y, width)
    local u = ui.u
    local dl = imgui.GetWindowDrawList()
    local font = ui.fonts.desc
    local h = ui.tabsHeight()
    local tabW = width / #labels
    for i, label in ipairs(labels) do
        local tx = x + tabW * (i - 1)
        imgui.SetCursorScreenPos(vec(tx, y))
        if imgui.InvisibleButton(id .. '##aba' .. i, vec(tabW, h)) then
            current = i
        end
        local hovered = imgui.IsItemHovered()
        if hovered then
            hand()
        end
        local ts = textSize(font, label)
        local color = current == i and pal('text') or (hovered and pal('column') or pal('hint'))
        drawText(dl, font, tx + (tabW - ts.x) * 0.5, y + (h - ts.y) * 0.5 - 1 * u, color, label)
    end
    local ts = textSize(font, labels[current])
    local ax = anim(id .. 'x', x + tabW * (current - 1) + (tabW - ts.x) * 0.5 - 7 * u, 18)
    local aw = anim(id .. 'w', ts.x + 14 * u, 18)
    dl:AddLine(vec(x, y + h - 0.5), vec(x + width, y + h - 0.5), pal('separator'), 1)
    dl:AddRectFilled(vec(ax, y + h - 2 * u), vec(ax + aw, y + h), pal('text'), 1 * u)
    return current
end

-- ---------------------------------------------------------------- menus flutuantes e campos

-- Item de menu flutuante (lista suspensa, menu do campo). Devolve true no clique.
local function popupItem(id, label, checked, isDefault, width)
    local u = ui.u
    local dl = imgui.GetWindowDrawList()
    local p = imgui.GetCursorScreenPos()
    local h = 26 * u
    local clicked = imgui.InvisibleButton(id, vec(width, h))
    local hovered = imgui.IsItemHovered()
    if hovered then
        hand()
        dl:AddRectFilled(p, vec(p.x + width, p.y + h), white(12), 5 * u)
    end
    local font = ui.fonts.desc
    local ts = textSize(font, label)
    drawText(dl, font, p.x + 10 * u, p.y + (h - ts.y) * 0.5, (hovered or checked) and pal('text') or pal('column'),
        label)
    local right = p.x + width - 10 * u
    if checked then
        ui.icon(dl, 'check', right - 5 * u, p.y + h * 0.5, pal('text'), true)
        right = right - 18 * u
    end
    if isDefault then
        local tag = 'padr\195\163o'
        local tg = textSize(font, tag)
        drawText(dl, font, right - tg.x, p.y + (h - tg.y) * 0.5, pal('hint'), tag)
    end
    return clicked
end

local function setBuffer(buffer, text)
    text = text:sub(1, ffi.sizeof(buffer) - 1)
    ffi.copy(buffer, text)
end

local function clipboardText()
    local clip = imgui.GetClipboardText()
    if clip == nil then
        return ''
    end
    return type(clip) == 'string' and clip or ffi.string(clip)
end

-- Menu do botao direito nos campos (Trok Dialogs): Copiar, Colar, Recortar, Limpar.
local function fieldMenu(popupId, buffer)
    local changed = false
    imgui.PushStyleVarVec2(imgui.StyleVar.WindowPadding, vec(6 * ui.u, 6 * ui.u))
    if imgui.BeginPopup(popupId) then
        ui.popupDrawn = true
        local width = 150 * ui.u
        local text = ffi.string(buffer)
        if popupItem('##copiar', 'Copiar', false, false, width) and text ~= '' then
            imgui.SetClipboardText(text)
        end
        if popupItem('##colar', 'Colar', false, false, width) then
            setBuffer(buffer, text .. clipboardText())
            changed = true
        end
        if popupItem('##recortar', 'Recortar', false, false, width) and text ~= '' then
            imgui.SetClipboardText(text)
            setBuffer(buffer, '')
            changed = true
        end
        if popupItem('##limpar', 'Limpar', false, false, width) then
            setBuffer(buffer, '')
            changed = true
        end
        if ui.closePopup then
            imgui.CloseCurrentPopup()
        end
        imgui.EndPopup()
    end
    imgui.PopStyleVar()
    return changed
end

-- kind: 'texto', 'senha' (olho) ou 'busca' (lupa + limpar).
-- Canais do draw list: 0 fundo da linha, 1 moldura, 2 texto do ImGui. Devolve changed, active.
local function field(dl, id, buffer, placeholder, kind, x, y, w, h, focusNow, multiline)
    local u = ui.u
    local font = ui.fonts.desc
    local reveal = ui.reveal[id] == true
    local iconW = (kind ~= 'texto') and 28 * u or 0
    local leftPad = kind == 'busca' and 24 * u or 0
    local hasText = buffer[0] ~= 0

    -- Botoes de icone antes do campo para ganharem o hover.
    local iconHovered = false
    if kind == 'senha' then
        imgui.SetCursorScreenPos(vec(x + w - iconW, y))
        if imgui.InvisibleButton(id .. '##olho', vec(iconW, h)) then
            reveal = not reveal
            ui.reveal[id] = reveal
        end
        iconHovered = imgui.IsItemHovered()
        if iconHovered then
            ui.tooltip(id .. '##olho', reveal and 'ocultar' or 'ver')
        end
    elseif kind == 'busca' and hasText then
        imgui.SetCursorScreenPos(vec(x + w - iconW, y))
        if imgui.InvisibleButton(id .. '##limpar', vec(iconW, h)) then
            setBuffer(buffer, '')
        end
        iconHovered = imgui.IsItemHovered()
    end
    if iconHovered then
        hand()
    end

    imgui.PushStyleVarVec2(imgui.StyleVar.FramePadding, vec(10 * u, (h - font.FontSize) * 0.5))
    imgui.PushStyleVarFloat(imgui.StyleVar.FrameRounding, 6 * u)
    imgui.PushStyleVarFloat(imgui.StyleVar.FrameBorderSize, 0)
    imgui.PushFont(font)
    imgui.SetCursorScreenPos(vec(x + leftPad, y))
    if focusNow then
        imgui.SetKeyboardFocusHere()
    end
    local flags = (kind == 'senha' and not reveal) and imgui.InputTextFlags.Password or 0
    -- O ImGui escreve ao lado do campo o que vem antes do primeiro "##" (o id "Texto##entrada1" mostrava
    -- "Texto"): o rotulo comeca com "##" para nao aparecer nada.
    local label = '##' .. id
    local changed
    if multiline then
        imgui.PushStyleVarVec2(imgui.StyleVar.FramePadding, vec(10 * u, 7 * u))
        changed = imgui.InputTextMultiline(label, buffer, ffi.sizeof(buffer), vec(w - leftPad, h), flags)
        imgui.PopStyleVar()
    else
        imgui.SetNextItemWidth(w - leftPad - iconW)
        changed = imgui.InputText(label, buffer, ffi.sizeof(buffer), flags)
    end
    local active = imgui.IsItemActive()
    local hovered = imgui.IsItemHovered()
    if hovered then
        imgui.SetMouseCursor(imgui.MouseCursor.TextInput)
    end
    local popupId = '##menuCampo' .. id
    if hovered and imgui.IsMouseClicked(1) then
        imgui.OpenPopup(popupId)
    end
    imgui.PopFont()
    imgui.PopStyleVar(3)
    if fieldMenu(popupId, buffer) then
        changed = true
    end

    dl:ChannelsSetCurrent(1)
    box(dl, x, y, x + w, y + h, white(active and 10 or 6),
        white(active and 70 or ((hovered or iconHovered) and 34 or 22)), 6 * u)
    if buffer[0] == 0 and not active and placeholder then
        local py = multiline and y + 7 * u or y + (h - font.FontSize) * 0.5
        drawText(dl, font, x + leftPad + 10 * u, py, pal('hint'), placeholder)
    end
    if kind == 'senha' then
        -- Olho do lucide, como no Trok Dialogs: aberto = senha visivel, cortado = oculta.
        ui.icon(dl, reveal and 'olho' or 'olhoCortado', x + w - iconW * 0.5, y + h * 0.5,
            iconHovered and pal('text') or pal('hint'), h < 28 * u)
    elseif kind == 'busca' then
        ui.icon(dl, 'busca', x + 13 * u, y + h * 0.5, active and pal('text') or pal('hint'), true)
        if buffer[0] ~= 0 then
            ui.icon(dl, 'fechar', x + w - iconW * 0.5, y + h * 0.5, iconHovered and pal('text') or pal('hint'), true)
        end
    end
    dl:ChannelsSetCurrent(2)
    return changed, active
end

-- Campo solto (dialogos), ocupando a largura que voce der.
function ui.textField(id, buffer, placeholder, password, x, y, w, h, focus)
    local dl = imgui.GetWindowDrawList()
    dl:ChannelsSplit(3)
    dl:ChannelsSetCurrent(2)
    local changed, active = field(dl, id, buffer, placeholder, password and 'senha' or 'texto', x, y, w, h, focus,
        false)
    dl:ChannelsMerge()
    return changed, active
end

-- ---------------------------------------------------------------- linhas numeradas

local Rows = {}
Rows.__index = Rows

-- keys = { up, down, left, right, enter, space, digit } vindos do laco principal.
-- selected = linha selecionada (1..n). Depois de desenhar, leia rows.selected.
-- Valor padrao: barras mostram uma linha vertical no padrao; setas, giro e segmentado mostram um
-- tracinho; todos ganham "Restaurar" quando saem do padrao (a dica mostra qual e o padrao).
function ui.rows(id, x, width, selected, keys, showNumbers)
    local u = ui.u
    local self = setmetatable({}, Rows)
    self.id = id
    self.dl = imgui.GetWindowDrawList()
    self.x, self.w = x, width
    self.selected = selected
    self.keys = keys or {}
    self.numbers = showNumbers ~= false
    self.index = 0
    self.rowH, self.gap, self.recuo = 28 * u, 2 * u, 12 * u
    self.numberW = textSize(ui.fonts.desc, '99').x
    self.labelX = self.numbers and x + self.recuo + self.numberW + 9 * u or x + self.recuo
    self.y = imgui.GetCursorScreenPos().y
    self.keyboardMoved = false
    return self
end

function Rows:isSelected()
    return self.selected == self.index
end

function Rows:key(name)
    return self:isSelected() and self.keys[name]
end

function Rows:uid(suffix)
    return self.id .. suffix .. self.index
end

function Rows:start()
    self.index = self.index + 1
    self.y = imgui.GetCursorScreenPos().y
    self.restore = nil
end

function Rows:right()
    return self.x + self.w - self.recuo * 0.5
end

-- Linha inteira clicavel com fundo de selecao/hover. Submeter depois dos botoes do controle.
function Rows:row(height, controlHovered, selectable)
    selectable = selectable ~= false
    imgui.SetCursorScreenPos(vec(self.x, self.y))
    local clicked = imgui.InvisibleButton(self:uid('##linha'), vec(self.w, height))
    local hovered = imgui.IsItemHovered() or controlHovered
    if clicked and selectable then
        self.selected = self.index
    end
    if selectable then
        if self:isSelected() then
            self.dl:AddRectFilled(vec(self.x, self.y), vec(self.x + self.w, self.y + height), pal('selection'),
                6 * ui.u)
        elseif hovered then
            self.dl:AddRectFilled(vec(self.x, self.y), vec(self.x + self.w, self.y + height), pal('hover'), 6 * ui.u)
        end
    end
    return hovered, clicked
end

function Rows:number(hovered, centerY)
    if not self.numbers then
        return
    end
    local font = ui.fonts.desc
    local number = tostring(self.index)
    local ns = textSize(font, number)
    local color = self:isSelected() and pal('numberSel') or (hovered and pal('numberHover') or pal('number'))
    drawText(self.dl, font, self.x + self.recuo + self.numberW - ns.x, centerY - ns.y * 0.5, color, number)
end

function Rows:label(label, hovered)
    local centerY = self.y + self.rowH * 0.5
    self:number(hovered, centerY)
    ui.richText(self.dl, ui.fonts.body, self.labelX, centerY - ui.fonts.body.FontSize * 0.5, pal('text'), label)
    if self.restore then
        local r = self.restore
        drawText(self.dl, ui.fonts.desc, r.x, self.y + (self.rowH - r.h) * 0.5,
            r.hovered and pal('text') or pal('hint'), 'Restaurar')
    end
end

-- "Restaurar" discreto a esquerda do controle, so quando o valor saiu do padrao (Trok Radar).
-- A dica mostra qual e o valor padrao.
function Rows:restoreButton(differs, controlLeft, defaultText)
    if not differs then
        return false
    end
    local u = ui.u
    local ts = textSize(ui.fonts.desc, 'Restaurar')
    local bx = controlLeft - 12 * u - ts.x
    imgui.SetCursorScreenPos(vec(bx - 4 * u, self.y))
    local clicked = imgui.InvisibleButton(self:uid('##restaurar'), vec(ts.x + 8 * u, self.rowH))
    local hovered = imgui.IsItemHovered()
    if hovered then
        hand()
        ui.tooltip(self:uid('##restaurar'), 'Volta ao padr\195\163o: ' .. tostring(defaultText))
    end
    self.restore = { x = bx, h = ts.y, hovered = hovered }
    return clicked
end

-- Tracinho discreto que marca "este e o valor padrao": 8u de largura, 1.5u de espessura (pelo menos 1 px) e pontas
-- arredondadas, centrado em (x, y).
function ui.defaultMark(dl, x, y, color)
    local u = ui.u
    local h = math.max(1, 1.5 * u) * 0.5
    dl:AddRectFilled(vec(x - 4 * u, y - h), vec(x + 4 * u, y + h), color, h)
end

-- Marca do valor padrao nas setas, no giro e no segmentado.
function Rows:defaultMark(x, y)
    ui.defaultMark(self.dl, x, y, pal('marker'))
end

function Rows:finish(height)
    imgui.SetCursorScreenPos(vec(self.x, self.y))
    imgui.Dummy(vec(self.w, height + self.gap))
    if self.keyboardMoved and self:isSelected() then
        -- Rola a lista para a linha escolhida pelo teclado ficar visivel.
        local winY, winH = imgui.GetWindowPos().y, imgui.GetWindowHeight()
        local scroll = imgui.GetScrollY()
        local pad = self.gap * 4
        if self.y - pad < winY then
            imgui.SetScrollY(scroll - (winY - self.y) - pad)
        elseif self.y + height + pad > winY + winH then
            imgui.SetScrollY(scroll + (self.y + height + pad) - (winY + winH))
        end
    end
    self.y = imgui.GetCursorScreenPos().y
end

-- 1. Acao: botao discreto com contorno (o "Mover lista" do Kill List).
function Rows:action(label, button)
    self:start()
    local u = ui.u
    local bw = math.max(136 * u, textSize(ui.fonts.desc, button).x + 28 * u)
    local bh = 24 * u
    local bx, by = self:right() - bw, self.y + (self.rowH - bh) * 0.5
    imgui.SetCursorScreenPos(vec(bx, by))
    local clicked = imgui.InvisibleButton(self:uid('##acao'), vec(bw, bh))
    local bHovered, bActive = imgui.IsItemHovered(), imgui.IsItemActive()
    if bHovered then
        hand()
    end
    local hovered = self:row(self.rowH, bHovered)
    self:label(label, hovered)
    if self:key('enter') or self:key('space') then
        clicked = true
    end
    if clicked then
        self.selected = self.index
    end
    local lit = bHovered or bActive
    box(self.dl, bx, by, bx + bw, by + bh, white(lit and 16 or 8), white(lit and 40 or 22), 6 * u)
    local ts = textSize(ui.fonts.desc, button)
    drawText(self.dl, ui.fonts.desc, bx + (bw - ts.x) * 0.5, by + (bh - ts.y) * 0.5,
        lit and pal('text') or pal('column'), button)
    self:finish(self.rowH)
    return clicked
end

-- Controle de setas do Kill List: < valor >. Botoes primeiro (ganham o hover), desenho depois.
local function arrowButtons(self)
    local u = ui.u
    local cw, aw = 150 * u, 32 * u
    local vw = cw - 2 * aw
    local cx = self:right() - cw
    imgui.SetCursorScreenPos(vec(cx, self.y))
    local cl = imgui.InvisibleButton(self:uid('##cfgE'), vec(aw, self.rowH))
    local hl = imgui.IsItemHovered()
    imgui.SetCursorScreenPos(vec(cx + aw, self.y))
    local cv = imgui.InvisibleButton(self:uid('##cfgV'), vec(vw, self.rowH))
    local hv = imgui.IsItemHovered()
    imgui.SetCursorScreenPos(vec(cx + aw + vw, self.y))
    local cr = imgui.InvisibleButton(self:uid('##cfgD'), vec(aw, self.rowH))
    local hr = imgui.IsItemHovered()
    return { cx = cx, aw = aw, vw = vw, cl = cl, cv = cv, cr = cr, hl = hl, hv = hv, hr = hr }
end

local function drawArrows(self, a, text, leftOff, rightOff, isDefault)
    local centerY = self.y + self.rowH * 0.5
    ui.icon(self.dl, 'esquerda', a.cx + a.aw * 0.5, centerY,
        leftOff and pal('disabled') or (a.hl and pal('text') or pal('column')))
    ui.icon(self.dl, 'direita', a.cx + a.aw + a.vw + a.aw * 0.5, centerY,
        rightOff and pal('disabled') or (a.hr and pal('text') or pal('column')))
    local ts = textSize(ui.fonts.desc, text)
    drawText(self.dl, ui.fonts.desc, a.cx + a.aw + (a.vw - ts.x) * 0.5, centerY - ts.y * 0.5,
        a.hv and pal('text') or pal('column'), text)
    if isDefault then
        self:defaultMark(a.cx + a.aw + a.vw * 0.5, centerY + 9 * ui.u)
    end
end

-- 2. Numero com setas. Devolve valor, mudou.
function Rows:stepper(label, value, min, max, step, fmt, default)
    self:start()
    local a = arrowButtons(self)
    local leftOff, rightOff = value <= min, value >= max
    a.hl, a.hr = a.hl and not leftOff, a.hr and not rightOff
    local changed = false
    if self:restoreButton(value ~= default, a.cx, string.format(fmt, default)) then
        value, changed = default, true
    end
    if a.hl or a.hr then
        hand()
    end
    if (a.cl or self:key('left')) and not leftOff then
        value, changed = math.max(min, value - step), true
    end
    if (a.cr or self:key('right')) and not rightOff then
        value, changed = math.min(max, value + step), true
    end
    if a.cl or a.cr then
        self.selected = self.index
    end
    local hovered = self:row(self.rowH, a.hl or a.hv or a.hr)
    self:label(label, hovered)
    drawArrows(self, a, string.format(fmt, value), value <= min, value >= max, value == default)
    self:finish(self.rowH)
    return value, changed
end

-- 3. Lista de opcoes que gira (o "Alinhamento" do Kill List). index de 1 a #options.
function Rows:cycle(label, index, options, default)
    self:start()
    local a = arrowButtons(self)
    local changed = false
    if self:restoreButton(index ~= default, a.cx, options[default]) then
        index, changed = default, true
    end
    if a.hl or a.hv or a.hr then
        hand()
    end
    if a.cl or self:key('left') then
        index, changed = (index - 2) % #options + 1, true
    end
    if a.cv or a.cr or self:key('right') or self:key('enter') then
        index, changed = index % #options + 1, true
    end
    if a.cl or a.cv or a.cr then
        self.selected = self.index
    end
    local hovered = self:row(self.rowH, a.hl or a.hv or a.hr)
    self:label(label, hovered)
    drawArrows(self, a, options[index], false, false, index == default)
    self:finish(self.rowH)
    return index, changed
end

-- 4. Interruptor (Trok Radar): trilho escuro -> claro, bolinha clara -> escura.
function Rows:toggle(label, value)
    self:start()
    local u = ui.u
    local tw, th = 40 * u, 22 * u
    local tx, ty = self:right() - tw, self.y + (self.rowH - th) * 0.5
    imgui.SetCursorScreenPos(vec(tx, ty))
    local clicked = imgui.InvisibleButton(self:uid('##b'), vec(tw, th))
    local tHovered = imgui.IsItemHovered()
    if tHovered then
        hand()
    end
    local hovered, rowClicked = self:row(self.rowH, tHovered)
    if rowClicked or self:key('enter') or self:key('space') or self:key('left') or self:key('right') then
        clicked = true
    end
    if clicked then
        value = not value
        self.selected = self.index
    end
    self:label(label, hovered)

    local t = anim(self:uid('##toggle'), value and 1 or 0)
    local track = tHovered and 40 + 200 * t or 32 + 194 * t
    local knobBase = tHovered and 226 or 196
    local knob = knobBase + (18 - knobBase) * t
    local r = th * 0.5
    self.dl:AddRectFilled(vec(tx, ty), vec(tx + tw, ty + th), gray(track), r)
    local border = (1 - t) * (tHovered and 34 or 22)
    self.dl:AddRect(vec(tx - 0.5, ty - 0.5), vec(tx + tw + 0.5, ty + th + 0.5), white(border), r + 0.5, nil, 1)
    self.dl:AddCircleFilled(vec(tx + r + (tw - th) * t, ty + r), r - 3 * u, gray(knob), 24)
    self:finish(self.rowH)
    return value, clicked
end

-- 5. Segmentado: duas ou tres escolhas lado a lado (o "Quadrado | Redondo" do Trok Radar).
function Rows:segmented(label, index, options, default)
    self:start()
    local u = ui.u
    local segW = 0
    for _, option in ipairs(options) do
        segW = math.max(segW, textSize(ui.fonts.desc, option).x + 26 * u)
    end
    local sh, sw = 24 * u, segW * #options
    local sx, sy = self:right() - sw, self.y + (self.rowH - sh) * 0.5
    local changed, hoveredSeg = false, nil
    for i = 1, #options do
        imgui.SetCursorScreenPos(vec(sx + segW * (i - 1), sy))
        if imgui.InvisibleButton(self:uid('##e' .. i .. '_'), vec(segW, sh)) and index ~= i then
            index, changed = i, true
            self.selected = self.index
        end
        if imgui.IsItemHovered() then
            hoveredSeg = i
            hand()
        end
    end
    if self:restoreButton(index ~= default, sx, options[default]) then
        index, changed = default, true
    end
    if self:key('left') and index > 1 then
        index, changed = index - 1, true
    end
    if self:key('right') and index < #options then
        index, changed = index + 1, true
    end
    local hovered = self:row(self.rowH, hoveredSeg ~= nil)
    self:label(label, hovered)
    box(self.dl, sx, sy, sx + sw, sy + sh, white(6), white(18), 6 * u)
    local px = anim(self:uid('##seg'), sx + segW * (index - 1), 20)
    box(self.dl, px + 2 * u, sy + 2 * u, px + segW - 2 * u, sy + sh - 2 * u, white(26), white(34), 5 * u)
    for i, option in ipairs(options) do
        local ts = textSize(ui.fonts.desc, option)
        local color = index == i and pal('text') or (hoveredSeg == i and pal('column') or pal('hint'))
        local segX = sx + segW * (i - 1)
        drawText(self.dl, ui.fonts.desc, segX + (segW - ts.x) * 0.5, sy + (sh - ts.y) * 0.5 - 1 * u, color, option)
        if i == default then
            self:defaultMark(segX + segW * 0.5, sy + sh - 4 * u)
        end
    end
    self:finish(self.rowH)
    return index, changed
end

-- 6. Barra deslizante com valor a direita ("%d px", "%d m", "%d%%"), linha vertical no padrao
-- (Trok Radar) e Restaurar.
function Rows:slider(label, value, min, max, step, fmt, default)
    self:start()
    local u = ui.u
    local valueW, trackW, knobR = 56 * u, 180 * u, 7 * u
    local right = self:right()
    local tx, centerY = right - valueW - 10 * u - trackW, self.y + self.rowH * 0.5
    local changed = false

    imgui.SetCursorScreenPos(vec(tx - knobR, self.y))
    imgui.InvisibleButton(self:uid('##l'), vec(trackW + knobR * 2, self.rowH))
    local sHovered, active = imgui.IsItemHovered(), imgui.IsItemActive()
    if sHovered or active then
        hand()
    end
    if active then
        local f = math.max(0, math.min(1, (imgui.GetIO().MousePos.x - tx) / trackW))
        local v = min + math.floor(f * (max - min) / step + 0.5) * step
        v = math.max(min, math.min(max, v))
        if v ~= value then
            value, changed = v, true
        end
        self.selected = self.index
    end
    if self:restoreButton(value ~= default, tx - knobR, string.format(fmt, default)) then
        value, changed = default, true
    end
    if self:key('left') and value > min then
        value, changed = math.max(min, value - step), true
    end
    if self:key('right') and value < max then
        value, changed = math.min(max, value + step), true
    end

    local hovered = self:row(self.rowH, sHovered)
    self:label(label, hovered)
    local f = (value - min) / (max - min)
    local fx = anim(self:uid('##sl'), tx + trackW * f, 24)
    local th = 4 * u
    self.dl:AddRectFilled(vec(tx, centerY - th * 0.5), vec(tx + trackW, centerY + th * 0.5), white(26), th * 0.5)
    self.dl:AddRectFilled(vec(tx, centerY - th * 0.5), vec(fx, centerY + th * 0.5), pal('strong'), th * 0.5)
    -- Linha vertical no valor padrao, por baixo da bolinha.
    local dx = math.floor(tx + trackW * (default - min) / (max - min)) + 0.5
    self.dl:AddLine(vec(dx, centerY - 7 * u), vec(dx, centerY + 7 * u), white((sHovered or active) and 130 or 90),
        1.5 * u)
    local kr = (sHovered or active) and knobR + 1 * u or knobR
    self.dl:AddCircleFilled(vec(fx, centerY), kr, pal('text'), 24)
    self.dl:AddCircle(vec(fx, centerY), kr, rgba(0, 0, 0, 90), 24, 1)
    local text = string.format(fmt, value)
    local ts = textSize(ui.fonts.desc, text)
    drawText(self.dl, ui.fonts.desc, right - ts.x, centerY - ts.y * 0.5, active and pal('text') or pal('column'), text)
    self:finish(self.rowH)
    return value, changed
end

local function keyName(vk)
    if not vk or vk == 0 then
        return 'Nenhuma'
    end
    local ok, name = pcall(vkeys.id_to_name, vk)
    return (ok and name) or string.format('Tecla %d', vk)
end

-- 7. Tecla de atalho: clique (ou Enter) e aperte a tecla. Esc cancela, Backspace limpa.
function Rows:keybind(label, vk, default)
    self:start()
    local u = ui.u
    local myId = self:uid('##k')
    local capturing = ui.capturing == myId
    local changed = false
    if capturing and ui.captured ~= nil then
        if ui.captured == 'limpar' then
            vk, changed = 0, true
        elseif ui.captured ~= 'cancelar' then
            vk, changed = ui.captured, true
        end
        ui.capturing, ui.captured, capturing = nil, nil, false
    end

    local shown = capturing and 'Pressione uma tecla' or keyName(vk)
    local cw, ch = math.max(64 * u, textSize(ui.fonts.desc, shown).x + 22 * u), 24 * u
    local cx, cy = self:right() - cw, self.y + (self.rowH - ch) * 0.5
    imgui.SetCursorScreenPos(vec(cx, cy))
    local clicked = imgui.InvisibleButton(myId, vec(cw, ch))
    local kHovered = imgui.IsItemHovered()
    if kHovered then
        hand()
    end
    if self:restoreButton(vk ~= default and not capturing, cx, keyName(default)) then
        vk, changed = default, true
    end
    if (clicked or self:key('enter')) and not capturing then
        ui.capturing, ui.captured, capturing = myId, nil, true
        self.selected = self.index
    elseif capturing and imgui.IsMouseClicked(1) then
        ui.capturing, capturing = nil, false
    end

    local hovered = self:row(self.rowH, kHovered)
    self:label(label, hovered)
    local pulse = capturing and 0.55 + 0.45 * math.sin(imgui.GetTime() * 6) or 1
    box(self.dl, cx, cy, cx + cw, cy + ch, white((kHovered or capturing) and 14 or 8),
        white(capturing and 90 * pulse or (kHovered and 40 or 22)), 6 * u)
    local ts = textSize(ui.fonts.desc, shown)
    local color = capturing and pal('text', pulse) or ((vk == 0) and pal('hint') or pal('text'))
    drawText(self.dl, ui.fonts.desc, cx + (cw - ts.x) * 0.5, cy + (ch - ts.y) * 0.5, color, shown)
    self:finish(self.rowH)
    return vk, changed
end

local PRESETS = {
    { 240, 240, 240 }, { 150, 150, 150 }, { 255, 107, 107 }, { 255, 196, 87 },
    { 120, 224, 143 }, { 87, 191, 255 }, { 178, 137, 255 },
}

-- 8. Cor: hex + amostra; o clique abre o seletor (quadrado S/V, barra de matiz, RGB, a cor padrao e
-- cores rapidas). color = { r, g, b } (0..255), alterada no lugar.
function Rows:color(label, color, default)
    self:start()
    local u = ui.u
    local sw, sh = 34 * u, 20 * u
    local sx, sy = self:right() - sw, self.y + (self.rowH - sh) * 0.5
    local hex = hexColor(color)
    local hs = textSize(ui.fonts.desc, hex)
    local hexX = sx - 10 * u - hs.x
    local changed = false

    imgui.SetCursorScreenPos(vec(hexX - 4 * u, self.y))
    local clicked = imgui.InvisibleButton(self:uid('##corA'), vec(sx + sw - hexX + 4 * u, self.rowH))
    local cHovered = imgui.IsItemHovered()
    if cHovered then
        hand()
    end
    if self:restoreButton(not sameColor(color, default), hexX, hexColor(default)) then
        color[1], color[2], color[3] = default[1], default[2], default[3]
        changed = true
    end
    local popupId = self:uid('##corPop')
    if clicked or self:key('enter') then
        self.selected = self.index
        imgui.OpenPopup(popupId)
    end

    local hovered = self:row(self.rowH, cHovered)
    self:label(label, hovered)
    drawText(self.dl, ui.fonts.desc, hexX, self.y + (self.rowH - hs.y) * 0.5, cHovered and pal('text') or pal('column'),
        hex)
    self.dl:AddRectFilled(vec(sx, sy), vec(sx + sw, sy + sh), rgba(color[1], color[2], color[3]), 5 * u)
    self.dl:AddRect(vec(sx - 0.5, sy - 0.5), vec(sx + sw + 0.5, sy + sh + 0.5), white(cHovered and 70 or 40), 5 * u,
        nil, 1)

    imgui.SetNextWindowPos(vec(sx + sw, sy + sh + 6 * u), imgui.Cond.Appearing, vec(1, 0))
    imgui.PushStyleVarVec2(imgui.StyleVar.WindowPadding, vec(6 * u, 6 * u))
    if imgui.BeginPopup(popupId) then
        ui.popupDrawn = true
        local pdl = imgui.GetWindowDrawList()
        local hsv = ui.hsv
        if imgui.IsWindowAppearing() or not hsv or hsv.id ~= popupId then
            local h, s, v = rgbToHsv(color[1] / 255, color[2] / 255, color[3] / 255)
            hsv = { id = popupId, h = h, s = s, v = v }
            ui.hsv = hsv
        end
        local function apply()
            local r, g, b = hsvToRgb(hsv.h, hsv.s, hsv.v)
            color[1], color[2], color[3] = math.floor(r * 255 + 0.5), math.floor(g * 255 + 0.5), math.floor(b * 255 + 0.5)
            changed = true
        end
        local side, barW, pad = 168 * u, 14 * u, 8 * u
        imgui.Dummy(vec(0, pad * 0.5))
        local p = imgui.GetCursorScreenPos()
        local px, py = p.x + pad * 0.5, p.y

        imgui.SetCursorScreenPos(vec(px, py))
        imgui.InvisibleButton('##corSV', vec(side, side))
        if imgui.IsItemActive() then
            local m = imgui.GetIO().MousePos
            hsv.s = math.max(0, math.min(1, (m.x - px) / side))
            hsv.v = 1 - math.max(0, math.min(1, (m.y - py) / side))
            apply()
        end
        local hr, hg, hb = hsvToRgb(hsv.h, 1, 1)
        local hue = rgba(hr * 255, hg * 255, hb * 255)
        pdl:AddRectFilledMultiColor(vec(px, py), vec(px + side, py + side), white(255), hue, hue, white(255))
        pdl:AddRectFilledMultiColor(vec(px, py), vec(px + side, py + side), rgba(0, 0, 0, 0), rgba(0, 0, 0, 0),
            rgba(0, 0, 0, 255), rgba(0, 0, 0, 255))
        pdl:AddRect(vec(px, py), vec(px + side, py + side), white(30), 0, nil, 1)
        local dot = vec(px + hsv.s * side, py + (1 - hsv.v) * side)
        pdl:AddCircle(dot, 6 * u, rgba(0, 0, 0, 160), 16, 3 * u)
        pdl:AddCircle(dot, 6 * u, white(255), 16, 1.5 * u)

        local hx = px + side + pad
        imgui.SetCursorScreenPos(vec(hx, py))
        imgui.InvisibleButton('##corH', vec(barW, side))
        if imgui.IsItemActive() then
            hsv.h = math.max(0, math.min(0.9999, (imgui.GetIO().MousePos.y - py) / side))
            apply()
        end
        for i = 0, 5 do
            local r0, g0, b0 = hsvToRgb(i / 6, 1, 1)
            local r1, g1, b1 = hsvToRgb(((i + 1) % 6) / 6, 1, 1)
            local c0, c1 = rgba(r0 * 255, g0 * 255, b0 * 255), rgba(r1 * 255, g1 * 255, b1 * 255)
            local y0, y1 = py + side * i / 6, py + side * (i + 1) / 6
            pdl:AddRectFilledMultiColor(vec(hx, y0), vec(hx + barW, y1), c0, c0, c1, c1)
        end
        local hy = py + hsv.h * side
        pdl:AddRect(vec(hx - 2 * u, hy - 3 * u), vec(hx + barW + 2 * u, hy + 3 * u), white(255), 2 * u, nil, 1.5 * u)

        local bottom = py + side + pad
        drawText(pdl, ui.fonts.desc, px, bottom, pal('column'),
            string.format('R %d   G %d   B %d', color[1], color[2], color[3]))
        local hexNow = hexColor(color)
        drawText(pdl, ui.fonts.desc, hx + barW - textSize(ui.fonts.desc, hexNow).x, bottom, pal('text'), hexNow)

        -- Primeira amostra = a cor padrao (com o tracinho da casa); depois, cores rapidas.
        local slots = { default }
        for _, preset in ipairs(PRESETS) do
            slots[#slots + 1] = preset
        end
        local presetY = bottom + ui.fonts.desc.FontSize + pad
        local presetW = (side + pad + barW - (#slots - 1) * 4 * u) / #slots
        for i, swatch in ipairs(slots) do
            local x = px + (i - 1) * (presetW + 4 * u)
            imgui.SetCursorScreenPos(vec(x, presetY))
            if imgui.InvisibleButton('##pre' .. i, vec(presetW, 16 * u)) then
                color[1], color[2], color[3] = swatch[1], swatch[2], swatch[3]
                local h, s, v = rgbToHsv(swatch[1] / 255, swatch[2] / 255, swatch[3] / 255)
                hsv.h, hsv.s, hsv.v = h, s, v
                changed = true
            end
            local pHovered = imgui.IsItemHovered()
            if pHovered then
                hand()
                if i == 1 then
                    ui.tooltip(popupId .. '##padrao', 'Cor padr\195\163o (' .. hexColor(default) .. ')')
                end
            end
            pdl:AddRectFilled(vec(x, presetY), vec(x + presetW, presetY + 16 * u), rgba(swatch[1], swatch[2], swatch[3]),
                4 * u)
            if i == 1 then
                local lum = (swatch[1] * 3 + swatch[2] * 6 + swatch[3]) / 10
                ui.defaultMark(pdl, x + presetW * 0.5, presetY + 8 * u, lum > 140 and rgba(0, 0, 0, 150) or white(200))
            end
            if pHovered or sameColor(swatch, color) then
                pdl:AddRect(vec(x - 1.5, presetY - 1.5), vec(x + presetW + 1.5, presetY + 16 * u + 1.5), white(200),
                    5 * u, nil, 1)
            end
        end
        imgui.SetCursorScreenPos(vec(px, presetY + 16 * u))
        imgui.Dummy(vec(side + pad + barW + pad * 0.5, pad))
        if ui.closePopup then
            imgui.CloseCurrentPopup()
        end
        imgui.EndPopup()
    end
    imgui.PopStyleVar()
    self:finish(self.rowH)
    return color, changed
end

-- 9. Lista suspensa (dropdown). index de 1 a #options; a opcao padrao vem marcada "padrao".
function Rows:dropdown(label, index, options, default)
    self:start()
    local u = ui.u
    local dw, dh = 170 * u, 24 * u
    local dx, dy = self:right() - dw, self.y + (self.rowH - dh) * 0.5
    local changed = false
    local popupId = self:uid('##lista')
    imgui.SetCursorScreenPos(vec(dx, dy))
    local clicked = imgui.InvisibleButton(self:uid('##menu'), vec(dw, dh))
    local dHovered = imgui.IsItemHovered()
    if dHovered then
        hand()
    end
    if self:restoreButton(index ~= default, dx, options[default]) then
        index, changed = default, true
    end
    local open = imgui.IsPopupOpen(popupId)
    if clicked or self:key('enter') then
        self.selected = self.index
        imgui.OpenPopup(popupId)
    end
    if self:key('left') and index > 1 then
        index, changed = index - 1, true
    end
    if self:key('right') and index < #options then
        index, changed = index + 1, true
    end

    local hovered = self:row(self.rowH, dHovered)
    self:label(label, hovered)
    local lit = dHovered or open
    box(self.dl, dx, dy, dx + dw, dy + dh, white(lit and 14 or 8), white(open and 60 or (dHovered and 40 or 22)), 6 * u)
    local ts = textSize(ui.fonts.desc, options[index])
    drawText(self.dl, ui.fonts.desc, dx + 10 * u, dy + (dh - ts.y) * 0.5, lit and pal('text') or pal('column'),
        options[index])
    ui.icon(self.dl, open and 'cima' or 'baixo', dx + dw - 14 * u, dy + dh * 0.5, lit and pal('text') or pal('column'),
        true)

    imgui.SetNextWindowPos(vec(dx, dy + dh + 4 * u), imgui.Cond.Always)
    imgui.SetNextWindowSize(vec(dw, 0), imgui.Cond.Always)
    imgui.PushStyleVarVec2(imgui.StyleVar.WindowPadding, vec(6 * u, 6 * u))
    if imgui.BeginPopup(popupId) then
        ui.popupDrawn = true
        for i, option in ipairs(options) do
            if popupItem('##op' .. i, option, index == i, i == default, dw - 12 * u) then
                if index ~= i then
                    index, changed = i, true
                end
                imgui.CloseCurrentPopup()
            end
        end
        if ui.closePopup then
            imgui.CloseCurrentPopup()
        end
        imgui.EndPopup()
    end
    imgui.PopStyleVar()
    self:finish(self.rowH)
    return index, changed
end

local function fieldRow(self, label, kind, id, buffer, placeholder, multilineLines)
    self:start()
    local u = ui.u
    local fw = 240 * u
    local fh = multilineLines and ui.fonts.desc.FontSize * multilineLines + 14 * u or 24 * u
    local rowH = multilineLines and fh + 8 * u or self.rowH
    local fx = self:right() - fw
    local fy = multilineLines and self.y + 4 * u or self.y + (self.rowH - fh) * 0.5
    self.dl:ChannelsSplit(3)
    self.dl:ChannelsSetCurrent(2)
    local changed, active = field(self.dl, self:uid(id), buffer, placeholder, kind, fx, fy, fw, fh, self:key('enter'),
        multilineLines ~= nil)
    if active then
        self.selected = self.index
    end
    self.dl:ChannelsSetCurrent(0)
    local hovered = self:row(rowH, false)
    self:label(label, hovered)
    self.dl:ChannelsMerge()
    self:finish(rowH)
    return changed
end

-- 10. Campo de texto (Enter na linha comeca a digitar). password = true mostra o olho.
function Rows:input(label, buffer, placeholder, password)
    return fieldRow(self, label, password and 'senha' or 'texto', password and '##senha' or '##entrada', buffer,
        placeholder)
end

-- 11. Busca: lupa a esquerda e X para limpar.
function Rows:search(label, buffer, placeholder)
    return fieldRow(self, label, 'busca', '##busca', buffer, placeholder)
end

-- 12. Texto longo, varias linhas.
function Rows:multiline(label, buffer, placeholder, lines)
    return fieldRow(self, label, 'texto', '##texto', buffer, placeholder, lines or 3)
end

-- 13. Barra de progresso (ou indeterminada, sem porcentagem).
function Rows:progress(label, fraction, indeterminate)
    self:start()
    local u = ui.u
    local bw, bh, valueW = 200 * u, 6 * u, 44 * u
    local right = self:right()
    local bx, centerY = right - valueW - 10 * u - bw, self.y + self.rowH * 0.5
    local hovered = self:row(self.rowH, false)
    self:label(label, hovered)
    self.dl:AddRectFilled(vec(bx, centerY - bh * 0.5), vec(bx + bw, centerY + bh * 0.5), white(16), bh * 0.5)
    local text
    if indeterminate then
        local t = (imgui.GetTime() * 0.8) % 1
        local seg = bw * 0.3
        local s0 = bx - seg + (bw + seg) * t
        local a, b = math.max(bx, s0), math.min(bx + bw, s0 + seg)
        if b > a then
            self.dl:AddRectFilled(vec(a, centerY - bh * 0.5), vec(b, centerY + bh * 0.5), pal('strong'), bh * 0.5)
        end
        text = '...'
    else
        fraction = math.max(0, math.min(1, fraction))
        if fraction > 0 then
            self.dl:AddRectFilled(vec(bx, centerY - bh * 0.5), vec(bx + bw * fraction, centerY + bh * 0.5),
                pal('strong'), bh * 0.5)
        end
        text = string.format('%d%%', math.floor(fraction * 100 + 0.5))
    end
    local ts = textSize(ui.fonts.desc, text)
    drawText(self.dl, ui.fonts.desc, right - ts.x, centerY - ts.y * 0.5, pal('column'), text)
    self:finish(self.rowH)
end

-- 14. Carregando: anel girando + status.
function Rows:loading(label, status)
    self:start()
    local u = ui.u
    local right, centerY = self:right(), self.y + self.rowH * 0.5
    local hovered = self:row(self.rowH, false)
    self:label(label, hovered)
    local ts = textSize(ui.fonts.desc, status)
    drawText(self.dl, ui.fonts.desc, right - ts.x, centerY - ts.y * 0.5, pal('column'), status)
    spinner(self.dl, right - ts.x - 18 * u, centerY, 7 * u, imgui.GetTime())
    self:finish(self.rowH)
end

-- 15. Etiquetas. badges = { { 'Novo', 'forte' }, { 'Beta', 'contorno' }, { 'v3.3' } }
function Rows:badges(label, badges)
    self:start()
    local u = ui.u
    local right, centerY, bh = self:right(), self.y + self.rowH * 0.5, 20 * u
    local hovered = self:row(self.rowH, false)
    self:label(label, hovered)
    local x = right
    for i = #badges, 1, -1 do
        local text, kind = badges[i][1], badges[i][2]
        local ts = textSize(ui.fonts.desc, text)
        local bw = ts.x + 16 * u
        x = x - bw
        local color = pal('column')
        if kind == 'forte' then
            self.dl:AddRectFilled(vec(x, centerY - bh * 0.5), vec(x + bw, centerY + bh * 0.5), pal('strong'), bh * 0.5)
            color = pal('ink')
        elseif kind == 'contorno' then
            box(self.dl, x, centerY - bh * 0.5, x + bw, centerY + bh * 0.5, nil, white(46), bh * 0.5)
        else
            self.dl:AddRectFilled(vec(x, centerY - bh * 0.5), vec(x + bw, centerY + bh * 0.5), white(16), bh * 0.5)
        end
        drawText(self.dl, ui.fonts.desc, x + 8 * u, centerY - ts.y * 0.5, color, text)
        x = x - 6 * u
    end
    self:finish(self.rowH)
end

-- 16. Linha com (i): a dica aparece apos um instante com o mouse parado em cima.
function Rows:info(label, tooltip)
    self:start()
    local u = ui.u
    local right, centerY, r = self:right(), self.y + self.rowH * 0.5, 9 * u
    imgui.SetCursorScreenPos(vec(right - 2 * r - 4 * u, self.y))
    imgui.InvisibleButton(self:uid('##info'), vec(2 * r + 8 * u, self.rowH))
    local iHovered = imgui.IsItemHovered()
    local hovered = self:row(self.rowH, iHovered)
    self:label(label, hovered)
    ui.icon(self.dl, 'info', right - r, centerY, iHovered and pal('text') or pal('hint'))
    if iHovered or (self:isSelected() and self.keys.holdEnter) then
        ui.tooltip(self:uid('##info'), tooltip)
    end
    self:finish(self.rowH)
end

-- Titulo de secao (nao conta como linha).
function Rows:section(title)
    local u = ui.u
    self.y = imgui.GetCursorScreenPos().y
    local h = 26 * u
    local ts = textSize(ui.fonts.desc, title)
    local ty = self.y + h - ts.y - 4 * u
    drawText(self.dl, ui.fonts.desc, self.x + self.recuo, ty, pal('hint'), title)
    local lx = self.x + self.recuo + ts.x + 10 * u
    self.dl:AddLine(vec(lx, ty + ts.y * 0.5 + 1), vec(self:right(), ty + ts.y * 0.5 + 1), pal('separator'), 1)
    imgui.SetCursorScreenPos(vec(self.x, self.y))
    imgui.Dummy(vec(self.w, h))
end

local function activated(self)
    local digit = self.keys.digit == self.index
    if digit then
        self.selected = self.index
    end
    return (imgui.IsItemHovered() and imgui.IsMouseDoubleClicked(0)) or self:key('enter') or digit
end

-- 17. Item de lista numerada (Trok Dialogs): Enter, clique duplo ou a tecla do numero.
function Rows:item(text)
    self:start()
    local hovered = self:row(self.rowH, false)
    local fired = activated(self)
    self:label(text, hovered)
    self:finish(self.rowH)
    return fired
end

-- 18. Cabecalho de tabela (lista do SA-MP com colunas). widths em pixels de 1080p.
function Rows:tableHeader(columns, widths)
    local u = ui.u
    self.y = imgui.GetCursorScreenPos().y
    local h, x = 24 * u, self.labelX
    for i, column in ipairs(columns) do
        local ts = textSize(ui.fonts.desc, column)
        drawText(self.dl, ui.fonts.desc, x, self.y + (h - ts.y) * 0.5, pal('hint'), column)
        x = x + widths[i] * u
    end
    self.dl:AddLine(vec(self.x + self.recuo, self.y + h - 0.5), vec(self:right(), self.y + h - 0.5), pal('separator'),
        1)
    imgui.SetCursorScreenPos(vec(self.x, self.y))
    imgui.Dummy(vec(self.w, h + self.gap))
end

-- 19. Linha de tabela.
function Rows:tableItem(cells, widths)
    self:start()
    local u = ui.u
    local hovered = self:row(self.rowH, false)
    local fired = activated(self)
    local centerY = self.y + self.rowH * 0.5
    self:number(hovered, centerY)
    local x = self.labelX
    for i, cell in ipairs(cells) do
        local font = i == 1 and ui.fonts.body or ui.fonts.desc
        ui.richText(self.dl, font, x, centerY - font.FontSize * 0.5, i == 1 and pal('text') or pal('column'), cell)
        x = x + widths[i] * u
    end
    self:finish(self.rowH)
    return fired
end

local function markRow(self, text, hovered, drawMark)
    local u = ui.u
    local centerY = self.y + self.rowH * 0.5
    self:number(hovered, centerY)
    drawMark(self.labelX, centerY, 16 * u)
    ui.richText(self.dl, ui.fonts.body, self.labelX + 26 * u, centerY - ui.fonts.body.FontSize * 0.5, pal('text'), text)
end

-- 20. Varias escolhas (caixa de marcar). Devolve marcado, mudou.
function Rows:checkItem(text, checked)
    self:start()
    local u = ui.u
    local hovered, clicked = self:row(self.rowH, false)
    local toggled = clicked or self:key('enter') or self:key('space') or self.keys.digit == self.index
    if toggled then
        checked = not checked
        self.selected = self.index
    end
    local t = anim(self:uid('##chk'), checked and 1 or 0, 20)
    markRow(self, text, hovered, function(x, cy, s)
        box(self.dl, x, cy - s * 0.5, x + s, cy + s * 0.5, white(6), white(hovered and 80 or 60), 4 * u)
        if t > 0.01 then
            self.dl:AddRectFilled(vec(x, cy - s * 0.5), vec(x + s, cy + s * 0.5), pal('strong', t), 4 * u)
            ui.icon(self.dl, 'check', x + s * 0.5, cy, pal('ink', t), true)
        end
    end)
    self:finish(self.rowH)
    return checked, toggled
end

-- 21. Uma escolha (bolinha). Devolve o grupo atualizado e se esta linha foi escolhida.
function Rows:radioItem(text, group, value)
    self:start()
    local u = ui.u
    local hovered, clicked = self:row(self.rowH, false)
    local picked = clicked or self:key('enter') or self:key('space') or self.keys.digit == self.index
    if picked then
        group = value
        self.selected = self.index
    end
    local t = anim(self:uid('##radio'), group == value and 1 or 0, 20)
    markRow(self, text, hovered, function(x, cy, s)
        local c = vec(x + s * 0.5, cy)
        self.dl:AddCircleFilled(c, s * 0.5, white(6), 24)
        self.dl:AddCircle(c, s * 0.5 - 0.5, white(hovered and 80 or 60), 24, 1)
        if t > 0.01 then
            self.dl:AddCircleFilled(c, s * 0.5, pal('strong', t), 24)
            self.dl:AddCircleFilled(c, s * 0.18 * t, pal('ink'), 16)
        end
    end)
    self:finish(self.rowH)
    return group, picked
end

-- Inicio/fim de quadro do kit (popups abertos, dica sem hover).
function ui.beginFrame()
    ui.popupDrawn = false
    ui.tipSeen = false
end

function ui.endFrame()
    ui.popupOpen = ui.popupDrawn
    ui.closePopup = false
    if not ui.tipSeen then
        ui.tipId = nil
    end
end

-- =====================================================================================
-- Fim do kit. Daqui para baixo e so a vitrine.
-- =====================================================================================

-- O mimgui e o WindowsMouse nao podem trocar o cursor ao mesmo tempo (igual ao Kill List):
-- publicamos o pedido na janela do jogo (1 = maozinha, 2 = I de texto, ausente = seta).
local GTA_WINDOW_HANDLE = 0xC8CF88
local CURSOR_PROPERTY = 'TrokCursor.Pedido'
local cursorWindow = nil
local cursorRequest = -1

local function getGameWindow()
    local raw = readMemory(GTA_WINDOW_HANDLE, 4, false)
    if type(raw) ~= 'number' or raw < 0x10000 then
        return nil
    end
    local window = ffi.cast('void*', raw)
    return user32.IsWindow(window) ~= 0 and window or nil
end

local function publishCursor(cursor)
    local request = cursor == imgui.MouseCursor.Hand and 1 or (cursor == imgui.MouseCursor.TextInput and 2 or 0)
    local window = getGameWindow()
    if not window then
        return
    end
    if cursorWindow ~= nil and cursorWindow ~= window and cursorRequest > 0 then
        user32.RemovePropA(cursorWindow, CURSOR_PROPERTY)
        cursorRequest = -1
    end
    cursorWindow = window
    if request == cursorRequest then
        return
    end
    cursorRequest = request
    if request > 0 then
        user32.SetPropA(window, CURSOR_PROPERTY, ffi.cast('void*', request))
    else
        user32.RemovePropA(window, CURSOR_PROPERTY)
    end
end

local function releaseCursor()
    if cursorWindow ~= nil and cursorRequest > 0 and user32.IsWindow(cursorWindow) ~= 0 then
        user32.RemovePropA(cursorWindow, CURSOR_PROPERTY)
    end
    cursorWindow = nil
    cursorRequest = -1
end

local TABS = { 'Linhas', 'Texto', 'Listas', 'Avisos', 'Di\195\161logos' }

local DEFAULTS = {
    lines = 7,
    align = 2,
    numbers = true,
    blur = true,
    shape = 2,
    iconSpot = 1,
    size = 100,
    border = 2,
    zoom = 250,
    mapKey = 0x4D, -- M
    mapColor = { 230, 230, 230 },
    font = 1,
    checks = { true, false, true },
    radio = 2,
}

local function freshValues()
    local v = {}
    for k, value in pairs(DEFAULTS) do
        if type(value) == 'table' then
            v[k] = { unpack(value) }
        else
            v[k] = value
        end
    end
    return v
end

local S = {
    open = false,
    tab = 1,
    selected = { 1, 1, 1, 1, 1 },
    rowCount = { 1, 1, 1, 1, 1 },
    shell = { moved = false, x = 0, y = 0, dragging = false },
    v = freshValues(),
    moving = false,
    movingDrag = false,
    preview = nil,
    previewStart = nil,
    dialog = nil,
    dialogShell = { moved = false, x = 0, y = 0, dragging = false },
    dialogSelected = 1,
    dialogRows = 1,
    confirm = false,
    -- Quadros desde que cada tela abriu. No primeiro quadro o teclado e ignorado: o Enter que
    -- mandou o /trokuilua (ou que abriu a tela) nao pode acionar nada dentro dela.
    menuAge = 0,
    dialogAge = 0,
    confirmAge = 0,
}

local buffers = {
    name = imgui.new.char[64](),
    password = imgui.new.char[64](),
    search = imgui.new.char[64](),
    note = imgui.new.char[256](),
    dialog = imgui.new.char[64](),
}

-- Teclas lidas em onWindowMessage e entregues ao proximo quadro.
local pending = {}

local function takeKeys(allowed)
    local keys = pending
    pending = {}
    if not allowed then
        return {}
    end
    return keys
end

local function uiActive()
    return S.open or S.moving or S.dialog ~= nil
end

local function keyboardFree()
    return not imgui.GetIO().WantTextInput and not ui.popupOpen and ui.capturing == nil
end

local function moveSelection(selected, count, keys)
    if count <= 0 then
        return selected, false
    end
    if keys.up then
        return (selected - 2) % count + 1, true
    elseif keys.down then
        return selected % count + 1, true
    end
    return selected, false
end

-- ---------------------------------------------------------------- abas do menu

local function tabLines(r)
    local v = S.v
    if r:action('Posi\195\167\195\163o da pr\195\169via', 'Mover pr\195\169via') then
        S.moving = true
        S.movingDrag = false
        S.previewStart = S.preview and { S.preview[1], S.preview[2] } or nil
    end
    v.lines = r:stepper('Linhas vis\195\173veis', v.lines, 1, 20, 1, '%d', DEFAULTS.lines)
    v.align = r:cycle('Alinhamento', v.align, { 'Esquerda', 'Direita' }, DEFAULTS.align)
    v.numbers = r:toggle('N\195\186meros nas linhas', v.numbers)
    v.blur = r:toggle('Desfocar o fundo', v.blur)
    v.shape = r:segmented('Formato', v.shape, { 'Quadrado', 'Redondo' }, DEFAULTS.shape)
    v.iconSpot = r:segmented('\195\141cones', v.iconSpot, { 'Dentro', 'Na borda' }, DEFAULTS.iconSpot)
    v.size = r:slider('Tamanho', v.size, 80, 130, 5, '%d%%', DEFAULTS.size)
    v.border = r:slider('Espessura da borda', v.border, 0, 8, 1, '%d px', DEFAULTS.border)
    v.zoom = r:slider('Zoom (alcance)', v.zoom, 100, 600, 10, '%d m', DEFAULTS.zoom)
    v.mapKey = r:keybind('Mapa r\195\161pido', v.mapKey, DEFAULTS.mapKey)
    r:color('Cor do mapa', v.mapColor, DEFAULTS.mapColor)
    v.font = r:dropdown('Fonte do texto', v.font, { 'Normal', 'Compacta', 'Grande', 'Monoespa\195\167ada' }, DEFAULTS.font)
end

local function tabText(r)
    r:input('Nome', buffers.name, 'Seu nome no servidor')
    r:input('Senha', buffers.password, 'Nunca \195\169 lembrada', true)
    r:search('Buscar', buffers.search, 'Digite para filtrar')
    r:multiline('Observa\195\167\195\163o', buffers.note, 'Texto longo, v\195\161rias linhas')
    r:info('Menu do campo', 'Bot\195\163o direito em qualquer campo abre Copiar, Colar, Recortar e Limpar. '
        .. 'Enter come\195\167a a digitar na linha selecionada; Esc sai do campo.')
end

local function tabLists(r)
    local v = S.v
    r:section('Lista numerada  \194\183  teclas 1 a 9')
    for _, item in ipairs({ 'Spawnar no hospital', 'Spawnar em casa', 'Spawnar na fac\195\167\195\163o' }) do
        if r:item(item) then
            ui.toast('Item escolhido', item)
        end
    end

    r:section('Tabela com cabe\195\167alho')
    local widths = { 220, 120, 80 }
    r:tableHeader({ 'Jogador', 'N\195\173vel', 'Ping' }, widths)
    for _, row in ipairs({ { 'Victor_Trok', '32', '41 ms' }, { 'Enzo_Rampani', '18', '63 ms' },
        { 'Catharina', '25', '38 ms' } }) do
        if r:tableItem(row, widths) then
            ui.toast('Linha escolhida', row[1])
        end
    end

    r:section('V\195\161rias escolhas')
    v.checks[1] = r:checkItem('Mostrar territ\195\179rios', v.checks[1])
    v.checks[2] = r:checkItem('Rota de miss\195\163o', v.checks[2])
    v.checks[3] = r:checkItem('Indicador de norte', v.checks[3])

    r:section('Uma escolha')
    v.radio = r:radioItem('Norte fixo', v.radio, 1)
    v.radio = r:radioItem('Gira com a c\195\162mera', v.radio, 2)
    v.radio = r:radioItem('Personagem centralizado', v.radio, 3)
end

local function tabFeedback(r)
    r:progress('Download', (imgui.GetTime() * 0.18) % 1)
    r:progress('Sincronizando', 0, true)
    r:loading('Conectando', 'Aguarde')
    r:badges('Etiquetas', { { 'Novo', 'forte' }, { 'Beta', 'contorno' }, { 'v3.3' } })
    r:info('Dica', 'Textos de apoio aparecem ap\195\179s um instante com o mouse parado em cima.')
    if r:action('Notifica\195\167\195\163o', 'Mostrar') then
        ui.toast('Trok UI', 'Notifica\195\167\195\163o no canto da tela. Clique para dispensar.')
    end
    if r:action('Confirma\195\167\195\163o', 'Abrir') then
        S.confirm = true
        S.confirmAge = 0
    end
end

local function openDialog(kind)
    S.dialog = kind
    S.dialogSelected = 1
    S.dialogAge = 0
    S.dialogShell = { moved = false, x = 0, y = 0, dragging = false }
    ffi.fill(buffers.dialog, ffi.sizeof(buffers.dialog))
end

local function tabDialogs(r)
    if r:action('Mensagem', 'Abrir') then openDialog('mensagem') end
    if r:action('Entrada de texto', 'Abrir') then openDialog('entrada') end
    if r:action('Senha', 'Abrir') then openDialog('senha') end
    if r:action('Lista', 'Abrir') then openDialog('lista') end
    if r:action('Tabela com cabe\195\167alho', 'Abrir') then openDialog('tabela') end
end

-- Sem dica de Tab: no SA-MP o Tab abre o placar (as abas trocam com o mouse).
local MENU_HINTS = { { 'Setas', 'Ajustar' }, { 'Esc', 'Fechar', true } }

local function drawMenu()
    local u = ui.u
    local fresh = S.menuAge == 0
    S.menuAge = S.menuAge + 1
    -- Com a confirmacao aberta, as teclas sao dela (o menu nao pode consumi-las antes).
    local keys = S.confirm and {} or takeKeys(not fresh and keyboardFree())

    local _, screenY = getScreenResolution()
    local width = 640 * u
    local height = math.min(screenY * 0.72, 540 * u)
    ui.pushStyle()
    -- Com a confirmacao na frente, o menu nao recebe cliques (fica escurecido por baixo).
    local flags = S.confirm and imgui.WindowFlags.NoInputs or 0
    local keep = ui.beginShell('##trokUiShowcaseLua', 'Trok UI', VERSION_TAG, width, height, S.shell, flags)

    local pos = imgui.GetWindowPos()
    local padX = 18 * u
    S.tab = ui.tabs('##abas', TABS, S.tab, pos.x + padX, pos.y + ui.headerHeight(), width - 2 * padX)

    local listTop = pos.y + ui.headerHeight() + ui.tabsHeight() + 10 * u
    local listH = height - (listTop - pos.y) - ui.footerHeight() - 8 * u
    imgui.SetCursorScreenPos(vec(pos.x + padX - 4 * u, listTop))
    imgui.BeginChild('##lista', vec(width - 2 * padX + 8 * u, listH), false, imgui.WindowFlags.NoBackground)
    local cp = imgui.GetCursorScreenPos()
    local rowW = imgui.GetContentRegionAvail().x - 10 * u

    local selected, moved = moveSelection(S.selected[S.tab], S.rowCount[S.tab], keys)
    local r = ui.rows(TABS[S.tab], cp.x + 4 * u, rowW, selected, keys, S.v.numbers)
    r.keyboardMoved = moved
    if S.tab == 1 then
        tabLines(r)
    elseif S.tab == 2 then
        tabText(r)
    elseif S.tab == 3 then
        tabLists(r)
    elseif S.tab == 4 then
        tabFeedback(r)
    else
        tabDialogs(r)
    end
    S.selected[S.tab] = r.selected
    S.rowCount[S.tab] = r.index
    imgui.Dummy(vec(0, 6 * u))
    imgui.EndChild()

    if S.confirm then
        -- Veu sobre o menu e o jogo; a confirmacao vem por cima, em outra janela.
        local dl = imgui.GetWindowDrawList()
        local screenX = getScreenResolution()
        dl:PushClipRectFullScreen()
        dl:AddRectFilled(vec(0, 0), vec(screenX, screenY), rgba(0, 0, 0, 110))
        dl:PopClipRect()
    end

    local clicked = ui.endShell(MENU_HINTS)
    ui.popStyle()

    if not keep or clicked == 2 or keys.escape then
        S.open = false
    end
end

-- ---------------------------------------------------------------- confirmacao

local function drawConfirm()
    local u = ui.u
    local fresh = S.confirmAge == 0
    S.confirmAge = S.confirmAge + 1
    local keys = takeKeys(not fresh)
    local width, height = 400 * u, ui.headerHeight() + 92 * u + ui.footerHeight()
    ui.pushStyle()
    if fresh then
        imgui.SetNextWindowFocus()
    end
    local keep = ui.beginShell('##trokConfirmarLua', 'Restaurar padr\195\181es?', nil, width, height, {})
    local pos = imgui.GetWindowPos()
    ui.richText(imgui.GetWindowDrawList(), ui.fonts.body, pos.x + 22 * u, pos.y + ui.headerHeight() + 20 * u,
        pal('text'), 'Todas as linhas voltam ao valor padr\195\163o.\n{9A9A9A}Isso n\195\163o pode ser desfeito.')
    local clicked = ui.endShell({ { 'Enter', 'Confirmar', true }, { 'Esc', 'Cancelar', true } }, true)
    ui.popStyle()

    if clicked == 1 or keys.enter then
        S.v = freshValues()
        S.confirm = false
        ui.toast('Padr\195\181es restaurados', 'Todas as linhas voltaram ao valor original.')
    elseif not keep or clicked == 2 or keys.escape then
        S.confirm = false
    end
end

-- ---------------------------------------------------------------- dialogos de exemplo (Trok Dialogs)

local DIALOGS = {
    mensagem = {
        title = 'Bem-vindo',
        body = '{FFFFFF}Bem-vindo ao {9AD0FF}Trok Roleplay{FFFFFF}!\n-> As regras est\195\163o em {FFD27A}/regras{FFFFFF}.\n'
            .. '>> Setas do servidor viram \195\173cones do lucide.\n{9A9A9A}Cores no formato {RRGGBB} funcionam.',
        ok = 'Ok',
    },
    entrada = { title = 'Nome do ve\195\173culo', body = 'Digite um nome para o seu ve\195\173culo:', ok = 'Enviar',
        cancel = 'Cancelar', field = true },
    senha = { title = 'Login', body = 'Esta conta est\195\161 registrada.\nDigite a sua senha para entrar:', ok = 'Entrar',
        cancel = 'Sair', field = true, password = true },
    lista = { title = 'Spawn', ok = 'Selecionar', cancel = 'Cancelar',
        items = { '> Hospital', '> Casa', '> Fac\195\167\195\163o', '-> Emprego', '\194\187 \195\154ltima posi\195\167\195\163o' } },
    tabela = { title = 'Jogadores online', ok = 'Selecionar', cancel = 'Cancelar',
        columns = { 'Jogador', 'N\195\173vel', 'Ping' }, widths = { 200, 110, 80 },
        rows = { { 'Victor_Trok', '32', '41 ms' }, { 'Enzo_Rampani', '18', '63 ms' }, { 'Catharina', '25', '38 ms' },
            { 'Camilla', '12', '55 ms' } } },
}

local function drawDialog()
    local u = ui.u
    local d = DIALOGS[S.dialog]
    local screenX, screenY = getScreenResolution()
    local fresh = S.dialogAge == 0
    S.dialogAge = S.dialogAge + 1
    local keysOk = not fresh and not ui.popupOpen
    local keys = takeKeys(keysOk)
    local isList = d.items ~= nil or d.rows ~= nil

    if S.v.blur then
        -- Os dialogos da casa desfocam o jogo; aqui, um veu atras de todas as janelas.
        imgui.GetBackgroundDrawList():AddRectFilled(vec(0, 0), vec(screenX, screenY), rgba(0, 0, 0, 110))
    end

    local bodyW, bodyH = 0, 0
    if d.body then
        bodyW, bodyH = ui.richText(nil, ui.fonts.body, 0, 0, pal('text'), d.body, false)
    end
    local padX = 22 * u
    local width = math.max(380 * u, bodyW + padX * 2)
    local content = bodyH
    if d.field then
        content = content + 14 * u + 30 * u
    end
    if d.items then
        width = math.max(width, 420 * u)
        content = content + #d.items * 30 * u
    elseif d.rows then
        width = math.max(width, 480 * u)
        content = content + 26 * u + #d.rows * 30 * u
    end
    local height = math.min(ui.headerHeight() + 18 * u + content + 18 * u + ui.footerHeight(), screenY * 0.8)

    ui.pushStyle()
    if fresh then
        imgui.SetNextWindowFocus()
    end
    local keep = ui.beginShell('##trokDialogLua', d.title, nil, width, height, S.dialogShell)
    local pos = imgui.GetWindowPos()
    local y = pos.y + ui.headerHeight() + 18 * u
    if d.body then
        ui.richText(imgui.GetWindowDrawList(), ui.fonts.body, pos.x + padX, y, pal('text'), d.body)
        y = y + bodyH
    end
    local chosen = nil
    local submit = keys.enter or keys.enterInField
    if d.field then
        y = y + 14 * u
        ui.textField('##entradaDialogo', buffers.dialog, d.password and 'Senha' or 'Digite aqui', d.password,
            pos.x + padX, y, width - 2 * padX, 30 * u, fresh)
    elseif isList then
        imgui.SetCursorScreenPos(vec(pos.x + padX - 12 * u, y))
        local selected, moved = moveSelection(S.dialogSelected, S.dialogRows, keys)
        local r = ui.rows('##listaDialogo', pos.x + padX - 12 * u, width - 2 * padX + 24 * u, selected, keys, true)
        r.keyboardMoved = moved
        if d.items then
            for i, item in ipairs(d.items) do
                if r:item(item) then
                    chosen = i
                end
            end
        else
            r:tableHeader(d.columns, d.widths)
            for i, row in ipairs(d.rows) do
                if r:tableItem(row, d.widths) then
                    chosen = i
                end
            end
        end
        S.dialogSelected = r.selected
        S.dialogRows = r.index
        submit = false
    end

    local hints = { { 'Enter', d.ok, true } }
    if d.cancel then
        hints[2] = { 'Esc', d.cancel, true }
    end
    local clicked = ui.endShell(hints, true)
    ui.popStyle()

    if chosen or clicked == 1 or submit then
        local message
        if isList then
            message = string.format('Bot\195\163o %s, item %d.', d.ok, chosen or S.dialogSelected)
        elseif d.field then
            message = string.format('Bot\195\163o %s, %d caracteres digitados.', d.ok, #ffi.string(buffers.dialog))
        else
            message = string.format('Bot\195\163o %s.', d.ok)
        end
        ui.toast('Resposta do di\195\161logo', message)
        S.dialog = nil
    elseif not keep or clicked == 2 or keys.escape or keys.escapeInField then
        ui.toast('Resposta do di\195\161logo', d.cancel and 'Cancelado (bot\195\163o direito).' or 'Fechado.')
        S.dialog = nil
    end
end

-- ---------------------------------------------------------------- modo de mover (Trok Radar / Kill List)

local function drawMove()
    local u = ui.u
    local io = imgui.GetIO()
    local screenX, screenY = getScreenResolution()
    local keys = takeKeys(true)
    local cw, ch = 260 * u, 64 * u
    if not S.preview then
        S.preview = { screenX - cw - 40 * u, 120 * u }
    end
    if not S.previewStart then
        S.previewStart = { S.preview[1], S.preview[2] }
    end

    imgui.SetNextWindowPos(vec(0, 0), imgui.Cond.Always)
    imgui.SetNextWindowSize(vec(screenX, screenY), imgui.Cond.Always)
    imgui.PushStyleVarFloat(imgui.StyleVar.WindowBorderSize, 0)
    imgui.PushStyleVarVec2(imgui.StyleVar.WindowPadding, vec(0, 0))
    imgui.Begin('##trokMoverLua', nil, imgui.WindowFlags.NoDecoration + imgui.WindowFlags.NoBackground +
        imgui.WindowFlags.NoMove + imgui.WindowFlags.NoSavedSettings)
    local dl = imgui.GetWindowDrawList()

    imgui.SetCursorScreenPos(vec(S.preview[1], S.preview[2]))
    imgui.InvisibleButton('##previa', vec(cw, ch))
    local hovered = imgui.IsItemHovered()
    if imgui.IsItemActive() and imgui.IsMouseDragging(0, 0) then
        S.movingDrag = true
        S.preview[1] = math.max(0, math.min(screenX - cw, S.preview[1] + io.MouseDelta.x))
        S.preview[2] = math.max(0, math.min(screenY - ch, S.preview[2] + io.MouseDelta.y))
    end
    if hovered or S.movingDrag then
        imgui.SetMouseCursor(imgui.MouseCursor.ResizeAll)
    end
    local x1, y1 = S.preview[1], S.preview[2]
    dl:AddRectFilled(vec(x1, y1), vec(x1 + cw, y1 + ch), rgba(12, 12, 12, 230), 10 * u)
    dl:AddRect(vec(x1, y1), vec(x1 + cw, y1 + ch), white(S.movingDrag and 120 or (hovered and 90 or 60)), 10 * u, nil,
        1.5 * u)
    drawText(dl, ui.fonts.body, x1 + 16 * u, y1 + 12 * u, pal('text'), 'Pr\195\169via')
    drawText(dl, ui.fonts.desc, x1 + 16 * u, y1 + 16 * u + ui.fonts.body.FontSize, pal('hint'),
        S.movingDrag and 'Solte para fixar' or 'Arraste para mover')
    imgui.End()
    imgui.PopStyleVar(2)

    -- Pilula de ajuda no rodape da tela (a mesma janela de ajuda do Kill List).
    local help = S.movingDrag and 'Solte para fixar' or 'Arraste a pr\195\169via  |  Esc ou bot\195\163o direito cancela'
    local fg = imgui.GetForegroundDrawList()
    local hs = textSize(ui.fonts.body, help)
    local px, py = (screenX - hs.x) * 0.5 - 18 * u, screenY - 28 * u - hs.y - 24 * u
    fg:AddRectFilled(vec(px, py), vec(px + hs.x + 36 * u, py + hs.y + 24 * u), rgba(12, 12, 12, 252), 10 * u)
    fg:AddRect(vec(px, py), vec(px + hs.x + 36 * u, py + hs.y + 24 * u), white(14), 10 * u, nil, 1)
    drawText(fg, ui.fonts.body, px + 18 * u, py + 12 * u, pal('text'), help)

    if S.movingDrag and not imgui.IsMouseDown(0) then
        S.moving, S.movingDrag = false, false
        S.menuAge = 0
        ui.toast('Posi\195\167\195\163o salva', 'A pr\195\169via ficou onde voc\195\170 soltou.')
    elseif keys.escape or imgui.IsMouseClicked(1) then
        S.preview = { S.previewStart[1], S.previewStart[2] }
        S.moving, S.movingDrag = false, false
        S.menuAge = 0
    end
end

-- ---------------------------------------------------------------- quadros do mimgui

local rendererReady = false

imgui.OnInitialize(function()
    local io = imgui.GetIO()
    io.IniFilename = nil
    io.ConfigFlags = bit.bor(io.ConfigFlags, imgui.ConfigFlags.NoMouseCursorChange)
    io.MouseDrawCursor = false
    ui.buildFonts()
    rendererReady = true
end)

-- Notificacoes ficam na tela mesmo com o menu fechado, sem cursor. Este quadro tambem liga o mimgui:
-- o OnInitialize so roda quando algum quadro pede para desenhar, entao ele pede enquanto o renderer
-- nao esta pronto (o mesmo truque do Trok Kill List).
local toastFrame = imgui.OnFrame(
    function()
        return not rendererReady or (#ui.toasts > 0 and not isPauseMenuActive())
    end,
    function(frame)
        if not rendererReady then
            return
        end
        frame.HideCursor = not uiActive()
        ui.drawToasts()
    end
)
toastFrame.HideCursor = true

imgui.OnFrame(
    function()
        return rendererReady and uiActive() and not isPauseMenuActive()
    end,
    function()
        -- O Tab e do SA-MP (placar): o ImGui da vitrine nunca o ve apertado (senao pularia para um campo de texto).
        imgui.GetIO().KeysDown[0x09] = false -- VK_TAB
    end,
    function(frame)
        frame.HideCursor = false
        frame.LockPlayer = true
        ui.beginFrame()
        if S.moving then
            drawMove()
        elseif S.dialog then
            drawDialog()
        elseif S.open then
            drawMenu()
            if S.confirm then
                drawConfirm()
            end
        end
        ui.endFrame()
        publishCursor(uiActive() and imgui.GetMouseCursor() or imgui.MouseCursor.Arrow)
    end
)

-- ---------------------------------------------------------------- teclado e comando

local VK = {
    TAB = 0x09, ENTER = 0x0D, ESC = 0x1B, SPACE = 0x20,
    LEFT = 0x25, UP = 0x26, RIGHT = 0x27, DOWN = 0x28, BACK = 0x08,
}

local function toggleMenu()
    if S.moving then
        S.preview = S.previewStart and { S.previewStart[1], S.previewStart[2] } or S.preview
        S.moving = false
        return
    end
    if S.dialog then
        S.dialog = nil
        return
    end
    S.open = not S.open
    S.confirm = false
    S.menuAge = 0
    pending = {}
    print('[Trok UI Showcase] vitrine ' .. (S.open and 'aberta' or 'fechada') ..
        (rendererReady and '' or ' (mimgui ainda nao iniciou)'))
end

-- Teclado lido direto das mensagens da janela (como o WndProc do .asi): com a vitrine aberta o jogo
-- e o SA-MP nao recebem as teclas, mas o mimgui continua recebendo. Teclas soltas (WM_KEYUP) sempre
-- passam: sem o "soltar" a tecla ficaria presa no GTA. Setas repetem enquanto seguradas.
-- Excecoes: T e F6 abrem o chat do SA-MP (menos digitando num campo da vitrine), o Tab abre o placar do
-- SA-MP (sempre: a vitrine nao usa o Tab), e com o chat ou um dialogo do servidor aberto a vitrine larga o
-- teclado para nao reagir ao que se digita la.
local held = {}
local CHAT_KEYS = { [0x54] = true, [0x75] = true } -- T, F6
local sampKeyboardAt = -1

-- O teclado e do SA-MP? Vale tambem por 150 ms depois de o chat fechar: o Enter que manda a mensagem
-- nao pode cair na vitrine.
local function sampOwnsKeyboard()
    if sampIsChatInputActive() or sampIsDialogActive() then
        sampKeyboardAt = os.clock()
        return true
    end
    return os.clock() - sampKeyboardAt < 0.15
end

local function onKey(vk, repeated)
    if ui.capturing then
        -- Linha de tecla esperando: a proxima tecla vira o atalho. Esc cancela, Backspace limpa.
        if not repeated then
            ui.captured = (vk == VK.ESC and 'cancelar') or (vk == VK.BACK and 'limpar') or vk
        end
        return
    end
    if imgui.GetIO().WantTextInput then
        -- Digitando num campo: Enter envia o dialogo e Esc fecha, como no SA-MP.
        if not repeated and vk == VK.ENTER then pending.enterInField = true end
        if not repeated and vk == VK.ESC then pending.escapeInField = true end
        return
    end
    if vk == VK.UP then pending.up = true
    elseif vk == VK.DOWN then pending.down = true
    elseif vk == VK.LEFT then pending.left = true
    elseif vk == VK.RIGHT then pending.right = true
    elseif repeated then return
    elseif vk == VK.ENTER then pending.enter = true
    elseif vk == VK.SPACE then pending.space = true
    elseif vk == VK.ESC then
        if ui.popupOpen then
            ui.closePopup = true
        else
            pending.escape = true
        end
    elseif vk >= 0x31 and vk <= 0x39 then pending.digit = vk - 0x30
    elseif vk >= 0x61 and vk <= 0x69 then pending.digit = vk - 0x60
    end
end

function onWindowMessage(msg, wparam, lparam)
    if msg == 0x101 or msg == 0x105 then -- WM_KEYUP, WM_SYSKEYUP
        held[wparam] = nil
        return
    end
    local keyDown = msg == 0x100 or msg == 0x104 -- WM_KEYDOWN, WM_SYSKEYDOWN
    if keyDown then
        held[wparam] = true
    end
    -- So as mensagens que a vitrine usa (tecla apertada, caractere, roda): o resto nem consulta o SA-MP.
    local used = keyDown or msg == 0x102 or msg == 0x20A
    if not used or not uiActive() or isPauseMenuActive() or sampOwnsKeyboard() then
        return
    end
    if wparam == VK.TAB and (keyDown or msg == 0x102) then
        return -- Tab (e o "\t" que vem junto) segue para o SA-MP abrir o placar
    end
    local typing = imgui.GetIO().WantTextInput or ui.capturing ~= nil
    if not typing and ((keyDown and CHAT_KEYS[wparam]) or (msg == 0x102 and (wparam == 0x74 or wparam == 0x54))) then
        return -- T/F6 (e o "t" que vem junto) seguem para o SA-MP abrir o chat
    end
    if keyDown then
        onKey(wparam, bit.band(lparam, 0x40000000) ~= 0)
        pending.holdEnter = held[VK.ENTER] == true
    end
    if msg == 0x100 or msg == 0x102 or msg == 0x20A then -- WM_KEYDOWN, WM_CHAR, WM_MOUSEWHEEL
        consumeWindowMessage(true, false)
    end
end

function main()
    if not isSampfuncsLoaded() or not isSampLoaded() then
        return
    end
    while not isSampAvailable() do
        wait(100)
    end

    sampRegisterChatCommand('trokuilua', toggleMenu)
    print('[Trok UI Showcase] v' .. thisScript().version .. ' iniciada. /trokuilua abre a vitrine.')

    -- Se a vitrine foi aberta e o mimgui nao iniciou em 2 s, avisa no chat em vez de ficar mudo.
    local waitingSince, warned = nil, false
    while true do
        wait(0)
        -- Todo quadro, para a folga de 150 ms valer mesmo se o Enter que fecha o chat vier depois de uma pausa.
        sampOwnsKeyboard()
        if not uiActive() or isPauseMenuActive() then
            pending = {}
            releaseCursor()
        end
        if uiActive() and not rendererReady and not warned then
            waitingSince = waitingSince or os.clock()
            if os.clock() - waitingSince > 2 then
                warned = true
                print('[Trok UI Showcase] o mimgui nao iniciou em 2 s')
                sampAddChatMessage('[Trok UI] o mimgui nao iniciou. Me envie o arquivo moonloader\\moonloader.log', 0xFF8A8A)
            end
        end
    end
end

function onScriptTerminate(script)
    if script == thisScript() then
        releaseCursor()
    end
end
