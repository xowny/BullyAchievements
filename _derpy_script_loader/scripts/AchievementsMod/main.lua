local MOD_NAME = "AchievementsMod"
local MOD_VERSION = "asi-bridge-1"
local LOAD_CONTEXT = type(_G) == "table" and _G.ACHIEVEMENTS_CONTEXT or nil

local CONFIG = {
    telemetryPollMs = 1000,
    statePollMs = 500,
    popupPollMs = 150,
    popupDurationMs = 4000,
    popupFadeMs = 250,
    runtimeWarmupMs = 3000,
    gamePath = ""
}

local UI_SHARED = GetPersistentDataTable("AchievementsModUI")
local POPUP_SHARED = GetPersistentDataTable("AchievementsModPopup")

local CURRENT_POPUP = nil
local POPUP_TEXTURES = { box = nil, icon = nil, iconName = "", iconAR = 1, boxAR = 1 }
local LAST_TELEMETRY = 0
local LAST_STATE = 0
local LAST_POPUP = 0
local LAST_ERRANDS_SCAN = 0
local CACHED_ERRANDS = 0
local ERRAND_BASE_GETTER = nil
local RUNTIME_READY_SINCE = 0
local THREAD_STARTED = false
local LAST_STATE_UPDATED = -1
local LAST_POPUP_NONCE = 0
local DATA_ROOT = nil
local RUNTIME_DIR = "_derpy_script_loader/scripts/AchievementsMod/Runtime"
local LATCH_SODA_ACTIVE = false
local LATCH_KISS_ACTIVE = false
local LATCH_KISS_TARGET_FEMALE = false
local LATCH_ON_SKATE = false
local LATCH_SWIRLIE_ACTIVE = false
local IsRuntimeReady

local MISSIONS = {
    "1_01","1_02B","1_03","1_04","1_05","1_06","1_07","1_08","1_09","1_10","1_11","1_11B",
    "1_B","1_G1","1_S01","2_01","2_03","2_04","2_05","2_06","2_07","2_08","2_09","2_B",
    "2_G2","2_S04","2_S06","3_B","3_G3","3_R09","3_R08_Rich7","3_R08_Business4","3_R08_Poor2",
    "3_R08_School1","4_01","4_B2","5_01","5_B","6_B","C_Art_5","C_Biology_5","C_Chem_5",
    "C_English_5","C_Geography_5","C_Math_5","C_Music_5","C_Photography_5","C_Shop_5",
    "C_Wrestling_5","GoKart_GP5"
}

local CLASSES = {
    "C_Art_1","C_Wrestling_1","C_Photography_1","C_Shop_1","C_Math_1",
    "C_Biology_1","C_Geography_1","C_Music_1","C_Chem_1","C_English_1"
}

local LAWN_MISSIONS = {
    "LawnMowing1a","LawnMowing1b","LawnMowing1c",
    "LawnMowing2a","LawnMowing2b","LawnMowing2c",
    "LawnMowing3a","LawnMowing3b","LawnMowing3c",
    "JobLawnMowing1a","JobLawnMowing1b","JobLawnMowing1c",
    "JobLawnMowing2a","JobLawnMowing2b","JobLawnMowing2c"
}

local function SafeCall(fnName, a, b, c, d, e, f, g, h)
    local fn = _G[fnName]
    if type(fn) ~= "function" then
        return nil, false
    end
    local ok, r1, r2, r3 = pcall(fn, a, b, c, d, e, f, g, h)
    if not ok then
        return nil, false
    end
    return r1, true, r2, r3
end

local function SafeBool(fnName, a, b, c, d, e, f, g, h)
    local r1, ok = SafeCall(fnName, a, b, c, d, e, f, g, h)
    if not ok then
        return false
    end
    if type(r1) == "boolean" then
        return r1
    end
    if type(r1) == "number" then
        return r1 ~= 0
    end
    if type(r1) == "string" then
        return r1 ~= "" and r1 ~= "0" and r1 ~= "false"
    end
    return r1 ~= nil
