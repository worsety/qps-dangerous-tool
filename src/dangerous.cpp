#include <windows.h>
#include <detours.h>
#include <vector>
#include <array>
#include "dangerous.h"
#include "mod.h"

using namespace std;
using namespace QPSD;

explicit_ptr<Player> qp;
explicit_ptr<Vector<Enemy>> enemies;
explicit_ptr<List<Bullet>> bullets;
explicit_ptr<List<TempHitbox>> temphitboxes;
explicit_ptr<List<Solid>> solids;
explicit_ptr<List<Star>> stars;
explicit_ptr<List<Shot>> shots;
explicit_ptr<List<Laser>> lasers;
explicit_ptr<List<Snake>> snakes;
explicit_ptr<List<Sprite>> sprites;
explicit_ptr<List<ScoreNotif>> scorenotifs;
explicit_ptr<List<Balloon>> balloons;
explicit_ptr<List<Caption>> captions;

pseudo_ref<int> stage_num;
pseudo_ref<int> game_frame;
pseudo_ref<int> stage_end;
pseudo_ref<int> stage_phase;
pseudo_ref<int> stage_phase_frame;
pseudo_ref<float> camera_x_scrolling_speed;
pseudo_ref<char> controls_enabled;
pseudo_ref<char> collision_enabled;
pseudo_ref<char> damage_enabled;
pseudo_ref<char> pause_enabled;
pseudo_ref<char> shoot_enabled;
pseudo_ref<char> chain_timer_active;
pseudo_ref<int> chain_timer_multiplier;
pseudo_ref<char> boss_break;
pseudo_ref<char> boss_dead;
pseudo_ref<char> boss_unkflag; // used by the final boss
pseudo_ref<char> in_bossfight;
pseudo_ref<int> boss_damage_idx;
pseudo_ref<float> boss_damage;
pseudo_ref<char> in_conversation;
pseudo_ref<int> last_enemy_id;
pseudo_ref<int> last_enemy_group_id; // only by a couple of enemies in the game
pseudo_ref<char> in_results_screen;
pseudo_ref<int> bgm_track;
pseudo_ref<int> bgm_next_track;
pseudo_ref<char> bgm_next_loop;
pseudo_ref<int> bgm_volume;
pseudo_ref<int> bgm_handle;

explicit_ptr<BG2> bg2;
explicit_ptr<BG3> bg3;
explicit_ptr<BG4> bg4;
explicit_ptr<BG5> bg5;

pseudo_ref<SideBarChar[2]> sidebar_char;

pseudo_ref<char> in_cutscene;
pseudo_ref<char[8]> cutscene_active;
explicit_ptr<void (*)()> cutscene_delete;

pseudo_ref<char> pausemenu_isopen;

pseudo_ref<char> screen_fade_active;
pseudo_ref<int> screen_fade_type;
pseudo_ref<float> screen_fade_rate;
pseudo_ref<float> screen_fade_progress;
pseudo_ref<char> screen_fade_finished;

explicit_ptr<void ()> apply_bgm_volume;
explicit_ptr<void ()> conversation_reset;

explicit_ptr<int (int x1, int y1, int x2, int y2)> SetDrawArea;
explicit_ptr<int ()> SetDrawAreaFull;
explicit_ptr<int (const wchar_t *FileName, int NotUse3DFlag)> LoadGraph;
explicit_ptr<int (int x1, int y1, int x2, int y2, int Color, int Thickness)> DrawLine;
explicit_ptr<int (int Handle)> SubHandle;

// For testing and the future
void draw_overlay()
{
}

void set_bgm(int track, bool instant = false, bool restart = false)
{
    if (!restart && track == bgm_next_track && track == bgm_track)
        return;
    bgm_next_track = track;
    if (restart && track == bgm_track)
        bgm_track = 0;
    if (instant)
        bgm_volume = 0;
    // Getting the value set when the music was started seems involved
    // but it's set consistently per track
    switch (track) {
    case 0: // silence
    case 5: // credits
    case 6: // results
    case 7: // game over
    case 16: // final boss intro
        bgm_next_loop = 0;
        return;
    default:
        bgm_next_loop = 1;
        return;
    }
}

