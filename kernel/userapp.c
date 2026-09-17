void _start(void) {
    // Ring 3 사용자 공간 테스트 코드
    volatile char *msg = "HELLO FROM MACH-O USERLAND!";
    (void)msg;
#ifdef USERAPP_FAULT_TEST
    // 하위 identity mapping은 Task의 user 권한으로 공개되지 않는다.
    // 이 쓰기는 #PF를 일으키며 커널의 user-fault 복구 경로를 검증한다.
    *(volatile unsigned long long *)0x200000 = 0xC0DEC0DE;
#endif
    while (1)
        ;
}
