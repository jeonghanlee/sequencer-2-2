program misplacedExitTest

ss x { 
    state y {
        when () {
        } state y
        exit {
        }
        when() {
        } state y
    }
}