const wchar_t *sidebar_image_left[4] = {
    L".\\model\\SIDE\\side_qp00.bmp",
    L".\\model\\SIDE\\side_qp01.bmp",
    L".\\model\\SIDE\\side_qpw00.bmp",
    L".\\model\\SIDE\\side_qpw01.bmp",
};
const wchar_t *sidebar_image_right[6][4] = {
    {},
    {
        L".\\model\\SIDE\\side_syura00.bmp",
        L".\\model\\SIDE\\side_syura00.bmp",
    },
    {
        L".\\model\\SIDE\\side_kyosuke00.bmp",
        L".\\model\\SIDE\\side_kyosuke00.bmp",
    },
    {
        L".\\model\\SIDE\\side_krila00.bmp",
        L".\\model\\SIDE\\side_krila00.bmp",
    },
    {
        L".\\model\\SIDE\\side_yuki00.bmp",
        L".\\model\\SIDE\\side_yuki01.bmp",
    },
    {
        L".\\model\\SIDE\\side_sb00.bmp",
        L".\\model\\SIDE\\side_sb01.bmp",
        L".\\model\\SIDE\\side_sbw00.bmp",
        L".\\model\\SIDE\\side_sbw01.bmp",
    },
};

void fix_sidebars()
{
    if (sidebar_char[0].visible) {
        if (stage_phase == 12 || stage_num == 5 && stage_phase == 13)
            return;
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 2; j++)
                SubHandle(sidebar_char[i].texHandle[j]);
            sidebar_char[i].visible = 0;
        }
    }
    if (!sidebar_char[0].visible) {
        if (stage_phase != 12 && !(stage_num == 5 && stage_phase == 13))
            return;
        sidebar_char[0].texHandle[0] = LoadGraph(sidebar_image_left[2 * (stage_phase == 13)], 0);
        sidebar_char[0].texHandle[1] = LoadGraph(sidebar_image_left[1 + 2 * (stage_phase == 13)], 0);
        sidebar_char[1].texHandle[0] = LoadGraph(sidebar_image_right[stage_num][2 * (stage_phase == 13)], 0);
        sidebar_char[1].texHandle[1] = LoadGraph(sidebar_image_right[stage_num][1 + 2 * (stage_phase == 13)], 0);
        for (int i = 0; i < 2; i++) {
            sidebar_char[i].visible = 1;
            sidebar_char[i].opacity = 0.f;
            sidebar_char[i].dw08 = 0;
            sidebar_char[i].state = 0; // fade in
            sidebar_char[i].dw10 = 0;
        }
    }
}

void cutscenes_delete()
{
    for (int i = 0; i < 8; i++)
        if (cutscene_active[i]) {
            cutscene_delete[i]();
            cutscene_active[i] = 0;
        }
    in_cutscene = 0;
}

