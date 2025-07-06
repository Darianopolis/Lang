#include <print>

using inst_t = int64_t;

enum : inst_t
{
    OP_LOAD_LOCAL,
    OP_LOAD_NUMBER,
    OP_SUBTRACT,
    OP_ADD,
    OP_LESS,
    OP_JUMP_FALSE,
    OP_JUMP,
    OP_PUSHDOWN,
    OP_CALL1,
    OP_RETURN,  // push down one on return
    OP_HALT,
};

inst_t bytecode[] {
    // Function
    OP_LOAD_LOCAL,   1, // [ n n ]
    OP_LOAD_NUMBER,  2, // [ n n 2 ]
    OP_LESS,            // [ n n<2 ]
    OP_JUMP_FALSE,   6, // [ n ]
    OP_LOAD_LOCAL,   1, // [ n n ]
    OP_JUMP,        19,
    OP_LOAD_LOCAL,   1, // [ n n ]
    OP_LOAD_NUMBER,  1, // [ n n 1 ]
    OP_SUBTRACT,        // [ n n-1 ]
    OP_LOAD_NUMBER,  0, // [ n n-1 fib ]
    OP_CALL1,           // [ n fib(n-1) ]
    OP_LOAD_LOCAL,   2, // [ n fib(n-1) n ]
    OP_LOAD_NUMBER,  2, // [ n fib(n-1) n 2 ]
    OP_SUBTRACT,        // [ n fib(n-1) n-2 ]
    OP_LOAD_NUMBER,  0, // [ n fib(n-1) n-2 fib ]
    OP_CALL1,           // [ n fib(n-1) fib(n-2) ]
    OP_ADD,             // [ n fib(n-1)+fib(n-2) ]
    OP_PUSHDOWN,        // [ fib(n-1)+fib(n-2) ]
    OP_RETURN,

    // Entry point
    OP_LOAD_NUMBER, 40, // [ n ]
    OP_LOAD_NUMBER,  0, // [ n fib ]
    OP_CALL1,           // [ fib(n) ]
    OP_HALT,
};

double     stack[256];
inst_t* ip_stack[256];

double run_goto()
{
    // inst_t bytecode[] {
    //     // Function
    //     inst_t(&&GOTO_LOAD_LOCAL),   1, // [ n n ]
    //     inst_t(&&GOTO_LOAD_NUMBER),  2, // [ n n 2 ]
    //     inst_t(&&GOTO_LESS),            // [ n n<2 ]
    //     inst_t(&&GOTO_JUMP_FALSE),   6, // [ n ]
    //     inst_t(&&GOTO_LOAD_LOCAL),   1, // [ n n ]
    //     inst_t(&&GOTO_JUMP),        19,
    //     inst_t(&&GOTO_LOAD_LOCAL),   1, // [ n n ]
    //     inst_t(&&GOTO_LOAD_NUMBER),  1, // [ n n 1 ]
    //     inst_t(&&GOTO_SUBTRACT),        // [ n n-1 ]
    //     inst_t(&&GOTO_LOAD_NUMBER),  0, // [ n n-1 fib ]
    //     inst_t(&&GOTO_CALL1),           // [ n fib(n-1) ]
    //     inst_t(&&GOTO_LOAD_LOCAL),   2, // [ n fib(n-1) n ]
    //     inst_t(&&GOTO_LOAD_NUMBER),  2, // [ n fib(n-1) n 2 ]
    //     inst_t(&&GOTO_SUBTRACT),        // [ n fib(n-1) n-2 ]
    //     inst_t(&&GOTO_LOAD_NUMBER),  0, // [ n fib(n-1) n-2 fib ]
    //     inst_t(&&GOTO_CALL1),           // [ n fib(n-1) fib(n-2) ]
    //     inst_t(&&GOTO_ADD),             // [ n fib(n-1)+fib(n-2) ]
    //     inst_t(&&GOTO_PUSHDOWN),        // [ fib(n-1)+fib(n-2) ]
    //     inst_t(&&GOTO_RETURN),

    //     // Entry point
    //     inst_t(&&GOTO_LOAD_NUMBER), 40, // [ n ]
    //     inst_t(&&GOTO_LOAD_NUMBER),  0, // [ n fib ]
    //     inst_t(&&GOTO_CALL1),           // [ fib(n) ]
    //     inst_t(&&GOTO_HALT),
    // };

    // #define dispatch *(void*)*ip;

    inst_t* ip = bytecode + 30;
    double* sp = stack;

    uint8_t call_depth = 0;

    std::array jump_table {
        &&GOTO_LOAD_LOCAL,
        &&GOTO_LOAD_NUMBER,
        &&GOTO_SUBTRACT,
        &&GOTO_ADD,
        &&GOTO_LESS,
        &&GOTO_JUMP_FALSE,
        &&GOTO_JUMP,
        &&GOTO_PUSHDOWN,
        &&GOTO_CALL1,
        &&GOTO_RETURN,
        &&GOTO_HALT,
    };

    #define dispatch *jump_table[*ip]

    goto dispatch;

    GOTO_LOAD_LOCAL:
    {
        auto delta = -*(ip + 1);
        *sp++ = sp[delta];
        ip += 2;
        goto dispatch;
    }
    GOTO_LOAD_NUMBER:
    {
        *sp++ = *(ip + 1);
        ip += 2;
        goto dispatch;
    }
    GOTO_SUBTRACT:
    {
        auto r = *--sp;
        auto l = *--sp;
        *sp++ = l - r;
        ip++;
        goto dispatch;
    }
    GOTO_ADD:
    {
        auto r = *--sp;
        auto l = *--sp;
        *sp++ = l + r;
        ip++;
        goto dispatch;
    }
    GOTO_LESS:
    {
        auto r = *--sp;
        auto l = *--sp;
        *sp++ = l < r;
        ip++;
        goto dispatch;
    }
    GOTO_JUMP_FALSE:
    {
        auto c = *--sp;
        if (!c) {
            ip += *(ip + 1);
        } else {
            ip += 2;
        }
        goto dispatch;
    }
    GOTO_JUMP:
    {
        ip += *(ip + 1);
        goto dispatch;
    }
    GOTO_PUSHDOWN:
    {
        auto v = *(--sp);
        *(sp - 1) = v;
        ip++;
        goto dispatch;
    }
    GOTO_CALL1:
    {
        auto addr = *(--sp);
        ip_stack[call_depth++] = ip + 1;
        ip = bytecode + uintptr_t(addr);
        goto dispatch;
    }
    GOTO_RETURN:
    {
        ip = ip_stack[--call_depth];
        goto dispatch;
    }
    GOTO_HALT:
    {
        return *(--sp);
    }
}

