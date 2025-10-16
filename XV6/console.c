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
// #define INPUT_BUF 512 // I want'em BIIIG.. I want'em CHUUUNKY... I want'em BIIIG

// Tab handling
int is_tab_context = 0;
int tab_check = 0;
int special_tab_char = '\x01';

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

#define INTERNAL_FLAG_CHAR  0x1F 

// Stack
typedef struct {
  int stack[INPUT_BUF];
  int size;
  void (*clear)(struct Stack *self);
  void (*push)(struct Stack *self, int val);
  int  (*pop)(struct Stack *self);
} Stack;
void Stack_clear(Stack *self) {
  self->size = 0;
  memset(self->stack, 0, sizeof(self->stack));
}
void Stack_push(Stack *self, int val) {
  if (self->size < INPUT_BUF)
    self->stack[self->size++] = val;
}
int Stack_pop(Stack *self) {
  if (self->size == 0) return -1;
  return self->stack[--self->size];
}
static Stack lastInputIndex = { .size = 0, .clear = Stack_clear, .push = Stack_push, .pop = Stack_pop };
static Stack lastInputValue = { .size = 0, .clear = Stack_clear, .push = Stack_push, .pop = Stack_pop };

// Null declaration
#define NULL ((char*)0)

// Mathematical functions
int min(int a, int b);
int max(int a, int b);

// Additional functions
void executeAtCursorPos(int target_pos, void (*action)(void));
void cleanConsole();
void strSplit(char *dst, char *src, int start, int end, int size);
void cprintf_color(char *str, uchar color);
void consputc_color(int c, uchar color);
void moveCursorToPos(int pos);
void clearBuffer();
void debugPrintBuffer(void);

void moveRight();
void moveLeft();

// Clipboard Functions
void resetClipboard();
void PasteClipboard();
void copySToClipboard();
void highlightSelectedWords();
void gayConsole();
void straitConsole();
uchar getRainbowColor(int offset);
void undoLastInput();
void highlight_range(void);
void rainbow_range(void);
void normalize_range(void);



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
//PAGEBREAK: 50strcmp

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
  int count = 0;
  int idx = input.e;

  while (input.buf[idx % INPUT_BUF] != '\0')
  {
    consputc(input.buf[idx % INPUT_BUF]);
    count++;
    idx++;
  }

  for (int k = 0; k < count; k++)
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
      input.buf[i % INPUT_BUF] = input.buf[(i + 1) % INPUT_BUF];
    }
    
    moveLeft();

    input.end--;
    input.buf[input.end % INPUT_BUF] = '\0';
    redraw_from_edit_point();
  }
}

