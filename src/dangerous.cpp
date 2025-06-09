#include <windows.h>
#include <detours.h>
#include <vector>
#include <array>
#include <memory>
#include "dangerous.h"
#include "mod.h"
#include "gui.h"
#include "util.h"

using namespace std;
using namespace QPSD;

HMODULE hPracticeDLL;
unsigned gameVersion; // bytes are game x.y.z

pseudo_ref<HWND> hwndGame;
static pseudo_ref<DWORD> idGameThread;

explicit_ptr<Player> qp;
static explicit_ptr<Vector<Enemy>> enemies;
static explicit_ptr<List<Bullet>> bullets;
static explicit_ptr<List<TempHitbox>> temphitboxes;
static explicit_ptr<List<Solid>> solids;
static explicit_ptr<List<Star>> stars;
static explicit_ptr<List<Shot>> shots;
static explicit_ptr<List<Laser>> lasers;
static explicit_ptr<List<Snake>> snakes;
static explicit_ptr<List<Sprite>> sprites;
static explicit_ptr<List<ScoreNotif>> scorenotifs;
static explicit_ptr<List<Balloon>> balloons;
static explicit_ptr<List<Caption>> captions;

static pseudo_ref<int> stage_num;
static pseudo_ref<int> game_frame;
static pseudo_ref<int> stage_end;
static pseudo_ref<int> stage_phase;
static pseudo_ref<int> stage_phase_frame;
static pseudo_ref<float> camera_x_scrolling_speed;
static pseudo_ref<char> controls_enabled;
static pseudo_ref<char> collision_enabled;
static pseudo_ref<char> damage_enabled;
static pseudo_ref<char> pause_enabled;
static pseudo_ref<char> shoot_enabled;
static pseudo_ref<char> chain_timer_active;
static pseudo_ref<int> chain_timer_multiplier;
static pseudo_ref<char> boss_break;
static pseudo_ref<char> boss_dead;
static pseudo_ref<char> boss_unkflag; // used by the final boss
static pseudo_ref<char> in_bossfight;
static pseudo_ref<int> boss_damage_idx;
static pseudo_ref<float> boss_damage;
static pseudo_ref<char> in_conversation;
static pseudo_ref<int> last_enemy_id;
static pseudo_ref<int> last_enemy_group_id; // only by a couple of enemies in the game
static pseudo_ref<char> in_results_screen;
static pseudo_ref<int> bgm_track;
static pseudo_ref<int> bgm_next_track;
static pseudo_ref<char> bgm_next_loop;
static pseudo_ref<int> bgm_volume;
static pseudo_ref<int> bgm_handle;

static explicit_ptr<BG2> bg2;
static explicit_ptr<BG3> bg3;
static explicit_ptr<BG4> bg4;
static explicit_ptr<BG5> bg5;

static pseudo_ref<SideBarChar[2]> sidebar_char;

static pseudo_ref<char> in_cutscene;
static pseudo_ref<char[8]> cutscene_active;
static explicit_ptr<void (*)()> cutscene_delete;

static pseudo_ref<char> pausemenu_isopen;

pseudo_ref<int> gameMode;
static pseudo_ref<char> screen_fade_active;
static pseudo_ref<int> screen_fade_type;
static pseudo_ref<float> screen_fade_rate;
static pseudo_ref<float> screen_fade_progress;
static pseudo_ref<char> screen_fade_finished;

static pseudo_ref<char[35][4]> trophy_flags;
static char trophy_flags_snapshot[35][4];
static bool trophy_flags_recorded;

static explicit_ptr<void ()> game_draw;
static explicit_ptr<void ()> menu_draw;
static explicit_ptr<void ()> apply_bgm_volume;
static explicit_ptr<void ()> conversation_reset;

static explicit_ptr<int (const wchar_t *FileName, int NotUse3DFlag)> LoadGraph;
static explicit_ptr<int (int Handle)> SubHandle;
static explicit_ptr<int (int x1, int y1, int x2, int y2)> SetDrawArea;
static explicit_ptr<int ()> SetDrawAreaFull;
static explicit_ptr<int (int x1, int y1, int x2, int y2, int Color, int Thickness)> DrawLine;

// For testing and the future
static void draw_overlay()
{
}

static void set_bgm(int track, bool instant = false, bool restart = false)
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

static void fix_sidebars()
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

static void cutscenes_delete()
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
static void fix_aesthetics()
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
static void copy(std::vector<T> &dst, const Vector<T> &src)
{
    size_t n = src.size();
    dst.resize(n);
    if (n)
        memcpy(&dst[0], src.start, n * sizeof(T));
}

