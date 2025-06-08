#pragma once
#define _X86_
#include <heapapi.h>
#include <array>
#include "mod.h"

namespace QPSD {
    // Uses libstdc++ maybe?
    template<typename T>
    struct ListNode {
        ListNode<T> *next;
        ListNode<T> *prev;
        T value;
    };
    template<typename T>
    struct List {
        ListNode<T> *front;
        size_t size;
        void clear()
        {
            ListNode<T> *next;
            HANDLE heap = GetProcessHeap();
            for (ListNode<T> *node = front->next; node != front; node = next)
            {
                next = node->next;
                HeapFree(heap, 0, node);
            }
            front->next = front;
            front->prev = front;
            size = 0;
        }
    };
    template<typename T>
    struct Vector {
        T *start;
        T *finish;
        T *end_of_storage;
        size_t size() const
        {
            return finish - start;
        }
        void clear()
        {
            finish = start;
        }
    };
    struct Pos {
        float x, y;
    };
    union Enemy {
        struct {
            int id;
            char _pad04[-0x04+0x28];
            Pos scrPos; // 0x28
            int nHP; // 0x30
            char _pad34[-0x34+0x78];
            int idParent; // 0x78
        };
        char _raw[0xa4];
    };
    struct Player {
        int iPlayer; // 0x000
        int tfLifetime; // 0x004
        int lives; // 0x008
        int unk00c; // 0x00c
        Pos scrPos; // 0x010
        Pos scrPrev; // 0x018
        // New positions are added at the head when you move unfocused,
        // while focused movement adjusts all positions.
        // I have no idea what this is for.
        Pos scrWeirdPosRingBuffer[60]; // 0x020
        int iWeirdPosRingBufferHead; // 0x200
        int state; // 0x204
        int formationSwitchMode; // 0x208
        int style; // 0x20c
        char fAutoSlowMovement; // 0x210
        float mulHitboxIndicatorSize; // 0x214
        // 24 auto slows with setting, set directly to 24 with focus
        int tfShootHeldOrFocus; // 0x218
        int tfContinueShooting; // 0x21c
        char fMoving; // 0x220
        float lerpHyperReadyAnim; // 0x224
        float lerpHyperAnim; // 0x228
        int tfIFrames; // 0x22c
        int stageScore; // 0x230
        int tempScore; // 0x234
        int chain; // 0x238
        int stageMaxChain; // 0x23c
        int tfChainTimer; // 0x240
        int unk244; // 0x244
        int pxPointsBoxSizeAdd; // 0x248
        int stageStarsBig; // 0x24c
        int stageStarsSmall; // 0x250
        int stageStarsGreen; // 0x254
        int stageBonusAcc; // 0x258
        // The 4th is Final
        int formations[4]; // 0x25c
        int formation; // 0x26c
        int iFormationActive; // 0x270
        int rbitProgress; // 0x274
        int cRbits; // 0x278
        Pos scrRbitAnchor; // 0x27c
        float radRbitAngle; // 0x284
        float pxRbitRadius; // 0x288
        struct Rbit {
            enum : int {
                RBIT_READY = 1,
                RBIT_MOVING = 2,
                RBIT_CAUGHT = 3,
            } state; // 0x00
            int unk04; // 0x04
            char fExists; // 0x08
            Pos scrPos; // 0x0c
            float radAim; // 0x14
            // unused? Initialized to same value as radAim
            float radUnk18; // 0x18
            int idEnemyCaughtBy; // 0x1c
        } rbits[12]; // 0x28c
        float degHyperCharge; // 0x40c, 240 = 100%
        char fHyper; // 0x410
        int pxHyperConversionRadius; // 0x414
        int extends; // 0x418
        int stageNonHyperFinishes; // 0x41c
        int stageHypers; // 0x420
        int stageMisses; // 0x424
        int stageHyperBreaks; // 0x428
        Player& operator=(const Player &src)
        {
            memcpy(this, &src, sizeof *this);
            return *this;
        }
    };
    static_assert(0x42c == sizeof(Player));
    // Some of these probably aren't strictly what I say here
    union Bullet {
        struct {
        };
        char _raw[0x38];
    };
    union TempHitbox {
        struct {
        };
        char _raw[0x20];
    };
    union Solid {
        struct {
        };
        char _raw[0x60];
    };
    union Star {
        struct {
        };
        char _raw[0x24];
    };
    union Shot {
        struct {
        };
        char _raw[0x2c];
    };
    union Laser {
        struct {
        };
        char _raw[0x18];
    };
    // The things the little ufos fire in stage 5
    union Snake {
        struct {
        };
        char _raw[0x280];
    };
    union Sprite {
        struct {
        };
        char _raw[0x30];
    };
    union ScoreNotif {
        struct {
        };
        char _raw[0x1c];
    };
    union Balloon {
        struct {
        };
        char _raw[0x18];
    };
    union Caption {
        struct {
        };
        char _raw[0x18];
    };

    enum STAGE_END : int {
        STAGE_END_CREDITS = 1,
        STAGE_END_NEXTSTAGE = 2,
        STAGE_END_DEAD = 3,
        STAGE_END_RETRY = 4,
        STAGE_END_SURRENDER = 5,
    };

    // Only naming members that have to be updated to jump to checkpoints
    // Stage 1's background is stateless
    struct BG2 {
        union {
            struct {
                int dw00, dw04, dw08;
                float f0c;
            };
            char _raw[0x10];
        } state;
        int handles[7];
        struct {
            Vector<std::array<int, 3>> v3_1;
            Vector<std::array<int, 3>> v3_2;
            Vector<std::array<int, 2>> v2;
        } geometry;
    };
    struct BG3 {
        union {
            struct {
                int dw00, dw04;
                char b08, b09;
                float f0c, f10, f14, f18, f1c, f20;
            };
            char _raw[0x24];
        } state;
        int handles[13];
        struct {
            Vector<std::array<int, 3>> v3;
            Vector<std::array<int, 4>> v4;
        } geometry;
    };
    struct BG4 {
        union {
            struct {
                int dw00, dw04;
                float f08, f0c, f10, f14;
                char b18;
                float f1c;
            };
            char _raw[0x20];
        } state;
        int handles[7];
        struct {
            List<std::array<int, 5>> l5;
            List<std::array<int, 2>> l2;
        } geometry;
    };
    struct BG5 {
        union {
            struct {
                int dw00, dw04;
                float f08, f0c, f10, f14, f18, f1c, f20, f24, f28, f2c, f30, f34;
            };
            char _raw[0x38];
        } state;
        int handles[9];
        struct {
            List<std::array<int, 5>> l5;
            union {
                // can't be bothered.  It's a bunch of floats then 3456 vertices
                char _raw[0x2882c];
            } wind;
        } geometry;
        int handles2[2];
    };
    struct SideBarChar {
        char visible;
        float opacity;
        int dw08;
        int state;
        int dw10;
        char _pad14[0x10];
        int texHandle[2];
    };
};

extern pseudo_ref<HWND> hwndGame;
extern pseudo_ref<int> gameMode;
extern explicit_ptr<QPSD::Player> qp;

extern HMODULE hPracticeDLL;
extern unsigned gameVersion;
extern bool freezeGame;
extern bool brokenBackgrounds;

void unhook();
bool fix_backgrounds();