end

local function SafeNum(fnName, defaultValue, a, b, c, d, e, f, g, h)
    local r1, ok = SafeCall(fnName, a, b, c, d, e, f, g, h)
    if not ok or type(r1) ~= "number" then
        return defaultValue
    end
    return r1
end

local function TimerMs()
    if type(GetSystemTimer) == "function" then
        return GetSystemTimer()
    end
    return 0
end

local function LoadConfig()
    local path = GetScriptFilePath() .. "AchievementsMod.ini"
    local file = io.open(path, "r")
    if not file then
        return
    end
    for line in file:lines() do
        line = string.gsub(line, ";.*$", "")
        line = string.gsub(line, "#.*$", "")
        local _, _, key, value = string.find(line, "^%s*([^=]+)%s*=%s*(.-)%s*$")
        if key and value and CONFIG[key] ~= nil then
            if value == "true" then
                CONFIG[key] = true
            elseif value == "false" then
                CONFIG[key] = false
            else
                local num = tonumber(value)
                CONFIG[key] = num ~= nil and num or value
            end
        end
    end
    file:close()
end

local function SetupProxyFunctions()
    if type(GetBaseGameFunction) == "function" and type(ERRAND_BASE_GETTER) ~= "function" then
        local ok, baseFn = pcall(GetBaseGameFunction, "MinigameGetErrandCompletion")
        if ok and type(baseFn) == "function" then
            ERRAND_BASE_GETTER = baseFn
        end
    end
    -- Do not proxy MinigameGetErrandCompletion through this mod. Scenarios.lua
    -- calls it directly for ambient errand gating, and routing ownership through
    -- AchievementsMod can suppress errands instead of just observing completion.
end

local function GetCurrentAreaProxyScripts()
    local area = SafeNum("AreaGetVisible", -1)
    local scripts = {}
    local function add(name)
        local i
        if type(name) ~= "string" or name == "" then
            return
        end
        for i = 1, table.getn(scripts) do
            if scripts[i] == name then
                return
            end
        end
        table.insert(scripts, name)
    end

    if area == 0 then
        add("MainMap.lua")
    elseif area == 14 then
        add("Bdorm.lua")
    elseif area == 35 then
        add("Gdorm.lua")
    elseif area == 2 then
        add("SchoolHallways.lua")
    elseif area == 13 then
        add("GymAndPool.lua")
        add("GymAndBarber.lua")
    elseif area == 32 then
        add("PrepHouse.lua")
    elseif area == 36 then
        add("Tenements.lua")
    elseif area == 38 then
        add("Asylum.lua")
    elseif area == 45 then
        add("Midway.lua")
    end

    add("POI/Events.lua")
    add("Events.lua")
    add("main.lua")
    return scripts
end

local function NormalizePath(path)
    if type(path) ~= "string" then
        return nil
    end
    path = string.gsub(path, "\\", "/")
    path = string.gsub(path, "/+", "/")
    return path
end

local function FileExists(path)
    local file = io.open(path, "rb")
    if file then
        file:close()
        return true
    end
    return false
end

local function ParentPath(path)
    if type(path) ~= "string" then
        return nil
    end
    local _, _, parent = string.find(path, "^(.*)/[^/]+/?$")
    return parent
end

local function GetScriptDir()
    local path = NormalizePath(GetScriptFilePath())
    if type(path) ~= "string" or path == "" then
        return "."
    end
    if string.find(path, "%.[^/]+$") then
        local parent = ParentPath(path)
        if type(parent) == "string" and parent ~= "" then
            return parent
        end
    end
    return path
end

local function GetGameRoot()
    if type(DATA_ROOT) == "string" and DATA_ROOT ~= "" then
        return DATA_ROOT
    end
    if type(CONFIG.gamePath) == "string" and CONFIG.gamePath ~= "" then
        DATA_ROOT = NormalizePath(CONFIG.gamePath)
        return DATA_ROOT
    end
    local path = NormalizePath(GetScriptFilePath())
    local i = 0
    while type(path) == "string" and i < 8 do
        if FileExists(path .. "/Bully.exe") then
            DATA_ROOT = path
            return DATA_ROOT
        end
        path = ParentPath(path)
        i = i + 1
    end
    DATA_ROOT = NormalizePath(GetScriptFilePath())
    return DATA_ROOT
