-- Teste do Trok UI Showcase.lua sem o jogo: um mimgui de mentira que confere a API usada,
-- o equilibrio das pilhas (Push/Pop, Begin/End, canais) e os tipos dos argumentos.
-- Uso (na pasta lua/): luajit test/mock_run.lua

local ffi = require 'ffi'
ffi.cdef [[ typedef unsigned short ImWchar; ]]
-- user32 de mentira (o cursor da casa usa IsWindow/SetPropA/RemovePropA).
local realLoad = ffi.load
ffi.load = function(name)
    if name == 'user32' then
        return { IsWindow = function() return 1 end, SetPropA = function() return 1 end, RemovePropA = function() end }
    end
    return realLoad(name)
end

local SCRIPT = arg[1] or 'Trok UI Showcase.lua'
local failures = 0
local function fail(msg)
    failures = failures + 1
    io.stderr:write('FALHA: ' .. msg .. '\n' .. debug.traceback('', 2) .. '\n')
    if failures > 5 then os.exit(1) end
end

local function isVec(v) return type(v) == 'table' and type(v.x) == 'number' and type(v.y) == 'number' end
local function isNum(v) return type(v) == 'number' and v == v end
local function check(cond, msg) if not cond then fail(msg) end end

local seen = {}   -- cobertura: janelas, popups e textos que apareceram
local texts = {}

-- ---------------------------------------------------------------- estado do quadro
local frame = {}
local function resetFrame()
    frame = { styleVar = 0, styleColor = 0, font = 0, window = {}, child = 0, popup = 0, channels = {}, clip = 0,
        ids = {}, clickIds = frame.clickIds or {}, hovered = frame.hovered or {}, lastId = nil, drawCalls = 0,
        popupsOpen = frame.popupsOpen or {}, openRequests = {} }
end
resetFrame()

local function drawList(name)
    local dl = { name = name, split = 0, current = 0 }
    local function vecs(...)
        for i, v in ipairs({ ... }) do check(isVec(v), name .. ': argumento ' .. i .. ' nao e ImVec2') end
    end
    function dl:AddLine(a, b, col, th) vecs(a, b); check(isNum(col), 'AddLine cor'); check(isNum(th), 'AddLine espessura'); frame.drawCalls = frame.drawCalls + 1 end
    function dl:AddRect(a, b, col, r, flags, th) vecs(a, b); check(isNum(col) and isNum(r) and isNum(th), 'AddRect args') end
    function dl:AddRectFilled(a, b, col, r, flags) vecs(a, b); check(isNum(col), 'AddRectFilled cor'); check(r == nil or isNum(r), 'AddRectFilled raio') end
    function dl:AddRectFilledMultiColor(a, b, c1, c2, c3, c4) vecs(a, b); check(isNum(c1) and isNum(c2) and isNum(c3) and isNum(c4), 'MultiColor') end
    function dl:AddCircle(c, r, col, seg, th) vecs(c); check(isNum(r) and isNum(col) and isNum(seg), 'AddCircle') end
    function dl:AddCircleFilled(c, r, col, seg) vecs(c); check(isNum(r) and isNum(col), 'AddCircleFilled') end
    function dl:AddTextFontPtr(font, size, pos, col, text) check(type(font) == 'table' and font.FontSize, 'fonte'); check(isNum(size), 'tamanho'); vecs(pos); check(isNum(col), 'cor do texto'); check(type(text) == 'string', 'texto nao e string: ' .. tostring(text)); texts[text] = true end
    function dl:AddImageQuad(tex, p1, p2, p3, p4, u1, u2, u3, u4, col) check(tex ~= nil, 'textura'); vecs(p1, p2, p3, p4, u1, u2, u3, u4); check(isNum(col), 'cor do icone') end
    function dl:ChannelsSplit(n) check(self.split == 0, name .. ': ChannelsSplit dentro de outro split'); self.split = n; self.current = 0; frame.channels[self] = true end
    function dl:ChannelsSetCurrent(i) check(self.split > 0 and i < self.split, name .. ': canal ' .. i .. ' fora do split ' .. self.split); self.current = i end
    function dl:ChannelsMerge() check(self.split > 0, name .. ': Merge sem Split'); self.split = 0; frame.channels[self] = nil end
    function dl:PushClipRectFullScreen() frame.clip = frame.clip + 1 end
    function dl:PushClipRect(a, b, intersect) vecs(a, b); check(type(intersect) == 'boolean', 'PushClipRect intersect'); frame.clip = frame.clip + 1 end
    function dl:PopClipRect() frame.clip = frame.clip - 1 end
    return dl
