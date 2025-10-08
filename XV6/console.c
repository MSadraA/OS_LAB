// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void consputc(int);

static int panicked = 0;

static struct {
  struct spinlock lock;
  int locking;
} cons;

// Const Values
#define INPUT_BUF 128

// Exclemation command
#define EXCLAMATION 6
#define KEYWORDS_CNT 7
const char *KEYWORDS[KEYWORDS_CNT] = {"void", "int", "char", "if", "for", "while", "return"};
const char *DELIMITER = " ";
const char EXCLAMATION_CHAR = '!';

// Program listing
#define MAX_FILES 128
#define MAX_NAME DIRSIZ
static int programs_num = 0;
char programs[MAX_FILES][MAX_NAME];

struct autocomplete_state {
  int initialized;
  int tab_state;
  char last_prefix[MAX_NAME];
  char matches[MAX_FILES][MAX_NAME];
  int match_count;
  int match_index;
};

static struct autocomplete_state auto_state;

// Color codes
#define BLACK 0x0
#define BLUE 0x1
#define GREEN 0x2
#define CYAN 0x3
#define RED 0x4
#define MAGENTA 0x5
#define BROWN 0x6
#define LIGHT_GRAY 0x7
#define DARK_GRAY 0x8
#define LIGHT_BLUE 0x9
#define LIGHT_GREEN 0xA
#define LIGHT_CYAN 0xB
#define LIGHT_RED 0xC
#define LIGHT_MAGENTA 0xD
#define YELLOW 0xE
#define WHITE 0xF
#define HIGHLIGHT 0xF0

#define NORMAL_COLOR ((BLACK << 4) | LIGHT_GRAY)
#define HIGHLIGHT_COLOR ((WHITE << 4) | BLACK)

#define RAINBOW_HIGHLIGHT(fg) ((WHITE << 4) | (fg))
enum RainbowColors {
  RED_COLOR = LIGHT_RED,
  ORANGE_COLOR = YELLOW,
  GREEN_COLOR = LIGHT_GREEN,
  CYAN_COLOR = LIGHT_CYAN,
  BLUE_COLOR = LIGHT_BLUE,
  MAGENTA_COLOR = LIGHT_MAGENTA,
  RAINBOW_COUNT = 7
};


// KEY DRIVER CODE
#define KBD_BACKSAPCE 0x08
#define KBD_CTRL_H 0x19
#define KBD_KEY_LEFT 0xE4
#define KBD_KEY_RIGHT 0xE5

// Clipboard buffer
typedef struct {
  char buf[INPUT_BUF];
  int start_index;
  int end_index;
  int flag;
  int valid;
  int flag_s;
} Clipboard;
static Clipboard clipboard = {.start_index = 0, .end_index = 0, .flag = 0, .valid = 0, .flag_s = 0};
int being_copied = 0;

// History buffer
#define HISTORY_SIZE 10
#define HISTORY_LIMIT 5
typedef struct {
  char buf[HISTORY_SIZE][INPUT_BUF];
  int index;
  int size;
} HBuffer;

static HBuffer history = {.index = 0, .size = 0};
static HBuffer cmd_history = {.index = 0 , .size = 0};

// Null declaration
#define NULL ((char*)0)

// Mathematical functions
int min(int a, int b);
int max(int a, int b);

// Additional functions
int find_prefix_matches(char *prefix, int n, char matches[MAX_FILES][MAX_NAME]);
void clean_console(); //todo
void saveLastInHistory();
void saveLastInCmdHistory();
void showHistory();
void strSplit(char *dst, char *src, int start, int end);
void cprintf_color(char *str, uchar color);
char* remove_between_sharps();
void print_colored_keywords(char *input) ;

// Clipboard Functions
void resetClipboard();
void PasteClipboard();
void copySToClipboard();
void highlightSelectedWords();
void gayConsole();
void straitConsole();
uchar getRainbowColor(int offset);

void consputc_color(int c, uchar color);