// Only (mostly) correct for defined checkpoints
// Intentionally does not account for jumping into bosses
// FIXME: Stage 3 outside is constantly generating geometry
// so rewinding during or after the bat section
// can have very odd results
void fix_aesthetics()
{
    int stage = stage_num;
    int phase = stage_phase;
    int frame = stage_phase_frame;

    switch (stage) {
    case 1:
        set_bgm(8);
        break;
    case 2:
        set_bgm(10);
        bg2->state.dw04 = 0;
        bg2->state.dw08 = 3;
        bg2->state.f0c = 0.f;
        break;
    case 3:
        set_bgm(12);
        bg3->state.dw04 = 0;
        bg3->state.b08 = 0;
        bg3->state.b09 = 0;
        bg3->state.f0c = 0.f;
        bg3->state.f10 = 2.f;
        bg3->state.f14 = 450.f;
        bg3->state.f18 = 1.f;
        bg3->state.f1c = 1.f;
        bg3->state.f20 = 1.f;
        switch (phase) {
        case 1:
            bg3->state.b09 = 1;
            break;
        case 3:
            bg3->state.f20 = 0.f;
            break;
        case 7:
            bg3->state.b08 = 1;
            bg3->state.b09 = 1;
            if (frame <= 100)
                bg3->state.f18 = 0.975f;
            else
                bg3->state.f18 = 0.5f;
            break;
        case 11:
            bg3->state.dw04 = 100;
            bg3->state.b08 = 1;
            bg3->state.f10 = 0.74872136f;
            bg3->state.f18 = 0.5f;
            bg3->state.f1c = 0.f;
            bg3->state.f20 = 0.75f;
            break;
        default:
            break;
        }
        break;
    case 4:
        switch (phase) {
        case 6:
            if (frame < 600)
                break;
        case 7:
        case 8:
        case 9:
            set_bgm(15);
            break;
        default:
            set_bgm(14);
        }
        bg4->state.dw04 = 100;
        bg4->state.f0c = 450.f;
        bg4->state.f10 = -450.f;
        bg4->state.f14 = 0.95f;
        bg4->state.b18 = 0;
        bg4->state.f1c = 0;
        switch (phase) {
        case 1:
            bg4->state.f0c = 750.f;
            bg4->state.f10 = -300.f;
            bg4->state.f14 = 1.05f;
            break;
        case 2:
            break;
        case 3:
            break;
        case 11:
            bg4->state.dw04 = 200;
            bg4->state.f0c = 750.f;
            bg4->state.f10 = -250.f;
            bg4->state.f14 = 0.951f;
            bg4->state.f1c = 0.999f;
            break;
        default:
            break;
        }
        break;
    case 5:
        set_bgm(17);
        bg5->state.dw04 = 0;
        bg5->state.f08 = 0.f;
        bg5->state.f10 = -500.f;
        bg5->state.f18 = 0.75f;
        bg5->state.f1c = 0.f;
        bg5->state.f20 = 0.f;
        bg5->state.f24 = 0.5f;
        bg5->state.f28 = 0.f;
        bg5->state.f2c = 0.f;
        bg5->state.f30 = 1.f;
        bg5->state.f34 = 0.75f;
        switch (phase) {
        case 1:
            bg5->state.f08 = 0.55f;
            bg5->state.f10 = -200.f;
            bg5->state.f18 = 0.5f;
            bg5->state.f34 = 0.2f;
            break;
        case 2:
            break;
        case 4:
            break;
        case 9:
            bg5->state.dw04 = 200;
            if (frame <= 100) {
                bg5->state.f08 = -1.35f;
                bg5->state.f30 = 0.9f;
                bg5->state.f34 = 0.95f;
            } else {
                bg5->state.f08 = -1.5708f;
                bg5->state.f18 = 0.251f;
                bg5->state.f1c = 0.499f;
                bg5->state.f30 = 0.f;
                bg5->state.f34 = 0.f;
            }
            break;
        }
        break;
    default:
        break;
    }
    fix_sidebars();
}

struct SaveState {
    int stage_num = -1;
    int game_frame;
    int stage_end;
    int stage_phase;
    int stage_phase_frame;
    float camera_x_scrolling_speed;
    char controls_enabled;
    char collision_enabled;
    char damage_enabled;
    char pause_enabled;
    char shoot_enabled;
    char chain_timer_active;
    int chain_timer_multiplier;
    char boss_break;
    char boss_dead;
    char boss_unkflag;
    char in_bossfight;
    int boss_damage_idx;
    float boss_damage;
    int bgm_track;
    int last_enemy_id;
    int last_enemy_group_id;
    Player qp;
    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<TempHitbox> temphitboxes;
    std::vector<Solid> solids;
    std::vector<Star> stars;
    std::vector<Shot> shots;
    std::vector<Laser> lasers;
    std::vector<Snake> snakes;
    std::vector<Sprite> sprites;
    std::vector<ScoreNotif> scorenotifs;
    std::vector<Balloon> balloons;
    std::vector<Caption> captions;
    struct {
        char state[0x38];
        std::vector<std::array<int, 2>> v2;
        std::vector<std::array<int, 3>> v3_1;
        std::vector<std::array<int, 3>> v3_2;
        std::vector<std::array<int, 4>> v4;
        std::vector<std::array<int, 5>> v5;
        char wind[0x2882c];
    } bg;
    void save_state();
    void load_state();
} savestate[2];

template<typename T>
void copy(std::vector<T> &dst, const Vector<T> &src)
{
    size_t n = src.size();
    dst.resize(n);
    if (n)
        memcpy(&dst[0], src.start, n * sizeof(T));
}

