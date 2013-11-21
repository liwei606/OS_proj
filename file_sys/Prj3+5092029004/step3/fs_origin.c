#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#undef DEBUG

int NUM_SECTORS(void);
int FREELIST_NSEC(void);
int ROOT_PAGE_NUM(void);

// You can set this value to the actual number of sectors
static int s_num_sectors = 2560 * 1024;

int NUM_SECTORS() {
    return s_num_sectors;
}

int FREELIST_NSEC() {
    return NUM_SECTORS() / 256;
}

int ROOT_PAGE_NUM() {
    return FREELIST_NSEC();
}

enum {
    CONTENT_BYTES_PER_PAGE = 252,
    INODE_NUM = 10000,
    INODE_MAGIC_NUMBER = 0xCAFE
};

typedef struct {
    char *c;
} Storage;

enum { INODE_FILE, INODE_FOLDER };

typedef struct {
    int page_num; // do not save
    int type;
    int filesize;
    int firstpage;
} Inode;

struct FileSystem;

typedef struct {
    struct FileSystem *fs; // reference
    Inode *inode;
} File;

#define AS_FILE(x) ((File *)(x))

typedef struct {
    char cname[4096];
    int page_num;
} FolderItem;

typedef struct {
    File file;
    int nitem;
    FolderItem *items;
} Folder;

typedef struct {
    struct FileSystem *fs; // reference
    int max_page_num;
    int nslot;
    int *slots;
} Freelist;

typedef struct FileSystem {
    Storage *stor;
    Freelist *freelist;
    Inode *cur;
    int ninode;
    Inode *inodes[INODE_NUM];
} FileSystem;

int util_readint(char *array, int offset);
void util_writeint(char *array, int offset, int value);

char storage_readchar(Storage *stor, int page_num, int offset);
void storage_writechar(Storage *stor, int page_num, int offset, char value);
int storage_readint(Storage *stor, int page_num, int offset);
void storage_writeint(Storage *stor, int page_num, int offset, int value);
void storage_readpage(Storage *stor, int page_num, char *buf);
void storage_writepage(Storage *stor, int page_num, const char *buf);

Inode* inode_new(int page_num);
void inode_free(Inode **inode);

void file_init(File *file, FileSystem *fs, Inode *inode);
File* file_new(FileSystem *fs, Inode *inode);
void file_free(File **file);
void file_get_contents(File *file, char *buf);
void file_put_contents(File *file, const char *buf, int buflen);

Freelist* freelist_new(FileSystem *fs);
int in_freelist(int sec, Freelist *freelist);
void freelist_free(Freelist *freelist);
int freelist_allocate(Freelist *freelist);
void freelist_release(Freelist *freelist, int page_num);

Folder* folder_open(FileSystem *fs, Inode *inode);
void folder_close(Folder **folder);
int folder_get_child(Folder *folder, const char *cname);
void folder_add_child(Folder *folder, const char *cname, int page_num);
void folder_remove_child(Folder *folder, const char *cname);
Inode* folder_lookup(FileSystem *fs, Inode *folder_inode, const char *path);
int skip_folder_item(const char *s);
void sort_strings(char **a, int n);
void folder_dump(FileSystem *fs, Inode *folder_inode, FILE *outfile);

Inode* fs_load_inode(FileSystem *fs, int page_num);
void fs_save_inode(FileSystem *fs, Inode *inode);
void fs_init(FileSystem *fs);
FileSystem* fs_new(void);
void fs_free(FileSystem **fs);
int fs_format(FileSystem *fs);
int fs_exists(FileSystem *fs, const char *f);
int fs_isfile(FileSystem *fs, const char *f);
int fs_isdir(FileSystem *fs, const char *d);
void fs_split_path(const char *path, char *ppath, char *cname);
int fs_create(FileSystem *fs, const char *f);
int fs_mkdir(FileSystem *fs, const char *d);
int fs_unlink(FileSystem *fs, const char *f);
int fs_chdir(FileSystem *fs, const char *path);
int fs_rmdir(FileSystem *fs, const char *d);
void fs_ls(FileSystem *fs, FILE *fp);
void fs_cat(FileSystem *fs, const char *f, FILE *fp);
int fs_write(FileSystem *fs, const char *f, int l, const char *data);
int fs_insert(FileSystem *fs, const char *f, int pos, int l, const char *data);
int fs_delete(FileSystem *fs, const char *f, int pos, int l);

int process_request(const char *line, FILE *fp, FileSystem *fs);

int util_readint(char *array, int offset) {
    union {
        char c[4];
        int value;
    } buf;
    int i;
    
    for (i = 0; i < 4; i++) buf.c[i] = array[offset + i];
    return buf.value;
}

void util_writeint(char *array, int offset, int value) {
    union {
        char c[4];
        int value;
    } buf;
    int i;
    
    buf.value = value;
    for (i = 0; i < 4; i++) array[offset + i] = buf.c[i];
}

