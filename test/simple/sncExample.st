program snctest
option +r;
float v;
assign v to "{user}:xxxExample";
monitor v;

evflag tick;

ss ss1
{
	state low
	{
	    when(v>5.0)
	    {
		printf("now changing to high\n");
	    } state high
	    when(delay(.1)) 
	    {
	    } state low
	}
	state high
	{
	    when(v<=5.0)
	    {
		printf("changing to low\n");
	    } state low
	    when(delay(.1))
	    {
	    } state high
        when (efTestAndClear(tick)) {
            printf("tick!");
        } state high
	}
}

ss ss2 {
    state it {
        when (delay(10)) {
            efSet(tick);
        } state it
    }
}
