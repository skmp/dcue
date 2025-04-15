#pragma once
#include <coroutine>
#include <variant>
#include <memory>
#include <type_traits>

extern float timeDeltaTime;

// An atomic yield value representing one frame step.
enum class Step { Frame };

struct conditional_awaiter {
    bool should_suspend;

    bool await_ready() const { return !should_suspend; }
    void await_suspend(std::coroutine_handle<>) { /* Optionally do something here */ }
    void await_resume() {}
};

// Forward declaration of Task.
struct Task;

// The yielded value type is either a simple frame marker or a nested Task (wrapped in a unique_ptr).
// Using a unique_ptr here avoids cyclic dependency issues.
using YieldValue = std::variant<Step, std::unique_ptr<Task>>;

//-----------------------------------------------------------
// Task: a generator-style coroutine that yields one frame at a time.
//-----------------------------------------------------------
struct Task {
    struct promise_type {
        YieldValue currentValue;  // The value yielded on suspension

        auto get_return_object() {
            return Task{ std::coroutine_handle<promise_type>::from_promise(*this) };
        }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        // When yielding a simple frame marker.
        std::suspend_always yield_value(Step s) {
            currentValue = s;
            return {};
        }
        // Overload for yielding a nested Task.
        conditional_awaiter yield_value(Task t) {
            t.next();
            if (t.done()) {
                return { false };
            }
            // Wrap the nested task in a unique_ptr so that we can hold it in our variant.
            currentValue = std::make_unique<Task>(std::move(t));
            return { true };
        }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> coro;

    Task(std::coroutine_handle<promise_type> h) : coro(h) {}
    Task(Task&& other) noexcept : coro(other.coro) { other.coro = nullptr; }
    Task(const Task&) = delete;
    ~Task() { if(coro) coro.destroy(); }

    bool done() const { return coro.done(); }


    void next() {
        if (done())
            return;

        // Check if the current yielded value is a nested Task.
        while (std::holds_alternative<std::unique_ptr<Task>>(coro.promise().currentValue)) {
            auto& nestedPtr = std::get<std::unique_ptr<Task>>(coro.promise().currentValue);
            if (!nestedPtr->done()) {
                // Drive the nested task one frame.
                nestedPtr->next();
                // Return control so that we advance one frame.

                if (!nestedPtr->done()) {
                    return;
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        coro.resume();
    }
};

inline Task WaitTime(float seconds) {
    float remaining = seconds;
    while (remaining > 0.0f) {
        co_yield Step::Frame;
        remaining -= timeDeltaTime;
    }
}

inline Task WaitFrame() {
    co_yield Step::Frame;
}

inline Task WaitUntil(std::function<bool()> cond) {
    while(!cond()) {
        co_yield Step::Frame;
    }
}

void queueCoroutine(Task&& coroutine);