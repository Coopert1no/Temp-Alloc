/*
    This is a very simple single header temporary allocator with overflow pages insipired by the 'temp' allocator from the Jai programming language.
    It is highly recomended to set the max_capacity value according to the memory usage in your game/app, because the less allocations we do,
    the faster it will perform. Also there is less free() calls, less memory fragmentation and so on.
    All this information is available with temp_track_allocation_info() and temp_get_alloc_info() procedures.

    Some Notes:
        * This library uses a global Temp_Storage object, so it's not really thread safe.

    How to use this:
    * create a file and define TEMP_ALLOC_IMPLEMENTATION in it, then include temp_alloc.h.
      #define TEMP_ALLOC_IMPLEMENTATION
      #include "temp_alloc.h"
    * From now on, just include temp_alloc.h everywhere you want to.
    * call temp_init(size_t given_capacity) to initialize the allocator.
      if given_capacity is set to 0, then DEFAULT_TEMP_ALLOC_CAPACITY_SIZE will be used. It is 64 MB for now.
    * temp_alloc(size_t size) to allocate memory in bytes.
    * temp_free() doesn't do anything. It needed only for the stl allocator.
      This allacator is not really design to free things, so you know...
    * temp_realloc(void* old_memory, size_t old_size, size_t new_size) will reallocate your memory. But note: it won't free anything! So the data won't change or anything.
    * temp_reset() to reset the allocator. Usually you would want to call this at the end of your game/app loop.
    * temp_deinit() to free all the memory and deinit the allocator.
    * temp_printf(const char* format, ...) will return a formatted temporary string.
    * temp_copy_string(const char* c_string) will return a copied temporary string.

    * You can use temp_alloc_stl in stl containers.
    Example:
        std::vector<int, temp_alloc_stl<int> temp_vec;
        temp_vec.push_back(10);
        temp_vec.push_back(15);
        temp_vec.push_back(11);

    NOTE: This is important!!!
        Your stl contaier HAS to be destroyed before resetting the temporary storage!!
        Otherwise, if it will try to free the memory from a page that was already freed before... Well, you won't have a good time. So keep that in mind!

    * temp_set_alloc_proc will set your allocation procedure callback when allocating memory. It's standart malloc by default.
    * temp_set_free_proc  will set your free procedure callback when allocating memory. It's standart free by default.
*/

#ifndef __TEMP_ALLOC__
#define __TEMP_ALLOC__

#define DEFAULT_TEMP_ALLOC_CAPACITY_SIZE_MB 64
#define DEFAULT_TEMP_ALLOC_CAPACITY_SIZE DEFAULT_TEMP_ALLOC_CAPACITY_SIZE_MB * 1024 * 1024
#define ALIGMENT_BYTES sizeof(size_t)

typedef struct
{
    // NOTE: All this data gets reset after temp_reset() call.
    size_t max_allocation;
    size_t allocation_count;
    size_t average_allocation;
    size_t total_allocated_bytes;
    size_t overflow_pages_allocated;
} Temp_Alloc_Info;

typedef struct
{
    size_t current_size;
    size_t max_capacity;

    void* data;
    void* at;
    void* next;
} Overflow_Page;

typedef struct
{
    void* (*alloc_proc)(size_t);
    void (*free_proc)(void*);

    bool track_allocation_info;
    void* data;
    void* at;
    size_t max_capacity;
    size_t current_size;
    size_t original_capacity;

    Overflow_Page* overflow_page;

    Temp_Alloc_Info info;
} Temp_Storage;

void  temp_init(size_t given_capacity);
void  temp_set_alloc_proc(void* (*alloc_proc)(size_t));
void  temp_set_free_proc(void (*free_proc)(void*));
void  temp_track_allocation_info(bool track_status);
void* temp_alloc(size_t size_to_alloc);
char* temp_printf(const char* format, ...);
char* temp_copy_string(const char* c_string);
char* temp_copy_string_size(const char* c_string, size_t size);
void  temp_free(void*);
void* temp_realloc(void* old_memory, size_t old_size, size_t new_size);
void  temp_reset();
void  temp_deinit();

