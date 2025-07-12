#include "lang.hpp"

// #include <zstd.h>

NAMESPACE_BEGIN(lang)

void ast_print_flat(Constants& constants, Ast& ast)
{
    u32 offset = 0;
    for (auto v : ast.code) {
        std::print("[{:3}] ", offset++);
        if (v >= AstHeader_Start) {
            std::println("{}", get_ast_header_name(AstHeader(v)));
        } else {
            std::println("{}", v);
        }
    }
}

void ast_compact(Constants& constants, Ast& ast, AstOffset offset, Ast& compacted)
{
    auto compact_node = [&](this auto&& compact_node, AstOffset offset) -> void {

        auto* ip = ast.pointer_to(offset);

        auto compact_list = [&](AstOffset next) {
            while (next) {
                compact_node(next + 1);
                next = *ast.pointer_to(AstOffset{next});
            }
            compacted.write(AstOffset{});
        };
        auto header = AstHeader(*ip);
        compacted.write(header);
        switch (header) {
            case AstHeader::File:
            case AstHeader::Block:
                compact_list(ip[1]);
                break;
            case AstHeader::Literal_Number:
            case AstHeader::Literal_String:
            case AstHeader::Identifier:
                compacted.write(ip[1]);
                break;
            case AstHeader::FieldAccess:
                compacted.write(ip[1]);
                compact_node(ip[2]);
                break;
            case AstHeader::Dereference:
            case AstHeader::Postincrement:
            case AstHeader::Postdecrement:
            case AstHeader::Break:
            case AstHeader::Continue:
            case AstHeader::Return:
            #define X(name, ...) case AstHeader::name:
                OPS_Prefix
            #undef X
                compact_node(ip[1]);
                break;
            #define X(name, ...) case AstHeader::name:
                OPS_Arithmetic
                OPS_Bitwise
                OPS_Logical
                OPS_Comparison
                OPS_Range
                OPS_Assign
            #undef X
                compact_node(ip[1]);
                compact_node(ip[2]);
                break;
            case AstHeader::VarDecl:
            case AstHeader::VarDecl_Type:
            case AstHeader::VarDecl_ConstInit:
            case AstHeader::VarDecl_MutableInit:
            case AstHeader::VarDecl_TypeConstInit:
            case AstHeader::VarDecl_TypeMutableInit:
                compact_node(ip[1]);
                ip += 2;
                if (ast_vardecl_header_has_type(header)) {
                    compact_node(*ip);
                    ip += 1;
                }
                if (ast_vardecl_header_has_init(header)) {
                    compact_node(*ip);
                }
                break;
            case AstHeader::Function_Definition:
            case AstHeader::Function_Prototype:
                compact_list(ip[1]);
                compact_node(ip[2]);
                if (header == AstHeader::Function_Definition) {
                    compact_list(ip[3]);
                }
                break;
            case AstHeader::Defer:
                compact_list(ip[1]);
                break;
            case AstHeader::Call:
            case AstHeader::Subscript:
                compact_node(ip[1]);
                compact_list(ip[2]);
                break;
            case AstHeader::While:
            case AstHeader::WhileElse:
            case AstHeader::If:
            case AstHeader::IfElse:
                compact_node(ip[1]);
                compact_list(ip[2]);
                if (header == AstHeader::WhileElse || header == AstHeader::IfElse) {
                    compact_list(ip[3]);
                }
                break;
            case AstHeader::For:
            case AstHeader::ForElse:
                compact_node(ip[1]);
                compact_list(ip[2]);
                compact_list(ip[3]);
                if (header == AstHeader::ForElse) {
                    compact_list(ip[4]);
                }
                break;
            default:
                ;
        }
    };

    compact_node(offset);
}

