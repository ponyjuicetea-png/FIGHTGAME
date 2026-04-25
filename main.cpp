#include <allegro5/allegro.h>
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_ttf.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int SCREEN_W = 1600;
constexpr int SCREEN_H = 900;
constexpr int WORLD_W = 120;
constexpr int WORLD_H = 120;
constexpr float TILE_SIZE = 64.0f;
constexpr float WORLD_PIXEL_W = WORLD_W * TILE_SIZE;
constexpr float WORLD_PIXEL_H = WORLD_H * TILE_SIZE;
constexpr float PLAYER_RADIUS = 18.0f;
constexpr float BASE_X = WORLD_PIXEL_W * 0.5f;
constexpr float BASE_Y = WORLD_PIXEL_H * 0.5f;
constexpr double FPS = 60.0;
constexpr const char* SAVE_DIR = "save_slots";
constexpr const char* META_RUBY_FILE = "meta_rubies.txt";

enum class TileBiome { Grass, Meadow, Water, DarkGrass };
enum class ResourceType { Tree, Rock, Bush, Crystal };
enum class BuildingType { None, Wall, Turret, Camp, DarkMage };
enum class MonsterType { Slime, Hunter, Brute, DarkMage, Boss };
enum class Scene { Title, Playing, GameOver, Victory };
enum class JobClass { Drifter, Vampire, Lumberjack, Fisher, Alien, Elite };
enum class ShopType { None, Bomb, Grocery, Tool };
enum class SlotMenuMode { None, Save, Load };

struct Vec2 { float x = 0.0f; float y = 0.0f; };
struct Inventory {
    int wood = 0;
    int stone = 0;
    int fiber = 0;
    int crystal = 0;
    int vegetable = 0;
    int meat = 0;
    int fish = 0;
    int bomb = 0;
};
struct Player {
    Vec2 pos { BASE_X, BASE_Y };
    Vec2 facing { 1.0f, 0.0f };
    float speed = 260.0f;
    int hp = 120;
    int maxHp = 120;
    float attackCooldown = 0.0f;
    float shootCooldown = 0.0f;
    float gatherCooldown = 0.0f;
    float potionCooldown = 0.0f;
    float fishCooldown = 0.0f;
    float bombCooldown = 0.0f;
    float coreRecallCooldown = 0.0f;
    float alienBurstCooldown = 0.0f;
    float alienSummonCooldown = 0.0f;
    float hunger = 100.0f;
    float hungerDamageTimer = 0.0f;
    int level = 1;
    int xp = 0;
    int xpToNext = 25;
    int cores = 100;
    int vampireHitCount = 0;
    bool onBoat = false;
    int boatIndex = -1;
    JobClass jobClass = JobClass::Drifter;
};
struct Boat {
    Vec2 pos {};
    float dir = 1.0f;
    float fireCooldown = 0.0f;
};
struct Minion {
    Vec2 pos {};
    float speed = 170.0f;
    float attackCooldown = 0.0f;
    float gatherCooldown = 0.0f;
    bool gatherOnly = false;
    bool alive = true;
};
struct ResourceNode {
    ResourceType type = ResourceType::Tree;
    Vec2 pos {};
    int hp = 1;
    int maxHp = 1;
    bool alive = true;
    float respawnTimer = 0.0f;
};
struct Projectile {
    Vec2 pos {};
    Vec2 vel {};
    float life = 0.0f;
    int damage = 0;
    bool friendly = true;
    float radius = 6.0f;
    bool poison = false;
    float poisonDuration = 0.0f;
    bool bomb = false;
    float blastRadius = 0.0f;
    bool alive = true;
};
struct PoisonCloud {
    Vec2 pos {};
    float radius = 40.0f;
    float life = 0.0f;
    bool alive = true;
};
struct ChildNPC {
    Vec2 pos {};
    bool rescued = false;
};
struct Building {
    BuildingType type = BuildingType::None;
    Vec2 pos {};
    int hp = 1;
    int maxHp = 1;
    float radius = 18.0f;
    float fireCooldown = 0.0f;
    float summonCooldown = 0.0f;
    bool alive = true;
};
struct Monster {
    MonsterType type = MonsterType::Slime;
    Vec2 pos {};
    Vec2 vel {};
    int hp = 1;
    int maxHp = 1;
    int damage = 1;
    float speed = 1.0f;
    float attackCooldown = 0.0f;
    bool enraged = false;
    float poisonTimer = 0.0f;
    float poisonTickTimer = 0.0f;
    float slowTimer = 0.0f;
    bool alive = true;
};
struct Controls {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool gather = false;
    bool melee = false;
    bool shoot = false;
    bool fish = false;
};
struct GameState {
    std::vector<std::vector<TileBiome>> map;
    std::vector<ResourceNode> resources;
    std::vector<Projectile> projectiles;
    std::vector<PoisonCloud> poisonClouds;
    std::vector<ChildNPC> children;
    std::vector<Boat> boats;
    std::vector<Building> buildings;
    std::vector<Minion> minions;
    std::vector<Monster> monsters;
    Inventory bag { 35, 28, 14, 6 };
    Player player {};
    Controls controls {};
    BuildingType selectedBuild = BuildingType::Wall;
    JobClass selectedJob = JobClass::Drifter;
    Scene scene = Scene::Title;
    bool buildMode = false;
    bool shopOpen = false;
    ShopType activeShop = ShopType::None;
    SlotMenuMode slotMenuMode = SlotMenuMode::None;
    bool pauseMenuOpen = false;
    bool running = true;
    bool saveExists = false;
    int currentSaveSlot = 1;
    int selectedSaveSlot = 1;
    int permanentRubies = 0;
    int currentStage = 1;
    int totalWoodGathered = 0;
    int totalStoneGathered = 0;
    int totalFiberGathered = 0;
    int totalCrystalGathered = 0;
    int totalMonsterKills = 0;
    int totalDarkMageKills = 0;
    int rescuedChildren = 0;
    int stage4BanditKills = 0;
    int stage4DarkMageKills = 0;
    bool bossSpawned = false;
    bool bossDefeated = false;
    bool villageEventStarted = false;
    bool villageBaseDestroyed = false;
    bool emeraldDropped = false;
    bool carryingEmerald = false;
    bool emeraldDelivered = false;
    bool darkMageRewardGranted = false;
    int shrineClaimDay = 0;
    float bossIntroTimer = 0.0f;
    float bossShakeTimer = 0.0f;
    int villageBaseHp = 0;
    int villageBaseMaxHp = 0;
    int day = 1;
    float dayTimer = 0.0f;
    float danger = 1.0f;
    float waveTimer = 10.0f;
    float autoSaveTimer = 0.0f;
    float frontGateAlertTimer = 0.0f;
    float messageTimer = 8.0f;
    bool hasSword = false;
    bool hasArmor = false;
    bool hasPoisonUpgrade = false;
    std::string message = "採集資源、建立防線，守住你的核心據點。";
    float mouseScreenX = SCREEN_W * 0.5f;
    float mouseScreenY = SCREEN_H * 0.5f;
    std::mt19937 rng { static_cast<std::mt19937::result_type>(std::time(nullptr)) };
};

Vec2 bossZoneCenter() {
    return { WORLD_PIXEL_W - 420.0f, 420.0f };
}
Vec2 villageCenter() {
    return { WORLD_PIXEL_W - 760.0f, WORLD_PIXEL_H * 0.34f };
}
Vec2 villageBasePos() {
    return { villageCenter().x + 160.0f, villageCenter().y - 40.0f };
}
Vec2 shrinePos() {
    return { BASE_X, 260.0f };
}
Vec2 shopPos() {
    return { BASE_X + 380.0f, BASE_Y + 220.0f };
}
Vec2 groceryPos() {
    return { BASE_X + 470.0f, BASE_Y + 220.0f };
}
Vec2 toolShopPos() {
    return { BASE_X + 560.0f, BASE_Y + 220.0f };
}

struct Bgm {
    ALLEGRO_SAMPLE* sample = nullptr;
    ALLEGRO_SAMPLE_INSTANCE* instance = nullptr;
    bool ready = false;
};

ALLEGRO_SAMPLE* gGunshotSample = nullptr;
ALLEGRO_SAMPLE* gGatherSample = nullptr;
ALLEGRO_SAMPLE* gMonsterDeathSample = nullptr;
ALLEGRO_SAMPLE* gBossIntroSample = nullptr;
ALLEGRO_SAMPLE* gPoisonPotionSample = nullptr;
ALLEGRO_SAMPLE* gRescueChildSample = nullptr;
ALLEGRO_SAMPLE* gBombThrowSample = nullptr;

struct SavePreview {
    bool exists = false;
    int day = 0;
    int stage = 0;
    JobClass job = JobClass::Drifter;
};

float clampf(float v, float lo, float hi) { return std::max(lo, std::min(v, hi)); }
float length(Vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }
Vec2 normalize(Vec2 v) { float len = length(v); return len <= 0.0001f ? Vec2 { 0.0f, 0.0f } : Vec2 { v.x / len, v.y / len }; }
float distance(Vec2 a, Vec2 b) { return length({ a.x - b.x, a.y - b.y }); }
Vec2 add(Vec2 a, Vec2 b) { return { a.x + b.x, a.y + b.y }; }
Vec2 sub(Vec2 a, Vec2 b) { return { a.x - b.x, a.y - b.y }; }
Vec2 mul(Vec2 a, float s) { return { a.x * s, a.y * s }; }
float randf(GameState& game, float minValue, float maxValue) { std::uniform_real_distribution<float> dist(minValue, maxValue); return dist(game.rng); }
int randi(GameState& game, int minValue, int maxValue) { std::uniform_int_distribution<int> dist(minValue, maxValue); return dist(game.rng); }
std::string saveSlotPath(int slot) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%s/slot%03d.txt", SAVE_DIR, slot);
    return buffer;
}
bool saveFileExists(int slot) { std::ifstream input(saveSlotPath(slot)); return input.good(); }
bool anySaveFilesExist() {
    for (int slot = 1; slot <= 100; ++slot) {
        if (saveFileExists(slot)) return true;
    }
    return false;
}
int loadPermanentRubies() {
    std::ifstream in(META_RUBY_FILE);
    int value = 0;
    if (in) in >> value;
    return std::max(0, value);
}
bool savePermanentRubies(int rubies) {
    std::ofstream out(META_RUBY_FILE, std::ios::trunc);
    if (!out) return false;
    out << std::max(0, rubies) << '\n';
    return true;
}
SavePreview readSavePreview(int slot) {
    SavePreview preview {};
    std::ifstream in(saveSlotPath(slot));
    if (!in) return preview;
    std::string header;
    int version = 0;
    int selectedBuildInt = 0;
    int selectedJobInt = 0;
    bool buildMode = false;
    bool shopOpen = false;
    int activeShopInt = 0;
    in >> header >> version;
    if (!in || header != "FGSAVE") return preview;
    preview.exists = true;
    in >> preview.day;
    float dayTimer = 0.0f;
    float danger = 0.0f;
    float waveTimer = 0.0f;
    in >> dayTimer >> danger >> waveTimer >> selectedBuildInt >> buildMode >> selectedJobInt >> shopOpen >> activeShopInt;
    in >> preview.stage;
    preview.job = static_cast<JobClass>(selectedJobInt);
    return preview;
}
bool isNight(const GameState& game) { return game.dayTimer >= 95.0f; }
float difficultyFactor(const GameState& game) {
    float dayCurve = 1.0f + (game.day - 1) * 0.28f + std::pow(static_cast<float>(game.day), 1.12f) * 0.09f;
    float nightBonus = isNight(game) ? 0.55f + static_cast<float>(game.day) * 0.03f : 0.0f;
    float raidBonus = (game.day % 5 == 0) ? 0.45f : 0.0f;
    float stageBonus = 0.0f;
    if (game.currentStage == 2) stageBonus = 0.85f + game.day * 0.03f;
    else if (game.currentStage == 3) stageBonus = 1.2f + game.day * 0.04f;
    else if (game.currentStage == 4) stageBonus = 1.5f + game.day * 0.05f + (game.emeraldDelivered ? 0.45f : 0.15f);
    else if (game.currentStage == 5) stageBonus = 1.95f + game.day * 0.05f + (game.bossSpawned ? 0.9f : 0.35f);
    return dayCurve + nightBonus + raidBonus + stageBonus;
}
ALLEGRO_COLOR biomeColor(TileBiome biome) {
    switch (biome) {
        case TileBiome::Grass: return al_map_rgb(63, 137, 66);
        case TileBiome::Meadow: return al_map_rgb(95, 166, 84);
        case TileBiome::Water: return al_map_rgb(36, 92, 148);
        case TileBiome::DarkGrass: return al_map_rgb(49, 109, 56);
    }
    return al_map_rgb(80, 130, 80);
}
ALLEGRO_COLOR resourceColor(ResourceType type) {
    switch (type) {
        case ResourceType::Tree: return al_map_rgb(30, 110, 45);
        case ResourceType::Rock: return al_map_rgb(135, 140, 145);
        case ResourceType::Bush: return al_map_rgb(92, 180, 88);
        case ResourceType::Crystal: return al_map_rgb(116, 232, 255);
    }
    return al_map_rgb(255, 255, 255);
}
const char* buildName(BuildingType type) {
    switch (type) {
        case BuildingType::Wall: return "圍牆";
        case BuildingType::Turret: return "砲塔";
        case BuildingType::Camp: return "營地";
        case BuildingType::DarkMage: return "黑暗小法師";
        default: return "無";
    }
}
Inventory buildCost(BuildingType type) {
    switch (type) {
        case BuildingType::Wall: return { 10, 6, 0, 0 };
        case BuildingType::Turret: return { 22, 18, 0, 4 };
        case BuildingType::Camp: return { 32, 24, 14, 6 };
        case BuildingType::DarkMage: return { 60, 48, 30, 20 };
        default: return {};
    }
}
bool canAfford(const Inventory& bag, const Inventory& cost) {
    return bag.wood >= cost.wood && bag.stone >= cost.stone && bag.fiber >= cost.fiber && bag.crystal >= cost.crystal;
}
void spendResources(Inventory& bag, const Inventory& cost) { bag.wood -= cost.wood; bag.stone -= cost.stone; bag.fiber -= cost.fiber; bag.crystal -= cost.crystal; }
void clearControls(GameState& game) { game.controls = {}; }
void addMessage(GameState& game, const std::string& text, float duration = 5.0f) { game.message = text; game.messageTimer = duration; }
Vec2 worldToScreen(Vec2 world, Vec2 camera) { return { world.x - camera.x, world.y - camera.y }; }
Vec2 screenToWorld(Vec2 screen, Vec2 camera) { return { screen.x + camera.x, screen.y + camera.y }; }
ShopType detectNearbyShop(const GameState& game) {
    float bestDist = 92.0f;
    ShopType bestShop = ShopType::None;
    const std::array<std::pair<ShopType, Vec2>, 3> shops = {{
        { ShopType::Bomb, shopPos() },
        { ShopType::Grocery, groceryPos() },
        { ShopType::Tool, toolShopPos() }
    }};
    for (const auto& entry : shops) {
        float d = distance(game.player.pos, entry.second);
        if (d <= bestDist) {
            bestDist = d;
            bestShop = entry.first;
        }
    }
    return bestShop;
}
Vec2 getCamera(const GameState& game) {
    float camX = clampf(game.player.pos.x - SCREEN_W * 0.5f, 0.0f, WORLD_PIXEL_W - SCREEN_W);
    float camY = clampf(game.player.pos.y - SCREEN_H * 0.5f, 0.0f, WORLD_PIXEL_H - SCREEN_H);
    if (game.bossShakeTimer > 0.0f) {
        float shake = game.bossShakeTimer * 18.0f;
        camX = clampf(camX + std::sin(game.dayTimer * 80.0f) * shake, 0.0f, WORLD_PIXEL_W - SCREEN_W);
        camY = clampf(camY + std::cos(game.dayTimer * 72.0f) * shake * 0.8f, 0.0f, WORLD_PIXEL_H - SCREEN_H);
    }
    return { camX, camY };
}
bool isPassable(const GameState& game, Vec2 pos) {
    pos.x = clampf(pos.x, 0.0f, WORLD_PIXEL_W - 1.0f);
    pos.y = clampf(pos.y, 0.0f, WORLD_PIXEL_H - 1.0f);
    int tx = static_cast<int>(pos.x / TILE_SIZE);
    int ty = static_cast<int>(pos.y / TILE_SIZE);
    return game.map[ty][tx] != TileBiome::Water;
}
bool isWaterTile(const GameState& game, Vec2 pos) {
    pos.x = clampf(pos.x, 0.0f, WORLD_PIXEL_W - 1.0f);
    pos.y = clampf(pos.y, 0.0f, WORLD_PIXEL_H - 1.0f);
    int tx = static_cast<int>(pos.x / TILE_SIZE);
    int ty = static_cast<int>(pos.y / TILE_SIZE);
    return game.map[ty][tx] == TileBiome::Water;
}
const char* jobName(JobClass job) {
    switch (job) {
        case JobClass::Drifter: return "無業遊民";
        case JobClass::Vampire: return "吸血鬼";
        case JobClass::Lumberjack: return "伐木工";
        case JobClass::Fisher: return "漁夫";
        case JobClass::Alien: return "外星人";
        case JobClass::Elite: return "菁英";
        default: return "未知";
    }
}
const char* jobDescription(JobClass job) {
    switch (job) {
        case JobClass::Drifter: return "沒有特殊能力，最穩定。";
        case JobClass::Vampire: return "每 3 次命中吸血，J 回核心。";
        case JobClass::Lumberjack: return "採樹石各 +20，採草與水晶各 +6。";
        case JobClass::Fisher: return "J 上下船，K 在水面放船，砲船自動開火。";
        case JobClass::Alien: return "J 八連砲，K 每 30 秒召喚一隻小兵。";
        case JobClass::Elite: return "受到傷害降低，毒藥水與中毒傷害更高。";
        default: return "";
    }
}
ALLEGRO_COLOR jobBodyColor(JobClass job) {
    switch (job) {
        case JobClass::Drifter: return al_map_rgb(69, 158, 255);
        case JobClass::Vampire: return al_map_rgb(156, 54, 78);
        case JobClass::Lumberjack: return al_map_rgb(154, 108, 56);
        case JobClass::Fisher: return al_map_rgb(56, 156, 196);
        case JobClass::Alien: return al_map_rgb(102, 232, 202);
        case JobClass::Elite: return al_map_rgb(236, 196, 92);
        default: return al_map_rgb(69, 158, 255);
    }
}
void refreshDifficulty(GameState& game) { game.danger = difficultyFactor(game); }
int countBuildingsOfType(const GameState& game, BuildingType type) {
    int count = 0;
    for (const auto& building : game.buildings) {
        if (building.alive && building.type == type) {
            ++count;
        }
    }
    return count;
}

void setupRescueChildren(GameState& game) {
    game.children.clear();
    game.children.push_back({ { 420.0f, 420.0f }, false });
    game.children.push_back({ { WORLD_PIXEL_W - 520.0f, 520.0f }, false });
    game.children.push_back({ { 540.0f, WORLD_PIXEL_H - 520.0f }, false });
    game.children.push_back({ { WORLD_PIXEL_W - 620.0f, WORLD_PIXEL_H - 620.0f }, false });
    game.rescuedChildren = 0;
}
void spawnShrineDarkMages(GameState& game) {
    const std::array<Vec2, 4> shrineMages = {{
        { shrinePos().x - 120.0f, shrinePos().y + 40.0f },
        { shrinePos().x + 120.0f, shrinePos().y + 30.0f },
        { shrinePos().x - 56.0f, shrinePos().y + 132.0f },
        { shrinePos().x + 62.0f, shrinePos().y + 144.0f }
    }};
    for (const auto& pos : shrineMages) {
        Monster mage {};
        mage.type = MonsterType::DarkMage;
        mage.pos = pos;
        mage.hp = mage.maxHp = 180;
        mage.damage = 15;
        mage.speed = 80.0f;
        mage.attackCooldown = randf(game, 0.1f, 1.0f);
        game.monsters.push_back(mage);
    }
}
void setupVillageEncounter(GameState& game) {
    game.villageEventStarted = true;
    game.villageBaseDestroyed = false;
    game.emeraldDropped = false;
    game.carryingEmerald = false;
    game.emeraldDelivered = false;
    game.darkMageRewardGranted = false;
    game.stage4BanditKills = 0;
    game.stage4DarkMageKills = 0;
    game.villageBaseMaxHp = 380;
    game.villageBaseHp = 380;

    const std::array<Vec2, 4> mageSpawns = {{
        { shrinePos().x - 120.0f, shrinePos().y + 40.0f },
        { shrinePos().x + 120.0f, shrinePos().y + 30.0f },
        { villageCenter().x + 10.0f, villageCenter().y + 80.0f },
        { villageCenter().x + 180.0f, villageCenter().y - 150.0f }
    }};
    for (const auto& pos : mageSpawns) {
        Monster mage {};
        mage.type = MonsterType::DarkMage;
        mage.pos = pos;
        mage.hp = mage.maxHp = 220;
        mage.damage = 17;
        mage.speed = 82.0f;
        mage.attackCooldown = randf(game, 0.1f, 1.2f);
        game.monsters.push_back(mage);
    }

    for (int i = 0; i < 18; ++i) {
        Monster bandit {};
        bandit.type = (i % 3 == 0) ? MonsterType::Brute : MonsterType::Hunter;
        bandit.pos = { villageCenter().x + randf(game, -240.0f, 220.0f), villageCenter().y + randf(game, -170.0f, 160.0f) };
        bandit.hp = bandit.maxHp = bandit.type == MonsterType::Brute ? 120 : 72;
        bandit.damage = bandit.type == MonsterType::Brute ? 24 : 16;
        bandit.speed = bandit.type == MonsterType::Brute ? 86.0f : 122.0f;
        bandit.attackCooldown = randf(game, 0.0f, 1.0f);
        game.monsters.push_back(bandit);
    }
}
void setupFishingBoats(GameState& game) {
    game.boats.clear();
    const std::array<Vec2, 3> preferredCenters = {{
        { WORLD_PIXEL_W * 0.18f, WORLD_PIXEL_H * 0.26f },
        { WORLD_PIXEL_W * 0.56f, WORLD_PIXEL_H * 0.52f },
        { WORLD_PIXEL_W * 0.82f, WORLD_PIXEL_H * 0.74f }
    }};
    for (size_t i = 0; i < preferredCenters.size(); ++i) {
        bool found = false;
        for (int radius = 0; radius <= 260 && !found; radius += 26) {
            for (int dx = -radius; dx <= radius && !found; dx += 26) {
                for (int dy = -radius; dy <= radius && !found; dy += 26) {
                    Vec2 probe { preferredCenters[i].x + dx, preferredCenters[i].y + dy };
                    if (probe.x < 80.0f || probe.y < 80.0f || probe.x > WORLD_PIXEL_W - 80.0f || probe.y > WORLD_PIXEL_H - 80.0f) continue;
                    if (isWaterTile(game, probe)) {
                        Boat boat {};
                        boat.pos = probe;
                        boat.dir = (i % 2 == 0) ? 1.0f : -1.0f;
                        boat.fireCooldown = 1.0f + static_cast<float>(i) * 0.4f;
                        game.boats.push_back(boat);
                        found = true;
                    }
                }
            }
        }
    }
}
Vec2 findNearbyLandPosition(const GameState& game, Vec2 center) {
    for (float radius = 48.0f; radius <= 180.0f; radius += 24.0f) {
        for (int step = 0; step < 16; ++step) {
            float angle = (ALLEGRO_PI * 2.0f / 16.0f) * step;
            Vec2 probe { center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius };
            if (isPassable(game, probe)) return probe;
        }
    }
    return { BASE_X, BASE_Y + 120.0f };
}