char storage_readchar(Storage *stor, int page_num, int offset) {
    return stor->c[page_num * 256 + offset];
}

void storage_writechar(Storage *stor, int page_num, int offset, char value) {
    stor->c[page_num * 256 + offset] = value;
}

int storage_readint(Storage *stor, int page_num, int offset) {
    return util_readint(stor->c, page_num * 256 + offset);
}

void storage_writeint(Storage *stor, int page_num, int offset, int value) {
    util_writeint(stor->c, page_num * 256 + offset, value);
}

void storage_readpage(Storage *stor, int page_num, char *buf) {
    memcpy(buf, stor->c + page_num * 256, 256);
}

void storage_writepage(Storage *stor, int page_num, const char *buf) {
    memcpy(stor->c + page_num * 256, buf, 256);
}

Inode* inode_new(int page_num) {
    Inode *inode = NULL;
    
    inode = (Inode *) malloc(sizeof(Inode));
    inode->page_num = page_num;
    return inode;
}

void inode_free(Inode **inode) {
    if (inode && *inode) {
        free(*inode);
        *inode = NULL;
    }
}

Inode* fs_load_inode(FileSystem *fs, int page_num) {
    int i = 0;
    Inode *inode = NULL;
    int magic_number = 0;
    
    for (i = 0; i < fs->ninode; ++i) {
        if (fs->inodes[i]->page_num == page_num) {
            return fs->inodes[i];
        }
    }
    magic_number = storage_readint(fs->stor, page_num, CONTENT_BYTES_PER_PAGE);
    if (magic_number != INODE_MAGIC_NUMBER) {
        return NULL;
    }
    if (fs->ninode == INODE_NUM) {
        inode_free(&(fs->inodes[0]));
        fs->ninode--;
        for (i = 0; i < fs->ninode; ++i) {
            fs->inodes[i] = fs->inodes[i + 1];
        }
    }
    fs->inodes[fs->ninode++] = inode = inode_new(page_num);
    inode->type = storage_readint(fs->stor, page_num, 0);
    inode->filesize = storage_readint(fs->stor, page_num, 4);
    inode->firstpage = storage_readint(fs->stor, page_num, 16);
    return inode;
}

void fs_save_inode(FileSystem *fs, Inode *inode) {
    storage_writeint(fs->stor, inode->page_num, 0, inode->type);
    storage_writeint(fs->stor, inode->page_num, 4, inode->filesize);
    storage_writeint(fs->stor, inode->page_num, 16, inode->firstpage);
    storage_writeint(fs->stor, inode->page_num, CONTENT_BYTES_PER_PAGE, INODE_MAGIC_NUMBER);
}

void file_init(File *file, FileSystem *fs, Inode *inode) {
    file->fs = fs;
    file->inode = inode;
}

File* file_new(FileSystem *fs, Inode *inode) {
    File *file = NULL;
    
    file = (File *) malloc(sizeof(File));
    file_init(file, fs, inode);
    return file;
}

void file_free(File **file) {
    if (file && *file) {
        free(*file);
        *file = NULL;
    }
}

void file_get_contents(File *file, char *buf) {
    int page = 0;
    int left = 0;
    int offset = 0;
    
    page = file->inode->firstpage;
    left = file->inode->filesize;
    offset = 0;
    while (page) {
        char buffer[256];
        
        storage_readpage(file->fs->stor, page, buffer);
        if (left <= CONTENT_BYTES_PER_PAGE) {
            memcpy(buf + offset, buffer, left);
            buf[offset + left] = 0;
            offset += left;
            left = 0;
        } else {
            memcpy(buf + offset, buffer, CONTENT_BYTES_PER_PAGE);
            offset += CONTENT_BYTES_PER_PAGE;
            left -= CONTENT_BYTES_PER_PAGE;
        }
        page = util_readint(buffer, CONTENT_BYTES_PER_PAGE);
    }
}

void file_put_contents(File *file, const char *buf, int buflen) {
    int page = 0;
    int npage = 0;
    int nextpage = 0;
    int i = 0;
    
    page = file->inode->firstpage;
    while (page) {
        char buffer[256];
        
        storage_readpage(file->fs->stor, page, buffer);
        freelist_release(file->fs->freelist, page);
        page = util_readint(buffer, CONTENT_BYTES_PER_PAGE);
    }
    file->inode->firstpage = 0;
    file->inode->filesize = buflen;
    npage = ceil(1.0 * buflen / CONTENT_BYTES_PER_PAGE);
#ifdef DEBUG
    fprintf(stderr, "file_put_contents, npage=%d\n", npage);
#endif
    nextpage = 0;
    for (i = npage - 1; i >= 0; --i) {
        page = freelist_allocate(file->fs->freelist);
        storage_writepage(file->fs->stor, page, buf + i * CONTENT_BYTES_PER_PAGE);
        storage_writeint(file->fs->stor, page, CONTENT_BYTES_PER_PAGE, nextpage);
        nextpage = page;
    }
    file->inode->firstpage = nextpage;
}