void moveCursorToPos(int pos);

// <string.h> standar functions
char* strchr(const char* str, int c);
int strcmp(const char* str1, const char* str2);
char* strtok(char* str, const char* delimiters);


static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}
//PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if(locking)
    acquire(&cons.lock);

  if (fmt == 0)
  {
    consputc('N');
    consputc('\n');
    consputc('N');
    panic("null fmt");
  }

  argp = (uint*)(void*)(&fmt + 1);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if((s = (char*)*argp++) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if(locking)
    release(&cons.lock);
}

void
panic(char *s)
{
  int i;
  uint pcs[10];

  cli();
  cons.locking = 0;
  // use lapiccpunum so that we can call panic from mycpu()
  cprintf("lapicid %d: panic: ", lapicid());
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for(i=0; i<10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for(;;)
    ;
}

//PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c)
{
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  if(c == '\n')
    pos += 80 - pos%80;
  else if(c == BACKSPACE){
    if(pos > 0) --pos;
  } else
    crt[pos++] = (c&0xff) | 0x0700;  // black on white

  // Check if pos is out of the screen.
  if(pos < 0 || pos > 25*80)
    panic("pos under/overflow");

  if((pos/80) >= 24){  // Scroll up.
    memmove(crt, crt+80, sizeof(crt[0])*23*80);
    pos -= 80;
    memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
  }

  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
  crt[pos] = ' ' | 0x0700;
}

void move_cursor(int dir)
{
  outb(CRTPORT, 14);
  int pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  pos += dir;
  if (pos < 0) pos = 0;
  if (pos > 80*25 - 1) pos = 80*25 - 1;

  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos >> 8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
}

void
consputc(int c)
{
  if(panicked){
    cli();
    for(;;)
      ;
  }

  if(c == BACKSPACE){
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } else
    uartputc(c);
  cgaputc(c);
}

struct {
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
  uint end; // End index
} input;


void redraw_from_edit_point(void) {
  for (int i = input.e; i < input.end; i++)
    consputc(input.buf[i]);

  for (int i = input.end; i > input.e; i--)
    move_cursor(-1);
}

void delete_char() {
  if (input.e <= input.w) return;

  if(input.e == input.end){
    consputc(BACKSPACE);
    input.e--;
    input.end--;
    input.buf[input.end % INPUT_BUF] = '\0';
  }
  else{
    for (int i = input.e - 1; i < input.end - 1; i++) 
    {
      input.buf[i] = input.buf[i + 1];
    }
    move_cursor(-1);
    input.e--;
    input.end--;
    input.buf[input.end % INPUT_BUF] = '\0';
    redraw_from_edit_point();
  }
}

void insert_char(char c) {
  if(c == '\n') return;
  
  for (int i = input.end ; i > input.e; i--)
    input.buf[i] = input.buf[i - 1];

  consputc(c);

  input.buf[input.e++ % INPUT_BUF] = c;
  input.end++;
  input.buf[input.end % INPUT_BUF] = '\0';

  redraw_from_edit_point();
}

#define C(x)  ((x)-'@')  // Control-x

void
consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;

  acquire(&cons.lock);
  while((c = getc()) >= 0){
    switch(c){
    case C('P'):  // Process listing. CTRL+P
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      resetClipboard();
      break;
    case C('U'):  // Kill line. CTRL+U
      resetClipboard();
      while(input.e != input.w &&
            input.buf[(input.e-1) % INPUT_BUF] != '\n'){
        input.e--;
        input.end --; // have to check
        consputc(BACKSPACE);
      }
      break;
    case KBD_BACKSAPCE: case '\x7f':  // Backspace
      // being_copied = 0;
      delete_char();
      reset_auto_fill_state();
      break;
    case KBD_CTRL_H: // CTRL + H. History
      resetClipboard();
      showHistory();
      break;

    case '\t': // Tab
      resetClipboard();
        release(&cons.lock);
        handle_auto_fill();
        acquire(&cons.lock);
        break;
      
    case C('S'):
      if (clipboard.flag_s == 1)
      {
        clipboard.end_index = input.e;
        being_copied = 0;
        
        int temp = clipboard.start_index;
        clipboard.start_index = min(clipboard.start_index, clipboard.end_index);
        clipboard.end_index = max(temp, clipboard.end_index);
        
        // todo
        // colorise the selected area
        // highlightSelectedWords();
        gayConsole();
        clipboard.flag_s = 0;
        break;
      }
      resetClipboard();

      clipboard.flag_s = 1;
      clipboard.flag = 1;
      clipboard.start_index = input.e;
      being_copied = 1;
      break;

      case C('C'):
      // CTRL+C
      copySToClipboard();
      break;
      
    case C('V'):
      // CTRL+V;
      PasteClipboard();
      break;

    case KBD_KEY_LEFT:
      // Left Arrow
      if (input.e != input.w)
      {
        move_cursor(-1);
        input.e--;
      }    
      break;

    case KBD_KEY_RIGHT:
      {
        // Right Arrow
        int line_end = input.w + strlen(input.buf + input.w);
        if (input.e < input.w + INPUT_BUF && input.e < line_end)
        {
          move_cursor(1);
          input.e++;
        }
        break;
      }
      
    case C('D'): // Ctrl + D
      {
        int line_end = input.w + strlen(input.buf + input.w);
        // Move to the end of the current word
        while (input.e < line_end && input.buf[input.e % INPUT_BUF] != ' ' && input.buf[input.e % INPUT_BUF] != '\0')
        {
          move_cursor(1);
          input.e++;
        }
        // Move to the beginning of the next word
        while (input.e < line_end && input.buf[input.e % INPUT_BUF] == ' ')
        {
          move_cursor(1);
          input.e++;
        }
        break;
      }
      
    case C('A'): // Ctrl + A
      {
        if (input.e <= input.w)
          break;
        // Move to the beginning of the line
        if (input.buf[(input.e - 1 + INPUT_BUF) % INPUT_BUF] == ' ')
        {
          while (input.e > input.w && input.buf[(input.e - 1 + INPUT_BUF) % INPUT_BUF] == ' ')
          {
            move_cursor(-1);
            input.e--;
          }
        }
        // Move to the beginning of the current word
        while (input.e > input.w && input.buf[(input.e - 1 + INPUT_BUF) % INPUT_BUF] != ' ')
        {
          move_cursor(-1);
          input.e--;
        }
        break;
      }

    default:
      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;
        
        if (c == '\n' || !being_copied)
        {
          resetClipboard();
        }

        insert_char(c);

        // if (c == '\n' || input.e == input.r + INPUT_BUF || c == C('D')) {
        if (c == '\n' || input.e == input.r + INPUT_BUF) {
          
          release(&cons.lock);
          cprintf("[DBG] r=%d e=%d end=%d w=%d buf=%s\n", input.r, input.e, input.end, input.w, input.buf);
          acquire(&cons.lock);

          input.buf[input.end++ % INPUT_BUF] = '\n';
          consputc('\n');
          input.w = input.end;
          input.e = input.end;
          wakeup(&input.r);
        }

        if(c == '\n' && input.buf[input.r] == EXCLAMATION_CHAR )
        {
          release(&cons.lock);
          char* cmd_without_sharps = remove_between_sharps();
          cprintf(" ");
          print_colored_keywords(cmd_without_sharps);
          cprintf("\n");
          acquire(&cons.lock);
        }
        
        // If the command is finished, save it in history
        if (c == '\n') 
        {
          if (input.r != input.w - 1) {
            release(&cons.lock);
            saveLastInHistory();
            saveLastInCmdHistory();
            acquire(&cons.lock);
          }
        }
      }
      break;
    }
  }

  // if (being_copied == 0)
  // {
  //   resetClipboard();
  // }

  release(&cons.lock);
  if(doprocdump) {
    procdump();  // now call procdump() wo. cons.lock held
  }
}

