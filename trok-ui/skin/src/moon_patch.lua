-- Trok Skin: visual da casa nos menus do imgui antigo do moonloader (moon_imgui: lib\imgui.lua + lib\MoonImGui.dll,
-- com o Dear ImGui 1.52). O .asi roda este arquivo uma vez em cada script, logo que o MoonImGui.dll monta a tabela
-- do modulo e antes do resto do imgui.lua: recebe a tabela do modulo e a tabela "skin" com as funcoes do .asi.
--
-- Troca tres coisas na tabela do modulo deste script:
--  - imgui.ImGuiRenderer: o renderizador que o imgui.lua cria passa a avisar o comeco de cada quadro (tema da
--    casa como base do estilo, como o igNewFrame do mimgui);
--  - imgui.Begin: antes de cada janela, padroniza (ou devolve ao visual do script, se ela fica como esta) e liga a
--    borda da janela e dos campos (ShowBorders, o FrameBorderSize do 1.52);
--  - as fontes no atlas do script: fonte da casa no lugar das fontes de sistema (e, com layout=1, o tamanho da casa).
-- Nunca mexe na pilha do ImGui (no 1.52, push sem pop no lugar certo dispara assert e trava o jogo): so le e
-- escreve o estilo. Qualquer erro aqui desliga a skin so neste script e devolve o visual dele.
local imgui, skin = ...

