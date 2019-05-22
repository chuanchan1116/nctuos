/* This file use for NCTU OSDI course */


// It's handel the file system APIs 
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <fs.h>
#include <fat/ff.h>

/*  Lab7, file I/O system call interface.*/
/*  Note: Here you need handle the file system call from user.
 *       1. When user open a new file, you can use the fd_new() to alloc a file object(struct fs_fd)
 *       2. When user R/W or seek the file, use the fd_get() to get file object.
 *       3. After get file object call file_* functions into VFS level
 *       4. Update the file objet's position or size when user R/W or seek the file.(You can find the useful marco in ff.h)
 *       5. Remember to use fd_put() to put file object back after user R/W, seek or close the file.
 *       6. Handle the error code, for example, if user call open() but no fd slot can be use, sys_open should return -STATUS_ENOSPC.
 *
 *  Call flow example:
 *        ┌──────────────┐
 *        │     open     │
 *        └──────────────┘
 *               ↓
 *        ╔══════════════╗
 *   ==>  ║   sys_open   ║  file I/O system call interface
 *        ╚══════════════╝
 *               ↓
 *        ┌──────────────┐
 *        │  file_open   │  VFS level file API
 *        └──────────────┘
 *               ↓
 *        ┌──────────────┐
 *        │   fat_open   │  fat level file operator
 *        └──────────────┘
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

// Below is POSIX like I/O system call 
int sys_open(const char *file, int flags, int mode)
{
    //We dont care the mode.
    int fd_id = fd_new(), ret;
    if(fd_id < 0) return -STATUS_ENOSPC;
    struct fs_fd *fd = fd_get(fd_id);
    ret = file_open(fd, file, flags);
    fd_put(fd);
    if(ret != FR_OK) {
        switch (ret)
        {
        case -FR_NO_FILE:
            return -STATUS_ENOENT;
            
        case -FR_EXIST:
            return -STATUS_EEXIST;

        case -FR_WRITE_PROTECTED:
            return -STATUS_EROFS;
        
        default:
            return -STATUS_EIO;
        }
    }
    fd->size = f_size((FIL *)fd->data);
    return fd_id;
}

int sys_close(int fd)
{
    struct fs_fd *f = fd_get(fd);
    if(f == NULL) return -STATUS_EINVAL;
    fd_put(f);
    int ret = file_close(f);
    if(ret == FR_OK) {
        fd_put(f);
        return STATUS_OK;
    }
    return -STATUS_EBADF;
}
int sys_read(int fd, void *buf, size_t len)
{
    struct fs_fd *f = fd_get(fd);
    if(f == NULL) return -STATUS_EBADF;
    if(len < 0 || buf == NULL) return -STATUS_EINVAL;
    fd_put(f);
    int ret = file_read(f, buf, len);
    if(ret >= 0) return ret;
    return -STATUS_EIO;
}
int sys_write(int fd, const void *buf, size_t len)
{
    struct fs_fd *f = fd_get(fd);
    if(f == NULL) return -STATUS_EBADF;
    if(len < 0 || buf == NULL) return -STATUS_EINVAL;
    fd_put(f);
    int ret = file_write(f, buf, len);
    if(ret >= 0) return ret;
    return -STATUS_EIO;
}

/* Note: Check the whence parameter and calcuate the new offset value before do file_seek() */
off_t sys_lseek(int fd, off_t offset, int whence)
{
    if(offset < 0) return -STATUS_EINVAL;
    struct fs_fd *f = fd_get(fd);
    if(f == NULL) return -STATUS_EBADF;
    fd_put(f);
    switch (whence)
    {
    case SEEK_SET:
        break;
    
    case SEEK_CUR:
        offset += f->pos;
        break;

    case SEEK_END:
        offset += f->size;
    }
    f->pos += offset;
    int ret = file_lseek(f, offset);

    if(ret == FR_OK) return offset;
    return -STATUS_EIO;
}

int sys_unlink(const char *pathname)
{
    int ret = file_unlink(pathname);
    if(ret == FR_OK) return STATUS_OK;
    return -STATUS_ENOENT;
}

int sys_readdir(DIR *dirp, FILINFO *fno) {
    int ret = file_readdir(dirp, fno);
    if(ret == FR_OK) return STATUS_OK;
    return -STATUS_EIO;
}
              
int sys_opendir(DIR *dirp, const char *path) {
    int ret = file_opendir(dirp, path);
    if(ret == FR_OK) return STATUS_OK;
    if(ret == FR_NO_PATH) return -STATUS_ENOENT;
    if(ret == FR_INVALID_NAME) return -STATUS_EINVAL;
    return -STATUS_EIO;
}

int sys_closedir(DIR *dirp) {
    int ret = file_closedir(dirp);
    if(ret == FR_OK) return STATUS_OK;
    if(ret == FR_INVALID_OBJECT) return -STATUS_EINVAL;
    return -STATUS_EIO;
}
