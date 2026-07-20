#ifndef PAGER_UTILS_H_
#define PAGER_UTILS_H_

#include <stdbool.h>

typedef struct pager Pager;
typedef struct page Page;

extern bool write_page_size(Pager *pager, Page *page);

#endif