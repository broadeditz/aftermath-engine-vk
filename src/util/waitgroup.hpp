#include <mutex>

class WaitGroup {
    std::atomic<int> count{0};
    std::mutex mx;
    std::condition_variable cv;

public:
    void add(int n) { count += n; }

    void done() {
        if (--count == 0) {
            cv.notify_all();
        }
    }

    void wait() {
        std::unique_lock<std::mutex> lock(mx);
        cv.wait(lock, [this]() { return count == 0; });
    }
};
