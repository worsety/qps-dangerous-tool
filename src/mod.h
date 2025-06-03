#include <windows.h>
#include <cstddef>

// Convenience types to simplify host program data access
// without initializing a reference at declaration time

template<typename T>
class pseudo_ref {
    T *ptr = nullptr;
public:
    typedef T value_type;
    typedef T *ptr_type;
    void set(T *ptr) { this->ptr = ptr; }
    operator T&() const { return *ptr; }
    T* operator&() const { return ptr; }
    T& operator=(const T rhs) { return *ptr = rhs; }
};

template<typename T>
class explicit_ptr {
    T* ptr = nullptr;
public:
    typedef T value_type;
    typedef T *ptr_type;
    void set(T *ptr) { this->ptr = ptr; }
    operator T*() const { return ptr; }
    T* operator->() const { return ptr; }
};

#define PTR_MOD(moduleName, addr, ptr) do { (ptr).set(reinterpret_cast<decltype(ptr)::ptr_type>((char*)GetModuleHandle(moduleName) + addr)); } while (0)
#define REF_MOD(moduleName, addr, pRef) do { (pRef).set(reinterpret_cast<decltype(pRef)::ptr_type>((char*)GetModuleHandle(moduleName) + addr)); } while (0)
#define PTR_EXE(rva, ptr) PTR_MOD(nullptr, (rva) - 0x40'0000, ptr)
#define REF_EXE(rva, pRef) REF_MOD(nullptr, (rva) - 0x40'0000, pRef)
#define PTR_DLL(moduleName, rva, ptr) PTR_MOD(moduleName, (rva) - 0x1000'0000, ptr)
#define REF_DLL(moduleName, rva, pRef) REF_MOD(moduleName, (rva) - 0x1000'0000, pRef)