int
consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while(n > 0){
    while(input.r == input.w){
      if(myproc()->killed){
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }

      break;
    }
    *dst++ = c;
    --n;
    if(c == '\n')
      break;
  }

  release(&cons.lock);
  ilock(ip);

  return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
  int i;
  iunlock(ip);
  acquire(&cons.lock);
  for(i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);
  ilock(ip);

  return n;
}

void
consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

  ioapicenable(IRQ_KBD, 0);
  // programs_num = list_programs();
}

void saveLastInHistory()
{
  // Save the command in history  
  strSplit(history.buf[history.index], input.buf, input.r, input.w - 1);
  history.index = (history.index + 1) % HISTORY_SIZE;

  if (history.size < HISTORY_SIZE)
  {
    history.size++;
  } 

}

void saveLastInCmdHistory()
{
  if (input.buf[input.r] != EXCLAMATION_CHAR)
  {
    strSplit(cmd_history.buf[cmd_history.index], input.buf, input.r, input.w - 1);
    cmd_history.index = (cmd_history.index + 1) % HISTORY_SIZE;

    if (cmd_history.size < HISTORY_SIZE)
    {
      cmd_history.size++;
    } 
  }
}

void showHistory()
{
  release(&cons.lock);
  cprintf("--History BEGIN--\n\n");
  int start = history.size < HISTORY_LIMIT ? 0 : history.size - HISTORY_LIMIT;
  for (int i = history.size - 1; i >= start; i--)
  {
    cprintf("%s\n", history.buf[i]);
  }
  // // show cmd_history
  // cprintf("command history :\n");
  // for (int i = cmd_history.size - 1; i >= 0; i--)
  // {
  //   cprintf("%s\n", cmd_history.buf[i]);
  // }
  // //////////////////////////////////
  cprintf("\n--History END----\n");
  cprintf("$ ");
  acquire(&cons.lock);
}