-- O script: o nome do arquivo (log, manter_scripts) e o caminho, para o .asi ver se e um mod da casa pelo que ele
-- tem dentro (pasta resource\trok ou janela ##trok), com qualquer nome.
local path, file
pcall(function() path = thisScript().path end)
pcall(function() file = thisScript().filename end)
if type(path) ~= 'string' then path = '' end
if type(file) ~= 'string' or file == '' then file = path:match('[^\\/]+$') or '?' end
if not skin.script(file, path) then
    return -- mod da casa ou em manter_scripts: fica como esta
end

local version = type(imgui.GetVersion) == 'function' and imgui.GetVersion() or '?'
if version ~= '1.52' then
    skin.log('imgui antigo com Dear ImGui ' .. tostring(version) .. ' em ' .. file ..
             '; a skin foi feita para o 1.52 -- fica como esta')
    return
end
for _, name in ipairs({'Begin', 'GetStyle', 'GetIO', 'ImGuiRenderer', 'ImFontConfig'}) do
    if imgui[name] == nil then
        skin.log('imgui antigo sem ' .. name .. ' em ' .. file .. ' -- fica como esta')
        return
    end
end

local SHOW_BORDERS = 128 -- ImGuiWindowFlags_ShowBorders: borda da janela, dos campos e o divisor do titulo
local COUNT = 43         -- cores do ImGui 1.52 (imgui.Col vai de 1 a 43)
local WINDOW_BG = 3
-- Campos do estilo que a skin troca: cantos e alinhamento do titulo sempre; daqui em diante (LAYOUT_*), os
-- espacamentos, so com layout=1. Os pares sao ImVec2 (x, y).
local FLOATS = {'WindowRounding', 'ChildWindowRounding', 'FrameRounding', 'ScrollbarRounding', 'GrabRounding',
                'IndentSpacing', 'ScrollbarSize', 'GrabMinSize'}
local PAIRS = {'WindowTitleAlign', 'WindowPadding', 'FramePadding', 'ItemSpacing', 'ItemInnerSpacing'}
local LAYOUT_FLOAT, LAYOUT_PAIR = 6, 2
local NF, NP = #FLOATS, #PAIRS

local palette = skin.palette() -- {cores = {{r, g, b, a}, ...}, fundo = {[i] = true}, significado = {[i] = true}}
local house = {}
for i = 1, COUNT do
    local c = palette.cores[i]
    house[i * 4 - 3], house[i * 4 - 2], house[i * 4 - 1], house[i * 4] = c[1], c[2], c[3], c[4]
end

-- Fundo que o script deixou transparente: a escolha e dele. Janela com fundo abaixo de 50% e HUD por cima do jogo;
-- nos outros fundos so conta o quase invisivel (o 1.52 ja vem com campos a 30%, e isso nao e escolha do script).
local function transparent(i, alpha)
    if not palette.fundo[i] then return false end
    return alpha < (i == WINDOW_BG and 0.5 or 0.1)
end

local function newLook()
    local look = {s = {}, p = {}, c = {}}
    for i = 1, NF do look.s[i] = 0 end
    for i = 1, NP * 2 do look.p[i] = 0 end
    return look
end

local function copy(from, to)
    for i = 1, NF do to.s[i] = from.s[i] end
    for i = 1, NP * 2 do to.p[i] = from.p[i] end
    for i = 1, COUNT * 4 do to.c[i] = from.c[i] end
end

-- style: o ImGuiStyle pela API do moon_imgui (cores de 1 a 43) ou pela FFI (cores de 0 a 42, base = -1).
local function read(style, base, look)
    for i = 1, NF do look.s[i] = style[FLOATS[i]] end
    for i = 1, NP do
        local v = style[PAIRS[i]]
        look.p[i * 2 - 1], look.p[i * 2] = v.x, v.y
    end
    local colors, c = style.Colors, look.c
    for i = 1, COUNT do
        local v = colors[i + base]
        c[i * 4 - 3], c[i * 4 - 2], c[i * 4 - 1], c[i * 4] = v.x, v.y, v.z, v.w
    end
end

local function near(a, b)
    return math.abs(a - b) < 1e-5
end

local function samePair(a, b, i)
    return near(a[i * 2 - 1], b[i * 2 - 1]) and near(a[i * 2], b[i * 2])
end

local function sameColor(a, b, i)
    local k = i * 4
    return near(a[k - 3], b[k - 3]) and near(a[k - 2], b[k - 2]) and near(a[k - 1], b[k - 1]) and near(a[k], b[k])
end

-- Escreve so o que mudou (pares e cores sao referencias para o estilo do contexto).
local function write(style, base, want, cur)
    for i = 1, NF do
        if not near(want.s[i], cur.s[i]) then style[FLOATS[i]] = want.s[i] end
    end
    for i = 1, NP do
        if not samePair(want.p, cur.p, i) then
            local v = style[PAIRS[i]]
            v.x, v.y = want.p[i * 2 - 1], want.p[i * 2]
        end
    end
    local colors, w = style.Colors, want.c
    for i = 1, COUNT do
        if not sameColor(w, cur.c, i) then
            local v = colors[i + base]
            v.x, v.y, v.z, v.w = w[i * 4 - 3], w[i * 4 - 2], w[i * 4 - 1], w[i * 4]
        end
    end
end

-- Acesso rapido: pela API, ler 43 cores custa ~60 us por janela (cada campo e uma chamada ao sol2); a FFI do
-- LuaJIT le o ImGuiStyle do 1.52 direto (layout do imgui.h do 1.52, no proprio imgui.lua), mas so e usada depois
-- de conferir que ve exatamente os mesmos valores que a API. Senao, fica a API.
local ffi, STYLE_PP
do
    local ok, lib = pcall(require, 'ffi')
    if ok and type(lib) == 'table' then
        local defined = pcall(lib.typeof, 'TrokSkinStyle152') or pcall(lib.cdef, [[
            typedef struct { float x, y; } TrokSkinVec2;
            typedef struct { float x, y, z, w; } TrokSkinVec4;
            typedef struct {
                float Alpha;
                TrokSkinVec2 WindowPadding, WindowMinSize;
                float WindowRounding;
                TrokSkinVec2 WindowTitleAlign;
                float ChildWindowRounding;
                TrokSkinVec2 FramePadding;
                float FrameRounding;
                TrokSkinVec2 ItemSpacing, ItemInnerSpacing, TouchExtraPadding;
                float IndentSpacing, ColumnsMinSpacing, ScrollbarSize, ScrollbarRounding, GrabMinSize, GrabRounding;
                TrokSkinVec2 ButtonTextAlign, DisplayWindowPadding, DisplaySafeAreaPadding;
                bool AntiAliasedLines, AntiAliasedShapes;
                float CurveTessellationTol;
                TrokSkinVec4 Colors[43];
            } TrokSkinStyle152;
        ]])
        if defined then
            ffi, STYLE_PP = lib, lib.typeof('TrokSkinStyle152**')
        end
    end
end
local fast = nil -- nil: ainda nao conferido; true: FFI; false: API

local function sameLook(a, b)
    for i = 1, NF do
        if a.s[i] ~= b.s[i] then return false end
    end
    for i = 1, NP * 2 do
        if a.p[i] ~= b.p[i] then return false end
    end
    for i = 1, COUNT * 4 do
        if a.c[i] ~= b.c[i] then return false end
    end
    return true
end

local function checkFast(style)
    local ok, result = pcall(function()
        local ptr = ffi.cast(STYLE_PP, style)[0]
        if ptr == nil then return false end
        local viaApi, viaFfi = newLook(), newLook()
        read(style, 0, viaApi)
        read(ptr, -1, viaFfi)
        if not sameLook(viaApi, viaFfi) then return false end
        -- Outros campos do layout, para nao aceitar um deslocamento por acaso.
        for _, field in ipairs({'Alpha', 'CurveTessellationTol'}) do
            if style[field] ~= ptr[field] then return false end
        end
        for _, field in ipairs({'TouchExtraPadding', 'ButtonTextAlign'}) do
            local a, b = style[field], ptr[field]
            if a.x ~= b.x or a.y ~= b.y then return false end
        end
        return true
    end)
    fast = ok and result == true
    if not fast then
        skin.log('imgui antigo: ' .. file .. ': leitura direta do estilo nao conferiu; usando a API (mais lenta)')
    end
end

-- O estilo do contexto ativo (o do script, dentro do quadro dele) e a base do indice das cores.
local function styleRef()
    local style = imgui.GetStyle()
    if fast == nil and STYLE_PP then checkFast(style) end
    if fast then return ffi.cast(STYLE_PP, style)[0], -1 end
    return style, 0
end

-- Estado deste script: o visual dele (orig), para o tema poder desligar na hora, e o que a skin escreveu no
-- comeco do quadro (applied), para perceber o que o script mudou depois.
local orig, applied, cur, want = nil, newLook(), newLook(), newLook()
local houseLook = newLook() -- cantos, alinhamento e espacamentos da casa para a tela atual (a cada quadro)
local layoutOn = false      -- layout=1: espacamentos da casa tambem
local theme = true          -- tema ligado neste quadro (/trokskin vale a partir do quadro seguinte)
local dead = false
local notes, noted = {}, 0
-- Kit (layout=1): cabecalho e controles desenhados como no Trok UI. kitWindows: pilha de Begin/End do quadro
-- (KIT_WINDOW: os controles da janela sao os do kit; KIT_SHELL: ela ganhou o cabecalho); titleFont e descFont: fontes
-- de titulo e pequena do kit, criadas junto com a primeira fonte trocada.
local kitOn = skin.spacing(1080) ~= nil
local kitWindows, kitDepth, kitScale = {}, 0, nil
local KIT_WINDOW, KIT_SHELL = 1, 2
local titleFont, descFont = nil, nil
-- Esc do kit: fecha o popup aberto (lista, menu de contexto) ou a janela com X que estava em foco no ultimo quadro
-- (como o X), e o jogo nao recebe a tecla. escWindow/escPopup/escSeen: a janela em foco, a janela com popup aberto e
-- quando; escClose/escPopupClose: o que o Esc mandou fechar (vale so para o quadro seguinte); escHeld: Esc engolido
-- ate soltar; escBusy: um controle ativo no fim do ultimo quadro (digitando); escLive: menu do kit na tela neste
-- quadro; lastShown: ultimo quadro de cada janela (para saber quando uma abre); kitNames: nome das janelas da pilha.
local escOn = kitOn and type(consumeWindowMessage) == 'function' and imgui.IsRootWindowOrAnyChildFocused ~= nil and
              imgui.IsAnyItemActive ~= nil and imgui.IsPopupOpen ~= nil and imgui.SetWindowFocus ~= nil and
              imgui.IsRootWindowOrAnyChildHovered ~= nil
local escWindow, escPopup, escSeen, escClose, escPopupClose, escCloseFrames = nil, nil, 0, nil, nil, 0
local escHeld, escBusy, escLive, frameNo, lastShown, kitNames = false, false, false, 0, {}, {}

local function loadHouse(height)
    local s, p = houseLook.s, houseLook.p
    s[1], s[2], s[3], s[4], s[5], p[1], p[2] = skin.scalars(height)
    local indent, scroll, grab, wpx, wpy, fpx, fpy, isx, isy, iix, iiy = skin.spacing(height)
    layoutOn = indent ~= nil
    if layoutOn then
        s[6], s[7], s[8] = indent, scroll, grab
        p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10] = wpx, wpy, fpx, fpy, isx, isy, iix, iiy
    end
end

