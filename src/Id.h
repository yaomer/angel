#ifndef _ANGEL_ID_H
#define _ANGEL_ID_H

#include <set>
#include "noncopyable.h"

namespace Angel {

class Id : noncopyable {
public:
    Id() : _id(0), _idNums(0) {  }
    explicit Id(size_t id) 
        : _id(id), _idNums(0) {  }
    ~Id() {  }
    // 返回最小的未使用的id
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
    std::set<size_t> _freeIdList;
};
}

#endif // _ANGEL_ID_H
