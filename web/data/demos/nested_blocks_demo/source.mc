int main() {
    int value = 3;
    {
        int value = 7;
        {
            int extra = 12;
            value = value + extra;
        }
        return value;
    }
}
