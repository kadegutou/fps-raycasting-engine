#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>
#include <array>
#include <algorithm>
#include <random>
#include <iomanip>
#include <optional>
#include <queue>
#include <chrono>
#include <SDL3/SDL.h>
#include <cstdint>
using std::uint32_t;

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

// ============================================================
// Module 2: Map
// ============================================================
class Map {
public:
    [[nodiscard]] int exit_x() const noexcept { return exit_x_; }
    [[nodiscard]] int exit_y() const noexcept { return exit_y_; }

    [[nodiscard]] bool is_rock(int x, int y) const noexcept {
        for (auto [rx, ry] : rocks_)
            if (rx == x && ry == y) return true;
        return false;
    }

    explicit Map(int w, int h)
        : width_(w), height_(h), grid_(h, std::vector<int>(w, 1)), floor_h_(h, std::vector<int>(w, 0))
    {}

    static Map create_maze(int w = 41, int h = 41) {
        Map m(w, h);
        m.generate_dfs(1, 1);

        std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist_x(1, w - 2);
        std::uniform_int_distribution<int> dist_y(1, h - 2);

        // 1. 走廊拓宽：把 1 格通道扩成 2 格宽（墙密度降 40%）
        for (int y = 2; y < h - 2; ++y) {
            for (int x = 2; x < w - 2; ++x) {
                if (m.grid_[y][x] == 0) {  // 是通道才拓宽
                    if (m.grid_[y-1][x] == 1 && (rng() % 3 == 0)) m.grid_[y-1][x] = 0;  // 上
                    if (m.grid_[y+1][x] == 1 && (rng() % 3 == 0)) m.grid_[y+1][x] = 0;  // 下
                    if (m.grid_[y][x-1] == 1 && (rng() % 3 == 0)) m.grid_[y][x-1] = 0;  // 左
                    if (m.grid_[y][x+1] == 1 && (rng() % 3 == 0)) m.grid_[y][x+1] = 0;  // 右
                }
            }
        }

        // 2. 炸大房间：随机打通 5x5 开阔区域
        int rooms = (w * h) / 200;
        for (int i = 0; i < rooms; ++i) {
            int cx = dist_x(rng);
            int cy = dist_y(rng);
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    int nx = cx + dx, ny = cy + dy;
                    if (nx > 0 && nx < w - 1 && ny > 0 && ny < h - 1)
                        m.grid_[ny][nx] = 0;
                }
            }
        }

        // 3. 确保出生点和出口周围通畅
        m.exit_x_ = w - 2;
        m.exit_y_ = h - 2;
        m.grid_[m.exit_y_][m.exit_x_] = 0;
        // 出生点 5x5 通畅
        for (int dy = 0; dy <= 4; ++dy)
            for (int dx = 0; dx <= 4; ++dx)
                m.grid_[1+dy][1+dx] = 0;

        // 出口 5x5 通畅
        for (int dy = 0; dy <= 4; ++dy)
            for (int dx = 0; dx <= 4; ++dx)
                m.grid_[(h-2)-dy][(w-2)-dx] = 0;

        // 4. 放置独立石头（只放在 3x3 房间中央）
        int rock_count = rooms;  // 每个房间放一个石头
        for (int i = 0; i < rock_count; ++i) {
            int rx, ry;
            int attempts = 0;
            do {
                rx = dist_x(rng);
                ry = dist_y(rng);
                ++attempts;
            } while (attempts < 50 && (
                (rx <= 4 && ry <= 4) ||
                (rx >= w - 5 && ry >= h - 5) ||
                // 必须是通道
                m.grid_[ry][rx] != 0 ||
                // 周围 3x3 必须全是通道（确保在开阔地）
                m.grid_[ry-1][rx-1] != 0 || m.grid_[ry-1][rx] != 0 || m.grid_[ry-1][rx+1] != 0 ||
                m.grid_[ry][rx-1] != 0 || m.grid_[ry][rx+1] != 0 ||
                m.grid_[ry+1][rx-1] != 0 || m.grid_[ry+1][rx] != 0 || m.grid_[ry+1][rx+1] != 0 ||
                m.is_rock(rx, ry)
            ));

            if (attempts >= 50) continue;

            // 放置 2x2 石头
            m.rocks_.push_back({rx, ry});
            m.rocks_.push_back({rx+1, ry});
            m.rocks_.push_back({rx, ry+1});
            m.rocks_.push_back({rx+1, ry+1});
        }

        // 5. 台阶地形：随机区域抬高 1~2 格
        const int MH = h;  // ← 保存迷宫高度，防止被局部变量覆盖
        int steps = (w * MH) / 80;  // 台阶数量翻倍
        for (int i = 0; i < steps; ++i) {
            int cx = dist_x(rng);
            int cy = dist_y(rng);
            if (m.grid_[cy][cx] != 0) continue;
            int step_h = 1 + (rng() % 2);  // ← 改名：不再叫 h
            for (int dy = -2; dy <= 2; ++dy) {  // 5x5 大台阶
                for (int dx = -2; dx <= 2; ++dx) {
                    int nx = cx + dx, ny = cy + dy;
                    if (nx > 0 && nx < w - 1 && ny > 0 && ny < MH - 1 && m.grid_[ny][nx] == 0) {
                        m.floor_h_[ny][nx] = step_h;
                    }
                }
            }
        }
        // 确保出生点和出口地面高度为 0
        for (int dy = 0; dy <= 4; ++dy)
            for (int dx = 0; dx <= 4; ++dx)
                m.floor_h_[1+dy][1+dx] = 0;
        for (int dy = 0; dy <= 4; ++dy)
            for (int dx = 0; dx <= 4; ++dx)
                m.floor_h_[(MH-2)-dy][(w-2)-dx] = 0;

        // 调试：统计台阶数量
        int cnt = 0;
        for (int y = 0; y < MH; ++y)
            for (int x = 0; x < w; ++x)
                if (m.floor_h_[y][x] > 0) ++cnt;
        std::cout << "Steps generated: " << cnt << "\n";

        return m;
    }

    [[nodiscard]] bool is_wall(int x, int y) const noexcept {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return true;
        if (is_rock(x, y)) return true;
        return grid_[y][x] == 1;
    }

    [[nodiscard]] int floor_h(int x, int y) const noexcept {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return 0;
        return floor_h_[y][x];
    }

    [[nodiscard]] int width() const noexcept  { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }

private:
    void carve(int x, int y) {
        if (x >= 0 && x < width_ && y >= 0 && y < height_) grid_[y][x] = 0;
    }

    void generate_dfs(int x, int y) {
        carve(x, y);

        std::array<std::pair<int, int>, 4> dirs = {{
            {0, -2}, {0, 2}, {-2, 0}, {2, 0}
        }};
        std::shuffle(dirs.begin(), dirs.end(), std::mt19937{std::random_device{}()});

        for (auto [dx, dy] : dirs) {
            int nx = x + dx;
            int ny = y + dy;

            if (nx <= 0 || nx >= width_ - 1 || ny <= 0 || ny >= height_ - 1) continue;
            if (grid_[ny][nx] == 0) continue;

            carve(x + dx / 2, y + dy / 2);
            generate_dfs(nx, ny);
        }
    }

    int exit_x_ = 0, exit_y_ = 0;
    int width_, height_;
    std::vector<std::vector<int>> grid_;
    std::vector<std::vector<int>> floor_h_;  // 地面高度：0=基准, 1=高台, 2=更高
    std::vector<std::pair<int,int>> rocks_;
};

// ============================================================
// Module 2.5: BFS Pathfinding
// ============================================================
[[nodiscard]] std::optional<std::vector<std::pair<int,int>>> find_path(
    const Map& map,
    std::pair<int,int> from,
    std::pair<int,int> to
) {
    if (from == to) return std::vector<std::pair<int,int>>{from};

    int w = map.width();
    int h = map.height();
    std::vector<std::vector<bool>> vis(h, std::vector<bool>(w, false));
    std::vector<std::vector<std::pair<int,int>>> parent(
        h, std::vector<std::pair<int,int>>(w, {-1,-1})
    );

    std::queue<std::pair<int,int>> q;
    q.push(from);
    vis[from.second][from.first] = true;

    const std::array<std::pair<int,int>, 4> dirs = {{{0,1},{1,0},{0,-1},{-1,0}}};

    while (!q.empty()) {
        auto [cx, cy] = q.front(); q.pop();
        if (cx == to.first && cy == to.second) break;

        for (auto [dx, dy] : dirs) {
            int nx = cx + dx;
            int ny = cy + dy;
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            if (vis[ny][nx] || map.is_wall(nx, ny)) continue;
            vis[ny][nx] = true;
            parent[ny][nx] = {cx, cy};
            q.push({nx, ny});
        }
    }

    if (!vis[to.second][to.first]) return std::nullopt;

    std::vector<std::pair<int,int>> path;
    auto cur = to;
    while (true) {
        path.push_back(cur);
        if (cur == from) break;
        cur = parent[cur.second][cur.first];
    }
    std::reverse(path.begin(), path.end());
    return path;
}

// ============================================================
// Module 3: Player
// ============================================================
struct Player {
    Vec2 pos{1.5, 1.5};
    Vec2 dir{1.0, 0.0};
    Vec2 plane{0.0, 0.66};
    double pitch = 0.0;  // -1.0 = 完全看地, +1.0 = 完全看天, 0 = 平视
    double recoil_pitch = 0.0;     // 后坐力造成的临时视角抬头
    double jump_offset = 0.0;      // 跳跃时的临时垂直偏移
    double jump_vel = 0.0;         // 跳跃初速度
    bool is_crouching = false;     // 是否下蹲
    double z = 0.0;  // 垂直高度（台阶系统）

