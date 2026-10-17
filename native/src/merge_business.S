.text
.p2align 2
.global prepareMergeBusiness
.hidden prepareMergeBusiness
.hidden mergeKeyRequests
.hidden mergePrepared
.hidden mergeInterruptOffset
.hidden originalMergeBusiness
.type prepareMergeBusiness, %function
prepareMergeBusiness:
    // Save arguments on Dart's stack; CSP is not the active Dart stack pointer.
    stp x2, x3, [x15, #-32]!
    str x4, [x15, #16]
    adrp x2, mergeKeyRequests
    add x2, x2, :lo12:mergeKeyRequests
    ldar w2, [x2]
    cbz w2, 1f
    // The stock method has already placed the animation manager in x0.
    adrp x2, mergeInterruptOffset
    ldrsw x2, [x2, :lo12:mergeInterruptOffset]
    add x4, x22, #0x20 // Dart true.
    str w4, [x0, x2]
    adrp x2, mergePrepared
    add x2, x2, :lo12:mergePrepared
    mov w4, #1
    stlr w4, [x2]
1:
    ldr x4, [x15, #16]
    ldp x2, x3, [x15], #32
    adrp x16, originalMergeBusiness
    ldr x16, [x16, :lo12:originalMergeBusiness]
    br x16
.size prepareMergeBusiness, .-prepareMergeBusiness
.section .note.GNU-stack,"",%progbits