end

local function DataPath(fileName)
    return RUNTIME_DIR .. "/" .. fileName
end

local function ParseIni(path)
    local data = {}
    local file = io.open(path, "r")
    if not file then
        return data
    end
    local section = "default"
    data[section] = {}
    for line in file:lines() do
        line = string.gsub(line, "[\r\n]+$", "")
        line = string.gsub(line, "^%s+", "")
        line = string.gsub(line, "%s+$", "")
        if line ~= "" and not string.find(line, "^;") and not string.find(line, "^#") then
            local _, _, name = string.find(line, "^%[([^%]]+)%]$")
            if name then
                section = name
                if type(data[section]) ~= "table" then
                    data[section] = {}
                end
            else
                local _, _, key, value = string.find(line, "^([^=]+)=(.*)$")
                if key and value then
                    key = string.gsub(key, "%s+$", "")
                    value = string.gsub(value, "^%s+", "")
                    data[section][key] = value
                end
            end
        end
    end
    file:close()
    return data
end

local function IniInt(data, section, key, defaultValue)
    local value = data[section] and data[section][key]
    local num = tonumber(value)
    return num ~= nil and num or defaultValue
end

local function IniBool(data, section, key, defaultValue)
    local value = data[section] and data[section][key]
    if value == "1" or value == "true" then
        return true
    end
    if value == "0" or value == "false" then
        return false
    end
    return defaultValue
end

local function WriteTextAtomic(path, text)
    local file = io.open(path, "wb")
    if not file then
        return false
    end
    file:write(text)
    file:close()
    return true
end

local function IntString(value)
    local num = tonumber(value)
    if num == nil then
        num = 0
    end
    return string.format("%.0f", num)
end

local function CountCompletedErrands(maxEnum)
    local now = TimerMs()
    if now - LAST_ERRANDS_SCAN < 2000 then
        return CACHED_ERRANDS
    end

    local function GetErrandCompletion(enum)
        local best = -1
        if type(ERRAND_BASE_GETTER) == "function" then
            local ok, value = pcall(ERRAND_BASE_GETTER, enum)
            if ok and type(value) == "number" then
                best = value
            end
        end
        if type(MinigameGetErrandCompletion) == "function" then
            local ok, value = pcall(MinigameGetErrandCompletion, enum)
            if ok and type(value) == "number" and value > best then
                best = value
            end
        end
        return best > 0 and best or 0
    end

    local count = 0
    local i = 0
    while i <= maxEnum do
        local value = GetErrandCompletion(i)
        if value >= 1 then
            count = count + 1
        end
        i = i + 1
    end
    LAST_ERRANDS_SCAN = now
    CACHED_ERRANDS = count
    return count
end

local function UpdateActionLatches()
    if not IsRuntimeReady() then
        LATCH_SODA_ACTIVE = false
        LATCH_KISS_ACTIVE = false
        LATCH_KISS_TARGET_FEMALE = false
        LATCH_ON_SKATE = false
        LATCH_SWIRLIE_ACTIVE = false
        return
    end

    if SafeBool("PedIsPlaying", gPlayer, "/Global/SodaMach", true) then
        LATCH_SODA_ACTIVE = true
    end

    if SafeBool("PedIsPlaying", gPlayer, "/Global/Vehicles/SkateBoard", true) then
        LATCH_ON_SKATE = true
    end

    if SafeBool("PedIsPlaying", gPlayer, "/Global/Ambient/Scripted/Swirlie", true)
        or SafeBool("PedIsPlaying", gPlayer, "/Global/Toilet/Toilet_Grab", true)
        or SafeBool("PedMePlaying", gPlayer, "Toilet_Grab")
        or SafeBool("PedMePlaying", gPlayer, "Swirlie") then
        LATCH_SWIRLIE_ACTIVE = true
    end

    if SafeBool("PedIsPlaying", gPlayer, "/Global/Player/Social_Actions/MakeOut/Makeout", true)
        or SafeBool("PedIsPlaying", gPlayer, "/Global/Player/Social_Actions/MakeOut/Makeout/GrappleAttempt/Kisses", true) then
        LATCH_KISS_ACTIVE = true
        if type(PedGetGrappleTargetPed) == "function" and type(PedIsValid) == "function" and type(PedIsFemale) == "function" then
            local target, ok = SafeCall("PedGetGrappleTargetPed", gPlayer)
            if ok and type(target) == "number" and target >= 0 and SafeBool("PedIsValid", target) then
                LATCH_KISS_TARGET_FEMALE = SafeBool("PedIsFemale", target)
            end
        end
    end
