-- Teste do Trok Skin: imita o que o imgui.lua do moon_imgui 1.1.5 usa da API do moonloader. O moon_host.exe roda
-- este arquivo em cada lua_State antes do script (HOST_HWND, HOST_DEVICE, HOST_FONTS e SCRIPT_FILE vem dele).
package.path = 'moonloader\\lib\\?.lua;moonloader\\lib\\?\\init.lua'
package.cpath = 'moonloader\\lib\\?.dll'

local events = {}

function addEventHandler(name, fn)
    events[name] = events[name] or {}
    table.insert(events[name], fn)
end

function getMoonloaderVersion() return 26 end
function readMemory(address) return address == 0x00C8CF88 and HOST_HWND or 0 end
function getD3DDevicePtr() return HOST_DEVICE end
function getFolderPath() return HOST_FONTS end -- 0x14: pasta de fontes do Windows
function doesFileExist(path)
    local file = io.open(path, 'rb')
    if file then file:close() end
    return file ~= nil
end
function isPauseMenuActive() return false end
function showCursor() end
function lockPlayerControl() end
function wait() end
function thisScript()
    return {filename = SCRIPT_FILE, name = SCRIPT_FILE:gsub('%.lua$', ''), path = 'moonloader\\' .. SCRIPT_FILE}
end
lua_thread = {create = function() return {} end}

package.preload['windows.message'] = function()
    return {WM_KEYDOWN = 0x100, WM_KEYUP = 0x101, WM_SYSKEYDOWN = 0x104, WM_SYSKEYUP = 0x105, WM_KILLFOCUS = 0x8}
end
package.preload['bitex'] = function()
    return {bextract = function(v, field, width) return bit.band(bit.rshift(v, field), bit.lshift(1, width) - 1) end}
end
package.preload['memory'] = function() return {fill = function() end} end

-- Um quadro: o onD3DPresent do script (o imgui.lua desenha ali).
function __host_frame()
    for _, fn in ipairs(events.onD3DPresent or {}) do
        fn()
    end
end

-- Uma mensagem da janela do jogo (mouse ou teclado), como no moonloader: os onWindowMessage do script, ate um deles
-- segurar a mensagem (consumeWindowMessage). Devolve true se o script segurou (o jogo e os outros scripts nao a veem).
local consumed = false
function consumeWindowMessage() consumed = true end
function __host_message(msg, wparam, lparam)
    consumed = false
    for _, fn in ipairs(events.onWindowMessage or {}) do
        fn(msg, wparam, lparam)
        if consumed then break end
    end
    return consumed
end

-- Valores do script (o que o teste confere depois dos cliques): o script define HOST_VALUES (ou outra HOST_*).
function __host_values(which)
    local fn = _G[which or 'HOST_VALUES']
    return fn and fn() or ''
end
