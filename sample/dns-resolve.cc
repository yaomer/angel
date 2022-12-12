#include <angel/resolver.h>

#include <getopt.h>

void usage()
{
    fprintf(stderr, "Usage: ./dns-resolve [-t A|MX|NS|etc] [domain name]\n");
    exit(1);
}

#include <angel/logger.h>
int main(int argc, char *argv[])
{
    // angel::set_log_flush(angel::logger::flush_flags::stdout);
    int c;
    int type = angel::dns::A;
    while ((c = getopt(argc, argv, "t:")) != -1) {
        switch (c) {
        case 't':
            if (strcasecmp(optarg, "A") == 0) type = angel::dns::A;
            else if (strcasecmp(optarg, "MX") == 0) type = angel::dns::MX;
            else if (strcasecmp(optarg, "NS") == 0) type = angel::dns::NS;
            else if (strcasecmp(optarg, "SOA") == 0) type = angel::dns::SOA;
            else if (strcasecmp(optarg, "CNAME") == 0) type = angel::dns::CNAME;
            else if (strcasecmp(optarg, "TXT") == 0) type = angel::dns::TXT;
            else if (strcasecmp(optarg, "PTR") == 0) type = angel::dns::PTR;
            else usage();
            break;
        default:
            fprintf(stderr, "Illegal argument \"%c\"\n", c);
            usage();
        }
    }
    if (optind != argc - 1) {
        usage();
    }

    auto *resolver = angel::dns::resolver::get_resolver();

    auto r = resolver->query(argv[optind], type);
    resolver->show(r);
}