    void rotate(double angle) {
        double old_dir_x = dir.x;
        dir.x = dir.x * std::cos(angle) - dir.y * std::sin(angle);
        dir.y = old_dir_x * std::sin(angle) + dir.y * std::cos(angle);

        double old_plane_x = plane.x;
        plane.x = plane.x * std::cos(angle) - plane.y * std::sin(angle);
        plane.y = old_plane_x * std::sin(angle) + plane.y * std::cos(angle);
    }

    void move(double dist, const Map& map) {
        double nx = pos.x + dir.x * dist;
        double ny = pos.y + dir.y * dist;
        int cx = static_cast<int>(pos.x);
        int cy = static_cast<int>(pos.y);
        double ch = map.floor_h(cx, cy);

        if (!map.is_wall(static_cast<int>(nx), static_cast<int>(pos.y))) {
            double nh = map.floor_h(static_cast<int>(nx), static_cast<int>(pos.y));
            if (nh <= ch + 1.0 || jump_offset > 0.02) pos.x = nx;  // 平地或跳跃时允许
        }
        if (!map.is_wall(static_cast<int>(pos.x), static_cast<int>(ny))) {
            double nh = map.floor_h(static_cast<int>(pos.x), static_cast<int>(ny));
            if (nh <= ch + 1.0 || jump_offset > 0.02) pos.y = ny;
        }
    }

    void strafe(double dist, const Map& map) {
        double nx = pos.x - dir.y * dist;
        double ny = pos.y + dir.x * dist;
        int cx = static_cast<int>(pos.x);
        int cy = static_cast<int>(pos.y);
        double ch = map.floor_h(cx, cy);

        if (!map.is_wall(static_cast<int>(nx), static_cast<int>(pos.y))) {
            double nh = map.floor_h(static_cast<int>(nx), static_cast<int>(pos.y));
            if (nh <= ch + 1.0 || jump_offset > 0.02) pos.x = nx;
        }
        if (!map.is_wall(static_cast<int>(pos.x), static_cast<int>(ny))) {
            double nh = map.floor_h(static_cast<int>(pos.x), static_cast<int>(ny));
            if (nh <= ch + 1.0 || jump_offset > 0.02) pos.y = ny;
        }
    }
};

class Raycaster;  // 前置声明

// ============================================================
// Module 3.5: Enemy
// ============================================================
struct Enemy {
    Vec2 pos{17.5, 17.5};
    char symbol = 'E';
    double speed = 0.04;
    int hp = 5;

    // AI 视野与状态（新增）
    double dir_x = 0.0;
    double dir_y = 1.0;
    enum class State { PATROL, CHASE, SEARCH };
    State state = State::PATROL;
    double view_dist = 10.0;
    double view_angle = 1.22;
    int alert_timer = 0;
    Vec2 last_seen{0,0};
    int patrol_timer = 0;
    double patrol_dir_x = 0.0;
    double patrol_dir_y = 0.0;

    void take_hit() { if (hp > 0) --hp; }
    void update(const Map& map, const Vec2& player_pos, const Raycaster& caster);
};

struct HealthPack {
    Vec2 pos;
    bool active = true;
};

struct AmmoPack {
    Vec2 pos;
    bool active = true;
};

// ============================================================
// Module 3.75: Weapon System
// ============================================================
enum class WeaponType { Pistol, Shotgun, Rifle };

struct Weapon {
    WeaponType type;
    std::string name;
    int damage;          // 单发伤害
    int max_ammo;        // 弹匣容量
    int current_ammo;    // 当前弹匣
    int reserve_ammo;    // 备用（手枪=999无限）
    int fire_rate;       // 射击冷却帧数
    int reload_time;     // 换弹帧数
    double spread;       // 扩散角度（弧度）
    int pellets;         // 弹丸数（霰弹枪用）
    uint32_t color;      // 枪身主色
};

// ============================================================
// Module 3.8: Audio Synthesizer (程序化音效，零外部文件)
// ============================================================
class AudioSynth {
public:
    AudioSynth() {
        if (!SDL_WasInit(SDL_INIT_AUDIO)) SDL_Init(SDL_INIT_AUDIO);
        
        SDL_AudioSpec spec;
        spec.freq = 44100;
        spec.format = SDL_AUDIO_F32LE;
        spec.channels = 1;
        stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (stream_) {
            SDL_AudioDeviceID dev = SDL_GetAudioStreamDevice(stream_);
            SDL_ResumeAudioDevice(dev);
        }
    }

    ~AudioSynth() {
        if (stream_) SDL_DestroyAudioStream(stream_);
    }

    void play_shoot(WeaponType type) {
        if (!stream_) return;
        if (type == WeaponType::Pistol)      play_noise(0.15, 25.0, 0.6);
        else if (type == WeaponType::Rifle)    play_noise(0.05, 50.0, 0.45);
        else if (type == WeaponType::Shotgun)  play_noise(0.18, 10.0, 0.85);
    }

    void play_reload() {
        if (!stream_) return;
        int gap = static_cast<int>(0.08 * sample_rate_);
        std::vector<float> buf;
        buf.reserve(gap + 8000);
        append_click(buf, 400.0, 0.04, 0.3);
        buf.insert(buf.end(), gap, 0.0f);
        append_click(buf, 600.0, 0.03, 0.25);
        push_samples(buf);
    }

    void play_hurt()      { if (stream_) play_sawtooth(70.0, 0.3, 6.0, 0.6); }
    void play_pickup()    { if (stream_) play_sweep(880.0, 1320.0, 0.12, 0.35); }
    void play_empty()     { if (stream_) play_click(120.0, 0.05, 0.3); }
    void play_enemy_hit() { if (stream_) play_noise(0.05, 25.0, 0.3); }

private:
    SDL_AudioStream* stream_ = nullptr;
    static constexpr int sample_rate_ = 44100;

    void push_samples(const std::vector<float>& buf) {
        if (!stream_ || buf.empty()) return;
        SDL_PutAudioStreamData(stream_, buf.data(), static_cast<int>(buf.size() * sizeof(float)));
    }

    void append_click(std::vector<float>& buf, double freq, double duration, double amplitude) {
        int samples = static_cast<int>(duration * sample_rate_);
        int period = static_cast<int>(sample_rate_ / freq);
        for (int i = 0; i < samples; ++i) {
            double t = i / static_cast<double>(sample_rate_);
            float env = static_cast<float>(amplitude * std::exp(-t * 60.0));
            buf.push_back(((i % period) < period / 2) ? env : -env);
        }
    }

    void play_click(double freq, double duration, double amplitude) {
        std::vector<float> buf;
        append_click(buf, freq, duration, amplitude);
        push_samples(buf);
    }

    void play_noise(double duration, double decay, double amplitude) {
        int samples = static_cast<int>(duration * sample_rate_);
        std::vector<float> buf;
        buf.reserve(samples);
        std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (int i = 0; i < samples; ++i) {
            double t = i / static_cast<double>(sample_rate_);
            float env = static_cast<float>(amplitude * std::exp(-t * decay));
            buf.push_back(dist(rng) * env * 2.5f);  // ← 音量放大 2.5 倍
        }
        push_samples(buf);
    }

    void play_sawtooth(double freq, double duration, double decay, double amplitude) {
        int samples = static_cast<int>(duration * sample_rate_);
        int period = static_cast<int>(sample_rate_ / freq);
        std::vector<float> buf;
        buf.reserve(samples);
        for (int i = 0; i < samples; ++i) {
            double t = i / static_cast<double>(sample_rate_);
            float env = static_cast<float>(amplitude * std::exp(-t * decay));
            float pos = (i % period) / static_cast<float>(period);
            buf.push_back((pos * 2.0f - 1.0f) * env);
        }
        push_samples(buf);
    }

    void play_sweep(double f0, double f1, double duration, double amplitude) {
        int samples = static_cast<int>(duration * sample_rate_);
        std::vector<float> buf;
        buf.reserve(samples);
        double phase = 0.0;
        for (int i = 0; i < samples; ++i) {
            double t = i / static_cast<double>(sample_rate_);
            double freq = f0 + (f1 - f0) * (t / duration);
            phase += freq / sample_rate_;
            if (phase > 1.0) phase -= 1.0;
            float env = static_cast<float>(amplitude * std::exp(-t * 12.0));
            buf.push_back((phase < 0.5) ? env : -env);
        }
        push_samples(buf);
    }
};

// ============================================================
// Module 4: Raycaster
// ============================================================
struct HitResult {
    double perp_dist;
    int side;
    double wall_x;
    bool is_rock = false;   // ← 新增
    int floor_h = 0;   // 击中格子的地面高度
};

class Raycaster {
public:
    [[nodiscard]] HitResult cast(const Vec2& pos, const Vec2& ray_dir, const Map& map) const {
        int map_x = static_cast<int>(pos.x);
        int map_y = static_cast<int>(pos.y);

        double delta_dist_x = (ray_dir.x == 0.0) ? 1e30 : std::abs(1.0 / ray_dir.x);
        double delta_dist_y = (ray_dir.y == 0.0) ? 1e30 : std::abs(1.0 / ray_dir.y);

        double side_dist_x, side_dist_y;
        int step_x, step_y;

        if (ray_dir.x < 0) {
            step_x = -1;
            side_dist_x = (pos.x - map_x) * delta_dist_x;
        } else {
            step_x = 1;
            side_dist_x = (map_x + 1.0 - pos.x) * delta_dist_x;
        }

        if (ray_dir.y < 0) {
            step_y = -1;
            side_dist_y = (pos.y - map_y) * delta_dist_y;
        } else {
            step_y = 1;
            side_dist_y = (map_y + 1.0 - pos.y) * delta_dist_y;
        }

        bool hit = false;
        int side = 0;
        while (!hit) {
            if (side_dist_x < side_dist_y) {
                side_dist_x += delta_dist_x;
                map_x += step_x;
                side = 0;
            } else {
                side_dist_y += delta_dist_y;
                map_y += step_y;
                side = 1;
            }
            if (map.is_wall(map_x, map_y)) hit = true;
        }

        double perp_dist;
        if (side == 0)
            perp_dist = (map_x - pos.x + (1 - step_x) / 2.0) / ray_dir.x;
        else
            perp_dist = (map_y - pos.y + (1 - step_y) / 2.0) / ray_dir.y;

        double wall_x;
        if (side == 0)
            wall_x = pos.y + perp_dist * ray_dir.y;
        else
            wall_x = pos.x + perp_dist * ray_dir.x;
        wall_x -= std::floor(wall_x);

        return { std::abs(perp_dist), side, wall_x, map.is_rock(map_x, map_y), map.floor_h(map_x, map_y) };
    }
};