void strSplit(char *dst, char *src, int start, int end)
{
  int i = 0;
  for (int j = start; j < end; j++)
  {
    dst[i++] = src[j];
  }
  dst[i] = '\0';
}

void clean_console()
{
  //todo
}


// Clipboard Functions
void resetClipboard()
{
  if (!being_copied)
    straitConsole();

  clipboard.flag_s = 0;
  clipboard.flag = 0;
  clipboard.start_index = 0;
  clipboard.end_index = 0;
  being_copied = 0;
}

void PasteClipboard()
{      
  if (clipboard.valid == 1)
  {
    for (int i = 0; i < strlen(clipboard.buf); i++)
    {
      insert_char(clipboard.buf[i]);
    }
  }
  // resetClipboard();
}

void copySToClipboard()
{
  if (clipboard.flag == 1)
  {
    strSplit(clipboard.buf, input.buf, clipboard.start_index, clipboard.end_index);
    clipboard.valid = 1;
    being_copied = 0;
    resetClipboard();
  }
}

void highlightSelectedWords() {
  int original_pos = input.e;

  // move cursor to start_index loc.
  moveCursorToPos(clipboard.start_index);

  // Highlight start to end index
  for (int i = clipboard.start_index; i < clipboard.end_index; i++) {
    consputc_color(input.buf[i % INPUT_BUF], HIGHLIGHT_COLOR);
    input.e++;
  }

  // return cursor to its original pos
  moveCursorToPos(original_pos);
}

void straitConsole() {
  int original_pos = input.e;

  // move cursor to start_index loc.
  moveCursorToPos(clipboard.start_index);

  // Restore normal color from start to end index
  for (int i = clipboard.start_index; i < clipboard.end_index; i++) {
    consputc_color(input.buf[i % INPUT_BUF], NORMAL_COLOR);
    input.e++;
  }

  // return cursor to its original pos
  moveCursorToPos(original_pos);
}

