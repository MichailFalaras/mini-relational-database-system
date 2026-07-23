#ifndef PAGER_UTILS_H_
#define PAGER_UTILS_H_

#include <stdbool.h>

typedef struct pager Pager;
typedef struct page Page;
typedef enum free_page_result FreePageResult;
typedef enum page_free_status PageFreeStatus;

extern bool write_page_size(Pager *pager, Page *page);

extern bool read_page_size(Pager *pager, Page *page);

extern FreePageResult get_free_page(Pager *pager, uint32_t *page_num);

extern PageFreeStatus is_page_free(Pager *pager, uint32_t page_num);

extern bool connect_free_page(Pager *pager, uint32_t page_num);

#endif