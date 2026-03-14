RequireLoaderVersion(9)

local loaded = false

local function LoadRuntimeBridge()
    if loaded then
        return
    end
    if type(_G) == "table" then
        _G.ACHIEVEMENTS_CONTEXT = "runtime"
    end
    LoadScript("AchievementsMod/main.lua")
    if type(_G) == "table" then
        _G.ACHIEVEMENTS_CONTEXT = nil
    end
    loaded = true
end

function main()
    LoadRuntimeBridge()
    if type(_G) == "table" then
        _G.ACH_RUNTIME_SCRIPT_ACTIVE = true
        _G.ACH_RUNTIME_SCRIPT_STARTING = false
    end
    if type(_G.ACH_StartRuntime) == "function" then
        _G.ACH_StartRuntime()
    end
    while true do
        Wait(1000)
    end
end

function MissionCleanup()
    if type(_G) == "table" then
        _G.ACH_RUNTIME_SCRIPT_ACTIVE = false
        _G.ACH_RUNTIME_SCRIPT_STARTING = false
        _G.ACH_RUNTIME_THREAD_STARTED = false
    end
end