Freelist* freelist_new(FileSystem *fs) {
    Freelist *freelist = NULL;
    int sec = 0;
    int islot = 0;
    
    freelist = (Freelist *) malloc(sizeof(Freelist));
    freelist->fs = fs;
    freelist->max_page_num = -1;
    freelist->nslot = 0;
    for (sec = 0; sec < NUM_SECTORS(); ++sec) {
        if (storage_readchar(fs->stor, sec / 256, sec % 256)) {
            freelist->max_page_num = sec;
        } else {
            freelist->nslot++;
        }
    }
    freelist->slots = (int *) malloc(sizeof(int) * (freelist->nslot + 1));
    for (sec = 0; sec < NUM_SECTORS(); ++sec) {
        if (!storage_readchar(fs->stor, sec / 256, sec % 256)) {
            freelist->slots[islot++] = sec;
        }
    }
    return freelist;
}

int in_freelist(int sec, Freelist *freelist) {
    int i = 0;
    
    for (i = 0; i < freelist->nslot; ++i)
        if (sec == freelist->slots[i])
            return 1;
    return 0;
}

void freelist_free(Freelist *freelist) {
    if (freelist) {
        int sec = 0;
        
        for (sec = 0; sec < NUM_SECTORS(); ++sec) {
            if (sec > freelist->max_page_num) {
                storage_writechar(freelist->fs->stor, sec / 256, sec % 256, 0);
            } else if (in_freelist(sec, freelist)) {
                storage_writechar(freelist->fs->stor, sec / 256, sec % 256, 0);
            } else {
                storage_writechar(freelist->fs->stor, sec / 256, sec % 256, 1);
            }
        }
        free(freelist->slots);
        free(freelist);
    }
}

int freelist_allocate(Freelist *freelist) {
    int page_num = -1;
    
    if (freelist->nslot > 0) {
        page_num = freelist->slots[--(freelist->nslot)];
    } else {
        page_num = ++(freelist->max_page_num);
    }
#ifdef DEBUG
    fprintf(stderr, "freelist allocate %d\n", page_num);
#endif
    return page_num;
}

void freelist_release(Freelist *freelist, int page_num) {
    int *new_slots = NULL;
    FileSystem *fs = NULL;
    int i = 0;
    
#ifdef DEBUG
    fprintf(stderr, "freelist release %d\n", page_num);
#endif
    new_slots = (int *) malloc(sizeof(int) * (freelist->nslot + 1));
    memcpy(new_slots, freelist->slots, sizeof(int) * freelist->nslot);
    new_slots[freelist->nslot] = page_num;
    free(freelist->slots);
    freelist->slots = new_slots;
    
    fs = freelist->fs;
    for (i = 0; i < fs->ninode; ++i) {
        if (fs->inodes[i]->page_num == page_num) {
            break;
        }
    }
    if (i < fs->ninode) {
        fs->ninode--;
        while (i < fs->ninode) {
            fs->inodes[i] = fs->inodes[i + 1];
            i++;
        }
    }
}

Folder* folder_open(FileSystem *fs, Inode *inode) {
    Folder *folder = NULL;
    char *buffer = NULL;
    int offset = 0;
    int i = 0;
    
    folder = (Folder *) malloc(sizeof(Folder));
    file_init(AS_FILE(folder), fs, inode);
    buffer = (char *) malloc(inode->filesize);
    file_get_contents(AS_FILE(folder), buffer);
#ifdef DEBUG
    fprintf(stderr, "folder_open buffer=");
    for (i = 0; i < inode->filesize; ++i) fprintf(stderr, " %x", buffer[i]);
    fprintf(stderr, "\n");
#endif
    folder->nitem = util_readint(buffer, 0);
#ifdef DEBUG
    fprintf(stderr, "folder_open, folder->nitem=%d\n", folder->nitem);
#endif
    folder->items = (FolderItem *) malloc(sizeof(FolderItem) * folder->nitem);
    offset = 4;
    for (i = 0; i < folder->nitem; ++i) {
        int cname_len = 0;
        
        cname_len = util_readint(buffer, offset);
        offset += 4;
        memcpy(folder->items[i].cname, buffer + offset, cname_len);
        folder->items[i].cname[cname_len] = 0;  // make it a string
        offset += cname_len;
        folder->items[i].page_num = util_readint(buffer, offset);
        offset += 4;
    }
    free(buffer);
    return folder;
}

