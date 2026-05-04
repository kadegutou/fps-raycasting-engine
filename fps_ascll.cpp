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

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

// ============================================================
// Module 2: Map
// ============================================================
class Map {
public:
    explicit Map(int w, int h)
        : width_(w), height_(h), grid_(h, std::vector<int>(w, 1))
    {}

    static Map create_maze(int w = 16, int h = 16) {
        Map m(w, h);
        m.generate_dfs(1, 1);
        return m;
    }

    [[nodiscard]] bool is_wall(int x, int y) const noexcept {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return true;
        return grid_[y][x] == 1;
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

    int width_, height_;
    std::vector<std::vector<int>> grid_;
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

        if (!map.is_wall(static_cast<int>(nx), static_cast<int>(pos.y))) pos.x = nx;
        if (!map.is_wall(static_cast<int>(pos.x), static_cast<int>(ny))) pos.y = ny;
    }
};

// ============================================================
// Module 3.5: Enemy
// ============================================================
struct Enemy {
    Vec2 pos{17.5, 17.5};
    char symbol = 'E';
    double speed = 0.04;
    int hp = 3;           // ← 新增

    void take_hit() {     // ← 新增
        if (hp > 0) --hp;
    }

    void update(const Map& map, const Vec2& player_pos) {
        if (hp <= 0) return;

        // 感知距离：8 格内才追击
        double pdx = player_pos.x - pos.x;
        double pdy = player_pos.y - pos.y;
        double dist_to_player = std::sqrt(pdx*pdx + pdy*pdy);
        if (dist_to_player > 20.0) return;   // 太远，站着不动

        // 3 格内冲刺
        double current_speed = speed;
        if (dist_to_player < 3.0) current_speed *= 1.5;

        auto path = find_path(map,
            {static_cast<int>(pos.x), static_cast<int>(pos.y)},
            {static_cast<int>(player_pos.x), static_cast<int>(player_pos.y)}
        );
        if (!path || path->size() <= 1) return;

        auto [tx, ty] = (*path)[1];
        double target_x = tx + 0.5;
        double target_y = ty + 0.5;

        double dx = target_x - pos.x;
        double dy = target_y - pos.y;
        double dist = std::sqrt(dx*dx + dy*dy);

        if (dist > current_speed) {         // ← 改这里
            pos.x += (dx / dist) * current_speed;   // ← 改这里
            pos.y += (dy / dist) * current_speed;   // ← 改这里
        } else {
            pos.x = target_x;
            pos.y = target_y;
        }
    }
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
// Module 4: Raycaster
// ============================================================
struct HitResult {
    double perp_dist;
    int side;
    double wall_x;
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

        return { std::abs(perp_dist), side, wall_x };
    }
};

// ============================================================
// Module 5: Renderer
// ============================================================
class Renderer {
public:
    explicit Renderer(int w, int h) : width_(w), height_(h) {}

    void render(const Map& map, const Player& player, const Raycaster& caster,
                const std::vector<Enemy>& enemies,
                const std::vector<HealthPack>& packs, 
                const std::vector<AmmoPack>& ammo_packs, 
                int player_hp, int ammo, int hit_flash = 0) {
#ifdef _WIN32
        std::system("cls");
#else
        std::cout << "\033[2J\033[H";
#endif

        // 1. Main 3D buffer + Z-buffer
        std::vector<std::string> buf(height_, std::string(width_, ' '));
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

            int draw_start = (height_ - line_height) / 2;
            int draw_end = draw_start + line_height;
            if (draw_start < 0) draw_start = 0;
            if (draw_end >= height_) draw_end = height_ - 1;

            char wall_char = pick_char(hit);

            for (int y = draw_start; y <= draw_end; ++y)
                buf[y][x] = wall_char;
        }