template<typename T>
void copy(Vector<T> &dst, const std::vector<T> &src)
{
    size_t n = src.size();
    if (dst.start + n > dst.end_of_storage) {
        HANDLE heap = GetProcessHeap();
        HeapFree(heap, 0, dst.start);
        dst.start = reinterpret_cast<T*>(HeapAlloc(heap, 0, n * sizeof(T)));
        if (!dst.start)
            ExitProcess(0);
        dst.end_of_storage = dst.start + n;
    }
    if (n)
        memcpy(dst.start, &src[0], n * sizeof(T));
    dst.finish = dst.start + n;
}

template<typename T>
void copy(std::vector<T> &dst, const List<T> &src)
{
    size_t n = src.size;
    dst.resize(n);
    size_t i = 0;
    for (ListNode<T> *node = src.front->next; node != src.front; node = node->next)
        memcpy(&dst[i++], &node->value, sizeof(T));
}

template<typename T>
void copy(List<T> &dst, const std::vector<T> &src)
{
    HANDLE heap = GetProcessHeap();
    dst.clear();
    size_t n = src.size();
    ListNode<T> *prev = dst.front->prev;
    for (size_t i = 0; i < n; ++i) {
        ListNode<T> *node = reinterpret_cast<ListNode<T>*>(HeapAlloc(heap, 0, sizeof(ListNode<T>)));
        if (!node)
            ExitProcess(0);
        memcpy(&node->value, &src[i], sizeof(T));
        node->prev = prev;
        node->next = dst.front;
        prev->next = node;
        dst.front->prev = node;
        prev = node;
    }
    dst.size = n;
}

void SaveState::save_state()
{
    stage_num = ::stage_num;
    game_frame = ::game_frame;
    stage_end = ::stage_end;
    stage_phase = ::stage_phase;
    stage_phase_frame = ::stage_phase_frame;
    camera_x_scrolling_speed = ::camera_x_scrolling_speed;
    controls_enabled = ::controls_enabled;
    collision_enabled = ::collision_enabled;
    damage_enabled = ::damage_enabled;
    pause_enabled = ::pause_enabled;
    shoot_enabled = ::shoot_enabled;
    chain_timer_active = ::chain_timer_active;
    chain_timer_multiplier = ::chain_timer_multiplier;
    boss_break = ::boss_break;
    boss_dead = ::boss_dead;
    boss_unkflag = ::boss_unkflag;
    in_bossfight = ::in_bossfight;
    boss_damage_idx = ::boss_damage_idx;
    boss_damage = ::boss_damage;
    last_enemy_id = ::last_enemy_id;
    last_enemy_group_id = ::last_enemy_group_id;
    bgm_track = ::bgm_next_track;
    qp = *::qp;
    copy(enemies, *::enemies);
    copy(bullets, *::bullets);
    copy(temphitboxes, *::temphitboxes);
    copy(solids, *::solids);
    copy(stars, *::stars);
    copy(shots, *::shots);
    copy(lasers, *::lasers);
    copy(snakes, *::snakes);
    copy(sprites, *::sprites);
    copy(scorenotifs, *::scorenotifs);
    copy(balloons, *::balloons);
    copy(captions, *::captions);
    switch (stage_num) {
    case 2:
        memcpy(bg.state, &::bg2->state, sizeof(::bg2->state));
        copy(bg.v3_1, ::bg2->geometry.v3_1);
        copy(bg.v3_2, ::bg2->geometry.v3_2);
        copy(bg.v2, ::bg2->geometry.v2);
        break;
    case 3:
        memcpy(bg.state, &::bg3->state, sizeof(::bg3->state));
        copy(bg.v3_1, ::bg3->geometry.v3);
        copy(bg.v4, ::bg3->geometry.v4);
        break;
    case 4:
        memcpy(bg.state, &::bg4->state, sizeof(::bg4->state));
        copy(bg.v5, ::bg4->geometry.l5);
        copy(bg.v2, ::bg4->geometry.l2);
        break;
    case 5:
        memcpy(bg.state, &::bg5->state, sizeof(::bg5->state));
        memcpy(bg.wind, &::bg5->geometry.wind, sizeof(::bg5->geometry.wind));
        copy(bg.v5, ::bg5->geometry.l5);
        break;
    default:
        break;
    }
}

