// test std::priority_queue

#include <set>
#include <queue>
#include <vector>
#include <iostream>
#include <algorithm>

struct greater_t {
    bool operator()(const int& a, const int& b) {
        // give priority to smaller items
        return a > b;
    }
};

class MyVec
{
    std::vector<int> m_queue;
  public:
    void push(int x) {
        m_queue.push_back(x);
        std::sort(m_queue.begin(), m_queue.end(), greater_t());
    }
    void chirp() {
        std::cout << "ovec:";
        for (const auto& i : m_queue) {
            std::cout << " " << i;
        }
        std::cout << std::endl;
    }
    
};

typedef std::set<int, greater_t> set_t;
class MySet : public set_t
{
public:
    void push(int x) { this->insert(x); }
    void chirp() {
        std::cout << "oset:";
        for (const auto& i : *this) {
            std::cout << " " << i;
        }
        std::cout << std::endl;
    }
            
};


typedef std::priority_queue<int, std::vector<int>, greater_t> queue_t;
class MyQueue : public queue_t
{
public:
    void chirp() {
        auto it = c.cbegin();
        auto last = c.cend();
        std::cout << "pqueue:";
        while (it != last) {
            std::cout << " " << *it;
            ++it;
        }
        std::cout << std::endl;
    }
};


int main()
{
    const std::vector<int> dat{3,2,42,4,43};

    MyVec mv;
    MySet ms;
    MyQueue mq;
    for (auto d : dat) {
        mv.push(d);
        ms.push(d);
        mq.push(d);
    }
    mv.chirp();
    ms.chirp();
    mq.chirp();
    return 0;
}