        // 2. Enemy sprite (inverse camera matrix + Z-buffer)
        for (const auto& enemy : enemies) {
            double dx = enemy.pos.x - player.pos.x;
            double dy = enemy.pos.y - player.pos.y;

            double inv_det = 1.0 / (player.plane.x * player.dir.y - player.dir.x * player.plane.y);
            double transform_x = inv_det * (player.dir.y * dx - player.dir.x * dy);
            double transform_y = inv_det * (-player.plane.y * dx + player.plane.x * dy);

            if (transform_y <= 0.05) continue;
            if (transform_y > 30.0) continue;  // 21x21 maze max dist ~28

            int screen_x = static_cast<int>((width_ / 2.0) * (1.0 + transform_x / transform_y));

            // Min size 3x3: always visible even at long range
            int sprite_height = static_cast<int>(height_ / transform_y);
            if (sprite_height < 3) sprite_height = 3;
            if (sprite_height > height_) sprite_height = height_;

            int sprite_width = std::max(3, sprite_height / 2);

            int draw_start_y = (height_ - sprite_height) / 2;
            int draw_end_y = draw_start_y + sprite_height;
            if (draw_start_y < 0) draw_start_y = 0;
            if (draw_end_y > height_) draw_end_y = height_;

            int draw_start_x = screen_x - sprite_width / 2;
            int draw_end_x = draw_start_x + sprite_width;

            for (int sx = draw_start_x; sx < draw_end_x; ++sx) {
                if (sx < 0 || sx >= width_) continue;
                if (transform_y >= z_buffer[sx]) continue;  // behind wall

                for (int y = draw_start_y; y < draw_end_y; ++y) {
                    // Highlight border for visibility
                    bool is_border = (sx == draw_start_x || sx == draw_end_x - 1 ||
                                      y == draw_start_y || y == draw_end_y - 1);
                    buf[y][sx] = is_border ? '*' : enemy.symbol;
                }
            }
        }

        // 3. Minimap
        auto minimap = generate_minimap(map, player, 8, enemies, packs, ammo_packs);

        // ANSI 颜色映射
        auto ansi_color = [](char c) -> const char* {
            switch (c) {
                case 'E': case 'M': case 'S': case '*': return "\033[91m"; // 敌人红
                case '+': return "\033[92m"; // 血包绿
                case '=': return "\033[93m"; // 弹药黄
                case 'P': return "\033[96m"; // 玩家青
                case '#': case '%': case '@': return "\033[37m"; // 墙白
                default: return "\033[90m"; // 地板灰
            }
        };

        // 4. Composite output
        for (int y = 0; y < height_; ++y) {
            // 主画面逐字符上色
            for (char c : buf[y]) {
                std::cout << ansi_color(c) << c;
            }
            std::cout << "\033[0m"; // 重置

            if (y < static_cast<int>(minimap.size())) {
                std::cout << "  |";
                // 小地图逐字符上色
                for (char c : minimap[y]) {
                    std::cout << ansi_color(c) << c;
                }
                std::cout << "\033[0m|";
            }
            std::cout << '\n';
        }

        // 5. HUD: position + enemy distance + direction + turn hint
        std::cout << "\n=== Pos(" << std::fixed << std::setprecision(1)
                  << player.pos.x << ", " << player.pos.y << ") ===";
        std::cout << "  Player HP: " << player_hp << "/5";
        std::cout << "  AMMO: " << ammo << "/30";

        if (!enemies.empty()) {
            const auto& e = enemies[0];
            double edx = e.pos.x - player.pos.x;
            double edy = e.pos.y - player.pos.y;
            double edist = std::sqrt(edx*edx + edy*edy);

            double inv_det = 1.0 / (player.plane.x * player.dir.y - player.dir.x * player.plane.y);
            double t_x = inv_det * (player.dir.y * edx - player.dir.x * edy);
            double t_y = inv_det * (-player.plane.y * edx + player.plane.x * edy);

            // 新增：敌人是否处于追击距离
            bool chasing = (edist <= 8.0);

            std::string dir_hint;
            std::string turn_hint;
            if (chasing && t_y > 0.5 && std::abs(t_x / t_y) < 0.3) {
                dir_hint = "[E CHASING!]";
                turn_hint = "SHOOT NOW!";
            } else if (chasing && t_x > 0) {
                dir_hint = "[E CHASING!]";
                turn_hint = ">>> TURN AND SHOOT";
            } else if (chasing) {
                dir_hint = "[E CHASING!]";
                turn_hint = "<<< TURN AND SHOOT";
            } else if (t_y > 0.5 && std::abs(t_x / t_y) < 0.3) {
                dir_hint = "[E AHEAD]";
                turn_hint = "ENEMY IN SIGHT!";
            } else if (t_x > 0) {
                dir_hint = "[E RIGHT]";
                turn_hint = ">>> PRESS D";
            } else {
                dir_hint = "[E LEFT]";
                turn_hint = "<<< PRESS A";
            }

            std::cout << "  Enemy: " << std::setprecision(1) << edist 
                      << " HP:" << e.hp << " " << dir_hint;
            if (hit_flash > 0) std::cout << " *** HIT! ***";
            std::cout << "\n>>> " << turn_hint << " <<<";
        }
        std::cout << "\n";
    }

