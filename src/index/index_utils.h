#ifndef INDEX_UTILS_H_
#define INDEX_UTILS_H_

#include "../../include/index.h"

/* Creation of logical Index struct */
Index *index_metadata_create(const char *index_name, IndexType type, const IndexKey *key, uint32_t root_page_num);

#endif