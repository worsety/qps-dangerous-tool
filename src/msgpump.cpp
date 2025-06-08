#include "msgpump.h"
#include <windows.h>
#include <vector>
#include <list>
#include <new>

using namespace std;

struct MessagePump_s {
    struct SyncCallback {
        MessagePump_SyncObject_Callback onSignalled;
        MessagePump_SyncObject_Callback onAbandoned;
        void *param;
    };
    struct AcceleratorTable {
        HWND window;
        HACCEL acceleratorTable;
    };
    vector<HANDLE> syncObjects;
    vector<SyncCallback> syncCallbacks;
    vector<AcceleratorTable> acceleratorTables;
    vector<HWND> modelessDialogBoxes;
    CRITICAL_SECTION cs;
    MessagePump_s()
    {
        InitializeCriticalSection(&cs);
    }
    ~MessagePump_s()
    {
        DeleteCriticalSection(&cs);
    }
};

static bool initialized;
static CRITICAL_SECTION csMsgPumpList;
static list<MessagePump_s> messagePumps;

static void MessagePumps_init(void)
{
    if (initialized)
        return;
    InitializeCriticalSection(&csMsgPumpList);
    initialized = true;
}

MessagePumpPtr MessagePump_new(void)
{
    MessagePumpPtr ret;
    MessagePumps_init();
    EnterCriticalSection(&csMsgPumpList);
    try {
        messagePumps.emplace_front();
        ret = &messagePumps.front();
    } catch (bad_alloc&) {
        ret = nullptr;
    }
    LeaveCriticalSection(&csMsgPumpList);
    return ret;
}

void MessagePump_delete(MessagePumpPtr msgPump)
{
    MessagePumps_init();
    EnterCriticalSection(&csMsgPumpList);
    for (auto it = messagePumps.begin(); it != messagePumps.end(); ++it)
        if (&*it == msgPump) {
            messagePumps.erase(it);
            break;
        }
    LeaveCriticalSection(&csMsgPumpList);
}

int MessagePump_add_syncObject(MessagePumpPtr msgPump,
    HANDLE syncObject,
    MessagePump_SyncObject_Callback onSignalled,
    MessagePump_SyncObject_Callback onAbandoned,
    void *callbackParam)
{
    int ret;
    EnterCriticalSection(&msgPump->cs);
    try {
        if (msgPump->syncObjects.size() >= MAXIMUM_WAIT_OBJECTS - 1)
            ret = 0;
        else {
            msgPump->syncObjects.push_back(syncObject);
            msgPump->syncCallbacks.push_back({onSignalled, onAbandoned, callbackParam});
            ret = 1;
        }
    } catch (bad_alloc&) {
        if (msgPump->syncCallbacks.size() < msgPump->syncObjects.size())
            msgPump->syncObjects.erase(msgPump->syncObjects.end() - 1);
        ret = 0;
    }
    LeaveCriticalSection(&msgPump->cs);
    return ret;
}

void MessagePump_remove_syncObject(MessagePumpPtr msgPump, HANDLE syncObject)
{
    EnterCriticalSection(&msgPump->cs);
    int i = 0;
    for (auto it = msgPump->syncObjects.begin(); it != msgPump->syncObjects.end(); ++it, ++i)
        if (*it == syncObject) {
            msgPump->syncObjects.erase(it);
            msgPump->syncCallbacks.erase(msgPump->syncCallbacks.begin() + i);
            break;
        }
    LeaveCriticalSection(&msgPump->cs);
}

int MessagePump_add_acceleratorTable(MessagePumpPtr msgPump,
        HWND window, HACCEL acceleratorTable)
{
    int ret;
    EnterCriticalSection(&msgPump->cs);
    try {
        msgPump->acceleratorTables.push_back({window, acceleratorTable});
        ret = 1;
    } catch (bad_alloc&) {
        ret = 0;
    }
    LeaveCriticalSection(&msgPump->cs);
    return ret;
}


void MessagePump_remove_acceleratorTable(MessagePumpPtr msgPump,
        HWND window, HACCEL acceleratorTable)
{
    EnterCriticalSection(&msgPump->cs);
    for (auto it = msgPump->acceleratorTables.begin(); it != msgPump->acceleratorTables.end(); ++it)
        if (it->window == window && it->acceleratorTable == acceleratorTable) {
            msgPump->acceleratorTables.erase(it);
            break;
        }
    LeaveCriticalSection(&msgPump->cs);
}

int MessagePump_add_modelessDialogBox(MessagePumpPtr msgPump,
        HWND window)
{
    int ret = 0;
    EnterCriticalSection(&msgPump->cs);
    try {
        msgPump->modelessDialogBoxes.push_back(window);
        ret = 1;
    } catch (bad_alloc&) {
        ret = 0;
    }
    LeaveCriticalSection(&msgPump->cs);
    return ret;
}


void MessagePump_remove_modelessDialogBox(MessagePumpPtr msgPump,
        HWND window)
{
    EnterCriticalSection(&msgPump->cs);
    for (auto it = msgPump->modelessDialogBoxes.begin(); it != msgPump->modelessDialogBoxes.end(); ++it)
        if (*it == window) {
            msgPump->modelessDialogBoxes.erase(it);
            break;
        }
    LeaveCriticalSection(&msgPump->cs);
}

int MessagePump_run(MessagePumpPtr msgPump)
{
    int ret;
    MSG msg;
    while (true) {
        EnterCriticalSection(&msgPump->cs);
        size_t nWaitObjects = msgPump->syncObjects.size();
        DWORD wakeReason = MsgWaitForMultipleObjectsEx(
            nWaitObjects,
            msgPump->syncObjects.data(),
            INFINITE,
            QS_ALLINPUT,
            MWMO_INPUTAVAILABLE
        );
        if (wakeReason == WAIT_OBJECT_0 + nWaitObjects) {
            if (!PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
                goto handled;
            for (auto &accel : msgPump->acceleratorTables)
                if (TranslateAccelerator(accel.window, accel.acceleratorTable, &msg))
                    goto handled;
            for (auto window : msgPump->modelessDialogBoxes)
                if (IsDialogMessage(window, &msg))
                    goto handled;
            LeaveCriticalSection(&msgPump->cs);
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (WM_QUIT == msg.message) {
                ret = msg.wParam;
                break;
            }
            continue;
        } else if (wakeReason >= WAIT_OBJECT_0
                && wakeReason < WAIT_OBJECT_0 + nWaitObjects) {
            auto i = wakeReason - WAIT_OBJECT_0;
            auto &cb = msgPump->syncCallbacks[i];
            if (cb.onSignalled) {
                auto syncObject = msgPump->syncObjects[i];
                cb.onSignalled(syncObject, cb.param);
            }
        } else if (wakeReason >= WAIT_ABANDONED_0
                && wakeReason < WAIT_ABANDONED_0 + nWaitObjects) {
            auto i = wakeReason - WAIT_ABANDONED_0;
            auto &cb = msgPump->syncCallbacks[i];
            if (cb.onAbandoned) {
                auto syncObject = msgPump->syncObjects[i];
                cb.onAbandoned(syncObject, cb.param);
            }
        } else if (wakeReason == WAIT_IO_COMPLETION) {
            // I cannot think of any earthly reason you would care,
            // this is what completion routines are for.
        } else {
            // WAIT_FAILED or something about the API changed
            ret = -1;
            LeaveCriticalSection(&msgPump->cs);
            break;
        }
handled:
        LeaveCriticalSection(&msgPump->cs);
    }
    return ret;
}
