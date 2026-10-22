-- Teste do Trok Model.lua sem o jogo: um MoonLoader + SAMPFUNCS de mentira (o mundo, a memoria do GTA, o chat e o
-- carregamento de modelos) roda o script quadro a quadro e confere cada /model.
-- Uso (na pasta trok-model/): luajit test/mock_run.lua

local SCRIPT = arg[1] or 'Trok Model.lua'
local failures, checks = 0, 0
local function check(cond, msg)
    checks = checks + 1
    if cond then
        print('ok: ' .. msg)
    else
        failures = failures + 1
        print('FALHA: ' .. msg)
    end
end
local function near(a, b, eps) return type(a) == 'number' and math.abs(a - b) <= (eps or 1e-3) end

-- O arquivo tem que ser ASCII puro (o chat do SA-MP nao le UTF-8).
do
    local f = assert(io.open(SCRIPT, 'rb'))
    local src = f:read('*a')
    f:close()
    check(not src:find('[\128-\255]'), 'o .lua e ASCII puro')
end

-- ---------------------------------------------------------------- memoria do GTA de mentira

local mem32, mem8 = {}, {}
local INFO_REF, INFO = 0x40CD67, 0xA9B0C8
local STREAMING, FILES = 0x8E4CC0, 0x8E48D8
local infoBase = INFO
mem32[INFO_REF] = INFO
local function putName(img, name)
    for i = 1, #name do mem8[FILES + img * 0x30 + i - 1] = name:byte(i) end
    mem8[FILES + img * 0x30 + #name] = 0
end
putName(0, 'MODELS\\GTA3.IMG')
putName(1, 'MODELS\\GTA_INT.IMG')
putName(5, 'SAMP\\SAMP.IMG')
local models = {} -- id -> { img, dims, loads }
local function model(id, img, dims, loads)
    models[id] = { img = img, dims = dims, loads = loads ~= false }
    mem8[STREAMING + id * 0x14 + 7] = img
end
model(1337, 0, { -0.5, -0.5, -0.5, 0.5, 0.5, 0.5 })      -- gta3.img, 1 m
model(14444, 1, { -3, -1, 0, 3, 1, 2 })                  -- gta_int.img
model(19475, 5, { -0.1, -0.1, -0.1, 0.1, 0.1, 0.1 })     -- samp.img
model(411, 0, { -1.0, -2.5, -0.6, 1.0, 2.5, 0.8 })       -- Infernus
model(537, 0, { -1.5, -9, -1, 1.5, 9, 2 })               -- trem
model(46, 0, { -0.4, -0.3, -1.0, 0.4, 0.3, 0.9 })        -- skin
model(0, 0, { -0.4, -0.3, -1.0, 0.4, 0.3, 0.9 })         -- CJ
model(2000, 0, { -1, -1, -1, 1, 1, 1 }, false)           -- nunca carrega
model(3000, 9, { -1, -1, -1, 1, 1, 1 })                  -- .img desconhecido
model(3001, 0, {})                                       -- sem colisao: o jogo nao da as medidas
local function syncInfoPtrs()
    for id in pairs(models) do mem32[infoBase + id * 4] = 0x1000000 + id end
end
syncInfoPtrs()

package.preload['memory'] = function()
    return {
        getuint32 = function(a) return mem32[a] or 0 end,
        getuint8 = function(a) return mem8[a] or 0 end,
    }
end

-- ---------------------------------------------------------------- MoonLoader e SAMPFUNCS de mentira

local clock = 100.0
local chat = {}
local commands = {}
local entities = {} -- handle -> { kind, model, x, y, z, heading, collision, frozen, locked, proofs, pedtype }
local nextHandle = 1
local requested, loaded = {}, {}
local player = { x = 100, y = 200, z = 11, heading = 90 }
local calls = { loadAll = 0, noLonger = {} }

_G.PLAYER_PED = 1
function script_name() end
function script_author() end
function script_version() end
function localClock() return clock end
function isSampLoaded() return true end
function isSampfuncsLoaded() return true end
function isSampAvailable() return true end
function sampRegisterChatCommand(name, fn) commands[name] = fn end
function sampAddChatMessage(text, color) chat[#chat + 1] = text end
function wait() coroutine.yield() end
function thisScript() return 'trok-model' end

function requestModel(id) requested[id] = true end
function loadAllModelsNow()
    calls.loadAll = calls.loadAll + 1
    for id in pairs(requested) do
        if models[id] and models[id].loads then loaded[id] = true end
    end
end
function hasModelLoaded(id) return loaded[id] == true end
function markModelAsNoLongerNeeded(id) calls.noLonger[id] = true end
function getModelDimensions(id)
    assert(loaded[id], 'getModelDimensions antes de carregar ' .. id)
    local up = table.unpack or unpack
    return up(models[id].dims)
end
function getCharCoordinates(ped) assert(ped == PLAYER_PED) return player.x, player.y, player.z end
function getCharHeading(ped) assert(ped == PLAYER_PED) return player.heading end

local function new(kind, id, x, y, z, extra)
    assert(loaded[id], kind .. ' criado sem o modelo carregado: ' .. id)
    local h = nextHandle
    nextHandle = nextHandle + 1
    entities[h] = { kind = kind, model = id, x = x, y = y, z = z, heading = 0, collision = true, frozen = false }
    if extra then for k, v in pairs(extra) do entities[h][k] = v end end
    return h
end
local function ent(h, kind)
    local e = entities[h]
    assert(e and e.kind == kind, 'handle invalido: ' .. tostring(h))
    return e
end

function createObject(id, x, y, z) return new('objeto', id, x, y, z) end
function setObjectCollision(h, v) ent(h, 'objeto').collision = v end
function freezeObjectPosition(h, v) ent(h, 'objeto').frozen = v end
function setObjectHeading(h, a) ent(h, 'objeto').heading = a end
function setObjectCoordinates(h, x, y, z) local e = ent(h, 'objeto') e.x, e.y, e.z = x, y, z end
function doesObjectExist(h) return entities[h] ~= nil and entities[h].kind == 'objeto' end
function deleteObject(h) ent(h, 'objeto') entities[h] = nil end

function createCar(id, x, y, z) return new('veiculo', id, x, y, z) end
function freezeCarPosition(h, v) ent(h, 'veiculo').frozen = v end
function setCarCollision(h, v) ent(h, 'veiculo').collision = v end
function setCarProofs(h, ...) ent(h, 'veiculo').proofs = { ... } end
function lockCarDoors(h, s) ent(h, 'veiculo').locked = s end
function setCarHeading(h, a) ent(h, 'veiculo').heading = a end
function doesVehicleExist(h) return entities[h] ~= nil and entities[h].kind == 'veiculo' end
function deleteCar(h) ent(h, 'veiculo') entities[h] = nil end

function createChar(pedtype, id, x, y, z) return new('skin', id, x, y, z, { pedtype = pedtype }) end
function freezeCharPosition(h, v) ent(h, 'skin').frozen = v end
function setCharCollision(h, v) ent(h, 'skin').collision = v end
function setCharProofs(h, ...) ent(h, 'skin').proofs = { ... } end
function clearCharTasksImmediately(h) ent(h, 'skin').cleared = true end
function setCharHeading(h, a) ent(h, 'skin').heading = a end
function doesCharExist(h) return entities[h] ~= nil and entities[h].kind == 'skin' end
function deleteChar(h) ent(h, 'skin') entities[h] = nil end

-- ---------------------------------------------------------------- roda o script

dofile(SCRIPT)
local co = coroutine.create(main)
local function frame(dt)
    clock = clock + (dt or 0.016)
    local ok, err = coroutine.resume(co)
    if not ok then error('o main do script parou: ' .. tostring(err)) end
    check_alive = coroutine.status(co) ~= 'dead'
end
local function frames(n, dt) for _ = 1, n do frame(dt) end end
local function count()
    local n, last = 0, nil
    for h, e in pairs(entities) do n = n + 1 last = e end
    return n, last
end
local function lastChat() return chat[#chat] or '' end
local function cmd(text)
    commands.model(text)
    frames(3)
end

frames(2)
check(type(commands.model) == 'function', '/model registrado')

cmd('')
check(lastChat():find('use /model <id>', 1, true), '/model sozinho, sem nada criado: mostra o uso')
cmd('abc')
check(lastChat():find('numero do modelo', 1, true), '/model abc: pede um numero')
cmd('99999')
check(lastChat():find('de 0 a 19999', 1, true), '/model 99999: fora da faixa')
cmd('5000')
check(lastChat():find('5000 nao existe', 1, true) and count() == 0, '/model 5000: modelo que nao existe, nada criado')

-- Objeto do gta3.img: personagem em (100, 200, 11) olhando para o oeste (90 graus).
cmd('1337')
local n, e = count()
local dist = math.max(2.0, math.sqrt(0.5) + 1.0)
check(n == 1 and e.kind == 'objeto' and e.model == 1337, '/model 1337: um objeto criado')
check(near(e.x, 100 - dist, 0.01) and near(e.y, 200, 0.01), 'na frente do personagem (para onde ele olha)')
check(near(e.z, 11 - 1.0 + 0.4 + 0.5, 0.1), 'a parte de baixo ~0,4 m acima do pe')
check(e.collision == false and e.frozen == true, 'sem colisao e parado no ar')
check(near(e.heading, 270, 3), 'de frente para o personagem (girando so 36 graus/s desde que nasceu)')
check(lastChat():find('1337: objeto do gta3.img', 1, true), 'chat: "1337: objeto do gta3.img"')
check(calls.noLonger[1337], 'o modelo e liberado para o streaming depois de criado')
local z0, h0 = e.z, e.heading
local minZ, maxZ = 1e9, -1e9
for _ = 1, 120 do
    frame(0.016)
    minZ, maxZ = math.min(minZ, e.z), math.max(maxZ, e.z)
end
check(not near(e.heading, h0, 1) and near((e.heading - h0) % 360, 36 * (120 * 0.016 + 0.048), 2),
    'gira devagar (36 graus por segundo)')
check(maxZ - minZ > 0.05 and maxZ - minZ <= 0.17, 'sobe e desce de leve (flutuando)')

-- gta_int.img substitui o anterior; o maior fica mais longe.
cmd('14444')
n, e = count()
check(n == 1 and e.model == 14444, '/model 14444: o anterior sai, o novo entra')
check(near(e.x, 100 - (math.sqrt(10) + 1.0), 0.01), 'modelo maior nasce mais longe (inteiro na frente)')
check(lastChat():find('do gta_int.img', 1, true), 'chat: do gta_int.img')

cmd('19475')
check(lastChat():find('19475: objeto do samp.img', 1, true), 'samp.img reconhecido')

cmd('3000')
check(lastChat():find('3000: objeto.', 1, true), '.img desconhecido: so o tipo, sem nome')
cmd('3001')
n, e = count()
check(n == 1 and e.model == 3001 and near(e.x, 100 - 2.0, 0.01), 'modelo sem medidas: nasce a 2 m, sem erro')

-- Veiculo
cmd('411')
n, e = count()
check(n == 1 and e.kind == 'veiculo' and e.model == 411, '/model 411: veiculo criado')
check(e.frozen and e.collision == false and e.locked == 2, 'veiculo parado, sem colisao e trancado')
check(e.proofs and #e.proofs == 5 and e.proofs[1] == true, 'veiculo a prova de tudo')
frames(30)
check(not near(e.heading, 270, 1), 'veiculo gira')

cmd('537')
check(lastChat():find('trem', 1, true) and count() == 1, '/model 537: trem recusado, o veiculo anterior continua')
cmd('0')
check(lastChat():find('CJ', 1, true), '/model 0: o CJ recusado')

-- Skin
cmd('46')
n, e = count()
check(n == 1 and e.kind == 'skin' and e.pedtype == 4 and e.frozen and e.collision == false and e.cleared,
    '/model 46: skin criada, parada, sem colisao e sem tarefas')
check(near(e.z, 11 - 1.0 + 0.4 + 1.0, 0.01), 'skin com o pe ~0,4 m acima do seu')

-- Modelo que nao carrega: desiste em 3 s, sem criar nada e sem travar o script.
local before = count()
commands.model('2000')
frames(250, 0.016)
check(lastChat():find('2000 nao carregou', 1, true), '/model 2000: desiste depois de 3 s')
check(count() == before, 'nada novo criado quando o modelo nao carrega')

-- Apagar
cmd('')
check(count() == 0 and lastChat():find('apagado', 1, true), '/model sozinho: apaga')

-- O jogo apagou o objeto por conta propria (interior, reconexao): o script so esquece.
cmd('1337')
for h in pairs(entities) do entities[h] = nil end
frames(5)
cmd('')
check(lastChat():find('use /model', 1, true), 'objeto sumido: o script esquece sem erro')

-- Limit adjuster: a tabela de modelos mudou de lugar.
infoBase = 0x3000000
mem32[INFO_REF] = infoBase
for id in pairs(models) do mem32[INFO + id * 4] = nil end
syncInfoPtrs()
cmd('1337')
check(count() == 1, 'acha os modelos pela referencia do jogo (limit adjuster)')

-- Fim do script: apaga o que criou.
onScriptTerminate('outro-script')
check(count() == 1, 'outro script terminando nao mexe em nada')
onScriptTerminate(thisScript())
check(count() == 0, 'o script terminando apaga o modelo')

print(string.format('%s (%d verificacoes, %d falhas)', failures == 0 and 'PASSOU' or 'FALHOU', checks, failures))
os.exit(failures == 0 and 0 or 1)
