Async DNS Resolver
---

关于如何处理`resolver::query()`的返回值`result_future`，可以参考`resolver::show()`函数

(这里你需要了解[future](https://en.cppreference.com/w/cpp/thread/shared_future)的用法)

### 查询`A`记录
```cpp
#include <angel/resolver.h>

int main()
{
    angel::dns::resolver r;
    auto f = r.query("baidu.com", angel::dns::A);
    r.show(f);
}
```
```
baidu.com has address 39.156.66.10
baidu.com has address 110.242.68.66
```
### 查询`MX`记录
```cpp
int main()
{
    angel::dns::resolver r;
    auto f = r.query("baidu.com", angel::dns::MX);
    r.show(f);
}
```
```
baidu.com mail is handled by 20 jpmx.baidu.com
baidu.com mail is handled by 20 usmx01.baidu.com
baidu.com mail is handled by 10 mx.maillb.baidu.com
baidu.com mail is handled by 20 mx1.baidu.com
baidu.com mail is handled by 20 mx50.baidu.com
baidu.com mail is handled by 15 mx.n.shifen.com
```

### 多线程并发访问
```cpp
#include <angel/resolver.h>
#include <angel/logger.h>
#include <thread>

int main()
{
    angel::dns::resolver r;
    auto f = [&r]() {
        for (int i = 0; i < 500; i++) {
            auto f = r.query("baidu.com", angel::dns::A);
            auto& ans = f.get();
            for (auto& item : ans) {
                auto *x = angel::dns::get_a(item);
                log_info("%s", x->addr.c_str());
            }
        }
    };
    std::thread t1(f);
    std::thread t2(f);
    std::thread t3(f);
    std::thread t4(f);
    t1.join();
    t2.join();
    t3.join();
    t4.join();
}
```
```
$ time ./a.out

real    0m1.616s
user    0m0.189s
sys     0m0.250s
```
输出结果在`.log`目录下，使用下面命令可以很容易验证程序工作正常

```
$ grep '39.156.66.10' .log/${file} | wc -l
2000
```

如果将上面的4线程查询改为单线程呢？
```cpp
int main()
{
    f();
    f();
    f();
    f();
}
```
```
real    0m5.833s
user    0m0.225s
sys     0m0.282s
```
结果显而易见，多线程下有更好的查询性能。