// ============================================================
// Module 3.5b: Enemy AI Implementation
// ============================================================
void Enemy::update(const Map& map, const Vec2& player_pos, const Raycaster& caster) {
    if (hp <= 0) return;

    double pdx = player_pos.x - pos.x;
    double pdy = player_pos.y - pos.y;
    double dist_to_player = std::sqrt(pdx*pdx + pdy*pdy);

    // 根据类型调整视野
    double vd = view_dist;
    double va = view_angle;
    if (symbol == 'N') { vd = 14.0; va = 0.7; }       // 狙击手：远、窄
    else if (symbol == 'F') { vd = 8.0; va = 1.57; }   // 极速：近、宽（90度）
    else if (symbol == 'T') { vd = 6.0; va = 0.8; }    // 坦克：近、窄
    else if (symbol == 'G') { vd = 12.0; va = 1.57; }  // 幽灵：中、宽
    else if (symbol == 'M') { vd = 10.0; va = 1.4; }   // 快速：中、较宽
    else if (symbol == 'S') { vd = 8.0; va = 1.0; }    // 慢速：近、中等

    // 视野检测
    bool can_see = false;
    if (dist_to_player < 3.0) {
        can_see = true; // 近距离听觉感知
    } else if (dist_to_player <= vd) {
        double ndx = pdx / dist_to_player;
        double ndy = pdy / dist_to_player;
        double dot = dir_x * ndx + dir_y * ndy;
        if (dot > std::cos(va / 2.0)) {
            Vec2 ray_dir{ndx, ndy};
            auto hit = caster.cast(pos, ray_dir, map);
            if (hit.perp_dist > dist_to_player - 0.5) can_see = true;
        }
    }
    if (symbol == 'G' && dist_to_player <= vd) can_see = true; // 幽灵穿墙感知

    // 状态机转换
    if (can_see) {
        state = State::CHASE;
        alert_timer = 90; // 约1.5秒记忆
        last_seen = player_pos;
        dir_x = pdx / dist_to_player;
        dir_y = pdy / dist_to_player;
    } else if (state == State::CHASE) {
        state = State::SEARCH;
        alert_timer = 90;
    }

    // 行为执行
    double current_speed = speed;
    if (dist_to_player < 3.0 && state == State::CHASE) current_speed *= 1.5;

    if (state == State::PATROL) {
        --patrol_timer;
        if (patrol_timer <= 0) {
            std::mt19937 rng{std::random_device{}()};
            std::array<std::pair<int,int>, 4> dirs = {{{1,0},{-1,0},{0,1},{0,-1}}};
            std::shuffle(dirs.begin(), dirs.end(), rng);
            for (auto [pdx_, pdy_] : dirs) {
                int nx = static_cast<int>(pos.x) + pdx_;
                int ny = static_cast<int>(pos.y) + pdy_;
                if (!map.is_wall(nx, ny)) {
                    patrol_dir_x = pdx_;
                    patrol_dir_y = pdy_;
                    dir_x = pdx_;
                    dir_y = pdy_;
                    break;
                }
            }
            patrol_timer = 60 + (rng() % 60); // 1~2秒
        }
        double nx = pos.x + patrol_dir_x * current_speed;
        double ny = pos.y + patrol_dir_y * current_speed;
        if (!map.is_wall(static_cast<int>(nx), static_cast<int>(pos.y))) pos.x = nx;
        else { patrol_dir_x = 0; }
        if (!map.is_wall(static_cast<int>(pos.x), static_cast<int>(ny))) pos.y = ny;
        else { patrol_dir_y = 0; }
        if (patrol_dir_x == 0 && patrol_dir_y == 0) patrol_timer = 0;
        return;
    }

    // CHASE / SEARCH：BFS 追踪
    Vec2 target = (state == State::CHASE) ? player_pos : last_seen;
    auto path = find_path(map,
        {static_cast<int>(pos.x), static_cast<int>(pos.y)},
        {static_cast<int>(target.x), static_cast<int>(target.y)}
    );
    if (!path || path->size() <= 1) {
        if (state == State::SEARCH) state = State::PATROL;
        return;
    }
    auto [tx, ty] = (*path)[1];
    double target_x = tx + 0.5;
    double target_y = ty + 0.5;
    double dx = target_x - pos.x;
    double dy = target_y - pos.y;
    double dist = std::sqrt(dx*dx + dy*dy);
    if (dist > current_speed) {
        pos.x += (dx / dist) * current_speed;
        pos.y += (dy / dist) * current_speed;
    } else {
        pos.x = target_x;
        pos.y = target_y;
    }

    if (state == State::SEARCH) {
        --alert_timer;
        if (alert_timer <= 0) state = State::PATROL;
    }
}