void advanceStage(GameState& game) {
    if (game.currentStage < 5) {
        game.currentStage += 1;
        if (game.currentStage == 2) {
            addMessage(game, "第一關完成，第二關開始：建立完整防線並清除怪群。", 6.0f);
        } else if (game.currentStage == 3) {
            setupRescueChildren(game);
            addMessage(game, "第三關開始：前往地圖各處救出 4 個小孩。", 7.0f);
        } else if (game.currentStage == 4) {
            game.children.clear();
            setupVillageEncounter(game);
            addMessage(game, "第四關開始：前往損壞的村莊，討伐山匪、黑暗大法師與藍基地。", 7.0f);
        } else if (game.currentStage == 5) {
            addMessage(game, "第五關開始：前往黑色地點，召喚並擊敗最終魔王。", 7.0f);
        }
    }
}
void grantXp(GameState& game, int amount) {
    game.player.xp += amount;
    while (game.player.xp >= game.player.xpToNext) {
        game.player.xp -= game.player.xpToNext;
        game.player.level += 1;
        game.player.maxHp += 12;
        game.player.hp = std::min(game.player.maxHp, game.player.hp + 24);
        game.player.xpToNext += 18;
        game.player.cores = std::min(100, game.player.cores + 10);
        addMessage(game, "升級了！你的戰力與核心穩定度都提升了。", 6.0f);
    }
}

void seedWorld(GameState& game) {
    game.map.assign(WORLD_H, std::vector<TileBiome>(WORLD_W, TileBiome::Grass));
    for (int y = 0; y < WORLD_H; ++y) {
        for (int x = 0; x < WORLD_W; ++x) {
            float nx = static_cast<float>(x) / WORLD_W;
            float ny = static_cast<float>(y) / WORLD_H;
            float waveA = std::sin(nx * 11.0f) + std::cos(ny * 14.0f);
            float waveB = std::sin((nx + ny) * 18.0f);
            float noise = waveA * 0.32f + waveB * 0.25f + randf(game, -0.18f, 0.18f);
            if (noise < -0.38f) game.map[y][x] = TileBiome::Water;
            else if (noise < 0.02f) game.map[y][x] = TileBiome::DarkGrass;
            else if (noise < 0.25f) game.map[y][x] = TileBiome::Grass;
            else game.map[y][x] = TileBiome::Meadow;
        }
    }
    for (int y = WORLD_H / 2 - 4; y <= WORLD_H / 2 + 4; ++y) {
        for (int x = WORLD_W / 2 - 4; x <= WORLD_W / 2 + 4; ++x) {
            if (x >= 0 && y >= 0 && x < WORLD_W && y < WORLD_H) game.map[y][x] = TileBiome::Meadow;
        }
    }
    game.resources.clear();
    for (int i = 0; i < 900; ++i) {
        ResourceNode node {};
        float x = randf(game, TILE_SIZE, WORLD_PIXEL_W - TILE_SIZE);
        float y = randf(game, TILE_SIZE, WORLD_PIXEL_H - TILE_SIZE);
        if (!isPassable(game, { x, y }) || distance({ x, y }, { BASE_X, BASE_Y }) < 240.0f) continue;
        int roll = randi(game, 0, 99);
        if (roll < 42) { node.type = ResourceType::Tree; node.hp = node.maxHp = 5; }
        else if (roll < 72) { node.type = ResourceType::Rock; node.hp = node.maxHp = 6; }
        else if (roll < 92) { node.type = ResourceType::Bush; node.hp = node.maxHp = 4; }
        else { node.type = ResourceType::Crystal; node.hp = node.maxHp = 8; }
        node.pos = { x, y };
        game.resources.push_back(node);
    }
}

Building makeBuilding(BuildingType type, Vec2 pos) {
    Building b {};
    b.type = type;
    b.pos = pos;
    if (type == BuildingType::Wall) { b.maxHp = b.hp = 120; b.radius = 28.0f; }
    else if (type == BuildingType::Turret) { b.maxHp = b.hp = 90; b.radius = 22.0f; b.fireCooldown = 0.5f; }
    else if (type == BuildingType::Camp) { b.maxHp = b.hp = 180; b.radius = 36.0f; }
    else if (type == BuildingType::DarkMage) { b.maxHp = b.hp = 140; b.radius = 30.0f; b.fireCooldown = 0.8f; b.summonCooldown = 18.0f; }
    return b;
}

