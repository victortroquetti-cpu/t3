-- Trok Model (MoonLoader) -- Victor_Trok
-- /model <id> cria o modelo flutuando um pouco a frente do seu personagem, girando devagar, so na sua tela (nada vai
-- para o servidor). Vale para objetos do gta3.img, do gta_int.img e do samp.img, e tambem para skins e veiculos.
-- /model sozinho apaga. Um modelo por vez: o novo substitui o anterior.
-- Precisa do MoonLoader e do SAMPFUNCS (como o Trok Kill List). Os textos ficam sem acento: o chat do SA-MP nao le UTF-8.

script_name('Trok Model')
script_author('Victor_Trok')
script_version('1.0')

local memory = require 'memory'

-- gta_sa.exe 1.0 US
local MODEL_INFO_PTRS_REF = 0x40CD67 -- onde o jogo guarda o endereco de CModelInfo::ms_modelInfoPtrs (limit adjusters)
local MODEL_INFO_PTRS = 0xA9B0C8     -- o endereco padrao, se a referencia nao parecer valida
local STREAMING_INFO = 0x8E4CC0      -- CStreaming::ms_aInfoForModel: 0x14 bytes por modelo, m_nImgId em +7
local STREAMING_FILES = 0x8E48D8     -- CStreaming::ms_files[8]: 0x30 bytes cada, o nome no comeco
local MAX_MODEL = 19999

local COLOR = 0xFFFFFF
local TAG = '{9AD8FF}[Trok Model]{FFFFFF} '
local LOAD_TIMEOUT = 3.0  -- s esperando o modelo carregar
local SPIN = 36           -- graus por segundo (uma volta a cada 10 s)
local BOB = 0.08          -- m de sobe e desce (so objetos)
local LIFT = 0.4          -- m entre o pe do personagem e a parte de baixo do modelo
local GAP = 1.0           -- m entre o personagem e a borda do modelo

-- Trens e bondes nao nascem com createCar (o jogo trava).
local TRAINS = { [449] = true, [537] = true, [538] = true, [569] = true, [570] = true, [590] = true }

local spawned = nil -- { kind, handle, id, x, y, z, heading, born }
local pending = nil -- o id pedido no chat (o carregamento roda no main, que pode esperar)

local function say(text)
    sampAddChatMessage(TAG .. text, COLOR)
end

local function now()
    return localClock and localClock() or os.clock()
end

local function kindOf(id)
    if id <= 311 then
        return 'skin'
    elseif id >= 400 and id <= 611 then
        return 'veiculo'
    end
    return 'objeto'
end

-- O modelo existe neste jogo? (o SA-MP registra os do samp.img na mesma tabela)
local function modelExists(id)
    local base = memory.getuint32(MODEL_INFO_PTRS_REF)
    if base < 0x400000 then
        base = MODEL_INFO_PTRS
    end
    return memory.getuint32(base + id * 4) ~= 0
end