template<typename T>
static void copy(Vector<T> &dst, const std::vector<T> &src)
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
static void copy(std::vector<T> &dst, const List<T> &src)
{
    size_t n = src.size;
    dst.resize(n);
    size_t i = 0;
    for (ListNode<T> *node = src.front->next; node != src.front; node = node->next)
        memcpy(&dst[i++], &node->value, sizeof(T));
}

template<typename T>
static void copy(List<T> &dst, const std::vector<T> &src)
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

static void reset_most_state()
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

static void cycle_hyper_charge()
{
    if (qp->fHyper || qp->formation == 49)
        return;
    if (qp->degHyperCharge >= 360.f)
        qp->degHyperCharge = 0.f;
    else if (qp->degHyperCharge >= 240.f)
        qp->degHyperCharge = 360.f;
    else
        qp->degHyperCharge = 240.f;
}

static int seek_recency;

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
            qp->degHyperCharge = 0.f;
            qp->chain = 0;
            qp->tfChainTimer = 0;
        }
        fix_aesthetics();
        seek_recency = 60;
    }
};

// Frequently flags are set at the end of a phase and
// sometimes even a boss is spawned for use in the next phase
const Checkpoint checkpoints[] = {
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
static int effective_phase()
{
    if (stage_num == 2 && stage_phase == 13)
        return 3;
    if (stage_num == 3 && stage_phase == 13)
        return 4;
    return stage_phase;
}

static void seek_backward()
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

static void seek_forward()
{
    auto phase = effective_phase();
    for (auto &c : checkpoints) {
        if (c.stage == stage_num
            && c.phase != 1 // You probably aren't trying to skip the title card
            && (c.phase > phase || c.phase == phase && c.frame > stage_phase_frame))
            return c.go();
    }
}

static void return_to_main_menu()
{
    if (4 != gameMode)
        return;
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
static bool key_struck(int vk)
{
    return (keys[keys_idx][vk] & 0x80)
        && !(keys[!keys_idx][vk] & 0x80);
}

static void process_hotkeys(bool in_game)
{
    GetKeyboardState(keys[keys_idx]);
    if (in_game) {
        if (key_struck(VK_F1)) {
            if (in_cutscene)
                cutscenes_delete();
            else if (in_conversation)
                conversation_reset();
            else
                for (Enemy *enemy = enemies->start; enemy != enemies->finish; enemy++)
                    if (11 == enemy->id)
                        enemy->nHP = 0;
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
    }
    if (key_struck(VK_F9))
        gui_open();
    keys_idx = !keys_idx;
}

// thiscall requires a member function but fastcall is compatible
namespace Orig {
    static explicit_ptr<void __cdecl ()> game_tick;
    static explicit_ptr<void __cdecl ()> menu_tick;
    static explicit_ptr<char __fastcall (void*, int, int, int)> leaderboard_upload;
    static explicit_ptr<void __fastcall (int, int, void*)> rankings_insert;
    static explicit_ptr<void __cdecl ()> game_sys_save;
    static explicit_ptr<char __cdecl ()> game_sys_save2; // does something else first
    static explicit_ptr<char __fastcall (void*, int, int)> grant_achievement;
}

namespace Hook {
    static void game_tick()
    {
        if (freezeGame) {
            game_draw();
            return;
        }
        if (!trophy_flags_recorded) {
            memcpy(trophy_flags_snapshot, trophy_flags, sizeof trophy_flags_snapshot);
            trophy_flags_recorded = true;
        }
        if (seek_recency > 0)
            --seek_recency;
        process_hotkeys(true);
        // Fade music back in if we revert music during a fade out
        if (bgm_handle && bgm_next_track == bgm_track && bgm_volume < 128) {
            bgm_volume++;
            apply_bgm_volume();
        }

        Orig::game_tick();
        draw_overlay();
    }

    static void menu_tick()
    {
        if (freezeGame) {
            menu_draw();
            return;
        }
        process_hotkeys(false);
        Orig::menu_tick();
    }

    static char __fastcall leaderboard_upload(void *obj, int edx, int leaderboard_idx, int score)
    {
        return 0;
    }

    static void __fastcall rankings_insert(int difficulty, int edx, void *score)
    {
    }

    static void game_sys_save()
    {
        if (trophy_flags_recorded)
            memcpy(trophy_flags, trophy_flags_snapshot, sizeof trophy_flags_snapshot);
        Orig::game_sys_save();
    }

    static char game_sys_save2()
    {
        if (trophy_flags_recorded)
            memcpy(trophy_flags, trophy_flags_snapshot, sizeof trophy_flags_snapshot);
        return Orig::game_sys_save2();
    }

    static char __fastcall grant_achievement(void *obj, int edx, int idx)
    {
        return 0;
    }
}

bool brokenBackgrounds;

static bool patchCode(void *dst, const void *src, size_t size)
{
    DWORD permOrig, permDiscard;
    if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &permOrig))
        return false;
    CopyMemory(dst, src, size);
    FlushInstructionCache(GetCurrentProcess(), dst, size);
    VirtualProtect(dst, size, permOrig, &permDiscard);
    return true;
}

const struct {
    const wchar_t *ja, *en;
} backgroundPaths[] = {
    { L".\\model\\3D\\街灯明かり.bmp", L".\\model\\3D\\streetlamplight.bmp" },
    { L".\\model\\3D\\夜空.bmp", L".\\model\\3D\\unused_nightsky.bmp" },
    { L".\\model\\3D\\風.bmp", L".\\model\\3D\\unused_wind.bmp" },
};

// jmp 0x499cb0
// lea esp, [esp+0] ;nop
// lea ecx, [ecx+0] ;nop
const unsigned char bg3_init_codeOrig[] = {
    0xeb, 0x0a, 0x8d, 0xa4, 0x24, 0x00, 0x00, 0x00, 0x00, 0x8d, 0x49, 0x00
};
// ; Initialize uninitialized memory with 100f countdown for lamps lighting
// ; at night. Zero means currently lit.  Any non-zero value fixes the initial
// ; lamps, but new lamps get assigned a random time from 100f to 500f.
// mov dword ptr [esp+34h], 100
// jmp 0x499cb0
// nop2
const unsigned char bg3_init_codePatch[] = {
    0xc7, 0x44, 0x24, 0x34, 0x64, 0x00, 0x00, 0x00, 0xeb, 0x02, 0x66, 0x90
};

bool fix_backgrounds()
{
    if (!brokenBackgrounds)
        return false;

    // We'll be suspending this thread, call from the GUI thread
    if (GetCurrentThreadId() == idGameThread)
        return false;

    pseudo_ref<wchar_t*> bgPathRefs[3];
    explicit_ptr<void> bg3_init_patchPoint;

    switch (gameVersion) {
    case 0x04'01'04'01:
        REF_EXE(0x49'9c22, bgPathRefs[0]);
        REF_EXE(0x49'ef00, bgPathRefs[1]);
        REF_EXE(0x49'ef11, bgPathRefs[2]);
        PTR_EXE(0x49'9ca4, bg3_init_patchPoint);
        break;
    default:
        return false;
    }

    SmartHandle gameThread(OpenThread(THREAD_SUSPEND_RESUME, FALSE, idGameThread), CloseHandle);
    SuspendThread(gameThread.get());

    wchar_t *newString = nullptr;
    int cBrokenBG = 3;
    for (int i = 0; i < 3; i++) {
        auto &bg = backgroundPaths[i];
        for (auto path : {bg.ja, bg.en}) {
            int hGraph = LoadGraph(path, 0);
            if (-1 == hGraph)
                continue;
            SubHandle(hGraph);
            cBrokenBG--;
            if (!lstrcmp(path, bgPathRefs[i]))
                break;
            if (!newString)
                newString = reinterpret_cast<wchar_t*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256));
            if (!newString) {
                ResumeThread(gameThread.get());
                return false;
            }
            if (lstrcpy(newString, path)) {
                if (!patchCode(
                    reinterpret_cast<void*>(&bgPathRefs[i]),
                    reinterpret_cast<const void*>(&newString),
                    sizeof(wchar_t*)
                )) {
                    cBrokenBG++;
                    continue;
                }
                newString += lstrlen(newString) + 1;
            }
            break;
        }
    }

    bool lampsBroken = true;
    if (!memcmp(bg3_init_patchPoint, bg3_init_codePatch, sizeof(bg3_init_codePatch)))
        lampsBroken = false;
    else if (!memcmp(bg3_init_patchPoint, bg3_init_codeOrig, sizeof(bg3_init_codeOrig))) {
        if (patchCode(
            bg3_init_patchPoint,
            reinterpret_cast<const void*>(bg3_init_codePatch),
            sizeof(bg3_init_codePatch)
        ))
            lampsBroken = false;
    }

    ResumeThread(gameThread.get());

    brokenBackgrounds = false; // don't try again even there was a problem
    return !cBrokenBG && !lampsBroken;
}

bool freezeGame;

static bool hooked;

const DWORD detoursThreadAccess = THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT;

bool hook()
{
    SmartHandle gameThread(OpenThread(detoursThreadAccess, FALSE, idGameThread), CloseHandle);
    DetourTransactionBegin();
    if (gameThread)
        DetourUpdateThread(gameThread.get());
    DetourAttach(&(PVOID&)Orig::game_tick.ptr, Hook::game_tick);
    DetourAttach(&(PVOID&)Orig::menu_tick.ptr, Hook::menu_tick);
    DetourAttach(&(PVOID&)Orig::leaderboard_upload.ptr, Hook::leaderboard_upload);
    DetourAttach(&(PVOID&)Orig::rankings_insert.ptr, Hook::rankings_insert);
    DetourAttach(&(PVOID&)Orig::game_sys_save.ptr, Hook::game_sys_save);
    DetourAttach(&(PVOID&)Orig::game_sys_save2.ptr, Hook::game_sys_save2);
    DetourAttach(&(PVOID&)Orig::grant_achievement.ptr, Hook::grant_achievement);
    return hooked = NO_ERROR == DetourTransactionCommit();
}

void unhook()
{
    if (!hooked)
        return;
    return_to_main_menu();
    SmartHandle gameThread(OpenThread(detoursThreadAccess, FALSE, idGameThread), CloseHandle);
    DetourTransactionBegin();
    if (gameThread)
        DetourUpdateThread(gameThread.get());
    DetourDetach(&(PVOID&)Orig::game_tick.ptr, Hook::game_tick);
    DetourDetach(&(PVOID&)Orig::menu_tick.ptr, Hook::menu_tick);
    DetourDetach(&(PVOID&)Orig::leaderboard_upload.ptr, Hook::leaderboard_upload);
    DetourDetach(&(PVOID&)Orig::rankings_insert.ptr, Hook::rankings_insert);
    DetourDetach(&(PVOID&)Orig::game_sys_save.ptr, Hook::game_sys_save);
    DetourDetach(&(PVOID&)Orig::game_sys_save2.ptr, Hook::game_sys_save2);
    DetourDetach(&(PVOID&)Orig::grant_achievement.ptr, Hook::grant_achievement);
    DetourTransactionCommit();
    if (trophy_flags_recorded)
        memcpy(trophy_flags, trophy_flags_snapshot, sizeof trophy_flags_snapshot);
    hooked = false;
}

static SmartHandle runOnceMutex(nullptr, CloseHandle);

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
    {
        // Prevent accidentally loading two versions of the DLL
        wchar_t mutexName[32] = L"ShootingPractice_";
        DWORD procId = GetProcessId(GetCurrentProcess());
        // Keeping to kernel32 although by the time this is injected it should be fine not to
        for (wchar_t *p = mutexName + lstrlen(mutexName); procId; procId >>= 4)
            *p++ = L'A' + (procId & 15);
        runOnceMutex.reset(CreateMutex(nullptr, TRUE, mutexName));
        if (!runOnceMutex)
            return FALSE;
        if (ERROR_ALREADY_EXISTS == GetLastError())
            return FALSE;

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
            gameVersion = 0x04'01'04'01;
            brokenBackgrounds = true;

            PTR_EXE(0x84'88a4, hwndGame);
            REF_EXE(0x84'88cc, idGameThread);

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

            REF_EXE(0xb1'42dc, gameMode);
            REF_EXE(0xb1'3f38, screen_fade_active);
            REF_EXE(0xb1'3f3c, screen_fade_type);
            REF_EXE(0xb1'3f40, screen_fade_rate);
            REF_EXE(0xb1'3f44, screen_fade_progress);
            REF_EXE(0xb1'3f48, screen_fade_finished);

            REF_EXE(0xb3'2c2e, trophy_flags);

            PTR_EXE(0x41'3c10, game_draw);
            PTR_EXE(0x4a'33e0, menu_draw);
            PTR_EXE(0x4a'2c20, apply_bgm_volume);
            PTR_EXE(0x40'aff0, conversation_reset);

            PTR_EXE(0x4b'5890, SetDrawArea);
            PTR_EXE(0x4b'5b90, SetDrawAreaFull);
            PTR_EXE(0x4c'29c0, LoadGraph);
            PTR_EXE(0x4c'2e70, DrawLine);
            PTR_EXE(0x4f'5840, SubHandle);

            PTR_EXE(0x41'5c60, Orig::game_tick);
            PTR_EXE(0x4a'4dd0, Orig::menu_tick);
            PTR_EXE(0x40'2ab0, Orig::leaderboard_upload);
            PTR_EXE(0x41'06d0, Orig::rankings_insert);
            PTR_EXE(0x41'2ac0, Orig::game_sys_save);
            PTR_EXE(0x41'3340, Orig::game_sys_save2);
            PTR_EXE(0x40'2f00, Orig::grant_achievement);

            hPracticeDLL = hModule;
            break;
        default:
            return FALSE;
        }

        if (!hook())
            return FALSE;
        gui_run();
        break;
    }
    case DLL_PROCESS_DETACH:
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}
