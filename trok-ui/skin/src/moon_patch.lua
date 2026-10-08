-- Trok Skin: visual da casa nos menus do imgui antigo do moonloader (moon_imgui: lib\imgui.lua + lib\MoonImGui.dll,
-- com o Dear ImGui 1.52). O .asi roda este arquivo uma vez em cada script, logo que o MoonImGui.dll monta a tabela
-- do modulo e antes do resto do imgui.lua: recebe a tabela do modulo e a tabela "skin" com as funcoes do .asi.
--
-- Troca tres coisas na tabela do modulo deste script:
--  - imgui.ImGuiRenderer: o renderizador que o imgui.lua cria passa a avisar o comeco de cada quadro (tema da
--    casa como base do estilo, como o igNewFrame do mimgui);
--  - imgui.Begin: antes de cada janela, padroniza (ou devolve ao visual do script, se ela fica como esta) e liga a
--    borda da janela e dos campos (ShowBorders, o FrameBorderSize do 1.52);
--  - AddFontFromFileTTF no atlas do script: fonte da casa no lugar das fontes de sistema.
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
local SCALARS = {'WindowRounding', 'ChildWindowRounding', 'FrameRounding', 'ScrollbarRounding', 'GrabRounding'}
local WINDOW_BG = 3

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

-- Visual: os campos do estilo que a skin troca (cantos, alinhamento do titulo e cores). Espacamentos e tamanhos
-- ficam como o script deixou.
local function newLook()
    return {s = {0, 0, 0, 0, 0}, ax = 0, ay = 0, c = {}}
end

local function copy(from, to)
    for i = 1, #SCALARS do to.s[i] = from.s[i] end
    to.ax, to.ay = from.ax, from.ay
    for i = 1, COUNT * 4 do to.c[i] = from.c[i] end
end

-- style: o ImGuiStyle pela API do moon_imgui (cores de 1 a 43) ou pela FFI (cores de 0 a 42, base = -1).
local function read(style, base, look)
    for i = 1, #SCALARS do look.s[i] = style[SCALARS[i]] end
    local align = style.WindowTitleAlign
    look.ax, look.ay = align.x, align.y
    local colors, c = style.Colors, look.c
    for i = 1, COUNT do
        local v = colors[i + base]
        c[i * 4 - 3], c[i * 4 - 2], c[i * 4 - 1], c[i * 4] = v.x, v.y, v.z, v.w
    end
end

local function near(a, b)
    return math.abs(a - b) < 1e-5
end

local function sameColor(a, b, i)
    local k = i * 4
    return near(a[k - 3], b[k - 3]) and near(a[k - 2], b[k - 2]) and near(a[k - 1], b[k - 1]) and near(a[k], b[k])
end

-- Escreve so o que mudou (as cores sao referencias para o estilo do contexto).
local function write(style, base, want, cur)
    for i = 1, #SCALARS do
        if not near(want.s[i], cur.s[i]) then style[SCALARS[i]] = want.s[i] end
    end
    if not near(want.ax, cur.ax) or not near(want.ay, cur.ay) then
        local align = style.WindowTitleAlign
        align.x, align.y = want.ax, want.ay
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
    if a.ax ~= b.ax or a.ay ~= b.ay then return false end
    for i = 1, #SCALARS do
        if a.s[i] ~= b.s[i] then return false end
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
        for _, field in ipairs({'Alpha', 'IndentSpacing', 'ScrollbarSize', 'GrabMinSize', 'CurveTessellationTol'}) do
            if style[field] ~= ptr[field] then return false end
        end
        for _, field in ipairs({'WindowPadding', 'FramePadding', 'ItemSpacing', 'ButtonTextAlign'}) do
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

local function setHouseColor(look, i)
    local k = i * 4
    look.c[k - 3], look.c[k - 2], look.c[k - 1], look.c[k] = house[k - 3], house[k - 2], house[k - 1], house[k]
end

