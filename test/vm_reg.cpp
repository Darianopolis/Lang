#include <print>

using inst_t = int64_t;

enum : inst_t
{
    OP_LOAD_NUMBER, // num     -> [loc]
    OP_SUBTRACT_RL, // [a]  b  -> [dst]
    OP_ADD,         // [a] [b] -> [dst]
    OP_LESS_RL,     // [a]  b  -> [dst]
    OP_JUMP_TRUE,   // [a]     -> delta
    OP_CALL1,       // ret addr
    OP_RETURN,
    OP_HALT,
};

inst_t bytecode[] {
    // Function
    OP_LESS_RL,         // [ n n<2 ]
    0,
    2,
    1,
    OP_JUMP_TRUE,       //
    1,
    21,
    OP_SUBTRACT_RL,     // [ n n-1 ]
    0,
    1,
    1,
    OP_CALL1,           // [ n fib(n-1) ]
    1,
    0,
    OP_SUBTRACT_RL,     // [ n fib(n-1) n-2 ]
    0,
    2,
    2,
    OP_CALL1,           // [ n fib(n-1) fib(n-2) ]
    2,
    0,
    OP_ADD,             // [ fib(n-1)+fib(n-2) ]
    1,
    2,
    0,
    OP_RETURN,

    // Entry point
    OP_LOAD_NUMBER, 40, 0,  // [ n ]
    OP_CALL1,               // [ fib(n) ]
    0, 0,
    OP_HALT,
};

struct Frame {
    inst_t* ip;
    double* sp;
};

double                         stack[256];
Frame frames[256];

// std::array strs {
//     "OP_LOAD_NUMBER",
//     "OP_SUBTRACT_RL",
//     "OP_ADD",
//     "OP_LESS_RL",
//     "OP_JUMP_TRUE",
//     "OP_CALL1",
//     "OP_RETURN",
//     "OP_HALT",
// };

void transform_bytecode(inst_t* ip, void** labels)
{
    std::array op_size {
        3,
        4,
        4,
        4,
        3,
        3,
        1,
        1,
    };

    for (;;) {
        auto op = *ip;
        *ip = inst_t(labels[op]);
        ip += op_size[op];
        if (op == OP_HALT) break;
    }
}

double run_goto()
{
    // std::array<std::pair<void*, size_t>, strs.size()> direct_thread_lookup
    //     // = std::to_array<std::pair<void*, size_t>>({
    // {{
    //     { &&GOTO_LOAD_NUMBER, 3 },
    //     { &&GOTO_SUBTRACT_RL, 4 },
    //     { &&GOTO_ADD, 4 },
    //     { &&GOTO_LESS_RL, 4 },
    //     { &&GOTO_JUMP_TRUE, 3 },
    //     { &&GOTO_CALL1, 3 },
    //     { &&GOTO_RETURN, 1 },
    //     { &&GOTO_HALT, 1 },
    // }};
    // // });

    std::array jump_table {
        &&GOTO_LOAD_NUMBER,
        &&GOTO_SUBTRACT_RL,
        &&GOTO_ADD,
        &&GOTO_LESS_RL,
        &&GOTO_JUMP_TRUE,
        &&GOTO_CALL1,
        &&GOTO_RETURN,
        &&GOTO_HALT,
    };

    // {
    //     inst_t* ip = bytecode;
    //     for (;;) {
    //         auto op = *ip;
    //         auto[addr, size] = direct_thread_lookup[op];
    //         // std::println("[{:20}] = ({}, {})", strs[op], jump_table.at(op), size);
    //         *ip = inst_t(addr);
    //         ip += size;
    //         if (op == OP_HALT) break;
    //     }
    // }

    // inst_t bytecode[] {
    //     // Function
    //     inst_t(&&GOTO_LESS_RL),         // [ n n<2 ]
    //     0,
    //     2,
    //     1,
    //     inst_t(&&GOTO_JUMP_TRUE),       //
    //     1,
    //     21,
    //     inst_t(&&GOTO_SUBTRACT_RL),     // [ n n-1 ]
    //     0,
    //     1,
    //     1,
    //     inst_t(&&GOTO_CALL1),           // [ n fib(n-1) ]
    //     1,
    //     0,
    //     inst_t(&&GOTO_SUBTRACT_RL),     // [ n fib(n-1) n-2 ]
    //     0,
    //     2,
    //     2,
    //     inst_t(&&GOTO_CALL1),           // [ n fib(n-1) fib(n-2) ]
    //     2,
    //     0,
    //     inst_t(&&GOTO_ADD),             // [ fib(n-1)+fib(n-2) ]
    //     1,
    //     2,
    //     0,
    //     inst_t(&&GOTO_RETURN),

    //     // Entry point
    //     inst_t(&&GOTO_LOAD_NUMBER), 40, 0,  // [ n ]
    //     inst_t(&&GOTO_CALL1),               // [ fib(n) ]
    //     0, 0,
    //     inst_t(&&GOTO_HALT),
    // };

    transform_bytecode(bytecode, jump_table.data());
    #define dispatch goto *(void*)*ip;

    // #define dispatch goto *jump_table[*ip];

    inst_t* ip = bytecode + 26;
    double* sp = stack;
    Frame* fp = frames;

    dispatch;

    GOTO_LOAD_NUMBER:
    {
        sp[ip[2]] = ip[1];
        ip += 3;
        dispatch;
    }
    GOTO_SUBTRACT_RL:
    {
        auto l = sp[ip[1]];
        auto r =    ip[2];
        sp[ip[3]] = l - r;
        ip += 4;
        dispatch;
    }
    GOTO_ADD:
    {
        auto l = sp[ip[1]];
        auto r = sp[ip[2]];
        sp[ip[3]] = l + r;
        ip += 4;
        dispatch;
    }
    GOTO_LESS_RL:
    {
        auto l = sp[ip[1]];
        auto r =    ip[2];
        sp[ip[3]] = l < r;
        ip += 4;
        dispatch;
    }
    GOTO_JUMP_TRUE:
    {
        auto c = sp[ip[1]];
        if (c) {
            ip += ip[2];
        } else {
            ip += 3;
        }
        dispatch;
    }
    GOTO_CALL1:
    {
        auto addr = ip[2];
        *(fp++) = { ip + 3, sp };
        sp += ip[1];
        ip = bytecode + uintptr_t(addr);
        dispatch;
    }
    GOTO_RETURN:
    {
        --fp;
        ip = fp->ip;
        sp = fp->sp;
        dispatch;
    }
    GOTO_HALT:
    {
        return *sp;
    }
}

int main()
{
    auto res = run_goto();
    std::println("res = {}", res);
}

// std::array strs {
//     "OP_LOAD_NUMBER",
//     "OP_SUBTRACT_RL",
//     "OP_ADD",
//     "OP_LESS_RL",
//     "OP_JUMP_TRUE",
//     "OP_CALL1",
//     "OP_RETURN",
//     "OP_HALT",
// };

    // std::array jump_table {
    //     &&GOTO_LOAD_NUMBER,
    //     &&GOTO_SUBTRACT_RL,
    //     &&GOTO_ADD,
    //     &&GOTO_LESS_RL,
    //     &&GOTO_JUMP_TRUE,
    //     &&GOTO_CALL1,
    //     &&GOTO_RETURN,
    //     &&GOTO_HALT,
    // };

    // // #define dispatch std::println("before, ip = {}, *ip = {} --> {}", ip - bytecode, *ip, strs[*ip]); \
    // //      goto *jump_table[*ip]
    // #define dispatch goto *jump_table[*ip];