end

local function CountCompletedClasses()
    local count = 0
    local i
    for i = 1, table.getn(CLASSES) do
        if SafeBool("IsMissionCompleated", CLASSES[i]) or SafeNum("GetMissionSuccessCount", 0, CLASSES[i]) >= 1 then
            count = count + 1
        end
    end
    return count
end

local function CountCompletedLawnMissions()
    local count = 0
    local i
    for i = 1, table.getn(LAWN_MISSIONS) do
        local mission = LAWN_MISSIONS[i]
        if SafeBool("IsMissionCompleated", mission) or SafeNum("GetMissionSuccessCount", 0, mission) >= 1 then
            count = count + 1
        end
    end
    return count
end

IsRuntimeReady = function()
    if type(gPlayer) ~= "number" or gPlayer < 0 then
        return false
    end
    if SafeBool("AreaIsLoading") then
        return false
    end
    return true
end

local function IsTelemetrySafe()
    local now = TimerMs()
    if not IsRuntimeReady() then
        RUNTIME_READY_SINCE = 0
        return false
    end
    if RUNTIME_READY_SINCE == 0 then
        RUNTIME_READY_SINCE = now
        return false
    end
    return (now - RUNTIME_READY_SINCE) >= CONFIG.runtimeWarmupMs
end

local function CollectTelemetryText()
    local hour, okClock, minute = SafeCall("ClockGet")
    if type(minute) ~= "number" then
        minute = 0
    end
    local runtimeSafe = IsTelemetrySafe()
    local bikeId = -1
    local flowersCurrent = 0
    local sodaActive = false
    local kissActive = false
    local kissTargetFemale = false
    local onSkate = false
    local swirlieActive = false
    local lawnCompletedCount = 0
    local lawnPercent = 0
    local miniObjective16 = false
    local hasPosition = false
    local posX = 0
    local posY = 0

    if runtimeSafe then
        bikeId = SafeNum("PlayerGetBikeId", -1)
        flowersCurrent = SafeNum("ItemGetCurrentNum", 0, 475)
        sodaActive = LATCH_SODA_ACTIVE
        kissActive = LATCH_KISS_ACTIVE
        kissTargetFemale = LATCH_KISS_TARGET_FEMALE
        onSkate = LATCH_ON_SKATE
        swirlieActive = LATCH_SWIRLIE_ACTIVE
        lawnCompletedCount = CountCompletedLawnMissions()
        lawnPercent = lawnCompletedCount > 0 and 100 or 0
        miniObjective16 = SafeBool("MiniObjectiveGetIsComplete", 16)

        local px, okPos, py = SafeCall("PlayerGetPosXYZ")
        if okPos and type(px) == "number" and type(py) == "number" then
            hasPosition = true
            posX = px
            posY = py
        end
    end

    local out = {}
    local function add(line)
        table.insert(out, line)
    end

    add("[meta]")
    add("updated=" .. IntString(TimerMs()))
    add("")
    add("[state]")
    add("runtimeReady=" .. (runtimeSafe and "1" or "0"))
    add("missionActive=" .. (SafeBool("MissionActive") and "1" or "0"))
    add("cutsceneActive=" .. (SafeBool("GetCutsceneRunning") and "1" or "0"))
    add("gameCompletion=" .. tostring(SafeNum("StatGetGameCompletion", 0)))
    add("stat16=" .. tostring(SafeNum("StatGetAsInt", 0, 16)))
    add("pdiff16=" .. tostring(SafeNum("StatGetPrincipalDiffAsInt", 0, 16)))
    add("stat28=" .. tostring(SafeNum("StatGetAsInt", 0, 28)))
    add("stat29=" .. tostring(SafeNum("StatGetAsInt", 0, 29)))
    add("stat30=" .. tostring(SafeNum("StatGetAsInt", 0, 30)))
    add("stat31=" .. tostring(SafeNum("StatGetAsInt", 0, 31)))
    add("stat32=" .. tostring(SafeNum("StatGetAsInt", 0, 32)))
    add("stat33=" .. tostring(SafeNum("StatGetAsInt", 0, 33)))
    add("stat34=" .. tostring(SafeNum("StatGetAsInt", 0, 34)))
    add("stat35=" .. tostring(SafeNum("StatGetAsInt", 0, 35)))
    add("pdiff28=" .. tostring(SafeNum("StatGetPrincipalDiffAsInt", 0, 28)))
    add("pdiff29=" .. tostring(SafeNum("StatGetPrincipalDiffAsInt", 0, 29)))
    add("pdiff30=" .. tostring(SafeNum("StatGetPrincipalDiffAsInt", 0, 30)))
    add("pdiff31=" .. tostring(SafeNum("StatGetPrincipalDiffAsInt", 0, 31)))
    add("pdiff32=" .. tostring(SafeNum("StatGetPrincipalDiffAsInt", 0, 32)))
    add("pdiff33=" .. tostring(SafeNum("StatGetPrincipalDiffAsInt", 0, 33)))
    add("pdiff34=" .. tostring(SafeNum("StatGetPrincipalDiffAsInt", 0, 34)))
    add("pdiff35=" .. tostring(SafeNum("StatGetPrincipalDiffAsInt", 0, 35)))
    add("stat66=" .. tostring(SafeNum("StatGetAsInt", 0, 66)))
    add("stat238=" .. tostring(SafeNum("StatGetAsInt", 0, 238)))
    add("roomTrophies=" .. tostring(SafeNum("StatGetAsInt", 0, 1)))
    add("money=" .. tostring(SafeNum("PlayerGetMoney", 0)))
    add("punishmentPoints=" .. tostring(SafeNum("PlayerGetPunishmentPoints", 0)))
    add("bikeId=" .. tostring(bikeId))
    add("flowersCurrent=" .. tostring(flowersCurrent))
    add("lawnCompletedCount=" .. tostring(lawnCompletedCount))
    add("lawnPercent=" .. tostring(lawnPercent))
    add("classesCompleted=" .. tostring(CountCompletedClasses()))
    add("errands=" .. tostring(CountCompletedErrands(49)))
    add("clockMinutes=" .. tostring(okClock and hour * 60 + minute or -1))
    add("curfewActive=" .. ((okClock and (hour >= 23 or hour < 7)) and "1" or "0"))
    add("yearbookFull=" .. (SafeBool("YearbookIsFull") and "1" or "0"))
    add("miniObjective16=" .. (miniObjective16 and "1" or "0"))
    add("swirlieActive=" .. (swirlieActive and "1" or "0"))
    add("sodaActive=" .. (sodaActive and "1" or "0"))
    add("kissActive=" .. (kissActive and "1" or "0"))
    add("kissTargetFemale=" .. (kissTargetFemale and "1" or "0"))
    add("onSkate=" .. (onSkate and "1" or "0"))
    add("saved11=" .. tostring(SafeNum("PlayerGetScriptSavedData", 0, 11)))
    add("saved12=" .. tostring(SafeNum("PlayerGetScriptSavedData", 0, 12)))
    add("hasPosition=" .. (hasPosition and "1" or "0"))
    add("posX=" .. tostring(posX))
    add("posY=" .. tostring(posY))
    add("")
    add("[minigame]")
    local i
    for i = 0, 4 do
        add("score" .. tostring(i) .. "=0")
    end
    add("")
    add("[collectibles]")
    for i = 0, 5 do
        add("collectable" .. tostring(i) .. "=" .. tostring(SafeNum("CollectiblesGetNumCollectable", 0, i)))
        add("collected" .. tostring(i) .. "=" .. tostring(SafeNum("CollectiblesGetNumCollected", 0, i)))
    end
    add("")
    add("[factions]")
    for i = 1, 5 do
        add("clique" .. tostring(i) .. "=" .. tostring(SafeNum("GetFactionRespect", 0, i)))
    end
    add("")
    add("[missions]")
    for i = 1, table.getn(MISSIONS) do
        local mission = MISSIONS[i]
        local done = SafeBool("IsMissionCompleated", mission) or SafeNum("GetMissionSuccessCount", 0, mission) >= 1
        add(mission .. "=" .. (done and "1" or "0"))
    end
    add("")
    add("[success]")
    add("GoKart_GP5=" .. tostring(SafeNum("GetMissionSuccessCount", 0, "GoKart_GP5")))
    add("")
    if runtimeSafe then
        LATCH_SODA_ACTIVE = false
        LATCH_KISS_ACTIVE = false
        LATCH_KISS_TARGET_FEMALE = false
        LATCH_ON_SKATE = false
        LATCH_SWIRLIE_ACTIVE = false
    end
    return table.concat(out, "\n")
