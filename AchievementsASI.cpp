#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <iomanip>
#include <map>
#include <mmsystem.h>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

static_assert(sizeof(void*) == 4, "Achievements ASI must be built as 32-bit (Win32).");
#pragma comment(lib, "winmm.lib")

namespace {

struct RuntimeConfig {
    bool enable = true;
    bool verboseLogging = false;
    bool popupDuringMission = true;
    bool popupMp3Enabled = true;
    int backendPollMs = 1000;
    int popupDurationMs = 4000;
    int popupGapMs = 600;
    int popupMp3Volume = 1000;
    char modulePath[MAX_PATH] = {0};
    char moduleDir[MAX_PATH] = {0};
    char achievementsDir[MAX_PATH] = {0};
    char dataDir[MAX_PATH] = {0};
    char iniPath[MAX_PATH] = {0};
    char logPath[MAX_PATH] = {0};
    char telemetryPath[MAX_PATH] = {0};
    char statePath[MAX_PATH] = {0};
    char popupPath[MAX_PATH] = {0};
    char savePath[MAX_PATH] = {0};
    char raritySnapshotPath[MAX_PATH] = {0};
    char popupMp3Path[MAX_PATH] = {0};
    HANDLE initThread = nullptr;
} g_cfg;

using SectionMap = std::map<std::string, std::map<std::string, std::string> >;

enum class RuleType {
    None,
    Threshold,
    BoolMetric,
    BoolPair,
    MissionDone,
    MissionOrSuccess,
    CollectibleAll,
    YearbookFull,
    BoyGenius,
    AllSingle,
    AllNonMulti,
    KissBoth,
    MiniObjective16,
    LawnPercent
};

struct AchievementDef {
    std::string id;
    std::string title;
    std::string description;
    std::string icon;
    std::string mode;
    bool secret = false;
    bool verified = true;
    RuleType rule = RuleType::None;
    std::string key1;
    std::string key2;
    int threshold = 0;
};

struct RarityInfo {
    double percent = -1.0;
    std::string source;
};

struct Telemetry {
    bool valid = false;
    int updated = 0;
    bool runtimeReady = false;
    bool missionActive = false;
    bool cutsceneActive = false;
    int gameCompletion = 0;
    int stat16 = 0;
    int pdiff16 = 0;
    int stat28 = 0;
    int stat29 = 0;
    int stat30 = 0;
    int stat31 = 0;
    int stat32 = 0;
    int stat33 = 0;
    int stat34 = 0;
    int stat35 = 0;
    int pdiff28 = 0;
    int pdiff29 = 0;
    int pdiff30 = 0;
    int pdiff31 = 0;
    int pdiff32 = 0;
    int pdiff33 = 0;
    int pdiff34 = 0;
    int pdiff35 = 0;
    int stat66 = 0;
    int stat238 = 0;
    int roomTrophies = 0;
    int money = 0;
    int punishmentPoints = 0;
    int bikeId = -1;
    int flowersCurrent = 0;
    int lawnCompletedCount = 0;
    int lawnPercent = 0;
    int classesCompleted = 0;
    int errands = 0;
    int clockMinutes = -1;
    bool curfewActive = false;
    bool yearbookFull = false;
    bool miniObjective16 = false;
    bool swirlieActive = false;
    bool sodaActive = false;
    bool kissActive = false;
    bool kissTargetFemale = false;
    bool onSkate = false;
    bool saved11 = false;
    bool saved12 = false;
    double posX = 0.0;
    double posY = 0.0;
    bool hasPosition = false;
    int minigameScore[5] = {0, 0, 0, 0, 0};
    int collectable[6] = {0, 0, 0, 0, 0, 0};
    int collected[6] = {0, 0, 0, 0, 0, 0};
    int faction[6] = {0, 0, 0, 0, 0, 0};
    std::unordered_map<std::string, int> missions;
    std::unordered_map<std::string, int> successCounts;
};

struct RuntimeState {
    std::vector<AchievementDef> defs;
    std::unordered_map<std::string, std::size_t> idToIndex;
    std::vector<bool> unlocked;
    bool initialSyncComplete = false;

    int lastTelemetryUpdated = -1;
    int popupNonce = 0;
    int lastCommandNonce = 0;
    DWORD popupStartTick = 0;
    DWORD popupNextAllowedTick = 0;
    int activePopupIndex = -1;
    std::deque<int> popupQueue;

    int troubleTotal = 0;
    int troubleLast = 0;
    int bikesJacked = 0;
    int lastBikeId = -1;
    bool lastOnBike = false;
    int sodasTotal = 0;
    bool lastSodaActive = false;
    int kissesGirl = 0;
    int kissesBoy = 0;
    bool lastKissActive = false;
    int curfewMinutes = 0;
    int lastClockMinutes = -1;
    int lawnJobsEarned = 0;
    int lastLawnCompletedCount = -1;
    bool lawnCompletionArmed = false;
    double footDistance = 0.0;
    double bikeDistance = 0.0;
    double skateDistance = 0.0;
    double lastPosX = 0.0;
    double lastPosY = 0.0;
    bool lastPosValid = false;
    int swirliesTotal = 0;
    int lastStat16 = 0;
    int lastPdiff16 = -1;
    bool lastSwirlieActive = false;
    int eggCarsTotal = 0;
    int lastStat28 = 0;
    int lastStat29 = 0;
    int lastStat30 = 0;
    int lastStat31Observed = 0;
    int lastStat32 = 0;
    int lastStat33 = 0;
    int lastStat34Observed = 0;
    int lastStat35 = 0;
    int lastPdiff28 = 0;
    int lastPdiff29 = 0;
    int lastPdiff30 = 0;
    int lastPdiff31 = 0;
    int lastPdiff32 = 0;
    int lastPdiff33 = 0;
    int lastPdiff34 = 0;
    int lastPdiff35 = 0;
    bool stateDirty = true;
    bool popupDirty = true;
    std::unordered_map<std::string, RarityInfo> rarityById;
} g_state;

static void BuildSiblingPath(const char* baseDir, const char* fileName, char* outPath, size_t outSize) {
    if (!outPath || outSize == 0) {
        return;
    }
    outPath[0] = '\0';
    if (!baseDir || baseDir[0] == '\0') {
        return;
    }
    std::snprintf(outPath, outSize, "%s\\%s", baseDir, fileName);
}

static void BuildSubPath(const char* baseDir, const char* subPath, char* outPath, size_t outSize) {
    if (!outPath || outSize == 0) {
        return;
    }
    outPath[0] = '\0';
    if (!baseDir || baseDir[0] == '\0' || !subPath || subPath[0] == '\0') {
        return;
    }
    std::snprintf(outPath, outSize, "%s\\%s", baseDir, subPath);
}

static void Log(const char* fmt, ...) {
    if (!g_cfg.verboseLogging || g_cfg.logPath[0] == '\0') {
        return;
    }

    FILE* fp = std::fopen(g_cfg.logPath, "a");
    if (!fp) {
        return;
    }

    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::fprintf(fp, "[%04u-%02u-%02u %02u:%02u:%02u] ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    va_list args;
    va_start(args, fmt);
    std::vfprintf(fp, fmt, args);
    va_end(args);

    std::fprintf(fp, "\n");
    std::fclose(fp);
}

static std::string Trim(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n')) {
        ++start;
    }
    std::size_t end = value.size();
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
        --end;
    }
    return value.substr(start, end - start);
}

static std::string EscapeIni(const std::string& value) {
    std::string out = value;
    std::replace(out.begin(), out.end(), '\r', ' ');
    std::replace(out.begin(), out.end(), '\n', ' ');
    return out;
}

