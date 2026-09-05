#ifndef EMULATOR_PAGE_BACKEND_H
#define EMULATOR_PAGE_BACKEND_H

struct page_backend {
    const char *fixture;
    int live;
    unsigned short fail_page;
    unsigned char fail_error;
    unsigned char requested_subpages[32];
    unsigned char requested_sources[32];
    unsigned short requested_pages[32];
    unsigned int request_count;
};
unsigned char page_backend_fetch(void *, unsigned char, const char *,
                                 unsigned short, unsigned char,
                                 unsigned char[960], unsigned char *,
                                 unsigned short *, unsigned short *,
                                 unsigned char[7]);
#endif
