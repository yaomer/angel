#ifndef _ANGEL_ID_H
#define _ANGEL_ID_H

#include <set>
#include "noncopyable.h"

namespace Angel {

class Id : noncopyable {
public:
    Id() : _id(0) {  }
    explicit Id(size_t id) : _id(id) {  }
    ~Id() {  }
    size_t getId()
    {
        size_t id;
        if (!_freeIdList.empty()) {
            id = *_freeIdList.begin();
            _freeIdList.erase(_freeIdList.begin());
        } else {
            id = _id++;
        }
        return id;
    }
    void putId(size_t id) 
    {
        _freeIdList.insert(id);
    }
private:
    size_t _id;
    std::set<size_t> _freeIdList;
};
}

#endif // _ANGEL_ID_H
