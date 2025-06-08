// Works on x86 and x64 MSVC, GCC, and Clang as of 2025-06-08
#pragma once

namespace const_obfuscate {
    constexpr unsigned next_rand(const unsigned state)
    {
        return state * 1103515245 + 12345 & 0x7fff'ffff;
    }

    // I'd like to use std::array but MSVC is whining about constexpr
    template<typename T, size_t N>
    constexpr auto obfuscate(const T (&cleartext)[N], const unsigned seed) {
        struct { T data[N]; } ret{};
        for (unsigned i = 0, state = seed; i < N; i++, state = next_rand(state))
            ret.data[i] = cleartext[i] ^ state;
        return ret;
    }
    // A separate function which isn't declared constexpr plus
    // a volatile (for Clang) variable convinces all compilers
    // not to fold the entire expression for now.
    template<typename T, size_t N>
    auto deobfuscate(const volatile T (&cleartext)[N], const unsigned seed) {
        struct { T data[N]; } ret{};
        for (unsigned i = 0, state = seed; i < N; i++, state = next_rand(state))
            ret.data[i] = cleartext[i] ^ state;
        return ret;
    }
}

// MSVC needs the constexpr variable or the plaintext will
// appear in the output, GCC and Clang do not
#define OBF_DATA(array) ([&] { \
    constexpr auto _ciphertext{const_obfuscate::obfuscate(array, __LINE__)}; \
    const volatile auto _ciphertextCopy{_ciphertext}; \
    return const_obfuscate::deobfuscate(_ciphertextCopy.data, __LINE__); \
}().data)

#define OBF_FUNC(func) obf##func
#define OBF_IMPORT_DECL(func) static decltype(&func) OBF_FUNC(func)
#define OBF_IMPORT(module, func) do { \
    HMODULE hModule = LoadLibraryA(#module); \
    if (!hModule) ExitThread(1); \
    OBF_FUNC(func) = reinterpret_cast<decltype(OBF_FUNC(func))>(GetProcAddress(hModule, OBF_DATA(#func))); \
    CloseHandle(hModule); \
} while(0)
