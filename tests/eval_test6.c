int main() {
    int sum = 0;
    int i = 0;
    while(i < 10) {
        sum = sum + i;
        if (sum > 10)
            break;
        i++;
    }
    return sum;
}

int fn() {
    int sum = 0;
    int i = 0;
    while (i < 10) {
        i = i + 1;
        if (i % 2)
            continue;
        sum = sum + i;
    }
    return sum;
}

int another() {
    int sum = 0;
    int i = 0;
    while (i < 10) {
        if ((sum / 2) * 2 != sum)
            continue
        sum = sum + i;
        i = i + 1;
    }
    return sum;
}
