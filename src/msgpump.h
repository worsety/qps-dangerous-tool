/* Generic Windows message pump exposing MsgWaitForMultipleObjectsEx functionality.
 * Can be modified while running and is thread safe.
 */
#pragma once
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX

// C++ can use MessagePump below which wraps a smart pointer with member functions
typedef struct MessagePump_s *MessagePumpPtr;
typedef void (*MessagePump_SyncObject_Callback)(HANDLE, void*);

#ifdef __cplusplus
extern "C" {
#endif
MessagePumpPtr MessagePump_new(void);
void MessagePump_delete(MessagePumpPtr);

// Returns WM_QUIT exit code or -1 on abnormal termination.
// Does not call MessagePump_delete before returning.
// You must reuse or delete the MessagePump yourself.
int MessagePump_run(MessagePumpPtr);

/* Monitors any of the handles MsgWaitForMultipleObjects accepts.
 * Neither takes ownership of the handle nor duplicates it, the caller
 * is still responsible for closing the handle and must call
 * MessagePump_remove_syncObject beforehand if the message pump is running.
 * Returns non-zero on success.
 */
int MessagePump_add_syncObject(MessagePumpPtr,
    HANDLE syncObject,
    MessagePump_SyncObject_Callback onSignalled,
    MessagePump_SyncObject_Callback onAbandoned,
    void *callbackParam);
void MessagePump_remove_syncObject(MessagePumpPtr, HANDLE);

// Returns non-zero on success
int MessagePump_add_acceleratorTable(MessagePumpPtr, HWND, HACCEL);
void MessagePump_remove_acceleratorTable(MessagePumpPtr, HWND, HACCEL);

// Returns non-zero on success
int MessagePump_add_modelessDialogBox(MessagePumpPtr, HWND);
void MessagePump_remove_modelessDialogBox(MessagePumpPtr, HWND);
#ifdef __cplusplus
};

#include <memory>
class MessagePump : std::unique_ptr<MessagePump_s, void (*)(MessagePumpPtr)> {
public:
    MessagePump()
        : std::unique_ptr<MessagePump_s, void (*)(MessagePumpPtr)>(MessagePump_new(), MessagePump_delete)
    {
    }
    int run() const
    {
        return MessagePump_run(get());
    }
    int add_syncObject(HANDLE syncObject,
        MessagePump_SyncObject_Callback onSignalled,
        MessagePump_SyncObject_Callback onAbandoned = nullptr,
        void *callbackParam = nullptr) const
    {
        return MessagePump_add_syncObject(get(), syncObject,
            onSignalled, onAbandoned, callbackParam);
    }
    void remove_syncObject(HANDLE syncObject) const
    {
        MessagePump_remove_syncObject(get(), syncObject);
    }
    int add_acceleratorTable(HWND window, HACCEL acceleratorTable) const
    {
        return MessagePump_add_acceleratorTable(get(), window, acceleratorTable);
    }
    void remove_acceleratorTable(HWND window, HACCEL acceleratorTable) const
    {
        MessagePump_remove_acceleratorTable(get(), window, acceleratorTable);
    }
    int add_modelessDialogBox(HWND window) const
    {
        return MessagePump_add_modelessDialogBox(get(), window);
    }
    void remove_modelessDialogBox(HWND window) const
    {
        return MessagePump_remove_modelessDialogBox(get(), window);
    }
};
#endif
