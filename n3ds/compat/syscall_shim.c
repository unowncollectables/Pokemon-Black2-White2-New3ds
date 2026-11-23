/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Ubuntu's newlib calls the non-reentrant names. devkitPro's libsysbase
 * provides the reentrant ones. A real devkitARM sysroot does not need this
 * file; the Makefile compiles it only with PBW2_LOCAL_SYSBASE=1. */
#include <reent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern _ssize_t _write_r(struct _reent *r, int fd, const void *ptr, size_t len);
extern _ssize_t _read_r(struct _reent *r, int fd, void *ptr, size_t len);
extern _off_t _lseek_r(struct _reent *r, int fd, _off_t off, int whence);
extern int _fstat_r(struct _reent *r, int fd, struct stat *st);
extern int _isatty_r(struct _reent *r, int fd);
extern int _close_r(struct _reent *r, int fd);
extern void *_sbrk_r(struct _reent *r, ptrdiff_t incr);
extern int _open_r(struct _reent *r, const char *file, int flags, int mode);
extern int _kill_r(struct _reent *r, int pid, int sig);
extern int _getpid_r(struct _reent *r);

int _write(int fd, const void *ptr, size_t len) {
    return _write_r(_REENT, fd, ptr, len);
}
int _read(int fd, void *ptr, size_t len) {
    return _read_r(_REENT, fd, ptr, len);
}
int _close(int fd) {
    return _close_r(_REENT, fd);
}
int _fstat(int fd, struct stat *st) {
    return _fstat_r(_REENT, fd, st);
}
int _isatty(int fd) {
    return _isatty_r(_REENT, fd);
}
off_t _lseek(int fd, off_t off, int whence) {
    return _lseek_r(_REENT, fd, off, whence);
}
void *_sbrk(ptrdiff_t incr) {
    return _sbrk_r(_REENT, incr);
}
int _open(const char *file, int flags, int mode) {
    return _open_r(_REENT, file, flags, mode);
}
int _kill(int pid, int sig) {
    return _kill_r(_REENT, pid, sig);
}
int _getpid(void) {
    return _getpid_r(_REENT);
}
