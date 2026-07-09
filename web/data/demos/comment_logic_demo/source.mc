int main() {
    // Single-line comments should be skipped by the lexer.
    int a = 10;
    int b = 12;

    /* Block comments should keep line and column tracking correct. */
    if (a < b && !(a == 0) || b == 12) {
        return a + b;
    }

    return 0;
}
