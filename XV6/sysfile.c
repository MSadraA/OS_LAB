//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

#define MAX_PATH_LENGTH 128


// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

int
sys_open(void)
{
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int major, minor;

  begin_op();
  if((argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}

int
sys_make_duplicate(void){
  char *src_path;
  char dest_path[MAX_PATH_LENGTH];
  struct inode *ip_src, *ip_dest;
  char buf[BSIZE];

  if(argstr(0, &src_path) < 0){
    cprintf("Failed to get source path\n");
    return 1;
  }

  int i;
  // copy src_path to dest_path
  for(i = 0; src_path[i] && i < MAX_PATH_LENGTH; i++){
    dest_path[i] = src_path[i];
  }
  if(i + 6 > MAX_PATH_LENGTH){
    cprintf("Path too long\n");
    return 1;
  }
  dest_path[i++] = '.';
  dest_path[i++] = 'c';
  dest_path[i++] = 'o';
  dest_path[i++] = 'p';
  dest_path[i++] = 'y';
  dest_path[i] = '\0';

  begin_op();
  if((ip_src = namei(src_path)) == 0){
    cprintf("Failed to open source file\n");
    end_op();
    return -1;
  }
  ilock(ip_src);
  if((ip_dest = create(dest_path, T_FILE, 0, 0)) == 0){
    iunlockput(ip_src);
    cprintf("Failed to create destination file\n");
    end_op();
    return 1;
  }
  end_op();
  
  // copy loop from readi
  int off = 0;
  int n;
  
  while((n = readi(ip_src, buf, off, BSIZE)) > 0){
    begin_op();
    if(writei(ip_dest, buf, off, n) != n){
      iunlockput(ip_src);
      iunlockput(ip_dest);
      cprintf("Failed to write to destination file\n");
      end_op();
      return 1;
    }
    end_op();
    off += n;
  }

  // cleanup
  iunlockput(ip_src);
  iunlockput(ip_dest);
  return 0;
}

// helper function for string length
static int
kstrlen(char *str)
{
  int length;
  // iterate until we hit the null terminator
  for(length = 0; str[length]; length++)
    ;
  return length;
}

// helper function for finding a substring
// searches for 'to_find' inside 'main_str'
static char*
kstrstr(char *main_str, char *to_find)
{
  int find_len = kstrlen(to_find);
  // an empty string is always found
  if(find_len == 0)
    return main_str;

  char *main_ptr = main_str;
  
  // iterate over the main string
  while(*main_ptr){
    // check for first character match
    if(*main_ptr == *to_find){
      char *main_runner = main_ptr;
      char *find_runner = to_find;
      
      // check if the rest of the string matches
      while(*find_runner && *main_runner == *find_runner){
        main_runner++;
        find_runner++;
      }
      
      // if we reached the end of 'to_find', it's a full match
      if(*find_runner == '\0')
        return main_ptr; // return the start of the match
    }
    main_ptr++;
  }
  
  return 0; // no match
}

int
sys_grep_syscall(void){
  char *keyword_ptr;    // user pointer
  char *filename_ptr;   // user pointer
  char *user_buffer;    // user buffer pointer
  int user_buffer_size;
  struct inode *ip;
  
  char *file_buf = 0;   // kernel buffer for file
  char *key_word = 0;   // kernel buffer for keyword
  
  int result = -1; // default return value (error)
  char *match = 0;
  int line_len = 0;
  char *line_start, *line_end;

  // get all arguments from user
  if(argstr(0, &keyword_ptr) < 0 || argstr(1, &filename_ptr) < 0  || argint(3, &user_buffer_size) < 0) {
    cprintf("Failed to get arguments\n");
    return 1;
  }
  if(argptr(2, (void*) &user_buffer, user_buffer_size) < 0){
    cprintf("Failed to get user buffer\n");
    return 1;
  }

  // allocate kernel buffers
  file_buf = kalloc();
  key_word = kalloc();
  if(file_buf == 0 || key_word == 0){
    cprintf("kalloc failed\n");
    goto cleanup;
  }

  // safely copy keyword from user space to kernel buffer
  if(safestrcpy(key_word, keyword_ptr, PGSIZE) < 0){
     cprintf("Failed to copy keyword to kernel space\n");
     goto cleanup;
  }

  // open source file
  begin_op();
  if((ip = namei(filename_ptr)) == 0){
    cprintf("Failed to open source file\n");
    end_op();
    goto cleanup;
  }
  ilock(ip);

  // check if file is too large for our buffer
  if(ip->size > PGSIZE){
    cprintf("File too large for grep buffer\n");
    iunlockput(ip);
    end_op();
    goto cleanup;
  }
  
  // read entire file into kernel buffer
  if(readi(ip, file_buf, 0, ip->size) != ip->size){
    cprintf("Failed to read file\n");
    iunlockput(ip);
    end_op();
    goto cleanup;
  }
  file_buf[ip->size] = '\0'; // ensure null terminated
  
  iunlockput(ip);
  end_op();

  // find keyword in file buffer
  match = kstrstr(file_buf, key_word);

  if(match == 0){
    // not found
    goto cleanup;
  }

  // found it, now find the start of the line
  line_start = match;
  while(line_start > file_buf && *(line_start - 1) != '\n'){
    line_start--;
  }

  // find the end of the line
  line_end = match;
  while(*line_end != '\0' && *line_end != '\n'){
    line_end++;
  }
  
  line_len = line_end - line_start;

  // check if it fits in the user buffer
  if(line_len > user_buffer_size - 1){ // -1 for the null char
    cprintf("User buffer too small\n");
    goto cleanup;
  }

  // copy line to user space
  if(copyout(myproc()->pgdir, (uint)user_buffer, line_start, line_len) < 0){
    cprintf("copyout to user failed\n");
    goto cleanup;
  }
  
  // copy null terminator to user space
  char null_char = '\0';
  if(copyout(myproc()->pgdir, (uint)(user_buffer + line_len), &null_char, 1) < 0){
    cprintf("copyout null-terminator failed\n");
    goto cleanup;
  }
  
  // success! return the line length
  result = line_len;

cleanup:
  // free allocated kernel buffers
  if(key_word)
    kfree(key_word);
  if(file_buf)
    kfree(file_buf);

  return result;
}