void folder_close(Folder **folder) {
    if (folder && *folder) {
        int len = 0;
        int i = 0;
        char *buffer = NULL;
        int offset = 0;
        
        len = 4;
        for (i = 0; i < (*folder)->nitem; ++i) {
            len += 4;
            len += strlen((*folder)->items[i].cname);
            len += 4;
        }
        buffer = (char *) malloc(len);
        util_writeint(buffer, 0, (*folder)->nitem);
#ifdef DEBUG
        fprintf(stderr, "folder_close, (*folder)->nitem=%d\n", (*folder)->nitem);
#endif
        offset = 4;
        for (i = 0; i < (*folder)->nitem; ++i) {
            int cname_len = 0;
            
            cname_len = (int) strlen((*folder)->items[i].cname);
            util_writeint(buffer, offset, cname_len);
            offset += 4;
            memcpy(buffer + offset, (*folder)->items[i].cname, cname_len);
            offset += cname_len;
            util_writeint(buffer, offset, (*folder)->items[i].page_num);
            offset += 4;
        }
#ifdef DEBUG
        fprintf(stderr, "offset=%d, len=%d\n", offset, len);
        fprintf(stderr, "folder_close buffer=");
        for (i = 0; i < len; ++i) fprintf(stderr, " %x", buffer[i]);
        fprintf(stderr, "\n");
#endif
        file_put_contents(AS_FILE(*folder), buffer, len);
        free(buffer);
        free((*folder)->items);
        free(*folder);
        *folder = NULL;
    }
}

int folder_get_child(Folder *folder, const char *cname) {
    int i = 0;

#ifdef DEBUG
    fprintf(stderr, "folder_get_child, cname=`%s`\n", cname);
    fprintf(stderr, "> folder->nitem=%d\n", folder->nitem);
#endif
    for (i = 0; i < folder->nitem; ++i) {
#ifdef DEBUG
        fprintf(stderr, "> folder->items[%d].cname=`%s`\n", i, folder->items[i].cname);
#endif
        if (0 == strcmp(cname, folder->items[i].cname)) {
            return folder->items[i].page_num;
        }
    }
    return -1;
}

void folder_add_child(Folder *folder, const char *cname, int page_num) {
    FolderItem *new_items = NULL;
    
#ifdef DEBUG
    fprintf(stderr, "folder_add_child, cname=`%s`, page_num=%d\n", cname, page_num);
#endif
    new_items = (FolderItem *) malloc(sizeof(FolderItem) * (folder->nitem + 1));
    memcpy(new_items, folder->items, sizeof(FolderItem) * folder->nitem);
    strcpy(new_items[folder->nitem].cname, cname);
    new_items[folder->nitem].page_num = page_num;
    free(folder->items);
    folder->items = new_items;
    folder->nitem++;
}

void folder_remove_child(Folder *folder, const char *cname) {
    int i = 0;
    
    for (i = 0; i < folder->nitem; ++i) {
        if (0 == strcmp(cname, folder->items[i].cname)) {
            break;
        }
    }
    if (i < folder->nitem) {
        folder->nitem--;
        while (i < folder->nitem) {
            strcpy(folder->items[i].cname, folder->items[i + 1].cname);
            folder->items[i].page_num = folder->items[i + 1].page_num;
            i++;
        }
    }
}

Inode* folder_lookup(FileSystem *fs, Inode *folder_inode, const char *path) {
    char normal_path[4096] = "";
    char *p = NULL;
    Inode *cur = NULL;
    int childpage;
    
    strcpy(normal_path, "./");
    strcat(normal_path, path);
#ifdef DEBUG
    fprintf(stderr, "folder_lookup, path=`%s`, normal_path=`%s`\n", path, normal_path);
#endif
    p = normal_path;
    cur = folder_inode;
    while (p[0]) {
        size_t i = 0;
        char tmp[4096] = "";
        Folder *fd = NULL;
        
        if (!cur) {
#ifdef DEBUG
            fprintf(stderr, "cur is NULL\n");
#endif
            return NULL;
        }
        if (cur->type != INODE_FOLDER) {
#ifdef DEBUG
            fprintf(stderr, "cur is not a folder\n");
#endif
            return NULL;
        }
#ifdef DEBUG
        fprintf(stderr, "before folder_open: cur->page_num=%d, cur->filesize=%u\n", cur->page_num, cur->filesize);
#endif
        fd = folder_open(fs, cur);
        i = 0;
        while (i < strlen(p) && p[i] != '/') ++i;
        strncpy(tmp, p, i);
#ifdef DEBUG
        fprintf(stderr, "> tmp=`%s`\n", tmp);
#endif
        childpage = folder_get_child(fd, tmp);
#ifdef DEBUG
        fprintf(stderr, "> childpage=%d\n", childpage);
#endif
        cur = fs_load_inode(fs, childpage);
#ifdef DEBUG
        if (!cur) {
            fprintf(stderr, "> cur = fs_load_inode(fs, %d) returns NULL\n", childpage);
        }
#endif
        folder_close(&fd);
        p += (i + 1);
    }
    return cur;
}

int skip_folder_item(const char *s) {
    return 0 == strcmp("", s) || 0 == strcmp(".", s) || 0 == strcmp("..", s);
}

void sort_strings(char **a, int n) {
    int i, j;
    
    for (i = 0; i < n; ++i)
        for (j = 0; j < i; ++j)
            if (strcmp(a[j], a[i]) > 0) {
                char *t;
                
                t = a[j];
                a[j] = a[i];
                a[i] = t;
            }
}

