#include <angel/smtplib.h>
#include <angel/mime.h>

#include <getopt.h>

#include <iostream>

static struct option opts[] = {
    { "host", required_argument, nullptr, 'h' },
    { "sender", required_argument, nullptr, 's' },
    { "receiver", required_argument, nullptr, 'r' },
    { "password", required_argument, nullptr, 'p' },
    { "subject", required_argument, nullptr, 'j' },
    { "body", required_argument, nullptr, 'b' },
};

static void usage()
{
    fprintf(stderr,
            "Usage: ./sendmail [options][all required]\n"
            "    --host     Sender SMTP Host (smtp.example.com)\n"
            "    --sender   Sender Mailbox (username@example.com)\n"
            "    --receiver Receiver Mailbox (username@example.com)\n"
            "    --password Sender Mailbox Authorization Code\n"
            "    --subject  Mail Subject\n"
            "    --body     Mail Body (text/html;charset=utf-8)\n"
           );
    exit(1);
}

int main(int argc, char *argv[])
{
    int c;
    std::string host;
    std::string sender;
    std::string password;
    std::vector<std::string> receivers;
    std::string subject;
    std::string body;
    while ((c = getopt_long(argc, argv, "h:s:r:p:j:b:", opts, nullptr)) != -1) {
        switch (c) {
        case 'h':
            host = optarg;
            break;
        case 's':
            sender = optarg;
            break;
        case 'r':
            receivers = { optarg };
            break;
        case 'p':
            password = optarg;
            break;
        case 'j':
            subject = optarg;
            break;
        case 'b':
            body = optarg;
            break;
        default:
            fprintf(stderr, "Illegal argument \"%c\"\n", c);
            usage();
        }
    }
    if (host.empty() || sender.empty() || receivers.empty() || password.empty() || subject.empty() || body.empty()) {
        usage();
    }

    angel::mime::text mail(body, "html", "utf-8");
    mail.add_header("From", sender);
    mail.add_header("To", receivers.front());
    mail.add_header("Subject", subject);

    angel::smtplib::smtp smtp;

    auto f = smtp.send_mail(host, 25, sender, password, sender, receivers, mail.str());
    std::cout << "sending...\n";

    auto& res = f.get();
    if (res.is_ok) {
        std::cout << "send successfully\n";
    } else {
        std::cout << res.err.c_str() << "\n";
    }
}
