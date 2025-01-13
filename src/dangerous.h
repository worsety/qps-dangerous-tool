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
};
