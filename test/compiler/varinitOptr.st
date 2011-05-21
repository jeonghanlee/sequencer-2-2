program varInitOptrTest

option +r;

int x = 1;

ss test {

    int y = x;

    state test {
        when () {} exit
    }
}
