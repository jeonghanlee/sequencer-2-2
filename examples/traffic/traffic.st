program traffic

#define R 0
#define Y 1
#define G 2

int lights[3];
assign lights to {
    "traffic:red_light",
    "traffic:yellow_light",
    "traffic:green_light"
};

double red_time = 3.0;
double red_yellow_time = 1.0;
double green_time = 4.0;
double yellow_time = 1.0;

ss light {
    state red {
        entry {
            lights[R]=1; pvPut(lights[R]);
            lights[Y]=0; pvPut(lights[Y]);
            lights[G]=0; pvPut(lights[G]);
        }
        when (delay(red_time)) {
        } state red_yellow
    }
    state red_yellow {
        entry {
            lights[R]=1; pvPut(lights[R]);
            lights[Y]=1; pvPut(lights[Y]);
            lights[G]=0; pvPut(lights[G]);
        }
        when (delay(red_yellow_time)) {
        } state green
    }
    state green {
        entry {
            lights[R]=0; pvPut(lights[R]);
            lights[Y]=0; pvPut(lights[Y]);
            lights[G]=1; pvPut(lights[G]);
        }
        when (delay(green_time)) {
        } state yellow
    }
    state yellow {
        entry {
            lights[R]=0; pvPut(lights[R]);
            lights[Y]=1; pvPut(lights[Y]);
            lights[G]=0; pvPut(lights[G]);
        }
        when (delay(yellow_time)) {
        } state red
    }
}
