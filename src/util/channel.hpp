#include <deque>
#include <mutex>
#include <vector>
#include <utility>

template <typename T>
class Channel {
	std::deque<T> queue;
	std::mutex mx;
	std::condition_variable writeSignal;
	std::condition_variable readSignal;
	bool closed = false;
	size_t capacity;

	public:
	explicit Channel(size_t cap = 0) : capacity(cap) {}

	// send a value to the queue, returns false if closed
	bool send(T value){
		std::unique_lock<std::mutex> lock(mx);

		if (capacity > 0) {
			writeSignal.wait(lock, [this]() {
				return queue.size() < capacity || closed;
			});
		}

		if (closed) return false;

		queue.push_back(std::move(value));
		readSignal.notify_one();

		return true;
	}

	// sendMany append multiple values to the queue, returns false if closed
	bool sendMany(std::vector<T> values){
		std::unique_lock<std::mutex> lock(mx);
		if (capacity > 0) {
            writeSignal.wait(lock, [this]() {
                return queue.size() < capacity || closed;
            });
        }

		if (closed) return false;

		queue.insert(queue.end(), values.begin(), values.end());
		readSignal.notify_all();
		return true;
	}

	// receive the next value from the queue, returns false if closed
	bool receive(T& out) {
		std::unique_lock<std::mutex> lock(mx);

		// Wait while queue is empty and channel isn't closed
        readSignal.wait(lock, [this]() {
            return !queue.empty() || closed;
        });

        if (queue.empty()) {
            return false; // Channel is closed and empty
        }

		out = std::move(queue.front());
		queue.pop_front();
		writeSignal.notify_one();
		return true;
    }

    int size() {
    	return queue.size();
    }

    void shrink_to_fit() {
    	std::unique_lock<std::mutex> lock(mx);
    	queue.shrink_to_fit();
    }

    // close the Channel & unblock all writes & reads
    void close() {
    	std::unique_lock<std::mutex> lock(mx);
        closed = true;
        readSignal.notify_all();
        writeSignal.notify_all();
    }
};