// double run_loop()
// {
//     inst_t* ip = bytecode + 30;
//     double* sp = stack;

//     uint8_t call_depth = 0;

//     for (;;) {
//         switch (*ip) {
//             case OP_LOAD_LOCAL: {
//                 auto delta = -*(ip + 1);
//                 *sp++ = sp[delta];
//                 ip += 2;
//                 break;
//             }
//             case OP_LOAD_NUMBER:
//                 *sp++ = *(ip + 1);
//                 ip += 2;
//                 break;
//             case OP_SUBTRACT: {
//                 auto r = *--sp;
//                 auto l = *--sp;
//                 *sp++ = l - r;
//                 ip++;
//                 break;
//             }
//             case OP_ADD: {
//                 auto r = *--sp;
//                 auto l = *--sp;
//                 *sp++ = l + r;
//                 ip++;
//                 break;
//             }
//             case OP_LESS: {
//                 auto r = *--sp;
//                 auto l = *--sp;
//                 *sp++ = l < r;
//                 ip++;
//                 break;
//             }
//             case OP_JUMP_FALSE: {
//                 auto c = *--sp;
//                 if (!c) {
//                     ip += *(ip + 1);
//                 } else {
//                     ip += 2;
//                 }
//                 break;
//             }
//             case OP_JUMP: {
//                 ip += *(ip + 1);
//                 break;
//             }
//             case OP_PUSHDOWN: {
//                 auto v = *(--sp);
//                 *(sp - 1) = v;
//                 ip++;
//                 break;
//             }
//             case OP_CALL1: {
//                 auto addr = *(--sp);
//                 ip_stack[call_depth++] = ip + 1;
//                 ip = bytecode + uintptr_t(addr);
//                 break;
//             }
//             case OP_RETURN: {
//                 ip = ip_stack[--call_depth];
//                 break;
//             }
//             case OP_HALT: {
//                 return *(--sp);
//             }
//         }
//     }
// }

double fib(double n)
{
    return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

int main()
{
    auto res = run_goto();
    // auto res = run_loop();
    // auto res = fib(40);
    std::println("res = {}", res);
}