void insert_char(char c, int flag) {
  if (c == '\n') return;

  if (input.end - input.r >= INPUT_BUF) {
    input.r++;  
  }

  if (flag) {
    lastInputIndex.push(&lastInputIndex, input.e);
    lastInputValue.push(&lastInputValue, INTERNAL_FLAG_CHAR);
  }

  // shift block: [input.e .. input.end-1] one step to the right (circular-safe)
  int pos = input.end;
  int distance = (input.end - input.e + INPUT_BUF) % INPUT_BUF;

  for (int k = 0; k < distance; k++) {
    int from = (pos - k - 1 + INPUT_BUF) % INPUT_BUF;
    int to = (from + 1) % INPUT_BUF;
    input.buf[to] = input.buf[from];
  }
  
  consputc(c);
  input.buf[input.e % INPUT_BUF] = c;
  input.e++;
  input.end++;

  input.buf[input.end % INPUT_BUF] = '\0';

  // debugPrintBuffer();
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
      lastInputIndex.clear(&lastInputIndex);
      lastInputValue.clear(&lastInputValue);
      resetClipboard();
      cleanConsole();
      tab_check = 0;
      break;
    case KBD_BACKSAPCE: case '\x7f':  // Backspace
      // being_copied = 0;
      // lastInput.clear(&lastInput);

      lastInputIndex.push(&lastInputIndex, input.e);
      lastInputValue.push(&lastInputValue, input.buf[(input.e - 1 + INPUT_BUF) % INPUT_BUF]);

      delete_char();
      tab_check = 0;
      break;

    case '\t': // Tab
      lastInputIndex.clear(&lastInputIndex);
      lastInputValue.clear(&lastInputValue);
      resetClipboard();
      input.buf[input.end++ % INPUT_BUF] = tab_check ? 'T':'F';
      input.buf[input.end++ % INPUT_BUF] = '\t';
      input.buf[input.end++ % INPUT_BUF] = '\n';

      moveCursorToPos(input.w);
      input.w = input.end;
      input.e = input.end;
      
      is_tab_context = 1;
      wakeup(&input.r);
      tab_check = 1;

      // debug_input_buffer();
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
      tab_check = 0;
      PasteClipboard();
      break;

    case C('Z'):
      // CTRL+Z
      tab_check = 0;
      undoLastInput();
      break;

    case KBD_KEY_LEFT:
      // Left Arrow
      if (input.e != input.w)
      {
        moveLeft();
      }    
      break;

    case KBD_KEY_RIGHT:
      {
        // Right Arrow
        // int line_end = input.w + strlen(input.buf + input.w);
        // if (input.e < input.w + INPUT_BUF && input.e < line_end)
        // if (input.buf[input.e % INPUT_BUF] != '\0' && input.e < line_end)
        if (input.e < input.end && input.buf[(input.e) % INPUT_BUF] != '\0')
        {
          moveRight();
        }
        break;
      }
      
    case C('D'): // Ctrl + D
      {
        // int line_end = input.w + strlen(input.buf + input.w);
        // Move to the end of the current word
        while (input.e < input.end && input.buf[input.e % INPUT_BUF] != ' ' && input.buf[input.e % INPUT_BUF] != '\0')
        {
          moveRight();
        }
        // Move to the beginning of the next word
        while (input.e < input.end && input.buf[input.e % INPUT_BUF] == ' ')
        {
          moveRight();
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
            moveLeft();
          }
        }
        // Move to the beginning of the current word
        while (input.e > input.w && input.buf[(input.e - 1 + INPUT_BUF) % INPUT_BUF] != ' ')
        {
          moveLeft();
        }
        break;
      }

    default:
      if (c != 0) {                        
        c = (c == '\r') ? '\n' : c;
    
        if (c == '\n' || !being_copied)
          resetClipboard();
    
        insert_char(c, 1);          
        tab_check = 0;
    
        if (c == '\n') {         
          // debugPrintBuffer();
          lastInputIndex.clear(&lastInputIndex);
          lastInputValue.clear(&lastInputValue);
    
          input.buf[input.end % INPUT_BUF] = '\n';
          input.end++;
    
          consputc('\n');
          input.w = input.end;
          input.e = input.end;
          
          wakeup(&input.r);
        }
      }
      break;
    }
  }


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
  
  if(strlen(buf) == 1 && buf[0] == special_tab_char){
    release(&cons.lock);
    ilock(ip);
    return n;
  }

  if(is_tab_context && strlen(buf) > 0 && buf[0] == special_tab_char){
    for(i = 1; i < n; i++)
      insert_char(buf[i] & 0xff , 1);
    // debug_input_buffer();
    is_tab_context = 0;
  }
  else{
    for(i = 0; i < n; i++)
      consputc(buf[i] & 0xff);
  }
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
}

void strSplit(char *dst, char *src, int start, int end, int size)
{
  int i = 0;
  for (int j = start; j < end; j++)
  {
    dst[i++] = src[j % size];
  }
  dst[i] = '\0';
}

void cleanConsole()
{
  moveCursorToPos(input.end);
  while (input.end != input.w)
    delete_char();
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
      insert_char(clipboard.buf[i], 1);
    }
  }
  // resetClipboard();
}