-- Cantos e alinhamento da casa e, com layout=1, os espacamentos.
local function setHouseFields(look)
    local lastF, lastP = layoutOn and NF or LAYOUT_FLOAT - 1, layoutOn and NP or LAYOUT_PAIR - 1
    for i = 1, lastF do look.s[i] = houseLook.s[i] end
    for i = 1, lastP * 2 do look.p[i] = houseLook.p[i] end
end

local function setHouseColor(look, i)
    local k = i * 4
    look.c[k - 3], look.c[k - 2], look.c[k - 1], look.c[k] = house[k - 3], house[k - 2], house[k - 1], house[k]
end

-- Volta ao visual do script o que ele nao empurrou para esta janela (com push, fica o que ele empurrou).
-- spacingOnly: so os espacamentos (HUD com layout=1).
local function keepScript(spacingOnly)
    for i = spacingOnly and LAYOUT_FLOAT or 1, NF do
        if near(cur.s[i], applied.s[i]) then want.s[i] = orig.s[i] end
    end
    for i = spacingOnly and LAYOUT_PAIR or 1, NP do
        if samePair(cur.p, applied.p, i) then
            want.p[i * 2 - 1], want.p[i * 2] = orig.p[i * 2 - 1], orig.p[i * 2]
        end
    end
    if spacingOnly then return end
    for i = 1, COUNT do
        if sameColor(cur.c, applied.c, i) then
            local k = i * 4
            want.c[k - 3], want.c[k - 2], want.c[k - 1], want.c[k] = orig.c[k - 3], orig.c[k - 2], orig.c[k - 1], orig.c[k]
        end
    end
end

-- Anota (log e /trokskin) a decisao de cada janela quando ela aparece ou muda. Titulo que muda a cada quadro
-- (FPS no titulo...) nao enche o log: depois de 32 janelas, so as ja vistas.
local function note(title, decision)
    local seen = notes[title]
    if seen == decision or (seen == nil and noted >= 32) then return end
    if seen == nil then noted = noted + 1 end
    notes[title] = decision
    skin.note(file, title, decision)
end

local function die(err)
    if dead then return end
    dead = true
    skin.log('imgui antigo: erro na skin em ' .. file .. ' (o script volta ao visual dele): ' .. tostring(err))
    if orig then
        pcall(function()
            local style, base = styleRef()
            read(style, base, cur)
            write(style, base, orig, cur)
        end)
    end
end

-- Comeco de cada quadro: base do estilo = tema da casa (ou o visual do script, com o tema desligado).
local function applyFrame()
    local style, base = styleRef()
    read(style, base, cur)
    if not orig then
        if #imgui.GetStyle().Colors ~= COUNT then error('estilo com ' .. #imgui.GetStyle().Colors .. ' cores') end
        orig = newLook()
        copy(cur, orig)
        copy(cur, applied)
    else
        -- O que o script mudou desde a ultima escrita da skin passa a ser o visual dele.
        for i = 1, NF do
            if not near(cur.s[i], applied.s[i]) then orig.s[i] = cur.s[i] end
        end
        for i = 1, NP do
            if not samePair(cur.p, applied.p, i) then
                orig.p[i * 2 - 1], orig.p[i * 2] = cur.p[i * 2 - 1], cur.p[i * 2]
            end
        end
        for i = 1, COUNT do
            if not sameColor(cur.c, applied.c, i) then
                local k = i * 4
                orig.c[k - 3], orig.c[k - 2], orig.c[k - 1], orig.c[k] = cur.c[k - 3], cur.c[k - 2], cur.c[k - 1], cur.c[k]
            end
        end
    end
    theme = skin.theme()
    kitDepth, kitScale = 0, nil
    loadHouse(imgui.GetIO().DisplaySize.y)
    copy(orig, want)
    if theme then
        setHouseFields(want)
        for i = 1, COUNT do
            if not transparent(i, orig.c[i * 4]) then setHouseColor(want, i) end
        end
    end
    write(style, base, want, cur)
    copy(want, applied)
end

local function frameStart()
    if not dead then
        local ok, err = pcall(applyFrame)
        if not ok then die(err) end
    end
end

