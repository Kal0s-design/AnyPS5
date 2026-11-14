#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {

constexpr int Ignored = 40;
constexpr int Noise = 30;
constexpr int Wake = 31;

std::atomic<int> woken{0};
std::atomic<bool> finished{false};

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "signal suspend disposition: %s\n", what);
    std::_Exit(1);
}

GuestSignalSet Only(int guest) {
    GuestSignalSet set{};
    set.bits[(guest - 1) / 32] = std::uint32_t{1} << ((guest - 1) % 32);
    return set;
}

void APS5_VABI OnNoise(int) {}

void APS5_VABI OnWake(int) {
    ++woken;
}

}

int main() {
    GuestSigaction noise{};
    noise.handler = reinterpret_cast<std::uintptr_t>(&OnNoise);
    Require(sigaction_nid_postfix(Noise, &noise, nullptr) == 0, "install the noise handler");
    GuestSigaction wake{};
    wake.handler = reinterpret_cast<std::uintptr_t>(&OnWake);
    Require(sigaction_nid_postfix(Wake, &wake, nullptr) == 0, "install the wake handler");
    std::thread watchdog([] {
        for (int tick = 0; tick < 150 && !finished.load(); ++tick) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (finished.load()) return;
        std::fprintf(stderr, "signal suspend disposition: sigsuspend and a disposition change never finished, each holds the lock the other waits for\n");
        std::_Exit(2);
    });
    std::atomic<bool> stop{false};
    std::atomic<bool> suspended{false};
    std::thread suspender([&] {
        const GuestSignalSet mask = Only(Noise);
        suspended = true;
        Require(sigsuspend_nid_postfix(&mask) == -1 && *__error_nid_postfix() == 4, "sigsuspend returns once the wake signal is delivered");
    });
    std::thread raiser([&] {
        GuestSignalSet blocked = Only(Noise);
        blocked.bits[0] |= Only(Wake).bits[0];
        Require(pthread_sigmask_nid_postfix(1, &blocked, nullptr) == 0, "block the signals on the raiser");
        while (!suspended.load()) std::this_thread::yield();
        while (!stop.load(std::memory_order_relaxed)) Require(raise_nid_postfix(Noise) == 0, "raise the blocked noise signal");
        Require(raise_nid_postfix(Wake) == 0, "raise the blocked wake signal");
    });
    GuestSigaction ignore{};
    ignore.handler = 1;
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (std::chrono::steady_clock::now() < end) Require(sigaction_nid_postfix(Ignored, &ignore, nullptr) == 0, "ignore a signal while another thread is suspended");
    stop = true;
    raiser.join();
    suspender.join();
    Require(woken.load() == 1, "the wake handler ran once");
    finished = true;
    watchdog.join();
    return 0;
}
