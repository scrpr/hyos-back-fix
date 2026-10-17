.text
.p2align 2
.global disableWindowReuse
.hidden disableWindowReuse
#if BACKFIX_STATUS_SERVER
.hidden noReuseEnabled
.hidden originalNoReuse
#endif
.type disableWindowReuse, %function
disableWindowReuse:
#if BACKFIX_STATUS_SERVER
    adrp x16, noReuseEnabled
    add x16, x16, :lo12:noReuseEnabled
    ldar w17, [x16]
    cbnz w17, 1f
    adrp x16, originalNoReuse
    ldr x16, [x16, :lo12:originalNoReuse]
    br x16
1:
#endif
    // Dart false = NULL_REG + 0x30.
    add x0, x22, #0x30
    ret
.size disableWindowReuse, .-disableWindowReuse
.section .note.GNU-stack,"",%progbits
