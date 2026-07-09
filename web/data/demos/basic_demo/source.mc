int main() {
    int sum = 0;
    int i = 0;

    while (i < 5) {
        sum = sum + i;
        i = i + 1;
    }

    if (sum == 10) {
        return sum + 3;
    } else {
        return 0;
    }
}
