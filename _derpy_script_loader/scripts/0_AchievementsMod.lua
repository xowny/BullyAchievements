RequireLoaderVersion(9)

local function TryLoadAchievements()
    if type(_G.ACH_RUNTIME_SCRIPT_ACTIVE) == "boolean" and _G.ACH_RUNTIME_SCRIPT_ACTIVE then
        return true
    end
    if type(_G.ACH_RUNTIME_SCRIPT_STARTING) == "boolean" and _G.ACH_RUNTIME_SCRIPT_STARTING then
        return true
    end

    local player = gPlayer
    local loading = false
    local areaIsLoading = rawget(_G, "AreaIsLoading")
    if type(areaIsLoading) == "function" then
        local ok, isLoading = pcall(areaIsLoading)
        if ok and isLoading then
            loading = true
        end
    end

    if type(player) == "number" and player > -1 and not loading and type(StartScript) == "function" then
        _G.ACH_RUNTIME_SCRIPT_STARTING = true
        local ok, started = pcall(StartScript, "AchievementsMod/runtime.lua")
        if ok and started ~= nil then
            return true
        end
        _G.ACH_RUNTIME_SCRIPT_ACTIVE = false
        _G.ACH_RUNTIME_SCRIPT_STARTING = false
    end

    return false
end

CreateThread(function()
    while true do
        TryLoadAchievements()
        Wait(500)
    end
end)
