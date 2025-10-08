struct stat;
struct rtcdate;

// system calls
int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
void next_palindrome(int num);
int set_sleep_syscall(int tick);
int get_system_time(struct rtcdate*);

int make_user(int user_id, const char* password);
int login(int user_id, const char* password);
int logout();
int logs();
int diff(const char* file1, const char* file2);


int create_realtime_process(int);
int change_process_queue(int pid, int queue);
int print_process_info(void);
void barber_sleep(void);
int customer_arrive(void);
void cut_hair(void);
void init_rw_lock(void);
void get_rw_pattern(int pattern);
void critical_section(void);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void printf(int, const char*, ...);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);

int list_programs(void);