end

local function RefreshStateFromFile()
    local data = ParseIni(DataPath("state.ini"))
    local updated = IniInt(data, "summary", "updated", 0)
    if updated <= 0 or updated == LAST_STATE_UPDATED then
        return
    end
    LAST_STATE_UPDATED = updated
    UI_SHARED.version = data.summary and data.summary.version or MOD_VERSION
    UI_SHARED.total = IniInt(data, "summary", "total", 0)
    UI_SHARED.unlocked = IniInt(data, "summary", "unlocked", 0)
    UI_SHARED.totalSingle = IniInt(data, "summary", "totalSingle", 0)
    UI_SHARED.unlockedSingle = IniInt(data, "summary", "unlockedSingle", 0)
    UI_SHARED.items = {}
    local i = 0
    while data["item" .. tostring(i)] do
        local section = data["item" .. tostring(i)]
        local hidden = IniBool(data, "item" .. tostring(i), "hidden", false)
        UI_SHARED.items[i + 1] = {
            id = section.id or "",
            title = hidden and "Secret Achievement" or (section.title or ("Achievement " .. tostring(i + 1))),
            description = hidden and "Unlock to reveal." or (section.description or ""),
            icon = section.icon or ("ach_icon_" .. tostring(i)),
            unlocked = IniBool(data, "item" .. tostring(i), "unlocked", false),
            secret = hidden and true or false,
            status = section.status or "Locked",
            rarityText = section.rarityLabel and section.rarityPercent and (section.rarityPercent .. "% " .. section.rarityLabel) or "",
            raritySource = section.raritySource or "",
            mode = section.mode or "single",
            verified = IniBool(data, "item" .. tostring(i), "verified", true)
        }
        i = i + 1
    end
