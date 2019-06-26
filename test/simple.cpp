
void runBench(long start, long limit) {
  // 27 is a good starting number
  long x = start;
  long reachedOne = 0;

  while (reachedOne < limit) {
    if (x == 1) {
      x = start;
      reachedOne++;
    }

    if (x % 2 == 0)
      x = x / 2;
    else
      x = 3 * x + 1;
  }
}

int main() //(int argc, const char **argv)
{
    runBench(27, 5'000'000);

    return 0;
}