void ast_print_tree(Constants& constants, Ast& ast, AstOffset offset)
{
    auto print_node_root = [&](this auto&& print_node, AstOffset offset, u64 depth) -> void {

        u32* ip = ast.pointer_to(offset);

        // #define PRINT(...)
        #define PRINT(fmt, ...) std::println("{}" fmt, indent_str(depth) __VA_OPT__(,) __VA_ARGS__)

        auto print_list = [&](AstOffset next, auto depth) {

            while (next) {
                print_node(next + 1, depth);
                next = *ast.pointer_to(next);
            }
            // compacted.write(AstOffset{});
        };

        auto header = AstHeader(*ip);

        // compacted.write(header);
        switch (header) {
            case AstHeader::File:
                print_list(ip[1], depth);
                break;
            case AstHeader::Block:
                PRINT("Block");
                print_list(ip[1], depth + 1);
                break;
            case AstHeader::Literal_Number:
                PRINT("Literal_Number({})", constants.numbers.from_id(ip[1]));
                // compacted.write(ip[1]);
                break;
            case AstHeader::Literal_String:
                PRINT("Literal_String({})", constants.strings.from_id(ip[1]));
                // compacted.write(ip[1]);
                break;
            case AstHeader::Literal_True:
            case AstHeader::Literal_False:
            case AstHeader::Break_Empty:
            case AstHeader::Continue_Empty:
            case AstHeader::Return_Empty:
                PRINT("{}", get_ast_header_name(header));
                break;
            case AstHeader::FieldAccess:
                PRINT("FieldAccess({})", constants.identifiers.from_id(ip[1]));
                print_node(ip[2], depth + 1);
                // compacted.write(ip[1]);
                break;
            case AstHeader::Identifier:
                PRINT("Identifier({})", constants.identifiers.from_id(ip[1]));
                // compacted.write(ip[1]);
                break;
// -----------------------------------------------------------------------------
            case AstHeader::Dereference:
            case AstHeader::Postincrement:
            case AstHeader::Postdecrement:
            case AstHeader::Break:
            case AstHeader::Continue:
            case AstHeader::Return:
            #define X(name, ...) case AstHeader::name:
                OPS_Prefix
            #undef X
                PRINT("{}", get_ast_header_name(header));
                print_node(ip[1], depth + 1);
                break;
// -----------------------------------------------------------------------------
            #define X(name, ...) case AstHeader::name:
                OPS_Arithmetic
                OPS_Bitwise
                OPS_Logical
                OPS_Comparison
                OPS_Range
                OPS_Assign
            #undef X
                PRINT("{}", get_ast_header_name(header));
                print_node(ip[1], depth + 1);
                print_node(ip[2], depth + 1);
                break;
// -----------------------------------------------------------------------------
            case AstHeader::VarDecl:
            case AstHeader::VarDecl_Type:
            case AstHeader::VarDecl_ConstInit:
            case AstHeader::VarDecl_MutableInit:
            case AstHeader::VarDecl_TypeConstInit:
            case AstHeader::VarDecl_TypeMutableInit:
                PRINT("VarDecl");
                print_node(ip[1], depth + 1);
                ip += 2;
                if (ast_vardecl_header_has_type(header)) {
                    PRINT(".TypeExpr");
                    print_node(*ip, depth + 1);
                    ip += 1;
                }
                if (ast_vardecl_header_has_init(header)) {
                    PRINT(".{}", ast_vardecl_header_is_constant(header) ? "ConstInit" : "MutableInit");
                    print_node(*ip, depth + 1);
                }
                break;
            case AstHeader::Function_Definition:
            case AstHeader::Function_Prototype:
                PRINT("{}", get_ast_header_name(header));
                PRINT(".Params");
                print_list(ip[1], depth + 1);
                PRINT(".ReturnType");
                print_node(ip[2], depth + 1);
                if (header == AstHeader::Function_Definition) {
                    PRINT(".Body");
                    print_list(ip[3], depth + 1);
                }
                break;
            case AstHeader::Defer:
                PRINT("Defer");
                print_list(ip[1], depth + 1);
                break;
            case AstHeader::Call:
            case AstHeader::Subscript:
                PRINT("{}", get_ast_header_name(header));
                print_node(ip[1], depth + 1);
                PRINT(".Args");
                print_list(ip[2], depth + 1);
                break;
            case AstHeader::While:
            case AstHeader::WhileElse:
                PRINT("While");
                print_node(ip[1], depth + 1);
                PRINT(".Body");
                print_list(ip[2], depth + 1);
                if (header == AstHeader::WhileElse) {
                    PRINT(".Else");
                    print_list(ip[3], depth + 1);
                }
                break;
            case AstHeader::For:
            case AstHeader::ForElse:
                PRINT("For");
                print_node(ip[1], depth + 1);
                PRINT(".Params");
                print_list(ip[2], depth + 1);
                PRINT(".Body");
                print_list(ip[3], depth + 1);
                if (header == AstHeader::ForElse) {
                    PRINT(".Else");
                    print_list(ip[4], depth + 1);
                }
                break;
            case AstHeader::If:
            case AstHeader::IfElse:
                PRINT("If");
                print_node(ip[1], depth + 1);
                PRINT(".Then");
                print_list(ip[2], depth + 1);
                if (header == AstHeader::IfElse) {
                    PRINT(".Else");
                    print_list(ip[3], depth + 1);
                }
                break;
            default:
                raise_error("unhandled ast header: {}", get_ast_header_name(header));
        }

        #undef PRINT
    };

    print_node_root(offset, 0);
}

// -----------------------------------------------------------------------------

void ast_print_details(Constants& constants, Ast& ast, AstOffset offset, std::string_view source)
{
    Ast compacted;
    ast_compact(constants, ast, offset, compacted);

#if 0
    log_debug("Linked");
    ast_print_flat(constants, ast);

    log_debug("Compacted");
    ast_print_flat(constants, compacted);
#endif

#if 1
    log_debug("Tree");
    ast_print_tree(constants, ast, offset);
#endif

    static std::vector<std::byte> storage;
    auto test_calculate_compressed_size = [&](const void* data, size_t size)
    {
        // storage.resize(ZSTD_compressBound(size));
        // return ZSTD_compress(storage.data(), storage.size(), data, size, 0);
        // TODO
        return size;
    };

    auto compress_size = test_calculate_compressed_size(source.data(), source.size());

    log_debug("unique identifiers = {} / {} ({:.2f}x)", constants.identifiers.unique_count(), constants.identifiers.total_lookups, f64(constants.identifiers.total_lookups) / constants.identifiers.unique_count());
    log_debug("unique strings     = {} / {} ({:.2f}x)", constants.strings.unique_count(), constants.strings.total_lookups, f64(constants.strings.total_lookups) / constants.strings.unique_count());
    log_debug("unique numbers     = {} / {} ({:.2f}x)", constants.numbers.unique_count(), constants.numbers.total_lookups, f64(constants.numbers.total_lookups) / constants.numbers.unique_count());

    log_trace("            source = {}", byte_size_to_string(source.size()));
    log_trace(" compressed source = {}", byte_size_to_string(compress_size));

    auto comparison_size = source.size();

    compress_size = test_calculate_compressed_size(compacted.code.data(), compacted.code.size() * 4);
    log_trace("    compressed AST = {}", byte_size_to_string(compress_size));

	log_debug(" ast size = {:9} ({:5.2f}x)",
        byte_size_to_string(ast.code.size() * 4),
        f64(ast.code.size() * 4) / comparison_size);

    log_debug("compacted = {:9} ({:5.2f}x)",
        byte_size_to_string(compacted.code.size() * 4),
        f64(compacted.code.size() * 4) / comparison_size);
}

NAMESPACE_END
