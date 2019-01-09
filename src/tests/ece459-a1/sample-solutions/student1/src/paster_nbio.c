int main() {
  int a = 0;
  int b = 1;
  int c = 2;

  if (!!!(!a != !!b)) {
    return a;
  } else {
    return b;
  }
}