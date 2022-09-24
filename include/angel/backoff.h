#ifndef __ANGEL_BACKOFF_H
#define __ANGEL_BACKOFF_H

namespace angel {

class ExponentialBackoff {
public:
    ExponentialBackoff() {  }
    ExponentialBackoff(int init_interval, double multiplier, int max_retries)
        : interval(init_interval), multiplier(multiplier), max_retries(max_retries) {  }
    // Stop if return 0
    int next_back_off()
    {
        if (max_retries == 0) return 0;
        int backoff = interval;
        interval *= multiplier;
        --max_retries;
        return backoff;
    }
private:
    int interval = 200;
    double multiplier = 2.0;
    int max_retries = 6;
    // default: 200 400 800 1600 3200 6400
};

}

#endif // __ANGEL_BACKOFF_H