void gayConsole() {
  int original_pos = input.e;

  // move cursor to start_index loc.
  moveCursorToPos(clipboard.start_index);

  // Highlight start to end index (local rainbow cycle)
  for (int i = clipboard.start_index; i < clipboard.end_index; i++) {
    uchar fg = getRainbowColor(i - clipboard.start_index);
    consputc_color(input.buf[i % INPUT_BUF], RAINBOW_HIGHLIGHT(fg));
    input.e++;
  }

  // return cursor to its original pos
  moveCursorToPos(original_pos);
}

uchar getRainbowColor(int offset) {
  uchar colors[RAINBOW_COUNT] = {
    RED_COLOR, ORANGE_COLOR, GREEN_COLOR, CYAN_COLOR, BLUE_COLOR, MAGENTA_COLOR
  };
  return colors[offset % RAINBOW_COUNT];
}

// Color functions
void consputc_color(int c, uchar color) {
  int pos;

  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  if(c == '\n')
    pos += 80 - pos % 80;
  else
    crt[pos++] = (c & 0xff) | (color << 8);

  if (pos >= 25 * 80) {
    memmove(crt, crt + 80, sizeof(crt[0]) * 24 * 80);
    pos -= 80;
    memset(crt + pos, 0, sizeof(crt[0]) * (24 * 80 - pos));
  }

  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos >> 8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
}


void cprintf_color(char *str, uchar color) {
  while (*str) {
    consputc_color(*str++, color);
  }
}
///////////////////////////////////////////////////////

// <string.h> standar functions  
char* strchr(const char* str, int c) {
  while (*str) {
      if (*str == (char)c) {
          return (char*)str;
      }
      str++;
  }
  return NULL
;
}

int strcmp(const char* str1, const char* str2) {
  while (*str1 && (*str1 == *str2)) {
      str1++;
      str2++;
  }
  return (unsigned char)*str1 - (unsigned char)*str2;
}

char* strtok(char* str, const char* delimiters) {
  static char* last = NULL
;
  if (str == NULL
) str = last;

  if (str == NULL
) return NULL
;

  while (*str && strchr(delimiters, *str)) str++;

  if (*str == '\0') return NULL;

  char* start = str;
  while (*str && !strchr(delimiters, *str)) str++;

  if (*str) {
      *str = '\0';
      last = str + 1;
  } else {
      last = NULL;
  }
  return start;
}
/////////////////////////////////////////////////

  // ! processing
  int is_keyword(const char* word) {
    for (int i = 0; i < KEYWORDS_CNT; i++) {    
        if (strcmp(word, KEYWORDS[i]) == 0) {
            return 1; //found
        }
    }
    return 0;//not found
  }

  char* remove_between_sharps() { 
    static char output[INPUT_BUF];
    int j = 0;
    int is_first_sharp = 1;
  
    for (int i = input.r + 1; i < input.w; i++) { 
      if (input.buf[i] == '#') {
        int next_sharp = i + 1;
        while (next_sharp < input.w && input.buf[next_sharp] != '#') {
          next_sharp++;
        }
        if (input.buf[next_sharp] == '#') { 
          i = next_sharp - 1;
          is_first_sharp = 0;
          continue;
        }
        else if (i == input.w - 1 && is_first_sharp) {  
          output[j++] = input.buf[i];  
          continue;
        }
      }
      else {
        output[j++] = input.buf[i];
      }
    }
    output[j-1] = '\0'; 
    return output;
  }

void print_colored_keywords(char *input) { //input : !if x == 3 return 0
  char *token;  
  token = strtok(input, DELIMITER);

  while (token != NULL) {
      if (is_keyword(token)) {
           cprintf_color(token, BLUE);
      } else {
          cprintf_color(token, WHITE);
      }
      consputc(' ');
      token = strtok(NULL, DELIMITER);
  }
}

