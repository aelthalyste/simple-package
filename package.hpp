#pragma once
#include <stdint.h>

typedef uint32_t EntryID;

static uint32_t PACKAGE_ENTRY_NO_TAG        = 0;
static uint32_t PACKAGE_HEADER_MAGIC_NUMBER = 'SIMP';
static uint32_t PACKAGE_TOC_MAGIC_NUMBER    = 'TOC!';



struct Package_Entry {
    uint32_t       name_len;
    const char     *name;
    
    EntryID        identifier; // unique identifier for the entry in the package. 
    
    uint32_t       user_tag;
    uint64_t       offset_from_start_of_file;
    
    uint64_t       data_len;
    const void     *data;
};

struct Package_Header {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t reserved00;

    uint64_t toc_offset;
    uint64_t reserved01;
};

struct Package_Creator {
    Package_Entry *entries;
    uint64_t entry_count;
    uint64_t entry_cap;

    bool build__internal(void *writer_context);
    bool (*_writer_function)(void *context, const void *data, uint64_t len);

    void add_entry(const char *entry_name, const void *data, uint64_t data_len, uint32_t user_tags);
    bool build_package_to_file(const char *OutputFile);
    
    bool     build_package_to_memory(void *bf, uint64_t bf_size);
    uint64_t calculate_size_needed_for_package();
};

void init_package_creator(Package_Creator *result);
void free_package_creator(Package_Creator *pc);


struct Package_Reader {
    uint8_t  *data;
    uint64_t data_len;

    uint8_t *toc; // points to actual toc, doesn't contain magic number
    Package_Header header;

    uint8_t do_i_own_data;// if true, free_package_reader will call free(data);

    // returns entry data, NULL if package doesn't contain the entry
    void *get_entry(const char *name, uint64_t *entry_size);
    void resolve_entry(void *it, char **name, uint64_t *entry_offset, uint64_t *entry_size);
    void *iterate_entries(void *it);
    void *skip_to_next_entry(void *entry);

};

bool init_package_reader_from_memory(Package_Reader *reader, void *memory, uint64_t mem_len);
bool init_package_reader_from_file(Package_Reader *reader, const char *fn);
void free_package_reader(Package_Reader *reader);
bool flush_reader_to_file(Package_Reader *reader, const char *FP);





#ifdef PACKAGE_IMPLEMENTATION

#define _CRT_SECURE_NO_WARNINGS 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct Package_Memory_Writer_Context {
    uint64_t used;
    uint64_t cap;
    void *data;
};


struct Memory_Builder {
    void *data;
    uint64_t len;
    uint64_t cap;
};

static inline void init_memory_builder(Memory_Builder *builder, uint64_t initial_cap) {
    memset(builder, 0, sizeof(*builder));
    builder->cap  = initial_cap;
    builder->data = malloc(builder->cap);
}

static inline void free_memory_builder(Memory_Builder *builder) {
    free(builder->data);
    memset(builder, 0, sizeof(*builder));
}

static inline void builder_append(Memory_Builder *builder, const void *data, uint64_t len) {
    if (builder->len + len >= builder->cap) {
        builder->cap *= 2;
        builder->data = realloc(builder->data, builder->cap);
    }

    memcpy((char *)builder->data + builder->len, data, len);
    builder->len += len;
}

static inline void builder_reset(Memory_Builder *builder) {
    builder->len = 0;
}





static bool fwrite_wrapper(void *context, const void *data, uint64_t data_len) {
    // @TODO : 64bit not supported..
    if (data_len > 1024ull * 1024ull * 1024ull * 2)
        return false;
    return fwrite(data, data_len, 1, (FILE *)context);
}

static bool memory_writer_wrapper(void *context, const void *data, uint64_t data_len) {
    auto ctx = (Package_Memory_Writer_Context *)context;
    if(data_len+ctx->used > ctx->cap)
        return false;
    memcpy((uint8_t *)ctx->data + ctx->used, data, data_len);
    ctx->used+=data_len;
    return true;
}

static bool dummy_writer(void *context, const void *data, uint64_t data_len) {
    (void)(data);
    uint64_t *v = (uint64_t *)context;
    *v += data_len;
    return true;
}



void init_package_creator(Package_Creator *result) {
    memset(result, 0, sizeof(*result));
    result->entry_cap = 32;
    result->entries = (Package_Entry *)calloc(result->entry_cap, sizeof(result->entries[0]));
}