end

local windowLists = {}
local function windowDrawList(name)
    windowLists[name] = windowLists[name] or drawList(name)
    return windowLists[name]
end
local fg, bg = drawList('foreground'), drawList('background')

-- ---------------------------------------------------------------- mimgui de mentira
local imgui = {}
local function vec(x, y) return { x = x or 0, y = y or 0 } end
imgui.ImVec2 = function(x, y) check(isNum(x) and isNum(y), 'ImVec2 com nao-numero: ' .. tostring(x) .. ', ' .. tostring(y)); return vec(x, y) end
imgui.ImVec4 = function(x, y, z, w) check(isNum(x) and isNum(y) and isNum(z) and isNum(w), 'ImVec4'); return { x = x, y = y, z = z, w = w } end
imgui.ImFontConfig = function() return {} end
imgui.ColorConvertFloat4ToU32 = function(v) check(type(v) == 'table' and v.w, 'Float4ToU32'); return 0xFFFFFFFF end

local glyphs = {
    [0x78] = true, [0xE049] = true, [0xE06F] = true, [0xE073] = true, [0xE0BA] = true, [0xE0BB] = true,
}
local function makeFont(size, isIcon)
    local f = { FontSize = size, ContainerAtlas = { TexID = 'atlas' } }
    function f:CalcTextSizeA(sz, maxw, wrapw, text)
        check(isNum(sz) and type(text) == 'string', 'CalcTextSizeA')
        return vec(#text * sz * 0.5, sz)
    end
    function f:FindGlyphNoFallback(cp)
        check(isNum(cp), 'FindGlyphNoFallback')
        if isIcon and glyphs[cp] then
            return { X0 = 1, Y0 = 2, X1 = size - 1, Y1 = size - 2, U0 = 0.1, V0 = 0.1, U1 = 0.2, V1 = 0.2 }
        end
        return nil
    end
    function f:FindGlyph(cp) return { X0 = 0, Y0 = size * 0.25, X1 = size * 0.6, Y1 = size * 0.8 } end
    return f
end

local io = {
    Fonts = {}, ConfigFlags = 0, DeltaTime = 1 / 60, MousePos = vec(0, 0), MouseDelta = vec(0, 0),
    WantTextInput = false, MouseDrawCursor = false,
}
local fontFiles = {}
function io.Fonts:GetGlyphRangesCyrillic() return 'cyrillic' end
function io.Fonts:AddFontFromFileTTF(path, size, cfg, ranges)
    check(type(path) == 'string' and isNum(size), 'AddFontFromFileTTF')
    check(ranges ~= nil, 'faixa de glifos ausente')
    fontFiles[#fontFiles + 1] = path
    return makeFont(size, path:find('lucide') ~= nil)
end
function io.Fonts:AddFontDefault() return makeFont(13, false) end
imgui.GetIO = function() return io end

imgui.StyleVar = setmetatable({}, { __index = function(_, k) return 'var:' .. k end })
imgui.Col = setmetatable({}, { __index = function(_, k) return 'col:' .. k end })
imgui.WindowFlags = setmetatable({}, { __index = function(_, k) return 1 end })
imgui.Cond = { Always = 1, Appearing = 2 }
imgui.MouseCursor = { Arrow = 0, TextInput = 1, ResizeAll = 2, Hand = 7 }
imgui.InputTextFlags = { Password = 32768 }
imgui.ConfigFlags = { NoMouseCursorChange = 32 }
imgui.DrawCornerFlags = { Bot = 12 }

imgui.PushStyleVarFloat = function(k, v) check(isNum(v), 'PushStyleVarFloat ' .. tostring(k)); frame.styleVar = frame.styleVar + 1 end
imgui.PushStyleVarVec2 = function(k, v) check(isVec(v), 'PushStyleVarVec2'); frame.styleVar = frame.styleVar + 1 end
imgui.PopStyleVar = function(n) frame.styleVar = frame.styleVar - (n or 1); check(frame.styleVar >= 0, 'PopStyleVar demais') end
imgui.PushStyleColor = function(k, v) check(type(v) == 'table' and v.w, 'PushStyleColor'); frame.styleColor = frame.styleColor + 1 end
imgui.PopStyleColor = function(n) frame.styleColor = frame.styleColor - (n or 1); check(frame.styleColor >= 0, 'PopStyleColor demais') end
imgui.PushFont = function(f) check(type(f) == 'table', 'PushFont sem fonte'); frame.font = frame.font + 1 end
imgui.PopFont = function() frame.font = frame.font - 1; check(frame.font >= 0, 'PopFont demais') end

local cursor = vec(0, 0)
local currentWindow = { name = 'none', pos = vec(0, 0), size = vec(1600, 900) }
local nextPos, nextSize
imgui.SetNextWindowPos = function(p, cond, pivot) check(isVec(p), 'SetNextWindowPos'); nextPos = p end
imgui.SetNextWindowSize = function(s, cond) check(isVec(s), 'SetNextWindowSize'); nextSize = s end
imgui.SetNextWindowFocus = function() end
imgui.Begin = function(id, open, flags)
    check(type(id) == 'string', 'Begin id'); check(isNum(flags), 'Begin flags')
    local w = { name = id, pos = nextPos or vec(100, 100), size = nextSize or vec(400, 300), parent = currentWindow }
    nextPos, nextSize = nil, nil
    table.insert(frame.window, w)
    currentWindow = w
    cursor = vec(w.pos.x, w.pos.y)
    seen['janela ' .. id] = true
    return true
end
imgui.End = function()
    local w = table.remove(frame.window)
    check(w ~= nil, 'End sem Begin')
    currentWindow = w and w.parent or currentWindow
end
imgui.BeginChild = function(id, size, border, flags)
    check(isVec(size), 'BeginChild tamanho')
    frame.child = frame.child + 1
    local w = { name = currentWindow.name .. '/' .. id, pos = vec(cursor.x, cursor.y), size = size, parent = currentWindow }
    table.insert(frame.window, w)
    currentWindow = w
    return true
end
imgui.EndChild = function()
    frame.child = frame.child - 1
    local w = table.remove(frame.window)
    currentWindow = w.parent
end
imgui.GetWindowDrawList = function() return windowDrawList(currentWindow.name) end
imgui.GetForegroundDrawList = function() return fg end
imgui.GetBackgroundDrawList = function() return bg end
imgui.GetWindowPos = function() return vec(currentWindow.pos.x, currentWindow.pos.y) end
imgui.GetWindowSize = function() return vec(currentWindow.size.x, currentWindow.size.y) end
imgui.GetWindowHeight = function() return currentWindow.size.y end
imgui.GetContentRegionAvail = function() return vec(currentWindow.size.x, currentWindow.size.y) end
imgui.GetScrollY = function() return 0 end
imgui.SetScrollY = function(v) check(isNum(v), 'SetScrollY') end
imgui.GetCursorScreenPos = function() return vec(cursor.x, cursor.y) end
imgui.SetCursorScreenPos = function(p) check(isVec(p), 'SetCursorScreenPos'); cursor = vec(p.x, p.y) end
imgui.Dummy = function(s) check(isVec(s), 'Dummy'); cursor = vec(cursor.x, cursor.y + s.y) end
imgui.SetNextItemWidth = function(w) check(isNum(w), 'SetNextItemWidth') end
imgui.SetKeyboardFocusHere = function() end
imgui.IsWindowAppearing = function() return false end

local function item(id)
    check(type(id) == 'string', 'id de item nao e string')
    frame.lastId = id
    if frame.ids[currentWindow.name .. id] then
        fail('id repetido na mesma janela: ' .. id .. ' (' .. currentWindow.name .. ')')
    end
    frame.ids[currentWindow.name .. id] = true
end
imgui.InvisibleButton = function(id, size)
    item(id); check(isVec(size) and size.x > 0 and size.y > 0, 'InvisibleButton tamanho ' .. id)
    cursor = vec(cursor.x, cursor.y + size.y)
    if frame.clickIds[id] then
        frame.clickIds[id] = nil
        return true
    end
    return false
end
imgui.InputText = function(id, buf, size, flags) item(id); check(type(size) == 'number', 'InputText tamanho'); return false end
imgui.InputTextMultiline = function(id, buf, size, sz, flags) item(id); check(isVec(sz), 'Multiline tamanho'); return false end
imgui.IsItemHovered = function() return frame.hovered[frame.lastId] == true end
imgui.IsItemActive = function() return frame.active ~= nil and frame.active[frame.lastId] == true end
imgui.IsItemClicked = function() return false end
imgui.IsMouseDragging = function() return frame.mouseDown == true end
imgui.IsMouseDown = function() return frame.mouseDown == true end
imgui.IsMouseClicked = function() return false end
imgui.IsMouseDoubleClicked = function() return false end
imgui.SetMouseCursor = function(c) check(isNum(c), 'SetMouseCursor') end
imgui.GetMouseCursor = function() return 0 end
imgui.GetTime = function() return os.clock() end
imgui.OpenPopup = function(id) check(type(id) == 'string', 'OpenPopup'); frame.popupsOpen[id] = true end
imgui.IsPopupOpen = function(id) return frame.popupsOpen[id] == true end
imgui.BeginPopup = function(id)
    if not frame.popupsOpen[id] then return false end
    seen['popup ' .. id] = true
    frame.popup = frame.popup + 1
    local w = { name = 'popup' .. id, pos = vec(cursor.x, cursor.y), size = vec(200, 200), parent = currentWindow, popupId = id }
    table.insert(frame.window, w)
    currentWindow = w
    return true
end
imgui.EndPopup = function()
    frame.popup = frame.popup - 1
    local w = table.remove(frame.window)
    currentWindow = w.parent
end
imgui.CloseCurrentPopup = function() frame.popupsOpen[currentWindow.popupId] = nil end
imgui.SetClipboardText = function(t) check(type(t) == 'string', 'SetClipboardText') end
imgui.GetClipboardText = function() return 'colado' end
imgui.new = setmetatable({}, { __index = function(_, ctype)
    return setmetatable({}, { __index = function(_, n)
        return function(...) return ffi.new(ctype .. '[?]', n, ...) end
    end })
end })

local initCallback
local frames = {}
imgui.OnInitialize = function(cb) initCallback = cb end
imgui.OnFrame = function(cond, draw)
    local f = { cond = cond, draw = draw, HideCursor = true }
    frames[#frames + 1] = f
    return f
end

-- ---------------------------------------------------------------- moonloader de mentira
local keysDown, keysPressed = {}, {}
local commands = {}
package.loaded['moonloader'] = {}
package.loaded['mimgui'] = imgui
package.loaded['vkeys'] = { id_to_name = function(id) return id == 0x4D and 'M' or ('VK' .. id) end }
script_name = function() end
script_author = function() end
script_version = function() end
script_description = function() end
getScreenResolution = function() return 1600, 900 end
getWorkingDirectory = function() return 'C:\\GTA\\moonloader' end
doesFileExist = function(path) return path:find('trok') ~= nil end
readMemory = function() return 0 end
isPauseMenuActive = function() return false end
wasKeyPressed = function(vk) return keysPressed[vk] == true end
isKeyDown = function(vk) return keysDown[vk] == true end
isSampfuncsLoaded = function() return true end
isSampLoaded = function() return true end
isSampAvailable = function() return true end
sampRegisterChatCommand = function(name, cb) commands[name] = cb end
chatMessages = {}
sampAddChatMessage = function(text) chatMessages[#chatMessages + 1] = text end
consumeWindowMessage = function() end
thisScript = function() return { version = '1.0.0' } end
wait = coroutine.yield

local chunk = assert(loadfile(SCRIPT))
chunk()
assert(initCallback, 'sem imgui.OnInitialize')
-- Como no mimgui: o OnInitialize so roda no primeiro quadro em que algum OnFrame pede para desenhar.
local initialized = false

local mainThread = coroutine.create(main)
local function step(opts)
    opts = opts or {}
    io.WantTextInput = opts.typing or false
    local ok, err = coroutine.resume(mainThread)
    if not ok then fail('main(): ' .. tostring(err)) end
    -- Teclas chegam pelas mensagens da janela, como no jogo.
    for vk in pairs(opts.keys or {}) do
        local ok3, err3 = pcall(onWindowMessage, 0x100, vk, 0)
        if not ok3 then fail('onWindowMessage: ' .. tostring(err3)) end
    end
    resetFrame()
    frame.clickIds = opts.click or {}
    frame.hovered = opts.hover or {}
    frame.active = opts.active
    frame.mouseDown = opts.mouseDown
    local wants = {}
    for i, f in ipairs(frames) do
        wants[i] = f.cond()
    end
    if not initialized then
        for i = 1, #frames do
            if wants[i] then
                initialized = true
                local okInit, errInit = pcall(initCallback)
                if not okInit then fail('OnInitialize: ' .. tostring(errInit)) end
                check(#fontFiles >= 5, 'fontes da casa nao carregadas (' .. #fontFiles .. ')')
                break
            end
        end
    end
    for i, f in ipairs(frames) do
        if initialized and wants[i] then
            local ok2, err2 = pcall(f.draw, f)
            if not ok2 then fail('quadro: ' .. tostring(err2)) end
        end
    end
    for vk in pairs(opts.keys or {}) do onWindowMessage(0x101, vk, 0) end
    for id in pairs(frame.clickIds) do
        fail('clique em item que nao existe neste quadro: ' .. id)
        frame.clickIds[id] = nil
    end
    check(frame.styleVar == 0, 'PushStyleVar sem Pop: ' .. frame.styleVar)
    check(frame.styleColor == 0, 'PushStyleColor sem Pop: ' .. frame.styleColor)
    check(frame.font == 0, 'PushFont sem Pop: ' .. frame.font)
    check(#frame.window == 0, 'Begin sem End: ' .. #frame.window)
    check(frame.child == 0, 'BeginChild sem End')
    check(frame.popup == 0, 'BeginPopup sem End')
    check(frame.clip == 0, 'PushClipRect sem Pop')
    for dl in pairs(frame.channels) do fail('ChannelsSplit sem Merge em ' .. dl.name) end
end

-- Teclas e cliques valem so no primeiro quadro; o hover continua nos seguintes.
local function run(n, opts)
    opts = opts or {}
    step(opts)
    for _ = 2, n do step({ hover = opts.hover, typing = opts.typing, active = opts.active, mouseDown = opts.mouseDown }) end
end

-- Primeiro quadro depois de carregar: o mimgui precisa inicializar sozinho.
step()
check(initialized, 'o mimgui nunca inicializou: nenhum OnFrame pediu para desenhar')
-- Abre como o /trokuilua (com Enter apertado no mesmo quadro).
assert(commands.trokuilua, 'comando /trokuilua nao registrado')
commands.trokuilua('')
run(2, { keys = { [0x0D] = true } })

local TABS = { 'Linhas', 'Texto', 'Listas', 'Avisos', 'Di\195\161logos' }
for t = 1, #TABS do
    run(3, { click = { ['##abas##aba' .. t] = true } })
    -- Passa pelas linhas com o teclado e mexe em todas com setas e Enter.
    for _ = 1, 16 do
        run(1, { keys = { [0x28] = true } })
        run(1, { keys = { [0x27] = true } })
        run(1, { keys = { [0x25] = true } })
    end
    -- Hover em tudo que tem dica (Restaurar, info, olho).
    run(3, { hover = setmetatable({}, { __index = function() return true end }) })
end

-- Popups: cor e lista suspensa da aba Linhas.
run(2, { click = { ['##abas##aba1'] = true } })
run(3, { click = { ['Linhas##corA12'] = true } })
run(3)
run(1, { keys = { [0x1B] = true } })
run(3, { click = { ['Linhas##menu13'] = true } })
run(2, { click = { ['##op2'] = true } })
-- Captura de tecla.
run(2, { click = { ['Linhas##k11'] = true } })
run(2, { keys = { [0x4E] = true } })
-- Menu do campo (botao direito) na aba Texto.
run(2, { click = { ['##abas##aba2'] = true } })
frame.popupsOpen['##menuCampoTexto##entrada1'] = true
run(2, { click = { ['##colar'] = true } })
-- Confirmacao e notificacao.
run(2, { click = { ['##abas##aba4'] = true } })
run(3, { click = { ['Avisos##acao7'] = true } })
run(3, { keys = { [0x0D] = true } })
run(2, { click = { ['Avisos##acao6'] = true } })
-- Cada dialogo de exemplo.
run(2, { click = { ['##abas##aba5'] = true } })
for i = 1, 5 do
    run(2, { click = { [TABS[5] .. '##acao' .. i] = true } })
    run(3, { keys = { [0x28] = true } })
    run(2, { hover = setmetatable({}, { __index = function() return true end }) })
    run(2, { keys = { [0x1B] = true } })
end
-- Modo de mover.
run(2, { click = { ['##abas##aba1'] = true } })
run(2, { click = { ['Linhas##acao1'] = true } })
run(5)
io.MouseDelta = vec(-12, 6)
run(4, { active = { ['##previa'] = true }, mouseDown = true })
io.MouseDelta = vec(0, 0)
run(3)
run(2, { click = { ['Linhas##acao1'] = true } })
run(2, { keys = { [0x1B] = true } })
-- Fecha.
run(3, { keys = { [0x1B] = true } })
run(20)

local names = {}
for k in pairs(seen) do names[#names + 1] = k end
table.sort(names)
print('cobertura: ' .. table.concat(names, ' | '))
for _, t in ipairs({ 'Restaurar', 'Pressione uma tecla', 'Resposta do di\195\161logo', 'Padr\195\181es restaurados',
    'Solte para fixar', 'Arraste para mover', 'Trok UI', 'padr\195\163o', 'Copiar', 'R 230   G 230   B 230',
    'VK78', 'Posi\195\167\195\163o salva' }) do
    check(texts[t], 'texto esperado nunca apareceu: ' .. t)
end
check(#chatMessages == 0, 'aviso no chat com o mimgui funcionando: ' .. tostring(chatMessages[1]))
if failures == 0 then
    print('ok: todas as abas, popups, dialogos e modo de mover rodaram sem erro e com as pilhas equilibradas')
else
    print(failures .. ' falha(s)')
    os.exit(1)
end