private:
    [[nodiscard]] std::vector<std::string> generate_minimap(
        const Map& map, const Player& player, int size,
        const std::vector<Enemy>& enemies,
        const std::vector<HealthPack>& packs, 
        const std::vector<AmmoPack>& ammo_packs) const
    {
        std::vector<std::string> result;
        int px = static_cast<int>(player.pos.x);
        int py = static_cast<int>(player.pos.y);
        int half = size / 2;

        for (int dy = -half; dy < half; ++dy) {
            std::string row;
            for (int dx = -half; dx < half; ++dx) {
                int mx = px + dx;
                int my = py + dy;

                bool drawn = false;
                if (mx == px && my == py) {
                    row += 'P';
                    drawn = true;
                } else {
                    for (const auto& e : enemies) {
                        if (mx == static_cast<int>(e.pos.x) && my == static_cast<int>(e.pos.y)) {
                            row += e.symbol;
                            drawn = true;
                            break;
                        }
                    }
                }

                if (!drawn) {
                    for (const auto& ap : ammo_packs) {
                        if (ap.active && mx == static_cast<int>(ap.pos.x) && my == static_cast<int>(ap.pos.y)) {
                            row += '=';
                            drawn = true;
                            break;
                        }
                    }
                }

                if (!drawn) {
                    for (const auto& hp : packs) {
                        if (hp.active && mx == static_cast<int>(hp.pos.x) && my == static_cast<int>(hp.pos.y)) {
                            row += '+';
                            drawn = true;
                            break;
                        }
                    }
                }

                if (!drawn) row += map.is_wall(mx, my) ? '#' : '.';
            }
            result.push_back(row);
        }
        return result;
    }

    [[nodiscard]] char pick_char(const HitResult& hit) const noexcept {
        int tex = (hit.wall_x < 0.5) ? 0 : 1;

        if (hit.perp_dist < 1.5) {
            constexpr char t[2][2] = {{'#', '%'}, {'@', 'x'}};
            return t[hit.side][tex];
        } else if (hit.perp_dist < 3.0) {
            constexpr char t[2][2] = {{'%', '+'}, {'x', '='}};
            return t[hit.side][tex];
        } else if (hit.perp_dist < 5.0) {
            constexpr char t[2][2] = {{'+', '*'}, {'=', '|'}};
            return t[hit.side][tex];
        } else {
            constexpr char t[2][2] = {{'*', '.'}, {'|', ','}};
            return t[hit.side][tex];
        }
    }

    int width_, height_;
};

// ============================================================
// Module 6: Engine
// ============================================================
class Engine {
public:
    Engine() : map_(Map::create_maze(21, 21)), renderer_(60, 20) {
        enemies_.push_back({{17.5, 17.5}, 'E', 0.08});  // 右下
        enemies_.push_back({{17.5, 3.5},  'M', 0.12});  // 右上，快怪
        enemies_.push_back({{3.5, 17.5},  'S', 0.06});  // 左下，慢怪

        // 随机生成 4 个血包，避开墙壁和出生点
        std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist_x(1, map_.width() - 2);
        std::uniform_int_distribution<int> dist_y(1, map_.height() - 2);
        for (int i = 0; i < 4; ++i) {
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
        }
    }

    void run() {
        std::cout << "=== FPS Raycasting Engine ===\n"
                  << "W/S = Forward/Back, A/D = Rotate, Q = Quit\n"
                  << "Press Enter to start...";
        std::cin.get();

        start_time_ = std::chrono::steady_clock::now();

        char cmd = ' ';
        while (cmd != 'q') {
            // 新增：移除死亡敌人（用 erase-remove 惯用法）
            enemies_.erase(
                std::remove_if(enemies_.begin(), enemies_.end(),
                    [](const Enemy& e){ return e.hp <= 0; }),
                enemies_.end()
            );

            if (shoot_cd_ > 0) --shoot_cd_;   // ← 新增
            if (reload_cd_ > 0) --reload_cd_;

            // ← 胜利判定插在这里
            if (player_.pos.x > 18.0 && player_.pos.y > 18.0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - start_time_
                ).count();

#ifdef _WIN32
                std::system("cls");
#else
                std::cout << "\033[2J\033[H";
#endif
                std::cout << "\n\n    === YOU ESCAPED THE MAZE ===\n"
                        << "    Enemies killed: " << (3 - static_cast<int>(enemies_.size())) << "\n"
                        << "    Ammo left: " << ammo_ << "/30\n"
                        << "    Score: " << score_ << "\n"
                        << "    Time: " << elapsed << "s\n\n";
                break;
            }

            for (auto& e : enemies_) e.update(map_, player_.pos);

            // 受伤无敌帧递减
            if (hurt_cd_ > 0) --hurt_cd_;

            // 敌人碰撞伤害（距离 < 0.8，且不在无敌状态）
            for (const auto& e : enemies_) {
                if (e.hp <= 0) continue;
                double dx = e.pos.x - player_.pos.x;
                double dy = e.pos.y - player_.pos.y;
                if (dx*dx + dy*dy < 1.0 && hurt_cd_ == 0) {
                    --player_hp_;
                    hurt_cd_ = 15;  // 15帧无敌（约2-3秒）
                    break;  // 一帧只受一次伤
                }
            }

            // 死亡判定
            if (player_hp_ <= 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - start_time_
                ).count();

#ifdef _WIN32
                std::system("cls");
#else
                std::cout << "\033[2J\033[H";
#endif
                std::cout << "\n\n    === GAME OVER ===\n"
                        << "    You were caught by the enemy.\n"
                        << "    Ammo left: " << ammo_ << "/30\n"
                        << "    Score: " << score_ << "\n"
                        << "    Time: " << elapsed << "s\n\n";
                break;
            }