void copySToClipboard()
{
  if (clipboard.flag == 1)
  {
    strSplit(clipboard.buf, input.buf, clipboard.start_index, clipboard.end_index, INPUT_BUF);
    clipboard.valid = 1;
    being_copied = 0;
    resetClipboard();
  }
}

void highlightSelectedWords() {
  executeAtCursorPos(clipboard.start_index, highlight_range); 
}

void straitConsole() {
  executeAtCursorPos(clipboard.start_index, normalize_range);
}

void gayConsole() {
  executeAtCursorPos(clipboard.start_index, rainbow_range);
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

void undoLastInput() {
  if (lastInputIndex.size <= 0)
    return;

  int original_pos = input.e;

  moveCursorToPos(lastInputIndex.pop(&lastInputIndex) + 1);
  
  char temp = lastInputValue.pop(&lastInputValue);
  if (temp != INTERNAL_FLAG_CHAR) {
    moveCursorToPos(input.e - 2);
    insert_char(temp, 0);
  } else {
    delete_char();
    if (original_pos > input.end)
      original_pos = input.end;
  }
    

  moveCursorToPos(original_pos);
}

void executeAtCursorPos(int target_pos, void (*action)(void)) {
  int original_pos = input.e;

  moveCursorToPos(target_pos);

  if (action)
    action();

  moveCursorToPos(original_pos);
}

void highlight_range(void) {
  for (int i = clipboard.start_index; i < clipboard.end_index; i++) {
    consputc_color(input.buf[i % INPUT_BUF], HIGHLIGHT_COLOR);
    input.e++;
  }
}

void rainbow_range(void) {
  for (int i = clipboard.start_index; i < clipboard.end_index; i++) {
    uchar fg = getRainbowColor(i - clipboard.start_index);
    consputc_color(input.buf[i % INPUT_BUF], RAINBOW_HIGHLIGHT(fg));
    input.e++;
  }
}

void normalize_range(void) {
  for (int i = clipboard.start_index; i < clipboard.end_index; i++) {
    consputc_color(input.buf[i % INPUT_BUF], NORMAL_COLOR);
    input.e++;
  }
}

void clear_console() {
  moveCursorToPos(input.end);
  input.e = input.end;
  if(input.end <= input.w) return;
  while(input.e != input.w){
    consputc(BACKSPACE);
    input.e--;
  }
}

void debug_input_buffer() {
    release(&cons.lock);
    cprintf("---- Debugging Input Buffer ----\n");

    cprintf("r (read index): %d\n", input.r);
    cprintf("e (edit index): %d\n", input.e);
    cprintf("w (write index): %d\n", input.w);
    cprintf("end (end index): %d\n", input.end);

    cprintf("Buffer from w to end: ");
    for (int i = input.w; i < input.end; i++) {
        cprintf("%c", input.buf[i % INPUT_BUF]);
    }

    cprintf("\n---- End of Debug ----\n");
    acquire(&cons.lock);
}

void clearBuffer() {
  memset(input.buf, 0, sizeof(input.buf));
  input.r = input.w = input.e = input.end = 0;
  // cprintf("[DBG] Buffer reset (no screen clear).\n");
}

void moveRight() {
  move_cursor(1);
  input.e++;
}

void moveLeft() {
  move_cursor(-1);
  input.e--;
}

void debugPrintBuffer(void) {
  release(&cons.lock);
  cprintf("\n[DBG] Buffer dump (r=%d, w=%d, e=%d, end=%d):\n",
    input.r, input.w, input.e, input.end);
  acquire(&cons.lock);

  for (int i = 0; i < INPUT_BUF; i++) {
    char c = input.buf[i];
    if (c == '\0')
      consputc('.');
    else if (c == '\n')
      consputc('\\'), consputc('n');
    else if (c == '\r')
      consputc('\\'), consputc('r');
    else
      consputc(c);
  }
  release(&cons.lock);
  cprintf("\n[END OF BUFFER]\n");
  acquire(&cons.lock);
}