Temp_Alloc_Info temp_get_alloc_info();

static Overflow_Page* _alloc_new_page(size_t size);

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>

template<class type>
struct temp_alloc_stl
{
    typedef type value_type;
    typedef type* pointer;
    typedef value_type& reference;
    typedef value_type const& const_reference;
    typedef value_type const* const_pointer;
    typedef std::size_t       size_type;
    typedef std::ptrdiff_t    difference_type;

    template <class type_other> struct rebind { typedef temp_alloc_stl<type_other> other; };
    temp_alloc_stl() noexcept = default;
    temp_alloc_stl(const temp_alloc_stl&) noexcept = default;
    template<class type_other> temp_alloc_stl(const temp_alloc_stl<type_other>&) noexcept { }

    temp_alloc_stl  select_on_container_copy_construction() const { return *this; }
    void            deallocate(type* p, size_type) { temp_free(p); }

    pointer allocate(size_t count, const void* = 0) { return static_cast<pointer>(temp_alloc(count * sizeof(value_type))); }

    size_type     max_size() const noexcept { return (PTRDIFF_MAX / sizeof(value_type)); }
    pointer       address(reference x) const { return &x; }
    const_pointer address(const_reference x) const { return &x; }
};

#endif // __cplusplus

#ifdef TEMP_ALLOC_IMPLEMENTATION
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

static Temp_Storage g_temp_storage;

static Overflow_Page* _alloc_new_page(size_t size)
{
    Overflow_Page new_page = { 0 };
    g_temp_storage.info.overflow_pages_allocated += 1;

    // if we've got a very large allocation, then allocate a block with this size + max_capacity
    if (size > g_temp_storage.max_capacity)
        new_page.max_capacity = size + g_temp_storage.max_capacity;
    else
        new_page.max_capacity = g_temp_storage.max_capacity;

    new_page.data = g_temp_storage.alloc_proc(new_page.max_capacity);
    new_page.at = new_page.data;
    new_page.next = NULL;

    if (g_temp_storage.overflow_page == NULL)
    {
        g_temp_storage.overflow_page = (Overflow_Page*)g_temp_storage.alloc_proc(sizeof(Overflow_Page));
        assert(g_temp_storage.overflow_page != NULL);
        *g_temp_storage.overflow_page = new_page;
        return g_temp_storage.overflow_page;
    }
    else
    {
        Overflow_Page* current_page = g_temp_storage.overflow_page;

        while (current_page->next != NULL)
            current_page = (Overflow_Page*)current_page->next;

        current_page->next = (Overflow_Page*)g_temp_storage.alloc_proc(sizeof(Overflow_Page));
        assert(current_page->next != NULL);
        current_page = (Overflow_Page*)current_page->next;
        *current_page = new_page;
        return current_page;
    }

    assert(false);
    return NULL;
}

void temp_init(size_t given_capacity)
{
    size_t capacity = 0;
    if (given_capacity == 0)
        capacity = DEFAULT_TEMP_ALLOC_CAPACITY_SIZE;

    // Default alloc proc is malloc.
    temp_set_alloc_proc(&malloc);
    temp_set_free_proc(&free);

    g_temp_storage.data = g_temp_storage.alloc_proc(capacity);
    g_temp_storage.at = g_temp_storage.data;

    assert(g_temp_storage.data != NULL);

    g_temp_storage.max_capacity = capacity;
    g_temp_storage.current_size = 0;
    g_temp_storage.overflow_page = NULL;
    g_temp_storage.original_capacity = capacity;
    g_temp_storage.track_allocation_info = false;
}

void temp_set_alloc_proc(void* (*alloc_proc)(size_t))
{
    g_temp_storage.alloc_proc = alloc_proc;
}
void temp_set_free_proc(void (*free_proc)(void*))
{
    g_temp_storage.free_proc = free_proc;
}

