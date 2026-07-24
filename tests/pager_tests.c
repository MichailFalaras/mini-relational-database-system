#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../include/pager.h"
#include "../include/page.h"
#include "../src/pager/pager_utils.h"

#define ASSERT(condition) \
    if (!(condition)) { \
        return 1;  \
    }

/* ---------- Unit Tests ---------- */

/* pager_close automatically flushes all pages. That's why there
 * is no separate test for flushing pages. */
static int test_opening_new_file() {
    const char *db_path = "build/database1.db";

    Pager *pager = pager_open(db_path);
    ASSERT(pager != NULL);
    ASSERT(pager->file_length == 0);
    ASSERT(pager->num_pages == 2);
    ASSERT(pager->access_counter == 4);
    /* Checks if file is actually made. */
    ASSERT(access(db_path, F_OK) == 0);

    bool res = pager_close(pager);
    ASSERT(res == true);

    return 0;
}

/* Opening already made file from the test above. */
static int test_opening_existing_file() {
    const char *db_path = "build/database1.db";

    Pager *pager = pager_open(db_path);
    ASSERT(pager != NULL);
    ASSERT(pager->file_length == 2 * PAGE_SIZE);
    ASSERT(pager->num_pages == 2);
    ASSERT(pager->access_counter == 4);

    bool res = pager_close(pager);
    ASSERT(res == true);

    return 0;
}

/* Opening empty filename. */
static int test_opening_empty_filename() {
    const char *db_path = "";

    Pager *pager = pager_open(db_path);
    ASSERT(pager == NULL);

    return 0;
}

/* Testing opening invalid file length by creating a normal
 * binary file, writing 5000 zeros and then trying to open it. */
static int test_invalid_file_length() {
    const char *db_path = "build/invalid_length.db";

    int fd = open(db_path, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR);
    if (fd < 0) {
        perror("test_invalid_file_length");
        exit(1);
    }

    char buffer[5000];
    memset(buffer, 0, 5000);
    ssize_t res = write(fd, buffer, 5000);
    if (res < 0) {
        perror("test_invalid_file_length");
        exit(1);
    }

    Pager *pager = pager_open(db_path);
    ASSERT(pager == NULL);

    return 0;
}

/* Testing logically allocating a page, then creating it,
 * clearing it and finally flushing it.*/
static int test_page_creation_and_clearing() {
    const char *db_path = "build/database2.db";
    
    Pager *pager = pager_open(db_path);

    uint32_t page_num;
    bool res = pager_allocate_page(pager, &page_num);
    ASSERT(res == true);
    ASSERT(page_num == 2);
    ASSERT(pager->num_pages == 3);

    Page *page = pager_get_page(pager, page_num);
    ASSERT(page != NULL);
    ASSERT(pager->pages[page_num] != NULL);
    ASSERT(page->last_interacted == pager->access_counter);

    res = page_clear(pager, page);
    ASSERT(res == true);
    ASSERT(page->is_dirty == true);
    ASSERT(page->last_interacted == pager->access_counter);

    res = pager_close(pager);
    ASSERT(res == true);

    return 0;
}

/* Testing getting a page from the disk. */
static int test_page_loading_existing_page() {
    const char *db_path = "build/database2.db";
    
    Pager *pager = pager_open(db_path);
    /* page_nums: 0, 1, 2 */
    ASSERT(pager->num_pages == 3);
    ASSERT(pager->file_length = 3 * PAGE_SIZE);

    uint32_t page_num = 2;
    Page *page = pager_get_page(pager, page_num);
    ASSERT(page != NULL);
    ASSERT(pager->pages[page_num] != NULL);
    ASSERT(page->last_interacted == pager->access_counter);

    bool res = pager_close(pager);
    ASSERT(res == true);

    return 0;
}

/* Testing logically allocating a page, creating it and
 * then getting its pointer from the cache through paper_get_page again. */
static int test_cache_page_retrieval() {
    const char *db_path = "build/database3.db";
    
    Pager *pager = pager_open(db_path);

    uint32_t page_num;
    bool res = pager_allocate_page(pager, &page_num);
    ASSERT(res == true);
    ASSERT(page_num == 2);
    ASSERT(pager->num_pages == 3);

    Page *page1 = pager_get_page(pager, page_num);
    ASSERT(page1 != NULL);
    ASSERT(pager->pages[page_num] != NULL);
    ASSERT(page1->last_interacted == pager->access_counter);

    /* Same page_num. Should return same page since its cached. */
    Page *page2 = pager_get_page(pager, page_num);
    ASSERT(page1 == page2);

    res = pager_close(pager);
    ASSERT(res == true);

    return 0;
}

