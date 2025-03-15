#ifndef __NONCOPYABLE_H_
#define __NONCOPYABLE_H_

#include <mutex>

class NonCopyable {
public:
//    std::mutex mtx;
//    NonCopyable *m_pInstance;

public:
    NonCopyable(const NonCopyable &) = delete;

    NonCopyable *operator=(const NonCopyable *) = delete;

    NonCopyable(){

    }
    ~NonCopyable(){

    }

//    NonCopyable *instance() {
//        std::unique_lock<std::mutex> lck(mtx);
//        lck.lock();
//        if (m_pInstance == nullptr) {
//            m_pInstance = new NonCopyable();
//        }
//        lck.unlock();
//        return m_pInstance;
//    };

};


#endif