end

local function LoadTextureSafe(path)
    local ok, tex = pcall(CreateTexture, path)
    if ok and tex ~= nil then
        local ar = 1
        local ok2, gotAr = pcall(GetTextureDisplayAspectRatio, tex)
        if ok2 and type(gotAr) == "number" and gotAr > 0 then
            ar = gotAr
        end
        return tex, ar
    end
    return nil, 1
end

local function EnsurePopupTextures(iconName)
    if POPUP_TEXTURES.box == nil then
        POPUP_TEXTURES.box, POPUP_TEXTURES.boxAR = LoadTextureSafe("MainMenu/Graphics/Base/Ebox.png")
        if POPUP_TEXTURES.box == nil then
            POPUP_TEXTURES.box, POPUP_TEXTURES.boxAR = LoadTextureSafe("Graphics/Base/Ebox.png")
        end
    end
    if POPUP_TEXTURES.iconName == iconName and POPUP_TEXTURES.icon ~= nil then
        return
    end
    POPUP_TEXTURES.iconName = iconName or ""
    POPUP_TEXTURES.icon = nil
    POPUP_TEXTURES.iconAR = 1
    if type(iconName) == "string" and iconName ~= "" then
        POPUP_TEXTURES.icon, POPUP_TEXTURES.iconAR = LoadTextureSafe("AchievementsMod/Graphics/Achievements/" .. iconName .. ".png")
        if POPUP_TEXTURES.icon == nil then
            POPUP_TEXTURES.icon, POPUP_TEXTURES.iconAR = LoadTextureSafe("MainMenu/Graphics/Achievements/" .. iconName .. ".png")
        end
    end
