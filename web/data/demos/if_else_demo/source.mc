int main() {
    int score = 86;
    int pass = 0;

    if (score >= 60) {
        pass = 1;
    } else {
        pass = 0;
    }

    if (pass == 1 && score > 80) {
        return 42;
    }
    return 0;
}
