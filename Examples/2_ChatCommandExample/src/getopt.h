#include <string.h>
#include <stdio.h>

char* optarg = nullptr;
int optind = 1;
int opterr = 1;
int optopt = '?';

int getopt(int argc, char* const argv[], const char* optstring) {
    static int nextchar = 0;
    if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0') {
        return -1;
    }

    if (strcmp(argv[optind], "--") == 0) {
        optind++;
        return -1;
    }

    char c = argv[optind][nextchar + 1];
    const char* opt = strchr(optstring, c);
    if (!opt) {
        if (opterr) fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], c);
        optopt = c;
        nextchar++;
        if (argv[optind][nextchar + 1] == '\0') {
            optind++;
            nextchar = 0;
        }
        return '?';
    }

    if (*(opt + 1) == ':') {
        if (argv[optind][nextchar + 2] != '\0') {
            optarg = &argv[optind][nextchar + 2];
            optind++;
            nextchar = 0;
        } else if (optind + 1 < argc) {
            optarg = argv[optind + 1];
            optind += 2;
            nextchar = 0;
        } else {
            if (opterr) fprintf(stderr, "%s: option requires an argument -- '%c'\n", argv[0], c);
            optopt = c;
            return (optstring[0] == ':') ? ':' : '?';
        }
    } else {
        nextchar++;
        if (argv[optind][nextchar + 1] == '\0') {
            optind++;
            nextchar = 0;
        }
        optarg = nullptr;
    }

    return c;
}

struct option {
    const char* name;
    int has_arg;
    int* flag;
    int val;
};

int getopt_long(int argc, char* const argv[], const char* optstring,
                const struct option* longopts, int* longindex) {
    if (optind >= argc) return -1;
    if (argv[optind][0] != '-') return -1;
    if (argv[optind][1] != '-') return getopt(argc, argv, optstring);

    optind++;
    const char* arg = argv[optind - 1] + 2;
    for (int i = 0; longopts[i].name; i++) {
        if (strcmp(longopts[i].name, arg) == 0) {
            if (longindex) *longindex = i;
            if (longopts[i].has_arg == required_argument) {
                if (optind >= argc) return '?';
                optarg = argv[optind];
                optind++;
            }
            if (longopts[i].flag) {
                *longopts[i].flag = longopts[i].val;
                return 0;
            }
            return longopts[i].val;
        }
    }
    return '?';
}

#define no_argument 0
#define required_argument 1
#define optional_argument 2
