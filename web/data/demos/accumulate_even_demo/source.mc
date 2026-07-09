int main() {
    int i = 0;
    int total = 0;
    for (i = 0; i <= 10; i = i + 1) {
        if (i % 2 == 0) {
            total = total + i;
        }
    }
    return total;
}