-- Antes de cada janela: o script pode ter mudado o estilo depois do comeco do quadro (ou empurrado algo para esta
-- janela). Janela padronizada volta ao tema; janela mantida (##trok ou manter=) volta ao visual do script; HUD
-- (fundo transparente) fica intocado. Devolve as flags novas quando a janela ganha a borda da casa.
local function applyWindow(name, flags)
    if not orig then return nil, false end -- nenhum quadro comecou ainda
    local style, base = styleRef()
    local title = tostring(name)
    if transparent(WINDOW_BG, style.Colors[WINDOW_BG + base].w) then
        note(title, 'fundo transparente (HUD): fica como esta')
        if layoutOn then
            -- O HUD fica onde o autor pos: os espacamentos voltam aos dele (a fonte vale para o script inteiro).
            read(style, base, cur)
            copy(cur, want)
            keepScript(true)
            write(style, base, want, cur)
        end
        return nil, false
    end
    read(style, base, cur)
    local kept = skin.kept(title)
    note(title, kept and 'mantida (##trok ou manter=)' or theme and 'padronizada' or 'tema desligado')
    copy(cur, want)
    if kept or not theme then
        keepScript(false)
        write(style, base, want, cur)
        return nil, false
    end
    setHouseFields(want)
    for i = 1, COUNT do
        -- Cores com significado (texto vermelho, grafico verde) ficam como o script empurrou.
        if not palette.significado[i] and not transparent(i, cur.c[i * 4]) then setHouseColor(want, i) end
    end
    write(style, base, want, cur)
    if type(flags) ~= 'number' and flags ~= nil then return nil, true end
    flags = flags or 0
    if math.floor(flags / SHOW_BORDERS) % 2 == 1 then return nil, true end
    return flags + SHOW_BORDERS, true
end

-- Fontes do atlas deste script (o metatable e o mesmo para todo ImFontAtlas do script). Liga no primeiro
-- SwitchContext/BeginFrame, com o contexto do script ativo. A regra de cada fonte vem do .asi (skin.font): normal,
-- so as fontes de sistema viram a fonte da casa, com a mesma largura de texto; com layout=1, a primeira fonte do
-- atlas vai para o tamanho de texto da casa e todas as outras acompanham.
local fontsReady = not skin.fonts()
local layoutFonts = skin.spacing(1080) ~= nil

local function patchFonts()
    fontsReady = true
    local atlas = imgui.GetIO().Fonts
    local mt = getmetatable(atlas)
    local index = type(mt) == 'table' and mt.__index
    local function method(name)
        if type(index) ~= 'function' then return nil end
        local ok, fn = pcall(index, atlas, name)
        return ok and type(fn) == 'function' and fn or nil
    end
    local add = method('AddFontFromFileTTF')
    if not add then
        skin.log('imgui antigo: ' .. file .. ': atlas de fontes diferente do esperado -- fonte fica como esta')
        return
    end

    local function replace(self, fontPath, size, ...)
        local cfg, ranges = ...
        if type(fontPath) ~= 'string' or type(size) ~= 'number' then return nil end
        local merge = cfg ~= nil and cfg.MergeMode == true
        local houseSize, face = skin.font(file, fontPath, size, ranges ~= nil, merge)
        if not houseSize then return nil end
        if not face then
            return add(self, fontPath, houseSize, ...) -- layout=1: so o tamanho
        end
        local font = add(self, skin.housePath(), houseSize, ...)
        if not font then
            skin.log('fonte da casa nao carregou; ' .. fontPath .. ' ficou')
            return nil
        end
        -- Glifos que a fonte da casa nao tem vem da fonte original (o ImGui nao sobrescreve os que ja existem).
        local fallback = cfg
        if fallback == nil then fallback = imgui.ImFontConfig() end
        fallback.MergeMode = true
        local ok, err
        if ranges ~= nil then
            ok, err = pcall(add, self, fontPath, houseSize, fallback, ranges)
        else
            ok, err = pcall(add, self, fontPath, houseSize, fallback)
        end
        if not ok then skin.log('fonte de reserva nao entrou: ' .. tostring(err)) end
        if kitOn and titleFont == nil then
            -- Fontes do kit, com a mesma reserva de glifos: a de titulo (18 x escala) para o cabecalho das janelas e
            -- a pequena (14,5 x escala) para o texto dos botoes.
            local function kitFont(kitSize)
                fallback.MergeMode = false
                local kf
                if ranges ~= nil then kf = add(self, skin.housePath(), kitSize, cfg, ranges)
                elseif cfg ~= nil then kf = add(self, skin.housePath(), kitSize, cfg)
                else kf = add(self, skin.housePath(), kitSize) end
                fallback.MergeMode = true
                if kf then
                    if ranges ~= nil then pcall(add, self, fontPath, kitSize, fallback, ranges)
                    else pcall(add, self, fontPath, kitSize, fallback) end
                end
                return kf
            end
            local titleSize = skin.titleSize()
            titleFont = kitFont(titleSize)
            descFont = kitFont(titleSize * 14.5 / 18)
        end
        if cfg ~= nil then cfg.MergeMode = false end
        skin.log(string.format('fonte %s %.1f px de %s -> fonte da casa %.1f px (%s)', fontPath, size, file, houseSize,
                               layoutFonts and 'tamanho da casa' or 'mesma largura de texto'))
        return font
    end

    local function addFont(self, ...)
        if not dead then
            local ok, font = pcall(replace, self, ...)
            if ok and font then return font end
            if not ok then skin.log('imgui antigo: erro na troca de fonte em ' .. file .. ': ' .. tostring(font)) end
        end
        return add(self, ...)
    end

    -- Fontes da memoria (icones embutidos etc.): so o tamanho, e so com layout=1. sizeArg: posicao do tamanho.
    local function memoryFont(original, sizeArg)
        return function(self, ...)
            local n, args = select('#', ...), {...}
            if not dead and type(args[sizeArg]) == 'number' then
                local ok, newSize = pcall(function()
                    local cfg = args[sizeArg + 1]
                    return skin.font(file, nil, args[sizeArg], false, type(cfg) == 'userdata' and cfg.MergeMode == true)
                end)
                if ok and newSize then
                    args[sizeArg] = newSize
                    return original(self, unpack(args, 1, n))
                end
            end
            return original(self, ...)
        end
    end

    local wrapped = {AddFontFromFileTTF = addFont}
    for name, sizeArg in pairs({AddFontFromMemoryTTF = 3, AddFontFromMemoryCompressedTTF = 3,
                                AddFontFromMemoryCompressedBase85TTF = 2}) do
        local original = method(name)
        if original then wrapped[name] = memoryFont(original, sizeArg) end
    end
    -- Atlas limpo: a proxima fonte e a nova referencia de tamanho (layout=1).
    for _, name in ipairs({'Clear', 'ClearFonts'}) do
        local original = method(name)
        if original then
            wrapped[name] = function(self, ...)
                pcall(skin.resetFonts, file)
                titleFont, descFont = nil, nil
                return original(self, ...)
            end
        end
    end

    mt.__index = function(self, key)
        local fn = wrapped[key]
        if fn then return fn end
        return index(self, key)
    end
end

local function fontsStart()
    if not fontsReady and not dead then
        local ok, err = pcall(patchFonts)
        if not ok then die(err) end
    end
end

-- ---------------------------------------------------------------- kit (layout=1)

local V = imgui.ImVec2
local colorCache = {}

local function rgba(r, g, b, a)
    local key = ((r * 256 + g) * 256 + b) * 256 + (a or 255)
    local c = colorCache[key]
    if not c then
        c = imgui.ColorConvertFloat4ToU32(imgui.ImVec4(r / 255, g / 255, b / 255, (a or 255) / 255))
        colorCache[key] = c
    end
    return c
end

local function gray(v, a)
    return rgba(v, v, v, a)
end

-- Cor atual do estilo (respeita o que o script empurrou, como texto vermelho), ja com o Alpha do estilo.
local function styleColor(i)
    local style, base = styleRef()
    local c = style.Colors[i + base]
    return imgui.ColorConvertFloat4ToU32(imgui.ImVec4(c.x, c.y, c.z, c.w * style.Alpha))
end

local function kitActive()
    return kitOn and kitDepth > 0 and (kitWindows[kitDepth] or 0) % 2 == KIT_WINDOW
end

local function kitU()
    if not kitScale then
        kitScale = (houseLook.s[1] or 10) / 10 -- WindowRounding da casa = 10 x escala
    end
    return kitScale
end

local function kitFail(err)
    if not kitOn then return end
    kitOn = false
    skin.log('imgui antigo: kit desligado em ' .. file .. ' (o tema segue): ' .. tostring(err))
end

local function visibleLabel(label)
    return label:match('^(.-)##') or label
end

-- Cabecalho do kit (44 x escala): titulo centralizado na fonte de titulo, X (se a janela tem botao de fechar) e a
-- linha embaixo, a unica linha da janela. O X nao e um item do ImGui (so desenho e clique): nao mexe no tamanho das
-- janelas que se ajustam ao conteudo. O conteudo que rola fica recortado embaixo do cabecalho (o End tira o
-- recorte). Sem rodape: nos menus da casa ele e a faixa das dicas de tecla, e menu de outro mod nao tem dicas.
local function kitShell(name, open)
    local u = kitU()
    local pos, size = imgui.GetWindowPos(), imgui.GetWindowSize()
    local dl = imgui.GetWindowDrawList()
    local headerH, padX = 44 * u, 18 * u
    local text = styleColor(1)
    local title = visibleLabel(name)
    if title ~= '' then
        if titleFont then imgui.PushFont(titleFont) end
        local ts = imgui.CalcTextSize(title)
        dl:AddText(V(math.floor(pos.x + (size.x - ts.x) * 0.5), math.floor(pos.y + (headerH - ts.y) * 0.5)), text, title)
        if titleFont then imgui.PopFont() end
    end
    dl:AddLine(V(pos.x + padX, pos.y + headerH - 0.5), V(pos.x + size.x - padX, pos.y + headerH - 0.5),
               rgba(255, 255, 255, 10), 1)
    if escOn then
        -- Menu do kit na tela; o jogador mexeu nele agora se ele acabou de abrir ou levou um clique (popup e item
        -- ativo nao contam como bloqueio: 1 + 4). Entre scripts, o Esc vai para o mexido por ultimo.
        local appearing = lastShown[name] ~= frameNo - 1
        lastShown[name] = frameNo
        escLive = true
        if appearing or (imgui.IsMouseClicked(0) and imgui.IsRootWindowOrAnyChildHovered(5)) then skin.escTouch(file) end
    end
    if type(open) == 'userdata' then
        local side = 28 * u
        local ax, ay = pos.x + size.x - padX - side + 6 * u, pos.y + (headerH - side) * 0.5
        local hovered = imgui.IsWindowHovered() and imgui.IsMouseHoveringRect(V(ax, ay), V(ax + side, ay + side))
        if hovered and imgui.IsMouseClicked(0) then open.v = false end
        local col = hovered and text or styleColor(2)
        local c, mx, my = 5 * u, ax + side * 0.5, ay + side * 0.5
        dl:AddLine(V(mx - c, my - c), V(mx + c, my + c), col, 1.5 * u)
        dl:AddLine(V(mx - c, my + c), V(mx + c, my - c), col, 1.5 * u)
        -- Esc: faz o mesmo que o X na janela que estava em foco quando a tecla desceu (escapeKey).
        if escClose ~= nil and escClose == name then
            escClose = nil
            open.v = false
            skin.log('Esc fechou a janela "' .. name .. '" de ' .. file)
        end
        if escOn and open.v and imgui.IsRootWindowOrAnyChildFocused() then
            escWindow, escSeen = name, os.clock()
        end
    end
    local style = styleRef()
    -- O conteudo comeca embaixo do cabecalho (posicao local: acompanha a rolagem da janela).
    imgui.SetCursorPos(V(style.WindowPadding.x, headerH + style.WindowPadding.y * 0.75))
    imgui.PushClipRect(V(pos.x, pos.y + headerH), V(pos.x + size.x, pos.y + size.y), true)
end

local function frameHeight(style)
    return imgui.GetFontSize() + style.FramePadding.y * 2
end

-- Interruptor do kit no lugar da caixa de marcar: mesmo id e mesmo retorno (true quando o valor muda).
local function kitToggle(label, value)
    local u = kitU()
    local style = styleRef()
    local pos = imgui.GetCursorScreenPos()
    local fh = frameHeight(style)
    local shown = visibleLabel(label)
    local ls = shown ~= '' and imgui.CalcTextSize(shown) or nil
    local th = math.min(24 * u, fh)
    local tw = th * 44 / 24
    local inner = style.ItemInnerSpacing.x
    local pressed = imgui.InvisibleButton(label, V(tw + (ls and inner + ls.x or 0), fh))
    local hovered = imgui.IsItemHovered()
    if pressed then value.v = not value.v end
    local on = value.v
    local dl = imgui.GetWindowDrawList()
    local r = th * 0.5
    local ax, ay = pos.x, math.floor(pos.y + (fh - th) * 0.5)
    dl:AddRectFilled(V(ax, ay), V(ax + tw, ay + th), gray(on and (hovered and 240 or 226) or (hovered and 40 or 32)), r, 15)
    if not on then
        dl:AddRect(V(ax - 0.5, ay - 0.5), V(ax + tw + 0.5, ay + th + 0.5), rgba(255, 255, 255, hovered and 34 or 22), r + 0.5,
                   15, 1)
    end
    dl:AddCircleFilled(V(ax + r + (on and tw - th or 0), ay + r), r - 3 * u, gray(on and 18 or (hovered and 226 or 196)), 24)
    if ls then dl:AddText(V(pos.x + tw + inner, math.floor(pos.y + (fh - ls.y) * 0.5)), styleColor(1), shown) end
    return pressed
end

-- Valor formatado como o script pediu ("%.0f", "%d%%"...) e as casas decimais que o formato mostra.
local function formatValue(fmt, v, integer)
    local precision, spec = (type(fmt) == 'string' and fmt or ''):match('%%[-+ #0]*%d*%.?(%d*)([diufFeEgG])')
    if not spec then
        fmt, spec, precision = integer and '%d' or '%.3f', integer and 'd' or 'f', integer and '' or '3'
    end
    local decimals = spec:find('[diu]') and 0 or tonumber(precision) or 6
    local ok, text = pcall(string.format, fmt, decimals == 0 and math.floor(v + 0.5) or v)
    return ok and text or tostring(v), decimals
end

-- Slider do kit: trilha fina com a bolinha e o valor ao lado, dentro da largura do slider do script; o rotulo fica
-- depois, como no ImGui. Mesmo id; devolve true quando o valor muda (arrastar ou clicar na trilha).
local function kitSlider(label, value, vmin, vmax, fmt, integer)
    local u = kitU()
    local style = styleRef()
    local pos = imgui.GetCursorScreenPos()
    local fh, w = frameHeight(style), imgui.CalcItemWidth()
    local shown = visibleLabel(label)
    local ls = shown ~= '' and imgui.CalcTextSize(shown) or nil
    local text, decimals = formatValue(fmt, value.v, integer)
    local valueW = 0
    for _, t in ipairs({text, (formatValue(fmt, vmin, integer)), (formatValue(fmt, vmax, integer))}) do
        valueW = math.max(valueW, imgui.CalcTextSize(t).x)
    end
    local knobR = 8 * u
    local x0, x1 = pos.x + knobR, pos.x + w - valueW - 10 * u - knobR
    if x1 - x0 < 24 * u then -- estreito demais para o valor ao lado: a trilha usa a largura toda
        valueW, x1 = 0, pos.x + w - knobR
    end
    local inner = style.ItemInnerSpacing.x
    imgui.InvisibleButton(label, V(math.max(1, w + (ls and inner + ls.x or 0)), fh))
    local hovered, active = imgui.IsItemHovered(), imgui.IsItemActive()
    local changed = false
    if active and x1 > x0 then
        local f = math.max(0, math.min(1, (imgui.GetIO().MousePos.x - x0) / (x1 - x0)))
        local v = vmin + (vmax - vmin) * f
        local step = 10 ^ -decimals
        v = integer and math.floor(v + 0.5) or math.floor(v / step + 0.5) * step
        v = math.max(vmin, math.min(vmax, v))
        if v ~= value.v then
            value.v = v
            changed = true
            text = formatValue(fmt, v, integer)
        end
    end
    local dl = imgui.GetWindowDrawList()
    local cy, th = math.floor(pos.y + fh * 0.5) + 0.5, 4 * u
    local t = (value.v - vmin) / (vmax - vmin)
    local fx = x0 + (x1 - x0) * math.max(0, math.min(1, t))
    dl:AddRectFilled(V(x0, cy - th * 0.5), V(x1, cy + th * 0.5), rgba(255, 255, 255, 26), th * 0.5, 15)
    dl:AddRectFilled(V(x0, cy - th * 0.5), V(fx, cy + th * 0.5), gray(226), th * 0.5, 15)
    local kr = (hovered or active) and knobR + 1 * u or knobR
    dl:AddCircleFilled(V(fx, cy), kr, gray(240), 24)
    dl:AddCircle(V(fx, cy), kr, rgba(0, 0, 0, 90), 24, 1)
    if valueW > 0 then
        local s = imgui.CalcTextSize(text)
        dl:AddText(V(pos.x + w - s.x, math.floor(pos.y + (fh - s.y) * 0.5)), active and styleColor(1) or gray(200), text)
    end
    if ls then dl:AddText(V(pos.x + w + inner, math.floor(pos.y + (fh - ls.y) * 0.5)), styleColor(1), shown) end
    return changed
end

-- Cor opaca de uma cor do estilo por cima do fundo da janela (para cobrir o que o ImGui desenhou sem deixar marca).
local function overWindow(i)
    local style, base = styleRef()
    local bg, fg = style.Colors[WINDOW_BG + base], style.Colors[i + base]
    local function mix(b, f) return math.floor((b * (1 - fg.w) + f * fg.w) * 255 + 0.5) end
    return rgba(mix(bg.x, fg.x), mix(bg.y, fg.y), mix(bg.z, fg.z), 255)
end

-- Chevron do kit (o "v" do lucide): 'd' para baixo, 'r' para a direita, centrado em (cx, cy).
local function chevron(dl, cx, cy, size, dir, col, thickness)
    local w, h = size * 0.5, size * 0.25
    if dir == 'd' then
        dl:AddLine(V(cx - w, cy - h), V(cx, cy + h), col, thickness)
        dl:AddLine(V(cx, cy + h), V(cx + w, cy - h), col, thickness)
    else
        dl:AddLine(V(cx - h, cy - w), V(cx + h, cy), col, thickness)
        dl:AddLine(V(cx + h, cy), V(cx - h, cy + w), col, thickness)
    end
end

-- Lista suspensa do kit: a caixa (fundo, contorno, valor e chevron) e redesenhada por cima da do ImGui depois que
-- ele trata o clique; a lista que abre continua a do ImGui, no tema.
local function comboPreview(items, index)
    if type(items) == 'table' then return items[index + 1] end
    if type(items) == 'string' then
        local i = 0
        for item in (items .. '\0'):gmatch('([^%z]*)%z') do
            if i == index then return item end
            i = i + 1
        end
    end
    return nil
end

local function kitComboFrame(pos, w, fh, preview)
    local u = kitU()
    local style = styleRef()
    local rounding = style.FrameRounding
    local dl = imgui.GetWindowDrawList()
    local hovered = imgui.IsItemHovered()
    dl:AddRectFilled(V(pos.x - 1, pos.y - 1), V(pos.x + w + 1, pos.y + fh + 1), overWindow(WINDOW_BG), rounding + 1, 15)
    dl:AddRectFilled(V(pos.x, pos.y), V(pos.x + w, pos.y + fh), rgba(255, 255, 255, hovered and 14 or 8), rounding, 15)
    dl:AddRect(V(pos.x, pos.y), V(pos.x + w, pos.y + fh), rgba(255, 255, 255, hovered and 40 or 22), rounding, 15, 1)
    local col = hovered and styleColor(1) or gray(200)
    local chevronX = pos.x + w - 14 * u
    if type(preview) == 'string' and preview ~= '' then
        local text = preview
        while #text > 0 and pos.x + 10 * u + imgui.CalcTextSize(text).x > chevronX - 10 * u do
            text = text:sub(1, -2) -- o valor nao passa por cima do chevron
        end
        local ts = imgui.CalcTextSize(text)
        dl:AddText(V(pos.x + 10 * u, math.floor(pos.y + (fh - ts.y) * 0.5)), col, text)
    end
    chevron(dl, chevronX, pos.y + fh * 0.5, 8 * u, 'd', col, 1.5 * u)
end

-- Cabecalho recolhivel: o triangulo do ImGui vira o chevron do kit (direita fechado, baixo aberto).
local function kitHeaderArrow(open)
    local style = styleRef()
    local a = imgui.GetItemRectMin()
    local fs = imgui.GetFontSize()
    local px, py = a.x + style.FramePadding.x, a.y + style.FramePadding.y
    local hovered, held = imgui.IsItemHovered(), imgui.IsItemActive()
    local under = (held and hovered) and 28 or hovered and 27 or 26 -- HeaderActive, HeaderHovered, Header
    local dl = imgui.GetWindowDrawList()
    dl:AddRectFilled(V(px - 1, py - 1), V(px + fs + 1, py + fs + 1), overWindow(under), 0, 15)
    chevron(dl, px + fs * 0.5, py + fs * 0.5, fs * 0.55, open and 'd' or 'r', styleColor(1), math.max(1, fs * 0.1))
end

local combo, collapsingHeader = imgui.Combo, imgui.CollapsingHeader

if kitOn and combo then
    imgui.Combo = function(label, value, ...)
        local frame
        if kitActive() and type(value) == 'userdata' then
            local ok, pos = pcall(imgui.GetCursorScreenPos)
            if ok then
                local style = styleRef()
                frame = {pos = pos, w = imgui.CalcItemWidth(), fh = frameHeight(style)}
            end
        end
        local a, b = combo(label, value, ...)
        if frame then
            local items = ...
            local ok, err = pcall(function()
                kitComboFrame(frame.pos, frame.w, frame.fh, comboPreview(items, value.v))
                if escOn and imgui.IsPopupOpen(label) then
                    escPopup, escSeen = kitNames[kitDepth], os.clock() -- a lista esta aberta: o Esc fecha ela primeiro
                end
            end)
            if not ok then kitFail(err) end
        end
        return a, b
    end
end

-- Popups abertos pelo proprio mod numa janela do kit (menus de contexto): o Esc fecha eles primeiro.
for _, key in ipairs({'BeginPopup', 'BeginPopupContextItem', 'BeginPopupContextWindow', 'BeginPopupContextVoid'}) do
    local original = imgui[key]
    if escOn and original then
        imgui[key] = function(...)
            local open, b = original(...)
            if open == true and kitActive() then escPopup, escSeen = kitNames[kitDepth], os.clock() end
            return open, b
        end
    end
end

if kitOn and collapsingHeader then
    imgui.CollapsingHeader = function(...)
        local kit = kitActive()
        local open, b = collapsingHeader(...)
        if kit and type(open) == 'boolean' then
            local ok, err = pcall(kitHeaderArrow, open)
            if not ok then kitFail(err) end
        end
        return open, b
    end
end

local checkbox, sliderFloat, sliderInt = imgui.Checkbox, imgui.SliderFloat, imgui.SliderInt

-- Botao do kit: texto na fonte pequena da casa (14,5 x escala), 26 de altura e 15 de folga de cada lado; fundo branco
-- a 8% com contorno a 22% (16% e 40% com o mouse em cima) e texto 200 (240 com o mouse em cima). E o botao do proprio
-- ImGui, com essas medidas e cores empurradas so durante a chamada: mesmo id, mesmo retorno e alinhamento da linha.
-- Tamanho que o mod deu fica (a folga encolhe para o texto caber); cor que o mod pos no botao ou no texto fica, e no
-- botao colorido pelo mod o texto fica no branco da casa. state guarda o que foi empurrado (um erro no meio
-- desempilha so isso: push sem pop trava o 1.52).
local BUTTON, BUTTON_HOVERED, BUTTON_ACTIVE, TEXT, BORDER, FRAME_PADDING = 23, 24, 25, 1, 6, 5
local litButtons = {}

local function isHouseColor(i)
    local style, base = styleRef()
    local c, k = style.Colors[i + base], i * 4
    return math.abs(c.x - house[k - 3]) < 0.002 and math.abs(c.y - house[k - 2]) < 0.002 and
           math.abs(c.z - house[k - 1]) < 0.002 and math.abs(c.w - house[k]) < 0.002
end

local function kitButtonPush(state, label, size)
    local u = kitU()
    state.key = (kitNames[kitDepth] or '') .. '\0' .. label
    local lit = litButtons[state.key] == true
    local kitColors = isHouseColor(BUTTON)
    local kitText = kitColors and isHouseColor(TEXT)
    if descFont then
        imgui.PushFont(descFont)
        state.font = true
    end
    local fs = imgui.GetFontSize()
    local shown = visibleLabel(label)
    local tw = shown ~= '' and imgui.CalcTextSize(shown).x or 0
    local padX, padY = 15 * u, math.max(0, (26 * u - fs) * 0.5)
    if type(size) == 'userdata' then
        if size.x > 0 then padX = math.min(padX, math.max(0, (size.x - tw) * 0.5)) end
        if size.y > 0 then padY = math.min(padY, math.max(0, (size.y - fs) * 0.5)) end
    end
    if kitColors then
        for _, c in ipairs({{BUTTON, 8}, {BUTTON_HOVERED, 16}, {BUTTON_ACTIVE, 16}}) do
            imgui.PushStyleColor(c[1], imgui.ImVec4(1, 1, 1, c[2] / 255))
            state.colors = state.colors + 1
        end
    end
    if kitText then
        local g = (lit and 240 or 200) / 255
        imgui.PushStyleColor(TEXT, imgui.ImVec4(g, g, g, 1))
        state.colors = state.colors + 1
    end
    imgui.PushStyleColor(BORDER, imgui.ImVec4(1, 1, 1, (lit and 40 or 22) / 255))
    state.colors = state.colors + 1
    imgui.PushStyleVar(FRAME_PADDING, V(padX, padY))
    state.vars = 1
end

local function kitButtonPop(state)
    if state.vars > 0 then imgui.PopStyleVar(state.vars) end
    if state.colors > 0 then imgui.PopStyleColor(state.colors) end
    if state.font then imgui.PopFont() end
end

local button = imgui.Button
if kitOn and button then
    imgui.Button = function(...)
        local label, size = ...
        if not kitActive() or type(label) ~= 'string' then return button(...) end
        local state = {font = false, colors = 0, vars = 0}
        local okPush, err = pcall(kitButtonPush, state, label, size)
        if not okPush then
            kitButtonPop(state)
            kitFail(err)
            return button(...)
        end
        local ok, pressed, b = pcall(button, ...) -- os mesmos argumentos que o script passou
        local hovered = ok and (imgui.IsItemHovered() or imgui.IsItemActive())
        kitButtonPop(state)
        if not ok then error(pressed, 2) end
        litButtons[state.key] = hovered or nil
        return pressed, b
    end
end

if kitOn and checkbox then
    imgui.Checkbox = function(label, value, ...)
        if kitActive() and type(label) == 'string' and type(value) == 'userdata' then
            local ok, pressed = pcall(kitToggle, label, value)
            if ok then return pressed end
            kitFail(pressed)
        end
        return checkbox(label, value, ...)
    end
end

local function wrapSlider(original, integer)
    return function(label, value, ...)
        local vmin, vmax, fmt = ...
        if kitActive() and type(label) == 'string' and type(value) == 'userdata' and type(vmin) == 'number' and
           type(vmax) == 'number' and vmax > vmin then
            local ok, changed = pcall(kitSlider, label, value, vmin, vmax, fmt, integer)
            if ok then return changed end
            kitFail(changed)
        end
        return original(label, value, ...) -- os mesmos argumentos que o script passou
    end
end
if kitOn and sliderFloat then imgui.SliderFloat = wrapSlider(sliderFloat, false) end
if kitOn and sliderInt then imgui.SliderInt = wrapSlider(sliderInt, true) end

-- Comeco do quadro: a janela em foco e recalculada; o fechamento pedido pelo Esc vale so para este quadro.
local function escFrame()
    frameNo = frameNo + 1
    escWindow, escPopup, escLive = nil, nil, false
    if escCloseFrames > 0 then
        escCloseFrames = escCloseFrames - 1
        if escCloseFrames == 0 then escClose, escPopupClose = nil, nil end
    end
end

-- O teclado e do jogo ou do SA-MP agora? Menu de pausa, chat ou dialogo (pelo SAMPFUNCS, se houver, e pela skin).
local function keyboardBusy()
    if type(isPauseMenuActive) == 'function' and isPauseMenuActive() then return true end
    if type(sampIsChatInputActive) == 'function' and sampIsChatInputActive() then return true end
    if type(sampIsDialogActive) == 'function' and sampIsDialogActive() then return true end
    return skin.keyboardBusy()
end

-- Esc apertado (WM_KEYDOWN) com uma janela do kit em foco: ela fecha no proximo quadro e o jogo nao recebe a tecla
-- (senao o menu de pausa abriria junto), nem as repeticoes ate soltar. Com um popup dela aberto, fecha so o popup.
-- Fica com o jogo se nao ha janela do kit com X em foco, se esta digitando ou segurando um controle, com o menu de
-- pausa, o chat ou um dialogo do SA-MP, ou se o jogador mexeu depois no menu de outro script. As teclas soltas
-- sempre passam (o GTA le o teclado pelas mensagens; tecla sem soltar ficaria presa).
local function escapeKey(msg, lparam)
    if msg ~= 0x100 then
        escHeld = false
        return false
    end
    if bit.band(lparam or 0, 0x40000000) ~= 0 then return escHeld end -- repeticao
    escHeld = false
    if (escWindow == nil and escPopup == nil) or os.clock() - escSeen > 0.5 or escBusy or keyboardBusy() or
       not skin.escWinner(file) then
        return false
    end
    if escPopup ~= nil then
        escPopupClose = escPopup -- primeiro o popup aberto, depois a janela (como nos menus da casa)
    else
        escClose = escWindow
    end
    escCloseFrames, escHeld = 2, true
    return true
end

-- O renderizador que o imgui.lua cria (um por script): repassa tudo e avisa o comeco do quadro.
local Renderer = imgui.ImGuiRenderer

local function wrapRenderer(real)
    return setmetatable({}, {
        __index = function(proxy, key)
            local original = real[key]
            if type(original) ~= 'function' then return original end
            local method
            if key == 'BeginFrame' then
                method = function(_, ...)
                    local a, b = original(real, ...)
                    fontsStart()
                    frameStart()
                    if escOn then escFrame() end
                    return a, b
                end
            elseif key == 'EndFrame' then
                method = function(_, ...)
                    if escOn then
                        local ok, busy = pcall(imgui.IsAnyItemActive)
                        escBusy = not ok or busy
                        if escLive then skin.escLive(file) end
                    end
                    return original(real, ...)
                end
            elseif key == 'DispatchWindowMessage' then
                -- O imgui.lua so segura a mensagem quando o ImGui quer o teclado; o Esc pego pelo kit a skin segura
                -- aqui mesmo e nao repassa (0 = mensagem nao tratada).
                method = function(_, msg, wparam, lparam, ...)
                    if escOn and wparam == 0x1B and (msg == 0x100 or msg == 0x101) then
                        local ok, taken = pcall(escapeKey, msg, lparam)
                        if ok and taken then
                            consumeWindowMessage(true, true)
                            return 0
                        end
                    end
                    return original(real, msg, wparam, lparam, ...)
                end
            elseif key == 'SwitchContext' then
                method = function(_, ...)
                    local a, b = original(real, ...)
                    fontsStart()
                    return a, b
                end
            else
                method = function(_, ...) return original(real, ...) end
            end
            rawset(proxy, key, method)
            return method
        end,
    })
end

local begin, endWindow = imgui.Begin, imgui.End
local NO_TITLE_BAR, NO_COLLAPSE, MENU_BAR = 1, 32, 1024

local function hasFlag(flags, bit)
    return math.floor(flags / bit) % 2 == 1
end

imgui.Begin = function(name, ...)
    local themed, newFlags, shell = false, nil, false
    local open, flags = ...
    if not dead then
        local ok
        ok, newFlags, themed = pcall(applyWindow, name, flags)
        if not ok then
            die(newFlags)
            newFlags, themed = nil, false
        end
        local f = newFlags or flags or 0
        shell = kitOn and themed and type(name) == 'string' and type(f) == 'number' and not hasFlag(f, NO_TITLE_BAR) and
                not hasFlag(f, MENU_BAR)
        if shell then
            newFlags = f + NO_TITLE_BAR + (hasFlag(f, NO_COLLAPSE) and 0 or NO_COLLAPSE)
        end
    end
    local visible
    if newFlags then
        visible = begin(name, open, newFlags)
    else
        visible = begin(name, ...)
    end
    local kit = (themed and kitOn) and KIT_WINDOW or 0
    if shell and visible and kitOn then
        local ok, err = pcall(kitShell, name, open)
        if ok then
            kit = kit + KIT_SHELL
        else
            kitFail(err)
        end
    end
    if kit > 0 and visible and escPopupClose ~= nil and escPopupClose == name then
        -- Esc com popup aberto: o foco volta para a janela e o ImGui 1.52 fecha, no quadro seguinte, o popup que
        -- ficou sem foco (o mesmo que clicar na janela, sem clicar em nada dela).
        escPopupClose = nil
        pcall(imgui.SetWindowFocus)
    end
    kitDepth = kitDepth + 1
    kitWindows[kitDepth] = kit
    kitNames[kitDepth] = name
    return visible
end

imgui.End = function(...)
    local kit = 0
    if kitDepth > 0 then
        kit = kitWindows[kitDepth] or 0
        kitWindows[kitDepth], kitNames[kitDepth] = nil, nil
        kitDepth = kitDepth - 1
    end
    if kit >= KIT_SHELL then
        -- O recorte empurrado no cabecalho sai aqui de qualquer jeito (push sem pop trava o 1.52).
        local ok, err = pcall(imgui.PopClipRect)
        if not ok then kitFail(err) end
    end
    return endWindow(...)
end

imgui.ImGuiRenderer = setmetatable({}, {
    __index = Renderer,
    __call = function(_, ...) return wrapRenderer(Renderer(...)) end,
})