// ============================================================
// Module 5: Renderer
// ============================================================
class Renderer {
public:
    explicit Renderer(int w, int h) : width_(w), height_(h) {
        SDL_Init(SDL_INIT_VIDEO);
        win_ = SDL_CreateWindow("FPS Raycasting", w, h, SDL_WINDOW_RESIZABLE);
        SDL_SetWindowRelativeMouseMode(win_, true);  // ← 新增：隐藏光标，捕获相对位移
        ren_ = SDL_CreateRenderer(win_, nullptr);
        tex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, w, h);
        pixels_.resize(w * h, 0xFF000000);
    }

    ~Renderer() {
        SDL_DestroyTexture(tex_);
        SDL_DestroyRenderer(ren_);
        SDL_DestroyWindow(win_);
        SDL_Quit();
    }

    void set_mouse_capture(bool capture) {
        SDL_SetWindowRelativeMouseMode(win_, capture);
    }

    void present() {
        SDL_UpdateTexture(tex_, nullptr, pixels_.data(), width_ * sizeof(uint32_t));
        SDL_RenderClear(ren_);
        SDL_RenderTexture(ren_, tex_, nullptr, nullptr);
        SDL_RenderPresent(ren_);
    }

    void render(const Map& map, const Player& player, const Raycaster& caster,
                const std::vector<Enemy>& enemies,
                const std::vector<HealthPack>& packs,
                const std::vector<AmmoPack>& ammo_packs,
                int player_hp, int ammo, int hit_flash = 0) {

        // 清空：天花板 + 地板（地平线跟随 pitch）
        std::fill(pixels_.begin(), pixels_.end(), 0xFF000000);
        int pitch_offset = static_cast<int>((player.pitch + player.recoil_pitch) * height_ * 0.5);
        int jump_offset  = static_cast<int>(player.jump_offset * height_ * 0.3);
        int crouch_offset = player.is_crouching ? static_cast<int>(height_ * 0.15) : 0;
        int z_offset = static_cast<int>(player.z * height_ * 0.25);
        int horizon = height_ / 2 + pitch_offset - jump_offset + crouch_offset + z_offset;
        if (horizon < 0) horizon = 0;
        if (horizon > height_) horizon = height_;

        for (int y = 0; y < horizon; ++y)
            for (int x = 0; x < width_; ++x)
                pixels_[y * width_ + x] = 0xFF1a1a2e; // 深蓝天花板
        uint32_t floor_color = (player.z > 0.5) ? 0xFF2a4a3a : 0xFF16213e;  // 高台=偏绿，平地=深蓝
        for (int y = horizon; y < height_; ++y)
            for (int x = 0; x < width_; ++x)
                pixels_[y * width_ + x] = floor_color;

        // 1. 射线投射画墙
        std::vector<double> z_buffer(width_, 1e30);
        for (int x = 0; x < width_; ++x) {
            double camera_x = 2.0 * x / width_ - 1.0;
            Vec2 ray_dir{
                player.dir.x + player.plane.x * camera_x,
                player.dir.y + player.plane.y * camera_x
            };
            auto hit = caster.cast(player.pos, ray_dir, map);
            z_buffer[x] = hit.perp_dist;

            int line_height = static_cast<int>(height_ / hit.perp_dist);
            if (line_height < 0) line_height = 0;
            if (line_height > height_) line_height = height_;

            int pitch_offset = static_cast<int>((player.pitch + player.recoil_pitch) * height_ * 0.5);
            int jump_offset = static_cast<int>(player.jump_offset * height_ * 0.3);
            int crouch_offset = player.is_crouching ? static_cast<int>(height_ * 0.15) : 0;
            int h_offset = static_cast<int>((hit.floor_h - player.z) * height_ * 0.25);  // 0.15→0.25，落差更明显
            int draw_start = (height_ - line_height) / 2 + pitch_offset - jump_offset + crouch_offset + h_offset;
            int draw_end = draw_start + line_height;
            if (draw_start < 0) draw_start = 0;
            if (draw_end >= height_) draw_end = height_ - 1;

            uint32_t color = wall_color(hit);
            for (int y = draw_start; y <= draw_end; ++y)
                pixels_[y * width_ + x] = color;
        }

        // 2. 画敌人 Sprite（红色方块 + 白边框）
        for (const auto& enemy : enemies) {
            if (enemy.hp <= 0) continue;
            double dx = enemy.pos.x - player.pos.x;
            double dy = enemy.pos.y - player.pos.y;
            double inv_det = 1.0 / (player.plane.x * player.dir.y - player.dir.x * player.plane.y);
            double transform_x = inv_det * (player.dir.y * dx - player.dir.x * dy);
            double transform_y = inv_det * (-player.plane.y * dx + player.plane.x * dy);
            if (transform_y <= 0.05) continue;

            int screen_x = static_cast<int>((width_ / 2.0) * (1.0 + transform_x / transform_y));
            int sprite_h = static_cast<int>(height_ / transform_y);
            if (sprite_h < 3) sprite_h = 3;
            int sprite_w = std::max(3, sprite_h / 2);
            int pitch_offset = static_cast<int>((player.pitch + player.recoil_pitch) * height_ * 0.5);
            int jump_offset  = static_cast<int>(player.jump_offset * height_ * 0.3);
            int crouch_offset = player.is_crouching ? static_cast<int>(height_ * 0.15) : 0;
            int ds_y = (height_ - sprite_h) / 2 + pitch_offset - jump_offset + crouch_offset;
            int de_y = ds_y + sprite_h;
            int ds_x = screen_x - sprite_w / 2;
            int de_x = ds_x + sprite_w;

            draw_humanoid(ds_x, ds_y, sprite_w, sprite_h, enemy.symbol, hit_flash, transform_y, z_buffer);
        }

        // 画独立石头障碍物（2x2 块：白边框 + 顶部高光 + 地面阴影）
        for (int y = 1; y < map.height() - 1; ++y) {
            for (int x = 1; x < map.width() - 1; ++x) {
                if (!map.is_rock(x, y)) continue;
                // 只画 2x2 块的左上角，避免同一石头重复绘制 4 次
                if (map.is_rock(x - 1, y) || map.is_rock(x, y - 1)) continue;

                double dx = (x + 1.0) - player.pos.x;
                double dy = (y + 1.0) - player.pos.y;
                double inv_det = 1.0 / (player.plane.x * player.dir.y - player.dir.x * player.plane.y);
                double transform_x = inv_det * (player.dir.y * dx - player.dir.x * dy);
                double transform_y = inv_det * (-player.plane.y * dx + player.plane.x * dy);
                if (transform_y <= 0.05) continue;

                int screen_x = static_cast<int>((width_ / 2.0) * (1.0 + transform_x / transform_y));
                // 尺寸加大：0.6 倍，最小 6 像素，保证远距离也能看到
                int sprite_h = static_cast<int>(height_ / transform_y * 0.6);
                if (sprite_h < 6) sprite_h = 6;
                int sprite_w = std::max(6, sprite_h);
                // 贴地偏移加大，确保大石头仍坐在地面上
                int pitch_offset = static_cast<int>((player.pitch + player.recoil_pitch) * height_ * 0.5);
                int jump_offset  = static_cast<int>(player.jump_offset * height_ * 0.3);
                int crouch_offset = player.is_crouching ? static_cast<int>(height_ * 0.15) : 0;
                int ds_y = (height_ - sprite_h) / 2 + static_cast<int>(height_ * 0.22) + pitch_offset - jump_offset + crouch_offset;
                int de_y = ds_y + sprite_h;
                int ds_x = screen_x - sprite_w / 2;
                int de_x = ds_x + sprite_w;

                // 地面阴影（石头底部 1 像素黑色，增强立体感）
                int shadow_y = de_y + 1;
                for (int sx = ds_x - 2; sx < de_x + 2; ++sx) {
                    if (sx < 0 || sx >= width_) continue;
                    if (shadow_y >= 0 && shadow_y < height_ && transform_y < z_buffer[sx]) {
                        pixels_[shadow_y * width_ + sx] = 0xFF222222;
                    }
                }

                for (int sx = ds_x; sx < de_x; ++sx) {
                    if (sx < 0 || sx >= width_) continue;
                    if (transform_y >= z_buffer[sx]) continue;
                    for (int yy = ds_y; yy < de_y; ++yy) {
                        if (yy < 0 || yy >= height_) continue;
                        bool border = (sx == ds_x || sx == de_x - 1 || yy == ds_y || yy == de_y - 1);
                        bool top_highlight = (yy < ds_y + std::max(2, sprite_h / 5));
                        uint32_t rock_color;
                        if (border) rock_color = 0xFFFFFFFF;          // 粗白边框
                        else if (top_highlight) rock_color = 0xFFFFCCFF; // 顶部淡紫高光
                        else rock_color = 0xFFFF00AA;                 // 主体亮紫红
                        pixels_[yy * width_ + sx] = rock_color;
                    }
                }
            }
        }

        // 画出口（绿色发光方块）
        {
            double dx = map.exit_x() + 0.5 - player.pos.x;
            double dy = map.exit_y() + 0.5 - player.pos.y;
            double inv_det = 1.0 / (player.plane.x * player.dir.y - player.dir.x * player.plane.y);
            double transform_x = inv_det * (player.dir.y * dx - player.dir.x * dy);
            double transform_y = inv_det * (-player.plane.y * dx + player.plane.x * dy);
            if (transform_y > 0.05) {
                int screen_x = static_cast<int>((width_ / 2.0) * (1.0 + transform_x / transform_y));
                int sprite_h = static_cast<int>(height_ / transform_y);
                if (sprite_h < 3) sprite_h = 3;
                int sprite_w = std::max(3, sprite_h / 2);
                int pitch_offset = static_cast<int>((player.pitch + player.recoil_pitch) * height_ * 0.5);
                int jump_offset  = static_cast<int>(player.jump_offset * height_ * 0.3);
                int crouch_offset = player.is_crouching ? static_cast<int>(height_ * 0.15) : 0;
                int ds_y = (height_ - sprite_h) / 2 + pitch_offset - jump_offset + crouch_offset;
                int de_y = ds_y + sprite_h;
                int ds_x = screen_x - sprite_w / 2;
                int de_x = ds_x + sprite_w;
                for (int sx = ds_x; sx < de_x; ++sx) {
                    if (sx < 0 || sx >= width_) continue;
                    if (transform_y >= z_buffer[sx]) continue;
                    for (int y = ds_y; y < de_y; ++y) {
                        if (y < 0 || y >= height_) continue;
                        pixels_[y * width_ + sx] = 0xFF00FF00; // 纯绿出口
                    }
                }
            }
        }

        // 3. 画小地图（右上角 80x80 像素块）
        draw_minimap(map, player, enemies, packs, ammo_packs);

        // 命中反馈：屏幕中心闪白十字
        if (hit_flash > 0) {
            int cx = width_ / 2, cy = height_ / 2;
            for (int i = -5; i <= 5; ++i) {
                if (cx + i >= 0 && cx + i < width_) pixels_[cy * width_ + cx + i] = 0xFFFFFFFF;
                if (cy + i >= 0 && cy + i < height_) pixels_[(cy + i) * width_ + cx] = 0xFFFFFFFF;
            }
        }

        // 命中反馈：屏幕边框爆闪（白色）
        if (hit_flash > 0) {
            int thickness = (hit_flash > 6) ? 4 : 2;
            for (int x = 0; x < width_; ++x) {
                for (int t = 0; t < thickness; ++t) {
                    pixels_[t * width_ + x] = 0xFFFFFFFF;
                    pixels_[(height_ - 1 - t) * width_ + x] = 0xFFFFFFFF;
                }
            }
            for (int y = 0; y < height_; ++y) {
                for (int t = 0; t < thickness; ++t) {
                    pixels_[y * width_ + t] = 0xFFFFFFFF;
                    pixels_[y * width_ + (width_ - 1 - t)] = 0xFFFFFFFF;
                }
            }
        }

        // 准星（屏幕中心十字）
        int cx = width_ / 2;
        int cy = height_ / 2;
        uint32_t cross_color = (hit_flash > 0) ? 0xFFFF0000 : 0xFFFFFFFF;  // 命中时变红
        int len = (hit_flash > 0) ? 8 : 6;  // 命中时变大
        int gap = 2;  // 中心留空，不遮挡目标
        for (int i = gap; i <= len; ++i) {
            // 上
            if (cy - i >= 0) pixels_[(cy - i) * width_ + cx] = cross_color;
            // 下
            if (cy + i < height_) pixels_[(cy + i) * width_ + cx] = cross_color;
            // 左
            if (cx - i >= 0) pixels_[cy * width_ + (cx - i)] = cross_color;
            // 右
            if (cx + i < width_) pixels_[cy * width_ + (cx + i)] = cross_color;
        }
    }

    void draw_pause_menu(int choice) {
        // 半透明黑色遮罩（和结算画面同风格）
        for (auto& p : pixels_) {
            uint8_t r = (p >> 16) & 0xFF;
            uint8_t g = (p >> 8) & 0xFF;
            uint8_t b = p & 0xFF;
            p = 0xFF000000 | ((r >> 1) << 16) | ((g >> 1) << 8) | (b >> 1);
        }

        std::string title = "PAUSED";
        int tw = static_cast<int>(title.length()) * 6;
        draw_text(title, (width_ - tw) / 2, height_ / 2 - 40, 0xFFFFFFFF);

        uint32_t c_col = (choice == 0) ? 0xFF00FF00 : 0xFF888888;
        std::string c = (choice == 0) ? "> CONTINUE <" : "  CONTINUE  ";
        tw = static_cast<int>(c.length()) * 6;
        draw_text(c, (width_ - tw) / 2, height_ / 2 + 10, c_col);

        uint32_t e_col = (choice == 1) ? 0xFFFF0000 : 0xFF888888;
        std::string ex = (choice == 1) ? "> EXIT <" : "  EXIT  ";
        tw = static_cast<int>(ex.length()) * 6;
        draw_text(ex, (width_ - tw) / 2, height_ / 2 + 30, e_col);
    }

    void draw_end_screen(bool won, int score, int time_sec, int choice = 0) {
        // 半透明黑色遮罩
        for (auto& p : pixels_) {
            uint8_t r = (p >> 16) & 0xFF;
            uint8_t g = (p >> 8) & 0xFF;
            uint8_t b = p & 0xFF;
            p = 0xFF000000 | ((r >> 1) << 16) | ((g >> 1) << 8) | (b >> 1);
        }

        std::string title = won ? "YOU ESCAPED" : "GAME OVER";
        int tw = static_cast<int>(title.length()) * 6;
        draw_text(title, (width_ - tw) / 2, height_ / 2 - 60, 0xFFFFFFFF);

        std::string s = "SCORE:" + std::to_string(score);
        tw = static_cast<int>(s.length()) * 6;
        draw_text(s, (width_ - tw) / 2, height_ / 2 - 30, 0xFFFFFFFF);

        std::string t = "TIME:" + std::to_string(time_sec) + "S";
        tw = static_cast<int>(t.length()) * 6;
        draw_text(t, (width_ - tw) / 2, height_ / 2 - 10, 0xFFFFFFFF);

        // CONTINUE 选项
        uint32_t c_col = (choice == 0) ? 0xFF00FF00 : 0xFF888888;
        std::string c = (choice == 0) ? "> CONTINUE <" : "  CONTINUE  ";
        tw = static_cast<int>(c.length()) * 6;
        draw_text(c, (width_ - tw) / 2, height_ / 2 + 20, c_col);

        // EXIT 选项
        uint32_t e_col = (choice == 1) ? 0xFFFF0000 : 0xFF888888;
        std::string ex = (choice == 1) ? "> EXIT <" : "  EXIT  ";
        tw = static_cast<int>(ex.length()) * 6;
        draw_text(ex, (width_ - tw) / 2, height_ / 2 + 40, e_col);
    }

    void draw_weapon(const Weapon& weapon, int recoil, int muzzle_flash, int reload_progress) {
        // 位置：屏幕右侧偏下（右手持枪视角）
        int base_x = width_ * 0.72;
        int base_y = height_ - 40;

        // 动画偏移
        int recoil_y = -recoil * 3;                    // 射击上跳更明显
        int reload_y = (reload_progress > 0) ? 40 : 0;  // 换弹下沉更深
        int idle = static_cast<int>(std::sin(SDL_GetTicks() / 300.0) * 3);  // 呼吸晃动
        int ay = base_y + recoil_y + reload_y + idle;

        if (weapon.type == WeaponType::Pistol) {
            int ax = base_x + 30;
            draw_rect(ax + 6, ay - 30, 10, 28, 0xFF444444);   // 枪管（更长更粗）
            draw_rect(ax, ay - 4, 22, 16, 0xFF666666);        // 枪身
            draw_rect(ax + 3, ay + 12, 16, 22, 0xFF8B4513);   // 握把
            draw_rect(ax + 2, ay - 4, 5, 10, 0xFF333333);      // 照门
            if (muzzle_flash > 0) {
                draw_rect(ax + 6, ay - 42, 10, 16, 0xFFFFFF00);
                draw_rect(ax + 3, ay - 48, 16, 10, 0xFFFFAA00);
            }
        } 
        else if (weapon.type == WeaponType::Shotgun) {
            int ax = base_x;
            draw_rect(ax + 10, ay - 60, 16, 52, 0xFF333333);   // 长枪管
            draw_rect(ax + 6, ay - 8, 24, 28, 0xFF444444);      // 枪身
            draw_rect(ax + 12, ay + 20, 16, 32, 0xFF8B4513);  // 握把
            draw_rect(ax + 4, ay - 8, 6, 14, 0xFF222222);       // 泵动
            if (muzzle_flash > 0) {
                draw_rect(ax + 10, ay - 76, 16, 20, 0xFFFFFF00);
                draw_rect(ax + 4, ay - 86, 28, 12, 0xFFFFAA00);
            }
        } 
        else if (weapon.type == WeaponType::Rifle) {
            int ax = base_x + 15;
            draw_rect(ax + 10, ay - 46, 12, 38, 0xFF556B2F);   // 枪管+护木
            draw_rect(ax + 6, ay - 8, 20, 22, 0xFF6B8E23);     // 机匣
            draw_rect(ax + 12, ay + 14, 10, 26, 0xFF222222);   // 弹匣
            draw_rect(ax + 10, ay + 14, 16, 22, 0xFF8B4513);   // 握把
            draw_rect(ax + 3, ay - 12, 6, 10, 0xFF333333);     // 瞄具
            if (muzzle_flash > 0) {
                draw_rect(ax + 10, ay - 58, 12, 16, 0xFFFFFF00);
                draw_rect(ax + 5, ay - 68, 22, 12, 0xFFFFAA00);
            }
        }

        // 换弹提示
        if (reload_progress > 0 && (reload_progress % 4 < 2)) {
            int ax = (weapon.type == WeaponType::Pistol) ? base_x + 35 :
                     (weapon.type == WeaponType::Shotgun) ? base_x + 16 : base_x + 22;
            draw_rect(ax, base_y + 30, 12, 6, 0xFFFFFF00);
        }
    }

