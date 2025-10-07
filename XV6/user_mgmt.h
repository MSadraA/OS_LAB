#ifndef USER_MGMT_H
#define USER_MGMT_H


#define MAX_USERS 64
#define PASSWORD_LEN 16
#define MAX_USER_LOGS 64

#define SUCCESS 0
#define FAILURE -1

struct user {
  int used;
  int user_id;
  char password[PASSWORD_LEN];
  int is_logged_in;

  int syscall_log[MAX_USER_LOGS];
  int log_count;
};

struct user_list {
  struct user data[MAX_USERS];
  int size;
};

int add_user(int user_id, const char *password);
int login_user(int user_id, const char *password);
int logout_user();
void log_syscall(int syscall_num);
void get_user_logs();

// helper functions
int is_user_id_unique(int user_id);


#endif
