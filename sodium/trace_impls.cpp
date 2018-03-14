#include <sodium/sodium.h>
#include <bacon_gc/gc.h>
#include <functional>

/*
namespace bacon_gc {
    template <typename A>
    struct Trace<std::function<A>> {
        template <typename F>
        static void trace(std::function<A>& a, F&& k) {
            // do nothing here, we can not trace a std::function<>
        }
    };

}
*/
