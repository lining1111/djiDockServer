//
// Created by lining on 11/17/22.
//

#ifndef _TIMETASK_H
#define _TIMETASK_H

#include <functional>
#include <chrono>
#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>

namespace utils {
    class Timer {
    public:
        Timer() : _expired(true), _try_to_expire(false) {

        }

        Timer(std::string name) : _name(name), _expired(true), _try_to_expire(false) {
        }

        Timer(const Timer &timer) {
            _name = timer._name;
            _expired = timer._expired.load();
            _try_to_expire = timer._try_to_expire.load();
        }

        ~Timer() {
            stop();
        }

        void start(int interval_ms, std::function<void()> task) {
            period = interval_ms;
            // is started, do not start again
            if (_expired == false)
                return;

            // start async timer, launch thread and wait in that thread
            _expired = false;
            std::thread([this, interval_ms, task]() {
                while (!_try_to_expire) {
                    // sleep every interval and do the task again and again until times up
                    task();
                    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
                }

                {
                    // timer be stopped, update the condition variable expired and wake main thread
                    std::lock_guard<std::mutex> locker(_mutex);
                    _expired = true;
                    _expired_cond.notify_one();
                }
            }).detach();
        }

        void startOnce(int delay_ms, std::function<void()> task) {
            period = delay_ms;
            std::thread([delay_ms, task]() {
                task();
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            }).detach();
        }

        void stop() {
            // do not stop again
            if (_expired)
                return;

            if (_try_to_expire)
                return;

            // wait until timer
            _try_to_expire = true; // change this bool value to make timer while loop stop
            {
                std::unique_lock<std::mutex> locker(_mutex);
                _expired_cond.wait(locker, [this] { return _expired == true; });

                // reset the timer
                if (_expired == true)
                    _try_to_expire = false;
            }
        }

        void setName(std::string name) {
            _name = name;
        }

        std::string getName() {
            return _name;
        }

        int getPeriodMs() {
            return period;
        }

    private:
        std::atomic<bool> _expired; // timer stopped status
        std::atomic<bool> _try_to_expire; // timer is in stop process
        std::mutex _mutex;
        std::condition_variable _expired_cond;
        std::string _name;
        int period;
    };

}


#endif //_TIMETASK_H
