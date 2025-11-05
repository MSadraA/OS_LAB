#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 2){
    printf(2, "Usage: %s <source_file>\n", argv[0]);
    exit();
  }

  char *source_file = argv[1];
  int result = make_duplicate(source_file);

  printf(1, "Result: %d\n", result);
  exit();
}