private:
    uint32_t wall_color(const HitResult& hit) const {
        uint32_t base;
        if (hit.is_rock) {
            base = 0xFFFF00AA;                 // 石头：亮紫红
        } else {
            base = (hit.wall_x < 0.5) ? 0xFF888888 : 0xFFAAAAAA;
        }

        if (hit.side == 1) base = (base & 0xFF000000) | ((base & 0x00FEFEFE) >> 1);

        if (hit.perp_dist > 3.0) {
            uint8_t r = (base >> 16) & 0xFF;
            uint8_t g = (base >> 8) & 0xFF;
            uint8_t b = base & 0xFF;
            double fog = std::min(1.0, (hit.perp_dist - 3.0) / 12.0);
            r = static_cast<uint8_t>(r * (1.0 - fog));
            g = static_cast<uint8_t>(g * (1.0 - fog));
            b = static_cast<uint8_t>(b * (1.0 - fog));
            base = 0xFF000000 | (r << 16) | (g << 8) | b;
        }

        // 高度标识：底部 2 像素根据 floor_h 变色
        if (hit.floor_h == 1) base = (base & 0xFF000000) | 0x00666600;  // 高度1=底部泛绿
        else if (hit.floor_h == 2) base = (base & 0xFF000000) | 0x00666666; // 高度2=底部泛灰

        return base;
    }

    void draw_rect(int x, int y, int w, int h, uint32_t color) {
        for (int yy = y; yy < y + h; ++yy)
            for (int xx = x; xx < x + w; ++xx)
                if (xx >= 0 && xx < width_ && yy >= 0 && yy < height_)
                    pixels_[yy * width_ + xx] = color;
    }

    void draw_humanoid(int ds_x, int ds_y, int sprite_w, int sprite_h,
                       char symbol, int hit_flash, double transform_y,
                       const std::vector<double>& z_buffer) {
        uint32_t c_head, c_body, c_legs;
        double h_scale = 1.0, w_scale = 1.0;

        switch (symbol) {
            case 'E': c_head = 0xFFFF6666; c_body = 0xFFFF0000; c_legs = 0xFFCC0000; break;
            case 'M': c_head = 0xFFFFD700; c_body = 0xFFFFA500; c_legs = 0xFFCC8400; h_scale = 1.15; w_scale = 0.85; break;
            case 'S': c_head = 0xFFB22222; c_body = 0xFF8B0000; c_legs = 0xFF660000; h_scale = 0.85; w_scale = 1.15; break;
            case 'F': c_head = 0xFFFFB347; c_body = 0xFFFF8C00; c_legs = 0xFFCC7000; h_scale = 0.75; w_scale = 0.75; break;
            case 'T': c_head = 0xFFA0522D; c_body = 0xFF8B4513; c_legs = 0xFF654321; h_scale = 0.95; w_scale = 1.35; break;
            case 'G': c_head = 0xFF88FFFF; c_body = 0xFF00FFFF; c_legs = 0xFF00CCCC; break;
            case 'N': c_head = 0xFFFF88FF; c_body = 0xFFFF00FF; c_legs = 0xFFCC00CC; h_scale = 1.25; w_scale = 0.75; break;
            default:  c_head = 0xFFFF6666; c_body = 0xFFFF0000; c_legs = 0xFFCC0000; break;
        }

        if (hit_flash > 0) { c_head = c_body = c_legs = 0xFFFFFFFF; }

        int eff_w = std::max(4, static_cast<int>(sprite_w * w_scale));
        int eff_h = std::max(6, static_cast<int>(sprite_h * h_scale));
        int off_x = (sprite_w - eff_w) / 2;
        int off_y = (sprite_h - eff_h) / 2;
        int bx = ds_x + off_x;
        int by = ds_y + off_y;

        // 头部 (顶部 ~25%)
        int hh = std::max(2, eff_h / 4);
        int hw = std::max(2, eff_w * 2 / 5);
        int hx = bx + (eff_w - hw) / 2;
        fill_rect(hx, by, hw, hh, c_head, transform_y, z_buffer);

        // 身体 (中部 ~40%)
        int bh = std::max(2, eff_h * 2 / 5);
        int bw = std::max(3, eff_w * 3 / 5);
        int bdx = bx + (eff_w - bw) / 2;
        int by_body = by + hh;
        fill_rect(bdx, by_body, bw, bh, c_body, transform_y, z_buffer);

        // 腿部 (底部 ~35%，左右分开)
        int lh = std::max(2, eff_h - hh - bh);
        int lw = std::max(1, bw / 3);
        int gap = std::max(1, bw / 5);
        int ly = by + hh + bh;
        int lx_left = bdx + (bw - gap) / 2 - lw;
        int lx_right = bdx + (bw + gap) / 2;
        fill_rect(lx_left, ly, lw, lh, c_legs, transform_y, z_buffer);
        fill_rect(lx_right, ly, lw, lh, c_legs, transform_y, z_buffer);
    }

    void fill_rect(int x, int y, int w, int h, uint32_t color, double transform_y, const std::vector<double>& z_buffer) {
        for (int sx = x; sx < x + w; ++sx) {
            if (sx < 0 || sx >= width_) continue;
            if (transform_y >= z_buffer[sx]) continue;
            for (int yy = y; yy < y + h; ++yy) {
                if (yy < 0 || yy >= height_) continue;
                pixels_[yy * width_ + sx] = color;
            }
        }
    }

    void draw_minimap(const Map& map, const Player& player,
                      const std::vector<Enemy>& enemies,
                      const std::vector<HealthPack>& packs,
                      const std::vector<AmmoPack>& ammo_packs) {
        int size = 8, cell = 10;
        int px = static_cast<int>(player.pos.x);
        int py = static_cast<int>(player.pos.y);
        int half = size / 2;
        int off_x = width_ - size * cell - 10;
        int off_y = 10;

        for (int dy = -half; dy < half; ++dy) {
            for (int dx = -half; dx < half; ++dx) {
                int mx = px + dx, my = py + dy;
                uint32_t col = 0xFF222222;

                bool drawn = false;
                if (mx == px && my == py) { col = 0xFF00FFFF; drawn = true; }
                else for (const auto& e : enemies)
                    if (!drawn && e.hp > 0 && mx == static_cast<int>(e.pos.x) && my == static_cast<int>(e.pos.y)) {
                        if (e.symbol == 'T') col = 0xFF8B4513;      // 坦克：棕色
                        else if (e.symbol == 'F') col = 0xFFFFA500;  // 极速：橙色
                        else if (e.symbol == 'G') col = 0xFF00FFFF; // 幽灵：青色
                        else if (e.symbol == 'N') col = 0xFFFF00FF; // 狙击手：紫色
                        else col = 0xFFFF0000;                       // 普通：红色
                        drawn = true; break;
                    }
                if (!drawn) for (const auto& ap : ammo_packs)
                    if (ap.active && mx == static_cast<int>(ap.pos.x) && my == static_cast<int>(ap.pos.y))
                        { col = 0xFFFFFF00; drawn = true; break; }
                if (!drawn) for (const auto& hp : packs)
                    if (hp.active && mx == static_cast<int>(hp.pos.x) && my == static_cast<int>(hp.pos.y))
                        { col = 0xFF00FF00; drawn = true; break; }

                if (!drawn && mx == map.exit_x() && my == map.exit_y()) {
                    col = 0xFF00FF00;  // 出口绿色
                    drawn = true;
                }

                if (!drawn && map.is_rock(mx, my)) {
                    col = 0xFFFF00AA;  // 亮紫红石头（与 3D 同步）
                    drawn = true;
                }
                
                if (!drawn && map.is_wall(mx, my)) col = 0xFFFFFFFF;

                if (!drawn) {
                    int fh = map.floor_h(mx, my);
                    if (fh == 1) { col = 0xFF666666; drawn = true; }      // 小台阶：深灰
                    else if (fh == 2) { col = 0xFFAAAAAA; drawn = true; } // 高台：浅灰
                }

                int sx = off_x + (dx + half) * cell;
                int sy = off_y + (dy + half) * cell;
                for (int yy = sy; yy < sy + cell; ++yy)
                    for (int xx = sx; xx < sx + cell; ++xx)
                        if (xx >= 0 && xx < width_ && yy >= 0 && yy < height_)
                            pixels_[yy * width_ + xx] = col;
            }
        }

        // 玩家方向指示（画在玩家格子中心，白色粗箭头，跟随朝向）
        int pcx = off_x + half * cell + cell / 2;
        int pcy = off_y + half * cell + cell / 2;
        double pang = std::atan2(player.dir.y, player.dir.x);

        // 画粗线段（长度 10，宽度 2x2）
        for (int r = 2; r <= 10; ++r) {
            int ix = pcx + static_cast<int>(std::cos(pang) * r);
            int iy = pcy + static_cast<int>(std::sin(pang) * r);
            // 2x2 粗点，用 k 枚举 4 个偏移 (0,0),(1,0),(0,1),(1,1)
            for (int k = 0; k < 4; ++k) {
                int bx = ix + (k & 1);      // k=0,2 -> 0; k=1,3 -> 1
                int by = iy + (k >> 1);     // k=0,1 -> 0; k=2,3 -> 1
                if (bx >= 0 && bx < width_ && by >= 0 && by < height_)
                    pixels_[by * width_ + bx] = 0xFFFFFFFF;
            }
        }

        // 画箭头头部（大 V 形）
        int tip_x = pcx + static_cast<int>(std::cos(pang) * 10);
        int tip_y = pcy + static_cast<int>(std::sin(pang) * 10);
        int perp_x = static_cast<int>(-std::sin(pang) * 3);
        int perp_y = static_cast<int>(std::cos(pang) * 3);

        auto setp = [&](int x, int y) {
            if (x >= 0 && x < width_ && y >= 0 && y < height_)
                pixels_[y * width_ + x] = 0xFFFFFFFF;
        };
        setp(tip_x, tip_y);
        setp(tip_x + perp_x/2, tip_y + perp_y/2);
        setp(tip_x - perp_x/2, tip_y - perp_y/2);
        setp(tip_x + perp_x, tip_y + perp_y);
        setp(tip_x - perp_x, tip_y - perp_y);
    }  // ← draw_minimap 结束

    void draw_char(char c, int x, int y, uint32_t color) {
        static const std::array<std::array<uint8_t,5>, 128> font = []{
            std::array<std::array<uint8_t,5>, 128> f{};
            f['A'] = {0x7E,0x09,0x09,0x09,0x7E};
            f['C'] = {0x7E,0x41,0x41,0x41,0x7E};
            f['D'] = {0x7F,0x41,0x41,0x41,0x7E};
            f['E'] = {0x7F,0x49,0x49,0x49,0x49};
            f['G'] = {0x7E,0x41,0x49,0x49,0x7E};
            f['I'] = {0x00,0x7F,0x7F,0x7F,0x00};
            f['M'] = {0x7F,0x02,0x04,0x02,0x7F};
            f['O'] = {0x7E,0x41,0x41,0x41,0x7E};
            f['P'] = {0x7F,0x11,0x11,0x11,0x0C};
            f['R'] = {0x7F,0x11,0x15,0x19,0x0C};
            f['S'] = {0x33,0x49,0x49,0x49,0x66};
            f['T'] = {0x01,0x7F,0x7F,0x7F,0x01};
            f['U'] = {0x7E,0x41,0x41,0x41,0x7E};
            f['V'] = {0x3E,0x02,0x01,0x02,0x3E};
            f['Y'] = {0x07,0x08,0x70,0x08,0x07};
            f['0'] = {0x7E,0x41,0x49,0x41,0x7E};
            f['1'] = {0x00,0x42,0x7F,0x01,0x00};
            f['2'] = {0x77,0x49,0x49,0x49,0x61};
            f['3'] = {0x36,0x49,0x49,0x49,0x36};
            f['4'] = {0x10,0x18,0x14,0x12,0x7F};
            f['5'] = {0x76,0x49,0x49,0x49,0x71};
            f['6'] = {0x7E,0x49,0x49,0x49,0x61};
            f['7'] = {0x01,0x01,0x01,0x1F,0x07};
            f['8'] = {0x36,0x49,0x49,0x49,0x36};
            f['9'] = {0x36,0x49,0x49,0x49,0x7E};
            f[':'] = {0x00,0x24,0x24,0x00,0x00};
            f['-'] = {0x08,0x08,0x08,0x08,0x08};
            f[' '] = {0x00,0x00,0x00,0x00,0x00};
            return f;
        }();

        auto col = font[static_cast<unsigned char>(c)];
        for (int cx = 0; cx < 5; ++cx) {
            uint8_t col_data = col[cx];
            for (int row = 0; row < 7; ++row) {
                if (col_data & (1 << row)) {
                    int px = x + cx;
                    int py = y + row;
                    if (px >= 0 && px < width_ && py >= 0 && py < height_)
                        pixels_[py * width_ + px] = color;
                }
            }
        }
    }

    void draw_text(const std::string& text, int x, int y, uint32_t color) {
        int cx = x;
        for (char c : text) {
            draw_char(c, cx, y, color);
            cx += 6;
        }
    }
    
    int width_, height_;
    SDL_Window* win_ = nullptr;
    SDL_Renderer* ren_ = nullptr;
    SDL_Texture* tex_ = nullptr;
    std::vector<std::uint32_t> pixels_;
};

