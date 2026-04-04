#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_ttf.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
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

enum class TileBiome { Grass, Meadow, Water, DarkGrass };
enum class ResourceType { Tree, Rock, Bush, Crystal };
enum class BuildingType { None, Wall, Turret, Camp };
enum class MonsterType { Slime, Hunter, Brute };

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Inventory {
    int wood = 0;
    int stone = 0;
    int fiber = 0;
    int crystal = 0;
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
    int level = 1;
    int xp = 0;
    int xpToNext = 25;
    int cores = 100;
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
    bool alive = true;
};

struct Building {
    BuildingType type = BuildingType::None;
    Vec2 pos {};
    int hp = 1;
    int maxHp = 1;
    float radius = 18.0f;
    float fireCooldown = 0.0f;
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
};

struct GameState {
    std::vector<std::vector<TileBiome>> map;
    std::vector<ResourceNode> resources;
    std::vector<Projectile> projectiles;
    std::vector<Building> buildings;
    std::vector<Monster> monsters;
    Inventory bag { 35, 28, 14, 6 };
    Player player {};
    Controls controls {};
    BuildingType selectedBuild = BuildingType::Wall;
    bool buildMode = false;
    bool running = true;
    bool gameOver = false;
    int day = 1;
    float dayTimer = 0.0f;
    float danger = 1.0f;
    float waveTimer = 10.0f;
    float messageTimer = 8.0f;
    std::string message = "採集資源、建立防線，守住你的核心據點。";
    float mouseScreenX = SCREEN_W * 0.5f;
    float mouseScreenY = SCREEN_H * 0.5f;
    std::mt19937 rng { static_cast<std::mt19937::result_type>(std::time(nullptr)) };
};

float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}

float length(Vec2 v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

Vec2 normalize(Vec2 v) {
    float len = length(v);
    if (len <= 0.0001f) {
        return { 0.0f, 0.0f };
    }
    return { v.x / len, v.y / len };
}

float distance(Vec2 a, Vec2 b) {
    return length({ a.x - b.x, a.y - b.y });
}

Vec2 add(Vec2 a, Vec2 b) {
    return { a.x + b.x, a.y + b.y };
}

Vec2 sub(Vec2 a, Vec2 b) {
    return { a.x - b.x, a.y - b.y };
}

Vec2 mul(Vec2 a, float s) {
    return { a.x * s, a.y * s };
}

float randf(GameState& game, float minValue, float maxValue) {
    std::uniform_real_distribution<float> dist(minValue, maxValue);
    return dist(game.rng);
}

int randi(GameState& game, int minValue, int maxValue) {
    std::uniform_int_distribution<int> dist(minValue, maxValue);
    return dist(game.rng);
}

bool isNight(const GameState& game) {
    return game.dayTimer >= 95.0f;
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
        default: return "無";
    }
}

Inventory buildCost(BuildingType type) {
    switch (type) {
        case BuildingType::Wall: return { 10, 6, 0, 0 };
        case BuildingType::Turret: return { 18, 14, 0, 2 };
        case BuildingType::Camp: return { 25, 18, 10, 4 };
        default: return {};
    }
}

bool canAfford(const Inventory& bag, const Inventory& cost) {
    return bag.wood >= cost.wood &&
        bag.stone >= cost.stone &&
        bag.fiber >= cost.fiber &&
        bag.crystal >= cost.crystal;
}

void spendResources(Inventory& bag, const Inventory& cost) {
    bag.wood -= cost.wood;
    bag.stone -= cost.stone;
    bag.fiber -= cost.fiber;
    bag.crystal -= cost.crystal;
}

void addMessage(GameState& game, const std::string& text, float duration = 5.0f) {
    game.message = text;
    game.messageTimer = duration;
}

Vec2 worldToScreen(Vec2 world, Vec2 camera) {
    return { world.x - camera.x, world.y - camera.y };
}

Vec2 screenToWorld(Vec2 screen, Vec2 camera) {
    return { screen.x + camera.x, screen.y + camera.y };
}

Vec2 getCamera(const GameState& game) {
    float camX = clampf(game.player.pos.x - SCREEN_W * 0.5f, 0.0f, WORLD_PIXEL_W - SCREEN_W);
    float camY = clampf(game.player.pos.y - SCREEN_H * 0.5f, 0.0f, WORLD_PIXEL_H - SCREEN_H);
    return { camX, camY };
}