static bool FileExists(const char* path) {
    if (!path || path[0] == '\0') {
        return false;
    }
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static void EnsureDirectory(const char* path) {
    if (!path || path[0] == '\0') {
        return;
    }
    CreateDirectoryA(path, nullptr);
}

static SectionMap ReadIniFile(const char* path) {
    SectionMap result;
    if (!FileExists(path)) {
        return result;
    }

    std::ifstream input(path);
    if (!input.is_open()) {
        return result;
    }

    std::string section = "default";
    std::string line;
    while (std::getline(input, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = Trim(line.substr(1, line.size() - 2));
            continue;
        }
        std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = Trim(line.substr(0, eq));
        std::string value = Trim(line.substr(eq + 1));
        result[section][key] = value;
    }
    return result;
}

static std::string GetIniString(const SectionMap& ini, const char* section, const char* key, const char* fallback = "") {
    SectionMap::const_iterator sit = ini.find(section);
    if (sit == ini.end()) {
        return fallback ? std::string(fallback) : std::string();
    }
    std::map<std::string, std::string>::const_iterator kit = sit->second.find(key);
    if (kit == sit->second.end()) {
        return fallback ? std::string(fallback) : std::string();
    }
    return kit->second;
}

static int GetIniInt(const SectionMap& ini, const char* section, const char* key, int fallback = 0) {
    const std::string value = GetIniString(ini, section, key, "");
    if (value.empty()) {
        return fallback;
    }
    char* end = nullptr;
    const double parsed = std::strtod(value.c_str(), &end);
    if (end == value.c_str()) {
        return fallback;
    }
    return static_cast<int>(std::lround(parsed));
}

static bool GetIniBool(const SectionMap& ini, const char* section, const char* key, bool fallback = false) {
    const std::string value = GetIniString(ini, section, key, "");
    if (value.empty()) {
        return fallback;
    }
    if (value == "1" || value == "true" || value == "TRUE" || value == "yes") {
        return true;
    }
    if (value == "0" || value == "false" || value == "FALSE" || value == "no") {
        return false;
    }
    return fallback;
}

static double GetIniDouble(const SectionMap& ini, const char* section, const char* key, double fallback = 0.0) {
    const std::string value = GetIniString(ini, section, key, "");
    if (value.empty()) {
        return fallback;
    }
    return std::atof(value.c_str());
}

static void WriteTextFile(const char* path, const std::string& text) {
    std::string tmp = std::string(path) + ".tmp";
    {
        std::ofstream out(tmp.c_str(), std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return;
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
    }
    DeleteFileA(path);
    MoveFileExA(tmp.c_str(), path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
}

static void InitializePaths(HMODULE module) {
    GetModuleFileNameA(module, g_cfg.modulePath, MAX_PATH);
    std::strncpy(g_cfg.moduleDir, g_cfg.modulePath, MAX_PATH - 1);
    g_cfg.moduleDir[MAX_PATH - 1] = '\0';

    char* slash = std::strrchr(g_cfg.moduleDir, '\\');
    if (slash) {
        *slash = '\0';
    }

    char dslDir[MAX_PATH] = {0};
    char scriptsDir[MAX_PATH] = {0};
    char achDir[MAX_PATH] = {0};
    BuildSubPath(g_cfg.moduleDir, "_derpy_script_loader", dslDir, sizeof(dslDir));
    BuildSubPath(dslDir, "scripts", scriptsDir, sizeof(scriptsDir));
    BuildSubPath(scriptsDir, "AchievementsMod", achDir, sizeof(achDir));
    std::strncpy(g_cfg.achievementsDir, achDir, MAX_PATH - 1);
    g_cfg.achievementsDir[MAX_PATH - 1] = '\0';
    EnsureDirectory(achDir);
    BuildSubPath(achDir, "Runtime", g_cfg.dataDir, sizeof(g_cfg.dataDir));
    EnsureDirectory(g_cfg.dataDir);
    char audioDir[MAX_PATH] = {0};
    BuildSubPath(achDir, "Audio", audioDir, sizeof(audioDir));
    EnsureDirectory(audioDir);
    char audioMp3Dir[MAX_PATH] = {0};
    BuildSubPath(audioDir, "MP3", audioMp3Dir, sizeof(audioMp3Dir));
    EnsureDirectory(audioMp3Dir);

    BuildSiblingPath(g_cfg.moduleDir, "AchievementsASI.ini", g_cfg.iniPath, sizeof(g_cfg.iniPath));
    BuildSiblingPath(g_cfg.dataDir, "AchievementsASI.log", g_cfg.logPath, sizeof(g_cfg.logPath));
    BuildSiblingPath(g_cfg.dataDir, "telemetry.ini", g_cfg.telemetryPath, sizeof(g_cfg.telemetryPath));
    BuildSiblingPath(g_cfg.dataDir, "state.ini", g_cfg.statePath, sizeof(g_cfg.statePath));
    BuildSiblingPath(g_cfg.dataDir, "popup.ini", g_cfg.popupPath, sizeof(g_cfg.popupPath));
    BuildSiblingPath(g_cfg.dataDir, "save.ini", g_cfg.savePath, sizeof(g_cfg.savePath));
    BuildSiblingPath(g_cfg.achievementsDir, "rarity_snapshot.ini", g_cfg.raritySnapshotPath, sizeof(g_cfg.raritySnapshotPath));
    BuildSubPath(audioMp3Dir, "steam_unlock.mp3", g_cfg.popupMp3Path, sizeof(g_cfg.popupMp3Path));
}

static void LoadConfig() {
    const SectionMap ini = ReadIniFile(g_cfg.iniPath);
    g_cfg.enable = GetIniBool(ini, "General", "Enable", true);
    g_cfg.verboseLogging = GetIniBool(ini, "General", "VerboseLogging", false);
    g_cfg.popupDuringMission = GetIniBool(ini, "General", "PopupDuringMission", true);
    g_cfg.backendPollMs = GetIniInt(ini, "General", "BackendPollMs", 1000);
    g_cfg.popupDurationMs = GetIniInt(ini, "General", "PopupDurationMs", 4000);
    g_cfg.popupGapMs = GetIniInt(ini, "General", "PopupGapMs", 600);
    g_cfg.popupMp3Enabled = GetIniBool(ini, "Audio", "EnablePopupMp3", true);
    g_cfg.popupMp3Volume = GetIniInt(ini, "Audio", "PopupMp3Volume", 1000);
    const std::string popupMp3RelativePath = GetIniString(ini, "Audio", "PopupMp3Path", "Audio\\MP3\\steam_unlock.mp3");
    if (!popupMp3RelativePath.empty()) {
        BuildSubPath(g_cfg.achievementsDir, popupMp3RelativePath.c_str(), g_cfg.popupMp3Path, sizeof(g_cfg.popupMp3Path));
    }
}

static std::string FormatPercent(double value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(value >= 10.0 ? 0 : 1) << value;
    return oss.str();
}

static std::string ClassifyRarity(double value) {
    if (value < 0.0) {
        return std::string();
    }
    if (value < 5.0) {
        return "Ultra Rare";
    }
    if (value < 10.0) {
        return "Very Rare";
    }
    if (value < 25.0) {
        return "Rare";
    }
    if (value < 50.0) {
        return "Uncommon";
    }
    return "Common";
}

static const RarityInfo* GetRarityInfo(const std::string& id) {
    const std::unordered_map<std::string, RarityInfo>::const_iterator it = g_state.rarityById.find(id);
    if (it == g_state.rarityById.end() || it->second.percent < 0.0) {
        return nullptr;
    }
    return &it->second;
}

static void LoadRaritySnapshot() {
    g_state.rarityById.clear();
    const SectionMap ini = ReadIniFile(g_cfg.raritySnapshotPath);
    SectionMap::const_iterator pit = ini.find("percent");
    if (pit == ini.end()) {
        Log("Rarity snapshot not found or empty: %s", g_cfg.raritySnapshotPath);
        return;
    }

    for (std::map<std::string, std::string>::const_iterator it = pit->second.begin(); it != pit->second.end(); ++it) {
        const double percent = std::atof(it->second.c_str());
        if (percent <= 0.0) {
            continue;
        }
        RarityInfo info;
        info.percent = percent;
        info.source = GetIniString(ini, "source", it->first.c_str(), "Online Snapshot");
        g_state.rarityById[it->first] = info;
    }

    Log("Rarity snapshot loaded: %u entries", static_cast<unsigned>(g_state.rarityById.size()));
}

static void PlayPopupMp3() {
    if (!g_cfg.popupMp3Enabled || g_cfg.popupMp3Path[0] == '\0' || !FileExists(g_cfg.popupMp3Path)) {
        return;
    }

    mciSendStringA("close BullyAchievementsPopup", nullptr, 0, nullptr);

    char command[2048] = {0};
    std::snprintf(command, sizeof(command), "open \"%s\" type mpegvideo alias BullyAchievementsPopup", g_cfg.popupMp3Path);
    if (mciSendStringA(command, nullptr, 0, nullptr) != 0) {
        Log("Popup MP3 open failed: %s", g_cfg.popupMp3Path);
        return;
    }

    if (g_cfg.popupMp3Volume >= 0) {
        char volumeCmd[128] = {0};
        const int popupVolume = g_cfg.popupMp3Volume < 0 ? 0 : (g_cfg.popupMp3Volume > 1000 ? 1000 : g_cfg.popupMp3Volume);
        std::snprintf(volumeCmd, sizeof(volumeCmd), "setaudio BullyAchievementsPopup volume to %d", popupVolume);
        mciSendStringA(volumeCmd, nullptr, 0, nullptr);
    }

    if (mciSendStringA("play BullyAchievementsPopup from 0", nullptr, 0, nullptr) != 0) {
        Log("Popup MP3 play failed: %s", g_cfg.popupMp3Path);
        mciSendStringA("close BullyAchievementsPopup", nullptr, 0, nullptr);
    }
}

static int CountUnlocked(const std::string& modeFilter, bool includeConsole) {
    int count = 0;
    for (std::size_t i = 0; i < g_state.defs.size(); ++i) {
        const AchievementDef& def = g_state.defs[i];
        if (!g_state.unlocked[i]) {
            continue;
        }
        if (!modeFilter.empty() && def.mode != modeFilter) {
            continue;
        }
        if (!includeConsole && def.mode == "console") {
            continue;
        }
        ++count;
    }
    return count;
}

static int GetMissionValue(const Telemetry& telemetry, const std::string& mission) {
    std::unordered_map<std::string, int>::const_iterator it = telemetry.missions.find(mission);
    return it == telemetry.missions.end() ? 0 : it->second;
}

static int GetSuccessValue(const Telemetry& telemetry, const std::string& mission) {
    std::unordered_map<std::string, int>::const_iterator it = telemetry.successCounts.find(mission);
    return it == telemetry.successCounts.end() ? 0 : it->second;
}

static bool IsMissionDone(const Telemetry& telemetry, const std::string& mission) {
    return GetMissionValue(telemetry, mission) > 0;
}

static int CountCliquesAt100(const Telemetry& telemetry) {
    int count = 0;
    for (int i = 1; i <= 5; ++i) {
        if (telemetry.faction[i] >= 100) {
            ++count;
        }
    }
    return count;
}

static bool AllBikeRacesDone(const Telemetry& telemetry) {
    return IsMissionDone(telemetry, "3_R08_Rich7")
        && IsMissionDone(telemetry, "3_R08_Business4")
        && IsMissionDone(telemetry, "3_R08_Poor2")
        && IsMissionDone(telemetry, "3_R08_School1");
}

static bool AllCarnivalPrizes(const Telemetry& telemetry) {
    return IsMissionDone(telemetry, "2_G2") && telemetry.saved11 && telemetry.saved12;
}

static bool AllCarnivalWins(const Telemetry& telemetry) {
    if (telemetry.minigameScore[1] > 0 && telemetry.minigameScore[3] > 0 && telemetry.minigameScore[4] > 0) {
        return true;
    }
    return telemetry.stat238 >= 100 && AllCarnivalPrizes(telemetry);
}

static int GetMetricValue(const Telemetry& telemetry, const std::string& metric) {
    if (metric == "curfewMinutes") return g_state.curfewMinutes;
    if (metric == "flowers") return telemetry.flowersCurrent;
    if (metric == "lawnJobsEarned") return g_state.lawnJobsEarned;
    if (metric == "eggCars") return g_state.eggCarsTotal;
    if (metric == "stat31") return telemetry.stat31;
    if (metric == "stat34") return telemetry.stat34;
    if (metric == "pdiff31") return telemetry.pdiff31;
    if (metric == "kissesGirl") return g_state.kissesGirl;
    if (metric == "sodas") return g_state.sodasTotal;
    if (metric == "errands") return telemetry.errands;
    if (metric == "skateDistance") return static_cast<int>(g_state.skateDistance);
    if (metric == "stat238") return telemetry.stat238;
    if (metric == "footDistance") return static_cast<int>(g_state.footDistance);
    if (metric == "stat66") return telemetry.stat66;
    if (metric == "bikeDistance") return static_cast<int>(g_state.bikeDistance);
    if (metric == "gameCompletion") return telemetry.gameCompletion;
    if (metric == "classesCompleted") return telemetry.classesCompleted;
    if (metric == "roomTrophies") return telemetry.roomTrophies;
    if (metric == "cliquesAt100") return CountCliquesAt100(telemetry);
    if (metric == "troublePoints") return g_state.troubleTotal;
    if (metric == "money") return telemetry.money;
    if (metric == "bikesJacked") return g_state.bikesJacked;
    if (metric == "swirlies") return g_state.swirliesTotal > telemetry.stat16 ? g_state.swirliesTotal : telemetry.stat16;
    return 0;
}

static bool GetBoolMetricValue(const Telemetry& telemetry, const std::string& metric) {
    if (metric == "fendWon") return telemetry.minigameScore[2] > 0;
    if (metric == "consumoWon") return telemetry.minigameScore[0] > 0;
    if (metric == "allBikeRacesDone") return AllBikeRacesDone(telemetry);
    if (metric == "allCarnivalPrizes") return AllCarnivalPrizes(telemetry);
    if (metric == "allCarnivalWins") return AllCarnivalWins(telemetry);
    return false;
}

static bool EvaluateDefinition(const AchievementDef& def, const Telemetry& telemetry) {
    switch (def.rule) {
    case RuleType::Threshold:
        return GetMetricValue(telemetry, def.key1) >= def.threshold;
    case RuleType::BoolMetric:
        return GetBoolMetricValue(telemetry, def.key1);
    case RuleType::BoolPair:
        return GetBoolMetricValue(telemetry, def.key1) && GetBoolMetricValue(telemetry, def.key2);
    case RuleType::MissionDone:
        return IsMissionDone(telemetry, def.key1);
    case RuleType::MissionOrSuccess:
        return IsMissionDone(telemetry, def.key1) || GetSuccessValue(telemetry, def.key1) >= def.threshold;
    case RuleType::CollectibleAll: {
        const int idx = def.threshold;
        return idx >= 0 && idx < 6 && telemetry.collectable[idx] > 0 && telemetry.collected[idx] >= telemetry.collectable[idx];
    }
    case RuleType::YearbookFull:
        return telemetry.yearbookFull;
    case RuleType::BoyGenius:
        return IsMissionDone(telemetry, "C_Art_5")
            && IsMissionDone(telemetry, "C_Wrestling_5")
            && IsMissionDone(telemetry, "C_Photography_5")
            && IsMissionDone(telemetry, "C_Shop_5")
            && IsMissionDone(telemetry, "C_Math_5")
            && IsMissionDone(telemetry, "C_Biology_5")
            && IsMissionDone(telemetry, "C_Geography_5")
            && IsMissionDone(telemetry, "C_Music_5")
            && IsMissionDone(telemetry, "C_Chem_5")
            && IsMissionDone(telemetry, "C_English_5");
    case RuleType::AllSingle:
        for (std::size_t i = 0; i < g_state.defs.size(); ++i) {
            const AchievementDef& other = g_state.defs[i];
            if (other.id == def.id || other.mode != "single") {
                continue;
            }
            if (!g_state.unlocked[i]) {
                return false;
            }
        }
        return true;
    case RuleType::AllNonMulti:
        for (std::size_t i = 0; i < g_state.defs.size(); ++i) {
            const AchievementDef& other = g_state.defs[i];
            if (other.id == def.id || other.mode == "multi") {
                continue;
            }
            if (!g_state.unlocked[i]) {
                return false;
            }
        }
        return true;
    case RuleType::KissBoth:
        return g_state.kissesGirl >= 1 && g_state.kissesBoy >= 1;
    case RuleType::MiniObjective16:
        return telemetry.miniObjective16;
    case RuleType::LawnPercent:
        return telemetry.lawnPercent >= 99;
    case RuleType::None:
    default:
        return false;
    }
}

static std::string MakeStatusText(const AchievementDef& def, const Telemetry& telemetry, bool unlocked) {
    if (unlocked) {
        return "Unlocked";
    }
    switch (def.rule) {
    case RuleType::Threshold: {
        const int value = GetMetricValue(telemetry, def.key1);
        if (def.key1 == "curfewMinutes") {
            std::ostringstream oss;
            oss << "Locked (" << (value / 60) << "/5 hours)";
            return oss.str();
        }
        if (def.key1 == "money") {
            std::ostringstream oss;
            oss << "Locked ($" << value << "/$" << def.threshold << ")";
            return oss.str();
        }
        if (def.key1 == "footDistance" || def.key1 == "bikeDistance" || def.key1 == "skateDistance") {
            double currentMeters = 0.0;
            if (def.key1 == "footDistance") {
                currentMeters = g_state.footDistance;
            } else if (def.key1 == "bikeDistance") {
                currentMeters = g_state.bikeDistance;
            } else {
                currentMeters = g_state.skateDistance;
            }
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1)
                << "Locked (" << (currentMeters / 1000.0)
                << "/" << (static_cast<double>(def.threshold) / 1000.0) << " km)";
            return oss.str();
        }
        std::ostringstream oss;
        oss << "Locked (" << value << "/" << def.threshold << ")";
        return oss.str();
    }
    case RuleType::CollectibleAll: {
        const int idx = def.threshold;
        std::ostringstream oss;
        oss << "Locked (" << telemetry.collected[idx] << "/" << telemetry.collectable[idx] << ")";
        return oss.str();
    }
    case RuleType::KissBoth: {
        std::ostringstream oss;
        oss << "Locked (" << g_state.kissesGirl << "/1 girls, " << g_state.kissesBoy << "/1 boys)";
        return oss.str();
    }
    default:
        return "Locked";
    }
}

static std::string MakeStatusWithRarity(const AchievementDef& def, const Telemetry& telemetry, bool unlocked) {
    std::string status = MakeStatusText(def, telemetry, unlocked);
    const RarityInfo* rarity = GetRarityInfo(def.id);
    if (!rarity) {
        return status;
    }
    status += " | ";
    status += FormatPercent(rarity->percent);
    status += "% ";
    status += ClassifyRarity(rarity->percent);
    return status;
}

static void QueueUnlock(std::size_t index) {
    if (index >= g_state.unlocked.size() || g_state.unlocked[index]) {
        return;
    }
    g_state.unlocked[index] = true;
    g_state.popupQueue.push_back(static_cast<int>(index));
    g_state.stateDirty = true;
    g_state.popupDirty = true;
    Log("Unlocked %s", g_state.defs[index].id.c_str());
}

static void LoadPersistentState() {
    const SectionMap ini = ReadIniFile(g_cfg.savePath);
    g_state.initialSyncComplete = GetIniBool(ini, "meta", "initialSyncComplete", false);
    for (std::size_t i = 0; i < g_state.defs.size(); ++i) {
        const AchievementDef& def = g_state.defs[i];
        g_state.unlocked[i] = GetIniBool(ini, "unlocked", def.id.c_str(), false);
    }
    g_state.troubleTotal = GetIniInt(ini, "custom", "troubleTotal", 0);
    g_state.troubleLast = GetIniInt(ini, "custom", "troubleLast", 0);
    g_state.bikesJacked = GetIniInt(ini, "custom", "bikesJacked", 0);
    g_state.lastBikeId = GetIniInt(ini, "custom", "lastBikeId", -1);
    g_state.lastOnBike = GetIniBool(ini, "custom", "lastOnBike", false);
    g_state.sodasTotal = GetIniInt(ini, "custom", "sodasTotal", 0);
    g_state.lastSodaActive = GetIniBool(ini, "custom", "lastSodaActive", false);
    g_state.kissesGirl = GetIniInt(ini, "custom", "kissesGirl", 0);
    g_state.kissesBoy = GetIniInt(ini, "custom", "kissesBoy", 0);
    g_state.lastKissActive = GetIniBool(ini, "custom", "lastKissActive", false);
    g_state.curfewMinutes = GetIniInt(ini, "custom", "curfewMinutes", 0);
    g_state.lastClockMinutes = GetIniInt(ini, "custom", "lastClockMinutes", -1);
    g_state.lawnJobsEarned = GetIniInt(ini, "custom", "lawnJobsEarned", 0);
    g_state.lastLawnCompletedCount = GetIniInt(ini, "custom", "lastLawnCompletedCount", -1);
    g_state.footDistance = GetIniDouble(ini, "custom", "footDistance", 0.0);
    g_state.bikeDistance = GetIniDouble(ini, "custom", "bikeDistance", 0.0);
    g_state.skateDistance = GetIniDouble(ini, "custom", "skateDistance", 0.0);
    g_state.lastPosX = GetIniDouble(ini, "custom", "lastPosX", 0.0);
    g_state.lastPosY = GetIniDouble(ini, "custom", "lastPosY", 0.0);
    g_state.lastPosValid = GetIniBool(ini, "custom", "lastPosValid", false);
    g_state.swirliesTotal = GetIniInt(ini, "custom", "swirliesTotal", 0);
    g_state.lastStat16 = GetIniInt(ini, "custom", "lastStat16", 0);
    g_state.lastPdiff16 = GetIniInt(ini, "custom", "lastPdiff16", -1);
    g_state.lastSwirlieActive = GetIniBool(ini, "custom", "lastSwirlieActive", false);
    g_state.eggCarsTotal = GetIniInt(ini, "custom", "eggCarsTotal", 0);
    g_state.lastPdiff31 = GetIniInt(ini, "custom", "lastPdiff31", 0);
}

static void SavePersistentState() {
    std::ostringstream out;
    out << "[meta]\n";
    out << "initialSyncComplete=" << (g_state.initialSyncComplete ? 1 : 0) << "\n\n";
    out << "[unlocked]\n";
    for (std::size_t i = 0; i < g_state.defs.size(); ++i) {
        out << g_state.defs[i].id << "=" << (g_state.unlocked[i] ? 1 : 0) << "\n";
    }
    out << "\n[custom]\n";
    out << "troubleTotal=" << g_state.troubleTotal << "\n";
    out << "troubleLast=" << g_state.troubleLast << "\n";
    out << "bikesJacked=" << g_state.bikesJacked << "\n";
    out << "lastBikeId=" << g_state.lastBikeId << "\n";
    out << "lastOnBike=" << (g_state.lastOnBike ? 1 : 0) << "\n";
    out << "sodasTotal=" << g_state.sodasTotal << "\n";
    out << "lastSodaActive=" << (g_state.lastSodaActive ? 1 : 0) << "\n";
    out << "kissesGirl=" << g_state.kissesGirl << "\n";
    out << "kissesBoy=" << g_state.kissesBoy << "\n";
    out << "lastKissActive=" << (g_state.lastKissActive ? 1 : 0) << "\n";
    out << "curfewMinutes=" << g_state.curfewMinutes << "\n";
    out << "lastClockMinutes=" << g_state.lastClockMinutes << "\n";
    out << "lawnJobsEarned=" << g_state.lawnJobsEarned << "\n";
    out << "lastLawnCompletedCount=" << g_state.lastLawnCompletedCount << "\n";
    out << "footDistance=" << g_state.footDistance << "\n";
    out << "bikeDistance=" << g_state.bikeDistance << "\n";
    out << "skateDistance=" << g_state.skateDistance << "\n";
    out << "lastPosX=" << g_state.lastPosX << "\n";
    out << "lastPosY=" << g_state.lastPosY << "\n";
    out << "lastPosValid=" << (g_state.lastPosValid ? 1 : 0) << "\n";
    out << "swirliesTotal=" << g_state.swirliesTotal << "\n";
    out << "lastStat16=" << g_state.lastStat16 << "\n";
    out << "lastPdiff16=" << g_state.lastPdiff16 << "\n";
    out << "lastSwirlieActive=" << (g_state.lastSwirlieActive ? 1 : 0) << "\n";
    out << "eggCarsTotal=" << g_state.eggCarsTotal << "\n";
    out << "lastPdiff31=" << g_state.lastPdiff31 << "\n";
    WriteTextFile(g_cfg.savePath, out.str());
}

static void WriteStateFile(const Telemetry& telemetry) {
    int totalSingle = 0;
    for (std::size_t i = 0; i < g_state.defs.size(); ++i) {
        if (g_state.defs[i].mode == "single") {
            ++totalSingle;
        }
    }

    std::ostringstream out;
    out << "[summary]\n";
    out << "version=asi-bridge-1\n";
    out << "updated=" << GetTickCount() << "\n";
    out << "total=" << g_state.defs.size() << "\n";
    out << "unlocked=" << CountUnlocked(std::string(), true) << "\n";
    out << "totalSingle=" << totalSingle << "\n";
    out << "unlockedSingle=" << CountUnlocked("single", true) << "\n\n";

    for (std::size_t i = 0; i < g_state.defs.size(); ++i) {
        const AchievementDef& def = g_state.defs[i];
        const bool hidden = def.secret && !g_state.unlocked[i];
        out << "[item" << i << "]\n";
        out << "id=" << EscapeIni(def.id) << "\n";
        out << "title=" << EscapeIni(def.title) << "\n";
        out << "description=" << EscapeIni(def.description) << "\n";
        out << "icon=" << EscapeIni(def.icon) << "\n";
        out << "unlocked=" << (g_state.unlocked[i] ? 1 : 0) << "\n";
        out << "secret=" << (def.secret ? 1 : 0) << "\n";
        out << "hidden=" << (hidden ? 1 : 0) << "\n";
        out << "mode=" << EscapeIni(def.mode) << "\n";
        out << "verified=" << (def.verified ? 1 : 0) << "\n";
        const RarityInfo* rarity = GetRarityInfo(def.id);
        if (rarity) {
            out << "rarityPercent=" << std::fixed << std::setprecision(2) << rarity->percent << "\n";
            out << "rarityLabel=" << EscapeIni(ClassifyRarity(rarity->percent)) << "\n";
            out << "raritySource=" << EscapeIni(rarity->source) << "\n";
        }
        out << "status=" << EscapeIni(MakeStatusWithRarity(def, telemetry, g_state.unlocked[i])) << "\n\n";
    }
    WriteTextFile(g_cfg.statePath, out.str());
}

static void WritePopupFile() {
    std::ostringstream out;
    out << "[popup]\n";
    out << "nonce=" << g_state.popupNonce << "\n";
    if (g_state.activePopupIndex >= 0 && g_state.activePopupIndex < static_cast<int>(g_state.defs.size())) {
        const AchievementDef& def = g_state.defs[static_cast<std::size_t>(g_state.activePopupIndex)];
        out << "active=1\n";
        out << "title=" << EscapeIni(def.title) << "\n";
        out << "icon=" << EscapeIni(def.icon) << "\n";
        out << "durationMs=" << g_cfg.popupDurationMs << "\n";
        out << "fadeMs=250\n";
    } else {
        out << "active=0\n";
    }
    WriteTextFile(g_cfg.popupPath, out.str());
}

static void AddDef(const char* id, const char* title, const char* description, const char* icon, bool secret, const char* mode,
    RuleType rule, const char* key1 = "", int threshold = 0, const char* key2 = "") {
    AchievementDef def;
    def.id = id;
    def.title = title;
    def.description = description;
    def.icon = icon;
    def.secret = secret;
    def.mode = mode;
    def.rule = rule;
    def.key1 = key1 ? key1 : "";
    def.key2 = key2 ? key2 : "";
    def.threshold = threshold;
    g_state.idToIndex[def.id] = g_state.defs.size();
    g_state.defs.push_back(def);
}

static void BuildDefinitions() {
    g_state.defs.clear();
    g_state.idToIndex.clear();

    AddDef("BA_AfterHours", "After Hours", "Spend 5 hours out after curfew.", "ach_icon_0", false, "single", RuleType::Threshold, "curfewMinutes", 300);
    AddDef("BA_GreenThumbsUp", "Green Thumbs Up", "Pick 50 flowers.", "ach_icon_1", false, "single", RuleType::Threshold, "flowers", 50);
    AddDef("BA_Eggsellent", "Eggsellent", "Egg 10 cars.", "ach_icon_2", false, "single", RuleType::Threshold, "eggCars", 10);
    AddDef("BA_KickinTheBalls", "Kickin' the Balls", "Complete Nut Shots mini-game.", "ach_icon_3", false, "single", RuleType::BoolMetric, "fendWon");
    AddDef("BA_WatchYourStep", "Watch Your Step", "Complete Rats in the Library.", "ach_icon_4", false, "single", RuleType::MissionDone, "5_01");
    AddDef("BA_DualNebula", "Dual Nebula", "Complete Consumo and Nut Shots.", "ach_icon_5", false, "single", RuleType::BoolPair, "consumoWon", 0, "fendWon");
    AddDef("BA_DownForTheCount", "Down for the Count", "Complete Chapter 3.", "ach_icon_6", false, "single", RuleType::MissionDone, "3_B");
    AddDef("BA_Casanova", "Casanova", "Kiss 25 girls.", "ach_icon_7", false, "single", RuleType::Threshold, "kissesGirl", 25);
    AddDef("BA_Sodalicious", "Sodalicious", "Drink 500 sodas.", "ach_icon_8", false, "single", RuleType::Threshold, "sodas", 500);
    AddDef("BA_HelpingHand", "Helping Hand", "Complete 41 errands.", "ach_icon_9", false, "single", RuleType::Threshold, "errands", 41);
    AddDef("BA_LittleAngel", "Little Angel", "Complete 10 errands.", "ach_icon_10", false, "single", RuleType::Threshold, "errands", 10);
    AddDef("BA_Keener", "Keener", "Complete Chapter 2.", "ach_icon_11", false, "single", RuleType::MissionDone, "2_B");
    AddDef("BA_TeachersPet", "Teacher's Pet", "Complete Chapter 4.", "ach_icon_12", false, "single", RuleType::MissionDone, "4_B2");
    AddDef("BA_PolePosition", "Pole Position", "Complete Chapter 5.", "ach_icon_13", false, "single", RuleType::MissionDone, "5_B");
    AddDef("BA_Freshman", "Freshman", "Complete Chapter 1.", "ach_icon_14", true, "single", RuleType::MissionDone, "1_B");
    AddDef("BA_Sophomore", "Sophomore", "Complete Chapter 2.", "ach_icon_15", true, "single", RuleType::MissionDone, "2_B");
    AddDef("BA_Junior", "Junior", "Complete Chapter 3.", "ach_icon_16", true, "single", RuleType::MissionDone, "3_B");
    AddDef("BA_Senior", "Senior", "Complete Chapter 4.", "ach_icon_17", true, "single", RuleType::MissionDone, "4_B2");
    AddDef("BA_SkatePro", "Skate Pro", "Travel 50 km on skateboard.", "ach_icon_18", false, "single", RuleType::Threshold, "skateDistance", 50000);
    AddDef("BA_SmartMouth", "Smart Mouth", "Unlock the full taunt set.", "ach_icon_19", false, "single", RuleType::MissionDone, "C_English_5");
    AddDef("BA_SmellYaLater", "Smell Ya Later", "Collect all transistors.", "ach_icon_20", false, "single", RuleType::CollectibleAll, "", 1);
    AddDef("BA_Skidmark", "Skidmark", "Finish all bike races.", "ach_icon_21", false, "single", RuleType::BoolMetric, "allBikeRacesDone");
    AddDef("BA_GlassDismissed", "Glass Dismissed", "Break 100 bottles in shooting gallery.", "ach_icon_22", false, "single", RuleType::Threshold, "stat238", 100);
    AddDef("BA_MommasBoy", "Momma's Boy", "Complete 31 errands.", "ach_icon_23", false, "single", RuleType::Threshold, "errands", 31);
    AddDef("BA_Marathon", "Marathon", "Travel 100 km on foot.", "ach_icon_24", false, "single", RuleType::Threshold, "footDistance", 100000);
    AddDef("BA_TheWheelDeal", "The Wheel Deal", "Win all carnival prizes.", "ach_icon_25", false, "single", RuleType::BoolMetric, "allCarnivalPrizes");
    AddDef("BA_SharpDressedMan", "Sharp Dressed Man", "Purchase 250 clothing items.", "ach_icon_26", false, "single", RuleType::Threshold, "stat66", 250);
    AddDef("BA_TourDeBullworth", "Tour de Bullworth", "Travel 100 km on bike.", "ach_icon_27", false, "single", RuleType::Threshold, "bikeDistance", 100000);
    AddDef("BA_Graduate", "Graduate", "Complete the final chapter.", "ach_icon_28", true, "single", RuleType::MissionDone, "6_B");
    AddDef("BA_ItsAllInTheWrists", "It's All in the Wrists", "Win all carnival side games at least once.", "ach_icon_29", false, "single", RuleType::BoolMetric, "allCarnivalWins");
    AddDef("BA_TheChampion", "The Champion", "Complete all bike races.", "ach_icon_30", false, "single", RuleType::BoolMetric, "allBikeRacesDone");
    AddDef("BA_BlackAndWhite", "Black and White", "Complete your yearbook.", "ach_icon_31", false, "single", RuleType::YearbookFull);
    AddDef("BA_BoyGenius", "Boy Genius", "Complete all classes to level 5.", "ach_icon_32", false, "single", RuleType::BoyGenius);
    AddDef("BA_MissionAccomplished", "Mission Accomplished", "Reach 100% game completion.", "ach_icon_33", false, "single", RuleType::Threshold, "gameCompletion", 100);
    AddDef("BA_Perfeccionist", "Perfeccionist", "Finish all single-player achievements.", "ach_icon_34", true, "single", RuleType::AllSingle);
    AddDef("BA_GreenThumb", "Green Thumb", "Pick all flowers.", "ach_icon_35", false, "single", RuleType::Threshold, "flowers", 50);
    AddDef("BA_SpeedFreak", "Speed Freak", "Win Go-Kart GP5.", "ach_icon_36", false, "single", RuleType::MissionOrSuccess, "GoKart_GP5", 1);
    AddDef("BA_OverTheRainbow", "Over the Rainbow", "Kiss both boys and girls at least once.", "ach_icon_37", false, "single", RuleType::KissBoth);

    AddDef("RA_WelcomeToBullworth", "Welcome To Bullworth", "Complete the mission: Welcome To Bullworth", "Welcome To Bullworth", false, "single", RuleType::MissionDone, "1_01");
    AddDef("RA_ThisIsYourSchool", "This Is Your School", "Complete the mission: This Is Your School", "This Is Your School", false, "single", RuleType::MissionDone, "1_02B");
    AddDef("RA_TheSetup", "The Setup", "Complete the mission: The Setup", "The Setup", false, "single", RuleType::MissionDone, "1_03");
    AddDef("RA_Slingshot", "Slingshot", "Complete the mission: The Slingshot", "Slingshot", false, "single", RuleType::MissionDone, "1_04");
    AddDef("RA_SaveAlgie", "Save Algie", "Complete the mission: Save Algie", "Save Algie", false, "single", RuleType::MissionDone, "1_05");
    AddDef("RA_ALittleHelp", "A Little Help", "Complete the mission: A Little Help", "A Little Help", false, "single", RuleType::MissionDone, "1_06");
    AddDef("RA_DefendBucky", "Defend Bucky", "Complete the mission: Defend Bucky", "Defend Bucky", false, "single", RuleType::MissionDone, "1_07");
    AddDef("RA_ThatBitch", "That Bitch", "Complete the mission: That Bitch", "That Bitch", false, "single", RuleType::MissionDone, "1_08");
    AddDef("RA_TheDiary", "The Diary", "Complete the mission: The Diary", "The Diary", false, "single", RuleType::MissionDone, "1_G1");
    AddDef("RA_TheCandidate", "The Candidate", "Complete the mission: The Candidate", "The Candidate", false, "single", RuleType::MissionDone, "1_09");
    AddDef("RA_Halloween", "Halloween", "Complete the mission: Halloween", "Halloween", false, "single", RuleType::MissionDone, "1_11");
    AddDef("RA_TheBigPrank", "The Big Prank", "Complete the mission: The Big Prank", "The Big Prank", false, "single", RuleType::MissionDone, "1_11B");
    AddDef("RA_HattrickVsGalloway", "Hattrick Vs Galloway", "Complete the mission: Hattrick vs Galloway", "Hattrick Vs Galloway", false, "single", RuleType::MissionDone, "1_S01");
    AddDef("RA_CharacterSheets", "Character Sheets", "Complete the mission: Character Sheets", "Character Sheets", false, "single", RuleType::MissionDone, "2_S04");
    AddDef("RA_Chemist", "Chemist", "Complete class 5 of Chemistry.", "Chemist", false, "single", RuleType::MissionDone, "C_Chem_5");
    AddDef("RA_BoxingChallenge", "Boxing Challenge", "Complete the mission: Boxing Challenge", "Boxing Challenge", false, "single", RuleType::MissionDone, "2_09");
    AddDef("RA_DishonorableFight", "Dishonorable Fight", "Complete the mission: Dishonorable Fight", "Dishonorable Fight", false, "single", RuleType::MissionDone, "2_B");
    AddDef("RA_CarnivalDate", "Carnival Date", "Complete the mission: Carnival Date", "Carnival Date", false, "single", RuleType::MissionDone, "2_G2");
    AddDef("RA_PantyRaid", "Panty Raid", "Complete the mission: Panty Raid", "Panty Raid", false, "single", RuleType::MissionDone, "2_S06");
    AddDef("RA_RaceTheVale", "Race the Vale", "Complete the mission: Race the Vale", "Race the Vale", false, "single", RuleType::MissionDone, "2_04");
    AddDef("RA_BeachRumble", "Beach Rumble", "Complete the mission: Beach Rumble", "Beach Rumble", false, "single", RuleType::MissionDone, "2_07");
    AddDef("RA_TadsHouse", "Tad's House", "Complete the mission: Tad's House", "Tad's House", false, "single", RuleType::MissionDone, "2_05");
    AddDef("RA_WeedKiller", "Weed Killer", "Complete the mission: Weed Killer", "Weed Killer", false, "single", RuleType::MissionDone, "2_08");
    AddDef("RA_MovieTickets", "Movie Tickets", "Complete the mission: Movie Tickets", "Movie Tickets", false, "single", RuleType::MissionDone, "2_06");
    AddDef("RA_TheEggs", "The Eggs", "Complete the mission: The Eggs", "The Eggs", false, "single", RuleType::MissionDone, "2_03");
    AddDef("RA_LastMinuteShopping", "Last Minute Shopping", "Complete the mission: Last Minute Shopping", "Last Minute Shopping", false, "single", RuleType::MissionDone, "2_01");
    AddDef("RA_RussellInTheHole", "Russell In The Hole", "Complete the mission: Russell in the Hole", "Russel In The Hole", false, "single", RuleType::MissionDone, "1_B");
    AddDef("RA_LolasRace", "Lola's Race", "Complete the mission: Lola's Race", "Lola's Race", false, "single", RuleType::MissionDone, "3_G3");
    AddDef("RA_Paparazzi", "Paparazzi", "Complete the mission: Paparazzi", "Paparazzi", false, "single", RuleType::MissionDone, "4_01");
    AddDef("RA_PrepChallenge", "Prep Challenge", "Complete the mission: Prep Challenge", "Prep Challenge", false, "single", RuleType::MissionDone, "3_R09");
    AddDef("RA_HelpGary", "Help Gary", "Complete the mission: Help Gary", "Help Gary", false, "single", RuleType::MissionDone, "1_10");
    AddDef("RA_BlowTheManDown", "Blow the Man Down", "Defeat a pirate.", "Blow the Man Down", false, "single", RuleType::MiniObjective16);
    AddDef("RA_Flushed", "Flushed", "Give someone a swirlie.", "Flushed", false, "single", RuleType::Threshold, "swirlies", 1);
    AddDef("RA_KissMyGrass", "Kiss My Grass", "Complete a mowing activity to 100 percent.", "Kiss My Grass", false, "single", RuleType::Threshold, "lawnJobsEarned", 1);
    AddDef("RA_KnowledgeIsPower", "Knowledge Is Power", "Successfully complete any three classes.", "Knowledge Is Power", false, "single", RuleType::Threshold, "classesCompleted", 3);
    AddDef("RA_OhMyGourd", "Oh My Gourd", "Destroy all pumpkins.", "Oh My Gourd", false, "single", RuleType::CollectibleAll, "", 4);
    AddDef("RA_RandomViolence", "Random Violence, Widespread Destruction, Gratuitous Sadism...", "Reach 50% game completion.", "Random Violence, Widespread Destruction, Gratuitous Sadism...", false, "single", RuleType::Threshold, "gameCompletion", 50);
    AddDef("RA_WellThatWasFun", "Well, That Was Fun... I Guess", "Destroy all gravestones.", "Well, That Was Fun... I Guess", false, "single", RuleType::CollectibleAll, "", 5);
    AddDef("RA_YeahOkay", "Yeah, Okay, I'll Save Your Ass", "Complete 10 errands.", "Yeah, Okay, I'll Save Your Ass", false, "single", RuleType::Threshold, "errands", 10);

    AddDef("PS4_Kleptomania", "Kleptomania", "Acquire all room trophies.", "ach_icon_50", false, "console", RuleType::Threshold, "roomTrophies", 40);
    AddDef("PS4_PopularityContest", "Popularity Contest", "Gain 100% respect from 2 cliques simultaneously.", "ach_icon_51", false, "console", RuleType::Threshold, "cliquesAt100", 2);
    AddDef("PS4_ProblemChild", "Problem Child", "Amass 160,000 trouble points.", "ach_icon_52", false, "console", RuleType::Threshold, "troublePoints", 160000);
    AddDef("PS4_RichKidBlues", "Rich Kid Blues", "Have $1,000 in pocket change.", "ach_icon_53", false, "console", RuleType::Threshold, "money", 1000);
    AddDef("PS4_ExpertBicycleThief", "The (Expert) Bicycle Thief", "Jack 20 bicycles.", "ach_icon_54", false, "console", RuleType::Threshold, "bikesJacked", 20);
    AddDef("PS4_Valedictorian", "Valedictorian", "Unlock every trophy.", "ach_icon_55", true, "console", RuleType::AllNonMulti);

    g_state.unlocked.assign(g_state.defs.size(), false);
}

static Telemetry ReadTelemetry() {
    Telemetry telemetry;
    const SectionMap ini = ReadIniFile(g_cfg.telemetryPath);
    telemetry.updated = GetIniInt(ini, "meta", "updated", 0);
    telemetry.runtimeReady = GetIniBool(ini, "state", "runtimeReady", false);
    telemetry.missionActive = GetIniBool(ini, "state", "missionActive", false);
    telemetry.cutsceneActive = GetIniBool(ini, "state", "cutsceneActive", false);
    telemetry.gameCompletion = GetIniInt(ini, "state", "gameCompletion", 0);
    telemetry.stat16 = GetIniInt(ini, "state", "stat16", 0);
    telemetry.pdiff16 = GetIniInt(ini, "state", "pdiff16", 0);
    telemetry.stat28 = GetIniInt(ini, "state", "stat28", 0);
    telemetry.stat29 = GetIniInt(ini, "state", "stat29", 0);
    telemetry.stat30 = GetIniInt(ini, "state", "stat30", 0);
    telemetry.stat31 = GetIniInt(ini, "state", "stat31", 0);
    telemetry.stat32 = GetIniInt(ini, "state", "stat32", 0);
    telemetry.stat33 = GetIniInt(ini, "state", "stat33", 0);
    telemetry.stat34 = GetIniInt(ini, "state", "stat34", 0);
    telemetry.stat35 = GetIniInt(ini, "state", "stat35", 0);
    telemetry.pdiff28 = GetIniInt(ini, "state", "pdiff28", 0);
    telemetry.pdiff29 = GetIniInt(ini, "state", "pdiff29", 0);
    telemetry.pdiff30 = GetIniInt(ini, "state", "pdiff30", 0);
    telemetry.pdiff31 = GetIniInt(ini, "state", "pdiff31", 0);
    telemetry.pdiff32 = GetIniInt(ini, "state", "pdiff32", 0);
    telemetry.pdiff33 = GetIniInt(ini, "state", "pdiff33", 0);
    telemetry.pdiff34 = GetIniInt(ini, "state", "pdiff34", 0);
    telemetry.pdiff35 = GetIniInt(ini, "state", "pdiff35", 0);
    telemetry.stat66 = GetIniInt(ini, "state", "stat66", 0);
    telemetry.stat238 = GetIniInt(ini, "state", "stat238", 0);
    telemetry.roomTrophies = GetIniInt(ini, "state", "roomTrophies", 0);
    telemetry.money = GetIniInt(ini, "state", "money", 0);
    telemetry.punishmentPoints = GetIniInt(ini, "state", "punishmentPoints", 0);
    telemetry.bikeId = GetIniInt(ini, "state", "bikeId", -1);
    telemetry.flowersCurrent = GetIniInt(ini, "state", "flowersCurrent", 0);
    telemetry.lawnCompletedCount = GetIniInt(ini, "state", "lawnCompletedCount", 0);
    telemetry.lawnPercent = GetIniInt(ini, "state", "lawnPercent", 0);
    telemetry.classesCompleted = GetIniInt(ini, "state", "classesCompleted", 0);
    telemetry.errands = GetIniInt(ini, "state", "errands", 0);
    telemetry.clockMinutes = GetIniInt(ini, "state", "clockMinutes", -1);
    telemetry.curfewActive = GetIniBool(ini, "state", "curfewActive", false);
    telemetry.yearbookFull = GetIniBool(ini, "state", "yearbookFull", false);
    telemetry.miniObjective16 = GetIniBool(ini, "state", "miniObjective16", false);
    telemetry.swirlieActive = GetIniBool(ini, "state", "swirlieActive", false);
    telemetry.sodaActive = GetIniBool(ini, "state", "sodaActive", false);
    telemetry.kissActive = GetIniBool(ini, "state", "kissActive", false);
    telemetry.kissTargetFemale = GetIniBool(ini, "state", "kissTargetFemale", false);
    telemetry.onSkate = GetIniBool(ini, "state", "onSkate", false);
    telemetry.saved11 = GetIniBool(ini, "state", "saved11", false);
    telemetry.saved12 = GetIniBool(ini, "state", "saved12", false);
    telemetry.posX = GetIniDouble(ini, "state", "posX", 0.0);
    telemetry.posY = GetIniDouble(ini, "state", "posY", 0.0);
    telemetry.hasPosition = GetIniBool(ini, "state", "hasPosition", false);

    for (int i = 0; i < 5; ++i) {
        char key[32];
        std::snprintf(key, sizeof(key), "score%d", i);
        telemetry.minigameScore[i] = GetIniInt(ini, "minigame", key, 0);
    }
    for (int i = 0; i < 6; ++i) {
        char keyA[32];
        char keyB[32];
        std::snprintf(keyA, sizeof(keyA), "collectable%d", i);
        std::snprintf(keyB, sizeof(keyB), "collected%d", i);
        telemetry.collectable[i] = GetIniInt(ini, "collectibles", keyA, 0);
        telemetry.collected[i] = GetIniInt(ini, "collectibles", keyB, 0);
    }
    for (int i = 1; i <= 5; ++i) {
        char key[32];
        std::snprintf(key, sizeof(key), "clique%d", i);
        telemetry.faction[i] = GetIniInt(ini, "factions", key, 0);
    }

    SectionMap::const_iterator mit = ini.find("missions");
    if (mit != ini.end()) {
        for (std::map<std::string, std::string>::const_iterator it = mit->second.begin(); it != mit->second.end(); ++it) {
            telemetry.missions[it->first] = std::atoi(it->second.c_str());
        }
    }
    SectionMap::const_iterator sit = ini.find("success");
    if (sit != ini.end()) {
        for (std::map<std::string, std::string>::const_iterator it = sit->second.begin(); it != sit->second.end(); ++it) {
            telemetry.successCounts[it->first] = std::atoi(it->second.c_str());
        }
    }

    telemetry.valid = telemetry.updated > 0;
    return telemetry;
}

static void UpdateCustomCounters(const Telemetry& telemetry) {
    if (!telemetry.runtimeReady) {
        return;
    }

    bool customChanged = false;

    if (telemetry.punishmentPoints > g_state.troubleLast) {
        g_state.troubleTotal += telemetry.punishmentPoints - g_state.troubleLast;
        customChanged = true;
    }
    g_state.troubleLast = telemetry.punishmentPoints;

    const bool onBike = telemetry.bikeId > -1;
    if (onBike && !g_state.lastOnBike && telemetry.bikeId != g_state.lastBikeId) {
        ++g_state.bikesJacked;
        g_state.lastBikeId = telemetry.bikeId;
        customChanged = true;
    }
    g_state.lastOnBike = onBike;

    if (telemetry.sodaActive && !g_state.lastSodaActive) {
        ++g_state.sodasTotal;
        customChanged = true;
    }
    g_state.lastSodaActive = telemetry.sodaActive;

    if (telemetry.kissActive && !g_state.lastKissActive) {
        if (telemetry.kissTargetFemale) {
            ++g_state.kissesGirl;
        } else {
            ++g_state.kissesBoy;
        }
        customChanged = true;
    }
    g_state.lastKissActive = telemetry.kissActive;

    if (telemetry.clockMinutes >= 0) {
        if (g_state.lastClockMinutes >= 0) {
            int delta = telemetry.clockMinutes - g_state.lastClockMinutes;
            if (delta < -720) {
                delta += 1440;
            } else if (delta > 720) {
                delta -= 1440;
            }
            if (delta > 0 && delta <= 10 && telemetry.curfewActive) {
                g_state.curfewMinutes += delta;
                customChanged = true;
            }
        }
        g_state.lastClockMinutes = telemetry.clockMinutes;
    }

    if (telemetry.missionActive) {
        g_state.lawnCompletionArmed = true;
    }

    if (g_state.lastLawnCompletedCount < 0) {
        g_state.lastLawnCompletedCount = telemetry.lawnCompletedCount;
    } else if (telemetry.lawnCompletedCount > g_state.lastLawnCompletedCount) {
        if (g_state.lawnCompletionArmed) {
            g_state.lawnJobsEarned += telemetry.lawnCompletedCount - g_state.lastLawnCompletedCount;
            customChanged = true;
        }
        g_state.lastLawnCompletedCount = telemetry.lawnCompletedCount;
        g_state.lawnCompletionArmed = false;
    } else if (telemetry.lawnCompletedCount < g_state.lastLawnCompletedCount) {
        g_state.lastLawnCompletedCount = telemetry.lawnCompletedCount;
        g_state.lawnCompletionArmed = false;
    }

    if (telemetry.hasPosition) {
        if (g_state.lastPosValid) {
            const double dx = telemetry.posX - g_state.lastPosX;
            const double dy = telemetry.posY - g_state.lastPosY;
            const double dist = std::sqrt(dx * dx + dy * dy);
            if (dist > 0.05 && dist < 25.0) {
                if (telemetry.onSkate) {
                    g_state.skateDistance += dist;
                } else if (onBike) {
                    g_state.bikeDistance += dist;
                } else {
                    g_state.footDistance += dist;
                }
                customChanged = true;
            }
        }
        g_state.lastPosX = telemetry.posX;
        g_state.lastPosY = telemetry.posY;
        g_state.lastPosValid = true;
    }

    if (telemetry.stat16 != g_state.lastStat16
        || telemetry.pdiff16 != g_state.lastPdiff16
        || telemetry.swirlieActive != g_state.lastSwirlieActive) {
        Log("Swirlie signal: stat16=%d pdiff16=%d active=%d total=%d",
            telemetry.stat16, telemetry.pdiff16, telemetry.swirlieActive ? 1 : 0, g_state.swirliesTotal);
    }

    if (g_state.lastPdiff16 < 0) {
        g_state.lastPdiff16 = telemetry.pdiff16;
        g_state.lastStat16 = telemetry.stat16;
        g_state.lastSwirlieActive = telemetry.swirlieActive;
    }

    if (telemetry.stat16 > g_state.lastStat16) {
        g_state.swirliesTotal += telemetry.stat16 - g_state.lastStat16;
        customChanged = true;
    }
    if (telemetry.pdiff16 > g_state.lastPdiff16) {
        g_state.swirliesTotal += telemetry.pdiff16 - g_state.lastPdiff16;
        customChanged = true;
    } else if (telemetry.swirlieActive && !g_state.lastSwirlieActive && telemetry.stat16 <= g_state.lastStat16) {
        ++g_state.swirliesTotal;
        customChanged = true;
    }
    g_state.lastStat16 = g_state.lastStat16 > telemetry.stat16 ? g_state.lastStat16 : telemetry.stat16;
    g_state.lastPdiff16 = telemetry.pdiff16;
    g_state.lastSwirlieActive = telemetry.swirlieActive;

    if (telemetry.pdiff31 > g_state.lastPdiff31) {
        g_state.eggCarsTotal += telemetry.pdiff31 - g_state.lastPdiff31;
        customChanged = true;
        Log("Egg counter advanced: pdiff31=%d total=%d", telemetry.pdiff31, g_state.eggCarsTotal);
    }
    g_state.lastPdiff31 = telemetry.pdiff31;

    if (telemetry.stat28 != g_state.lastStat28 || telemetry.stat29 != g_state.lastStat29 ||
        telemetry.stat30 != g_state.lastStat30 || telemetry.stat31 != g_state.lastStat31Observed ||
        telemetry.stat32 != g_state.lastStat32 || telemetry.stat33 != g_state.lastStat33 ||
        telemetry.stat34 != g_state.lastStat34Observed || telemetry.stat35 != g_state.lastStat35) {
        Log("Stat block changed: 28=%d 29=%d 30=%d 31=%d 32=%d 33=%d 34=%d 35=%d",
            telemetry.stat28, telemetry.stat29, telemetry.stat30, telemetry.stat31,
            telemetry.stat32, telemetry.stat33, telemetry.stat34, telemetry.stat35);
        g_state.lastStat28 = telemetry.stat28;
        g_state.lastStat29 = telemetry.stat29;
        g_state.lastStat30 = telemetry.stat30;
        g_state.lastStat31Observed = telemetry.stat31;
        g_state.lastStat32 = telemetry.stat32;
        g_state.lastStat33 = telemetry.stat33;
        g_state.lastStat34Observed = telemetry.stat34;
        g_state.lastStat35 = telemetry.stat35;
    }

    if (telemetry.pdiff28 != g_state.lastPdiff28 || telemetry.pdiff29 != g_state.lastPdiff29 ||
        telemetry.pdiff30 != g_state.lastPdiff30 || telemetry.pdiff31 != g_state.lastPdiff31 ||
        telemetry.pdiff32 != g_state.lastPdiff32 || telemetry.pdiff33 != g_state.lastPdiff33 ||
        telemetry.pdiff34 != g_state.lastPdiff34 || telemetry.pdiff35 != g_state.lastPdiff35) {
        Log("Principal diff changed: 28=%d 29=%d 30=%d 31=%d 32=%d 33=%d 34=%d 35=%d",
            telemetry.pdiff28, telemetry.pdiff29, telemetry.pdiff30, telemetry.pdiff31,
            telemetry.pdiff32, telemetry.pdiff33, telemetry.pdiff34, telemetry.pdiff35);
        g_state.lastPdiff28 = telemetry.pdiff28;
        g_state.lastPdiff29 = telemetry.pdiff29;
        g_state.lastPdiff30 = telemetry.pdiff30;
        g_state.lastPdiff31 = telemetry.pdiff31;
        g_state.lastPdiff32 = telemetry.pdiff32;
        g_state.lastPdiff33 = telemetry.pdiff33;
        g_state.lastPdiff34 = telemetry.pdiff34;
        g_state.lastPdiff35 = telemetry.pdiff35;
    }

    if (customChanged) {
        g_state.stateDirty = true;
    }
}

static void EvaluateAchievements(const Telemetry& telemetry) {
    if (!telemetry.runtimeReady) {
        return;
    }

    for (std::size_t i = 0; i < g_state.defs.size(); ++i) {
        if (g_state.unlocked[i]) {
            continue;
        }
        const AchievementDef& def = g_state.defs[i];
        if (EvaluateDefinition(def, telemetry)) {
            QueueUnlock(i);
        }
    }
}

static void PrimeAchievementsFromTelemetry(const Telemetry& telemetry) {
    if (!telemetry.runtimeReady) {
        return;
    }

    bool changed = false;
    for (std::size_t i = 0; i < g_state.defs.size(); ++i) {
        if (g_state.unlocked[i]) {
            continue;
        }
        const AchievementDef& def = g_state.defs[i];
        if (EvaluateDefinition(def, telemetry)) {
            g_state.unlocked[i] = true;
            changed = true;
            Log("Primed %s", def.id.c_str());
        }
    }

    g_state.initialSyncComplete = true;
    g_state.popupQueue.clear();
    g_state.activePopupIndex = -1;
    if (changed) {
        g_state.stateDirty = true;
    }
}

static void ProcessPopupQueue(const Telemetry& telemetry) {
    const DWORD now = GetTickCount();
    if (g_state.activePopupIndex >= 0) {
        if (now - g_state.popupStartTick >= static_cast<DWORD>(g_cfg.popupDurationMs)) {
            Log("Popup finished for %s",
                g_state.defs[static_cast<std::size_t>(g_state.activePopupIndex)].id.c_str());
            g_state.activePopupIndex = -1;
            g_state.popupNextAllowedTick = now + static_cast<DWORD>(g_cfg.popupGapMs);
            g_state.popupDirty = true;
        }
    }
    if (g_state.activePopupIndex >= 0) {
        return;
    }
    if (now < g_state.popupNextAllowedTick) {
        return;
    }
    if (!g_cfg.popupDuringMission && telemetry.missionActive) {
        return;
    }
    if (g_state.popupQueue.empty()) {
        return;
    }

    g_state.activePopupIndex = g_state.popupQueue.front();
    g_state.popupQueue.pop_front();
    g_state.popupStartTick = now;
    ++g_state.popupNonce;
    g_state.popupDirty = true;
    PlayPopupMp3();
    Log("Popup started for %s (nonce=%d)",
        g_state.defs[static_cast<std::size_t>(g_state.activePopupIndex)].id.c_str(),
        g_state.popupNonce);
}

static void BootstrapStateFiles() {
    Telemetry emptyTelemetry;
    WriteStateFile(emptyTelemetry);
    WritePopupFile();
    SavePersistentState();
}

static DWORD WINAPI InitThreadProc(void*) {
    LoadConfig();
    Log("AchievementsASI starting");
    Log("Module path: %s", g_cfg.modulePath);
    Log("Data dir: %s", g_cfg.dataDir);
    Log("INI path: %s", g_cfg.iniPath);
    Log("Enable=%d VerboseLogging=%d", g_cfg.enable ? 1 : 0, g_cfg.verboseLogging ? 1 : 0);

    if (!g_cfg.enable) {
        Log("Module disabled");
        return 0;
    }

    BuildDefinitions();
    LoadRaritySnapshot();
    LoadPersistentState();
    BootstrapStateFiles();
    Log("Definitions loaded: %u", static_cast<unsigned>(g_state.defs.size()));

    while (true) {
        Telemetry telemetry = ReadTelemetry();
        if (telemetry.valid && telemetry.updated != g_state.lastTelemetryUpdated) {
            if (g_state.lastTelemetryUpdated < 0) {
                Log("Telemetry online: updated=%d runtimeReady=%d missionActive=%d cutsceneActive=%d stat16=%d pdiff16=%d gameCompletion=%d",
                    telemetry.updated,
                    telemetry.runtimeReady ? 1 : 0,
                    telemetry.missionActive ? 1 : 0,
                    telemetry.cutsceneActive ? 1 : 0,
                    telemetry.stat16,
                    telemetry.pdiff16,
                    telemetry.gameCompletion);
                Log("Initial stat block: 28=%d 29=%d 30=%d 31=%d 32=%d 33=%d 34=%d 35=%d",
                    telemetry.stat28, telemetry.stat29, telemetry.stat30, telemetry.stat31,
                    telemetry.stat32, telemetry.stat33, telemetry.stat34, telemetry.stat35);
                Log("Initial principal diff: 28=%d 29=%d 30=%d 31=%d 32=%d 33=%d 34=%d 35=%d",
                    telemetry.pdiff28, telemetry.pdiff29, telemetry.pdiff30, telemetry.pdiff31,
                    telemetry.pdiff32, telemetry.pdiff33, telemetry.pdiff34, telemetry.pdiff35);
                g_state.lastStat28 = telemetry.stat28;
                g_state.lastStat29 = telemetry.stat29;
                g_state.lastStat30 = telemetry.stat30;
                g_state.lastStat31Observed = telemetry.stat31;
                g_state.lastStat32 = telemetry.stat32;
                g_state.lastStat33 = telemetry.stat33;
                g_state.lastStat34Observed = telemetry.stat34;
                g_state.lastStat35 = telemetry.stat35;
                g_state.lastPdiff28 = telemetry.pdiff28;
                g_state.lastPdiff29 = telemetry.pdiff29;
                g_state.lastPdiff30 = telemetry.pdiff30;
                g_state.lastPdiff32 = telemetry.pdiff32;
                g_state.lastPdiff33 = telemetry.pdiff33;
                g_state.lastPdiff34 = telemetry.pdiff34;
                g_state.lastPdiff35 = telemetry.pdiff35;
            }
            g_state.lastTelemetryUpdated = telemetry.updated;
            UpdateCustomCounters(telemetry);
            if (!g_state.initialSyncComplete) {
                PrimeAchievementsFromTelemetry(telemetry);
            } else {
                EvaluateAchievements(telemetry);
                ProcessPopupQueue(telemetry);
            }
            WriteStateFile(telemetry);
            if (g_state.stateDirty) {
                SavePersistentState();
                g_state.stateDirty = false;
            }
            if (g_state.popupDirty) {
                WritePopupFile();
                g_state.popupDirty = false;
            }
        } else {
            ProcessPopupQueue(telemetry);
            if (g_state.popupDirty) {
                WritePopupFile();
                g_state.popupDirty = false;
            }
        }

        Sleep(g_cfg.backendPollMs > 50 ? g_cfg.backendPollMs : 50);
    }
}

} // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        InitializePaths(hModule);
        g_cfg.initThread = CreateThread(nullptr, 0, InitThreadProc, nullptr, 0, nullptr);
        if (g_cfg.initThread) {
            CloseHandle(g_cfg.initThread);
            g_cfg.initThread = nullptr;
        }
    }
    return TRUE;
}