// ============================================================
// Module 6: Engine
// ============================================================
class Engine {
public:
    Engine() : map_(Map::create_maze(41, 41)), renderer_(1280, 720) {
        // 第一层包围（远端角落）
        enemies_.push_back({{39.5, 39.5}, 'E', 0.04, 5});
        enemies_.push_back({{39.5, 5.5},  'M', 0.06, 5});
        enemies_.push_back({{5.5, 39.5},  'S', 0.03, 5});
        enemies_.push_back({{5.5, 5.5},   'M', 0.07, 5});   // 左上，快速
        
        // 第二层包围（中场拦截）
        enemies_.push_back({{20.5, 20.5}, 'E', 0.05, 5});
        enemies_.push_back({{25.5, 15.5}, 'N', 0.04, 4});   // 狙击手，远距感知
        enemies_.push_back({{15.5, 25.5}, 'E', 0.045, 5});
        
        // 第三层包围（近身压迫）
        enemies_.push_back({{10.5, 10.5}, 'F', 0.09, 3});   // 极速怪，脆皮
        enemies_.push_back({{8.5, 8.5},   'F', 0.08, 3});   // 双极速，出生点威胁
        enemies_.push_back({{35.5, 35.5}, 'T', 0.015, 12}); // 坦克，12血，极慢
        enemies_.push_back({{30.5, 10.5}, 'G', 0.05, 6});   // 幽灵，中场游荡

        // 随机生成 4 个血包，避开墙壁和出生点
        std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist_x(1, map_.width() - 2);
        std::uniform_int_distribution<int> dist_y(1, map_.height() - 2);
        for (int i = 0; i < 2; ++i) {
            int hx, hy;
            do {
                hx = dist_x(rng);
                hy = dist_y(rng);
            } while (map_.is_wall(hx, hy) || (hx == 1 && hy == 1));
            health_packs_.push_back({{static_cast<double>(hx) + 0.5, static_cast<double>(hy) + 0.5}});
        }

        // 随机生成 3 个弹药箱
        for (int i = 0; i < 3; ++i) {
            int ax, ay;
            do {
                ax = dist_x(rng);
                ay = dist_y(rng);
            } while (map_.is_wall(ax, ay) || (ax == 1 && ay == 1));
            ammo_packs_.push_back({{static_cast<double>(ax) + 0.5, static_cast<double>(ay) + 0.5}});
            
            weapons_ = {
                {WeaponType::Pistol,  "PISTOL",  1, 12, 12, 999, 8,  30, 0.0,  1, 0xFFAAAAAA},
                {WeaponType::Shotgun, "SHOTGUN", 2, 5,  5,  30, 20, 45, 0.12, 5, 0xFF666666},
                {WeaponType::Rifle,   "RIFLE",   1, 30, 30, 90, 3,  25, 0.03, 1, 0xFF6B8E23}
            };
            current_weapon_ = 0;
            recoil_ = 0;
            muzzle_flash_ = 0;
            }
    }