end

local function RefreshPopupFromFile()
    local data = ParseIni(DataPath("popup.ini"))
    local isActive = IniBool(data, "popup", "active", false)
    if not isActive then
        CURRENT_POPUP = nil
        POPUP_SHARED.active = false
    end
    local nonce = IniInt(data, "popup", "nonce", 0)
    if nonce <= 0 or nonce == LAST_POPUP_NONCE then
        return
    end
    LAST_POPUP_NONCE = nonce
    if not isActive then
        CURRENT_POPUP = nil
        POPUP_SHARED.active = false
        POPUP_SHARED.start = nil
        POPUP_SHARED.duration = nil
        POPUP_SHARED.fadeMs = nil
        return
    end
    CURRENT_POPUP = {
        title = data.popup.title or "Achievement",
        icon = data.popup.icon or "ach_icon_0",
        start = TimerMs(),
        durationMs = IniInt(data, "popup", "durationMs", CONFIG.popupDurationMs),
        fadeMs = IniInt(data, "popup", "fadeMs", CONFIG.popupFadeMs)
    }
    POPUP_SHARED.active = true
    POPUP_SHARED.title = CURRENT_POPUP.title
    POPUP_SHARED.icon = CURRENT_POPUP.icon
    POPUP_SHARED.start = CURRENT_POPUP.start
    POPUP_SHARED.duration = CURRENT_POPUP.durationMs
    POPUP_SHARED.fadeMs = CURRENT_POPUP.fadeMs
end

