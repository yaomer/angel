#ifndef _ANGEL_ID_H
#define _ANGEL_ID_H

#include <set>
#include "noncopyable.h"

namespace Angel {

// 这里实现的Id class类似于文件描述符(fd)，
// 利用getId()可以获取未使用的最小的Id
class Id : noncopyable {
public:
    Id() : _id(0), _idNums(0) {  }
    explicit Id(size_t id) 
        : _id(id), _idNums(0) {  }
    ~Id() {  }
    // 返回最小的未使用的id，O(1)
    size_t getId()
    {
        size_t id;
        if (!_freeIdList.empty()) {
            id = *_freeIdList.begin();
            _freeIdList.erase(_freeIdList.begin());
        } else {
            id = _id++;
        }
        _idNums++;
        return id;
    }
    // O(log n)
    void putId(size_t id) 
    {
        _freeIdList.insert(id);
        _idNums--;
    }
    // 返回已使用的id数目
    size_t getIdNums() const { return _idNums; }
private:
    size_t _id;
    size_t _idNums;
    // FIXME: 考虑使用有序链表实现，可以节约内存
    std::set<size_t> _freeIdList;
};
}

#endif // _ANGEL_ID_H