        void run() {
        bool running = true;
        SDL_Event e;
        start_time_ = std::chrono::steady_clock::now();

        do {
            bool game_over = false;
            bool won = false;

            while (!game_over) {
                while (SDL_PollEvent(&e)) {
                    if (e.type == SDL_EVENT_QUIT) { game_over = true; running = false; }
                }

                const bool* keys = SDL_GetKeyboardState(nullptr);

                // ESC 防抖处理
                if (keys[SDL_SCANCODE_ESCAPE]) {
                    if (!esc_pressed_) {
                        esc_pressed_ = true;
                        if (paused_) {
                            paused_ = false;
                            pause_choice_ = 0;
                            renderer_.set_mouse_capture(true);
                        } else {
                            paused_ = true;
                            pause_choice_ = 0;
                            renderer_.set_mouse_capture(false);
                        }
                    }
                } else {
                    esc_pressed_ = false;
                }

                // 暂停菜单逻辑
                if (paused_) {
                    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
                        if (!nav_pressed_) { nav_pressed_ = true; pause_choice_ = 0; }
                    } else if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) {
                        if (!nav_pressed_) { nav_pressed_ = true; pause_choice_ = 1; }
                    } else {
                        nav_pressed_ = false;
                    }

                    if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_SPACE]) {
                        if (!enter_pressed_) {
                            enter_pressed_ = true;
                            if (pause_choice_ == 0) {
                                paused_ = false;
                                renderer_.set_mouse_capture(true);
                            } else {
                                running = false;
                            }
                        }
                    } else {
                        enter_pressed_ = false;
                    }

                    // 渲染游戏底图 + 暂停菜单覆盖
                    renderer_.render(map_, player_, caster_, enemies_, health_packs_,
                                    ammo_packs_, player_hp_, ammo_, hit_flash_);
                    renderer_.draw_pause_menu(pause_choice_);
                    renderer_.present();  // ← 统一 Present
                    SDL_Delay(16);
                    continue;
                }

                float mouse_x, mouse_y;
                SDL_GetRelativeMouseState(&mouse_x, &mouse_y);

                if (!paused_) {
                    double move_speed = player_.is_crouching ? 0.04 : 0.08;  // 下蹲减速
                    if (keys[SDL_SCANCODE_W]) player_.move(move_speed, map_);
                    if (keys[SDL_SCANCODE_S]) player_.move(-move_speed, map_);
                    if (keys[SDL_SCANCODE_A]) player_.strafe(-move_speed, map_);
                    if (keys[SDL_SCANCODE_D]) player_.strafe(move_speed, map_);
                    if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) shoot();
                    if (keys[SDL_SCANCODE_R]) reload();

                    // 台阶高度同步（平滑插值）
                    int tile_x = static_cast<int>(player_.pos.x);
                    int tile_y = static_cast<int>(player_.pos.y);
                    double target_z = static_cast<double>(map_.floor_h(tile_x, tile_y));
                    if (std::abs(player_.z - target_z) <= 1.1) {
                        player_.z += (target_z - player_.z) * 0.15;  // 平滑上下台阶
                    }
                    
                    // 空格跳跃（只有着地时才能跳）
                    if (keys[SDL_SCANCODE_SPACE] && player_.jump_offset <= 0.001) {
                        player_.jump_vel = 0.25;  // 起跳初速度
                    }
                    
                    // Shift 下蹲
                    player_.is_crouching = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];

                    // 武器切换（防抖）
                    if (keys[SDL_SCANCODE_1]) {
                        if (!key1_pressed_) { key1_pressed_ = true; switch_weapon(0); }
                    } else { key1_pressed_ = false; }
                    if (keys[SDL_SCANCODE_2]) {
                        if (!key2_pressed_) { key2_pressed_ = true; switch_weapon(1); }
                    } else { key2_pressed_ = false; }
                    if (keys[SDL_SCANCODE_3]) {
                        if (!key3_pressed_) { key3_pressed_ = true; switch_weapon(2); }
                    } else { key3_pressed_ = false; }

                    player_.rotate(mouse_x * 0.003);
                    player_.pitch -= mouse_y * 0.003;
                    if (player_.pitch > 1.0) player_.pitch = 1.0;
                    if (player_.pitch < -1.0) player_.pitch = -1.0;
                }

                if (!paused_) {
                    // 跳跃物理（简单重力）
                    if (player_.jump_vel != 0.0 || player_.jump_offset > 0.0) {
                        player_.jump_offset += player_.jump_vel;
                        player_.jump_vel -= 0.015;  // 重力
                        if (player_.jump_offset < 0.0) {
                            player_.jump_offset = 0.0;
                            player_.jump_vel = 0.0;
                        }
                    }

                    enemies_.erase(
                        std::remove_if(enemies_.begin(), enemies_.end(),
                            [](const Enemy& e){ return e.hp <= 0; }),
                        enemies_.end()
                    );

                    if (recoil_ > 0) --recoil_;
                    if (muzzle_flash_ > 0) --muzzle_flash_;

                    player_.recoil_pitch *= 0.82;  // 衰减更慢，必须主动压枪

                    if (shoot_cd_ > 0) --shoot_cd_;
                    if (reload_cd_ > 0) --reload_cd_;

                    double edx = player_.pos.x - (map_.exit_x() + 0.5);
                    double edy = player_.pos.y - (map_.exit_y() + 0.5);
                    if (edx*edx + edy*edy < 0.64) {
                        game_over = true; won = true;
                        continue;
                    }

                    for (auto& en : enemies_) en.update(map_, player_.pos, caster_);

                    if (hurt_cd_ > 0) --hurt_cd_;
                    for (const auto& en : enemies_) {
                        if (en.hp <= 0) continue;
                        double dx = en.pos.x - player_.pos.x;
                        double dy = en.pos.y - player_.pos.y;
                        double dist_sq = dx*dx + dy*dy;
                        if (dist_sq < 0.5 && hurt_cd_ == 0) {        // 贴身，高伤害
                            player_hp_ -= 2; hurt_cd_ = 10; audio_.play_hurt(); break;
                        } else if (dist_sq < 2.0 && hurt_cd_ == 0) {  // 靠近，持续刮痧
                            --player_hp_; hurt_cd_ = 8; audio_.play_hurt(); break;
                        }
                    }

                    if (player_hp_ <= 0) { game_over = true; won = false; continue; }

                    for (auto& hp : health_packs_) {
                        if (!hp.active) continue;
                        double dx = hp.pos.x - player_.pos.x, dy = hp.pos.y - player_.pos.y;
                        if (dx*dx + dy*dy < 0.64 && player_hp_ < 5) { if (dx*dx + dy*dy < 0.64 && player_hp_ < 5) { ++player_hp_; audio_.play_pickup(); hp.active = false; } }
                    }
                    for (auto& ap : ammo_packs_) {
                        if (!ap.active) continue;
                        double dx = ap.pos.x - player_.pos.x, dy = ap.pos.y - player_.pos.y;
                        if (dx*dx + dy*dy < 0.64) {
                            auto& w = weapons_[current_weapon_];
                            if (w.reserve_ammo != 999) {
                                w.reserve_ammo += 30;
                                if (w.reserve_ammo > 999) w.reserve_ammo = 999;
                            }
                            audio_.play_pickup();
                            ap.active = false;
                        }
                    }

                    if (hit_flash_ > 0) --hit_flash_;
                }

                renderer_.render(map_, player_, caster_, enemies_, health_packs_,
                                 ammo_packs_, player_hp_, ammo_, hit_flash_);
                renderer_.draw_weapon(weapons_[current_weapon_], recoil_, muzzle_flash_, reload_cd_);
                renderer_.present();  // ← 统一 Present
                auto& w = weapons_[current_weapon_];
                std::cout << "\r[" << w.name << "] " << w.current_ammo << "/" << w.reserve_ammo
                          << " HP:" << player_hp_ << "/5 Score:" << score_
                          << " Enemies:" << enemies_.size() << "    " << std::flush;
                SDL_Delay(16);
            }

            if (!running) break;

            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time_).count();

            if (show_end_screen(won, score_, elapsed)) {
                reset_game();
            } else {
                running = false;
            }

        } while (running);
    }