void SaveState::load_state()
{
    if (stage_num != ::stage_num)
        return;
    ::game_frame = game_frame;
    ::stage_end = stage_end;
    ::stage_phase = stage_phase;
    ::stage_phase_frame = stage_phase_frame;
    ::camera_x_scrolling_speed = camera_x_scrolling_speed;
    ::controls_enabled = controls_enabled;
    ::collision_enabled = collision_enabled;
    ::damage_enabled = damage_enabled;
    ::pause_enabled = pause_enabled;
    ::shoot_enabled = shoot_enabled;
    ::chain_timer_active = chain_timer_active;
    ::chain_timer_multiplier = chain_timer_multiplier;
    ::boss_break = boss_break;
    ::boss_dead = boss_dead;
    ::boss_unkflag = boss_unkflag;
    ::in_bossfight = in_bossfight;
    ::boss_damage_idx = boss_damage_idx;
    ::boss_damage = boss_damage;
    ::last_enemy_id = last_enemy_id;
    ::last_enemy_group_id = last_enemy_group_id;
    set_bgm(bgm_track);
    *::qp = qp;
    copy(*::enemies, enemies);
    copy(*::bullets, bullets);
    copy(*::temphitboxes, temphitboxes);
    copy(*::solids, solids);
    copy(*::stars, stars);
    copy(*::shots, shots);
    copy(*::lasers, lasers);
    copy(*::snakes, snakes);
    copy(*::sprites, sprites);
    copy(*::scorenotifs, scorenotifs);
    copy(*::balloons, balloons);
    copy(*::captions, captions);
    switch (stage_num) {
    case 2:
        memcpy(&::bg2->state, bg.state, sizeof(::bg2->state));
        copy(::bg2->geometry.v3_1, bg.v3_1);
        copy(::bg2->geometry.v3_2, bg.v3_2);
        copy(::bg2->geometry.v2, bg.v2);
        break;
    case 3:
        memcpy(&::bg3->state, bg.state, sizeof(::bg3->state));
        copy(::bg3->geometry.v3, bg.v3_1);
        copy(::bg3->geometry.v4, bg.v4);
        break;
    case 4:
        memcpy(&::bg4->state, bg.state, sizeof(::bg4->state));
        copy(::bg4->geometry.l5, bg.v5);
        copy(::bg4->geometry.l2, bg.v2);
        break;
    case 5:
        memcpy(&::bg5->state, bg.state, sizeof(::bg5->state));
        memcpy(&::bg5->geometry.wind, bg.wind, sizeof(::bg5->geometry.wind));
        copy(::bg5->geometry.l5, bg.v5);
        break;
    default:
        break;
    }
    fix_sidebars();
    cutscenes_delete();
    conversation_reset();
    ::in_results_screen = 0;
}

void reset_most_state()
{
    camera_x_scrolling_speed = 0.f;
    stage_end = 0;
    controls_enabled = 1;
    collision_enabled = 1;
    damage_enabled = 1;
    pause_enabled = 1;
    shoot_enabled = 1;
    chain_timer_multiplier = 1;
    boss_break = 0;
    boss_dead = 0;
    boss_unkflag = 0;
    in_bossfight = 0;
    in_results_screen = 0;
    enemies->clear();
    bullets->clear();
    temphitboxes->clear();
    solids->clear();
    stars->clear();
    lasers->clear();
    snakes->clear();
    sprites->clear();
    captions->clear();
    cutscenes_delete();
    conversation_reset();
    qp->state = 3; // revive if game over
}

void cycle_hyper_charge()
{
    if (qp->in_hyper || qp->shottype == 49)
        return;
    if (qp->hyper_charge >= 360.f)
        qp->hyper_charge = 0.f;
    else if (qp->hyper_charge >= 240.f)
        qp->hyper_charge = 360.f;
    else
        qp->hyper_charge = 240.f;
}

int seek_recency;

struct Checkpoint {
    int stage;
    int phase;
    int frame;
    bool stop_timer;
    void go() const
    {
        if (stage != stage_num)
            return;
        stage_phase = phase;
        stage_phase_frame = frame;
        chain_timer_active = !stop_timer;
        reset_most_state();
        if (phase == 1) {
            qp->hyper_charge = 0.f;
            qp->chain = 0;
            qp->chain_timer = 0;
        }
        fix_aesthetics();
        seek_recency = 60;
    }
};

