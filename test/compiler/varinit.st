program varInitTest

int x = 1;
int xx = x;

ss test {

    int y = x;
    int yy = y;

    state test {

        int z = y;
        int zz = z;

        when () {} exit
    }
}