bool isPassable(const GameState& game, Vec2 pos) {
    pos.x = clampf(pos.x, 0.0f, WORLD_PIXEL_W - 1.0f);
    pos.y = clampf(pos.y, 0.0f, WORLD_PIXEL_H - 1.0f);
    int tx = static_cast<int>(pos.x / TILE_SIZE);
    int ty = static_cast<int>(pos.y / TILE_SIZE);
    return game.map[ty][tx] != TileBiome::Water;
}

void grantXp(GameState& game, int amount) {
    game.player.xp += amount;
    while (game.player.xp >= game.player.xpToNext) {
        game.player.xp -= game.player.xpToNext;
        game.player.level += 1;
        game.player.maxHp += 12;
        game.player.hp = std::min(game.player.maxHp, game.player.hp + 24);
        game.player.xpToNext += 18;
        game.player.cores += 10;
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

            if (noise < -0.38f) {
                game.map[y][x] = TileBiome::Water;
            } else if (noise < 0.02f) {
                game.map[y][x] = TileBiome::DarkGrass;
            } else if (noise < 0.25f) {
                game.map[y][x] = TileBiome::Grass;
            } else {
                game.map[y][x] = TileBiome::Meadow;
            }
        }
    }

    for (int y = WORLD_H / 2 - 4; y <= WORLD_H / 2 + 4; ++y) {
        for (int x = WORLD_W / 2 - 4; x <= WORLD_W / 2 + 4; ++x) {
            if (x >= 0 && y >= 0 && x < WORLD_W && y < WORLD_H) {
                game.map[y][x] = TileBiome::Meadow;
            }
        }
    }

    game.resources.clear();
    for (int i = 0; i < 900; ++i) {
        ResourceNode node {};
        float x = randf(game, TILE_SIZE, WORLD_PIXEL_W - TILE_SIZE);
        float y = randf(game, TILE_SIZE, WORLD_PIXEL_H - TILE_SIZE);
        if (!isPassable(game, { x, y })) {
            continue;
        }
        if (distance({ x, y }, { BASE_X, BASE_Y }) < 240.0f) {
            continue;
        }
        int roll = randi(game, 0, 99);
        if (roll < 42) {
            node.type = ResourceType::Tree;
            node.hp = node.maxHp = 5;
        } else if (roll < 72) {
            node.type = ResourceType::Rock;
            node.hp = node.maxHp = 6;
        } else if (roll < 92) {
            node.type = ResourceType::Bush;
            node.hp = node.maxHp = 4;
        } else {
            node.type = ResourceType::Crystal;
            node.hp = node.maxHp = 8;
        }
        node.pos = { x, y };
        game.resources.push_back(node);
    }
}

void spawnMonster(GameState& game, int count) {
    for (int i = 0; i < count; ++i) {
        Monster monster {};
        int side = randi(game, 0, 3);
        if (side == 0) monster.pos = { randf(game, 0.0f, WORLD_PIXEL_W), -40.0f };
        if (side == 1) monster.pos = { WORLD_PIXEL_W + 40.0f, randf(game, 0.0f, WORLD_PIXEL_H) };
        if (side == 2) monster.pos = { randf(game, 0.0f, WORLD_PIXEL_W), WORLD_PIXEL_H + 40.0f };
        if (side == 3) monster.pos = { -40.0f, randf(game, 0.0f, WORLD_PIXEL_H) };

        int roll = randi(game, 0, 99);
        if (roll < 50) {
            monster.type = MonsterType::Slime;
            monster.hp = monster.maxHp = 26 + static_cast<int>(game.danger * 4.0f);
            monster.damage = 8 + static_cast<int>(game.danger * 1.5f);
            monster.speed = 92.0f + game.danger * 4.0f;
        } else if (roll < 82) {
            monster.type = MonsterType::Hunter;
            monster.hp = monster.maxHp = 38 + static_cast<int>(game.danger * 5.0f);
            monster.damage = 12 + static_cast<int>(game.danger * 2.0f);
            monster.speed = 118.0f + game.danger * 5.0f;
        } else {
            monster.type = MonsterType::Brute;
            monster.hp = monster.maxHp = 72 + static_cast<int>(game.danger * 8.0f);
            monster.damage = 18 + static_cast<int>(game.danger * 3.0f);
            monster.speed = 72.0f + game.danger * 3.0f;
        }
        game.monsters.push_back(monster);
    }
}

