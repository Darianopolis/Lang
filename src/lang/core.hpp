#pragma once

#include <utility>
#include <format>
#include <iostream>
#include <stdexcept>
#include <print>
#include <chrono>
#include <fstream>
#include <filesystem>

#include <string.h>

#define NAMESPACE_BEGIN(name) namespace name {
#define NAMESPACE_SWITCH(name) } namespace name {
#define NAMESPACE_END }
#define NAMESPACE_USE(name) using namespace name;

NAMESPACE_BEGIN(lang)

// -----------------------------------------------------------------------------

using namespace std::literals;

using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;

using i64 = int64_t;
using i32 = int32_t;
using i16 = int16_t;
using i8 = int8_t;

using usz = size_t;
using isz = i64;

using c32 = char32_t;

using byte = std::byte;

using f32 = float;
using f64 = double;

// -----------------------------------------------------------------------------

#define LANG_forceinline __attribute__((always_inline))
#define LANG_noinline __attribute__((noinline))

// -----------------------------------------------------------------------------
template<typename... Ts>
struct overload_set : Ts... {
    using Ts::operator()...;
};

template<typename... Ts> overload_set(Ts...) -> overload_set<Ts...>;

// -----------------------------------------------------------------------------

template<typename Fn>
struct Defer
{
    Fn fn;

    Defer(Fn&& fn): fn(std::move(fn)) {}
    ~Defer() { fn(); };
};

#define defer Defer _ = [&]

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//     Logging
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

#define VT_color_begin(color) "\u001B[" #color "m"
#define VT_color_reset "\u001B[0m"
#define VT_color(color, text) VT_color_begin(color) text VT_color_reset

#define VT_save_cursor "\0337"
#define VT_restore_cursor "\0338"
#define VT_move_prev_line "\033[F"
#define VT_move_down "\033[B"

enum class LogLevel
{
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
};

inline
LogLevel log_level = LogLevel::Trace;

struct LogLevelGuard
{
    LogLevel old_level;

    LogLevelGuard(LogLevel new_level)
        : old_level(log_level)
    {
        log_level = new_level;
    }

    ~LogLevelGuard()
    {
        log_level = old_level;
    }

    LogLevelGuard(const LogLevelGuard&) = delete;
    LogLevelGuard(LogLevelGuard&&) = delete;
};

template<typename ...Args>
void log_trace(std::format_string<Args...> fmt, Args&&... args)
{
    if (log_level > LogLevel::Trace) return;
    std::println("[" VT_color(90, "TRACE") "] " VT_color(90, "{}"), std::vformat(fmt.get(), std::make_format_args(args...)));
}

template<typename ...Args>
void log_debug(std::format_string<Args...> fmt, Args&&... args)
{
    if (log_level > LogLevel::Debug) return;
    std::println("[" VT_color(96, "DEBUG") "] {}", std::vformat(fmt.get(), std::make_format_args(args...)));
}

template<typename ...Args>
void log_info(std::format_string<Args...> fmt, Args&&... args)
{
    if (log_level > LogLevel::Info) return;
    std::println(" [" VT_color(94, "INFO") "] {}", std::vformat(fmt.get(), std::make_format_args(args...)));
}

template<typename ...Args>
void log_warn(std::format_string<Args...> fmt, Args&&... args)
{
    if (log_level > LogLevel::Warn) return;
    std::println(" [" VT_color(93, "WARN") "] {}", std::vformat(fmt.get(), std::make_format_args(args...)));
}

template<typename ...Args>
void log_error(std::format_string<Args...> fmt, Args&&... args)
{
    if (log_level > LogLevel::Error) return;
    std::println("[" VT_color(91, "ERROR") "] {}", std::vformat(fmt.get(), std::make_format_args(args...)));
}

// -----------------------------------------------------------------------------

