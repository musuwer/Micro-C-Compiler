int main() {
    int total = 0;

    for (int i = 0; i < 4; i = i + 1) {
        if (i == 2) {
            total = total + 10;
        } else {
            total = total + i;
        }
    }

    int j = 0;
    while (j < 3) {
        {
            int add = j + 1;
            total = total + add;
        }
        j = j + 1;
    }

    if (total > 15 && j == 3) {
        return total;
    } else {
        return 0;
    }
}