Building makeBuilding(BuildingType type, Vec2 pos) {
    Building b {};
    b.type = type;
    b.pos = pos;
    if (type == BuildingType::Wall) {
        b.maxHp = b.hp = 120;
        b.radius = 28.0f;
    } else if (type == BuildingType::Turret) {
        b.maxHp = b.hp = 90;
        b.radius = 22.0f;
        b.fireCooldown = 0.5f;
    } else if (type == BuildingType::Camp) {
        b.maxHp = b.hp = 180;
        b.radius = 36.0f;
    }
    return b;
}

void tryPlaceBuilding(GameState& game, Vec2 worldPos) {
    if (!game.buildMode || game.selectedBuild == BuildingType::None) {
        return;
    }
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
        addMessage(game, "材料不足，無法建造這個設施。", 3.0f);
        return;
    }
    spendResources(game.bag, cost);
    game.buildings.push_back(makeBuilding(game.selectedBuild, worldPos));
    addMessage(game, std::string(buildName(game.selectedBuild)) + " 已建造完成。", 3.0f);
}

void damageMonster(GameState& game, Monster& monster, int damage) {
    monster.hp -= damage;
    if (monster.hp <= 0) {
        monster.alive = false;
        game.bag.wood += randi(game, 0, 2);
        game.bag.stone += randi(game, 0, 2);
        game.bag.fiber += randi(game, 0, 1);
        if (randi(game, 0, 99) > 78) {
            game.bag.crystal += 1;
        }
        grantXp(game, monster.type == MonsterType::Brute ? 8 : 4);
    }
}

void handleGather(GameState& game) {
    if (!game.controls.gather || game.player.gatherCooldown > 0.0f) {
        return;
    }
    ResourceNode* best = nullptr;
    float bestDist = 85.0f;
    for (auto& node : game.resources) {
        if (!node.alive) {
            continue;
        }
        float d = distance(game.player.pos, node.pos);
        if (d < bestDist) {
            best = &node;
            bestDist = d;
        }
    }
    if (!best) {
        addMessage(game, "附近沒有可採集資源，靠近後按 E。", 2.2f);
        game.player.gatherCooldown = 0.35f;
        return;
    }

    best->hp -= 1 + (game.player.level >= 4 ? 1 : 0);
    game.player.gatherCooldown = 0.25f;
    if (best->type == ResourceType::Tree) game.bag.wood += 2;
    if (best->type == ResourceType::Rock) game.bag.stone += 2;
    if (best->type == ResourceType::Bush) game.bag.fiber += 2;
    if (best->type == ResourceType::Crystal) game.bag.crystal += 1;
    grantXp(game, 1);

    if (best->hp <= 0) {
        best->alive = false;
        best->respawnTimer = 18.0f + randf(game, 0.0f, 10.0f);
    }
}

void updateProjectiles(GameState& game, float dt) {
    for (auto& p : game.projectiles) {
        if (!p.alive) {
            continue;
        }
        p.pos = add(p.pos, mul(p.vel, dt));
        p.life -= dt;
        if (p.life <= 0.0f || p.pos.x < 0.0f || p.pos.y < 0.0f || p.pos.x > WORLD_PIXEL_W || p.pos.y > WORLD_PIXEL_H) {
            p.alive = false;
            continue;
        }

        if (p.friendly) {
            for (auto& monster : game.monsters) {
                if (!monster.alive) {
                    continue;
                }
                if (distance(p.pos, monster.pos) <= p.radius + 18.0f) {
                    damageMonster(game, monster, p.damage);
                    p.alive = false;
                    break;
                }
            }
        } else if (distance(p.pos, game.player.pos) <= p.radius + PLAYER_RADIUS) {
            game.player.hp -= p.damage;
            p.alive = false;
        }
    }
}