void folder_dump(FileSystem *fs, Inode *folder_inode, FILE *outfile) {
    Folder *folder = NULL;
    int i = 0;
    int nfile = 0;
    int nfolder = 0;
    char **file_names;
    char **folder_names;
    int ifile = 0;
    int ifolder = 0;
    
    folder = folder_open(fs, folder_inode);
    for (i = 0; i < folder->nitem; ++i) {
        if (skip_folder_item(folder->items[i].cname)) continue;
        if (fs_load_inode(fs, folder->items[i].page_num)->type == INODE_FILE) {
            nfile++;
        } else {
            nfolder++;
        }
    }
    file_names = malloc(sizeof(char *) * nfile);
    folder_names = malloc(sizeof(char *) * nfolder);
    for (i = 0; i < folder->nitem; ++i) {
        if (skip_folder_item(folder->items[i].cname)) continue;
        if (fs_load_inode(fs, folder->items[i].page_num)->type == INODE_FILE) {
            file_names[ifile] = malloc(strlen(folder->items[i].cname) + 1);
            strcpy(file_names[ifile], folder->items[i].cname);
            ifile++;
        } else {
            folder_names[ifolder] = malloc(strlen(folder->items[i].cname) + 1);
            strcpy(folder_names[ifolder], folder->items[i].cname);
            ifolder++;
        }
    }
    sort_strings(file_names, nfile);
    sort_strings(folder_names, nfolder);
    for (i = 0; i < nfile; ++i) {
        if (i) fprintf(outfile, " ");
        fprintf(outfile, "%s", file_names[i]);
    }
    fprintf(outfile, " & ");
    for (i = 0; i < nfolder; ++i) {
        if (i) fprintf(outfile, " ");
        fprintf(outfile, "%s", folder_names[i]);
    }
    fprintf(outfile, "\n");
    fflush(outfile);
    free(file_names);
    free(folder_names);
    folder_close(&folder);
}

enum { OK = 0, ERROR };

void fs_init(FileSystem *fs) {
    fs->stor = (Storage *) malloc(sizeof(Storage));
    fs->stor->c = malloc(sizeof(char) * NUM_SECTORS() * 256);
    fs->freelist = freelist_new(fs);
    fs->cur = fs_load_inode(fs, ROOT_PAGE_NUM());
    fs->ninode = 0;
}

FileSystem* fs_new() {
    FileSystem *fs = NULL;
    
    fs = (FileSystem *) malloc(sizeof(FileSystem));
    fs_init(fs);
    return fs;
}

void fs_free(FileSystem **fs) {
    if (fs && *fs) {
        int i = 0;
        
        for (i = 0; i < (*fs)->ninode; ++i) {
            fs_save_inode(*fs, (*fs)->inodes[i]);
            free((*fs)->inodes[i]);
        }
        (*fs)->ninode = 0;
        freelist_free((*fs)->freelist);
        (*fs)->freelist = NULL;
        free(*fs);
        *fs = NULL;
    }
}

int fs_format(FileSystem *fs) {
    int i = 0;
    int sec = 0;
    char buffer[256] = "";
    
    for (i = 0; i < fs->ninode; ++i) {
        fs_save_inode(fs, fs->inodes[i]);
        free(fs->inodes[i]);
    }
    fs->ninode = 0;
    freelist_free(fs->freelist);
    fs->freelist = NULL;
    for (sec = 0; sec < FREELIST_NSEC(); ++sec) {
        for (i = 0; i < 256 / 4; ++i) {
            storage_writeint(fs->stor, sec, i * 4, 0);
        }
    }
    storage_writeint(fs->stor, ROOT_PAGE_NUM(), 0, INODE_FOLDER);
    storage_writeint(fs->stor, ROOT_PAGE_NUM(), 4, 31);
    storage_writeint(fs->stor, ROOT_PAGE_NUM(), 16, ROOT_PAGE_NUM() + 1);
    storage_writeint(fs->stor, ROOT_PAGE_NUM(), CONTENT_BYTES_PER_PAGE, INODE_MAGIC_NUMBER);
    util_writeint(buffer, 0, 3);
    // first item ""
    util_writeint(buffer, 4, 0);
    util_writeint(buffer, 8, ROOT_PAGE_NUM());
    // second item "."
    util_writeint(buffer, 12, 1);
    buffer[16] = '.';
    util_writeint(buffer, 17, ROOT_PAGE_NUM());
    // third item ".."
    util_writeint(buffer, 21, 2);
    buffer[25] = '.';
    buffer[26] = '.';
    util_writeint(buffer, 27, ROOT_PAGE_NUM());
    storage_writepage(fs->stor, ROOT_PAGE_NUM() + 1, buffer);
    fs->freelist = freelist_new(fs);
    fs->cur = fs_load_inode(fs, ROOT_PAGE_NUM());
    return OK;
}

int fs_exists(FileSystem *fs, const char *f) {
    return NULL != folder_lookup(fs, fs->cur, f);
}

