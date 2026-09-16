void _start(void) {
    // Ring 3 사용자 공간 테스트 코드
    volatile char *msg = "HELLO FROM MACH-O USERLAND!";
    (void)msg;
    while (1);
}