private:
    void shoot() {
        auto& w = weapons_[current_weapon_];
        if (shoot_cd_ > 0 || reload_cd_ > 0) return;
        if (w.current_ammo <= 0) { audio_.play_empty(); return; }

        --w.current_ammo;
        audio_.play_shoot(w.type);

        --w.current_ammo;
        shoot_cd_ = w.fire_rate;
        recoil_ = (w.type == WeaponType::Shotgun) ? 18 : (w.type == WeaponType::Rifle ? 4 : 10);
        muzzle_flash_ = 4;

        // 真实后坐力：视角强制抬头，玩家需手动压枪
        if (w.type == WeaponType::Pistol) {
            player_.recoil_pitch += 0.08;
        } else if (w.type == WeaponType::Rifle) {
            player_.recoil_pitch += 0.05;   // 连发 6 枪准星直接上天
        } else if (w.type == WeaponType::Shotgun) {
            player_.recoil_pitch += 0.18;   // 一枪抬头看天
        }
        if (player_.recoil_pitch > 0.9) player_.recoil_pitch = 0.9;  // 封顶

        if (w.type == WeaponType::Shotgun) {
            std::mt19937 rng{std::random_device{}()};
            std::uniform_real_distribution<double> dist(-w.spread, w.spread);
            for (int i = 0; i < w.pellets; ++i) {
                double angle = dist(rng);
                double ca = std::cos(angle), sa = std::sin(angle);
                Vec2 pellet_dir{
                    player_.dir.x * ca - player_.dir.y * sa,
                    player_.dir.x * sa + player_.dir.y * ca
                };
                double len = std::sqrt(pellet_dir.x * pellet_dir.x + pellet_dir.y * pellet_dir.y);
                if (len > 0) { pellet_dir.x /= len; pellet_dir.y /= len; }
                shoot_single(pellet_dir, w.damage);
            }
        } else {
            double angle = 0.0;
            if (w.spread > 0.0) {
                std::mt19937 rng{std::random_device{}()};
                std::uniform_real_distribution<double> dist(-w.spread, w.spread);
                angle = dist(rng);
            }
            double ca = std::cos(angle), sa = std::sin(angle);
            Vec2 bullet_dir{
                player_.dir.x * ca - player_.dir.y * sa,
                player_.dir.x * sa + player_.dir.y * ca
            };
            double len = std::sqrt(bullet_dir.x * bullet_dir.x + bullet_dir.y * bullet_dir.y);
            if (len > 0) { bullet_dir.x /= len; bullet_dir.y /= len; }
            shoot_single(bullet_dir, w.damage);
        }
    }

    void shoot_single(const Vec2& dir, int damage) {
        for (auto& e : enemies_) {
            if (e.hp <= 0) continue;
            double dx = e.pos.x - player_.pos.x;
            double dy = e.pos.y - player_.pos.y;
            double dist = std::sqrt(dx * dx + dy * dy);
            if (dist > 15.0) continue;

            double ndx = dx / dist, ndy = dy / dist;
            double dot = dir.x * ndx + dir.y * ndy;
            if (dot <= 0.92) continue;

            auto hit = caster_.cast(player_.pos, dir, map_);
            if (hit.perp_dist > dist) {
                e.hp = std::max(0, e.hp - damage);
                audio_.play_enemy_hit();
                hit_flash_ = 12;
                if (e.hp <= 0) score_ += 100;
                break;
            }
        }
    }

    void reload() {
        auto& w = weapons_[current_weapon_];
        if (reload_cd_ > 0 || w.current_ammo == w.max_ammo) return;
        audio_.play_reload();
        if (w.reserve_ammo <= 0 && w.reserve_ammo != 999) return;

        reload_cd_ = w.reload_time;
        int need = w.max_ammo - w.current_ammo;
        if (w.reserve_ammo == 999) {
            w.current_ammo = w.max_ammo;
        } else {
            int give = std::min(need, w.reserve_ammo);
            w.current_ammo += give;
            w.reserve_ammo -= give;
        }
    }

    void switch_weapon(int idx) {
        if (idx == static_cast<int>(current_weapon_)) return;
        if (idx < 0 || idx >= static_cast<int>(weapons_.size())) return;
        current_weapon_ = static_cast<size_t>(idx);
        shoot_cd_ = 0;
        reload_cd_ = 0;
        recoil_ = 0;
        muzzle_flash_ = 0;
    }

    bool show_end_screen(bool won, int score, int time_sec) {
        renderer_.set_mouse_capture(false);  // 释放鼠标，方便按键盘
        int choice = 0;  // 0=CONTINUE, 1=EXIT
        SDL_Event e;

        while (true) {
            // 绘制暗色背景 + 文字
            renderer_.draw_end_screen(won, score, time_sec, choice);
            renderer_.present();  // ← 统一 Present

            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) return false;
                if (e.type == SDL_EVENT_KEY_DOWN) {
                    if (e.key.scancode == SDL_SCANCODE_ESCAPE) return false;
                    if (e.key.scancode == SDL_SCANCODE_W || e.key.scancode == SDL_SCANCODE_UP) choice = 0;
                    if (e.key.scancode == SDL_SCANCODE_S || e.key.scancode == SDL_SCANCODE_DOWN) choice = 1;
                    if (e.key.scancode == SDL_SCANCODE_RETURN || e.key.scancode == SDL_SCANCODE_SPACE) {
                        return choice == 0;  // 0=继续, 1=退出
                    }
                }
            }
            SDL_Delay(16);
        }
    }

    void reset_game() {
        player_.pos = {1.5, 1.5};
        player_.dir = {1.0, 0.0};
        player_.plane = {0.0, 0.66};

        map_ = Map::create_maze(41, 41);  // ← 修复：41×41

        enemies_.clear();
        // 第一层包围（远端角落）
        enemies_.push_back({{39.5, 39.5}, 'E', 0.04, 5});
        enemies_.push_back({{39.5, 5.5},  'M', 0.06, 5});
        enemies_.push_back({{5.5, 39.5},  'S', 0.03, 5});
        enemies_.push_back({{5.5, 5.5},   'M', 0.07, 5});
        
        // 第二层包围（中场拦截）
        enemies_.push_back({{20.5, 20.5}, 'E', 0.05, 5});
        enemies_.push_back({{25.5, 15.5}, 'N', 0.04, 4});
        enemies_.push_back({{15.5, 25.5}, 'E', 0.045, 5});
        
        // 第三层包围（近身压迫）
        enemies_.push_back({{10.5, 10.5}, 'F', 0.09, 3});
        enemies_.push_back({{8.5, 8.5},   'F', 0.08, 3});
        enemies_.push_back({{35.5, 35.5}, 'T', 0.015, 12});
        enemies_.push_back({{30.5, 10.5}, 'G', 0.05, 6});

        health_packs_.clear();
        ammo_packs_.clear();
        std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist_x(1, map_.width() - 2);
        std::uniform_int_distribution<int> dist_y(1, map_.height() - 2);
        for (int i = 0; i < 2; ++i) {
            int hx, hy;
            do { hx = dist_x(rng); hy = dist_y(rng); }
            while (map_.is_wall(hx, hy) || (hx == 1 && hy == 1));
            health_packs_.push_back({{static_cast<double>(hx) + 0.5, static_cast<double>(hy) + 0.5}});
        }
        for (int i = 0; i < 3; ++i) {
            int ax, ay;
            do { ax = dist_x(rng); ay = dist_y(rng); }
            while (map_.is_wall(ax, ay) || (ax == 1 && ay == 1));
            ammo_packs_.push_back({{static_cast<double>(ax) + 0.5, static_cast<double>(ay) + 0.5}});
        }

        player_hp_ = 3;
        score_ = 0;
        shoot_cd_ = 0;
        hurt_cd_ = 0;
        reload_cd_ = 0;
        hit_flash_ = 0;
        paused_ = false;
        pause_choice_ = 0;
        esc_pressed_ = false;
        nav_pressed_ = false;
        enter_pressed_ = false;
        renderer_.set_mouse_capture(true);
        start_time_ = std::chrono::steady_clock::now();
    }

    Map map_;
    Player player_;
    Raycaster caster_;
    Renderer renderer_;
    std::vector<Enemy> enemies_;
    int shoot_cd_ = 0;   // ← 新增
    std::vector<HealthPack> health_packs_;
    int player_hp_ = 3;   // 上限 5
    int hurt_cd_ = 0;   // ← 新增：受伤无敌帧
    int ammo_ = 30;   // ← 新增
    int score_ = 0;
    std::vector<AmmoPack> ammo_packs_;
    std::chrono::steady_clock::time_point start_time_;
    int reload_cd_ = 0;
    int hit_flash_ = 0;
    bool paused_ = false;
    int pause_choice_ = 0;           // ← 新增
    bool esc_pressed_ = false;       // ← 新增
    bool nav_pressed_ = false;       // ← 新增
    bool enter_pressed_ = false;     // ← 新增
    std::vector<Weapon> weapons_;
    size_t current_weapon_ = 0;
    int recoil_ = 0;
    int muzzle_flash_ = 0;
    bool key1_pressed_ = false;
    bool key2_pressed_ = false;
    bool key3_pressed_ = false;
    AudioSynth audio_;
};

int main() {
    try {
        Engine engine;
        engine.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n';
        return 1;
    }
    return 0;
}