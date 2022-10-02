Async SMTP Client
---

```cpp
#include <angel/smtplib.h>
#include <angel/mime.h>

#include <iostream>

int main()
{
    auto host = "smtp.qq.com";
    auto username = "xxx@qq.com";
    auto password = "Authorization code";
    auto content = "<h1> Hello </h1>";

    auto sender = username;
    std::vector<std::string> receivers = { "who@163.com" };

    angel::mime::text msg(content, "html", "utf-8");
    msg.add_header("From", "xxx<xxx@qq.com>");
    msg.add_header("To", "who<who@163.com>");
    msg.add_header("Subject", "Test Smtp");

    angel::smtplib::smtp smtp;
    auto f = smtp.send_mail(host, 25, username, password, sender, receivers, msg.str());

    auto& res = f.get();
    if (res.is_ok) {
        std::cout << "send successfully\n";
    } else {
        std::cout << res.err << "\n";
    }
}
```
