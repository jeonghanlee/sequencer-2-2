// run this as e.g.
// ./O.linux-x86_64/parameter "var=value"
// ./O.linux-x86_64/parameter "var="
program parameterTest("var=default")

ss one {
    state one {
        when() {
            printf("var=%s\n",macValueGet("var"));
        } exit
    }
}