int fs_isfile(FileSystem *fs, const char *f) {
    Inode *inode = NULL;
    
    inode = folder_lookup(fs, fs->cur, f);
    return inode && inode->type == INODE_FILE;
}

int fs_isdir(FileSystem *fs, const char *d) {
    Inode *inode = NULL;
    
    inode = folder_lookup(fs, fs->cur, d);
#ifdef DEBUG
    fprintf(stderr, "fs_isdir inode=%p\n", inode);
    if (inode) {
        fprintf(stderr, "INODE_FILE=%d, INODE_FOLDER=%d\n", INODE_FILE, INODE_FOLDER);
        fprintf(stderr, "fs_isdir inode->type=%d\n", inode->type);
    }
#endif
    return inode && inode->type == INODE_FOLDER;
}

void fs_split_path(const char *path, char *ppath, char *cname) {
    char *rpos = NULL;
    
    rpos = strrchr(path, '/');
    if (rpos) {
        if (ppath) strncpy(ppath, path, rpos - path);
        if (cname) strcpy(cname, rpos + 1);
    } else {
        if (ppath) strcpy(ppath, "");
        if (cname) strcpy(cname, path);
    }
}

int fs_create(FileSystem *fs, const char *f) {
    int p = 0;
    Inode *inode = NULL;
    char ppath[4096] = "";
    char cname[4096] = "";
    Folder *pfd = NULL;
    
    p = freelist_allocate(fs->freelist);
    if (p <= 1) return ERROR;
    inode = inode_new(p);
    if (!inode) return ERROR;
    inode->type = INODE_FILE;
    inode->filesize = 0;
    inode->firstpage = 0;
    fs_save_inode(fs, inode);
    inode_free(&inode);
    
    fs_split_path(f, ppath, cname);
    pfd = folder_open(fs, folder_lookup(fs, fs->cur, ppath));
    folder_add_child(pfd, cname, p);
    folder_close(&pfd);
    return OK;
}

int fs_mkdir(FileSystem *fs, const char *d) {
    int p = 0;
    Inode *inode = NULL;
    Folder *fd = NULL;
    char ppath[4096] = "";
    char cname[4096] = "";
    Folder *pfd = NULL;
    int parent_page_num = 0;
    
    p = freelist_allocate(fs->freelist);
    if (p <= 1) return ERROR;
    inode = inode_new(p);
    if (!inode) return ERROR;
    inode->type = INODE_FOLDER;
    inode->filesize = 0;
    inode->firstpage = 0;
    fs_save_inode(fs, inode);
    inode_free(&inode);
    
    fs_split_path(d, ppath, cname);
#ifdef DEBUG
    fprintf(stderr, "fs_mkdir, d=`%s`, ppath=`%s`, cname=`%s`\n", d, ppath, cname);
#endif
    
    pfd = folder_open(fs, folder_lookup(fs, fs->cur, ppath));
    parent_page_num = AS_FILE(pfd)->inode->page_num;
    folder_add_child(pfd, cname, p);
    folder_close(&pfd);
    
    fd = folder_open(fs, fs_load_inode(fs, p));
    fs_split_path(d, ppath, cname);
    folder_add_child(fd, "", ROOT_PAGE_NUM());
    folder_add_child(fd, ".", p);
    folder_add_child(fd, "..", parent_page_num);
    folder_close(&fd);
    return OK;
}

int fs_unlink(FileSystem *fs, const char *f) {
    Inode *inode = NULL;
    char ppath[4096] = "";
    char cname[4096] = "";
    Folder *pfd = NULL;
    
    if (ERROR == fs_write(fs, f, 0, "")) return ERROR;
    inode = folder_lookup(fs, fs->cur, f);
    if (!inode) return ERROR;
    freelist_release(fs->freelist, inode->page_num);
    fs_split_path(f, ppath, cname);
    pfd = folder_open(fs, folder_lookup(fs, fs->cur, ppath));
    folder_remove_child(pfd, cname);
    folder_close(&pfd);
    return OK;
}

int fs_chdir(FileSystem *fs, const char *path) {
    Inode *inode = NULL;
    
    inode = folder_lookup(fs, fs->cur, path);
    if (!inode) return ERROR;
    fs->cur = inode;
    return OK;
}

int fs_rmdir(FileSystem *fs, const char *d) {
    // TODO what if there are children in this directory?
    Inode *inode = NULL;
    char ppath[4096] = "";
    char cname[4096] = "";
    Folder *pfd = NULL;
    
    if (ERROR == fs_write(fs, d, 0, "")) return ERROR;
    inode = folder_lookup(fs, fs->cur, d);
    if (!inode) return ERROR;
    freelist_release(fs->freelist, inode->page_num);
    fs_split_path(d, ppath, cname);
    pfd = folder_open(fs, folder_lookup(fs, fs->cur, ppath));
    folder_remove_child(pfd, cname);
    folder_close(&pfd);
    return OK;
}