void updateMonsters(GameState& game, float dt) {
    for (auto& monster : game.monsters) {
        if (!monster.alive) {
            continue;
        }

        Vec2 target = game.player.pos;
        float nearest = distance(monster.pos, game.player.pos);

        for (const auto& building : game.buildings) {
            if (!building.alive) {
                continue;
            }
            float d = distance(monster.pos, building.pos);
            if (d < nearest) {
                nearest = d;
                target = building.pos;
            }
        }

        if (distance(monster.pos, { BASE_X, BASE_Y }) < nearest) {
            target = { BASE_X, BASE_Y };
        }

        Vec2 dir = normalize(sub(target, monster.pos));
        monster.vel = mul(dir, monster.speed);
        monster.pos = add(monster.pos, mul(monster.vel, dt));
        monster.attackCooldown -= dt;

        if (monster.type == MonsterType::Hunter && monster.attackCooldown <= 0.0f && distance(monster.pos, game.player.pos) < 260.0f) {
            Projectile p {};
            p.friendly = false;
            p.damage = monster.damage;
            p.radius = 7.0f;
            p.life = 1.5f;
            p.pos = monster.pos;
            p.vel = mul(normalize(sub(game.player.pos, monster.pos)), 340.0f);
            game.projectiles.push_back(p);
            monster.attackCooldown = 1.7f;
        }

        bool didAttack = false;
        if (distance(monster.pos, game.player.pos) <= PLAYER_RADIUS + 22.0f && monster.attackCooldown <= 0.0f) {
            game.player.hp -= monster.damage;
            monster.attackCooldown = 1.0f;
            didAttack = true;
        }

        if (!didAttack) {
            for (auto& building : game.buildings) {
                if (!building.alive) {
                    continue;
                }
                if (distance(monster.pos, building.pos) <= building.radius + 20.0f && monster.attackCooldown <= 0.0f) {
                    building.hp -= monster.damage;
                    monster.attackCooldown = 0.85f;
                    didAttack = true;
                    break;
                }
            }
        }

        if (!didAttack && distance(monster.pos, { BASE_X, BASE_Y }) <= 60.0f && monster.attackCooldown <= 0.0f) {
            game.player.cores -= monster.damage;
            monster.attackCooldown = 0.85f;
        }
    }
}

void updateBuildings(GameState& game, float dt) {
    for (auto& building : game.buildings) {
        if (!building.alive) {
            continue;
        }
        if (building.hp <= 0) {
            building.alive = false;
            continue;
        }

        if (building.type == BuildingType::Camp) {
            if (distance(game.player.pos, building.pos) < 100.0f && game.player.hp < game.player.maxHp) {
                game.player.hp = std::min(game.player.maxHp, game.player.hp + static_cast<int>(18.0f * dt));
            }
        }

        if (building.type == BuildingType::Turret) {
            building.fireCooldown -= dt;
            Monster* target = nullptr;
            float bestDist = 300.0f;
            for (auto& monster : game.monsters) {
                if (!monster.alive) {
                    continue;
                }
                float d = distance(building.pos, monster.pos);
                if (d < bestDist) {
                    bestDist = d;
                    target = &monster;
                }
            }
            if (target && building.fireCooldown <= 0.0f) {
                Projectile p {};
                p.friendly = true;
                p.damage = 12 + game.player.level * 2;
                p.radius = 5.0f;
                p.life = 1.3f;
                p.pos = building.pos;
                p.vel = mul(normalize(sub(target->pos, building.pos)), 500.0f);
                game.projectiles.push_back(p);
                building.fireCooldown = 0.55f;
            }
        }
    }
}

void updateResources(GameState& game, float dt) {
    for (auto& node : game.resources) {
        if (node.alive) {
            continue;
        }
        node.respawnTimer -= dt;
        if (node.respawnTimer <= 0.0f) {
            node.alive = true;
            node.hp = node.maxHp;
        }
    }
}

void cleanupDead(GameState& game) {
    game.projectiles.erase(
        std::remove_if(game.projectiles.begin(), game.projectiles.end(),
            [](const Projectile& p) { return !p.alive; }),
        game.projectiles.end());

    game.monsters.erase(
        std::remove_if(game.monsters.begin(), game.monsters.end(),
            [](const Monster& m) { return !m.alive; }),
        game.monsters.end());
}