inline
std::string indent_str(u32 depth, const char* indent = ". ")
{
    std::string out;
    for (u32 i = 0; i < depth; ++i) {
        out += indent;
    }
    return out;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//     Error Reporting
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

template<typename ...Args>
[[noreturn]]
void raise_error(std::format_string<Args...> fmt, Args&&... args)
{
    auto message = std::vformat(fmt.get(), std::make_format_args(args...));
    log_error("{}", message);
    std::terminate();
    // throw std::runtime_error(message);
}

[[noreturn]]
inline
void raise_unix_error(std::string_view message, int err = 0)
{
    err = err ?: errno;

    if (message.empty()) raise_error("({}) {}",              err, strerror(err));
    else                 raise_error("{}: ({}) {}", message, err, strerror(err));
}

enum class UnixErrorBehaviour {
    RetNull,
    RetNeg1,
    RetNegErrno,
    CheckErrno,
};

template<typename T>
T unix_check_(T res, UnixErrorBehaviour check, auto... allowed)
{
    bool error_occured = false;
    int error_code = 0;

    switch (check) {
        case UnixErrorBehaviour::RetNull:     if (!res)      { error_occured = true; error_code = errno; } break;
        case UnixErrorBehaviour::RetNeg1:     if (res == -1) { error_occured = true; error_code = errno; } break;
        case UnixErrorBehaviour::RetNegErrno: if (res < 0)   { error_occured = true; error_code = -res;  } break;
        case UnixErrorBehaviour::CheckErrno:  if (errno)     { error_occured = true; error_code = errno; } break;
    }

    if (!error_occured || (... || (error_code == allowed))) return res;

    raise_unix_error("unix_check", error_code);
}

#define unix_check_null(func, ...)                         unix_check_((func), UnixErrorBehaviour::RetNull     __VA_OPT__(,) __VA_ARGS__)
#define unix_check_n1(  func, ...)                         unix_check_((func), UnixErrorBehaviour::RetNeg1     __VA_OPT__(,) __VA_ARGS__)
#define unix_check_ne(  func, ...)                         unix_check_((func), UnixErrorBehaviour::RetNegErrno __VA_OPT__(,) __VA_ARGS__)
#define unix_check_ce(  func, ...) [&] { errno = 0; return unix_check_((func), UnixErrorBehaviour::CheckErrno  __VA_OPT__(,) __VA_ARGS__); }()

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//     Resource Management
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// template<typename T, usz Size, usz Alignment = 16>
// struct AlignedStorage
// {
//     using Type = T;

//     alignas(Alignment) std::byte data[Size];
// };

// #define get_impl(storage) reinterpret_cast<decltype(storage)::Type*>(_storage.data)
// #define decl_self(storage) auto* self = get_impl(storage)

struct RefCounted
{
    uint32_t ref_count = 1;

    template<std::derived_from<RefCounted> T>
    friend auto* ref(T* v)
    {
        v->ref_count++;
        return v;
    }

    template<std::derived_from<RefCounted> T>
    friend void unref(T* v)
    {
        if (!v) return;
        if (!--v->ref_count) {
            T::destroy(v);
        }
    }

    template<std::derived_from<RefCounted> T>
    friend T* take(T*& v)
    {
        T* t = v;
        v = nullptr;
        return t;
    }
};

inline
int take_fd(int& fd)
{
    int _fd = fd;
    fd = -1;
    return _fd;
}

#define get_impl(var) static_cast<std::remove_cvref_t<decltype(*var)>::Impl*>(var)

#define decl_self_nullable(var) auto* self = get_impl(var)
#define decl_self(var) decl_self_nullable(var); if (!self) raise_error("self: nullptr")

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//     File I/O
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

inline
std::string read_file(std::string_view path)
{
    std::ifstream in(std::filesystem::path(path), std::ios::in | std::ios::binary | std::ios::ate);
    if(!in.is_open()) {
        raise_error("failed to open file [{}]", path);
    }
    auto len = in.tellg();
    in.seekg(0);
    std::string message;
    message.resize_and_overwrite(len, [&](char *data, usz size) {
        in.read(data, size);
        return size;
    });
    return message;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//      Printing
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

inline
std::string ascii_debug_string(char c)
{
    switch (c) {
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\e': return "\\e";
        case '\a': return "\\a";
        case '\b': return "\\b";
        case ' ': return "' '";
        case ' ' + 1 ... 127: return std::string{ c };
        case 128:             return "DEL";
        default: return std::format("{{{}}}", int(c));
    }
}

namespace detail {
    inline
    u32 decimals_for_3sf(f64 value)
    {
        if (value < 10) return 2;
        if (value < 100) return 1;
        return 0;
    }

    inline
    std::string print_value_with_suffix(std::string_view suffix, f64 amount, u32 decimals)
    {
        switch (decimals) {
            case 0: return std::format("{:.0f}{}", amount, suffix);
            case 1: return std::format("{:.1f}{}", amount, suffix);
            case 2: return std::format("{:.2f}{}", amount, suffix);
            default: std::unreachable();
        }
    }
}

inline
std::string duration_to_string(std::chrono::duration<f64, std::nano> dur)
{
	f64 nanos = dur.count();

	if (nanos >= 1e9) {
		f64 seconds = nanos / 1e9;
		return detail::print_value_with_suffix("s", seconds, detail::decimals_for_3sf(seconds));
	}

	if (nanos >= 1e6) {
		f64 millis = nanos / 1e6;
		return detail::print_value_with_suffix("ms", millis, detail::decimals_for_3sf(millis));
	}

	if (nanos >= 1e3) {
		f64 micros = nanos / 1e3;
		return detail::print_value_with_suffix("us", micros, detail::decimals_for_3sf(micros));
	}

	if (nanos >= 0) {
		return detail::print_value_with_suffix("ns", nanos, detail::decimals_for_3sf(nanos));
	}

	return "0";
}

inline
std::string byte_size_to_string(u64 bytes)
{
	constexpr auto Exabyte   = usz(1) << 60;
	if (bytes >= Exabyte) {
		f64 exabytes = bytes / static_cast<f64>(Exabyte);
		return detail::print_value_with_suffix("EiB", exabytes, detail::decimals_for_3sf(exabytes));
	}

	constexpr auto Petabyte  = usz(1) << 50;
	if (bytes >= Petabyte) {
		f64 petabytes = bytes / static_cast<f64>(Petabyte);
		return detail::print_value_with_suffix("PiB", petabytes, detail::decimals_for_3sf(petabytes));
	}

	constexpr auto Terabyte  = usz(1) << 40;
	if (bytes >= Terabyte) {
		f64 terabytes = bytes / static_cast<f64>(Terabyte);
		return detail::print_value_with_suffix("TiB", terabytes, detail::decimals_for_3sf(terabytes));
	}

	constexpr auto Gigabyte = usz(1) << 30;
	if (bytes >= Gigabyte) {
		f64 gigabytes = bytes / static_cast<f64>(Gigabyte);
		return detail::print_value_with_suffix("GiB", gigabytes, detail::decimals_for_3sf(gigabytes));
	}

	constexpr auto Megabyte = usz(1) << 20;
	if (bytes >= Megabyte) {
		f64 megabytes = bytes / static_cast<f64>(Megabyte);
		return detail::print_value_with_suffix("MiB", megabytes, detail::decimals_for_3sf(megabytes));
	}

	constexpr auto Kilobyte = usz(1) << 10;
	if (bytes >= Kilobyte) {
		f64 kilobytes = bytes / static_cast<f64>(Kilobyte);
		return detail::print_value_with_suffix("KiB", kilobytes, detail::decimals_for_3sf(kilobytes));
	}

	if (bytes > 0) {
		return std::format("{} byte{}", static_cast<f64>(bytes), bytes == 1 ? "" : "s");
	}

	return "0 bytes";
}

NAMESPACE_END
