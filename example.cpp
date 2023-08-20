#include "ThreadPool.h"
#include <chrono>
#include <iostream>

int main()
{
    mutex print_lock; // make sure the serial cout operation is atomic, or the print result will be a mass.

    ThreadPool pool(4);
    std::vector<std::future<int>> results;

    for (int i = 0; i < 8; i++) {
        results.emplace_back(
            pool.enqueue([i, &print_lock] {
                
                {
                    std::unique_lock<std::mutex> lock(print_lock);
                    std::cout << "hello " << i << std::endl;
                }

                std::this_thread::sleep_for(std::chrono::seconds(1));

                {
                    std::unique_lock<std::mutex> lock(print_lock);
                    std::cout << "world " << i << std::endl;
                }
                
                return i * i;
            }));
    }

    while (!results.empty()) {
        for (auto it = results.begin(); it != results.end();) {
            std::future<int>& future = *it;
            std::future_status status = future.wait_for(std::chrono::seconds(0));

            if (status == std::future_status::ready) {
                int result = future.get();

                {
                    std::unique_lock<std::mutex> lock(print_lock);
                    std::cout << "Result: " << result << std::endl;
                }
                
                it = results.erase(it); // Remove the future from the vector
            } else if (status == std::future_status::timeout) {
                ++it; // Move to the next future
            }
        }
    }

    return 0;
}