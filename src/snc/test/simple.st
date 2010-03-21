program simple

int x[2];
assign x to {"x", "y"};

evflag f;

sync x to f;

ss main {
	state main {
		when (efTestAndClear(f)) {
        	x[0]--;
    	} state main
        when (x[0] > 0) {
        	x[0]++;
        } state main
    }
}
