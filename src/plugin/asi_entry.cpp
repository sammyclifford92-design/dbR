#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include "core/ki_meter.hpp"
#include "native_caller.hpp"
#include "script_hook_api.hpp"

extern "C" __declspec(dllexport) void InitializeASI();

namespace {

constexpr UINT64 kPlayerPedId = 0x096275889B8E0EE0;
constexpr UINT64 kDoesEntityExist = 0xD42BD6EB2E0F1677;
constexpr UINT64 kGetEntityCoords = 0xA86D5F069399F44D;
constexpr UINT64 kGetEntityForwardVector = 0x2412D9C05BB09B97;
constexpr UINT64 kAddExplosion = 0x7D6F58F69DA92530;
constexpr UINT64 kDrawRect = 0x405224591DF02025;
constexpr UINT64 kNetworkIsSessionActive = 0xD83C2B94E7508980;
constexpr UINT64 kPlayerId = 0x217E9DC48139933D;
constexpr UINT64 kGetSafeCoordForPed = 0xB61C8E878A4199CA;
constexpr UINT64 kSetEntityCoordsNoOffset = 0x239A3351AC1DA385;
constexpr UINT64 kGetEntityPlayerIsFreeAimingAt = 0xA6817C110B830EAD;
constexpr UINT64 kSetPedMoveRateOverride = 0x085BF80FA50A39D1;
constexpr UINT64 kSetPlayerWeaponDamageModifier = 0x94D529F7B73D7A85;
constexpr UINT64 kIsPedHuman = 0xB980061DA992779D;
constexpr UINT64 kIsPedDeadOrDying = 0x3317DEDB88C95038;
constexpr UINT64 kIsPedIncapacitated = 0xB655DB7582AEC805;
constexpr UINT64 kGetRelationshipBetweenPeds = 0xEBA5AD3A0EAF7121;
constexpr UINT64 kSetMetaPedTag = 0xBC6DF00D7A4A6819;
constexpr UINT64 kRemoveTagFromMetaPed = 0xD710A5007C2AC539;
constexpr UINT64 kGetNumComponentsInPed = 0x90403E8107B60E81;
constexpr UINT64 kGetCategoryOfComponentAtIndex = 0x9B90842304C938A7;
constexpr UINT64 kUpdatePedVariation = 0xCC8CA3E88256E58F;
constexpr UINT64 kGetMetaPedAssetGuids = 0xA9C28516A6DC9D56;
constexpr UINT64 kGetMetaPedAssetTint = 0xE7998FEC53A33BBE;

constexpr Hash joaat(const char* text) {
    Hash hash = 0;
    while (*text != '\0') {
        char character = *text++;
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character + ('a' - 'A'));
        }
        hash += static_cast<Hash>(character);
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

constexpr Hash kHairCategory = joaat("HAIR");
constexpr std::array<Hash, 17> kFusionClothingCategories{
    joaat("COATS"), joaat("VESTS"), joaat("PANTS"), joaat("SHIRTS_FULL"),
    joaat("BOOTS"), joaat("HATS"), joaat("NECKWEAR"), joaat("GLOVES"),
    joaat("CHAPS"), joaat("PONCHOS"), joaat("GAUNTLETS"), joaat("DRESSES"),
    joaat("SKIRTS"), joaat("SUSPENDERS"), joaat("EYEWEAR"), joaat("CLOAKS"),
    joaat("ACCESSORIES"),
};

HMODULE pluginModule = nullptr;
std::atomic_bool scriptRegistered = false;

struct ModConfig {
    int chargeKey = VK_F8;
    int blastKey = VK_F9;
    int kameKey = VK_F10;
    int vanishKey = VK_F11;
    int ssjKey = VK_F12;
    int maximumKi = 100;
    int initialKi = 25;
    int chargePerSecond = 30;
    int regenerationPerSecond = 8;
    int regenerationDelayMilliseconds = 2000;
    int blastCost = 25;
    int blastRangeMeters = 12;
    int blastDamagePercent = 25;
    int kameCost = 50;
    int kameRangeMeters = 30;
    int kameDamagePercent = 100;
    int kameMinimumChargeMilliseconds = 600;
    int kameMaximumChargeMilliseconds = 2500;
    int vanishCost = 20;
    int vanishDistanceMeters = 8;
    int ssjActivationCost = 20;
    int ssjSpeedPercent = 130;
    int ssjDamagePercent = 130;
    int ssjDrainPerSecond = 5;
    int fusionKey = VK_F7;
    int fusionActivationCost = 40;
    int fusionRangeMeters = 4;
    int fusionDurationSeconds = 30;
    int fusionSpeedPercent = 125;
    int fusionDamagePercent = 150;
    int fusionDrainPerSecond = 2;
    int explosionType = 0;
};

struct MetaTag {
    Hash category = 0;
    Hash drawable = 0;
    Hash albedo = 0;
    Hash normal = 0;
    Hash material = 0;
    Hash palette = 0;
    int tint0 = 0;
    int tint1 = 0;
    int tint2 = 0;
};

struct FusionState {
    bool active = false;
    Ped playerPed = 0;
    std::chrono::steady_clock::time_point expiresAt{};
    std::vector<Hash> changedCategories;
    std::vector<MetaTag> originalTags;
};

struct StatMultipliers {
    float speed = 1.0F;
    float damage = 1.0F;
};

std::wstring pluginDirectory() {
    if (pluginModule == nullptr) {
        return L".";
    }

    std::array<wchar_t, 32768> modulePath{};
    const DWORD pathLength = GetModuleFileNameW(
        pluginModule,
        modulePath.data(),
        static_cast<DWORD>(modulePath.size()));
    if (pathLength == 0 || pathLength >= modulePath.size()) {
        return L".";
    }

    std::wstring result(modulePath.data(), pathLength);
    const auto separator = result.find_last_of(L"\\/");
    if (separator != std::wstring::npos) {
        result.resize(separator + 1);
    } else {
        result = L".\\";
    }
    return result;
}

std::wstring filePath(const wchar_t* filename) {
    return pluginDirectory() + filename;
}

void appendLog(std::string_view message) {
    OutputDebugStringA(message.data());

    const std::wstring path = filePath(L"SaiyanInTheOldWest.log");
    HANDLE file = CreateFileW(
        path.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, message.data(), static_cast<DWORD>(message.size()), &written, nullptr);
    CloseHandle(file);
}

int readSetting(const wchar_t* section, const wchar_t* name, int fallback, int minimum, int maximum) {
    const UINT value = GetPrivateProfileIntW(
        section,
        name,
        static_cast<INT>(fallback),
        filePath(L"SaiyanInTheOldWest.ini").c_str());
    return std::clamp(static_cast<int>(value), minimum, maximum);
}

ModConfig readConfig() {
    ModConfig config;
    config.chargeKey = readSetting(L"Controls", L"ChargeKey", VK_F8, 1, 255);
    config.blastKey = readSetting(L"Controls", L"BlastKey", VK_F9, 1, 255);
    config.kameKey = readSetting(L"Controls", L"KameKey", VK_F10, 1, 255);
    config.vanishKey = readSetting(L"Controls", L"VanishKey", VK_F11, 1, 255);
    config.ssjKey = readSetting(L"Controls", L"SuperSaiyanKey", VK_F12, 1, 255);
    config.maximumKi = readSetting(L"Ki", L"Maximum", 100, 10, 1000);
    config.initialKi = readSetting(L"Ki", L"Initial", 25, 0, config.maximumKi);
    config.chargePerSecond = readSetting(L"Ki", L"ChargePerSecond", 30, 1, 500);
    config.regenerationPerSecond = readSetting(L"Ki", L"RegenerationPerSecond", 8, 0, 200);
    config.regenerationDelayMilliseconds = readSetting(L"Ki", L"RegenerationDelayMilliseconds", 2000, 0, 30000);
    config.blastCost = readSetting(L"Blast", L"Cost", 25, 1, config.maximumKi);
    config.blastRangeMeters = readSetting(L"Blast", L"RangeMeters", 12, 2, 50);
    config.blastDamagePercent = readSetting(L"Blast", L"DamagePercent", 25, 5, 100);
    config.kameCost = readSetting(L"Kame", L"Cost", 50, 1, config.maximumKi);
    config.kameRangeMeters = readSetting(L"Kame", L"RangeMeters", 30, 5, 100);
    config.kameDamagePercent = readSetting(L"Kame", L"DamagePercent", 100, 25, 500);
    config.kameMinimumChargeMilliseconds = readSetting(L"Kame", L"MinimumChargeMilliseconds", 600, 100, 10000);
    config.kameMaximumChargeMilliseconds = readSetting(L"Kame", L"MaximumChargeMilliseconds", 2500, config.kameMinimumChargeMilliseconds, 15000);
    config.vanishCost = readSetting(L"Vanish", L"Cost", 20, 1, config.maximumKi);
    config.vanishDistanceMeters = readSetting(L"Vanish", L"DistanceMeters", 8, 2, 20);
    config.ssjActivationCost = readSetting(L"SuperSaiyan", L"ActivationCost", 20, 1, config.maximumKi);
    config.ssjSpeedPercent = readSetting(L"SuperSaiyan", L"SpeedPercent", 130, 100, 200);
    config.ssjDamagePercent = readSetting(L"SuperSaiyan", L"DamagePercent", 130, 100, 300);
    config.ssjDrainPerSecond = readSetting(L"SuperSaiyan", L"KiDrainPerSecond", 5, 1, 100);
    config.fusionKey = readSetting(L"Controls", L"FusionKey", VK_F7, 1, 255);
    config.fusionActivationCost = readSetting(L"Fusion", L"ActivationCost", 40, 1, config.maximumKi);
    config.fusionRangeMeters = readSetting(L"Fusion", L"RangeMeters", 4, 2, 10);
    config.fusionDurationSeconds = readSetting(L"Fusion", L"DurationSeconds", 30, 5, 180);
    config.fusionSpeedPercent = readSetting(L"Fusion", L"SpeedPercent", 125, 100, 200);
    config.fusionDamagePercent = readSetting(L"Fusion", L"DamagePercent", 150, 100, 300);
    config.fusionDrainPerSecond = readSetting(L"Fusion", L"KiDrainPerSecond", 2, 1, 100);
    return config;
}

bool gameWindowHasFocus() {
    const HWND window = GetForegroundWindow();
    if (window == nullptr) {
        return false;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    return processId == GetCurrentProcessId();
}

bool keyIsDown(int virtualKey) {
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

void drawKiBar(const dbr::KiMeter& meter, bool transformed) {
    constexpr float barWidth = 0.32F;
    constexpr float barHeight = 0.018F;
    constexpr float barCenterX = 0.5F;
    constexpr float barCenterY = 0.93F;
    constexpr float barLeft = barCenterX - barWidth * 0.5F;

    invokeNative<Void>(kDrawRect, barCenterX, barCenterY, barWidth, barHeight,
        12, 17, 23, 210, FALSE, FALSE);

    const float ratio = std::clamp(meter.value() / meter.maximum(), 0.0F, 1.0F);
    if (ratio <= 0.0F) {
        return;
    }

    const float fillWidth = (barWidth - 0.004F) * ratio;
    const float fillCenter = barLeft + 0.002F + fillWidth * 0.5F;
    const int red = transformed ? 255 : 63;
    const int green = transformed ? 207 : 199;
    const int blue = transformed ? 64 : 218;
    invokeNative<Void>(kDrawRect, fillCenter, barCenterY, fillWidth, barHeight - 0.004F,
        red, green, blue, 240, FALSE, FALSE);
}

void fireKiBlast(Ped playerPed, const ModConfig& config) {
    const Vector3 position = invokeNative<Vector3>(kGetEntityCoords, playerPed, TRUE, FALSE);
    const Vector3 direction = invokeNative<Vector3>(kGetEntityForwardVector, playerPed);
    const float distance = static_cast<float>(config.blastRangeMeters);

    invokeNative<Void>(kAddExplosion,
        position.x + direction.x * distance,
        position.y + direction.y * distance,
        position.z + direction.z * distance + 0.5F,
        config.explosionType,
        static_cast<float>(config.blastDamagePercent) / 100.0F,
        TRUE,
        FALSE,
        0.15F);
}

void fireKame(Ped playerPed, const ModConfig& config, double chargeSeconds) {
    const Vector3 position = invokeNative<Vector3>(kGetEntityCoords, playerPed, TRUE, FALSE);
    const Vector3 direction = invokeNative<Vector3>(kGetEntityForwardVector, playerPed);
    const double maximumCharge = static_cast<double>(config.kameMaximumChargeMilliseconds) / 1000.0;
    const float chargeRatio = static_cast<float>(std::clamp(chargeSeconds / maximumCharge, 0.0, 1.0));
    const float damageScale = static_cast<float>(config.kameDamagePercent) / 100.0F * (0.5F + chargeRatio * 0.5F);
    const float distance = static_cast<float>(config.kameRangeMeters);

    invokeNative<Void>(kAddExplosion,
        position.x + direction.x * distance,
        position.y + direction.y * distance,
        position.z + direction.z * distance + 0.5F,
        config.explosionType,
        damageScale,
        TRUE,
        FALSE,
        0.35F + chargeRatio * 0.65F);
}

bool vanish(Ped playerPed, const ModConfig& config) {
    Vector3 destination{};
    Entity aimedEntity = 0;
    const Player player = invokeNative<Player>(kPlayerId);
    const bool isAimingAtEntity = invokeNative<BOOL>(kGetEntityPlayerIsFreeAimingAt, player, &aimedEntity) &&
        invokeNative<BOOL>(kDoesEntityExist, aimedEntity);

    if (isAimingAtEntity) {
        const Vector3 playerPosition = invokeNative<Vector3>(kGetEntityCoords, playerPed, TRUE, FALSE);
        const Vector3 targetPosition = invokeNative<Vector3>(kGetEntityCoords, aimedEntity, TRUE, FALSE);
        float towardPlayerX = playerPosition.x - targetPosition.x;
        float towardPlayerY = playerPosition.y - targetPosition.y;
        const float length = std::sqrt(towardPlayerX * towardPlayerX + towardPlayerY * towardPlayerY);
        if (length > 0.001F) {
            towardPlayerX /= length;
            towardPlayerY /= length;
        }
        destination = {
            targetPosition.x + towardPlayerX * 2.0F,
            targetPosition.y + towardPlayerY * 2.0F,
            targetPosition.z,
        };
    } else {
        const Vector3 position = invokeNative<Vector3>(kGetEntityCoords, playerPed, TRUE, FALSE);
        const Vector3 direction = invokeNative<Vector3>(kGetEntityForwardVector, playerPed);
        const float distance = static_cast<float>(config.vanishDistanceMeters);
        destination = {
            position.x + direction.x * distance,
            position.y + direction.y * distance,
            position.z + direction.z * distance,
        };
    }

    Vector3 safePosition{};
    if (!invokeNative<BOOL>(kGetSafeCoordForPed,
            destination.x, destination.y, destination.z, TRUE, &safePosition, 0)) {
        return false;
    }

    invokeNative<Void>(kSetEntityCoordsNoOffset,
        playerPed, safePosition.x, safePosition.y, safePosition.z, FALSE, FALSE, TRUE);
    return true;
}

int componentIndexInCategory(Ped ped, Hash category) {
    const int count = std::clamp(invokeNative<int>(kGetNumComponentsInPed, ped), 0, 128);
    for (int index = 0; index < count; ++index) {
        if (invokeNative<Hash>(kGetCategoryOfComponentAtIndex, ped, index, 0) == category) {
            return index;
        }
    }
    return -1;
}

bool readMetaTag(Ped ped, Hash category, MetaTag& tag) {
    const int index = componentIndexInCategory(ped, category);
    if (index < 0) {
        return false;
    }

    tag.category = category;
    return invokeNative<BOOL>(kGetMetaPedAssetGuids, ped, index,
               &tag.drawable, &tag.albedo, &tag.normal, &tag.material) &&
        invokeNative<BOOL>(kGetMetaPedAssetTint, ped, index,
               &tag.palette, &tag.tint0, &tag.tint1, &tag.tint2);
}

void applyMetaTag(Ped ped, const MetaTag& tag) {
    invokeNative<Void>(kSetMetaPedTag, ped, tag.drawable, tag.albedo, tag.normal,
        tag.material, tag.palette, tag.tint0, tag.tint1, tag.tint2);
}

bool applyFusionAppearance(Ped playerPed, Ped donorPed, FusionState& fusion) {
    std::vector<Hash> categories{kHairCategory};
    const size_t startIndex = static_cast<size_t>(donorPed) % kFusionClothingCategories.size();
    for (size_t offset = 0; offset < kFusionClothingCategories.size() && categories.size() < 3; ++offset) {
        const Hash category = kFusionClothingCategories[(startIndex + offset) % kFusionClothingCategories.size()];
        if (componentIndexInCategory(playerPed, category) >= 0 &&
            componentIndexInCategory(donorPed, category) >= 0) {
            categories.push_back(category);
        }
    }
    if (categories.size() < 2 || componentIndexInCategory(playerPed, kHairCategory) < 0 ||
        componentIndexInCategory(donorPed, kHairCategory) < 0) {
        return false;
    }

    std::vector<MetaTag> originalTags;
    std::vector<MetaTag> donorTags;
    for (Hash category : categories) {
        MetaTag original;
        MetaTag donor;
        if (!readMetaTag(playerPed, category, original) || !readMetaTag(donorPed, category, donor)) {
            return false;
        }
        originalTags.push_back(original);
        donorTags.push_back(donor);
    }

    fusion.playerPed = playerPed;
    fusion.changedCategories = categories;
    fusion.originalTags = originalTags;
    for (size_t index = 0; index < categories.size(); ++index) {
        invokeNative<Void>(kRemoveTagFromMetaPed, playerPed, categories[index], 0);
        applyMetaTag(playerPed, donorTags[index]);
    }
    invokeNative<Void>(kUpdatePedVariation, playerPed, FALSE, TRUE, TRUE, TRUE, FALSE);
    return true;
}

void restoreFusionAppearance(FusionState& fusion) {
    if (fusion.playerPed != 0 && invokeNative<BOOL>(kDoesEntityExist, fusion.playerPed)) {
        for (Hash category : fusion.changedCategories) {
            invokeNative<Void>(kRemoveTagFromMetaPed, fusion.playerPed, category, 0);
        }
        for (const MetaTag& tag : fusion.originalTags) {
            applyMetaTag(fusion.playerPed, tag);
        }
        invokeNative<Void>(kUpdatePedVariation, fusion.playerPed, FALSE, TRUE, TRUE, TRUE, FALSE);
    }
    fusion = {};
}

Ped findFusionTarget(Ped playerPed, const ModConfig& config) {
    std::array<int, 512> peds{};
    const int count = std::clamp(scriptHookApi().worldGetAllPeds(peds.data(), static_cast<int>(peds.size())), 0,
        static_cast<int>(peds.size()));
    const Vector3 playerPosition = invokeNative<Vector3>(kGetEntityCoords, playerPed, TRUE, FALSE);
    const float maximumDistance = static_cast<float>(config.fusionRangeMeters);
    const float maximumDistanceSquared = maximumDistance * maximumDistance;
    float nearestDistanceSquared = maximumDistanceSquared;
    Ped nearestPed = 0;

    for (int index = 0; index < count; ++index) {
        const Ped candidate = peds[index];
        if (candidate == playerPed || !invokeNative<BOOL>(kDoesEntityExist, candidate) ||
            !invokeNative<BOOL>(kIsPedHuman, candidate)) {
            continue;
        }

        const bool dead = invokeNative<BOOL>(kIsPedDeadOrDying, candidate, FALSE);
        const bool incapacitated = invokeNative<BOOL>(kIsPedIncapacitated, candidate) || dead;
        const int relationship = invokeNative<int>(kGetRelationshipBetweenPeds, playerPed, candidate);
        const bool friendlyOrStranger = relationship >= 0 && relationship <= 3 && !dead;
        const bool defeatedHostile = relationship >= 4 && incapacitated;
        if (!friendlyOrStranger && !defeatedHostile) {
            continue;
        }

        const Vector3 position = invokeNative<Vector3>(kGetEntityCoords, candidate, TRUE, FALSE);
        const float dx = position.x - playerPosition.x;
        const float dy = position.y - playerPosition.y;
        const float dz = position.z - playerPosition.z;
        const float distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared < nearestDistanceSquared) {
            nearestDistanceSquared = distanceSquared;
            nearestPed = candidate;
        }
    }
    return nearestPed;
}

StatMultipliers statMultipliers(const ModConfig& config, bool ssjEnabled, bool fusionActive) {
    StatMultipliers multipliers;
    if (ssjEnabled) {
        multipliers.speed *= static_cast<float>(config.ssjSpeedPercent) / 100.0F;
        multipliers.damage *= static_cast<float>(config.ssjDamagePercent) / 100.0F;
    }
    if (fusionActive) {
        multipliers.speed *= static_cast<float>(config.fusionSpeedPercent) / 100.0F;
        multipliers.damage *= static_cast<float>(config.fusionDamagePercent) / 100.0F;
    }
    return multipliers;
}

void applyStatMultipliers(Ped playerPed, Player player, const StatMultipliers& multipliers) {
    invokeNative<Void>(kSetPedMoveRateOverride, playerPed, multipliers.speed);
    invokeNative<Void>(kSetPlayerWeaponDamageModifier, player, multipliers.damage);
}

void ScriptMain() {
    const ModConfig config = readConfig();
    const dbr::KiTuning tuning{
        static_cast<float>(config.maximumKi),
        static_cast<float>(config.regenerationPerSecond),
        static_cast<double>(config.regenerationDelayMilliseconds) / 1000.0,
    };
    dbr::KiMeter ki(tuning, static_cast<float>(config.initialKi));
    auto lastTick = std::chrono::steady_clock::now();
    bool previousBlastKeyDown = false;
    bool previousKameKeyDown = false;
    bool previousVanishKeyDown = false;
    bool previousSsjKeyDown = false;
    bool previousFusionKeyDown = false;
    bool ssjEnabled = false;
    FusionState fusion;
    StatMultipliers appliedMultipliers;
    auto kameChargeStarted = lastTick;

    appendLog("Gameplay loop started. F7 fusion, F8 charge, F9 blast, hold/release F10 super, F11 vanish, F12 transform.\r\n");
    while (true) {
        const auto now = std::chrono::steady_clock::now();
        const double elapsed = std::clamp(
            std::chrono::duration<double>(now - lastTick).count(), 0.0, 0.1);
        lastTick = now;

        if (invokeNative<BOOL>(kNetworkIsSessionActive)) {
            if (fusion.active) {
                restoreFusionAppearance(fusion);
            }
            if (ssjEnabled || appliedMultipliers.speed != 1.0F || appliedMultipliers.damage != 1.0F) {
                const Ped currentPed = invokeNative<Ped>(kPlayerPedId);
                const Player currentPlayer = invokeNative<Player>(kPlayerId);
                if (invokeNative<BOOL>(kDoesEntityExist, currentPed)) {
                    applyStatMultipliers(currentPed, currentPlayer, {});
                } else {
                    invokeNative<Void>(kSetPlayerWeaponDamageModifier, currentPlayer, 1.0F);
                }
                ssjEnabled = false;
                appliedMultipliers = {};
            }
            previousBlastKeyDown = false;
            previousKameKeyDown = false;
            previousVanishKeyDown = false;
            previousSsjKeyDown = false;
            previousFusionKeyDown = false;
            scriptHookApi().scriptWait(250);
            continue;
        }

        const Ped playerPed = invokeNative<Ped>(kPlayerPedId);
        if (!invokeNative<BOOL>(kDoesEntityExist, playerPed)) {
            if (fusion.active) {
                restoreFusionAppearance(fusion);
            }
            if (appliedMultipliers.damage != 1.0F) {
                invokeNative<Void>(kSetPlayerWeaponDamageModifier, invokeNative<Player>(kPlayerId), 1.0F);
            }
            appliedMultipliers = {};
            ssjEnabled = false;
            previousBlastKeyDown = false;
            previousKameKeyDown = false;
            previousVanishKeyDown = false;
            previousSsjKeyDown = false;
            previousFusionKeyDown = false;
            scriptHookApi().scriptWait(100);
            continue;
        }

        if (fusion.active && now >= fusion.expiresAt) {
            restoreFusionAppearance(fusion);
        }

        const bool focused = gameWindowHasFocus();
        if (!focused) {
            previousBlastKeyDown = false;
            previousKameKeyDown = false;
            previousVanishKeyDown = false;
            previousSsjKeyDown = false;
            previousFusionKeyDown = false;
        }
        const bool chargeKeyDown = focused && keyIsDown(config.chargeKey);
        if (chargeKeyDown) {
            ki.charge(static_cast<float>(elapsed * config.chargePerSecond));
        } else {
            ki.advance(elapsed);
        }

        const bool blastKeyDown = focused && keyIsDown(config.blastKey);
        if (blastKeyDown && !previousBlastKeyDown && ki.trySpend(static_cast<float>(config.blastCost))) {
            fireKiBlast(playerPed, config);
        }
        previousBlastKeyDown = blastKeyDown;

        const bool kameKeyDown = focused && keyIsDown(config.kameKey);
        if (kameKeyDown && !previousKameKeyDown) {
            kameChargeStarted = now;
        } else if (!kameKeyDown && previousKameKeyDown) {
            const double chargeSeconds = std::chrono::duration<double>(now - kameChargeStarted).count();
            const int chargeMilliseconds = static_cast<int>(chargeSeconds * 1000.0);
            if (chargeMilliseconds >= config.kameMinimumChargeMilliseconds &&
                ki.trySpend(static_cast<float>(config.kameCost))) {
                fireKame(playerPed, config, std::min(chargeSeconds,
                    static_cast<double>(config.kameMaximumChargeMilliseconds) / 1000.0));
            }
        }
        previousKameKeyDown = kameKeyDown;

        const bool vanishKeyDown = focused && keyIsDown(config.vanishKey);
        if (vanishKeyDown && !previousVanishKeyDown &&
            ki.value() >= config.vanishCost && vanish(playerPed, config)) {
            ki.trySpend(static_cast<float>(config.vanishCost));
        }
        previousVanishKeyDown = vanishKeyDown;

        const bool ssjKeyDown = focused && keyIsDown(config.ssjKey);
        if (ssjKeyDown && !previousSsjKeyDown) {
            if (ssjEnabled) {
                ssjEnabled = false;
            } else if (ki.trySpend(static_cast<float>(config.ssjActivationCost))) {
                ssjEnabled = true;
            }
        }
        previousSsjKeyDown = ssjKeyDown;

        const bool fusionKeyDown = focused && keyIsDown(config.fusionKey);
        if (fusionKeyDown && !previousFusionKeyDown) {
            if (fusion.active) {
                restoreFusionAppearance(fusion);
                appendLog("Fusion ended by player.\r\n");
            } else if (ki.value() >= config.fusionActivationCost) {
                const Ped donorPed = findFusionTarget(playerPed, config);
                if (donorPed != 0 && applyFusionAppearance(playerPed, donorPed, fusion)) {
                    if (ki.trySpend(static_cast<float>(config.fusionActivationCost))) {
                        fusion.active = true;
                        fusion.expiresAt = now + std::chrono::seconds(config.fusionDurationSeconds);
                        appendLog("Fusion started with nearby human.\r\n");
                    } else {
                        restoreFusionAppearance(fusion);
                    }
                } else {
                    appendLog("Fusion failed: no eligible nearby human with compatible hair/clothing.\r\n");
                }
            }
        }
        previousFusionKeyDown = fusionKeyDown;

        const Player player = invokeNative<Player>(kPlayerId);
        const float drainPerSecond =
            (ssjEnabled ? static_cast<float>(config.ssjDrainPerSecond) : 0.0F) +
            (fusion.active ? static_cast<float>(config.fusionDrainPerSecond) : 0.0F);
        if (drainPerSecond > 0.0F && !ki.trySpend(elapsed * drainPerSecond)) {
            ssjEnabled = false;
            if (fusion.active) {
                restoreFusionAppearance(fusion);
                appendLog("Fusion ended: KI depleted.\r\n");
            }
        }

        const StatMultipliers requestedMultipliers = statMultipliers(config, ssjEnabled, fusion.active);
        if (requestedMultipliers.speed != appliedMultipliers.speed ||
            requestedMultipliers.damage != appliedMultipliers.damage) {
            applyStatMultipliers(playerPed, player, requestedMultipliers);
            appliedMultipliers = requestedMultipliers;
        }

        drawKiBar(ki, ssjEnabled || fusion.active);
        scriptHookApi().scriptWait(0);
    }
}

}

extern "C" __declspec(dllexport) void InitializeASI() {
    if (pluginModule == nullptr || scriptRegistered.exchange(true)) {
        return;
    }

    if (!scriptHookApi().load()) {
        scriptRegistered = false;
        appendLog("Script Hook RDR2 runtime missing or required exports unavailable.\r\n");
        return;
    }

    scriptHookApi().scriptRegister(pluginModule, ScriptMain);
    appendLog("Script Hook registered the KI gameplay loop.\r\n");
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        pluginModule = module;
        DisableThreadLibraryCalls(module);
    } else if (reason == DLL_PROCESS_DETACH && scriptRegistered.exchange(false)) {
        if (scriptHookApi().scriptUnregister != nullptr) {
            scriptHookApi().scriptUnregister(module);
        }
    }
    return TRUE;
}