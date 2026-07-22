#ifndef PAGER_UTILS_H_
#define PAGER_UTILS_H_

#include <stdbool.h>

typedef struct pager Pager;
typedef struct page Page;

extern bool write_page_size(Pager *pager, Page *page);

extern bool read_page_size(Pager *pager, Page *page);

extern bool is_page_free(Pager *pager, uint32_t page_num);

extern bool get_free_page(Pager *pager, uint32_t *page_num);

extern bool connect_free_page(Pager *pager, uint32_t page_num);

#endif