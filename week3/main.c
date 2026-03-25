#include <stdio.h>
#include <stdlib.h>

long long plus(long long, long long);
long long minus(long long, long long);

int main(int argc, char* argv[]) {  
  if (argc != 3) {
    printf("Usage: %s <int> <int>\n", argv[0]);
    return EXIT_FAILURE;
  }
  
  long long a = strtoll(argv[1], NULL, 10);
  long long b = strtoll(argv[2], NULL, 10);
  long long r = plus(a, b);
  long long s = minus(a, b);
  printf("%lld\n%lld\n", r, s);
  return EXIT_SUCCESS;
}