void fs_ls(FileSystem *fs, FILE *fp) {
    folder_dump(fs, fs->cur, fp);
}

void fs_cat(FileSystem *fs, const char *f, FILE *fp) {
    Inode *inode = NULL;
    char *data = NULL;
    File *file = NULL;
    
    inode = folder_lookup(fs, fs->cur, f);
    data = (char *) malloc(inode->filesize);
    file = file_new(fs, inode);
    file_get_contents(file, data);
    file_free(&file);
    fprintf(fp, "%s\n", data);
    fflush(fp);
    free(data);
}

int fs_write(FileSystem *fs, const char *f, int l, const char *data) {
    Inode *inode = NULL;
    File *file = NULL;
    
    inode = folder_lookup(fs, fs->cur, f);
    if (!inode) return ERROR;
    file = file_new(fs, inode);
    file_put_contents(file, data, l);
    file_free(&file);
    return OK;
}

int fs_insert(FileSystem *fs, const char *f, int pos, int l, const char *data) {
    Inode *inode = NULL;
    char *buffer = NULL;
    File *file = NULL;
    int i = 0;
    
    inode = folder_lookup(fs, fs->cur, f);
    if (!inode) return ERROR;
    buffer = (char *) malloc(inode->filesize + l);
    if (!buffer) return ERROR;
    file = file_new(fs, inode);
    file_get_contents(file, buffer);
    file_free(&file);
    if (pos > inode->filesize) {
        pos = inode->filesize;
    }
    for (i = inode->filesize - 1; i >= pos; --i) {
        buffer[i + l] = buffer[i];
    }
    memcpy(buffer + pos, data, l);
    file = file_new(fs, inode);
    file_put_contents(file, buffer, inode->filesize + l);
    file_free(&file);
    free(buffer);
    return OK;
}

int fs_delete(FileSystem *fs, const char *f, int pos, int l) {
    Inode *inode = NULL;
    char *data = NULL;
    File *file = NULL;
    int i = 0;
    
    inode = folder_lookup(fs, fs->cur, f);
    if (!inode) return ERROR;
    data = (char *) malloc(inode->filesize);
    if (!data) return ERROR;
    file = file_new(fs, inode);
    file_get_contents(file, data);
    file_free(&file);
    if (pos + l > inode->filesize) {
        l = inode->filesize - pos;
    }
    for (i = pos + l; i < inode->filesize; ++i) {
        data[i - l] = data[i];
    }
    file = file_new(fs, inode);
    file_put_contents(file, data, inode->filesize - l);
    file_free(&file);
    free(data);
    return OK;
}

enum { RESULT_EXIT, RESULT_DONE, RESULT_YES, RESULT_NO, RESULT_ELSE };