void updatePlayer(GameState& game, float dt) {
    Vec2 move {};
    if (game.controls.up) move.y -= 1.0f;
    if (game.controls.down) move.y += 1.0f;
    if (game.controls.left) move.x -= 1.0f;
    if (game.controls.right) move.x += 1.0f;

    move = normalize(move);
    Vec2 nextPos = add(game.player.pos, mul(move, game.player.speed * dt));
    nextPos.x = clampf(nextPos.x, PLAYER_RADIUS, WORLD_PIXEL_W - PLAYER_RADIUS);
    nextPos.y = clampf(nextPos.y, PLAYER_RADIUS, WORLD_PIXEL_H - PLAYER_RADIUS);

    if (isPassable(game, nextPos)) {
        game.player.pos = nextPos;
    }

    if (length(move) > 0.05f) {
        game.player.facing = move;
    }

    game.player.attackCooldown -= dt;
    game.player.shootCooldown -= dt;
    game.player.gatherCooldown -= dt;

    Vec2 camera = getCamera(game);
    Vec2 mouseWorld = screenToWorld({ game.mouseScreenX, game.mouseScreenY }, camera);
    Vec2 aimDir = normalize(sub(mouseWorld, game.player.pos));
    if (length(aimDir) > 0.01f) {
        game.player.facing = aimDir;
    }

    if (game.controls.melee && game.player.attackCooldown <= 0.0f) {
        for (auto& monster : game.monsters) {
            if (!monster.alive) {
                continue;
            }
            if (distance(game.player.pos, monster.pos) < 68.0f) {
                Vec2 toward = normalize(sub(monster.pos, game.player.pos));
                if (toward.x * game.player.facing.x + toward.y * game.player.facing.y > 0.1f) {
                    damageMonster(game, monster, 18 + game.player.level * 4);
                }
            }
        }
        game.player.attackCooldown = 0.35f;
    }

    if (!game.buildMode && game.controls.shoot && game.player.shootCooldown <= 0.0f) {
        Projectile p {};
        p.friendly = true;
        p.damage = 14 + game.player.level * 2;
        p.radius = 6.0f;
        p.life = 1.2f;
        p.pos = add(game.player.pos, mul(game.player.facing, 24.0f));
        p.vel = mul(game.player.facing, 520.0f);
        game.projectiles.push_back(p);
        game.player.shootCooldown = 0.18f;
    }
}