-- Estado deste script: o visual dele (orig), para o tema poder desligar na hora, e o que a skin escreveu no
-- comeco do quadro (applied), para perceber o que o script mudou depois.
local orig, applied, cur, want = nil, newLook(), newLook(), newLook()
local houseLook = newLook() -- cantos e alinhamento da casa para a tela atual (calculado a cada quadro)
local theme = true          -- tema ligado neste quadro (/trokskin vale a partir do quadro seguinte)
local dead = false
local notes, noted = {}, 0

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
        for i = 1, #SCALARS do
            if not near(cur.s[i], applied.s[i]) then orig.s[i] = cur.s[i] end
        end
        if not near(cur.ax, applied.ax) or not near(cur.ay, applied.ay) then
            orig.ax, orig.ay = cur.ax, cur.ay
        end
        for i = 1, COUNT do
            if not sameColor(cur.c, applied.c, i) then
                local k = i * 4
                orig.c[k - 3], orig.c[k - 2], orig.c[k - 1], orig.c[k] = cur.c[k - 3], cur.c[k - 2], cur.c[k - 1], cur.c[k]
            end
        end
    end
    theme = skin.theme()
    local hs = houseLook.s
    hs[1], hs[2], hs[3], hs[4], hs[5], houseLook.ax, houseLook.ay = skin.scalars(imgui.GetIO().DisplaySize.y)
    copy(orig, want)
    if theme then
        for i = 1, #SCALARS do want.s[i] = hs[i] end
        want.ax, want.ay = houseLook.ax, houseLook.ay
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
        return nil
    end
    read(style, base, cur)
    local kept = skin.kept(title)
    note(title, kept and 'mantida (##trok ou manter=)' or theme and 'padronizada' or 'tema desligado')
    copy(cur, want)
    if kept or not theme then
        -- Sem push do script, o campo volta ao valor dele; com push, fica o que ele empurrou.
        for i = 1, #SCALARS do
            if near(cur.s[i], applied.s[i]) then want.s[i] = orig.s[i] end
        end
        if near(cur.ax, applied.ax) and near(cur.ay, applied.ay) then
            want.ax, want.ay = orig.ax, orig.ay
        end
        for i = 1, COUNT do
            if sameColor(cur.c, applied.c, i) then
                local k = i * 4
                want.c[k - 3], want.c[k - 2], want.c[k - 1], want.c[k] = orig.c[k - 3], orig.c[k - 2], orig.c[k - 1], orig.c[k]
            end
        end
        write(style, base, want, cur)
        return nil
    end
    for i = 1, #SCALARS do want.s[i] = houseLook.s[i] end
    want.ax, want.ay = houseLook.ax, houseLook.ay
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

-- Fonte da casa no atlas deste script (o metatable e o mesmo para todo ImFontAtlas do script). Liga no primeiro
-- SwitchContext/BeginFrame, com o contexto do script ativo.
local fontsReady = not skin.fonts()

local function patchFonts()
    fontsReady = true
    local atlas = imgui.GetIO().Fonts
    local mt = getmetatable(atlas)
    local index = type(mt) == 'table' and mt.__index
    local add = type(index) == 'function' and index(atlas, 'AddFontFromFileTTF')
    if type(add) ~= 'function' then
        skin.log('imgui antigo: ' .. file .. ': atlas de fontes diferente do esperado -- fonte fica como esta')
        return
    end

    local function replace(self, path, size, ...)
        local cfg, ranges = ...
        if type(path) ~= 'string' or type(size) ~= 'number' or not skin.replaceable(path) then return nil end
        if cfg ~= nil and cfg.MergeMode then return nil end
        local ratio = skin.ratio(path, ranges ~= nil) -- nil: a fonte fica (o log ja diz o motivo)
        if not ratio then return nil end
        local houseSize = size * ratio
        local font = add(self, skin.housePath(), houseSize, ...)
        if not font then
            skin.log('fonte da casa nao carregou; ' .. path .. ' ficou')
            return nil
        end
        -- Glifos que a fonte da casa nao tem vem da fonte original (o ImGui nao sobrescreve os que ja existem).
        local merge = cfg
        if merge == nil then merge = imgui.ImFontConfig() end
        merge.MergeMode = true
        local ok, err
        if ranges ~= nil then
            ok, err = pcall(add, self, path, houseSize, merge, ranges)
        else
            ok, err = pcall(add, self, path, houseSize, merge)
        end
        if cfg ~= nil then cfg.MergeMode = false end
        if not ok then skin.log('fonte de reserva nao entrou: ' .. tostring(err)) end
        skin.log(string.format('fonte %s %.1f px de %s -> fonte da casa %.1f px (mesma largura de texto)', path, size,
                               file, houseSize))
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

    mt.__index = function(self, key)
        if key == 'AddFontFromFileTTF' then return addFont end
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
