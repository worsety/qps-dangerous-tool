#pragma once
#include <windows.h>

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
    union Enemy {
        struct {
            int id;
            char pad04[0x2c];
            int hp; // 0x30
        };
        char _raw[0xa4];
    };
    union Player {
        struct {
            char _pad000[8];
            int lives; // 0x008
            char _pad00c[-0x00c+0x238];
            int chain; // 0x238
            int max_chain; // 0x23c
            int chain_timer; // 0x240
            char _pad244[-0x244+0x26c];
            int shottype; // 0x26c
            char _pad270[-0x270+0x40c];
            float hyper_charge; // 0x40c, 240 = 100%
            char in_hyper; // 0x410
        };
        char _raw[0x42c];
        Player& operator=(const Player &src)
        {
            memcpy(this, &src, sizeof *this);
            return *this;
        }
    };
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
    };
};
