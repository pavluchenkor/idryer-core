#pragma once

// Lightweight callback wrapper — fn pointer + void* context.
// Replaces std::function to avoid heap allocation on microcontrollers.
//
// Usage:
//   Callback<void(const char*, int)> cb;
//   cb.set([](void* ctx, const char* s, int n) {
//       static_cast<MyClass*>(ctx)->handle(s, n);
//   }, this);
//   if (cb) cb("hello", 42);

namespace idryer {

template<typename Sig>
class Callback;

// ── void return ──────────────────────────────────────────────────────────────
template<typename... Args>
class Callback<void(Args...)> {
public:
    using FnPtr = void(*)(void*, Args...);

    void set(FnPtr fn, void* ctx = nullptr) { fn_ = fn; ctx_ = ctx; }
    void reset()                             { fn_ = nullptr; ctx_ = nullptr; }
    void operator()(Args... args)   const    { if (fn_) fn_(ctx_, args...); }
    explicit operator bool()        const    { return fn_ != nullptr; }

private:
    FnPtr fn_  = nullptr;
    void* ctx_ = nullptr;
};

// ── non-void return ──────────────────────────────────────────────────────────
template<typename Ret, typename... Args>
class Callback<Ret(Args...)> {
public:
    using FnPtr = Ret(*)(void*, Args...);

    void set(FnPtr fn, void* ctx = nullptr) { fn_ = fn; ctx_ = ctx; }
    void reset()                             { fn_ = nullptr; ctx_ = nullptr; }
    Ret  operator()(Args... args)   const    { return fn_ ? fn_(ctx_, args...) : Ret{}; }
    explicit operator bool()        const    { return fn_ != nullptr; }

private:
    FnPtr fn_  = nullptr;
    void* ctx_ = nullptr;
};

} // namespace idryer
