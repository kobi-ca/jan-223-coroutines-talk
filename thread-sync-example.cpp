//
// Created by kobi on 1/10/23.
//

#include <coroutine>
#include <chrono>
#include <iostream>
#include <functional>
#include <string>
#include <stdexcept>
#include <atomic>
#include <thread>

class Event {
public:
    Event() = default;

    Event(const Event&) = delete;
    Event(Event&&) = delete;
    Event& operator=(const Event&) = delete;
    Event& operator=(Event&&) = delete;

    class Awaiter;
    Awaiter operator co_await() const noexcept;

    void notify() noexcept;

private:

    friend class Awaiter;

    mutable std::atomic<void*> suspendedWaiter{nullptr};
    mutable std::atomic<bool> notified{false};
};

class Event::Awaiter {
public:
    Awaiter(const Event& eve): event(eve) {}

    bool await_ready() const;
    bool await_suspend(std::coroutine_handle<> corHandle) noexcept;
    void await_resume() noexcept {}
private:
    friend class Event;
    const Event& event;
    std::coroutine_handle<> coroutineHandle;
};

bool Event::Awaiter::await_ready() const {
    // allow at most one waiter
    if (event.suspendedWaiter.load() != nullptr) {
        throw std::runtime_error("More than one waiter is not valid");
    }
    // event.notified == false; suspends the coroutine
    // event.notified == true; the coroutine is executed like a normal function
    std::cout << "await_ready event.notified " << std::boolalpha << event.notified.load() << '\n';
    return event.notified;
}

bool Event::Awaiter::await_suspend(std::coroutine_handle<>corHandle) noexcept {
    coroutineHandle = corHandle;
    if (event.notified) {
        std::cout << "await_suspend event.notified " << std::boolalpha << event.notified.load() << '\n';
        return false;
    }
    // store the waiter for later notification
    std::cout << "await_suspend store this and return true\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    event.suspendedWaiter.store(this);
    std::cout << "await_suspend return true\n";
    return true;
}

void Event::notify() noexcept {
    notified = true;
    std::cout << "notify happen\n";

    // try to load the waiter
    std::cout << "notify before getting waiter\n";
    auto* waiter = static_cast<Awaiter*>(suspendedWaiter.load());
    std::cout << "notify after getting waiter\n";

    // check if a waiter is available
    if (waiter != nullptr) {
        // resume the coroutine => await_resume
        std::cout << "notify resuming the coro\n";
        waiter->coroutineHandle.resume();
    }
    std::cout << "notify func done\n";
}

Event::Awaiter Event::operator co_await() const noexcept {
    return Awaiter{ *this };
}

struct Task {
    struct promise_type {
        Task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept {
            std::cout << "final_suspend\n";
            return {}; }
        void return_void() {}
        void unhandled_exception() {
            std::cout << "exception\n";
        }
    };
};

Task receiver(Event& event) try {
    auto start = std::chrono::high_resolution_clock::now();
    co_await event;
    std::cout << "Got the notification! " << '\n';
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Waited " << elapsed.count() << " seconds." << '\n';
} catch (...) {
    std::cerr << "exception!!!n";
}

using namespace std::chrono_literals;

int main() {
    std::cout << '\n';

    std::cout << "Notification before waiting" << '\n';
    Event event1{};
    auto senderThread1 = std::thread([&event1]{ event1.notify(); }); // Notification
    auto receiverThread1 = std::thread(receiver, std::ref(event1));

    receiverThread1.join();
    senderThread1.join();

    std::cout << "done1" << '\n';

    std::cout << "Notification after 2 seconds waiting" << '\n';
    Event event2{};
    auto receiverThread2 = std::thread(receiver, std::ref(event2));
    auto senderThread2 = std::thread([&event2] {
        //std::this_thread::sleep_for(2s);
        event2.notify();                    // Notification
    });

    receiverThread2.join();
    senderThread2.join();
    std::cout << "done2" << '\n';
}