void resetCoreCamp(GameState& game) {
    game.buildings.clear();
    game.buildings.push_back(makeBuilding(BuildingType::Camp, { BASE_X - 150.0f, BASE_Y + 170.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Camp, { BASE_X + 150.0f, BASE_Y + 170.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Camp, { BASE_X, BASE_Y + 235.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Turret, { BASE_X - 220.0f, BASE_Y - 110.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Turret, { BASE_X + 220.0f, BASE_Y - 110.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Turret, { BASE_X - 240.0f, BASE_Y + 40.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Turret, { BASE_X + 240.0f, BASE_Y + 40.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Wall, { BASE_X - 90.0f, BASE_Y }));
    game.buildings.push_back(makeBuilding(BuildingType::Wall, { BASE_X + 90.0f, BASE_Y }));
    game.buildings.push_back(makeBuilding(BuildingType::Wall, { BASE_X, BASE_Y - 90.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Wall, { BASE_X, BASE_Y + 90.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Wall, { BASE_X - 150.0f, BASE_Y - 70.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Wall, { BASE_X + 150.0f, BASE_Y - 70.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Wall, { BASE_X - 230.0f, BASE_Y - 10.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Wall, { BASE_X + 230.0f, BASE_Y - 10.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Wall, { BASE_X - 170.0f, BASE_Y + 100.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Wall, { BASE_X + 170.0f, BASE_Y + 100.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Wall, { BASE_X - 70.0f, BASE_Y - 170.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Wall, { BASE_X + 70.0f, BASE_Y - 170.0f }));
}

void startNewGame(GameState& game) {
    game.bag = { 35, 28, 14, 6, 3, 1, 0, 0 };
    game.player = {};
    game.player.jobClass = game.selectedJob;
    if (game.selectedJob == JobClass::Vampire) {
        game.player.maxHp = 126;
        game.player.hp = 126;
    } else if (game.selectedJob == JobClass::Alien) {
        game.player.maxHp = 116;
        game.player.hp = 116;
    } else if (game.selectedJob == JobClass::Elite) {
        game.player.maxHp = 132;
        game.player.hp = 132;
    }
    game.selectedBuild = BuildingType::Wall;
    game.scene = Scene::Playing;
    game.buildMode = false;
    game.shopOpen = false;
    game.activeShop = ShopType::None;
    game.slotMenuMode = SlotMenuMode::None;
    game.pauseMenuOpen = false;
    game.currentStage = 1;
    game.totalWoodGathered = 0;
    game.totalStoneGathered = 0;
    game.totalFiberGathered = 0;
    game.totalCrystalGathered = 0;
    game.totalMonsterKills = 0;
    game.totalDarkMageKills = 0;
    game.rescuedChildren = 0;
    game.stage4BanditKills = 0;
    game.stage4DarkMageKills = 0;
    game.bossSpawned = false;
    game.bossDefeated = false;
    game.villageEventStarted = false;
    game.villageBaseDestroyed = false;
    game.emeraldDropped = false;
    game.carryingEmerald = false;
    game.emeraldDelivered = false;
    game.darkMageRewardGranted = false;
    game.shrineClaimDay = 0;
    game.bossIntroTimer = 0.0f;
    game.bossShakeTimer = 0.0f;
    game.villageBaseHp = 0;
    game.villageBaseMaxHp = 0;
    game.day = 1;
    game.dayTimer = 0.0f;
    game.player.hunger = 100.0f;
    game.player.hungerDamageTimer = 0.0f;
    game.player.onBoat = false;
    game.player.boatIndex = -1;
    game.player.bombCooldown = 0.0f;
    game.waveTimer = 8.0f;
    game.autoSaveTimer = 0.0f;
    game.frontGateAlertTimer = 0.0f;
    game.messageTimer = 8.0f;
    game.hasSword = false;
    game.hasArmor = false;
    game.hasPoisonUpgrade = false;
    game.currentSaveSlot = game.selectedSaveSlot;
    game.message = "第一關：採集資源並建立初步防線。";
    game.projectiles.clear();
    game.poisonClouds.clear();
    game.children.clear();
    game.boats.clear();
    game.minions.clear();
    game.monsters.clear();
    seedWorld(game);
    spawnShrineDarkMages(game);
    if (game.selectedJob == JobClass::Fisher) setupFishingBoats(game);
    resetCoreCamp(game);
    refreshDifficulty(game);
    clearControls(game);
}

bool saveGame(GameState& game, int slot) {
    std::filesystem::create_directories(SAVE_DIR);
    std::ofstream out(saveSlotPath(slot), std::ios::trunc);
    if (!out) return false;

    out << "FGSAVE 12\n";
    out << game.day << ' ' << game.dayTimer << ' ' << game.danger << ' ' << game.waveTimer << ' ' << static_cast<int>(game.selectedBuild) << ' ' << game.buildMode << ' ' << static_cast<int>(game.selectedJob) << ' ' << game.shopOpen << ' ' << static_cast<int>(game.activeShop) << '\n';
    out << game.currentStage << ' ' << game.totalWoodGathered << ' ' << game.totalStoneGathered << ' ' << game.totalFiberGathered << ' '
        << game.totalCrystalGathered << ' ' << game.totalMonsterKills << ' ' << game.totalDarkMageKills << ' ' << game.rescuedChildren << ' ' << game.bossSpawned << ' ' << game.bossDefeated << ' '
        << game.hasSword << ' ' << game.hasArmor << ' ' << game.hasPoisonUpgrade << ' '
        << game.stage4BanditKills << ' ' << game.stage4DarkMageKills << ' ' << game.villageEventStarted << ' ' << game.villageBaseDestroyed << ' '
        << game.emeraldDropped << ' ' << game.carryingEmerald << ' ' << game.emeraldDelivered << ' ' << game.darkMageRewardGranted << ' '
        << game.villageBaseHp << ' ' << game.villageBaseMaxHp << ' ' << game.shrineClaimDay << '\n';
    out << game.bag.wood << ' ' << game.bag.stone << ' ' << game.bag.fiber << ' ' << game.bag.crystal << ' '
        << game.bag.vegetable << ' ' << game.bag.meat << ' ' << game.bag.fish << ' ' << game.bag.bomb << '\n';
    out << game.player.pos.x << ' ' << game.player.pos.y << ' ' << game.player.facing.x << ' ' << game.player.facing.y << ' '
        << game.player.speed << ' ' << game.player.hp << ' ' << game.player.maxHp << ' ' << game.player.level << ' '
        << game.player.xp << ' ' << game.player.xpToNext << ' ' << game.player.cores << ' ' << game.player.hunger << ' ' << game.player.hungerDamageTimer << ' '
        << game.player.coreRecallCooldown << ' ' << game.player.vampireHitCount << ' ' << static_cast<int>(game.player.jobClass) << ' '
        << game.player.alienBurstCooldown << ' ' << game.player.alienSummonCooldown << ' ' << game.player.onBoat << ' ' << game.player.boatIndex << ' ' << game.player.bombCooldown << '\n';

    out << game.map.size() << ' ' << (game.map.empty() ? 0 : game.map.front().size()) << '\n';
    for (const auto& row : game.map) {
        for (TileBiome tile : row) out << static_cast<int>(tile) << ' ';
        out << '\n';
    }

    out << game.resources.size() << '\n';
    for (const auto& node : game.resources) {
        out << static_cast<int>(node.type) << ' ' << node.pos.x << ' ' << node.pos.y << ' ' << node.hp << ' ' << node.maxHp << ' ' << node.alive << ' ' << node.respawnTimer << '\n';
    }

    out << game.buildings.size() << '\n';
    for (const auto& building : game.buildings) {
        out << static_cast<int>(building.type) << ' ' << building.pos.x << ' ' << building.pos.y << ' ' << building.hp << ' ' << building.maxHp << ' ' << building.radius << ' ' << building.fireCooldown << ' ' << building.summonCooldown << ' ' << building.alive << '\n';
    }
    out << game.children.size() << '\n';
    for (const auto& child : game.children) {
        out << child.pos.x << ' ' << child.pos.y << ' ' << child.rescued << '\n';
    }
    game.currentSaveSlot = slot;
    game.selectedSaveSlot = slot;
    return true;
}

bool saveGame(GameState& game) {
    return saveGame(game, game.currentSaveSlot);
}

bool loadGame(GameState& game, int slot) {
    std::ifstream in(saveSlotPath(slot));
    if (!in) return false;

    std::string header;
    int version = 0;
    in >> header >> version;
    if (!in || header != "FGSAVE" || version != 12) return false;

    int selectedBuildInt = 0;
    int selectedJobInt = 0;
    int activeShopInt = 0;
    in >> game.day >> game.dayTimer >> game.danger >> game.waveTimer >> selectedBuildInt >> game.buildMode >> selectedJobInt >> game.shopOpen >> activeShopInt;
    game.selectedBuild = static_cast<BuildingType>(selectedBuildInt);
    game.selectedJob = static_cast<JobClass>(selectedJobInt);
    game.activeShop = static_cast<ShopType>(activeShopInt);
    in >> game.currentStage >> game.totalWoodGathered >> game.totalStoneGathered >> game.totalFiberGathered
        >> game.totalCrystalGathered >> game.totalMonsterKills >> game.totalDarkMageKills >> game.rescuedChildren >> game.bossSpawned >> game.bossDefeated
        >> game.hasSword >> game.hasArmor >> game.hasPoisonUpgrade
        >> game.stage4BanditKills >> game.stage4DarkMageKills >> game.villageEventStarted >> game.villageBaseDestroyed
        >> game.emeraldDropped >> game.carryingEmerald >> game.emeraldDelivered >> game.darkMageRewardGranted
        >> game.villageBaseHp >> game.villageBaseMaxHp >> game.shrineClaimDay;
    game.bossIntroTimer = 0.0f;
    game.bossShakeTimer = 0.0f;
    in >> game.bag.wood >> game.bag.stone >> game.bag.fiber >> game.bag.crystal >> game.bag.vegetable >> game.bag.meat >> game.bag.fish >> game.bag.bomb;
    in >> game.player.pos.x >> game.player.pos.y >> game.player.facing.x >> game.player.facing.y
        >> game.player.speed >> game.player.hp >> game.player.maxHp >> game.player.level
        >> game.player.xp >> game.player.xpToNext >> game.player.cores >> game.player.hunger >> game.player.hungerDamageTimer
        >> game.player.coreRecallCooldown >> game.player.vampireHitCount >> selectedJobInt
        >> game.player.alienBurstCooldown >> game.player.alienSummonCooldown >> game.player.onBoat >> game.player.boatIndex >> game.player.bombCooldown;
    game.player.jobClass = static_cast<JobClass>(selectedJobInt);

    int mapH = 0;
    int mapW = 0;
    in >> mapH >> mapW;
    if (!in || mapH != WORLD_H || mapW != WORLD_W) return false;

    game.map.assign(WORLD_H, std::vector<TileBiome>(WORLD_W, TileBiome::Grass));
    for (int y = 0; y < WORLD_H; ++y) {
        for (int x = 0; x < WORLD_W; ++x) {
            int tile = 0;
            in >> tile;
            game.map[y][x] = static_cast<TileBiome>(tile);
        }
    }

    size_t resourceCount = 0;
    in >> resourceCount;
    game.resources.clear();
    game.resources.reserve(resourceCount);
    for (size_t i = 0; i < resourceCount; ++i) {
        ResourceNode node {};
        int type = 0;
        in >> type >> node.pos.x >> node.pos.y >> node.hp >> node.maxHp >> node.alive >> node.respawnTimer;
        node.type = static_cast<ResourceType>(type);
        game.resources.push_back(node);
    }

    size_t buildingCount = 0;
    in >> buildingCount;
    game.buildings.clear();
    game.buildings.reserve(buildingCount);
    for (size_t i = 0; i < buildingCount; ++i) {
        Building b {};
        int type = 0;
        in >> type >> b.pos.x >> b.pos.y >> b.hp >> b.maxHp >> b.radius >> b.fireCooldown >> b.summonCooldown >> b.alive;
        b.type = static_cast<BuildingType>(type);
        game.buildings.push_back(b);
    }

    size_t childCount = 0;
    in >> childCount;
    game.children.clear();
    game.children.reserve(childCount);
    for (size_t i = 0; i < childCount; ++i) {
        ChildNPC child {};
        in >> child.pos.x >> child.pos.y >> child.rescued;
        game.children.push_back(child);
    }

    if (!in) return false;

    game.projectiles.clear();
    game.poisonClouds.clear();
    game.boats.clear();
    game.minions.clear();
    game.monsters.clear();
    if (game.currentStage == 1) spawnShrineDarkMages(game);
    if (game.player.jobClass == JobClass::Fisher) setupFishingBoats(game);
    game.scene = Scene::Playing;
    game.autoSaveTimer = 0.0f;
    game.frontGateAlertTimer = 0.0f;
    game.pauseMenuOpen = false;
    game.slotMenuMode = SlotMenuMode::None;
    game.messageTimer = 5.0f;
    game.message = "已載入存檔，繼續完成目前關卡。";
    game.currentSaveSlot = slot;
    game.selectedSaveSlot = slot;
    refreshDifficulty(game);
    clearControls(game);
    return true;
}
bool loadGame(GameState& game) {
    return loadGame(game, game.selectedSaveSlot);
}

void spawnMonster(GameState& game, int count) {
    float scale = difficultyFactor(game);
    for (int i = 0; i < count; ++i) {
        Monster monster {};
        int rollSide = randi(game, 0, 99);
        int side = 0;
        if (rollSide < 52) side = 0;
        else if (rollSide < 72) side = 1;
        else if (rollSide < 92) side = 3;
        else side = 2;
        if (side == 0) monster.pos = { randf(game, 0.0f, WORLD_PIXEL_W), -40.0f };
        if (side == 1) monster.pos = { WORLD_PIXEL_W + 40.0f, randf(game, 0.0f, WORLD_PIXEL_H) };
        if (side == 2) monster.pos = { randf(game, 0.0f, WORLD_PIXEL_W), WORLD_PIXEL_H + 40.0f };
        if (side == 3) monster.pos = { -40.0f, randf(game, 0.0f, WORLD_PIXEL_H) };

        int roll = randi(game, 0, 99);
        if (game.currentStage == 2) roll = std::min(99, roll + 12);
        if (game.currentStage == 3) roll = std::min(99, roll + 16);
        if (game.currentStage >= 4) roll = std::min(99, roll + 24);
        if (game.day >= 6 && randi(game, 0, 99) < std::min(28, game.day * 2 + game.currentStage * 5)) roll = 90;

        if (roll < 48) {
            monster.type = MonsterType::Slime;
            monster.hp = monster.maxHp = static_cast<int>(30 + scale * 12.0f + game.currentStage * 4.0f);
            monster.damage = static_cast<int>(9 + scale * 2.9f + game.currentStage);
            monster.speed = 92.0f + scale * 8.0f + game.currentStage * 3.0f;
        } else if (roll < 80) {
            monster.type = MonsterType::Hunter;
            monster.hp = monster.maxHp = static_cast<int>(44 + scale * 14.0f + game.currentStage * 7.0f);
            monster.damage = static_cast<int>(13 + scale * 3.8f + game.currentStage * 2.0f);
            monster.speed = 121.0f + scale * 9.0f + game.currentStage * 4.0f;
        } else {
            monster.type = MonsterType::Brute;
            monster.hp = monster.maxHp = static_cast<int>(88 + scale * 22.0f + game.currentStage * 12.0f);
            monster.damage = static_cast<int>(20 + scale * 4.8f + game.currentStage * 2.0f);
            monster.speed = 74.0f + scale * 6.0f + game.currentStage * 2.0f;
        }
        if (game.currentStage == 1 && monster.type == MonsterType::Brute) {
            monster.type = MonsterType::Hunter;
            monster.hp = monster.maxHp = static_cast<int>(40 + scale * 10.0f);
            monster.damage = static_cast<int>(11 + scale * 2.8f);
            monster.speed = 116.0f + scale * 7.0f;
        }
        game.monsters.push_back(monster);
    }
}

bool eatFood(GameState& game) {
    if (game.bag.meat > 0) {
        game.bag.meat -= 1;
        game.player.hunger = std::min(100.0f, game.player.hunger + 34.0f);
        game.player.hp = std::min(game.player.maxHp, game.player.hp + 8);
        addMessage(game, "你吃了肉，恢復了飽食與些微生命。", 3.0f);
        return true;
    }
    if (game.bag.vegetable > 0) {
        game.bag.vegetable -= 1;
        game.player.hunger = std::min(100.0f, game.player.hunger + 24.0f);
        game.player.hp = std::min(game.player.maxHp, game.player.hp + 5);
        addMessage(game, "你吃了菜，恢復了一些飽食與生命。", 3.0f);
        return true;
    }
    if (game.bag.fish > 0) {
        game.bag.fish -= 1;
        game.player.hunger = std::min(100.0f, game.player.hunger + 28.0f);
        game.player.hp = std::min(game.player.maxHp, game.player.hp + 6);
        addMessage(game, "你吃了魚，恢復了一些飽食與生命。", 3.0f);
        return true;
    }
    addMessage(game, "身上沒有可吃的肉、菜或魚。", 3.0f);
    return false;
}

void tryPlaceBuilding(GameState& game, Vec2 worldPos) {
    if (!game.buildMode || game.selectedBuild == BuildingType::None) return;
    if (distance(worldPos, game.player.pos) > 210.0f) {
        addMessage(game, "請再靠近一點才能放置建築。", 3.0f);
        return;
    }
    if (!isPassable(game, worldPos)) {
        addMessage(game, "水面上無法建造。", 3.0f);
        return;
    }
    for (const auto& building : game.buildings) {
        if (building.alive && distance(building.pos, worldPos) < building.radius + 34.0f) {
            addMessage(game, "距離其他建築太近了。", 3.0f);
            return;
        }
    }
    Inventory cost = buildCost(game.selectedBuild);
    if (!canAfford(game.bag, cost)) {
        if (game.selectedBuild == BuildingType::DarkMage) {
            addMessage(game, "黑暗小法師需要 60 木材、48 石頭、30 纖維、20 水晶。", 3.2f);
        } else {
            addMessage(game, "材料不足，無法建造這個設施。", 3.0f);
        }
        return;
    }
    spendResources(game.bag, cost);
    game.buildings.push_back(makeBuilding(game.selectedBuild, worldPos));
    addMessage(game, std::string(buildName(game.selectedBuild)) + " 已建造完成。", 3.0f);
}

void damageMonster(GameState& game, Monster& monster, int damage) {
    monster.hp -= damage;
    if (damage > 0 && game.player.jobClass == JobClass::Vampire && monster.alive) {
        game.player.vampireHitCount += 1;
        if (game.player.vampireHitCount >= 3) {
            game.player.vampireHitCount = 0;
            game.player.hp = std::min(game.player.maxHp, game.player.hp + 7);
            addMessage(game, "吸血鬼汲取了怪物的血，恢復生命。", 2.6f);
        }
    }
    if (monster.hp <= 0) {
        monster.alive = false;
        if (gMonsterDeathSample) {
            float pitch = 1.0f;
            if (monster.type == MonsterType::Hunter) pitch = 1.08f;
            if (monster.type == MonsterType::Brute) pitch = 0.72f;
            if (monster.type == MonsterType::Boss) pitch = 0.55f;
            al_play_sample(gMonsterDeathSample, 0.74f, 0.0f, pitch, ALLEGRO_PLAYMODE_ONCE, nullptr);
        }
        game.totalMonsterKills += 1;
        if (monster.type == MonsterType::DarkMage) game.totalDarkMageKills += 1;
        if (game.currentStage == 4) {
            if (monster.type == MonsterType::DarkMage) game.stage4DarkMageKills += 1;
            if (monster.type == MonsterType::Hunter || monster.type == MonsterType::Brute) game.stage4BanditKills += 1;
        }
        if (monster.type == MonsterType::Boss) {
            game.bossDefeated = true;
            game.player.cores = std::min(100, game.player.cores + 40);
            saveGame(game);
            game.saveExists = true;
            clearControls(game);
            game.scene = Scene::Victory;
            addMessage(game, "最終魔王已被擊敗，你征服了黑色禁地！", 8.0f);
        }
        game.bag.wood += randi(game, 0, 2);
        game.bag.stone += randi(game, 0, 2);
        game.bag.fiber += randi(game, 0, 1);
        if (monster.type == MonsterType::Brute) game.bag.meat += 2;
        else if (monster.type != MonsterType::Boss && randi(game, 0, 99) > 42) game.bag.meat += 1;
        if (monster.type == MonsterType::Boss) game.bag.crystal += 12;
        else if (randi(game, 0, 99) > 76) game.bag.crystal += 1;
        grantXp(game, monster.type == MonsterType::Brute ? 10 : 4);
    }
}

void handleGather(GameState& game) {
    if (!game.controls.gather || game.player.gatherCooldown > 0.0f) return;

    if (distance(game.player.pos, shrinePos()) < 78.0f) {
        if (game.totalDarkMageKills < 4) {
            game.player.gatherCooldown = 0.25f;
            addMessage(game, ("神社封印尚未解除，必須先打敗 4 隻黑暗大法師。目前 " + std::to_string(game.totalDarkMageKills) + " / 4。"), 3.8f);
        } else if (game.shrineClaimDay != game.day) {
            game.permanentRubies += 1;
            game.shrineClaimDay = game.day;
            game.player.gatherCooldown = 0.35f;
            if (savePermanentRubies(game.permanentRubies)) {
                addMessage(game, "你在神社取得了 1 顆永久紅寶石。", 3.6f);
            } else {
                addMessage(game, "紅寶石已取得，但寫入永久檔案失敗。", 3.6f);
            }
        } else {
            game.player.gatherCooldown = 0.25f;
            addMessage(game, "今天已經在神社領過紅寶石了。", 2.8f);
        }
        return;
    }

    ResourceNode* best = nullptr;
    float bestDist = 85.0f;
    for (auto& node : game.resources) {
        if (!node.alive) continue;
        float d = distance(game.player.pos, node.pos);
        if (d < bestDist) { best = &node; bestDist = d; }
    }
    if (!best) {
        addMessage(game, "附近沒有可採集資源，靠近後按 E。", 2.2f);
        game.player.gatherCooldown = 0.35f;
        return;
    }

    int gatherPower = 1 + (game.player.level >= 4 ? 1 : 0) + (game.day >= 8 ? 1 : 0);
    best->hp -= gatherPower;
    game.player.gatherCooldown = 0.25f;
    bool lumberjack = game.player.jobClass == JobClass::Lumberjack;
    if (best->type == ResourceType::Tree) {
        int gain = lumberjack ? 20 : 2;
        game.bag.wood += gain;
        game.totalWoodGathered += gain;
    }
    if (best->type == ResourceType::Rock) {
        int gain = lumberjack ? 20 : 2;
        game.bag.stone += gain;
        game.totalStoneGathered += gain;
    }
    if (best->type == ResourceType::Bush) {
        int gain = lumberjack ? 6 : 2;
        game.bag.fiber += gain;
        game.bag.vegetable += lumberjack ? 3 : 1;
        game.totalFiberGathered += gain;
    }
    if (best->type == ResourceType::Crystal) {
        int gain = lumberjack ? 6 : 1;
        game.bag.crystal += gain;
        game.totalCrystalGathered += gain;
    }
    if (gGatherSample) {
        float pitch = 1.0f;
        if (best->type == ResourceType::Rock) pitch = 0.84f;
        if (best->type == ResourceType::Bush) pitch = 1.14f;
        if (best->type == ResourceType::Crystal) pitch = 1.28f;
        al_play_sample(gGatherSample, 0.62f, 0.0f, pitch, ALLEGRO_PLAYMODE_ONCE, nullptr);
    }
    grantXp(game, 1);

    if (best->hp <= 0) {
        best->alive = false;
        best->respawnTimer = 18.0f + randf(game, 0.0f, 10.0f);
    }
}

void updateProjectiles(GameState& game, float dt) {
    for (auto& p : game.projectiles) {
        if (!p.alive) continue;
        p.pos = add(p.pos, mul(p.vel, dt));
        p.life -= dt;
        if (p.life <= 0.0f || p.pos.x < 0.0f || p.pos.y < 0.0f || p.pos.x > WORLD_PIXEL_W || p.pos.y > WORLD_PIXEL_H) {
            if (p.bomb) {
                for (auto& monster : game.monsters) {
                    if (!monster.alive) continue;
                    if (distance(p.pos, monster.pos) <= p.blastRadius) {
                        damageMonster(game, monster, p.damage);
                    }
                }
            }
            if (p.poison) {
                game.poisonClouds.push_back({ p.pos, 54.0f, 3.2f, true });
            }
            p.alive = false;
            continue;
        }
        if (p.friendly) {
            if (game.currentStage == 4 && !game.villageBaseDestroyed && distance(p.pos, villageBasePos()) <= p.radius + 44.0f) {
                game.villageBaseHp = std::max(0, game.villageBaseHp - p.damage);
                if (p.bomb) {
                    game.villageBaseHp = std::max(0, game.villageBaseHp - p.damage / 2);
                }
                p.alive = false;
                continue;
            }
            for (auto& monster : game.monsters) {
                if (!monster.alive) continue;
                if (distance(p.pos, monster.pos) <= p.radius + 18.0f) {
                    if (p.bomb) {
                        for (auto& blastTarget : game.monsters) {
                            if (!blastTarget.alive) continue;
                            if (distance(p.pos, blastTarget.pos) <= p.blastRadius) {
                                damageMonster(game, blastTarget, p.damage);
                            }
                        }
                        p.alive = false;
                        break;
                    }
                    damageMonster(game, monster, p.damage);
                    if (p.poison && monster.alive) {
                        monster.poisonTimer = std::max(monster.poisonTimer, p.poisonDuration);
                        monster.poisonTickTimer = 0.0f;
                        game.poisonClouds.push_back({ p.pos, 52.0f, 3.2f, true });
                    }
                    p.alive = false;
                    break;
                }
            }
        } else if (distance(p.pos, game.player.pos) <= p.radius + PLAYER_RADIUS) {
            int incoming = p.damage;
            if (game.player.jobClass == JobClass::Elite) incoming = std::max(1, static_cast<int>(std::round(incoming * 0.72f)));
            if (game.hasArmor) incoming = std::max(1, static_cast<int>(std::round(incoming * 0.72f)));
            game.player.hp -= incoming;
            p.alive = false;
        }
    }
}

void updateMonsters(GameState& game, float dt) {
    for (auto& monster : game.monsters) {
        if (!monster.alive) continue;

        monster.slowTimer = std::max(0.0f, monster.slowTimer - dt);

        if (monster.poisonTimer > 0.0f) {
            monster.poisonTimer = std::max(0.0f, monster.poisonTimer - dt);
            monster.poisonTickTimer -= dt;
            if (monster.poisonTickTimer <= 0.0f) {
                int poisonDamage = monster.type == MonsterType::Boss ? 3 : 4;
                if (game.player.jobClass == JobClass::Elite) poisonDamage += 3;
                if (game.hasPoisonUpgrade) poisonDamage += 5;
                damageMonster(game, monster, poisonDamage);
                monster.poisonTickTimer = 0.55f;
            }
            if (!monster.alive) continue;
        }

        if (monster.type == MonsterType::Boss && !monster.enraged && monster.hp <= monster.maxHp / 2) {
            monster.enraged = true;
            monster.damage += 10 + game.day;
            monster.speed += 28.0f;
            if (monster.attackCooldown > 0.35f) monster.attackCooldown = 0.35f;
            game.bossShakeTimer = std::max(game.bossShakeTimer, 0.9f);
            addMessage(game, "大魔王進入第二階段，攻勢全面暴走！", 5.5f);
            if (gBossIntroSample) al_play_sample(gBossIntroSample, 0.72f, 0.0f, 1.16f, ALLEGRO_PLAYMODE_ONCE, nullptr);
        }

        Vec2 target = game.player.pos;
        float nearest = distance(monster.pos, game.player.pos);
        for (const auto& building : game.buildings) {
            if (!building.alive) continue;
            float d = distance(monster.pos, building.pos);
            if (d < nearest) { nearest = d; target = building.pos; }
        }
        if (distance(monster.pos, { BASE_X, BASE_Y }) < nearest) target = { BASE_X, BASE_Y };

        Vec2 dir = normalize(sub(target, monster.pos));
        float speedScale = monster.slowTimer > 0.0f ? (monster.type == MonsterType::Boss ? 0.78f : 0.58f) : 1.0f;
        monster.vel = mul(dir, monster.speed * speedScale);
        monster.pos = add(monster.pos, mul(monster.vel, dt));
        monster.attackCooldown -= dt;

        if (monster.type == MonsterType::Hunter && monster.attackCooldown <= 0.0f && distance(monster.pos, game.player.pos) < 290.0f) {
            Projectile p {};
            p.friendly = false;
            p.damage = monster.damage;
            p.radius = 7.0f;
            p.life = 1.5f;
            p.pos = monster.pos;
            p.vel = mul(normalize(sub(game.player.pos, monster.pos)), 350.0f + difficultyFactor(game) * 12.0f + game.currentStage * 18.0f);
            game.projectiles.push_back(p);
            monster.attackCooldown = std::max(0.48f, 1.45f - game.day * 0.035f - game.currentStage * 0.08f);
        }
        if (monster.type == MonsterType::DarkMage && monster.attackCooldown <= 0.0f && distance(monster.pos, game.player.pos) < 420.0f) {
            Vec2 baseDir = normalize(sub(game.player.pos, monster.pos));
            for (int i = -3; i <= 3; ++i) {
                Vec2 spread { -baseDir.y * 0.14f * i, baseDir.x * 0.14f * i };
                Projectile p {};
                p.friendly = false;
                p.damage = monster.damage;
                p.radius = 9.0f;
                p.life = 1.9f;
                p.pos = monster.pos;
                p.vel = mul(normalize(add(baseDir, spread)), 290.0f);
                p.poison = true;
                p.poisonDuration = 4.8f;
                game.projectiles.push_back(p);
            }
            monster.attackCooldown = 1.8f;
        }
        if (monster.type == MonsterType::Boss && monster.attackCooldown <= 0.0f && distance(monster.pos, game.player.pos) < 380.0f) {
            Projectile p {};
            p.friendly = false;
            p.damage = monster.damage;
            p.radius = monster.enraged ? 14.0f : 11.0f;
            p.life = monster.enraged ? 2.1f : 1.8f;
            p.pos = monster.pos;
            p.vel = mul(normalize(sub(game.player.pos, monster.pos)), (monster.enraged ? 430.0f : 360.0f) + game.day * 4.0f);
            game.projectiles.push_back(p);
            if (monster.enraged) {
                for (int side = -1; side <= 1; side += 2) {
                    Vec2 toPlayer = normalize(sub(game.player.pos, monster.pos));
                    Vec2 spread { -toPlayer.y * 0.28f * side, toPlayer.x * 0.28f * side };
                    Projectile extra {};
                    extra.friendly = false;
                    extra.damage = std::max(1, monster.damage - 4);
                    extra.radius = 10.0f;
                    extra.life = 1.7f;
                    extra.pos = monster.pos;
                    extra.vel = mul(normalize(add(toPlayer, spread)), 390.0f + game.day * 3.0f);
                    game.projectiles.push_back(extra);
                }
            }
            monster.attackCooldown = monster.enraged ? 0.52f : 0.78f;
        }

        bool didAttack = false;
        if (distance(monster.pos, game.player.pos) <= PLAYER_RADIUS + 22.0f && monster.attackCooldown <= 0.0f) {
            int incoming = monster.damage;
            if (game.player.jobClass == JobClass::Elite) incoming = std::max(1, static_cast<int>(std::round(incoming * 0.72f)));
            if (game.hasArmor) incoming = std::max(1, static_cast<int>(std::round(incoming * 0.72f)));
            game.player.hp -= incoming;
            monster.attackCooldown = monster.type == MonsterType::Boss ? 0.7f : 0.92f;
            didAttack = true;
        }

        if (!didAttack) {
            for (auto& building : game.buildings) {
                if (!building.alive) continue;
                if (distance(monster.pos, building.pos) <= building.radius + 20.0f && monster.attackCooldown <= 0.0f) {
                    building.hp -= monster.damage;
                    monster.attackCooldown = monster.type == MonsterType::Boss ? 0.62f : 0.8f;
                    didAttack = true;
                    break;
                }
            }
        }

        if (!didAttack && distance(monster.pos, { BASE_X, BASE_Y }) <= 60.0f && monster.attackCooldown <= 0.0f) {
            game.player.cores -= monster.damage;
            monster.attackCooldown = monster.type == MonsterType::Boss ? 0.55f : 0.72f;
        }
    }
}

void updateBuildings(GameState& game, float dt) {
    for (auto& building : game.buildings) {
        if (!building.alive) continue;
        if (building.hp <= 0) { building.alive = false; continue; }
        building.fireCooldown -= dt;
        building.summonCooldown -= dt;

        if (building.type == BuildingType::Camp) {
            if (distance(game.player.pos, building.pos) < 100.0f && game.player.hp < game.player.maxHp) {
                game.player.hp = std::min(game.player.maxHp, game.player.hp + static_cast<int>((18.0f + game.day * 0.3f) * dt));
            }
        }

        if (building.type == BuildingType::Turret) {
            Monster* target = nullptr;
            float bestDist = 320.0f + game.day * 3.0f;
            for (auto& monster : game.monsters) {
                if (!monster.alive) continue;
                float d = distance(building.pos, monster.pos);
                if (d < bestDist) { bestDist = d; target = &monster; }
            }
            if (target && building.fireCooldown <= 0.0f) {
                Projectile p {};
                p.friendly = true;
                p.damage = 12 + game.player.level * 2 + game.day / 2;
                p.radius = 5.0f;
                p.life = 1.3f;
                p.pos = building.pos;
                p.vel = mul(normalize(sub(target->pos, building.pos)), 500.0f);
                game.projectiles.push_back(p);
                building.fireCooldown = std::max(0.22f, 0.55f - game.day * 0.01f);
            }
        } else if (building.type == BuildingType::DarkMage) {
            Monster* target = nullptr;
            float bestDist = 380.0f + game.day * 5.0f;
            for (auto& monster : game.monsters) {
                if (!monster.alive) continue;
                float d = distance(building.pos, monster.pos);
                if (d < bestDist) { bestDist = d; target = &monster; }
            }
            if (target && building.fireCooldown <= 0.0f) {
                Vec2 baseDir = normalize(sub(target->pos, building.pos));
                for (int i = -2; i <= 2; ++i) {
                    Vec2 spread { -baseDir.y * 0.11f * i, baseDir.x * 0.11f * i };
                    Projectile p {};
                    p.friendly = true;
                    p.damage = 8 + game.player.level + game.day / 2;
                    p.radius = 8.0f;
                    p.life = 1.25f;
                    p.pos = add(building.pos, mul(normalize(add(baseDir, spread)), 18.0f));
                    p.vel = mul(normalize(add(baseDir, spread)), 360.0f);
                    p.poison = true;
                    p.poisonDuration = game.hasPoisonUpgrade ? 6.2f : 4.2f;
                    game.projectiles.push_back(p);
                }
                building.fireCooldown = 1.45f;
            }
            if (building.summonCooldown <= 0.0f) {
                Minion minion {};
                minion.pos = add(building.pos, { randf(game, -26.0f, 26.0f), randf(game, -26.0f, 26.0f) });
                minion.speed = 155.0f;
                minion.gatherOnly = true;
                game.minions.push_back(minion);
                building.summonCooldown = 60.0f;
                addMessage(game, "黑暗小法師召喚出一隻小伐木者。", 2.8f);
            }
        }
    }
}

void updateResources(GameState& game, float dt) {
    for (auto& node : game.resources) {
        if (node.alive) continue;
        node.respawnTimer -= dt;
        if (node.respawnTimer <= 0.0f) {
            node.alive = true;
            node.hp = node.maxHp;
        }
    }
}

void cleanupDead(GameState& game) {
    game.projectiles.erase(std::remove_if(game.projectiles.begin(), game.projectiles.end(), [](const Projectile& p) { return !p.alive; }), game.projectiles.end());
    game.poisonClouds.erase(std::remove_if(game.poisonClouds.begin(), game.poisonClouds.end(), [](const PoisonCloud& c) { return !c.alive; }), game.poisonClouds.end());
    game.monsters.erase(std::remove_if(game.monsters.begin(), game.monsters.end(), [](const Monster& m) { return !m.alive; }), game.monsters.end());
}

void updatePlayer(GameState& game, float dt) {
    Vec2 move {};
    if (game.controls.up) move.y -= 1.0f;
    if (game.controls.down) move.y += 1.0f;
    if (game.controls.left) move.x -= 1.0f;
    if (game.controls.right) move.x += 1.0f;
    move = normalize(move);

    if (game.player.onBoat && game.player.boatIndex >= 0 && game.player.boatIndex < static_cast<int>(game.boats.size())) {
        Boat& boat = game.boats[game.player.boatIndex];
        Vec2 boatMove = move;
        if (length(boatMove) > 0.01f) {
            Vec2 tryPos = add(boat.pos, mul(boatMove, 190.0f * dt));
            tryPos.x = clampf(tryPos.x, 60.0f, WORLD_PIXEL_W - 60.0f);
            tryPos.y = clampf(tryPos.y, 60.0f, WORLD_PIXEL_H - 60.0f);
            if (isWaterTile(game, tryPos)) {
                boat.pos = tryPos;
                if (std::abs(boatMove.x) > 0.01f) boat.dir = boatMove.x > 0.0f ? 1.0f : -1.0f;
                game.player.facing = boatMove;
            }
        } else if (std::abs(boat.dir) > 0.01f) {
            game.player.facing = { boat.dir, 0.0f };
        }
        game.player.pos = add(boat.pos, { 0.0f, -20.0f });
    } else {
        game.player.onBoat = false;
        game.player.boatIndex = -1;
        Vec2 nextPos = add(game.player.pos, mul(move, game.player.speed * dt));
        nextPos.x = clampf(nextPos.x, PLAYER_RADIUS, WORLD_PIXEL_W - PLAYER_RADIUS);
        nextPos.y = clampf(nextPos.y, PLAYER_RADIUS, WORLD_PIXEL_H - PLAYER_RADIUS);
        if (isPassable(game, nextPos)) game.player.pos = nextPos;
        if (length(move) > 0.05f) game.player.facing = move;
    }

    game.player.attackCooldown -= dt;
    game.player.shootCooldown -= dt;
    game.player.gatherCooldown -= dt;
    game.player.potionCooldown -= dt;
    game.player.fishCooldown -= dt;
    game.player.bombCooldown -= dt;
    game.player.coreRecallCooldown -= dt;
    game.player.alienBurstCooldown -= dt;
    game.player.alienSummonCooldown -= dt;
    game.player.hunger = std::max(0.0f, game.player.hunger - dt * 1.15f);
    if (game.player.hunger <= 0.0f) {
        game.player.hungerDamageTimer -= dt;
        if (game.player.hungerDamageTimer <= 0.0f) {
            game.player.hp -= 4;
            game.player.hungerDamageTimer = 1.1f;
            addMessage(game, "你太餓了，不吃肉或菜就會持續掉血。", 2.5f);
        }
    } else {
        game.player.hungerDamageTimer = 0.0f;
    }

    Vec2 camera = getCamera(game);
    Vec2 mouseWorld = screenToWorld({ game.mouseScreenX, game.mouseScreenY }, camera);
    Vec2 aimDir = normalize(sub(mouseWorld, game.player.pos));
    if (length(aimDir) > 0.01f) game.player.facing = aimDir;

    if (game.controls.melee && game.player.attackCooldown <= 0.0f) {
        for (auto& monster : game.monsters) {
            if (!monster.alive) continue;
            if (distance(game.player.pos, monster.pos) < 70.0f) {
                Vec2 toward = normalize(sub(monster.pos, game.player.pos));
                if (toward.x * game.player.facing.x + toward.y * game.player.facing.y > 0.1f) {
                    int meleeDamage = 18 + game.player.level * 4 + game.day / 2 + (game.hasSword ? 16 : 0);
                    damageMonster(game, monster, meleeDamage);
                }
            }
        }
        game.player.attackCooldown = 0.35f;
    }

    if (!game.buildMode && game.controls.shoot && game.player.shootCooldown <= 0.0f) {
        Projectile p {};
        p.friendly = true;
        p.damage = 14 + game.player.level * 2 + game.day / 3;
        p.radius = 6.0f;
        p.life = 1.2f;
        p.pos = add(game.player.pos, mul(game.player.facing, 24.0f));
        p.vel = mul(game.player.facing, 520.0f);
        game.projectiles.push_back(p);
        if (gGunshotSample) {
            al_play_sample(gGunshotSample, 0.78f, 0.0f, 1.0f, ALLEGRO_PLAYMODE_ONCE, nullptr);
        }
        game.player.shootCooldown = std::max(0.1f, 0.18f - game.player.level * 0.003f);
    }

    if (game.controls.fish && game.player.fishCooldown <= 0.0f) {
        Vec2 fishPoint = add(game.player.pos, mul(game.player.facing, 84.0f));
        if (isWaterTile(game, fishPoint)) {
            int catchRoll = randi(game, 0, 99);
            if (catchRoll < 72) {
                game.bag.fish += 1;
                addMessage(game, "你釣到了一條魚。", 2.6f);
            } else {
                addMessage(game, "魚跑掉了，再試一次。", 2.2f);
            }
        } else {
            addMessage(game, "按 H 釣魚時，請朝水面方向站近一點。", 2.4f);
        }
        game.player.fishCooldown = 1.6f;
        game.controls.fish = false;
    }

    game.activeShop = detectNearbyShop(game);
    game.shopOpen = game.activeShop != ShopType::None;
}

void updatePoisonClouds(GameState& game, float dt) {
    for (auto& cloud : game.poisonClouds) {
        if (!cloud.alive) continue;
        cloud.life -= dt;
        if (cloud.life <= 0.0f) {
            cloud.alive = false;
            continue;
        }
        for (auto& monster : game.monsters) {
            if (!monster.alive) continue;
            if (distance(monster.pos, cloud.pos) <= cloud.radius + 14.0f) {
                monster.poisonTimer = std::max(monster.poisonTimer, 1.8f);
                monster.poisonTickTimer = std::min(monster.poisonTickTimer, 0.18f);
                monster.slowTimer = std::max(monster.slowTimer, 0.35f);
            }
        }
    }
}

void updateBoats(GameState& game, float dt) {
    for (int i = 0; i < static_cast<int>(game.boats.size()); ++i) {
        auto& boat = game.boats[i];
        if (!(game.player.onBoat && game.player.boatIndex == i)) {
            float tryX = boat.pos.x + boat.dir * 120.0f * dt;
            if (!isWaterTile(game, { tryX, boat.pos.y })) {
                boat.dir *= -1.0f;
                tryX = boat.pos.x + boat.dir * 120.0f * dt;
            }
            boat.pos.x = clampf(tryX, 60.0f, WORLD_PIXEL_W - 60.0f);
        }
        boat.fireCooldown -= dt;

        Monster* target = nullptr;
        float bestDist = 520.0f;
        for (auto& monster : game.monsters) {
            if (!monster.alive) continue;
            float d = distance(boat.pos, monster.pos);
            if (d < bestDist) {
                bestDist = d;
                target = &monster;
            }
        }
        if (target && boat.fireCooldown <= 0.0f) {
            Vec2 baseDir = normalize(sub(target->pos, boat.pos));
            for (int side = -1; side <= 1; side += 2) {
                Vec2 spread { -baseDir.y * 0.12f * side, baseDir.x * 0.12f * side };
                Projectile p {};
                p.friendly = true;
                p.damage = 18 + game.day / 2;
                p.radius = 8.0f;
                p.life = 1.6f;
                p.pos = add(boat.pos, { 0.0f, side * 6.0f });
                p.vel = mul(normalize(add(baseDir, spread)), 470.0f);
                game.projectiles.push_back(p);
            }
            boat.fireCooldown = 0.82f;
        }
    }
}

void updateMinions(GameState& game, float dt) {
    for (auto& minion : game.minions) {
        if (!minion.alive) continue;
        minion.attackCooldown -= dt;
        minion.gatherCooldown -= dt;

        if (!minion.gatherOnly) {
            Monster* targetMonster = nullptr;
            float bestMonsterDist = 220.0f;
            for (auto& monster : game.monsters) {
                if (!monster.alive) continue;
                float d = distance(minion.pos, monster.pos);
                if (d < bestMonsterDist) {
                    bestMonsterDist = d;
                    targetMonster = &monster;
                }
            }
            if (targetMonster) {
                Vec2 dir = normalize(sub(targetMonster->pos, minion.pos));
                minion.pos = add(minion.pos, mul(dir, minion.speed * dt));
                if (bestMonsterDist < 52.0f && minion.attackCooldown <= 0.0f) {
                    damageMonster(game, *targetMonster, 10 + game.day / 3);
                    minion.attackCooldown = 0.75f;
                }
                continue;
            }
        }

        ResourceNode* targetNode = nullptr;
        float bestNodeDist = minion.gatherOnly ? 340.0f : 180.0f;
        for (auto& node : game.resources) {
            if (!node.alive) continue;
            float d = distance(minion.pos, node.pos);
            if (d < bestNodeDist) {
                bestNodeDist = d;
                targetNode = &node;
            }
        }
        if (targetNode) {
            Vec2 dir = normalize(sub(targetNode->pos, minion.pos));
            minion.pos = add(minion.pos, mul(dir, minion.speed * dt));
            if (bestNodeDist < 50.0f && minion.gatherCooldown <= 0.0f) {
                if (targetNode->type == ResourceType::Tree) game.bag.wood += minion.gatherOnly ? 10 : 8;
                else if (targetNode->type == ResourceType::Rock) game.bag.stone += minion.gatherOnly ? 10 : 8;
                else if (targetNode->type == ResourceType::Bush) {
                    game.bag.fiber += minion.gatherOnly ? 5 : 4;
                    game.bag.vegetable += 1;
                } else if (targetNode->type == ResourceType::Crystal) game.bag.crystal += minion.gatherOnly ? 4 : 3;
                targetNode->alive = false;
                targetNode->respawnTimer = 22.0f;
                minion.gatherCooldown = minion.gatherOnly ? 1.6f : 2.0f;
            }
        } else {
            Vec2 dir = normalize(sub(game.player.pos, minion.pos));
            minion.pos = add(minion.pos, mul(dir, minion.speed * dt * 0.7f));
        }
    }
}

void updateFlow(GameState& game, float dt) {
    game.dayTimer += dt;
    game.waveTimer -= dt;
    game.messageTimer -= dt;
    game.bossIntroTimer = std::max(0.0f, game.bossIntroTimer - dt);
    game.bossShakeTimer = std::max(0.0f, game.bossShakeTimer - dt);
    game.frontGateAlertTimer = std::max(0.0f, game.frontGateAlertTimer - dt);
    game.autoSaveTimer += dt;
    refreshDifficulty(game);

    if (game.dayTimer >= 120.0f) {
        game.day += 1;
        game.dayTimer = 0.0f;
        refreshDifficulty(game);
        saveGame(game);
        game.saveExists = true;
        addMessage(game, game.day % 5 == 0 ? "精英夜即將降臨，敵潮會更加兇猛。" : "新的一天開始了，夜晚的怪物會更兇猛。", 5.0f);
    }

    int stagePressure = game.currentStage == 1 ? 0 : (game.currentStage == 2 ? 4 : (game.currentStage == 3 ? 6 : (game.currentStage == 4 ? 9 : 12)));
    int desiredMonsters = isNight(game)
        ? static_cast<int>(4 + game.day * 2.5f + game.danger * 2.6f + stagePressure)
        : static_cast<int>(1 + game.day * 1.0f + game.danger * 0.9f + stagePressure / 2);
    if (game.day % 5 == 0 && isNight(game)) desiredMonsters += 5 + game.day / 2 + game.currentStage * 2;

    if (game.waveTimer <= 0.0f && static_cast<int>(game.monsters.size()) < desiredMonsters) {
        int spawnBatch = std::max(1, desiredMonsters - static_cast<int>(game.monsters.size()));
        spawnBatch = std::min(spawnBatch, game.currentStage >= 5 ? 6 : 4);
        spawnMonster(game, spawnBatch);
        game.waveTimer = isNight(game)
            ? std::max(0.55f, 2.0f - game.day * 0.05f - game.currentStage * 0.18f)
            : std::max(1.9f, 7.2f - game.day * 0.12f - game.currentStage * 0.22f);
    }

    if (game.autoSaveTimer >= 30.0f) {
        if (saveGame(game)) game.saveExists = true;
        game.autoSaveTimer = 0.0f;
    }

    int frontGateThreats = 0;
    for (const auto& monster : game.monsters) {
        if (!monster.alive) continue;
        if (monster.pos.y < BASE_Y - 80.0f && std::abs(monster.pos.x - BASE_X) < 340.0f && distance(monster.pos, { BASE_X, BASE_Y }) < 520.0f) {
            ++frontGateThreats;
        }
    }
    if (frontGateThreats >= 3 && game.frontGateAlertTimer <= 0.0f) {
        addMessage(game, frontGateThreats >= 6 ? "警報：前門敵潮正在猛攻，砲塔火力全開！" : "警報：前門出現敵群，準備迎擊！", 4.5f);
        game.frontGateAlertTimer = 6.0f;
    }

    if (game.currentStage == 1) {
        if (game.totalWoodGathered >= 50 && game.totalStoneGathered >= 36 && countBuildingsOfType(game, BuildingType::Wall) >= 6) {
            advanceStage(game);
        }
    } else if (game.currentStage == 2) {
        if (game.totalMonsterKills >= 50
            && countBuildingsOfType(game, BuildingType::Turret) >= 9
            && countBuildingsOfType(game, BuildingType::Camp) >= 3
            && countBuildingsOfType(game, BuildingType::DarkMage) >= 2) {
            advanceStage(game);
        }
    } else if (game.currentStage == 3) {
        for (auto& child : game.children) {
            if (!child.rescued && distance(game.player.pos, child.pos) < 60.0f) {
                child.rescued = true;
                game.rescuedChildren += 1;
                if (gRescueChildSample) al_play_sample(gRescueChildSample, 0.82f, 0.0f, 1.0f, ALLEGRO_PLAYMODE_ONCE, nullptr);
                addMessage(game, ("你救出了小孩，已救援 " + std::to_string(game.rescuedChildren) + " / 4。"), 4.0f);
            }
        }
        if (game.rescuedChildren >= 4) {
            advanceStage(game);
        }
    } else if (game.currentStage == 4) {
        if (!game.villageBaseDestroyed && distance(game.player.pos, villageBasePos()) < 88.0f && game.controls.melee && game.player.attackCooldown <= 0.0f) {
            game.villageBaseHp = std::max(0, game.villageBaseHp - (game.hasSword ? 26 : 18));
        }
        if (!game.villageBaseDestroyed && game.villageBaseHp <= 0) {
            game.villageBaseDestroyed = true;
            game.emeraldDropped = true;
            addMessage(game, "藍基地被摧毀，綠寶石掉落了！", 5.2f);
        }
        if (game.emeraldDropped && !game.carryingEmerald && distance(game.player.pos, villageBasePos()) < 92.0f) {
            game.emeraldDropped = false;
            game.carryingEmerald = true;
            addMessage(game, "你撿起了綠寶石，快帶回核心。", 4.6f);
        }
        if (game.carryingEmerald && distance(game.player.pos, { BASE_X, BASE_Y }) < 84.0f) {
            game.carryingEmerald = false;
            game.emeraldDelivered = true;
            if (!game.darkMageRewardGranted) {
                game.buildings.push_back(makeBuilding(BuildingType::DarkMage, { BASE_X + 110.0f, BASE_Y - 180.0f }));
                game.darkMageRewardGranted = true;
                addMessage(game, "綠寶石送回核心，你得到一隻黑暗大法師。", 5.5f);
            }
        }
        if (game.stage4BanditKills >= 18 && game.stage4DarkMageKills >= 3 && game.emeraldDelivered) {
            advanceStage(game);
        }
    } else if (game.currentStage == 5 && !game.bossSpawned && distance(game.player.pos, bossZoneCenter()) < 150.0f) {
        Monster boss {};
        boss.type = MonsterType::Boss;
        boss.pos = add(bossZoneCenter(), { 0.0f, -20.0f });
        boss.hp = boss.maxHp = 700 + game.day * 34;
        boss.damage = 30 + game.day * 3;
        boss.speed = 108.0f;
        boss.attackCooldown = 0.0f;
        game.monsters.push_back(boss);
        game.bossSpawned = true;
        game.bossIntroTimer = 3.6f;
        game.bossShakeTimer = 1.5f;
        if (gBossIntroSample) al_play_sample(gBossIntroSample, 0.95f, 0.0f, 1.0f, ALLEGRO_PLAYMODE_ONCE, nullptr);
        addMessage(game, "警告：黑色禁地震動，大魔王現身！", 6.5f);
    }
}

void drawProgressBar(float x, float y, float w, float h, float t, ALLEGRO_COLOR bg, ALLEGRO_COLOR fg) {
    al_draw_filled_rounded_rectangle(x, y, x + w, y + h, 8.0f, 8.0f, bg);
    al_draw_filled_rounded_rectangle(x + 2.0f, y + 2.0f, x + 2.0f + (w - 4.0f) * clampf(t, 0.0f, 1.0f), y + h - 2.0f, 6.0f, 6.0f, fg);
}

void drawWorld(const GameState& game, ALLEGRO_FONT* bodyFont, ALLEGRO_FONT* titleFont) {
    Vec2 camera = getCamera(game);
    int startX = static_cast<int>(camera.x / TILE_SIZE);
    int startY = static_cast<int>(camera.y / TILE_SIZE);
    int endX = std::min(WORLD_W - 1, startX + SCREEN_W / static_cast<int>(TILE_SIZE) + 2);
    int endY = std::min(WORLD_H - 1, startY + SCREEN_H / static_cast<int>(TILE_SIZE) + 2);

    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            Vec2 screen = worldToScreen({ x * TILE_SIZE, y * TILE_SIZE }, camera);
            al_draw_filled_rectangle(screen.x, screen.y, screen.x + TILE_SIZE + 1.0f, screen.y + TILE_SIZE + 1.0f, biomeColor(game.map[y][x]));
        }
    }

    for (const auto& node : game.resources) {
        if (!node.alive) continue;
        Vec2 p = worldToScreen(node.pos, camera);
        if (p.x < -80.0f || p.y < -80.0f || p.x > SCREEN_W + 80.0f || p.y > SCREEN_H + 80.0f) continue;
        ALLEGRO_COLOR c = resourceColor(node.type);
        if (node.type == ResourceType::Tree) {
            al_draw_filled_triangle(p.x, p.y - 26.0f, p.x - 22.0f, p.y + 18.0f, p.x + 22.0f, p.y + 18.0f, c);
            al_draw_filled_rectangle(p.x - 6.0f, p.y + 18.0f, p.x + 6.0f, p.y + 36.0f, al_map_rgb(92, 52, 26));
        } else if (node.type == ResourceType::Rock) {
            al_draw_filled_circle(p.x, p.y, 22.0f, c);
            al_draw_filled_circle(p.x - 18.0f, p.y + 8.0f, 11.0f, c);
            al_draw_filled_circle(p.x + 16.0f, p.y + 10.0f, 12.0f, c);
        } else if (node.type == ResourceType::Bush) {
            al_draw_filled_circle(p.x - 10.0f, p.y + 8.0f, 14.0f, c);
            al_draw_filled_circle(p.x + 10.0f, p.y + 6.0f, 15.0f, c);
            al_draw_filled_circle(p.x, p.y - 4.0f, 15.0f, c);
        } else {
            al_draw_filled_triangle(p.x, p.y - 26.0f, p.x - 16.0f, p.y + 10.0f, p.x + 16.0f, p.y + 10.0f, c);
            al_draw_filled_triangle(p.x, p.y + 30.0f, p.x - 12.0f, p.y + 8.0f, p.x + 12.0f, p.y + 8.0f, al_map_rgb(70, 191, 230));
        }
    }

    al_draw_filled_circle(BASE_X - camera.x, BASE_Y - camera.y, 48.0f, al_map_rgb(222, 187, 84));
    al_draw_circle(BASE_X - camera.x, BASE_Y - camera.y, 58.0f, al_map_rgb(255, 230, 155), 4.0f);
    al_draw_text(titleFont, al_map_rgb(34, 24, 12), BASE_X - camera.x, BASE_Y - camera.y - 16.0f, ALLEGRO_ALIGN_CENTER, "核心");
    Vec2 shrine = worldToScreen(shrinePos(), camera);
    al_draw_filled_triangle(shrine.x, shrine.y - 52.0f, shrine.x - 44.0f, shrine.y - 12.0f, shrine.x + 44.0f, shrine.y - 12.0f, al_map_rgb(156, 58, 58));
    al_draw_filled_rectangle(shrine.x - 34.0f, shrine.y - 12.0f, shrine.x + 34.0f, shrine.y + 34.0f, al_map_rgb(236, 222, 204));
    al_draw_filled_rectangle(shrine.x - 10.0f, shrine.y - 2.0f, shrine.x + 10.0f, shrine.y + 34.0f, al_map_rgb(118, 72, 52));
    al_draw_filled_circle(shrine.x, shrine.y - 26.0f, 10.0f, al_map_rgb(214, 38, 56));
    al_draw_text(bodyFont, al_map_rgb(255, 240, 220), shrine.x, shrine.y - 86.0f, ALLEGRO_ALIGN_CENTER, "神社");
    if (distance(game.player.pos, shrinePos()) <= 96.0f) {
        al_draw_circle(shrine.x, shrine.y - 6.0f, 58.0f, al_map_rgba(255, 120, 140, 140), 3.0f);
    }
    if (game.currentStage >= 4) {
        Vec2 village = worldToScreen(villageCenter(), camera);
        Vec2 base = worldToScreen(villageBasePos(), camera);
        al_draw_filled_rectangle(village.x - 200.0f, village.y - 130.0f, village.x + 220.0f, village.y + 120.0f, al_map_rgba(92, 70, 58, 96));
        al_draw_line(village.x - 220.0f, village.y + 48.0f, village.x + 240.0f, village.y + 48.0f, al_map_rgb(120, 98, 68), 10.0f);
        al_draw_filled_triangle(village.x - 120.0f, village.y - 70.0f, village.x - 164.0f, village.y + 8.0f, village.x - 76.0f, village.y + 8.0f, al_map_rgb(136, 66, 54));
        al_draw_filled_rectangle(village.x - 148.0f, village.y + 8.0f, village.x - 92.0f, village.y + 58.0f, al_map_rgb(88, 64, 44));
        al_draw_filled_triangle(village.x - 10.0f, village.y - 48.0f, village.x - 52.0f, village.y + 24.0f, village.x + 32.0f, village.y + 24.0f, al_map_rgb(126, 60, 48));
        al_draw_filled_rectangle(village.x - 40.0f, village.y + 24.0f, village.x + 18.0f, village.y + 72.0f, al_map_rgb(82, 58, 40));
        al_draw_text(bodyFont, al_map_rgb(255, 222, 204), village.x, village.y - 162.0f, ALLEGRO_ALIGN_CENTER, "損壞的村莊");

        if (!game.villageBaseDestroyed) {
            al_draw_filled_rectangle(base.x - 42.0f, base.y - 42.0f, base.x + 42.0f, base.y + 42.0f, al_map_rgb(44, 98, 178));
            al_draw_filled_circle(base.x, base.y - 14.0f, 18.0f, al_map_rgb(122, 198, 255));
            al_draw_text(bodyFont, al_map_rgb(232, 244, 255), base.x, base.y - 82.0f, ALLEGRO_ALIGN_CENTER, "藍基地");
            drawProgressBar(base.x - 40.0f, base.y - 62.0f, 80.0f, 10.0f, game.villageBaseMaxHp > 0 ? static_cast<float>(game.villageBaseHp) / game.villageBaseMaxHp : 0.0f, al_map_rgba(30, 30, 30, 180), al_map_rgb(98, 188, 255));
        } else if (game.emeraldDropped) {
            al_draw_filled_circle(base.x, base.y, 14.0f, al_map_rgb(74, 232, 126));
            al_draw_circle(base.x, base.y, 24.0f, al_map_rgba(120, 255, 162, 160), 3.0f);
            al_draw_text(bodyFont, al_map_rgb(226, 255, 232), base.x, base.y - 44.0f, ALLEGRO_ALIGN_CENTER, "綠寶石");
        }
    }
    Vec2 shop = worldToScreen(shopPos(), camera);
    Vec2 grocery = worldToScreen(groceryPos(), camera);
    Vec2 tool = worldToScreen(toolShopPos(), camera);
    al_draw_filled_rectangle(shop.x - 72.0f, shop.y + 34.0f, tool.x + 72.0f, tool.y + 46.0f, al_map_rgb(118, 94, 66));
    al_draw_filled_rectangle(shop.x - 80.0f, shop.y + 46.0f, tool.x + 80.0f, tool.y + 56.0f, al_map_rgb(86, 66, 42));

    al_draw_filled_rectangle(shop.x - 34.0f, shop.y - 26.0f, shop.x + 34.0f, shop.y + 28.0f, al_map_rgb(118, 78, 38));
    al_draw_filled_triangle(shop.x, shop.y - 48.0f, shop.x - 42.0f, shop.y - 18.0f, shop.x + 42.0f, shop.y - 18.0f, al_map_rgb(210, 72, 48));
    al_draw_filled_rectangle(shop.x - 12.0f, shop.y - 2.0f, shop.x + 12.0f, shop.y + 28.0f, al_map_rgb(78, 44, 22));
    al_draw_text(bodyFont, al_map_rgb(255, 244, 214), shop.x, shop.y - 70.0f, ALLEGRO_ALIGN_CENTER, "炸彈店");

    al_draw_filled_rectangle(grocery.x - 34.0f, grocery.y - 26.0f, grocery.x + 34.0f, grocery.y + 28.0f, al_map_rgb(102, 128, 62));
    al_draw_filled_triangle(grocery.x, grocery.y - 48.0f, grocery.x - 42.0f, grocery.y - 18.0f, grocery.x + 42.0f, grocery.y - 18.0f, al_map_rgb(232, 226, 124));
    al_draw_filled_rectangle(grocery.x - 12.0f, grocery.y - 2.0f, grocery.x + 12.0f, grocery.y + 28.0f, al_map_rgb(74, 66, 24));
    al_draw_text(bodyFont, al_map_rgb(255, 248, 230), grocery.x, grocery.y - 70.0f, ALLEGRO_ALIGN_CENTER, "超市");

    al_draw_filled_rectangle(tool.x - 34.0f, tool.y - 26.0f, tool.x + 34.0f, tool.y + 28.0f, al_map_rgb(92, 96, 114));
    al_draw_filled_triangle(tool.x, tool.y - 48.0f, tool.x - 42.0f, tool.y - 18.0f, tool.x + 42.0f, tool.y - 18.0f, al_map_rgb(132, 164, 210));
    al_draw_filled_rectangle(tool.x - 12.0f, tool.y - 2.0f, tool.x + 12.0f, tool.y + 28.0f, al_map_rgb(58, 64, 88));
    al_draw_text(bodyFont, al_map_rgb(236, 242, 255), tool.x, tool.y - 70.0f, ALLEGRO_ALIGN_CENTER, "工具行");

    for (const auto& shopEntry : std::array<Vec2, 3> { shopPos(), groceryPos(), toolShopPos() }) {
        if (distance(game.player.pos, shopEntry) <= 120.0f) {
            Vec2 glow = worldToScreen(shopEntry, camera);
            al_draw_circle(glow.x, glow.y - 8.0f, 54.0f, al_map_rgba(255, 214, 130, 120), 3.0f);
        }
    }

    for (const auto& child : game.children) {
        if (child.rescued) continue;
        Vec2 p = worldToScreen(child.pos, camera);
        al_draw_filled_circle(p.x, p.y, 18.0f, al_map_rgb(255, 224, 146));
        al_draw_filled_circle(p.x, p.y - 20.0f, 10.0f, al_map_rgb(255, 212, 186));
        float markerBob = std::sin(game.dayTimer * 4.0f + child.pos.x * 0.01f) * 6.0f;
        al_draw_filled_circle(p.x, p.y - 52.0f + markerBob, 14.0f, al_map_rgb(255, 96, 96));
        al_draw_text(bodyFont, al_map_rgb(255, 250, 240), p.x, p.y - 61.0f + markerBob, ALLEGRO_ALIGN_CENTER, "!");
        al_draw_text(bodyFont, al_map_rgb(255, 248, 230), p.x, p.y + 22.0f, ALLEGRO_ALIGN_CENTER, "小孩");
    }

    for (const auto& boat : game.boats) {
        Vec2 p = worldToScreen(boat.pos, camera);
        al_draw_filled_rectangle(p.x - 34.0f, p.y - 12.0f, p.x + 34.0f, p.y + 12.0f, al_map_rgb(132, 96, 56));
        al_draw_filled_triangle(p.x + boat.dir * 40.0f, p.y, p.x + boat.dir * 18.0f, p.y - 14.0f, p.x + boat.dir * 18.0f, p.y + 14.0f, al_map_rgb(166, 124, 72));
        al_draw_filled_rectangle(p.x - 4.0f, p.y - 26.0f, p.x + 4.0f, p.y + 2.0f, al_map_rgb(220, 220, 228));
    }

    if (game.currentStage >= 5 || game.bossSpawned || game.bossDefeated) {
        Vec2 zone = worldToScreen(bossZoneCenter(), camera);
        float zonePulse = game.bossSpawned && !game.bossDefeated ? 10.0f + std::sin(game.dayTimer * 5.2f) * 8.0f : 0.0f;
        if (game.bossIntroTimer > 0.0f) {
            float introRing = 120.0f + (3.6f - game.bossIntroTimer) * 90.0f;
            al_draw_circle(zone.x, zone.y, introRing, al_map_rgba(214, 62, 62, 220), 8.0f);
            al_draw_circle(zone.x, zone.y, introRing - 24.0f, al_map_rgba(255, 240, 240, 120), 4.0f);
        }
        al_draw_filled_circle(zone.x, zone.y, 84.0f + zonePulse, al_map_rgba(14, 14, 18, 220));
        al_draw_circle(zone.x, zone.y, 96.0f + zonePulse * 0.5f, al_map_rgb(78, 78, 88), 5.0f);
        if (game.bossSpawned && !game.bossDefeated) {
            float crackReach = 112.0f + std::sin(game.dayTimer * 4.0f) * 8.0f;
            for (int i = 0; i < 6; ++i) {
                float ang = 0.45f + i * 1.02f;
                Vec2 a { zone.x + std::cos(ang) * 42.0f, zone.y + std::sin(ang) * 42.0f };
                Vec2 b { zone.x + std::cos(ang) * crackReach, zone.y + std::sin(ang) * crackReach };
                Vec2 c { zone.x + std::cos(ang + 0.18f) * (crackReach - 24.0f), zone.y + std::sin(ang + 0.18f) * (crackReach - 24.0f) };
                al_draw_line(a.x, a.y, b.x, b.y, al_map_rgb(32, 8, 8), 5.0f);
                al_draw_line(b.x, b.y, c.x, c.y, al_map_rgb(58, 14, 14), 3.0f);
            }
        }
        al_draw_text(bodyFont, al_map_rgb(220, 220, 225), zone.x, zone.y - 16.0f, ALLEGRO_ALIGN_CENTER, "黑色地點");
    }

    for (const auto& building : game.buildings) {
        if (!building.alive) continue;
        Vec2 p = worldToScreen(building.pos, camera);
        if (building.type == BuildingType::Wall) al_draw_filled_rectangle(p.x - 28.0f, p.y - 20.0f, p.x + 28.0f, p.y + 20.0f, al_map_rgb(137, 115, 96));
        else if (building.type == BuildingType::Turret) {
            al_draw_filled_circle(p.x, p.y, 22.0f, al_map_rgb(101, 105, 118));
            al_draw_filled_rectangle(p.x - 5.0f, p.y - 26.0f, p.x + 5.0f, p.y - 2.0f, al_map_rgb(188, 194, 207));
        } else if (building.type == BuildingType::Camp) {
            al_draw_filled_triangle(p.x, p.y - 36.0f, p.x - 34.0f, p.y + 30.0f, p.x + 34.0f, p.y + 30.0f, al_map_rgb(199, 110, 62));
            al_draw_filled_rectangle(p.x - 10.0f, p.y + 6.0f, p.x + 10.0f, p.y + 30.0f, al_map_rgb(95, 54, 24));
        } else if (building.type == BuildingType::DarkMage) {
            al_draw_filled_circle(p.x, p.y + 6.0f, 22.0f, al_map_rgb(70, 46, 96));
            al_draw_filled_triangle(p.x, p.y - 34.0f, p.x - 24.0f, p.y + 18.0f, p.x + 24.0f, p.y + 18.0f, al_map_rgb(42, 22, 56));
            al_draw_filled_circle(p.x, p.y - 38.0f, 10.0f, al_map_rgb(112, 255, 138));
            al_draw_circle(p.x, p.y - 2.0f, 30.0f, al_map_rgba(134, 255, 160, 150), 3.0f);
        }
        drawProgressBar(p.x - 28.0f, p.y - building.radius - 18.0f, 56.0f, 10.0f, static_cast<float>(building.hp) / building.maxHp, al_map_rgba(30, 30, 30, 180), al_map_rgb(106, 230, 126));
    }

    for (const auto& cloud : game.poisonClouds) {
        if (!cloud.alive) continue;
        Vec2 p = worldToScreen(cloud.pos, camera);
        float alpha = clampf(cloud.life / 3.2f, 0.0f, 1.0f);
        al_draw_filled_circle(p.x, p.y, cloud.radius, al_map_rgba_f(0.24f, 0.85f, 0.32f, 0.14f * alpha));
        al_draw_filled_circle(p.x - 18.0f, p.y + 6.0f, cloud.radius * 0.45f, al_map_rgba_f(0.48f, 1.0f, 0.58f, 0.11f * alpha));
        al_draw_filled_circle(p.x + 16.0f, p.y - 8.0f, cloud.radius * 0.38f, al_map_rgba_f(0.36f, 0.95f, 0.46f, 0.10f * alpha));
    }

    for (const auto& monster : game.monsters) {
        if (!monster.alive) continue;
        Vec2 p = worldToScreen(monster.pos, camera);
        ALLEGRO_COLOR c = al_map_rgb(164, 74, 72);
        if (monster.type == MonsterType::Hunter) c = al_map_rgb(145, 84, 167);
        if (monster.type == MonsterType::Brute) c = al_map_rgb(128, 50, 36);
        if (monster.type == MonsterType::DarkMage) c = al_map_rgb(56, 24, 82);
        if (monster.type == MonsterType::Boss) c = al_map_rgb(20, 20, 24);
        if (monster.poisonTimer > 0.0f) c = monster.type == MonsterType::Boss ? al_map_rgb(28, 68, 32) : al_map_rgb(74, 182, 84);
        float monsterRadius = monster.type == MonsterType::Brute ? 22.0f : 18.0f;
        if (monster.type == MonsterType::DarkMage) monsterRadius = 24.0f;
        if (monster.type == MonsterType::Boss) monsterRadius = 34.0f;
        al_draw_filled_circle(p.x, p.y, monsterRadius, c);
        if (monster.type == MonsterType::DarkMage) {
            al_draw_filled_circle(p.x, p.y - 26.0f, 10.0f, al_map_rgb(142, 255, 154));
            al_draw_circle(p.x, p.y, 32.0f, al_map_rgba(146, 78, 210, 160), 3.0f);
        }
        if (monster.type == MonsterType::Boss) {
            float bossPulse = 44.0f + std::sin(game.dayTimer * 8.0f) * 5.0f;
            al_draw_circle(p.x, p.y, bossPulse, al_map_rgb(168, 42, 42), 4.0f);
            al_draw_circle(p.x, p.y, bossPulse + 12.0f, al_map_rgba(255, 120, 120, 120), 3.0f);
            al_draw_filled_circle(p.x, p.y - 28.0f, 10.0f, al_map_rgb(188, 30, 30));
        }
        al_draw_filled_circle(p.x - 6.0f, p.y - 3.0f, 3.5f, al_map_rgb(255, 240, 230));
        al_draw_filled_circle(p.x + 6.0f, p.y - 3.0f, 3.5f, al_map_rgb(255, 240, 230));
        drawProgressBar(p.x - 24.0f, p.y - 34.0f, 48.0f, 8.0f, static_cast<float>(monster.hp) / monster.maxHp, al_map_rgba(25, 25, 25, 180), al_map_rgb(238, 91, 85));
    }

    for (const auto& p : game.projectiles) {
        Vec2 s = worldToScreen(p.pos, camera);
        ALLEGRO_COLOR projectileColor = p.friendly ? al_map_rgb(255, 220, 92) : al_map_rgb(226, 96, 191);
        if (p.poison) projectileColor = al_map_rgb(88, 232, 124);
        if (p.bomb) projectileColor = al_map_rgb(255, 132, 44);
        al_draw_filled_circle(s.x, s.y, p.radius, projectileColor);
    }

    for (const auto& minion : game.minions) {
        if (!minion.alive) continue;
        Vec2 p = worldToScreen(minion.pos, camera);
        if (minion.gatherOnly) {
            al_draw_filled_circle(p.x, p.y, 14.0f, al_map_rgb(176, 255, 178));
            al_draw_circle(p.x, p.y, 19.0f, al_map_rgb(72, 176, 92), 2.0f);
        } else {
            al_draw_filled_circle(p.x, p.y, 14.0f, al_map_rgb(160, 234, 255));
            al_draw_circle(p.x, p.y, 19.0f, al_map_rgb(88, 214, 250), 2.0f);
        }
    }

    Vec2 player = worldToScreen(game.player.pos, camera);
    ALLEGRO_COLOR bodyColor = jobBodyColor(game.player.jobClass);
    al_draw_filled_circle(player.x, player.y, PLAYER_RADIUS, bodyColor);
    al_draw_filled_circle(player.x, player.y - 24.0f, 11.0f, al_map_rgb(255, 222, 191));
    al_draw_line(player.x, player.y, player.x + game.player.facing.x * 30.0f, player.y + game.player.facing.y * 30.0f, al_map_rgb(255, 255, 255), 4.0f);

    if (game.player.jobClass == JobClass::Drifter) {
        al_draw_filled_rectangle(player.x - 12.0f, player.y - 14.0f, player.x + 12.0f, player.y - 4.0f, al_map_rgb(92, 124, 150));
    } else if (game.player.jobClass == JobClass::Vampire) {
        al_draw_filled_triangle(player.x - 14.0f, player.y - 30.0f, player.x - 2.0f, player.y - 52.0f, player.x + 4.0f, player.y - 28.0f, al_map_rgb(48, 16, 26));
        al_draw_filled_triangle(player.x + 14.0f, player.y - 30.0f, player.x + 2.0f, player.y - 52.0f, player.x - 4.0f, player.y - 28.0f, al_map_rgb(48, 16, 26));
        al_draw_line(player.x - 4.0f, player.y - 14.0f, player.x - 1.0f, player.y - 7.0f, al_map_rgb(255, 255, 255), 2.0f);
        al_draw_line(player.x + 4.0f, player.y - 14.0f, player.x + 1.0f, player.y - 7.0f, al_map_rgb(255, 255, 255), 2.0f);
    } else if (game.player.jobClass == JobClass::Lumberjack) {
        al_draw_filled_rectangle(player.x - 16.0f, player.y - 38.0f, player.x + 16.0f, player.y - 28.0f, al_map_rgb(166, 98, 44));
        al_draw_line(player.x + 16.0f, player.y - 4.0f, player.x + 30.0f, player.y - 22.0f, al_map_rgb(122, 78, 32), 4.0f);
        al_draw_filled_triangle(player.x + 30.0f, player.y - 22.0f, player.x + 18.0f, player.y - 28.0f, player.x + 18.0f, player.y - 12.0f, al_map_rgb(196, 204, 214));
    } else if (game.player.jobClass == JobClass::Fisher) {
        al_draw_filled_triangle(player.x, player.y - 48.0f, player.x - 16.0f, player.y - 20.0f, player.x + 16.0f, player.y - 20.0f, al_map_rgb(64, 108, 172));
        al_draw_line(player.x + 16.0f, player.y - 4.0f, player.x + 38.0f, player.y - 30.0f, al_map_rgb(140, 110, 72), 3.0f);
        al_draw_line(player.x + 38.0f, player.y - 30.0f, player.x + 44.0f, player.y - 10.0f, al_map_rgb(220, 236, 255), 2.0f);
    } else if (game.player.jobClass == JobClass::Alien) {
        al_draw_circle(player.x, player.y - 26.0f, 16.0f, al_map_rgb(176, 255, 232), 3.0f);
        al_draw_filled_circle(player.x - 7.0f, player.y - 26.0f, 4.0f, al_map_rgb(22, 42, 40));
        al_draw_filled_circle(player.x + 7.0f, player.y - 26.0f, 4.0f, al_map_rgb(22, 42, 40));
        al_draw_line(player.x - 8.0f, player.y - 38.0f, player.x - 16.0f, player.y - 54.0f, al_map_rgb(150, 255, 220), 2.0f);
        al_draw_line(player.x + 8.0f, player.y - 38.0f, player.x + 16.0f, player.y - 54.0f, al_map_rgb(150, 255, 220), 2.0f);
    } else if (game.player.jobClass == JobClass::Elite) {
        al_draw_circle(player.x, player.y - 2.0f, 24.0f, al_map_rgba(255, 226, 128, 180), 3.0f);
        al_draw_filled_rectangle(player.x - 14.0f, player.y - 42.0f, player.x + 14.0f, player.y - 34.0f, al_map_rgb(255, 214, 106));
        al_draw_filled_triangle(player.x, player.y - 54.0f, player.x - 8.0f, player.y - 34.0f, player.x + 8.0f, player.y - 34.0f, al_map_rgb(255, 214, 106));
    }

    if (game.buildMode) {
        Vec2 ghost = screenToWorld({ game.mouseScreenX, game.mouseScreenY }, camera);
        ghost.x = std::round(ghost.x / 16.0f) * 16.0f;
        ghost.y = std::round(ghost.y / 16.0f) * 16.0f;
        Vec2 s = worldToScreen(ghost, camera);
        float buildRadius = game.selectedBuild == BuildingType::Camp ? 36.0f : (game.selectedBuild == BuildingType::DarkMage ? 30.0f : 28.0f);
        al_draw_circle(s.x, s.y, buildRadius, al_map_rgba(255, 255, 255, 180), 3.0f);
        al_draw_textf(bodyFont, al_map_rgb(255, 255, 255), s.x, s.y - 48.0f, ALLEGRO_ALIGN_CENTER, "放置：%s", buildName(game.selectedBuild));
    }

    float darkness = isNight(game) ? 0.34f : (game.dayTimer > 82.0f ? (game.dayTimer - 82.0f) / 13.0f * 0.2f : 0.0f);
    if (darkness > 0.01f) al_draw_filled_rectangle(0.0f, 0.0f, SCREEN_W, SCREEN_H, al_map_rgba_f(0.05f, 0.07f, 0.15f, darkness));
}

void drawMinimap(const GameState& game, float x, float y, float w, float h) {
    al_draw_filled_rounded_rectangle(x, y, x + w, y + h, 12.0f, 12.0f, al_map_rgba(10, 14, 16, 210));
    float scaleX = w / WORLD_PIXEL_W;
    float scaleY = h / WORLD_PIXEL_H;
    for (int ty = 0; ty < WORLD_H; ty += 2) {
        for (int tx = 0; tx < WORLD_W; tx += 2) {
            al_draw_filled_rectangle(x + tx * TILE_SIZE * scaleX, y + ty * TILE_SIZE * scaleY, x + (tx + 2) * TILE_SIZE * scaleX, y + (ty + 2) * TILE_SIZE * scaleY, biomeColor(game.map[ty][tx]));
        }
    }
    for (const auto& building : game.buildings) {
        if (!building.alive) continue;
        al_draw_filled_circle(x + building.pos.x * scaleX, y + building.pos.y * scaleY, 3.0f, al_map_rgb(246, 215, 123));
    }
    for (const auto& child : game.children) {
        if (child.rescued) continue;
        al_draw_filled_circle(x + child.pos.x * scaleX, y + child.pos.y * scaleY, 3.5f, al_map_rgb(126, 255, 154));
    }
    for (const auto& monster : game.monsters) {
        if (!monster.alive) continue;
        al_draw_filled_circle(x + monster.pos.x * scaleX, y + monster.pos.y * scaleY, 2.0f, al_map_rgb(255, 82, 82));
    }
    al_draw_filled_circle(x + game.player.pos.x * scaleX, y + game.player.pos.y * scaleY, 4.0f, al_map_rgb(80, 198, 255));
}

void drawUi(const GameState& game, ALLEGRO_FONT* bodyFont, ALLEGRO_FONT* titleFont) {
    const Monster* boss = nullptr;
    for (const auto& monster : game.monsters) {
        if (monster.alive && monster.type == MonsterType::Boss) {
            boss = &monster;
            break;
        }
    }

    al_draw_filled_rounded_rectangle(18.0f, 18.0f, 520.0f, 184.0f, 16.0f, 16.0f, al_map_rgba(8, 12, 16, 210));
    al_draw_text(titleFont, al_map_rgb(255, 244, 214), 36.0f, 28.0f, 0, "邊境據點");
    al_draw_textf(bodyFont, al_map_rgb(201, 226, 255), 38.0f, 78.0f, 0, "第 %d 天  %s", game.day, isNight(game) ? "夜襲中" : "白天探索");
    al_draw_textf(bodyFont, al_map_rgb(201, 226, 255), 38.0f, 110.0f, 0, "等級 %d  經驗 %d/%d", game.player.level, game.player.xp, game.player.xpToNext);
    al_draw_textf(bodyFont, al_map_rgb(201, 226, 255), 38.0f, 142.0f, 0, "怪物 %d  危險值 %.1f", static_cast<int>(game.monsters.size()), game.danger);
    al_draw_textf(bodyFont, al_map_rgb(255, 108, 126), 38.0f, 174.0f, 0, "永久紅寶石 %d", game.permanentRubies);

    drawProgressBar(552.0f, 24.0f, 320.0f, 22.0f, static_cast<float>(game.player.hp) / game.player.maxHp, al_map_rgba(20, 20, 20, 190), al_map_rgb(222, 88, 88));
    al_draw_textf(bodyFont, al_map_rgb(255, 255, 255), 560.0f, 48.0f, 0, "生命 %d / %d", game.player.hp, game.player.maxHp);
    drawProgressBar(552.0f, 86.0f, 320.0f, 22.0f, clampf(static_cast<float>(game.player.cores) / 100.0f, 0.0f, 1.0f), al_map_rgba(20, 20, 20, 190), al_map_rgb(255, 208, 98));
    al_draw_textf(bodyFont, al_map_rgb(255, 255, 255), 560.0f, 110.0f, 0, "核心穩定度 %d / 100", std::max(0, game.player.cores));
    drawProgressBar(552.0f, 148.0f, 320.0f, 18.0f, 1.0f - clampf(game.player.potionCooldown / 1.2f, 0.0f, 1.0f), al_map_rgba(20, 20, 20, 190), al_map_rgb(88, 232, 124));
    al_draw_textf(bodyFont, al_map_rgb(230, 255, 236), 560.0f, 168.0f, 0, "毒藥水 %s", game.player.potionCooldown > 0.0f ? "冷卻中" : "可使用");
    drawProgressBar(552.0f, 194.0f, 320.0f, 18.0f, clampf(game.player.hunger / 100.0f, 0.0f, 1.0f), al_map_rgba(20, 20, 20, 190), al_map_rgb(255, 164, 72));
    al_draw_textf(bodyFont, game.player.hunger > 25.0f ? al_map_rgb(255, 244, 226) : al_map_rgb(255, 146, 146), 560.0f, 214.0f, 0, "飽食 %.0f / 100", game.player.hunger);

    al_draw_filled_rounded_rectangle(18.0f, SCREEN_H - 176.0f, 900.0f, SCREEN_H - 18.0f, 16.0f, 16.0f, al_map_rgba(8, 12, 16, 210));
    al_draw_textf(bodyFont, al_map_rgb(255, 238, 205), 36.0f, SCREEN_H - 160.0f, 0, "木材 %d   石材 %d   纖維 %d   水晶 %d   菜 %d   肉 %d   魚 %d   炸彈 %d", game.bag.wood, game.bag.stone, game.bag.fiber, game.bag.crystal, game.bag.vegetable, game.bag.meat, game.bag.fish, game.bag.bomb);
    al_draw_textf(bodyFont, al_map_rgb(214, 230, 240), 36.0f, SCREEN_H - 124.0f, 0, "職業：%s   目前建築：%s   模式：%s   裝備：%s%s%s", jobName(game.player.jobClass), buildName(game.selectedBuild), game.buildMode ? "建造" : "戰鬥", game.hasSword ? "劍 " : "", game.hasArmor ? "盔甲 " : "", game.hasPoisonUpgrade ? "毒藥強化" : "");
    al_draw_text(bodyFont, al_map_rgb(170, 202, 221), 36.0f, SCREEN_H - 92.0f, 0, "WASD 移動   E 採集 / 神社領紅寶石   Space 近戰   滑鼠左鍵 射擊 / 放置   G 毒藥水   H 釣魚   T 丟炸彈");
    al_draw_text(bodyFont, al_map_rgb(170, 202, 221), 36.0f, SCREEN_H - 60.0f, 0, "Q 建造模式   1 圍牆   2 砲塔   3 營地   4 黑暗小法師   R 吃食物   J 職業技能   靠近街店可交易");

    al_draw_filled_rounded_rectangle(860.0f, 18.0f, SCREEN_W - 18.0f, 320.0f, 16.0f, 16.0f, al_map_rgba(8, 12, 16, 210));
    al_draw_textf(titleFont, al_map_rgb(255, 235, 183), 884.0f, 30.0f, 0, "目標 - 第 %d 關", game.currentStage);
    std::string objectiveText;
    if (game.currentStage == 1) {
        objectiveText =
            "第一關：建立前線基地\n"
            "累計採集木材 50 / " + std::to_string(game.totalWoodGathered) + "\n"
            "累計採集石材 36 / " + std::to_string(game.totalStoneGathered) + "\n"
            "存活圍牆 6 / " + std::to_string(countBuildingsOfType(game, BuildingType::Wall)) + "\n"
            "神社黑暗大法師 4 / " + std::to_string(std::min(game.totalDarkMageKills, 4));
    } else if (game.currentStage == 2) {
        objectiveText =
            "第二關：鞏固防線並清怪\n"
            "累計擊殺怪物 50 / " + std::to_string(game.totalMonsterKills) + "\n"
            "存活砲塔 9 / " + std::to_string(countBuildingsOfType(game, BuildingType::Turret)) + "\n"
            "存活營地 3 / " + std::to_string(countBuildingsOfType(game, BuildingType::Camp)) + "\n"
            "黑暗小法師 2 / " + std::to_string(countBuildingsOfType(game, BuildingType::DarkMage));
    } else if (game.currentStage == 3) {
        objectiveText =
            "第三關：救援行動\n"
            "前往地圖四處救出小孩。\n"
            "已救援小孩 " + std::to_string(game.rescuedChildren) + " / 4\n"
            "全部救出後開啟第四關。";
    } else if (game.currentStage == 4) {
        objectiveText =
            "第四關：損壞村莊\n"
            "討伐山匪 18 / " + std::to_string(game.stage4BanditKills) + "\n"
            "黑暗大法師 3 / " + std::to_string(game.stage4DarkMageKills) + "\n"
            "藍基地：" + std::string(game.villageBaseDestroyed ? "已摧毀" : "尚未摧毀") + "\n"
            "綠寶石：" + std::string(game.emeraldDelivered ? "已送回核心" : (game.carryingEmerald ? "攜帶中" : (game.emeraldDropped ? "已掉落" : "未取得")));
    } else {
        objectiveText =
            "第五關：最終黑色禁地\n"
            "前往右上方黑色地點，召喚大魔王。\n"
            "大魔王狀態：" + std::string(game.bossDefeated ? "已擊敗" : (game.bossSpawned ? "戰鬥中" : "尚未出現")) + "\n"
            "完成最終討伐即可通關。";
    }
    al_draw_multiline_text(bodyFont, al_map_rgb(230, 236, 240), 884.0f, 82.0f, SCREEN_W - 926.0f, 38.0f, 0, objectiveText.c_str());

    if (boss) {
        al_draw_filled_rounded_rectangle(300.0f, 18.0f, 980.0f, 106.0f, 18.0f, 18.0f, al_map_rgba(18, 7, 10, 222));
        al_draw_text(titleFont, al_map_rgb(255, 210, 210), 330.0f, 28.0f, 0, boss->enraged ? "最終大魔王 第二階段" : "最終大魔王");
        drawProgressBar(330.0f, 72.0f, 620.0f, 20.0f, static_cast<float>(boss->hp) / boss->maxHp, al_map_rgba(24, 24, 24, 220), boss->enraged ? al_map_rgb(255, 72, 72) : al_map_rgb(190, 64, 64));
        al_draw_textf(bodyFont, al_map_rgb(255, 242, 240), 960.0f, 64.0f, ALLEGRO_ALIGN_RIGHT, "%d / %d", std::max(0, boss->hp), boss->maxHp);
    }

    drawMinimap(game, SCREEN_W - 280.0f, SCREEN_H - 220.0f, 250.0f, 190.0f);
    if (game.bossIntroTimer > 0.0f) {
        float flash = 0.45f + 0.35f * std::sin((3.6f - game.bossIntroTimer) * 18.0f);
        al_draw_filled_rounded_rectangle(290.0f, 338.0f, SCREEN_W - 290.0f, 418.0f, 18.0f, 18.0f, al_map_rgba_f(0.42f, 0.03f, 0.03f, flash));
        al_draw_text(titleFont, al_map_rgb(255, 232, 220), SCREEN_W * 0.5f, 352.0f, ALLEGRO_ALIGN_CENTER, "警告");
        al_draw_text(bodyFont, al_map_rgb(255, 248, 240), SCREEN_W * 0.5f, 390.0f, ALLEGRO_ALIGN_CENTER, "黑色地點出現異常波動，大魔王已降臨");
    }
    if (game.messageTimer > 0.0f) {
        al_draw_filled_rounded_rectangle(340.0f, SCREEN_H - 246.0f, SCREEN_W - 340.0f, SCREEN_H - 188.0f, 16.0f, 16.0f, al_map_rgba(18, 22, 30, 210));
        al_draw_text(bodyFont, al_map_rgb(255, 245, 214), SCREEN_W * 0.5f, SCREEN_H - 232.0f, ALLEGRO_ALIGN_CENTER, game.message.c_str());
    }
    if (game.shopOpen) {
        al_draw_filled_rounded_rectangle(520.0f, 268.0f, 1080.0f, 532.0f, 20.0f, 20.0f, al_map_rgba(14, 12, 10, 236));
        if (game.activeShop == ShopType::Bomb) {
            al_draw_text(titleFont, al_map_rgb(255, 232, 188), SCREEN_W * 0.5f, 292.0f, ALLEGRO_ALIGN_CENTER, "炸彈店");
            al_draw_text(bodyFont, al_map_rgb(236, 242, 248), SCREEN_W * 0.5f, 356.0f, ALLEGRO_ALIGN_CENTER, "這裡販售爆破補給");
            al_draw_text(bodyFont, al_map_rgb(255, 224, 158), SCREEN_W * 0.5f, 404.0f, ALLEGRO_ALIGN_CENTER, "1：20 水晶購買 10 個炸彈");
            al_draw_textf(bodyFont, al_map_rgb(222, 234, 240), SCREEN_W * 0.5f, 446.0f, ALLEGRO_ALIGN_CENTER, "目前水晶：%d   目前炸彈：%d", game.bag.crystal, game.bag.bomb);
            al_draw_text(bodyFont, al_map_rgb(190, 210, 224), SCREEN_W * 0.5f, 488.0f, ALLEGRO_ALIGN_CENTER, "購買後按 T 鍵就能丟出炸彈");
        } else if (game.activeShop == ShopType::Grocery) {
            al_draw_text(titleFont, al_map_rgb(255, 242, 188), SCREEN_W * 0.5f, 292.0f, ALLEGRO_ALIGN_CENTER, "超市");
            al_draw_text(bodyFont, al_map_rgb(236, 242, 248), SCREEN_W * 0.5f, 356.0f, ALLEGRO_ALIGN_CENTER, "補充生存食材");
            al_draw_text(bodyFont, al_map_rgb(196, 255, 176), SCREEN_W * 0.5f, 404.0f, ALLEGRO_ALIGN_CENTER, "1：20 木材換 4 個肉");
            al_draw_textf(bodyFont, al_map_rgb(222, 234, 240), SCREEN_W * 0.5f, 446.0f, ALLEGRO_ALIGN_CENTER, "目前木材：%d   目前肉：%d", game.bag.wood, game.bag.meat);
            al_draw_text(bodyFont, al_map_rgb(190, 210, 224), SCREEN_W * 0.5f, 488.0f, ALLEGRO_ALIGN_CENTER, "狩獵壓力大時可以來這裡補肉");
        } else if (game.activeShop == ShopType::Tool) {
            al_draw_text(titleFont, al_map_rgb(208, 226, 255), SCREEN_W * 0.5f, 292.0f, ALLEGRO_ALIGN_CENTER, "工具行");
            al_draw_text(bodyFont, al_map_rgb(236, 242, 248), SCREEN_W * 0.5f, 344.0f, ALLEGRO_ALIGN_CENTER, "武器、防具、毒藥強化");
            al_draw_text(bodyFont, al_map_rgb(255, 228, 164), SCREEN_W * 0.5f, 388.0f, ALLEGRO_ALIGN_CENTER, game.hasSword ? "1：已擁有 劍" : "1：40 纖維換 劍");
            al_draw_text(bodyFont, al_map_rgb(202, 230, 255), SCREEN_W * 0.5f, 426.0f, ALLEGRO_ALIGN_CENTER, game.hasArmor ? "2：已擁有 盔甲" : "2：30 水晶換 盔甲");
            al_draw_text(bodyFont, al_map_rgb(174, 255, 186), SCREEN_W * 0.5f, 464.0f, ALLEGRO_ALIGN_CENTER, game.hasPoisonUpgrade ? "3：已擁有 毒藥強化" : "3：35 水晶換 藥水強化");
            al_draw_text(bodyFont, al_map_rgb(190, 210, 224), SCREEN_W * 0.5f, 506.0f, ALLEGRO_ALIGN_CENTER, "劍提高近戰傷害，盔甲提高防禦，藥水提高毒傷");
        }
    }
}

void drawTitle(const GameState& game, ALLEGRO_FONT* bodyFont, ALLEGRO_FONT* titleFont) {
    al_clear_to_color(al_map_rgb(15, 18, 26));
    al_draw_filled_rectangle(0.0f, 0.0f, SCREEN_W, SCREEN_H, al_map_rgba(8, 16, 26, 255));
    al_draw_filled_circle(280.0f, 180.0f, 180.0f, al_map_rgba(52, 98, 140, 90));
    al_draw_filled_circle(SCREEN_W - 240.0f, SCREEN_H - 150.0f, 220.0f, al_map_rgba(197, 127, 74, 70));
    al_draw_text(titleFont, al_map_rgb(255, 245, 214), SCREEN_W * 0.5f, 130.0f, ALLEGRO_ALIGN_CENTER, "邊境據點");
    al_draw_text(bodyFont, al_map_rgb(212, 227, 241), SCREEN_W * 0.5f, 205.0f, ALLEGRO_ALIGN_CENTER, "五大關卡：採集建設、防線清怪、救援小孩、損壞村莊、最終魔王");
    al_draw_filled_rounded_rectangle(470.0f, 290.0f, 1130.0f, 620.0f, 24.0f, 24.0f, al_map_rgba(10, 14, 18, 215));
    al_draw_text(titleFont, al_map_rgb(255, 233, 184), SCREEN_W * 0.5f, 332.0f, ALLEGRO_ALIGN_CENTER, "開始選單");
    al_draw_text(bodyFont, al_map_rgb(235, 240, 246), SCREEN_W * 0.5f, 410.0f, ALLEGRO_ALIGN_CENTER, "Enter：開始新遊戲");
    auto jobColor = [&](JobClass job, ALLEGRO_COLOR on, ALLEGRO_COLOR off) { return game.selectedJob == job ? on : off; };
    al_draw_textf(bodyFont, jobColor(JobClass::Drifter, al_map_rgb(255, 236, 186), al_map_rgb(210, 220, 228)), SCREEN_W * 0.5f, 438.0f, ALLEGRO_ALIGN_CENTER, "1：無業遊民  沒有變化");
    al_draw_textf(bodyFont, jobColor(JobClass::Vampire, al_map_rgb(255, 182, 182), al_map_rgb(210, 220, 228)), SCREEN_W * 0.5f, 475.0f, ALLEGRO_ALIGN_CENTER, "2：吸血鬼  每 3 次命中吸血，J 回核心");
    al_draw_textf(bodyFont, jobColor(JobClass::Lumberjack, al_map_rgb(220, 255, 190), al_map_rgb(210, 220, 228)), SCREEN_W * 0.5f, 512.0f, ALLEGRO_ALIGN_CENTER, "3：伐木工  樹石 +20，雜草水晶 +6");
    al_draw_textf(bodyFont, jobColor(JobClass::Fisher, al_map_rgb(182, 230, 255), al_map_rgb(210, 220, 228)), SCREEN_W * 0.5f, 549.0f, ALLEGRO_ALIGN_CENTER, "4：漁夫  K 放船，船會自動開火");
    al_draw_textf(bodyFont, jobColor(JobClass::Alien, al_map_rgb(210, 255, 255), al_map_rgb(210, 220, 228)), SCREEN_W * 0.5f, 586.0f, ALLEGRO_ALIGN_CENTER, "5：外星人  J 八連砲，K 每 30 秒召喚小兵");
    al_draw_textf(bodyFont, jobColor(JobClass::Elite, al_map_rgb(255, 216, 120), al_map_rgb(210, 220, 228)), SCREEN_W * 0.5f, 623.0f, ALLEGRO_ALIGN_CENTER, "6：菁英  減少受到的傷害，毒藥傷害與持續時間更高");
    al_draw_textf(bodyFont, game.saveExists ? al_map_rgb(235, 240, 246) : al_map_rgb(140, 150, 160), SCREEN_W * 0.5f, 664.0f, ALLEGRO_ALIGN_CENTER, "L：載入遊戲%s", game.saveExists ? "" : "（目前沒有存檔）");
    al_draw_text(bodyFont, al_map_rgb(235, 240, 246), SCREEN_W * 0.5f, 702.0f, ALLEGRO_ALIGN_CENTER, "Enter：以目前職業開始   L：載入遊戲   ESC：離開");
    al_draw_textf(bodyFont, al_map_rgb(180, 204, 222), SCREEN_W * 0.5f, 738.0f, ALLEGRO_ALIGN_CENTER, "目前選擇：%s  %s", jobName(game.selectedJob), jobDescription(game.selectedJob));
    al_draw_textf(bodyFont, al_map_rgb(255, 112, 132), SCREEN_W * 0.5f, 774.0f, ALLEGRO_ALIGN_CENTER, "帳號永久紅寶石：%d", game.permanentRubies);
}

void drawPauseMenu(ALLEGRO_FONT* bodyFont, ALLEGRO_FONT* titleFont) {
    al_draw_filled_rectangle(0.0f, 0.0f, SCREEN_W, SCREEN_H, al_map_rgba(0, 0, 0, 150));
    al_draw_filled_rounded_rectangle(540.0f, 250.0f, 1060.0f, 560.0f, 20.0f, 20.0f, al_map_rgba(14, 16, 22, 238));
    al_draw_text(titleFont, al_map_rgb(255, 236, 198), SCREEN_W * 0.5f, 282.0f, ALLEGRO_ALIGN_CENTER, "暫停選單");
    al_draw_text(bodyFont, al_map_rgb(232, 238, 244), SCREEN_W * 0.5f, 360.0f, ALLEGRO_ALIGN_CENTER, "Enter：繼續遊戲");
    al_draw_text(bodyFont, al_map_rgb(232, 238, 244), SCREEN_W * 0.5f, 404.0f, ALLEGRO_ALIGN_CENTER, "S：開啟存檔清單");
    al_draw_text(bodyFont, al_map_rgb(232, 238, 244), SCREEN_W * 0.5f, 448.0f, ALLEGRO_ALIGN_CENTER, "L：開啟讀檔清單");
    al_draw_text(bodyFont, al_map_rgb(232, 238, 244), SCREEN_W * 0.5f, 492.0f, ALLEGRO_ALIGN_CENTER, "ESC：關閉選單");
}

void drawSlotMenu(const GameState& game, ALLEGRO_FONT* bodyFont, ALLEGRO_FONT* titleFont) {
    al_draw_filled_rectangle(0.0f, 0.0f, SCREEN_W, SCREEN_H, al_map_rgba(0, 0, 0, 170));
    al_draw_filled_rounded_rectangle(120.0f, 80.0f, SCREEN_W - 120.0f, SCREEN_H - 80.0f, 22.0f, 22.0f, al_map_rgba(10, 14, 20, 240));
    al_draw_text(titleFont, al_map_rgb(255, 234, 194), SCREEN_W * 0.5f, 96.0f, ALLEGRO_ALIGN_CENTER, game.slotMenuMode == SlotMenuMode::Save ? "存檔清單 100 格" : "載入遊戲 100 格");
    al_draw_textf(bodyFont, al_map_rgb(204, 220, 236), SCREEN_W * 0.5f, 142.0f, ALLEGRO_ALIGN_CENTER, "方向鍵移動  Enter%s  ESC返回", game.slotMenuMode == SlotMenuMode::Save ? "存檔" : "讀檔");

    const float startX = 150.0f;
    const float startY = 190.0f;
    const float cellW = 128.0f;
    const float cellH = 56.0f;
    for (int slot = 1; slot <= 100; ++slot) {
        int idx = slot - 1;
        int col = idx % 10;
        int row = idx / 10;
        float x = startX + col * cellW;
        float y = startY + row * cellH;
        bool selected = game.selectedSaveSlot == slot;
        SavePreview preview = readSavePreview(slot);
        ALLEGRO_COLOR fill = selected ? al_map_rgba(70, 106, 156, 220) : al_map_rgba(24, 28, 36, 220);
        al_draw_filled_rounded_rectangle(x, y, x + 112.0f, y + 42.0f, 10.0f, 10.0f, fill);
        al_draw_rounded_rectangle(x, y, x + 112.0f, y + 42.0f, 10.0f, 10.0f, preview.exists ? al_map_rgb(184, 214, 255) : al_map_rgb(96, 106, 118), 2.0f);
        if (preview.exists) {
            al_draw_textf(bodyFont, al_map_rgb(255, 248, 226), x + 10.0f, y + 4.0f, 0, "%03d D%d", slot, preview.day);
            al_draw_textf(bodyFont, al_map_rgb(208, 226, 240), x + 10.0f, y + 22.0f, 0, "關%d %s", preview.stage, jobName(preview.job));
        } else {
            al_draw_textf(bodyFont, al_map_rgb(220, 224, 232), x + 10.0f, y + 10.0f, 0, "%03d 空格", slot);
        }
    }
}

void drawGameOver(const GameState& game, ALLEGRO_FONT* bodyFont, ALLEGRO_FONT* titleFont) {
    al_draw_filled_rectangle(0.0f, 0.0f, SCREEN_W, SCREEN_H, al_map_rgba(0, 0, 0, 190));
    al_draw_text(titleFont, al_map_rgb(255, 220, 210), SCREEN_W * 0.5f, SCREEN_H * 0.5f - 100.0f, ALLEGRO_ALIGN_CENTER, "據點淪陷");
    al_draw_textf(bodyFont, al_map_rgb(255, 255, 255), SCREEN_W * 0.5f, SCREEN_H * 0.5f - 40.0f, ALLEGRO_ALIGN_CENTER, "你撐到了第 %d 天，最終危險值 %.1f。", game.day, game.danger);
    al_draw_text(bodyFont, al_map_rgb(255, 255, 255), SCREEN_W * 0.5f, SCREEN_H * 0.5f + 8.0f, ALLEGRO_ALIGN_CENTER, "Enter 回到標題畫面   C 繼續存檔   ESC 離開");
}

void drawVictory(const GameState& game, ALLEGRO_FONT* bodyFont, ALLEGRO_FONT* titleFont) {
    al_draw_filled_rectangle(0.0f, 0.0f, SCREEN_W, SCREEN_H, al_map_rgba(2, 6, 10, 198));
    float pulse = 0.55f + 0.45f * std::sin(game.dayTimer * 2.8f);
    al_draw_filled_circle(250.0f, 160.0f, 150.0f + pulse * 20.0f, al_map_rgba_f(0.95f, 0.78f, 0.32f, 0.12f + pulse * 0.08f));
    al_draw_filled_circle(SCREEN_W - 230.0f, 190.0f, 180.0f + pulse * 24.0f, al_map_rgba_f(0.92f, 0.24f, 0.18f, 0.10f + pulse * 0.06f));
    for (int i = 0; i < 7; ++i) {
        float x = 190.0f + i * 165.0f;
        float y = 680.0f + std::sin(game.dayTimer * 2.0f + i * 0.7f) * 18.0f;
        al_draw_filled_circle(x, y, 14.0f + i % 3, al_map_rgba(255, 218, 120, 170));
    }
    al_draw_text(titleFont, al_map_rgb(255, 239, 192), SCREEN_W * 0.5f, SCREEN_H * 0.5f - 120.0f, ALLEGRO_ALIGN_CENTER, "黑色禁地已被征服");
    al_draw_textf(bodyFont, al_map_rgb(232, 240, 245), SCREEN_W * 0.5f, SCREEN_H * 0.5f - 50.0f, ALLEGRO_ALIGN_CENTER, "你在第 %d 天完成五大關卡，成功擊敗最終大魔王。", game.day);
    al_draw_text(bodyFont, al_map_rgb(210, 226, 235), SCREEN_W * 0.5f, SCREEN_H * 0.5f, ALLEGRO_ALIGN_CENTER, "第一關採集建設、第二關防線清怪、第三關救援小孩、第四關損壞村莊、第五關擊敗大魔王。");
    al_draw_text(bodyFont, al_map_rgb(255, 255, 255), SCREEN_W * 0.5f, SCREEN_H * 0.5f + 60.0f, ALLEGRO_ALIGN_CENTER, "Enter 回到標題畫面   C 讀取通關前存檔   ESC 離開");
}

ALLEGRO_FONT* loadFontWithFallback(int size) {
    const std::array<const char*, 4> fontPaths = {
        "C:/Windows/Fonts/msjh.ttc",
        "C:/Windows/Fonts/malgun.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf"
    };
    for (const char* path : fontPaths) {
        ALLEGRO_FONT* font = al_load_ttf_font(path, size, 0);
        if (font) return font;
    }
    return al_create_builtin_font();
}

}  // namespace

int main() {
    if (!al_init()) return 1;
    al_install_keyboard();
    al_install_mouse();
    al_init_font_addon();
    al_init_ttf_addon();
    al_init_primitives_addon();

    ALLEGRO_DISPLAY* display = al_create_display(SCREEN_W, SCREEN_H);
    if (!display) return 1;
    al_set_window_title(display, "邊境據點 - 開放世界生存建造");

    ALLEGRO_TIMER* timer = al_create_timer(1.0 / FPS);
    ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
    ALLEGRO_FONT* bodyFont = loadFontWithFallback(28);
    ALLEGRO_FONT* titleFont = loadFontWithFallback(42);
    auto createBackgroundMusic = []() -> Bgm {
        Bgm bgm {};
        if (!al_install_audio()) return bgm;
        al_init_acodec_addon();
        if (!al_reserve_samples(8)) return bgm;

        constexpr int sampleRate = 44100;
        constexpr float beatSeconds = 0.34f;
        constexpr int notesPerLoop = 16;
        constexpr std::array<float, notesPerLoop> melody = {
            261.63f, 329.63f, 392.00f, 329.63f,
            293.66f, 349.23f, 440.00f, 349.23f,
            261.63f, 329.63f, 493.88f, 392.00f,
            220.00f, 293.66f, 392.00f, 329.63f
        };
        constexpr std::array<float, 4> bass = { 130.81f, 146.83f, 174.61f, 110.00f };

        int framesPerBeat = static_cast<int>(sampleRate * beatSeconds);
        int totalFrames = framesPerBeat * notesPerLoop;
        auto* pcm = new int16_t[totalFrames * 2];

        for (int i = 0; i < totalFrames; ++i) {
            int beat = i / framesPerBeat;
            float t = static_cast<float>(i) / sampleRate;
            float beatProgress = static_cast<float>(i % framesPerBeat) / framesPerBeat;
            float env = 1.0f;
            if (beatProgress < 0.08f) env = beatProgress / 0.08f;
            else if (beatProgress > 0.78f) env = 1.0f - (beatProgress - 0.78f) / 0.22f;
            env = clampf(env, 0.0f, 1.0f);

            float lead = std::sin(2.0f * ALLEGRO_PI * melody[beat] * t);
            float pad = 0.65f * std::sin(2.0f * ALLEGRO_PI * melody[beat] * 0.5f * t);
            float low = 0.85f * std::sin(2.0f * ALLEGRO_PI * bass[(beat / 4) % bass.size()] * t);
            float sparkle = 0.25f * std::sin(2.0f * ALLEGRO_PI * melody[beat] * 2.0f * t);
            float sampleValue = (lead * 0.34f + pad * 0.18f + low * 0.22f + sparkle * 0.08f) * env * 0.6f;
            int16_t s = static_cast<int16_t>(clampf(sampleValue, -1.0f, 1.0f) * 32767.0f);
            pcm[i * 2] = s;
            pcm[i * 2 + 1] = s;
        }

        bgm.sample = al_create_sample(pcm, totalFrames, sampleRate, ALLEGRO_AUDIO_DEPTH_INT16, ALLEGRO_CHANNEL_CONF_2, true);
        if (!bgm.sample) return bgm;
        bgm.instance = al_create_sample_instance(bgm.sample);
        if (!bgm.instance) {
            al_destroy_sample(bgm.sample);
            bgm.sample = nullptr;
            return bgm;
        }
        al_set_sample_instance_playmode(bgm.instance, ALLEGRO_PLAYMODE_LOOP);
        al_set_sample_instance_gain(bgm.instance, 0.42f);
        if (!al_attach_sample_instance_to_mixer(bgm.instance, al_get_default_mixer())) {
            al_destroy_sample_instance(bgm.instance);
            al_destroy_sample(bgm.sample);
            bgm.instance = nullptr;
            bgm.sample = nullptr;
            return bgm;
        }
        al_play_sample_instance(bgm.instance);
        bgm.ready = true;
        return bgm;
    };
    auto createGunshotSample = []() -> ALLEGRO_SAMPLE* {
        constexpr int sampleRate = 44100;
        constexpr float duration = 0.10f;
        int totalFrames = static_cast<int>(sampleRate * duration);
        auto* pcm = new int16_t[totalFrames * 2];

        for (int i = 0; i < totalFrames; ++i) {
            float t = static_cast<float>(i) / sampleRate;
            float env = 1.0f - (t / duration);
            env = clampf(env, 0.0f, 1.0f);
            float crack = std::sin(2.0f * ALLEGRO_PI * 920.0f * t) * 0.55f;
            float pop = std::sin(2.0f * ALLEGRO_PI * 180.0f * t) * 0.25f;
            float hiss = (static_cast<float>((i * 97) % 31) / 30.0f) * 2.0f - 1.0f;
            float sampleValue = (crack + pop + hiss * 0.28f) * env * 0.8f;
            int16_t s = static_cast<int16_t>(clampf(sampleValue, -1.0f, 1.0f) * 32767.0f);
            pcm[i * 2] = s;
            pcm[i * 2 + 1] = s;
        }

        return al_create_sample(pcm, totalFrames, sampleRate, ALLEGRO_AUDIO_DEPTH_INT16, ALLEGRO_CHANNEL_CONF_2, true);
    };
    auto createGatherSample = []() -> ALLEGRO_SAMPLE* {
        constexpr int sampleRate = 44100;
        constexpr float duration = 0.08f;
        int totalFrames = static_cast<int>(sampleRate * duration);
        auto* pcm = new int16_t[totalFrames * 2];

        for (int i = 0; i < totalFrames; ++i) {
            float t = static_cast<float>(i) / sampleRate;
            float env = 1.0f - (t / duration);
            env = clampf(env, 0.0f, 1.0f);
            float chime = std::sin(2.0f * ALLEGRO_PI * 760.0f * t) * 0.38f;
            float tick = std::sin(2.0f * ALLEGRO_PI * 1280.0f * t) * 0.18f;
            float thud = std::sin(2.0f * ALLEGRO_PI * 220.0f * t) * 0.18f;
            float sampleValue = (chime + tick + thud) * env * 0.85f;
            int16_t s = static_cast<int16_t>(clampf(sampleValue, -1.0f, 1.0f) * 32767.0f);
            pcm[i * 2] = s;
            pcm[i * 2 + 1] = s;
        }

        return al_create_sample(pcm, totalFrames, sampleRate, ALLEGRO_AUDIO_DEPTH_INT16, ALLEGRO_CHANNEL_CONF_2, true);
    };
    auto createMonsterDeathSample = []() -> ALLEGRO_SAMPLE* {
        constexpr int sampleRate = 44100;
        constexpr float duration = 0.18f;
        int totalFrames = static_cast<int>(sampleRate * duration);
        auto* pcm = new int16_t[totalFrames * 2];

        for (int i = 0; i < totalFrames; ++i) {
            float t = static_cast<float>(i) / sampleRate;
            float env = 1.0f - (t / duration);
            env = clampf(env, 0.0f, 1.0f);
            float growl = std::sin(2.0f * ALLEGRO_PI * (240.0f - t * 520.0f) * t) * 0.42f;
            float crunch = std::sin(2.0f * ALLEGRO_PI * 120.0f * t) * 0.18f;
            float noise = (static_cast<float>((i * 53) % 41) / 40.0f) * 2.0f - 1.0f;
            float sampleValue = (growl + crunch + noise * 0.22f) * env * 0.9f;
            int16_t s = static_cast<int16_t>(clampf(sampleValue, -1.0f, 1.0f) * 32767.0f);
            pcm[i * 2] = s;
            pcm[i * 2 + 1] = s;
        }

        return al_create_sample(pcm, totalFrames, sampleRate, ALLEGRO_AUDIO_DEPTH_INT16, ALLEGRO_CHANNEL_CONF_2, true);
    };
    auto createBossIntroSample = []() -> ALLEGRO_SAMPLE* {
        constexpr int sampleRate = 44100;
        constexpr float duration = 1.35f;
        int totalFrames = static_cast<int>(sampleRate * duration);
        auto* pcm = new int16_t[totalFrames * 2];

        for (int i = 0; i < totalFrames; ++i) {
            float t = static_cast<float>(i) / sampleRate;
            float env = 1.0f;
            if (t < 0.08f) env = t / 0.08f;
            else if (t > duration - 0.28f) env = (duration - t) / 0.28f;
            env = clampf(env, 0.0f, 1.0f);
            float siren = std::sin(2.0f * ALLEGRO_PI * (90.0f + std::sin(t * 7.0f) * 22.0f) * t) * 0.42f;
            float boom = std::sin(2.0f * ALLEGRO_PI * 48.0f * t) * 0.34f;
            float hiss = (((i * 67) % 97) / 96.0f) * 2.0f - 1.0f;
            float pulse = std::sin(2.0f * ALLEGRO_PI * 4.0f * t) > 0.0f ? 1.0f : 0.45f;
            float sampleValue = (siren + boom + hiss * 0.08f) * env * pulse * 0.9f;
            int16_t s = static_cast<int16_t>(clampf(sampleValue, -1.0f, 1.0f) * 32767.0f);
            pcm[i * 2] = s;
            pcm[i * 2 + 1] = s;
        }

        return al_create_sample(pcm, totalFrames, sampleRate, ALLEGRO_AUDIO_DEPTH_INT16, ALLEGRO_CHANNEL_CONF_2, true);
    };
    auto createPoisonPotionSample = []() -> ALLEGRO_SAMPLE* {
        constexpr int sampleRate = 44100;
        constexpr float duration = 0.24f;
        int totalFrames = static_cast<int>(sampleRate * duration);
        auto* pcm = new int16_t[totalFrames * 2];

        for (int i = 0; i < totalFrames; ++i) {
            float t = static_cast<float>(i) / sampleRate;
            float env = 1.0f - (t / duration);
            env = clampf(env, 0.0f, 1.0f);
            float bubble = std::sin(2.0f * ALLEGRO_PI * (420.0f - t * 520.0f) * t) * 0.34f;
            float splash = std::sin(2.0f * ALLEGRO_PI * 170.0f * t) * 0.26f;
            float fizz = (((i * 29) % 57) / 56.0f) * 2.0f - 1.0f;
            float sampleValue = (bubble + splash + fizz * 0.16f) * env * 0.92f;
            int16_t s = static_cast<int16_t>(clampf(sampleValue, -1.0f, 1.0f) * 32767.0f);
            pcm[i * 2] = s;
            pcm[i * 2 + 1] = s;
        }

        return al_create_sample(pcm, totalFrames, sampleRate, ALLEGRO_AUDIO_DEPTH_INT16, ALLEGRO_CHANNEL_CONF_2, true);
    };
    auto createRescueChildSample = []() -> ALLEGRO_SAMPLE* {
        constexpr int sampleRate = 44100;
        constexpr float duration = 0.28f;
        int totalFrames = static_cast<int>(sampleRate * duration);
        auto* pcm = new int16_t[totalFrames * 2];

        for (int i = 0; i < totalFrames; ++i) {
            float t = static_cast<float>(i) / sampleRate;
            float env = 1.0f - (t / duration);
            env = clampf(env, 0.0f, 1.0f);
            float noteA = std::sin(2.0f * ALLEGRO_PI * 660.0f * t) * 0.24f;
            float noteB = std::sin(2.0f * ALLEGRO_PI * 880.0f * t) * 0.30f;
            float noteC = std::sin(2.0f * ALLEGRO_PI * 990.0f * t) * 0.22f;
            float sampleValue = (noteA + noteB + noteC) * env * 0.95f;
            int16_t s = static_cast<int16_t>(clampf(sampleValue, -1.0f, 1.0f) * 32767.0f);
            pcm[i * 2] = s;
            pcm[i * 2 + 1] = s;
        }

        return al_create_sample(pcm, totalFrames, sampleRate, ALLEGRO_AUDIO_DEPTH_INT16, ALLEGRO_CHANNEL_CONF_2, true);
    };
    auto createBombThrowSample = []() -> ALLEGRO_SAMPLE* {
        constexpr int sampleRate = 44100;
        constexpr float duration = 0.24f;
        int totalFrames = static_cast<int>(sampleRate * duration);
        auto* pcm = new int16_t[totalFrames * 2];

        for (int i = 0; i < totalFrames; ++i) {
            float t = static_cast<float>(i) / sampleRate;
            float env = 1.0f - (t / duration);
            env = clampf(env, 0.0f, 1.0f);
            float whoosh = std::sin(2.0f * ALLEGRO_PI * (520.0f - t * 900.0f) * t) * 0.34f;
            float boom = std::sin(2.0f * ALLEGRO_PI * 92.0f * t) * 0.28f;
            float grit = (((i * 41) % 59) / 58.0f) * 2.0f - 1.0f;
            float sampleValue = (whoosh + boom + grit * 0.12f) * env * 0.92f;
            int16_t s = static_cast<int16_t>(clampf(sampleValue, -1.0f, 1.0f) * 32767.0f);
            pcm[i * 2] = s;
            pcm[i * 2 + 1] = s;
        }

        return al_create_sample(pcm, totalFrames, sampleRate, ALLEGRO_AUDIO_DEPTH_INT16, ALLEGRO_CHANNEL_CONF_2, true);
    };
    auto destroyBackgroundMusic = [](Bgm& bgm) {
        if (bgm.instance) {
            al_stop_sample_instance(bgm.instance);
            al_destroy_sample_instance(bgm.instance);
            bgm.instance = nullptr;
        }
        if (bgm.sample) {
            al_destroy_sample(bgm.sample);
            bgm.sample = nullptr;
        }
        if (bgm.ready) {
            al_uninstall_audio();
            bgm.ready = false;
        }
    };
    Bgm bgm = createBackgroundMusic();
    gGunshotSample = createGunshotSample();
    gGatherSample = createGatherSample();
    gMonsterDeathSample = createMonsterDeathSample();
    gBossIntroSample = createBossIntroSample();
    gPoisonPotionSample = createPoisonPotionSample();
    gRescueChildSample = createRescueChildSample();
    gBombThrowSample = createBombThrowSample();
    if (!timer || !queue || !bodyFont || !titleFont) return 1;

    al_register_event_source(queue, al_get_display_event_source(display));
    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_mouse_event_source());
    al_register_event_source(queue, al_get_timer_event_source(timer));

    GameState game {};
    game.permanentRubies = loadPermanentRubies();
    game.saveExists = anySaveFilesExist();
    bool redraw = true;
    al_start_timer(timer);

    while (game.running) {
        ALLEGRO_EVENT event {};
        al_wait_for_event(queue, &event);

        if (event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            game.running = false;
        } else if (event.type == ALLEGRO_EVENT_MOUSE_AXES) {
            game.mouseScreenX = static_cast<float>(event.mouse.x);
            game.mouseScreenY = static_cast<float>(event.mouse.y);
        } else if (event.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN && game.scene == Scene::Playing) {
            if (event.mouse.button == 1) {
                if (game.buildMode) {
                    Vec2 camera = getCamera(game);
                    Vec2 mouseWorld = screenToWorld({ static_cast<float>(event.mouse.x), static_cast<float>(event.mouse.y) }, camera);
                    mouseWorld.x = std::round(mouseWorld.x / 16.0f) * 16.0f;
                    mouseWorld.y = std::round(mouseWorld.y / 16.0f) * 16.0f;
                    tryPlaceBuilding(game, mouseWorld);
                } else {
                    game.controls.shoot = true;
                }
            }
        } else if (event.type == ALLEGRO_EVENT_MOUSE_BUTTON_UP && game.scene == Scene::Playing) {
            if (event.mouse.button == 1) game.controls.shoot = false;
        } else if (event.type == ALLEGRO_EVENT_KEY_DOWN) {
            if (game.scene == Scene::Title) {
                if (game.slotMenuMode != SlotMenuMode::None) {
                    switch (event.keyboard.keycode) {
                        case ALLEGRO_KEY_UP: if (game.selectedSaveSlot > 10) game.selectedSaveSlot -= 10; break;
                        case ALLEGRO_KEY_DOWN: if (game.selectedSaveSlot <= 90) game.selectedSaveSlot += 10; break;
                        case ALLEGRO_KEY_LEFT: if ((game.selectedSaveSlot - 1) % 10 != 0) game.selectedSaveSlot -= 1; break;
                        case ALLEGRO_KEY_RIGHT: if (game.selectedSaveSlot % 10 != 0) game.selectedSaveSlot += 1; break;
                        case ALLEGRO_KEY_ENTER:
                            if (saveFileExists(game.selectedSaveSlot) && loadGame(game, game.selectedSaveSlot)) {
                                game.saveExists = anySaveFilesExist();
                                addMessage(game, "已載入選擇的存檔。", 4.0f);
                            } else {
                                addMessage(game, "這個存檔格目前沒有資料。", 3.0f);
                            }
                            break;
                        case ALLEGRO_KEY_ESCAPE: game.slotMenuMode = SlotMenuMode::None; break;
                        default: break;
                    }
                } else {
                switch (event.keyboard.keycode) {
                    case ALLEGRO_KEY_ENTER: startNewGame(game); break;
                    case ALLEGRO_KEY_1: game.selectedJob = JobClass::Drifter; break;
                    case ALLEGRO_KEY_2: game.selectedJob = JobClass::Vampire; break;
                    case ALLEGRO_KEY_3: game.selectedJob = JobClass::Lumberjack; break;
                    case ALLEGRO_KEY_4: game.selectedJob = JobClass::Fisher; break;
                    case ALLEGRO_KEY_5: game.selectedJob = JobClass::Alien; break;
                    case ALLEGRO_KEY_6: game.selectedJob = JobClass::Elite; break;
                    case ALLEGRO_KEY_L: if (game.saveExists) game.slotMenuMode = SlotMenuMode::Load; break;
                    case ALLEGRO_KEY_ESCAPE: game.running = false; break;
                    default: break;
                }
                }
            } else if (game.scene == Scene::GameOver) {
                switch (event.keyboard.keycode) {
                    case ALLEGRO_KEY_ENTER: game.scene = Scene::Title; game.saveExists = anySaveFilesExist(); clearControls(game); break;
                    case ALLEGRO_KEY_L: if (game.saveExists) game.slotMenuMode = SlotMenuMode::Load; break;
                    case ALLEGRO_KEY_ESCAPE: game.running = false; break;
                    default: break;
                }
            } else if (game.scene == Scene::Victory) {
                switch (event.keyboard.keycode) {
                    case ALLEGRO_KEY_ENTER: game.scene = Scene::Title; game.saveExists = anySaveFilesExist(); clearControls(game); break;
                    case ALLEGRO_KEY_L: if (game.saveExists) game.slotMenuMode = SlotMenuMode::Load; break;
                    case ALLEGRO_KEY_ESCAPE: game.running = false; break;
                    default: break;
                }
            } else {
                if (game.slotMenuMode != SlotMenuMode::None) {
                    switch (event.keyboard.keycode) {
                        case ALLEGRO_KEY_UP: if (game.selectedSaveSlot > 10) game.selectedSaveSlot -= 10; break;
                        case ALLEGRO_KEY_DOWN: if (game.selectedSaveSlot <= 90) game.selectedSaveSlot += 10; break;
                        case ALLEGRO_KEY_LEFT: if ((game.selectedSaveSlot - 1) % 10 != 0) game.selectedSaveSlot -= 1; break;
                        case ALLEGRO_KEY_RIGHT: if (game.selectedSaveSlot % 10 != 0) game.selectedSaveSlot += 1; break;
                        case ALLEGRO_KEY_ENTER:
                            if (game.slotMenuMode == SlotMenuMode::Save) {
                                if (saveGame(game, game.selectedSaveSlot)) {
                                    game.saveExists = anySaveFilesExist();
                                    game.slotMenuMode = SlotMenuMode::None;
                                    game.pauseMenuOpen = false;
                                    addMessage(game, "已存到選擇的存檔格。", 3.0f);
                                } else {
                                    addMessage(game, "存檔失敗。", 3.0f);
                                }
                            } else {
                                if (saveFileExists(game.selectedSaveSlot) && loadGame(game, game.selectedSaveSlot)) {
                                    game.saveExists = anySaveFilesExist();
                                    addMessage(game, "已讀取選擇的存檔格。", 3.0f);
                                } else {
                                    addMessage(game, "這個存檔格目前沒有資料。", 3.0f);
                                }
                            }
                            break;
                        case ALLEGRO_KEY_ESCAPE: game.slotMenuMode = SlotMenuMode::None; break;
                        default: break;
                    }
                } else if (game.pauseMenuOpen) {
                    switch (event.keyboard.keycode) {
                        case ALLEGRO_KEY_ENTER: game.pauseMenuOpen = false; break;
                        case ALLEGRO_KEY_S: game.slotMenuMode = SlotMenuMode::Save; break;
                        case ALLEGRO_KEY_L: game.slotMenuMode = SlotMenuMode::Load; break;
                        case ALLEGRO_KEY_ESCAPE: game.pauseMenuOpen = false; break;
                        default: break;
                    }
                } else {
                switch (event.keyboard.keycode) {
                    case ALLEGRO_KEY_W: game.controls.up = true; break;
                    case ALLEGRO_KEY_S: game.controls.down = true; break;
                    case ALLEGRO_KEY_A: game.controls.left = true; break;
                    case ALLEGRO_KEY_D: game.controls.right = true; break;
                    case ALLEGRO_KEY_E: if (!game.shopOpen) game.controls.gather = true; break;
                    case ALLEGRO_KEY_SPACE: game.controls.melee = true; break;
                    case ALLEGRO_KEY_H: game.controls.fish = true; break;
                    case ALLEGRO_KEY_R: eatFood(game); break;
                    case ALLEGRO_KEY_T:
                        if (game.bag.bomb <= 0) {
                            addMessage(game, "你身上沒有炸彈，先去商店買。", 2.6f);
                        } else if (game.player.bombCooldown > 0.0f) {
                            addMessage(game, "炸彈還在冷卻中。", 2.2f);
                        } else {
                            Projectile p {};
                            p.friendly = true;
                            p.damage = 34 + game.player.level * 3 + game.day / 2;
                            p.radius = 10.0f;
                            p.life = 0.85f;
                            p.pos = add(game.player.pos, mul(game.player.facing, 22.0f));
                            p.vel = mul(game.player.facing, 300.0f);
                            p.bomb = true;
                            p.blastRadius = 160.0f;
                            game.projectiles.push_back(p);
                            if (gBombThrowSample) {
                                al_play_sample(gBombThrowSample, 0.84f, 0.0f, 0.92f, ALLEGRO_PLAYMODE_ONCE, nullptr);
                            }
                            game.bag.bomb -= 1;
                            game.player.bombCooldown = 0.7f;
                            addMessage(game, "你丟出了一顆炸彈。", 2.0f);
                        }
                        break;
                    case ALLEGRO_KEY_J:
                        if (game.player.jobClass == JobClass::Vampire) {
                            if (game.player.coreRecallCooldown <= 0.0f) {
                                game.player.pos = { BASE_X, BASE_Y + 120.0f };
                                game.player.coreRecallCooldown = 18.0f;
                                addMessage(game, "吸血鬼化作黑霧，瞬間回到核心。", 3.2f);
                            } else {
                                addMessage(game, "回核心能力冷卻中。", 2.5f);
                            }
                        } else if (game.player.jobClass == JobClass::Fisher) {
                            if (game.player.onBoat) {
                                game.player.pos = findNearbyLandPosition(game, game.player.pos);
                                game.player.onBoat = false;
                                game.player.boatIndex = -1;
                                addMessage(game, "你從漁船跳回岸上。", 2.6f);
                            } else {
                                int nearestBoat = -1;
                                float bestDist = 78.0f;
                                for (int i = 0; i < static_cast<int>(game.boats.size()); ++i) {
                                    float d = distance(game.player.pos, game.boats[i].pos);
                                    if (d < bestDist) {
                                        bestDist = d;
                                        nearestBoat = i;
                                    }
                                }
                                if (nearestBoat >= 0) {
                                    game.player.onBoat = true;
                                    game.player.boatIndex = nearestBoat;
                                    game.player.pos = add(game.boats[nearestBoat].pos, { 0.0f, -20.0f });
                                    addMessage(game, "你登上了漁船，開始在河上巡航。", 2.8f);
                                } else {
                                    addMessage(game, "靠近漁船後再按 J 上船。", 2.5f);
                                }
                            }
                        } else if (game.player.jobClass == JobClass::Alien) {
                            if (game.player.alienBurstCooldown <= 0.0f) {
                                for (int i = 0; i < 8; ++i) {
                                    float angle = (ALLEGRO_PI * 2.0f / 8.0f) * i;
                                    Projectile p {};
                                    p.friendly = true;
                                    p.damage = 14 + game.day / 2;
                                    p.radius = 7.0f;
                                    p.life = 1.1f;
                                    p.pos = game.player.pos;
                                    p.vel = { std::cos(angle) * 440.0f, std::sin(angle) * 440.0f };
                                    game.projectiles.push_back(p);
                                }
                                game.player.alienBurstCooldown = 8.0f;
                                addMessage(game, "外星人釋放八連砲。", 2.8f);
                            } else {
                                addMessage(game, "八連砲冷卻中。", 2.2f);
                            }
                        } else {
                            addMessage(game, "J 只有吸血鬼或外星人職業有特殊能力。", 2.5f);
                        }
                        break;
                    case ALLEGRO_KEY_K:
                        if (game.player.jobClass == JobClass::Fisher) {
                            Vec2 boatPos = add(game.player.pos, mul(game.player.facing, 96.0f));
                            if (!isWaterTile(game, boatPos)) {
                                addMessage(game, "按 K 放船時，請先面向附近水面。", 2.6f);
                            } else {
                                bool tooClose = false;
                                for (const auto& boat : game.boats) {
                                    if (distance(boat.pos, boatPos) < 110.0f) {
                                        tooClose = true;
                                        break;
                                    }
                                }
                                if (tooClose) {
                                    addMessage(game, "這裡離其他船太近，換個水面再放。", 2.6f);
                                } else {
                                    Boat boat {};
                                    boat.pos = boatPos;
                                    boat.dir = game.player.facing.x >= 0.0f ? 1.0f : -1.0f;
                                    boat.fireCooldown = 0.8f;
                                    game.boats.push_back(boat);
                                    addMessage(game, "你放出了一艘新漁船。", 2.6f);
                                }
                            }
                        } else if (game.player.jobClass == JobClass::Alien) {
                            if (game.player.alienSummonCooldown <= 0.0f) {
                                Minion minion {};
                                minion.pos = add(game.player.pos, { 26.0f, 12.0f });
                                game.minions.push_back(minion);
                                game.player.alienSummonCooldown = 30.0f;
                                addMessage(game, "外星人召喚了一隻採集戰鬥小兵。", 3.0f);
                            } else {
                                addMessage(game, "小兵召喚冷卻中。", 2.2f);
                            }
                        } else {
                            addMessage(game, "只有外星人職業能按 K 召喚小兵。", 2.5f);
                        }
                        break;
                    case ALLEGRO_KEY_G:
                        if (game.player.potionCooldown <= 0.0f) {
                            Projectile p {};
                            p.friendly = true;
                            p.damage = 8 + game.player.level + game.day / 3 + (game.player.jobClass == JobClass::Elite ? 6 : 0) + (game.hasPoisonUpgrade ? 6 : 0);
                            p.radius = 10.0f;
                            p.life = 1.0f;
                            p.pos = add(game.player.pos, mul(game.player.facing, 26.0f));
                            p.vel = mul(game.player.facing, 360.0f);
                            p.poison = true;
                            p.poisonDuration = (game.player.jobClass == JobClass::Elite ? 6.0f : 4.5f) + (game.hasPoisonUpgrade ? 1.8f : 0.0f);
                            game.projectiles.push_back(p);
                            if (gPoisonPotionSample) {
                                al_play_sample(gPoisonPotionSample, 0.72f, 0.0f, 1.0f, ALLEGRO_PLAYMODE_ONCE, nullptr);
                            }
                            game.player.potionCooldown = 1.2f;
                            addMessage(game, "你丟出了帶毒的魔法藥水。", 2.2f);
                        }
                        break;
                    case ALLEGRO_KEY_Q: game.buildMode = !game.buildMode; break;
                    case ALLEGRO_KEY_1:
                        if (game.shopOpen) {
                            if (game.activeShop == ShopType::Bomb) {
                                if (game.bag.crystal >= 20) {
                                    game.bag.crystal -= 20;
                                    game.bag.bomb += 10;
                                    addMessage(game, "炸彈店交易成功，你買到了 10 個炸彈。", 2.8f);
                                } else {
                                    addMessage(game, "水晶不足，20 水晶才能買 10 個炸彈。", 2.8f);
                                }
                            } else if (game.activeShop == ShopType::Grocery) {
                                if (game.bag.wood >= 20) {
                                    game.bag.wood -= 20;
                                    game.bag.meat += 4;
                                    addMessage(game, "超市交易成功，你換到了 4 個肉。", 2.8f);
                                } else {
                                    addMessage(game, "木材不足，20 木材才能換 4 個肉。", 2.8f);
                                }
                            } else if (game.activeShop == ShopType::Tool) {
                                if (game.hasSword) {
                                    addMessage(game, "你已經有劍了。", 2.3f);
                                } else if (game.bag.fiber >= 40) {
                                    game.bag.fiber -= 40;
                                    game.hasSword = true;
                                    addMessage(game, "你從工具行換到一把劍，近戰傷害提升。", 3.0f);
                                } else {
                                    addMessage(game, "纖維不足，40 纖維才能換劍。", 2.8f);
                                }
                            }
                        } else {
                            game.selectedBuild = BuildingType::Wall;
                        }
                        break;
                    case ALLEGRO_KEY_4: game.selectedBuild = BuildingType::DarkMage; break;
                    case ALLEGRO_KEY_2:
                        if (game.shopOpen && game.activeShop == ShopType::Tool) {
                            if (game.hasArmor) {
                                addMessage(game, "你已經有盔甲了。", 2.3f);
                            } else if (game.bag.crystal >= 30) {
                                game.bag.crystal -= 30;
                                game.hasArmor = true;
                                addMessage(game, "你穿上了盔甲，受到的傷害降低。", 3.0f);
                            } else {
                                addMessage(game, "水晶不足，30 水晶才能換盔甲。", 2.8f);
                            }
                        } else {
                            game.selectedBuild = BuildingType::Turret;
                        }
                        break;
                    case ALLEGRO_KEY_3:
                        if (game.shopOpen && game.activeShop == ShopType::Tool) {
                            if (game.hasPoisonUpgrade) {
                                addMessage(game, "你的毒藥已經強化過了。", 2.3f);
                            } else if (game.bag.crystal >= 35) {
                                game.bag.crystal -= 35;
                                game.hasPoisonUpgrade = true;
                                addMessage(game, "藥水強化完成，毒藥傷害更高了。", 3.0f);
                            } else {
                                addMessage(game, "水晶不足，35 水晶才能強化藥水。", 2.8f);
                            }
                        } else {
                            game.selectedBuild = BuildingType::Camp;
                        }
                        break;
                    case ALLEGRO_KEY_F5: if (saveGame(game, game.currentSaveSlot)) { game.saveExists = anySaveFilesExist(); addMessage(game, "遊戲已存到目前存檔格。", 3.0f); } else addMessage(game, "存檔失敗。", 3.0f); break;
                    case ALLEGRO_KEY_F9: if (loadGame(game, game.currentSaveSlot)) game.saveExists = anySaveFilesExist(); else addMessage(game, "讀檔失敗或目前存檔格沒有資料。", 3.0f); break;
                    case ALLEGRO_KEY_ESCAPE: game.pauseMenuOpen = true; game.slotMenuMode = SlotMenuMode::None; clearControls(game); break;
                    default: break;
                }
                }
            }
        } else if (event.type == ALLEGRO_EVENT_KEY_UP && game.scene == Scene::Playing) {
            switch (event.keyboard.keycode) {
                case ALLEGRO_KEY_W: game.controls.up = false; break;
                case ALLEGRO_KEY_S: game.controls.down = false; break;
                case ALLEGRO_KEY_A: game.controls.left = false; break;
                case ALLEGRO_KEY_D: game.controls.right = false; break;
                case ALLEGRO_KEY_E: game.controls.gather = false; break;
                case ALLEGRO_KEY_SPACE: game.controls.melee = false; break;
                case ALLEGRO_KEY_H: game.controls.fish = false; break;
                default: break;
            }
        } else if (event.type == ALLEGRO_EVENT_TIMER) {
            if (game.scene == Scene::Playing && !game.pauseMenuOpen && game.slotMenuMode == SlotMenuMode::None) {
                float dt = 1.0f / static_cast<float>(FPS);
                updateFlow(game, dt);
                updatePlayer(game, dt);
                handleGather(game);
                updateResources(game, dt);
                updatePoisonClouds(game, dt);
                updateBoats(game, dt);
                updateMinions(game, dt);
                updateBuildings(game, dt);
                updateMonsters(game, dt);
                updateProjectiles(game, dt);
                cleanupDead(game);
                if (game.player.hp <= 0 || game.player.cores <= 0) {
                    saveGame(game);
                    game.saveExists = anySaveFilesExist();
                    game.scene = Scene::GameOver;
                    clearControls(game);
                }
            } else {
                game.messageTimer -= 1.0f / static_cast<float>(FPS);
            }
            redraw = true;
        }

        if (redraw && al_is_event_queue_empty(queue)) {
            redraw = false;
            if (game.scene == Scene::Title) {
                drawTitle(game, bodyFont, titleFont);
                if (game.slotMenuMode != SlotMenuMode::None) drawSlotMenu(game, bodyFont, titleFont);
            } else {
                al_clear_to_color(al_map_rgb(15, 18, 26));
                drawWorld(game, bodyFont, titleFont);
                drawUi(game, bodyFont, titleFont);
                if (game.pauseMenuOpen) drawPauseMenu(bodyFont, titleFont);
                if (game.slotMenuMode != SlotMenuMode::None) drawSlotMenu(game, bodyFont, titleFont);
                if (game.scene == Scene::GameOver) drawGameOver(game, bodyFont, titleFont);
                if (game.scene == Scene::Victory) drawVictory(game, bodyFont, titleFont);
            }
            al_flip_display();
        }
    }

    al_destroy_font(bodyFont);
    al_destroy_font(titleFont);
    if (gGunshotSample) {
        al_destroy_sample(gGunshotSample);
        gGunshotSample = nullptr;
    }
    if (gGatherSample) {
        al_destroy_sample(gGatherSample);
        gGatherSample = nullptr;
    }
    if (gMonsterDeathSample) {
        al_destroy_sample(gMonsterDeathSample);
        gMonsterDeathSample = nullptr;
    }
    if (gBossIntroSample) {
        al_destroy_sample(gBossIntroSample);
        gBossIntroSample = nullptr;
    }
    if (gPoisonPotionSample) {
        al_destroy_sample(gPoisonPotionSample);
        gPoisonPotionSample = nullptr;
    }
    if (gRescueChildSample) {
        al_destroy_sample(gRescueChildSample);
        gRescueChildSample = nullptr;
    }
    if (gBombThrowSample) {
        al_destroy_sample(gBombThrowSample);
        gBombThrowSample = nullptr;
    }
    destroyBackgroundMusic(bgm);
    al_destroy_event_queue(queue);
    al_destroy_timer(timer);
    al_destroy_display(display);
    return 0;
}