// Frequently flags are set at the end of a phase and
// sometimes even a boss is spawned for use in the next phase
Checkpoint checkpoints[] = {
    {1, 1, 9999},
    {1, 3, 9999},
    {1, 4, 9999},
    {2, 1, 9999},
    {2, 2, 9999},
    // did you know there's an implemented but unused phase 3 in stage 2?
    {2, 4, 200},
    {2, 4, 4200},
    {3, 1, 9999},
    {3, 3, 9999},
    {3, 7, 100, true},
    {3, 7, 9999},
    {3, 11, 9999},
    {4, 1, 9999},
    {4, 2, 9999},
    {4, 3, 9999},
    {4, 11, 1, true},
    {5, 1, 9999},
    {5, 2, 9999},
    {5, 4, 9999},
    {5, 9, 100, true},
    {5, 9, 9999},
    // Just F1 and savestate in the cutscene if you want to skip to the final attack
};

// special handling for stage 2 and 3 minibosses
int effective_phase()
{
    if (stage_num == 2 && stage_phase == 13)
        return 3;
    if (stage_num == 3 && stage_phase == 13)
        return 4;
    return stage_phase;
}

void seek_backward()
{
    auto phase = effective_phase();
    // skip the nearest checkpoint if we just loaded a checkpoint
    int n = seek_recency > 0 ? 2 : 1;
    for (auto i = sizeof checkpoints / sizeof *checkpoints; i--;) {
        auto &c = checkpoints[i];
        if (c.stage == stage_num
            && (c.phase < phase || c.phase == phase && c.frame < stage_phase_frame)
            && 0 == --n)
            return c.go();
    }
}

void seek_forward()
{
    auto phase = effective_phase();
    for (auto &c : checkpoints) {
        if (c.stage == stage_num
            && c.phase != 1 // You probably aren't trying to skip the title card
            && (c.phase > phase || c.phase == phase && c.frame > stage_phase_frame))
            return c.go();
    }
}

void return_to_main_menu()
{
    stage_end = 5;
    screen_fade_active = 1;
    screen_fade_type = 1;
    screen_fade_rate = 0.05f;
    screen_fade_progress = 0.f;
    screen_fade_finished = 0;
    controls_enabled = 0;
    collision_enabled = 0;
    pausemenu_isopen = 0;
}

BYTE keys[2][256];
int keys_idx;
bool key_struck(int vk)
{
    return (keys[keys_idx][vk] & 0x80)
        && !(keys[!keys_idx][vk] & 0x80);
}

namespace Orig {
    explicit_ptr<void __cdecl ()> game_tick;
}

namespace Hook {
    void game_tick()
    {
        if (seek_recency > 0)
            --seek_recency;
        GetKeyboardState(keys[keys_idx]);
        if (key_struck(VK_F1)) {
            if (in_cutscene)
                cutscenes_delete();
            else if (in_conversation)
                conversation_reset();
            else
                for (Enemy *enemy = enemies->start; enemy != enemies->finish; enemy++)
                    if (11 == enemy->id)
                        enemy->hp = 0;
        }
        if (key_struck(VK_F2))
            cycle_hyper_charge();
        if (key_struck(VK_F3))
            seek_backward();
        if (key_struck(VK_F4))
            seek_forward();
        if (key_struck(VK_F5))
            savestate[0].save_state();
        if (key_struck(VK_F6))
            savestate[0].load_state();
        if (key_struck(VK_F7))
            savestate[1].save_state();
        if (key_struck(VK_F8))
            savestate[1].load_state();
        keys_idx = !keys_idx;

        // Fade music back in if we revert music during a fade out
        if (bgm_handle && bgm_next_track == bgm_track && bgm_volume < 128) {
            bgm_volume++;
            apply_bgm_volume();
        }

        Orig::game_tick();
        draw_overlay();
    }
}

void hook()
{
    DetourTransactionBegin();
    DetourAttach(&(PVOID&)Orig::game_tick.ptr, Hook::game_tick);
    DetourTransactionCommit();
}