            // 血包拾取判定
            for (auto& hp : health_packs_) {
                if (!hp.active) continue;
                double dx = hp.pos.x - player_.pos.x;
                double dy = hp.pos.y - player_.pos.y;
                if (dx*dx + dy*dy < 0.64) { // 距离 < 0.8
                    if (player_hp_ < 5) {
                        ++player_hp_;
                        hp.active = false;
                    }
                }
            }

            // 弹药箱拾取
            for (auto& ap : ammo_packs_) {
                if (!ap.active) continue;
                double dx = ap.pos.x - player_.pos.x;
                double dy = ap.pos.y - player_.pos.y;
                if (dx*dx + dy*dy < 0.64) {
                    ammo_ = 30;
                    ap.active = false;
                }
            }
                
            if (hit_flash_ > 0) --hit_flash_;

            renderer_.render(map_, player_, caster_, enemies_, health_packs_, ammo_packs_, player_hp_, ammo_);
            std::cout << "\n[WSAD/q]> ";
            std::string line;
            if (!std::getline(std::cin, line)) break;
            if (line.empty()) continue;

            bool quit = false;
            int processed = 0;
            for (char c : line) {
                if (c == 'q') { quit = true; break; }
                if (c == ' ' || c == 'w' || c == 's' || c == 'a' || c == 'd' || c == 'r') {
                    handle_input(c);
                    if (++processed >= 5) break;  // 一帧最多执行 5 个指令
                }
            }
            if (quit) break;
        }
    }

private:
    void handle_input(char c) {
        switch (c) {
            case 'w': player_.move(0.3, map_); break;
            case 's': player_.move(-0.3, map_); break;
            case 'a': player_.rotate(-0.15); break;
            case 'd': player_.rotate(0.15); break;
            case ' ': shoot(); break;   // ← 新增
            case 'r': reload(); break;
        }
    }

    void shoot() {
        if (shoot_cd_ > 0) return;   // ← 新增：冷却中直接返回
        if (ammo_ <= 0) return;   // ← 新增：没子弹了
        --ammo_;                   // ← 新增
        shoot_cd_ = 5;                // ← 新增：设置 5 帧冷却

        for (auto& e : enemies_) {
            if (e.hp <= 0) continue;

            double dx = e.pos.x - player_.pos.x;
            double dy = e.pos.y - player_.pos.y;
            double dist = std::sqrt(dx*dx + dy*dy);
            if (dist > 10.0) continue;          // 射程 10 格

            // 准星判定：敌人方向与玩家朝向夹角 < ~11度（dot > 0.98）
            double ndx = dx / dist, ndy = dy / dist;
            double dot = player_.dir.x * ndx + player_.dir.y * ndy;
            if (dot <= 0.92) continue;

            // 墙遮挡检测：向敌人方向打射线，确认敌人比墙近
            Vec2 ray_dir{ndx, ndy};
            auto hit = caster_.cast(player_.pos, ray_dir, map_);
            if (hit.perp_dist > dist) {
                e.take_hit();
                hit_flash_ = 3;   // ← 新增：命中闪烁 3 帧
                if (e.hp <= 0) score_ += 100;
                break;  // 一次只打一个
            }
        }
    }

    void reload() {
        if (reload_cd_ > 0) return;
        reload_cd_ = 15;   // 2-3秒硬直
        ammo_ = 30;
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