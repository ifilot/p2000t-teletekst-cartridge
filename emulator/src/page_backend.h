#ifndef EMULATOR_PAGE_BACKEND_H
#define EMULATOR_PAGE_BACKEND_H

struct page_backend {
    const char *fixture;
    int live;
    unsigned char requested_subpages[32];
    unsigned short requested_pages[32];
    unsigned int request_count;
};
int page_backend_fetch(void *, unsigned char, const char *, unsigned short,
                       unsigned char, unsigned char[960], unsigned char *,
                       unsigned short *, unsigned short *, unsigned char[7]);
#endif
