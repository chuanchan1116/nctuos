/* This file use for NCTU OSDI course */
/* It's contants fat file system operators */

#include <inc/stdio.h>
#include <fs.h>
#include <fat/ff.h>
#include <diskio.h>

extern struct fs_dev fat_fs;

/*  Lab7, fat level file operator.
 *       Implement below functions to support basic file system operators by using the elmfat's API(f_xxx).
 *       Reference: http://elm-chan.org/fsw/ff/00index_e.html (or under doc directory (doc/00index_e.html))
 *
 *  Call flow example:
 *        ┌──────────────┐
 *        │     open     │
 *        └──────────────┘
 *               ↓
 *        ┌──────────────┐
 *        │   sys_open   │  file I/O system call interface
 *        └──────────────┘
 *               ↓
 *        ┌──────────────┐
 *        │  file_open   │  VFS level file API
 *        └──────────────┘
 *               ↓
 *        ╔══════════════╗
 *   ==>  ║   fat_open   ║  fat level file operator
 *        ╚══════════════╝
 *               ↓
 *        ┌──────────────┐
 *        │    f_open    │  FAT File System Module
 *        └──────────────┘
 *               ↓
 *        ┌──────────────┐
 *        │    diskio    │  low level file operator
 *        └──────────────┘
 *               ↓
 *        ┌──────────────┐
 *        │     disk     │  simple ATA disk dirver
 *        └──────────────┘
 */

/* Note: 1. Get FATFS object from fs->data
*        2. Check fs->path parameter then call f_mount.
*/
int fat_mount(struct fs_dev *fs, const void* data)
{
    int ret = f_mount(fs->data, fs->path, 1);
    return -ret;
}

/* Note: Just call f_mkfs at root path '/' */
int fat_mkfs(const char* device_name)
{
    int ret = f_mkfs("/", 0, 0);
    return -ret;
}

/* Note: Convert the POSIX's open flag to elmfat's flag.
*        Example: if file->flags == O_RDONLY then open_mode = FA_READ
*                 if file->flags & O_APPEND then f_seek the file to end after f_open
*/
int fat_open(struct fs_fd* file)
{
    int ret = 0;
    BYTE mode = 0;
    if(file->flags == O_RDONLY) mode |= FA_READ;
    if(file->flags & O_WRONLY) mode |= FA_WRITE;
    if(file->flags & O_RDWR) mode |= (FA_READ | FA_WRITE);
    if((file->flags & O_CREAT) && !(file->flags & O_TRUNC)) mode |= FA_CREATE_NEW;
    if(file->flags & O_EXCL) mode |= FA_CREATE_ALWAYS;
    if((file->flags & O_TRUNC) && (mode & FA_WRITE)) {
        f_truncate(file->data);
        file->size = 0;
        mode |= FA_CREATE_ALWAYS;
    }
    ret = f_open(file->data, file->path, mode);
    if(file->flags & O_APPEND) f_lseek(file->data, f_size((FIL *)file->data));
    return -ret;
}

int fat_close(struct fs_fd* file)
{
    return f_close(file->data);
}
int fat_read(struct fs_fd* file, void* buf, size_t count)
{
    int br, ret;
    ret = f_read(file->data, buf, count, &br);
    if(ret != FR_OK) return -ret;
    file->pos += ret;
    return br;
}
int fat_write(struct fs_fd* file, const void* buf, size_t count)
{
    int ret, bw;
    ret = f_write(file->data, buf, count, &bw);
    if(ret != FR_OK) return -ret;
    file->pos += ret;
    if(file->pos > file->size) file->size = file->pos;
    return bw;
}
int fat_lseek(struct fs_fd* file, off_t offset)
{
    int ret = f_lseek(file->data, offset);
    return -ret;
}
int fat_unlink(struct fs_fd* file, const char *pathname)
{
    int ret = f_unlink(pathname);
    return -ret;
}

int fat_opendir(DIR *dp, const char *path) {
    int ret = f_opendir(dp, path);
    return -ret;
}

int fat_closedir(DIR *dp) {
    int ret = f_closedir(dp);
    return -ret;
}

int fat_readdir(DIR *dp, FILINFO * fno) {
    int ret = f_readdir(dp, fno);
    return -ret;
}

struct fs_ops elmfat_ops = {
    .dev_name = "elmfat",
    .mount = fat_mount,
    .mkfs = fat_mkfs,
    .open = fat_open,
    .close = fat_close,
    .read = fat_read,
    .write = fat_write,
    .lseek = fat_lseek,
    .unlink = fat_unlink,
    .opendir = fat_opendir,
    .closedir = fat_closedir,
    .readdir = fat_readdir
};



