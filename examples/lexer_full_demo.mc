int main() {
    // Day2 lexer coverage: single-line comment.
    /* Day2 lexer coverage:
       block comment with newline handling. */
    int a;
    float b;

    a = 10;
    b = 3.14;

    if (a >= 10 && b != 0.0 || !0) {
        return a + 3;
    } else {
        return 0;
    }
}
