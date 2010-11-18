program array

%%#include <stdlib.h>

ss array {
    state init {
        string a4a5s[4][5] = {
            { "00", "01", "02", "03", "04" },
            { "10", "11", "12", "13", "14" },
            { "20", "21", "22", "23", "24" },
            { "30", "31", "32", "33", "34" }
        };
        string (*a4ps[4])[5] = {
            &a4a5s[0],
            &a4a5s[1],
            &a4a5s[2],
            &a4a5s[3]
        };
        assign a4a5s to {};
        entry {
            int i, j;
            for (i = 0; i < 4; i++) {
                printf("\n");
                for (j = 0; j < 5; j++) {
                    printf(" %s=", a4a5s[i][j]);
                    printf("%s", (*a4ps[i])[j]);
                }
            }
            printf("\n");
            for (i = 0; i < 4; i++) {
                printf(" %p", a4ps[i]);
            }
            printf("\n");
        }
        when (delay(1)) {
            exit(0);
        } state init
    }
}