void updateFlow(GameState& game, float dt) {
    game.dayTimer += dt;
    game.waveTimer -= dt;
    game.messageTimer -= dt;

    if (game.dayTimer >= 120.0f) {
        game.day += 1;
        game.dayTimer = 0.0f;
        game.danger += 0.65f;
        addMessage(game, "新的一天開始了，夜晚的怪物會更兇猛。", 5.0f);
    }

    int desiredMonsters = isNight(game) ? static_cast<int>(4 + game.day * 2 + game.danger) : static_cast<int>(1 + game.day * 0.6f);
    if (game.waveTimer <= 0.0f && static_cast<int>(game.monsters.size()) < desiredMonsters) {
        spawnMonster(game, std::max(1, desiredMonsters - static_cast<int>(game.monsters.size())));
        game.waveTimer = isNight(game) ? 2.5f : 8.5f;
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

    for (const auto& building : game.buildings) {
        if (!building.alive) continue;
        Vec2 p = worldToScreen(building.pos, camera);
        if (building.type == BuildingType::Wall) {
            al_draw_filled_rectangle(p.x - 28.0f, p.y - 20.0f, p.x + 28.0f, p.y + 20.0f, al_map_rgb(137, 115, 96));
        } else if (building.type == BuildingType::Turret) {
            al_draw_filled_circle(p.x, p.y, 22.0f, al_map_rgb(101, 105, 118));
            al_draw_filled_rectangle(p.x - 5.0f, p.y - 26.0f, p.x + 5.0f, p.y - 2.0f, al_map_rgb(188, 194, 207));
        } else if (building.type == BuildingType::Camp) {
            al_draw_filled_triangle(p.x, p.y - 36.0f, p.x - 34.0f, p.y + 30.0f, p.x + 34.0f, p.y + 30.0f, al_map_rgb(199, 110, 62));
            al_draw_filled_rectangle(p.x - 10.0f, p.y + 6.0f, p.x + 10.0f, p.y + 30.0f, al_map_rgb(95, 54, 24));
        }
        drawProgressBar(p.x - 28.0f, p.y - building.radius - 18.0f, 56.0f, 10.0f, static_cast<float>(building.hp) / building.maxHp,
            al_map_rgba(30, 30, 30, 180), al_map_rgb(106, 230, 126));
    }

    for (const auto& monster : game.monsters) {
        if (!monster.alive) continue;
        Vec2 p = worldToScreen(monster.pos, camera);
        ALLEGRO_COLOR c = al_map_rgb(164, 74, 72);
        if (monster.type == MonsterType::Hunter) c = al_map_rgb(145, 84, 167);
        if (monster.type == MonsterType::Brute) c = al_map_rgb(128, 50, 36);
        al_draw_filled_circle(p.x, p.y, monster.type == MonsterType::Brute ? 22.0f : 18.0f, c);
        al_draw_filled_circle(p.x - 6.0f, p.y - 3.0f, 3.5f, al_map_rgb(255, 240, 230));
        al_draw_filled_circle(p.x + 6.0f, p.y - 3.0f, 3.5f, al_map_rgb(255, 240, 230));
        drawProgressBar(p.x - 24.0f, p.y - 34.0f, 48.0f, 8.0f, static_cast<float>(monster.hp) / monster.maxHp,
            al_map_rgba(25, 25, 25, 180), al_map_rgb(238, 91, 85));
    }

    for (const auto& p : game.projectiles) {
        Vec2 s = worldToScreen(p.pos, camera);
        al_draw_filled_circle(s.x, s.y, p.radius, p.friendly ? al_map_rgb(255, 220, 92) : al_map_rgb(226, 96, 191));
    }

    Vec2 player = worldToScreen(game.player.pos, camera);
    al_draw_filled_circle(player.x, player.y, PLAYER_RADIUS, al_map_rgb(69, 158, 255));
    al_draw_filled_circle(player.x, player.y - 24.0f, 11.0f, al_map_rgb(255, 222, 191));
    al_draw_line(player.x, player.y, player.x + game.player.facing.x * 30.0f, player.y + game.player.facing.y * 30.0f, al_map_rgb(255, 255, 255), 4.0f);

    if (game.buildMode) {
        Vec2 ghost = screenToWorld({ game.mouseScreenX, game.mouseScreenY }, camera);
        ghost.x = std::round(ghost.x / 16.0f) * 16.0f;
        ghost.y = std::round(ghost.y / 16.0f) * 16.0f;
        Vec2 s = worldToScreen(ghost, camera);
        al_draw_circle(s.x, s.y, game.selectedBuild == BuildingType::Camp ? 36.0f : 28.0f, al_map_rgba(255, 255, 255, 180), 3.0f);
        al_draw_textf(bodyFont, al_map_rgb(255, 255, 255), s.x, s.y - 48.0f, ALLEGRO_ALIGN_CENTER, "放置：%s", buildName(game.selectedBuild));
    }

    float darkness = isNight(game) ? 0.34f : (game.dayTimer > 82.0f ? (game.dayTimer - 82.0f) / 13.0f * 0.2f : 0.0f);
    if (darkness > 0.01f) {
        al_draw_filled_rectangle(0.0f, 0.0f, SCREEN_W, SCREEN_H, al_map_rgba_f(0.05f, 0.07f, 0.15f, darkness));
    }
}

void drawMinimap(const GameState& game, float x, float y, float w, float h) {
    al_draw_filled_rounded_rectangle(x, y, x + w, y + h, 12.0f, 12.0f, al_map_rgba(10, 14, 16, 210));
    float scaleX = w / WORLD_PIXEL_W;
    float scaleY = h / WORLD_PIXEL_H;

    for (int ty = 0; ty < WORLD_H; ty += 2) {
        for (int tx = 0; tx < WORLD_W; tx += 2) {
            al_draw_filled_rectangle(
                x + tx * TILE_SIZE * scaleX,
                y + ty * TILE_SIZE * scaleY,
                x + (tx + 2) * TILE_SIZE * scaleX,
                y + (ty + 2) * TILE_SIZE * scaleY,
                biomeColor(game.map[ty][tx]));
        }
    }

    for (const auto& building : game.buildings) {
        if (!building.alive) continue;
        al_draw_filled_circle(x + building.pos.x * scaleX, y + building.pos.y * scaleY, 3.0f, al_map_rgb(246, 215, 123));
    }
    for (const auto& monster : game.monsters) {
        if (!monster.alive) continue;
        al_draw_filled_circle(x + monster.pos.x * scaleX, y + monster.pos.y * scaleY, 2.0f, al_map_rgb(255, 82, 82));
    }
    al_draw_filled_circle(x + game.player.pos.x * scaleX, y + game.player.pos.y * scaleY, 4.0f, al_map_rgb(80, 198, 255));
}

void drawUi(const GameState& game, ALLEGRO_FONT* bodyFont, ALLEGRO_FONT* titleFont) {
    al_draw_filled_rounded_rectangle(18.0f, 18.0f, 520.0f, 184.0f, 16.0f, 16.0f, al_map_rgba(8, 12, 16, 210));
    al_draw_text(titleFont, al_map_rgb(255, 244, 214), 36.0f, 28.0f, 0, "邊境據點");
    al_draw_textf(bodyFont, al_map_rgb(201, 226, 255), 38.0f, 78.0f, 0, "第 %d 天  %s", game.day, isNight(game) ? "夜襲中" : "白天探索");
    al_draw_textf(bodyFont, al_map_rgb(201, 226, 255), 38.0f, 110.0f, 0, "等級 %d  經驗 %d/%d", game.player.level, game.player.xp, game.player.xpToNext);
    al_draw_textf(bodyFont, al_map_rgb(201, 226, 255), 38.0f, 142.0f, 0, "怪物 %d  危險值 %.1f", static_cast<int>(game.monsters.size()), game.danger);

    drawProgressBar(552.0f, 24.0f, 320.0f, 22.0f, static_cast<float>(game.player.hp) / game.player.maxHp,
        al_map_rgba(20, 20, 20, 190), al_map_rgb(222, 88, 88));
    al_draw_textf(bodyFont, al_map_rgb(255, 255, 255), 560.0f, 48.0f, 0, "生命 %d / %d", game.player.hp, game.player.maxHp);

    drawProgressBar(552.0f, 86.0f, 320.0f, 22.0f, clampf(static_cast<float>(game.player.cores) / 100.0f, 0.0f, 1.0f),
        al_map_rgba(20, 20, 20, 190), al_map_rgb(255, 208, 98));
    al_draw_textf(bodyFont, al_map_rgb(255, 255, 255), 560.0f, 110.0f, 0, "核心穩定度 %d / 100", std::max(0, game.player.cores));

    al_draw_filled_rounded_rectangle(18.0f, SCREEN_H - 176.0f, 732.0f, SCREEN_H - 18.0f, 16.0f, 16.0f, al_map_rgba(8, 12, 16, 210));
    al_draw_textf(bodyFont, al_map_rgb(255, 238, 205), 36.0f, SCREEN_H - 160.0f, 0, "木材 %d   石材 %d   纖維 %d   水晶 %d",
        game.bag.wood, game.bag.stone, game.bag.fiber, game.bag.crystal);
    al_draw_textf(bodyFont, al_map_rgb(214, 230, 240), 36.0f, SCREEN_H - 124.0f, 0, "目前建築：%s   模式：%s",
        buildName(game.selectedBuild), game.buildMode ? "建造" : "戰鬥");
    al_draw_text(bodyFont, al_map_rgb(170, 202, 221), 36.0f, SCREEN_H - 92.0f, 0,
        "WASD 移動   E 採集   Space 近戰   滑鼠左鍵 射擊 / 放置   Q 切換建造");
    al_draw_text(bodyFont, al_map_rgb(170, 202, 221), 36.0f, SCREEN_H - 60.0f, 0,
        "1 圍牆   2 砲塔   3 營地   撐過夜晚，守住金色核心。");

    al_draw_filled_rounded_rectangle(930.0f, 18.0f, SCREEN_W - 18.0f, 138.0f, 16.0f, 16.0f, al_map_rgba(8, 12, 16, 210));
    al_draw_text(titleFont, al_map_rgb(255, 235, 183), 950.0f, 30.0f, 0, "目標");
    al_draw_text(bodyFont, al_map_rgb(230, 236, 240), 950.0f, 78.0f, 0, "先採集木材與石材，建立圍牆，再用砲塔擴大防守範圍。");
    al_draw_text(bodyFont, al_map_rgb(230, 236, 240), 950.0f, 108.0f, 0, "營地能治療你，水晶則可支撐更高級的防禦建築。");

    drawMinimap(game, SCREEN_W - 280.0f, SCREEN_H - 220.0f, 250.0f, 190.0f);

    if (game.messageTimer > 0.0f) {
        al_draw_filled_rounded_rectangle(340.0f, SCREEN_H - 246.0f, SCREEN_W - 340.0f, SCREEN_H - 188.0f, 16.0f, 16.0f, al_map_rgba(18, 22, 30, 210));
        al_draw_text(bodyFont, al_map_rgb(255, 245, 214), SCREEN_W * 0.5f, SCREEN_H - 232.0f, ALLEGRO_ALIGN_CENTER, game.message.c_str());
    }

    if (game.gameOver) {
        al_draw_filled_rectangle(0.0f, 0.0f, SCREEN_W, SCREEN_H, al_map_rgba(0, 0, 0, 180));
        al_draw_text(titleFont, al_map_rgb(255, 220, 210), SCREEN_W * 0.5f, SCREEN_H * 0.5f - 60.0f, ALLEGRO_ALIGN_CENTER, "據點淪陷");
        al_draw_text(bodyFont, al_map_rgb(255, 255, 255), SCREEN_W * 0.5f, SCREEN_H * 0.5f, ALLEGRO_ALIGN_CENTER,
            "邊境防線已被突破。按 ESC 關閉，下一輪重整防線再戰。");
    }
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
        if (font) {
            return font;
        }
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
    if (!timer || !queue || !bodyFont || !titleFont) return 1;

    al_register_event_source(queue, al_get_display_event_source(display));
    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_mouse_event_source());
    al_register_event_source(queue, al_get_timer_event_source(timer));

    GameState game {};
    seedWorld(game);
    game.buildings.push_back(makeBuilding(BuildingType::Camp, { BASE_X + 110.0f, BASE_Y + 50.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Wall, { BASE_X - 90.0f, BASE_Y }));
    game.buildings.push_back(makeBuilding(BuildingType::Wall, { BASE_X + 90.0f, BASE_Y }));
    game.buildings.push_back(makeBuilding(BuildingType::Wall, { BASE_X, BASE_Y - 90.0f }));
    game.buildings.push_back(makeBuilding(BuildingType::Wall, { BASE_X, BASE_Y + 90.0f }));

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
        } else if (event.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
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
        } else if (event.type == ALLEGRO_EVENT_MOUSE_BUTTON_UP) {
            if (event.mouse.button == 1) game.controls.shoot = false;
        } else if (event.type == ALLEGRO_EVENT_KEY_DOWN) {
            switch (event.keyboard.keycode) {
                case ALLEGRO_KEY_W: game.controls.up = true; break;
                case ALLEGRO_KEY_S: game.controls.down = true; break;
                case ALLEGRO_KEY_A: game.controls.left = true; break;
                case ALLEGRO_KEY_D: game.controls.right = true; break;
                case ALLEGRO_KEY_E: game.controls.gather = true; break;
                case ALLEGRO_KEY_SPACE: game.controls.melee = true; break;
                case ALLEGRO_KEY_Q: game.buildMode = !game.buildMode; break;
                case ALLEGRO_KEY_1: game.selectedBuild = BuildingType::Wall; break;
                case ALLEGRO_KEY_2: game.selectedBuild = BuildingType::Turret; break;
                case ALLEGRO_KEY_3: game.selectedBuild = BuildingType::Camp; break;
                case ALLEGRO_KEY_ESCAPE: game.running = false; break;
                default: break;
            }
        } else if (event.type == ALLEGRO_EVENT_KEY_UP) {
            switch (event.keyboard.keycode) {
                case ALLEGRO_KEY_W: game.controls.up = false; break;
                case ALLEGRO_KEY_S: game.controls.down = false; break;
                case ALLEGRO_KEY_A: game.controls.left = false; break;
                case ALLEGRO_KEY_D: game.controls.right = false; break;
                case ALLEGRO_KEY_E: game.controls.gather = false; break;
                case ALLEGRO_KEY_SPACE: game.controls.melee = false; break;
                default: break;
            }
        } else if (event.type == ALLEGRO_EVENT_TIMER) {
            if (!game.gameOver) {
                float dt = 1.0f / static_cast<float>(FPS);
                updateFlow(game, dt);
                updatePlayer(game, dt);
                handleGather(game);
                updateResources(game, dt);
                updateBuildings(game, dt);
                updateMonsters(game, dt);
                updateProjectiles(game, dt);
                cleanupDead(game);
                if (game.player.hp <= 0 || game.player.cores <= 0) {
                    game.gameOver = true;
                }
            }
            redraw = true;
        }

        if (redraw && al_is_event_queue_empty(queue)) {
            redraw = false;
            al_clear_to_color(al_map_rgb(15, 18, 26));
            drawWorld(game, bodyFont, titleFont);
            drawUi(game, bodyFont, titleFont);
            al_flip_display();
        }
    }

    al_destroy_font(bodyFont);
    al_destroy_font(titleFont);
    al_destroy_event_queue(queue);
    al_destroy_timer(timer);
    al_destroy_display(display);
    return 0;
}
