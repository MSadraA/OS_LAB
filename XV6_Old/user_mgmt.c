#include "types.h"        
#include "param.h"        
#include "memlayout.h"    
#include "mmu.h"          
#include "x86.h"          
#include "proc.h"         
#include "defs.h"         
#include "user_mgmt.h"
#include "spinlock.h"

// Errors
char * USER_MAX_ERR = "User maximum number reached\n";
char * LOG_MAX_ERR = "Log maximum number reached\n";
char * UNIQUE_ERR = "The username is duplicate\n";
char * ALREADY_LOGEDIN = "Some one is already logged in\n";
char * USER_NOT_FOUND = "User not found or wrong password!\n";
char * NOT_LOGIN = "Not logged in yet\n";
char * NO_USER = "There are no users\n";

struct user_list users;

// cur_user
struct spinlock login_lock;
struct user *curr_user = 0;


int add_user(int user_id, const char *password) {
  if (users.size >= MAX_USERS)
  {
    cprintf(USER_MAX_ERR);
    return FAILURE;
  }

  if(!is_user_id_unique(user_id)){
    cprintf(UNIQUE_ERR);
    return FAILURE;
  }

  struct user *u = &users.data[users.size++];
  u->used = 1;
  u->user_id = user_id;
  safestrcpy(u->password, password, PASSWORD_LEN);
  u->is_logged_in = 0;
  u->log_count = 0;

  return SUCCESS;
}


int login_user(int user_id, const char *password) {
    acquire(&login_lock);

    
    if (curr_user->is_logged_in == 1) { //some one is already logged in
        release(&login_lock);
        cprintf(ALREADY_LOGEDIN);
        return FAILURE;
    }
    
    for (int i = 0; i < users.size; i++) {
        if (users.data[i].user_id == user_id &&
            strncmp(users.data[i].password, password, PASSWORD_LEN) == 0) {     
                users.data[i].is_logged_in = 1;
                curr_user = &users.data[i];

            release(&login_lock);
            return SUCCESS;
        }
    }
    release(&login_lock);
    cprintf(USER_NOT_FOUND);
    return FAILURE;
}


int logout_user() {
    acquire(&login_lock);

    if (curr_user == 0 || curr_user->is_logged_in == 0) {
        release(&login_lock);
        cprintf(NOT_LOGIN);
        return FAILURE;
    }
    curr_user->is_logged_in = 0;
    curr_user = 0;

    release(&login_lock);
    return SUCCESS;
}

void log_syscall(int syscall_no) {
    acquire(&login_lock);

    if (curr_user && curr_user->is_logged_in == 1)
    {
        if(curr_user->log_count >= MAX_USER_LOGS) // reset logs
        {
            cprintf(LOG_MAX_ERR);
            curr_user->log_count = 0;
            release(&login_lock);
            return;
        }
        // cprintf("Added : (%d)\n" , syscall_no);
        curr_user->syscall_log[curr_user->log_count++] = syscall_no;
    }
    release(&login_lock);
}

void get_user_logs(){
    acquire(&login_lock);

    if(curr_user && curr_user->is_logged_in == 1){
        cprintf("Recent logs for user (%d) : \n" , curr_user->user_id);
        cprintf("=====================================\n");
        for(int i = 0 ; i < curr_user->log_count ; i++){
            cprintf("%d\n" , curr_user->syscall_log[i]);
        }
        cprintf("=====================================\n");
    }
    else { 
        cprintf("Recent logs for all users : \n");
        cprintf("=====================================\n");
        for (int i = 0 ; i < users.size ; i++){
            for(int j = 0 ; j < users.data[i].log_count ; j++) {
                cprintf("%d\n" , users.data[i].syscall_log[j]);
            }
        }
        cprintf("=====================================\n");
    }
    release(&login_lock);
}


//helper functions
int is_user_id_unique(int user_id){
    for(int i = 0; i < users.size; i++) {
        if (users.data[i].user_id == user_id) {
            return 0; 
        }
    }
    return 1;
}