int process_request(const char *line, FILE *fp, FileSystem *fs) {
    char command[4096];
    
#ifdef DEBUG
    fprintf(stderr, "process request `%s`\n", line);
#endif
    sscanf(line, "%s", command);
#ifdef DEBUG
    fprintf(stderr, "command is `%s`\n", command);
#endif
    if (0 == strcmp("f", command)) {
        fs_format(fs);
        return RESULT_DONE;
    } else if (0 == strcmp("mk", command)) {
        char f[4096];
        char parent_path[4096];
        
        sscanf(line + 2, "%s", f);
#ifdef DEBUG
        fprintf(stderr, "> mk `%s`\n", f);
#endif
        if (fs_exists(fs, f)) {
#ifdef DEBUG
            fprintf(stderr, "> `%s` already exists\n", f);
#endif
            return RESULT_NO;
        } else {
#ifdef DEBUG
            fprintf(stderr, "> going to create `%s`\n", f);
#endif
        }
        fs_split_path(f, parent_path, NULL);
        if (fs_isdir(fs, parent_path)) {
            if (fs_create(fs, f)) {
#ifdef DEBUG
                fprintf(stderr, "> failed to create `%s`\n", f);
#endif
                return RESULT_NO;
            }
            return RESULT_YES;
        }
#ifdef DEBUG
        fprintf(stderr, "> parent path `%s` is not a directory\n", parent_path);
#endif
        return RESULT_NO;
    } else if (0 == strcmp("mkdir", command)) {
        char d[4096];
        char parent_path[4096];
        
        sscanf(line + 5, "%s", d);
        if (fs_exists(fs, d)) {
            return RESULT_NO;
        }
        fs_split_path(d, parent_path, NULL);
        if (fs_isdir(fs, parent_path)) {
            if (fs_mkdir(fs, d)) {
                return RESULT_NO;
            }
            return RESULT_YES;
        }
        return RESULT_NO;
    } else if (0 == strcmp("rm", command)) {
        char f[4096];
        
        sscanf(line + 2, "%s", f);
        if (fs_isfile(fs, f)) {
            if (fs_unlink(fs, f)) {
                return RESULT_NO;
            }
            return RESULT_YES;
        }
        return RESULT_NO;
    } else if (0 == strcmp("cd", command)) {
        char path[4096];
        
        sscanf(line + 2, "%s", path);
        if (fs_isdir(fs, path)) {
            if (fs_chdir(fs, path)) {
                return RESULT_NO;
            }
            return RESULT_YES;
        }
#ifdef DEBUG
        fprintf(stderr, "`%s` is not a directory\n", path);
#endif
        return RESULT_NO;
    } else if (0 == strcmp("rmdir", command)) {
        char d[4096];
        
        sscanf(line + 5, "%s", d);
        if (fs_isdir(fs, d)) {
            if (fs_rmdir(fs, d)) {
                return RESULT_NO;
            }
            return RESULT_YES;
        }
        return RESULT_NO;
    } else if (0 == strcmp("ls", command)) {
        fs_ls(fs, fp);
        return RESULT_ELSE;
    } else if (0 == strcmp("cat", command)) {
        char f[4096];
        
        sscanf(line + 3, "%s", f);
        if (fs_isfile(fs, f)) {
            fs_cat(fs, f, fp);
            return RESULT_ELSE;
        }
        return RESULT_NO;
    } else if (0 == strcmp("w", command)) {
        char f[4096];
        int l;
        char data[4096];
        
        sscanf(line + 1, "%s %d %[^\n]", f, &l, data);
        if (fs_isfile(fs, f)) {
            if (fs_write(fs, f, l, data)) {
                return RESULT_NO;
            }
            return RESULT_YES;
        }
        return RESULT_NO;
    } else if (0 == strcmp("i", command)) {
        char f[4096];
        int pos;
        int l;
        char data[4096];
        
        sscanf(line + 1, "%s %d %d %[^\n]", f, &pos, &l, data);
        if (fs_isfile(fs, f)) {
            if (fs_insert(fs, f, pos, l, data)) {
                return RESULT_NO;
            }
            return RESULT_YES;
        }
        return RESULT_NO;
    } else if (0 == strcmp("d", command)) {
        char f[4096];
        int pos;
        int l;
        
        sscanf(line + 1, "%s %d %d", f, &pos, &l);
        if (fs_isfile(fs, f)) {
            if (fs_delete(fs, f, pos, l)) {
                return RESULT_NO;
            }
            return RESULT_YES;
        }
        return RESULT_NO;
    } else if (0 == strcmp("e", command)) {
        return RESULT_EXIT;
    }
    return RESULT_ELSE;
}

int main(int argc, char **argv) {
    FileSystem *fs;
    FILE* logfile;
    char str[4096];
    //char buffer[4096];
    
    int sd, client, diskserv;
    struct sockaddr_in server_addr, name;
    struct hostent *host;
    
    // connect to disk server
    if ((diskserv = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Socket error\n");
		exit(1);
	}
	
	printf("Trying to connect...\n");
	name.sin_family 	= AF_INET;
	host				= gethostbyname("localhost"); 
	name.sin_port		= htons(atoi(argv[1]));
	memcpy(&name.sin_addr.s_addr, host->h_addr, host->h_length);
	
	if (connect(diskserv, (struct sockaddr *)&name, sizeof(name)) == -1) {
		fprintf(stderr, "Connect error\n");
		exit(1);
	}	
	printf("Connection with the disk is established!\n");	

	// connect to client
    sd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family		 = AF_INET;
    server_addr.sin_addr.s_addr  = htonl(INADDR_ANY);
    server_addr.sin_port		 = htons(atoi(argv[2]));
    
    if (bind(sd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
    	fprintf(stderr, "Bind error\n");
    	exit(1);
    }
    if (listen(sd, 1) == -1) {
    	fprintf(stderr, "Listen error\n");
    	exit(1);
    }
    fs = fs_new();
    if ((client = accept(sd, 0, 0)) == -1) {
    	fprintf(stderr, "Accept error\n");
    	exit(1);
    }
    logfile = fdopen(client, "w+");
    printf("Connection with client is established!\n");
       
    
    while (1) {
        int result;
        
        if (recv(client, str, 4096, 0) < 0) {
        	printf("Client closed connection\n");
        	break; }
        while (isspace(str[strlen(str) - 1])) {
            str[strlen(str) - 1] = 0;
        }
        printf("receive successfully\n");
        result = process_request(str, logfile, fs);
        if (RESULT_EXIT == result) {
            fprintf(logfile, "Goodbye!\n");
            fflush(logfile);
            break;
        } else if (RESULT_DONE == result) {
            fprintf(logfile, "Done\n");
            fflush(logfile);
        } else if (RESULT_YES == result) {
            fprintf(logfile, "Yes\n");
            fflush(logfile);
        } else if (RESULT_NO == result) {
            fprintf(logfile, "No\n");
            fflush(logfile);
        }
        else {
        	fprintf(logfile, "Wrong Input\n");
        	break;
        }
        printf("send succussfully.\n");
    }
    close(client);
    close(diskserv);
    close(sd);
    printf("GoodBye!\n");
    fs_free(&fs);
    fclose(logfile);
    return 0;
}

