#include <angel/resolver.h>

#include <iostream>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cout << "./dns-resolve [domain-name]\n";
        exit(1);
    }

    auto *resolv = angel::dns::resolver::get_resolver();

    auto r = resolv->query(argv[1], angel::dns::A);
    resolv->show(r);

    r = resolv->query(argv[1], angel::dns::MX);
    resolv->show(r);
}