void temp_track_allocation_info(bool track_status)
{
    g_temp_storage.track_allocation_info = track_status;
}

void* temp_alloc(size_t size_to_alloc)
{
    const size_t size = (size_to_alloc + ALIGMENT_BYTES-1) & ~ALIGMENT_BYTES-1;

    if (g_temp_storage.track_allocation_info)
    {
        g_temp_storage.info.allocation_count += 1;
        if (size > g_temp_storage.info.max_allocation)
            g_temp_storage.info.max_allocation = size_to_alloc;

        g_temp_storage.info.total_allocated_bytes += size;
    }

    if ((g_temp_storage.max_capacity - g_temp_storage.current_size) <= size)
    {
        Overflow_Page* page = _alloc_new_page(size);

        g_temp_storage.at = page->data;
        g_temp_storage.max_capacity = page->max_capacity;
        g_temp_storage.current_size = 0;
    }

    void* result = g_temp_storage.at;
    assert(result != NULL);

    char* data = (char*)g_temp_storage.at;
    data += size;

    g_temp_storage.at = data;
    g_temp_storage.current_size += size;
    return result;
}

char* temp_printf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    size_t buffer_size = vsnprintf(NULL, 0, format, args);
    va_end(args);

    char* buf = (char*)temp_alloc(buffer_size + 1);

    va_start(args, format);
    vsnprintf(buf, buffer_size + 1, format, args);
    va_end(args);
    return buf;
}

char* temp_copy_string(const char* c_string)
{
    const size_t c_string_size = strlen(c_string);
    char* new_string = (char*)temp_alloc(c_string_size + 1);
    new_string = (char*)memcpy(new_string, c_string, c_string_size + 1);
    return new_string;
}

char* temp_copy_string_size(const char* c_string, size_t size)
{
    char* new_string = (char*)temp_alloc(size + 1);
    new_string = (char*)memcpy(new_string, c_string, size + 1);
    return new_string;
}

void temp_free(void*) { /* Do nothing. */ }

void* temp_realloc(void* old_memory, size_t old_size, size_t new_size)
{
    void* memory = temp_alloc(new_size);
    return memcpy(memory, old_memory, old_size);
}

Temp_Alloc_Info temp_get_alloc_info()
{
    Temp_Alloc_Info info = { 0 };
    if (g_temp_storage.track_allocation_info)
    {
        info = g_temp_storage.info;
        info.average_allocation = info.total_allocated_bytes / info.allocation_count;
    }
    return info;
}

void temp_reset()
{
    // Free allocated pages.
    Overflow_Page* current_page = g_temp_storage.overflow_page;
    Overflow_Page* prev_page = NULL;

    if (current_page != NULL)
    {
        while (current_page->next != NULL)
        {
            g_temp_storage.free_proc(prev_page);
            g_temp_storage.free_proc(current_page->data);

            prev_page = current_page;
            current_page = (Overflow_Page*)current_page->next;
        }
        g_temp_storage.free_proc(prev_page);
        g_temp_storage.free_proc(current_page->data);
    }

    // Reset the storage.
    g_temp_storage.overflow_page = NULL;
    g_temp_storage.at = g_temp_storage.data;
    g_temp_storage.current_size = 0;
    g_temp_storage.max_capacity = g_temp_storage.original_capacity;

    // Reset allocation info.
    if (g_temp_storage.track_allocation_info)
        g_temp_storage.info = { 0 };
}

void temp_deinit()
{
    g_temp_storage.free_proc(g_temp_storage.data);

    g_temp_storage.data = NULL;
    g_temp_storage.at = NULL;
    g_temp_storage.current_size = 0;
    g_temp_storage.max_capacity = 0;
}

#endif // TEMP_ALLOC_IMPLEMENTATION

#endif // __TEMP_ALLOC__