-- De qual .img o modelo vem: "MODELS\GTA3.IMG" -> "gta3.img". nil se nao der para saber.
local function imgOf(id)
    local img = memory.getuint8(STREAMING_INFO + id * 0x14 + 7)
    if img >= 8 then
        return nil
    end
    local chars = {}
    for i = 0, 39 do
        local c = memory.getuint8(STREAMING_FILES + img * 0x30 + i)
        if c == 0 then
            break
        end
        if c < 32 or c > 126 then
            return nil
        end
        chars[#chars + 1] = string.char(c)
    end
    local name = table.concat(chars):match('([^\\/]+)$')
    return name and name ~= '' and name:lower() or nil
end

local function exists(s)
    if s.kind == 'objeto' then
        return doesObjectExist(s.handle)
    elseif s.kind == 'veiculo' then
        return doesVehicleExist(s.handle)
    end
    return doesCharExist(s.handle)
end

local function remove()
    if spawned and exists(spawned) then
        if spawned.kind == 'objeto' then
            deleteObject(spawned.handle)
        elseif spawned.kind == 'veiculo' then
            deleteCar(spawned.handle)
        else
            deleteChar(spawned.handle)
        end
    end
    spawned = nil
end

local function create(kind, id, x, y, z, heading)
    local h
    if kind == 'objeto' then
        h = createObject(id, x, y, z)
        setObjectCollision(h, false)
        freezeObjectPosition(h, true)
        setObjectHeading(h, heading)
    elseif kind == 'veiculo' then
        h = createCar(id, x, y, z)
        freezeCarPosition(h, true)
        setCarCollision(h, false)
        setCarProofs(h, true, true, true, true, true)
        lockCarDoors(h, 2) -- trancado: o carro nao existe no servidor
        setCarHeading(h, heading)
    else
        h = createChar(4, id, x, y, z)
        freezeCharPosition(h, true)
        setCharCollision(h, false)
        setCharProofs(h, true, true, true, true, true)
        clearCharTasksImmediately(h)
        setCharHeading(h, heading)
    end
    return h
end

local function spawn(id)
    if id < 0 or id > MAX_MODEL then
        say('o id vai de 0 a ' .. MAX_MODEL .. '.')
        return
    end
    if not modelExists(id) then
        say('o modelo ' .. id .. ' nao existe neste jogo.')
        return
    end
    local kind = kindOf(id)
    if id == 0 then
        say('o 0 e o CJ: a roupa dele e montada pelo jogo, nao da para criar assim.')
        return
    end
    if kind == 'veiculo' and TRAINS[id] then
        say('o ' .. id .. ' e um trem: trens nao nascem assim.')
        return
    end

    requestModel(id)
    loadAllModelsNow()
    local deadline = now() + LOAD_TIMEOUT
    while not hasModelLoaded(id) do
        if now() > deadline then
            say('o modelo ' .. id .. ' nao carregou.')
            return
        end
        wait(0)
    end

    -- A frente do personagem, longe o bastante para o modelo inteiro caber, com a parte de baixo um pouco acima do pe.
    local minX, minY, minZ, maxX, maxY = getModelDimensions(id)
    if not (minX and minY and minZ and maxX and maxY) then
        minX, minY, minZ, maxX, maxY = -0.5, -0.5, -0.5, 0.5, 0.5 -- sem colisao: o jogo nao sabe o tamanho
    end
    local radius = 0
    for _, cx in ipairs({ minX, maxX }) do
        for _, cy in ipairs({ minY, maxY }) do
            radius = math.max(radius, math.sqrt(cx * cx + cy * cy))
        end
    end
    local px, py, pz = getCharCoordinates(PLAYER_PED)
    local heading = getCharHeading(PLAYER_PED)
    local rad = math.rad(heading)
    local dist = math.max(2.0, radius + GAP)
    local x = px - math.sin(rad) * dist
    local y = py + math.cos(rad) * dist
    local z = (pz - 1.0) + LIFT - minZ -- a posicao do personagem fica ~1 m acima do pe

    remove()
    local facing = (heading + 180) % 360 -- de frente para voce
    local handle = create(kind, id, x, y, z, facing)
    markModelAsNoLongerNeeded(id)
    spawned = { kind = kind, handle = handle, id = id, x = x, y = y, z = z, heading = facing, born = now() }

    local img = imgOf(id)
    say(string.format('%d: %s%s. {AAAAAA}/model sozinho apaga.', id, kind, img and (' do ' .. img) or ''))
end

local function animate()
    if not spawned then
        return
    end
    if not exists(spawned) then
        spawned = nil -- o jogo apagou (mudou de interior, reconectou...)
        return
    end
    local t = now() - spawned.born
    local heading = (spawned.heading + t * SPIN) % 360
    if spawned.kind == 'objeto' then
        setObjectHeading(spawned.handle, heading)
        setObjectCoordinates(spawned.handle, spawned.x, spawned.y, spawned.z + math.sin(t * 2) * BOB)
    elseif spawned.kind == 'veiculo' then
        setCarHeading(spawned.handle, heading)
    else
        setCharHeading(spawned.handle, heading)
    end
end

local function command(arg)
    arg = (arg or ''):match('^%s*(.-)%s*$')
    if arg == '' then
        if spawned then
            remove()
            say('apagado.')
        else
            say('use /model <id> (objeto, skin ou veiculo). /model sozinho apaga.')
        end
        return
    end
    local id = tonumber(arg)
    if not id or id % 1 ~= 0 then
        say('use /model <id>, com o numero do modelo.')
        return
    end
    pending = math.floor(id)
end

function main()
    if not isSampLoaded() or not isSampfuncsLoaded() then
        return
    end
    while not isSampAvailable() do
        wait(100)
    end
    sampRegisterChatCommand('model', command)
    while true do
        wait(0)
        if pending then
            local id = pending
            pending = nil
            spawn(id)
        end
        animate()
    end
end

function onScriptTerminate(s)
    if s == thisScript() then
        remove()
    end
end