void free_package_creator(Package_Creator *pc) {
    if (pc) {
        if (pc->entries != NULL) {
            free(pc->entries);
        }
        memset(pc, 0, sizeof(*pc));
    }
}

void Package_Creator::add_entry(const char *entry_name, const void *data, uint64_t data_len, uint32_t user_tags) {

    if (entry_count == entry_cap) {
        entry_cap *= 2;
        entries = (Package_Entry *)realloc(entries, entry_cap*sizeof(entries[0])); 
    }

    Package_Entry *entry   = &entries[entry_count];
    ++entry_count;

    entry->name             = entry_name;
    entry->name_len         = (uint32_t)strlen(entry_name) + 1; // for null termination
    entry->identifier       = 0;
    entry->user_tag         = user_tags;
    entry->data             = data;
    entry->data_len         = data_len;
}

bool Package_Creator::build__internal(void *writer_context) {
    
    Memory_Builder builder;
    init_memory_builder(&builder, 1024 * 8);

    // write header
    {
        Package_Header header;
        memset(&header, 0, sizeof(header));
        header.magic      = PACKAGE_HEADER_MAGIC_NUMBER;
        header.toc_offset = sizeof(header);

        for(int i=0;i<entry_count;++i){
            header.toc_offset += entries[i].data_len;
        }

        builder_append(&builder, &header, sizeof(header));
        if (!_writer_function(writer_context, builder.data, builder.len)) {
            fprintf(stderr, "Package_Creator error : unable to write header!");
            goto ERROR;
        }
    }


    // write entry datas!
    {
        uint64_t offset_from_start_of_file = builder.len;
        for(int i=0;i<entry_count;++i) {
            Package_Entry *entry = &entries[i];
            entry->offset_from_start_of_file = offset_from_start_of_file;
            
            if (!_writer_function(writer_context, entry->data, entry->data_len)) {
                fprintf(stderr, "Package_Creator error : unable to write entry data. Entry name was %.*s", entry->name_len, entry->name);
                goto ERROR;
            }

            offset_from_start_of_file += entry->data_len;
        }
    }


    // write TOC
    // first 4 bytes indicates length of name,
    {
        builder_reset(&builder);
        uint32_t toc_magic = PACKAGE_TOC_MAGIC_NUMBER;
        builder_append(&builder, &toc_magic, sizeof(toc_magic));

        for(int i =0;i<entry_count;++i) {
            Package_Entry *entry = &entries[i];
            builder_append(&builder, &entry->name_len                 , sizeof(entry->name_len));
            builder_append(&builder, entry->name                      , entry->name_len);
            builder_append(&builder, &entry->identifier               , sizeof(entry->identifier));
            builder_append(&builder, &entry->user_tag                 , sizeof(entry->user_tag));
            builder_append(&builder, &entry->offset_from_start_of_file, sizeof(entry->offset_from_start_of_file));
            builder_append(&builder, &entry->data_len                 , sizeof(entry->data_len));
        }   


        if (!_writer_function(writer_context, builder.data, builder.len)) {
            fprintf(stderr, "Unable to put TOC !");
            goto ERROR;
        }
    }


    free_memory_builder(&builder);
    return true;


    ERROR:
    if (builder.data != NULL) 
        free_memory_builder(&builder);

    return false;
}


bool Package_Creator::build_package_to_memory(void *memory, uint64_t cap) {
    Package_Memory_Writer_Context ctx;
    ctx.data = memory;
    ctx.cap  = cap;
    ctx.used = 0;
    _writer_function = memory_writer_wrapper;
    return build__internal((void *)&ctx);
}

uint64_t Package_Creator::calculate_size_needed_for_package() {
    uint64_t size_ctx = 0;
    _writer_function = dummy_writer;
    build__internal(&size_ctx);
    return size_ctx;
}                                                                      

bool Package_Creator::build_package_to_file(const char *fn) {
    bool res = false;
    FILE *file = fopen(fn, "wb");
    if (file!=NULL) {
        _writer_function = fwrite_wrapper;
        res = build__internal((void *)file);
        fclose(file);
    }
    return res;
}



void Package_Reader::resolve_entry(void *it, char **name, uint64_t *entry_offset, uint64_t *entry_size) {
    
    uint8_t *n = (uint8_t *)it;
    if (n>=data+data_len) {
        *name = NULL;
        *entry_offset = 0;
        *entry_size   = 0;
        return;
    }

    uint32_t name_len = *(uint32_t *)n;
    n += 4; 
    *name = (char *)n; 

    n += name_len;
    n += 4; // identifier
    n += 4; // user_tag

    if (n < data+data_len) {
        *entry_offset = *(uint64_t *)n;
    }
    n += 8; // offset_from_start;

    if (n < data+data_len) {
        *entry_size = *(uint64_t *)n;
    }
    n += 8; // data_len;
}

