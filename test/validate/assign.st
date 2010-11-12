program assign_test

int x = 0;
assign x;

ss w {
    state w {
        when (delay(1)) {
            x = !x;
            pvPut(x);
        } state w
    }
}

ss r {
    state yes {
        when (!x) {
        } state no
    }
    state no {
        when (x) {
        } state yes
    }
}
