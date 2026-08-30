#ifndef EMULATOR_PAGE_BACKEND_H
#define EMULATOR_PAGE_BACKEND_H

struct page_backend {
    const char *fixture;
    int live;
    unsigned char requested_subpages[32];
    unsigned int request_count;
};
int page_backend_fetch(void *, unsigned char, unsigned short, unsigned char,
                       unsigned char[960], unsigned char *, unsigned char[7]);
#endif
