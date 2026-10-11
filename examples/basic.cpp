// Minimal standalone example of normal dispatch and explicit greedy dispatch.
#include "threadpool.h"
#include "greedy_threadpool.h"
#include <iostream>

int main() {
    namespace tp = nnn::threadpool;
    tp::threadpool normal(4);
    auto result = normal.submit([](int value) { return value * 2; }, 21);
    const int answer = result.get();

    tp::greedy_threadpool greedy(4); // Initially paused: start before waiting.
    auto work = greedy.submit([] { return 7; });
    greedy.go();
    const int other = work.get();
    greedy.wait_idle();
    greedy.stop();
    std::cout << "Normal result: " << answer << "\nGreedy result: " << other << '\n';
    return answer == 42 && other == 7 ? 0 : 1;
}
