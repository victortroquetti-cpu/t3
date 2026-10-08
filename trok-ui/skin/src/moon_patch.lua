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
    if not orig then return nil end -- nenhum quadro comecou ainda
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
        return nil
    end
    read(style, base, cur)
    local kept = skin.kept(title)
    note(title, kept and 'mantida (##trok ou manter=)' or theme and 'padronizada' or 'tema desligado')
    copy(cur, want)
    if kept or not theme then
        keepScript(false)
        write(style, base, want, cur)
        return nil
    end
    setHouseFields(want)
    for i = 1, COUNT do
        -- Cores com significado (texto vermelho, grafico verde) ficam como o script empurrou.
        if not palette.significado[i] and not transparent(i, cur.c[i * 4]) then setHouseColor(want, i) end
    end
    write(style, base, want, cur)
    if type(flags) ~= 'number' and flags ~= nil then return nil end
    flags = flags or 0
    if math.floor(flags / SHOW_BORDERS) % 2 == 1 then return nil end
    return flags + SHOW_BORDERS
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
        if cfg ~= nil then cfg.MergeMode = false end
        if not ok then skin.log('fonte de reserva nao entrou: ' .. tostring(err)) end
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
                    return a, b
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

local begin = imgui.Begin

imgui.Begin = function(name, ...)
    if not dead then
        local open, flags = ...
        local ok, newFlags = pcall(applyWindow, name, flags)
        if not ok then
            die(newFlags)
        elseif newFlags then
            return begin(name, open, newFlags)
        end
    end
    return begin(name, ...)
end

imgui.ImGuiRenderer = setmetatable({}, {
    __index = Renderer,
    __call = function(_, ...) return wrapRenderer(Renderer(...)) end,
})