/* Release 2 pages, one already existing in the disk and one just
 * getting created in RAM. 
 * Reuse last page entered in the Free Page List which is the head of
 * the list, connect page 0's free list pointer to the other page.
 * Reuse the other one too, leaving free list without
 * any free pages.
 * Finally, logically allocate a new page, create it and flush all. */
static int test_release_and_reuse_page() {
    const char *db_path = "build/database2.db";

    Pager *pager = pager_open(db_path);
    ASSERT(pager->num_pages == 3);
    ASSERT(pager->file_length = 3 * PAGE_SIZE);

    Page *page = pager_get_page(pager, pager->num_pages-1);
    ASSERT(page != NULL);
    
    uint32_t page_num;
    bool res = pager_allocate_page(pager, &page_num);
    ASSERT(res == true);

    page = pager_get_page(pager, page_num);
    ASSERT(page != NULL);

    uint32_t free_list_index;
    memcpy(
        &free_list_index,
        pager->pages[0]->page_data + FREE_LIST_HEAD_OFFSET,
        sizeof(uint32_t)
    );
    ASSERT(free_list_index == 0);

    res = pager_release_page(pager, 2);
    ASSERT(res == true);

    res = pager_release_page(pager, 3);
    ASSERT(res == true);

    memcpy(
        &free_list_index,
        pager->pages[0]->page_data + FREE_LIST_HEAD_OFFSET,
        sizeof(uint32_t)
    );

    uint32_t free_page1, free_page2;
    memcpy(
        &free_page1,
        pager->pages[2]->page_data,
        sizeof(uint32_t)
    );
    
    memcpy(
        &free_page2,
        pager->pages[3]->page_data,
        sizeof(uint32_t)
    );

    /* FIFO */
    ASSERT(free_list_index == 3);
    ASSERT(free_page2 == 2);
    ASSERT(free_page1 == 0);
    
    ASSERT(pager->pages[0]->is_dirty == true);
    ASSERT(pager->pages[3]->is_dirty == true);
    ASSERT(pager->pages[2]->is_dirty == true);

    res = pager_allocate_page(pager, &page_num);
    ASSERT(res == true);
    /* Page num should be 3 because we return the first free
     * page of the list. */
    ASSERT(page_num == 3);
    /* This should remove it from Free Page List. */
    page = pager_get_page(pager, page_num);
    ASSERT(page != NULL);

    /* Since we removed it, page 0 free list pointer should
     * point to page_num == 2*/
    memcpy(
        &free_list_index,
        pager->pages[0]->page_data + FREE_LIST_HEAD_OFFSET,
        sizeof(uint32_t)
    );
    ASSERT(free_list_index == 2);

    res = pager_allocate_page(pager, &page_num);
    ASSERT(res == true);
    ASSERT(page_num == 2);
    
    page = pager_get_page(pager, page_num);
    ASSERT(page != NULL);

    memcpy(
        &free_list_index,
        pager->pages[0]->page_data + FREE_LIST_HEAD_OFFSET,
        sizeof(uint32_t)
    );
    ASSERT(free_list_index == 0);

    /* Forced to allocate a new page. */
    res = pager_allocate_page(pager, &page_num);
    ASSERT(res == true);
    ASSERT(page_num == 4);
    
    page = pager_get_page(pager, page_num);
    ASSERT(page != NULL);

    res = pager_close(pager);
    ASSERT(res == true);

    return 0;
}

/* Continuation of previous test with no free pages. */
static int test_free_list_index_persistence() {
    const char *db_path = "build/database2.db";

    Pager *pager = pager_open(db_path);

    uint32_t free_list_index;
    memcpy(
        &free_list_index,
        pager->pages[0]->page_data + FREE_LIST_HEAD_OFFSET,
        sizeof(uint32_t)
    );
    /* It was 0 previously. */
    ASSERT(free_list_index == 0);

    bool res = pager_close(pager);
    ASSERT(res == true);

    return 0;
}

/* Testing releasing Page 0 or System Catalog root page. */
static int test_release_reserved_page() {
    const char *db_path = "build/database2.db";

    Pager *pager = pager_open(db_path);

    bool res = pager_release_page(pager, 0);
    ASSERT(res == false);

    res = pager_release_page(pager, 1);
    ASSERT(res == false);

    res = pager_close(pager);
    ASSERT(res == true);

    return 0;
}

/* Testing double releasing the same page, meaning we are
 * testing if program can detect already free pages. */
