Async SMTP Client
---

```cpp
#include <angel/smtplib.h>
#include <iostream>

int main()
{
    angel::smtplib::email mail;
    angel::smtplib::sender sender;

    auto username = "yours@qq.com";
    auto password = "Authorization code";

    mail.from = username;
    mail.to = {"yours@163.com"};
    mail.headers["Subject"] = "Test Smtp Client";
    mail.headers["From"] = "yours<yours@qq.com>";
    mail.headers["To"] = "yours<yours@163.com>";
    mail.data = "hello";

    auto f = sender.send("smtp.qq.com", 25, username, password, mail);
    // do something

    auto res = f.get();
    if (res.is_ok) {
        std::cout << "send successfully\n";
    } else {
        std::cout << res.err << "\n";
    }
}
```
