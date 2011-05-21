/*
 * Strictly speaking, the rejection below is a bug, or at least
 * an infelicity. See TODO.
 */
program varInitTest

int x = 1;

ss test {

    int y = x;  /* option -r: non-constant initializer */

    state test {
        when () {} exit
    }
}