static int test_double_release_page() {
    const char *db_path = "build/database6.db";

    Pager *pager = pager_open(db_path);

    uint32_t page_num;
    bool res = pager_allocate_page(pager, &page_num);
    ASSERT(res == true);

    /* This should remove it from Free Page List. */
    Page *page = pager_get_page(pager, page_num);
    ASSERT(page != NULL);
    
    res = pager_release_page(pager, page_num);
    ASSERT(res == true);

    res = pager_release_page(pager, page_num);
    ASSERT(res == false);

    res = pager_close(pager);
    ASSERT(res == true);

    return 0;
}

/* Testing releasing out of bounds page. */
static int test_out_of_bounds_release() {
    const char *db_path = "build/database6.db";

    Pager *pager = pager_open(db_path);

    ASSERT(!pager_release_page(pager, 999));

    bool res = pager_close(pager);
    ASSERT(res == true);

    return 0;
}

/* Testing least recently used page eviction.
 * Page 0 and System Catalog root page are not allowed to
 * be evicted through pager_evict_lru function. */
static int test_evict_lru_page() {
    const char *db_path = "build/database4.db";

    Pager *pager = pager_open(db_path);
    ASSERT(pager->file_length == 0);

    uint32_t page_num;
    bool res;
    for (uint32_t i = 0; i < 5; i++) {
        res = pager_allocate_page(pager, &page_num);
        ASSERT(res == true);

        Page *page = pager_get_page(pager, page_num);
        ASSERT(page != NULL);
    }

    res = pager_evict_lru(pager);
    ASSERT(res == true);
    ASSERT(pager->file_length == 3 *PAGE_SIZE);
    ASSERT(pager->pages[2] == NULL);

    res = pager_close(pager);
    ASSERT(res == true);

    return 0;
}

/* Testing eviction of all pages. */
static int test_evict_all_pages() {
    const char *db_path = "build/database5.db";

    Pager *pager = pager_open(db_path);
    ASSERT(pager->file_length == 0);

    uint32_t page_num;
    bool res;
    for (uint32_t i = 0; i < 5; i++) {
        res = pager_allocate_page(pager, &page_num);
        ASSERT(res == true);

        Page *page = pager_get_page(pager, page_num);
        ASSERT(page != NULL);
    }

    res = pager_evict_all(pager);
    ASSERT(res == true);
    ASSERT(pager->file_length == 7 * PAGE_SIZE);
    for (uint32_t i = 0; i < 5; i++) {
        ASSERT(pager->pages[i+2] == NULL);
    }

    res = pager_close(pager);
    ASSERT(res == true);

    return 0;
}

/* ---------- Logging Helper ---------- */

void generate_output(int result, int test_num, char *test_desc) {
    int space = 40 - (int) strlen(test_desc);
    char *result_str = result == 0 ? "SUCCESS" : "ERROR";

    printf("TEST[%d]: %s - %*s\n", test_num, test_desc, space, result_str);
}

int main(int argc, char *argv[]) {
    int result;

    /* Remove temporary database files to allow the test to
     * run multiple times. */
    result = unlink("build/database1.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database2.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database3.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database4.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database5.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/database6.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }
    result = unlink("build/invalid_length.db");
    if (result < 0) { if (errno != ENOENT) { return 1; } }

    /* ---------- Unit Tests Results ---------- */
    result = test_opening_new_file();
    generate_output(result, 0, "test_opening_new_file");
    result = test_opening_existing_file();
    generate_output(result, 1, "test_opening_existing_file");
    result = test_opening_empty_filename();
    generate_output(result, 2, "test_opening_empty_filename");
    result = test_invalid_file_length();
    generate_output(result, 3, "test_invalid_file_length");
    result = test_page_creation_and_clearing();
    generate_output(result, 4, "test_page_creation_and_clearing");
    result = test_page_loading_existing_page();
    generate_output(result, 5, "test_page_loading_existing_page");
    result = test_cache_page_retrieval();
    generate_output(result, 6, "test_cache_page_retrieval");
    result = test_release_and_reuse_page();
    generate_output(result, 7, "test_release_and_reuse_page");
    result = test_free_list_index_persistence();
    generate_output(result, 8, "test_free_list_index_persistence");
    result = test_release_reserved_page();
    generate_output(result, 9, "test_release_reserved_page");
    result = test_double_release_page();
    generate_output(result, 10, "test_double_release_page");
    result = test_out_of_bounds_release();
    generate_output(result, 11, "test_out_of_bounds_release");
    result = test_evict_lru_page();
    generate_output(result, 12, "test_evict_lru_page");
    result = test_evict_all_pages();
    generate_output(result, 13, "test_evict_all_pages");

    return 0;
}