void unhook()
{
    DetourTransactionBegin();
    DetourDetach(&(PVOID&)Orig::game_tick.ptr, Hook::game_tick);
    DetourTransactionCommit();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
    {
        explicit_ptr<IMAGE_DOS_HEADER> dos;
        explicit_ptr<IMAGE_NT_HEADERS32> nt;

        PTR_MOD(nullptr, 0, dos);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return FALSE;
        PTR_MOD(nullptr, dos->e_lfanew, nt);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return FALSE;

        switch(nt->FileHeader.TimeDateStamp) {
        // Before 1.0: absolutely not, update your game
        case 1359384688: // 1.3, last patch for doujin version
            // TODO: maybe 1.3 support some day
            return FALSE;
        case 1428050881: // 1.4.1
            PTR_EXE(0xb3'2e90, qp);
            PTR_EXE(0xb3'5974, bullets);
            PTR_EXE(0xb3'598c, temphitboxes);
            PTR_EXE(0xb3'5940, solids);
            PTR_EXE(0xb3'592c, stars);
            PTR_EXE(0xb3'597c, shots);
            PTR_EXE(0xb3'5984, lasers);
            PTR_EXE(0xb3'5948, snakes);
            PTR_EXE(0xb3'5950, sprites);
            PTR_EXE(0xb3'5958, scorenotifs);
            PTR_EXE(0xb3'596c, balloons);
            PTR_EXE(0xb3'3be4, captions);
            PTR_EXE(0xb3'3bec, enemies);
            REF_EXE(0xb3'486c, stage_num);
            REF_EXE(0xb3'4848, game_frame);
            REF_EXE(0xb3'484c, stage_end);
            REF_EXE(0xb3'4850, stage_phase);
            REF_EXE(0xb3'4854, stage_phase_frame);
            REF_EXE(0xb3'4858, camera_x_scrolling_speed);
            REF_EXE(0xb3'485c, controls_enabled);
            REF_EXE(0xb3'485d, collision_enabled);
            REF_EXE(0xb3'485e, damage_enabled);
            REF_EXE(0xb3'485f, pause_enabled);
            REF_EXE(0xb3'4860, shoot_enabled);
            REF_EXE(0xb3'4861, chain_timer_active);
            REF_EXE(0xb3'4864, chain_timer_multiplier);
            REF_EXE(0xb3'4868, boss_break);
            REF_EXE(0xb3'4869, boss_dead);
            REF_EXE(0xb3'486a, boss_unkflag);
            REF_EXE(0xb3'4994, in_bossfight);
            REF_EXE(0xb3'4998, boss_damage_idx);
            REF_EXE(0xb3'499c, boss_damage);
            REF_EXE(0xb3'3a88, in_conversation);
            REF_EXE(0xb3'49b8, last_enemy_id);
            REF_EXE(0xb3'49bc, last_enemy_group_id);
            REF_EXE(0xb3'35f0, in_results_screen);
            REF_EXE(0xb2'f960, bgm_track);
            REF_EXE(0xb2'f964, bgm_next_track);
            REF_EXE(0xb2'f968, bgm_next_loop);
            REF_EXE(0xb2'f96c, bgm_volume);
            REF_EXE(0xb2'f970, bgm_handle);
            PTR_EXE(0xb3'5c00, bg2);
            PTR_EXE(0xb3'5c50, bg3);
            PTR_EXE(0xb3'5cc0, bg4);
            PTR_EXE(0xb3'5d10, bg5);
            REF_EXE(0xb3'493c, sidebar_char);

            REF_EXE(0xb3'2a5c, in_cutscene);
            REF_EXE(0xb3'2a5d, cutscene_active);
            REF_EXE(0xb3'27c8, cutscene_delete);

            REF_EXE(0xb3'4910, pausemenu_isopen);

            REF_EXE(0xb1'3f38, screen_fade_active);
            REF_EXE(0xb1'3f3c, screen_fade_type);
            REF_EXE(0xb1'3f40, screen_fade_rate);
            REF_EXE(0xb1'3f44, screen_fade_progress);
            REF_EXE(0xb1'3f48, screen_fade_finished);

            PTR_EXE(0x4a'2c20, apply_bgm_volume);
            PTR_EXE(0x40'aff0, conversation_reset);

            PTR_EXE(0x4b'5890, SetDrawArea);
            PTR_EXE(0x4b'5b90, SetDrawAreaFull);
            PTR_EXE(0x4c'29c0, LoadGraph);
            PTR_EXE(0x4c'2e70, DrawLine);
            PTR_EXE(0x4f'5840, SubHandle);

            PTR_EXE(0x41'5c60, Orig::game_tick);
            break;
        default:
            return FALSE;
        }

        hook();
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