local function RenderPopup()
    if CURRENT_POPUP == nil then
        return
    end
    local now = TimerMs()
    local elapsed = now - CURRENT_POPUP.start
    if elapsed >= CURRENT_POPUP.durationMs then
        CURRENT_POPUP = nil
        POPUP_SHARED.active = false
        POPUP_SHARED.start = nil
        POPUP_SHARED.duration = nil
        POPUP_SHARED.fadeMs = nil
        return
    end
    local alpha = 1
    if elapsed > CURRENT_POPUP.durationMs - CURRENT_POPUP.fadeMs then
        alpha = math.max(0, (CURRENT_POPUP.durationMs - elapsed) / CURRENT_POPUP.fadeMs)
    end
    EnsurePopupTextures(CURRENT_POPUP.icon)

    local boxA = math.floor(200 * alpha)
    local iconA = math.floor(255 * alpha)
    local ar = SafeNum("GetDisplayAspectRatio", 1)
    if ar <= 0 then
        ar = 1
    end
    local h = 0.10
    local marginX = 0.02 + (0.02 / ar)
    local marginY = 0.06
    local cy = marginY + (h / 2)
    local iconH = h * 0.82
    local iconW = iconH * (POPUP_TEXTURES.iconAR or 1)
    local font = SafeCall("GetPreference", "Font1")
    if type(font) ~= "string" or font == "" then
        font = "Font1"
    end
    local headerText = "Achievement Unlocked"
    local titleText = CURRENT_POPUP.title or "Achievement"
    local headerW = MeasureTextInline("~scale+font~" .. headerText, 0.7, font, 0, 3) or 0
    local titleW = MeasureTextInline("~scale+font~" .. titleText, 0.8, font, 0, 3) or 0
    if headerW > 1 or titleW > 1 then
        local wpx = SafeNum("GetDisplayResolution", 0)
        if wpx > 1 then
            headerW = headerW / wpx
            titleW = titleW / wpx
        end
    end
    local textW = math.max(headerW, titleW)
    local pad = 0.02
    local gap = 0.02
    local w = math.max(0.28, iconW + gap + textW + pad * 2)
    local rightEdge = math.min(1 - marginX, 0.84 + (0.02 / ar))
    local leftEdge = rightEdge - w
    if leftEdge < marginX then
        leftEdge = marginX
        w = rightEdge - leftEdge
        if w < 0.20 then
            w = 0.20
            leftEdge = rightEdge - w
        end
    end
    local cx = leftEdge + w / 2
    local iconX = leftEdge + pad + iconW / 2
    local textX = leftEdge + pad + iconW + gap

    if POPUP_TEXTURES.box ~= nil then
        DrawTexture2(POPUP_TEXTURES.box, cx, cy, w, h, 0, 25, 25, 25, boxA)
    else
        DrawRectangle(cx, cy, w, h, 18, 18, 18, boxA)
    end
    if POPUP_TEXTURES.icon ~= nil then
        DrawTexture2(POPUP_TEXTURES.icon, iconX, cy, iconW, iconH, 0, 255, 255, 255, iconA)
    end
    DrawTextInline("~xy+scale+font+white~" .. headerText, textX, cy - h * 0.22, 0.7, font, 0, 3)
    DrawTextInline("~xy+scale+font+white~" .. titleText, textX, cy + h * 0.02, 0.8, font, 0, 3)
end

local function ACH_RefreshUI()
    RefreshStateFromFile()
end

function ACH_Main()
    while true do
        Wait(0)
        local now = TimerMs()
        UpdateActionLatches()
        if now - LAST_STATE >= CONFIG.statePollMs then
            LAST_STATE = now
            RefreshStateFromFile()
        end
        if now - LAST_POPUP >= CONFIG.popupPollMs then
            LAST_POPUP = now
            RefreshPopupFromFile()
        end
        local telemetrySafe = IsTelemetrySafe()
        if telemetrySafe and now - LAST_TELEMETRY >= CONFIG.telemetryPollMs then
            LAST_TELEMETRY = now
            WriteTextAtomic(DataPath("telemetry.ini"), CollectTelemetryText())
        end
        RenderPopup()
    end
end

function MissionCleanup()
    CURRENT_POPUP = nil
    POPUP_SHARED.active = false
    THREAD_STARTED = false
    if type(_G) == "table" then
        _G.ACH_RUNTIME_THREAD_STARTED = false
        _G.ACH_RUNTIME_SCRIPT_ACTIVE = false
        _G.ACH_RUNTIME_SCRIPT_STARTING = false
    end
end

LoadConfig()
if LOAD_CONTEXT == "runtime" then
    SetupProxyFunctions()
end
CURRENT_POPUP = nil
POPUP_SHARED.active = false
if type(_G) == "table" then
    _G.ACH_RefreshUI = ACH_RefreshUI
    if LOAD_CONTEXT == "runtime" then
        _G.ACH_StartRuntime = function()
            if not THREAD_STARTED then
                THREAD_STARTED = true
                _G.ACH_RUNTIME_THREAD_STARTED = true
                CreateThread(ACH_Main)
                return true
            end
            return false
        end
    end
end

ACH_RefreshUI()
if LOAD_CONTEXT == "runtime" and IsRuntimeReady() and not THREAD_STARTED then
    THREAD_STARTED = true
    if type(_G) == "table" then
        _G.ACH_RUNTIME_THREAD_STARTED = true
    end
    CreateThread(ACH_Main)
end