void * Package_Reader::iterate_entries(void *it) {
    
    if (it == NULL) {
        // first time
        return toc;
    }
    
    return skip_to_next_entry(it);
}

void * Package_Reader::skip_to_next_entry(void *it) {
    
    if ((uint8_t *)it >= data+data_len)
        return NULL;

    uint8_t *n = (uint8_t *)it;
    uint32_t name_len = *(uint32_t *)it;

    n += sizeof(Package_Entry::name_len);        // name len
    n += name_len; // name
    n += sizeof(Package_Entry::identifier); // identifier
    n += sizeof(Package_Entry::user_tag); // user_tag
    n += sizeof(Package_Entry::offset_from_start_of_file); // offset_from_start
    n += sizeof(Package_Entry::data_len); // data_len

    if (n >= data+data_len)
        return NULL;

    return n;
}

void * Package_Reader::get_entry(const char *name, uint64_t *out_entry_size) {    

    for (void *entry = iterate_entries(NULL); entry; entry = iterate_entries(entry)) {
        char *entry_name = NULL;
        
        uint64_t entry_offset = 0;
        uint64_t entry_size   = 0;

        resolve_entry(entry, &entry_name, &entry_offset, &entry_size);
        if (strcmp(entry_name, name) == 0) {
            void *result = data + entry_offset;
            *out_entry_size = entry_size;
            return result;
        }
    }

    *out_entry_size = 0;
    return NULL;
}

void free_package_reader(Package_Reader *reader) {
    if (reader->do_i_own_data) {
        free(reader->data);
    }
    memset(reader, 0, sizeof(*reader));
}

bool init_package_reader_from_file(Package_Reader *reader, const char *fn) {
    
    bool result = false;

    FILE *file = fopen(fn, "rb");
    if (file) {
        
        // get file size and read that much
        if (0 == fseek(file, 0, SEEK_END)) {
            long fs = ftell(file);
            if (fs != -1) {
                if (0 == fseek(file, 0, SEEK_SET)) {
                    
                    // allocate buffer then read
                    void *buffer = malloc(fs);
                    if (1 == fread(buffer, fs, 1, file)) {
                        

                        // actual package initialization is done in this function.
                        if (init_package_reader_from_memory(reader, buffer, fs)) {
                            reader->do_i_own_data = true;
                            result = true;
                        }
                        else {
                            free(buffer);
                        }

                    }
                    else {
                        free(buffer);
                    }

                }
            }
        }

        fclose(file);
    }

    if (!result)
        memset(reader, 0, sizeof(*reader));

    return result;
}

bool init_package_reader_from_memory(Package_Reader *reader, void *memory, uint64_t mem_len) {


    if (mem_len <= sizeof(Package_Header))
        return false;

    memset(reader,0,sizeof(*reader));
    
    bool result = false;
    reader->data     = (uint8_t *)memory;
    reader->data_len = mem_len;
    reader->header   = *(Package_Header *)memory;

    if (reader->header.magic == PACKAGE_HEADER_MAGIC_NUMBER) {
        
        if (mem_len >= reader->header.toc_offset) {
            reader->toc = reader->data + reader->header.toc_offset;
            if (*(uint32_t *)reader->toc == PACKAGE_TOC_MAGIC_NUMBER) {
                reader->toc += 4;
                // @Incomplete : 
                // @TODO       : do full validation of table of contents.
                result = true;
            }
            else {
                // toc magic number doesnt match!
            }
        }
        else {
            // memory wasnt big enough to hold toc, but header magic number matched!
        }

    }
    else {
        // header magic number didnt match!
    }

    return result;
}


bool flush_reader_to_file(Package_Reader *reader, const char *FP) {
    bool ret = false;
    FILE *f = fopen(FP, "wb");
    if (f) {
        if (1 == fwrite(reader->data, reader->data_len, 1, f)) {
            // ok!
            ret = true;
        }
        else {
            fprintf(stderr, "flush_reader_to_file failed, unable to write to file %s\n", FP); 
        }

        fclose(f);
    }
    else {
        fprintf(stderr, "flush_reader_to_file failed, unable to create file %s\n", FP);
    }
    return ret;
}



#endif