int find_prefix_matches(char *prefix, int n, char matches[MAX_FILES][MAX_NAME]) {
  int count = 0;
  int len = strlen(prefix);

  for (int i = 0; i < n; i++) {
    if (strncmp(programs[i], prefix, len) == 0) {
      strncpy(matches[count], programs[i], MAX_NAME);
      matches[count][MAX_NAME - 1] = '\0';
      count++;
    }
  }

  return count;
}


int list_programs_safe(void) // i don't know how it works 
{
  struct inode *dp;
  struct dirent de;
  uint off;
  int count = 0;

  begin_op();

  dp = namei("/");
  if (dp == 0) {
    cprintf("auto: cannot open root directory\n");
    end_op();
    return -1;
  }

  ilock(dp);

  for (off = 0; off + sizeof(de) <= dp->size; off += sizeof(de)) {
    if (readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      break;
    if (de.inum == 0)
      continue;
    if (count >= MAX_FILES)
      break;

    char namebuf[DIRSIZ + 1];
    memmove(namebuf, de.name, DIRSIZ);
    namebuf[DIRSIZ] = 0;

    safestrcpy(programs[count], namebuf, MAX_NAME);
    programs[count][MAX_NAME - 1] = '\0';
    count++;
  }

  iunlockput(dp);

  end_op();

  programs_num = count;

  // some built-in commands
  safestrcpy(programs[programs_num++], "cd", MAX_NAME);
  safestrcpy(programs[programs_num++], "exit", MAX_NAME);
  safestrcpy(programs[programs_num++], "help", MAX_NAME);
  safestrcpy(programs[programs_num++], "history", MAX_NAME);

  return count;
}

void
print_string_array(char arr[][MAX_NAME], int count)
{
  cprintf("\n-- String Array (%d entries) --\n", count);

  if (count == 0) {
    cprintf("(empty)\n");
    return;
  }

  for (int i = 0; i < count; i++) {
    if (arr[i][0] == '\0')
      continue; // skip empty slots
    cprintf("%d: %s\n", i, arr[i]);
  }
  cprintf("-- End of Array --\n");
}


void clear_line_and_write(const char *cmd){
  moveCursorToPos(input.end);
  input.e = input.end;
  while(input.e > input.w)
    delete_char();

  for (int i = 0; cmd[i] && i < INPUT_BUF-1; i++) {
    consputc(cmd[i]);
    input.buf[input.e % INPUT_BUF] = cmd[i];
    input.e ++;
    input.end ++;
  }
}

void handle_auto_fill(){
  if(auto_state.initialized == 0){
    int j = 0;
    for(int i = input.w ; i < input.end ; i++ , j++)
      auto_state.last_prefix[j] = input.buf[i];
    auto_state.last_prefix[j] = '\0';
    auto_state.match_count = find_prefix_matches(auto_state.last_prefix , programs_num , auto_state.matches);
    auto_state.match_index = 0;
    auto_state.initialized = 1;
  }

  if(auto_state.match_count == 0) return;

  if(auto_state.match_count > 1 && auto_state.tab_state == 0) {
    auto_state.tab_state = 1;
    return;
  }

  int n = auto_state.match_count;
  // print_string_array(auto_state.matches , MAX_FILES);
  // cprintf("%d ,%d ,%s\n" , auto_state.match_count , auto_state.match_index , auto_state.matches[auto_state.match_index % n]);
  clear_line_and_write(auto_state.matches[auto_state.match_index % n]);
  auto_state.match_index ++;
}

void reset_auto_fill_state(){
  auto_state.initialized = 0;
  auto_state.tab_state = 0;
  auto_state.match_count = 0;
  auto_state.match_index = 0;
}
// Mathematical functions
int min(int a, int b)
{
  return (a > b)? b:a;
}
int max(int a, int b)
{
  return (a > b)? a:b;
}

void moveCursorToPos(int pos) {
  while (input.e != pos) {
    if (input.e > pos) {
      move_cursor(-1);
      input.e--;
    } else {      
      move_cursor(1);
      input.e++;
    }
  }
}
