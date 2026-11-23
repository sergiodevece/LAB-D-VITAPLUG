// VitaPlug LAB-D · marco mínimo de pruebas (sin dependencias).
// Cada caso imprime PASS/FAIL y sus medidas ("evidence") para el informe.
#pragma once

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace labdtest {

struct Case
{
    const char* name;
    void (*fn)();
};

inline std::vector<Case>& registry()
{
    static std::vector<Case> r;
    return r;
}

struct Registrar
{
    Registrar (const char* n, void (*f)()) { registry().push_back ({ n, f }); }
};

inline int& failures()
{
    static int f = 0;
    return f;
}

inline void fail (const char* file, int line, const std::string& msg)
{
    ++failures();
    std::printf ("    FAIL %s:%d: %s\n", file, line, msg.c_str());
}

inline void evidence (const char* fmt, ...) __attribute__ ((format (printf, 1, 2)));
inline void evidence (const char* fmt, ...)
{
    std::printf ("    · ");
    va_list args;
    va_start (args, fmt);
    std::vprintf (fmt, args);
    va_end (args);
    std::printf ("\n");
}

inline int runAll (int argc, char** argv)
{
    const char* filter = argc > 1 ? argv[1] : "";
    int run = 0, failed = 0;
    for (const auto& c : registry())
    {
        if (std::strncmp (c.name, filter, std::strlen (filter)) != 0)
            continue;
        ++run;
        const int before = failures();
        std::printf ("[ RUN  ] %s\n", c.name);
        std::fflush (stdout);
        c.fn();
        const bool ok = failures() == before;
        if (! ok)
            ++failed;
        std::printf ("[%s] %s\n", ok ? " PASS " : " FAIL ", c.name);
        std::fflush (stdout);
    }
    std::printf ("\n%d casos, %d PASS, %d FAIL (filtro \"%s\")\n", run, run - failed, failed, filter);
    if (run == 0)
        return 2;
    return failed == 0 ? 0 : 1;
}

} // namespace labdtest

#define LABD_CAT2(a, b) a##b
#define LABD_CAT(a, b) LABD_CAT2 (a, b)
#define TEST_CASE(name)                                                                  \
    static void LABD_CAT (labd_test_fn_, __LINE__)();                                    \
    static labdtest::Registrar LABD_CAT (labd_test_reg_, __LINE__) (name, &LABD_CAT (labd_test_fn_, __LINE__)); \
    static void LABD_CAT (labd_test_fn_, __LINE__)()

#define CHECK(cond)                                                                      \
    do { if (! (cond)) labdtest::fail (__FILE__, __LINE__, "CHECK(" #cond ")"); } while (0)

#define CHECK_MSG(cond, ...)                                                             \
    do { if (! (cond)) { char _b[512]; std::snprintf (_b, sizeof (_b), __VA_ARGS__);     \
         labdtest::fail (__FILE__, __LINE__, _b); } } while (0)

#define CHECK_NEAR(a, b, tol)                                                            \
    do { const double _a = (double) (a), _b2 = (double) (b), _t = (double) (tol);        \
         if (! (std::abs (_a - _b2) <= _t)) { char _m[512];                              \
            std::snprintf (_m, sizeof (_m), "CHECK_NEAR(%s, %s, %s): %.9g vs %.9g", #a, #b, #tol, _a, _b2); \
            labdtest::fail (__FILE__, __LINE__, _m); } } while (0)

#define EVIDENCE(...) labdtest::evidence (__VA_ARGS__)
