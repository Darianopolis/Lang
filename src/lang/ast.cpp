#include "lang.hpp"

// #include <zstd.h>

NAMESPACE_BEGIN(lang)

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
        auto header = std::bit_cast<AstHeader>(*ip);
        compacted.write(header);
        switch (header.type) {
            case AstNodeType::File:
            case AstNodeType::Block:
            case AstNodeType::Defer:
                compact_list(ip[1]);
                break;
            case AstNodeType::Number:
            case AstNodeType::String:
            case AstNodeType::Identifier:
                compacted.write(ip[1]);
                break;
            case AstNodeType::FieldAccess:
                compacted.write(ip[1]);
                compact_node(ip[2]);
                break;
            case AstNodeType::Op:
                if (ast_get_op_arity(header) >= 1) compact_node(ip[1]);
                if (ast_get_op_arity(header) == 2) compact_node(ip[2]);
                break;
            case AstNodeType::VarDecl:
                compact_node(ip[1]);
                ip += 2;
                if (header.flags & Ast_VarDecl_HasType) {
                    compact_node(*ip);
                    ip += 1;
                }
                if (header.flags & Ast_VarDecl_HasInit) {
                    compact_node(*ip);
                }
                break;
            case AstNodeType::Function:
                compact_list(ip[1]);
                compact_node(ip[2]);
                if (header.flags & Ast_Node_HasMainBlock) {
                    compact_list(ip[3]);
                }
                break;
            case AstNodeType::Call:
            case AstNodeType::Subscript:
                compact_node(ip[1]);
                compact_list(ip[2]);
                break;
            case AstNodeType::While:
            case AstNodeType::If:
                compact_node(ip[1]);
                compact_list(ip[2]);
                if (header.flags & Ast_Node_HasAlternativeBlock) {
                    compact_list(ip[3]);
                }
                break;
            case AstNodeType::For:
                compact_node(ip[1]);
                compact_list(ip[2]);
                compact_list(ip[3]);
                if (header.flags & Ast_Node_HasAlternativeBlock) {
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

        #define PRINT(fmt, ...) std::println("{}" fmt, indent_str(depth) __VA_OPT__(,) __VA_ARGS__)

        auto print_list = [&](AstOffset next, auto depth) {
            while (next) {
                print_node(next + 1, depth);
                next = *ast.pointer_to(next);
            }
        };

        auto header = std::bit_cast<AstHeader>(*ip);

        switch (header.type) {
            case AstNodeType::File:
                print_list(ip[1], depth);
                break;
            case AstNodeType::Block:
                PRINT("Block");
                print_list(ip[1], depth + 1);
                break;
            case AstNodeType::Number:
                PRINT("Number<{}>({})", ast_get_number_base(header), constants.numbers.from_id(ip[1]));
                break;
            case AstNodeType::String:
                PRINT("String({})", constants.strings.from_id(ip[1]));
                break;
            case AstNodeType::Boolean:
                PRINT("Boolean({})", header.flags & Ast_Boolean_True);
                break;
            case AstNodeType::FieldAccess:
                PRINT("FieldAccess({})", constants.identifiers.from_id(ip[1]));
                print_node(ip[2], depth + 1);
                break;
            case AstNodeType::Identifier:
                PRINT("Identifier({})", constants.identifiers.from_id(ip[1]));
                break;
            case AstNodeType::Op:
                PRINT("Op<{}>({})", ast_get_op_arity(header), get_op_name(ast_get_op_type(header)));
                break;
            case AstNodeType::VarDecl:
                PRINT("Decl<{}>", (header.flags & Ast_VarDecl_IsConstant) ? "Const" : "Var");
                print_node(ip[1], depth + 1);
                ip += 2;
                if (header.flags & Ast_VarDecl_HasType) {
                    PRINT(".TypeExpr");
                    print_node(*ip, depth + 1);
                    ip += 1;
                }
                if (header.flags & Ast_VarDecl_HasInit) {
                    PRINT(".Init");
                    print_node(*ip, depth + 1);
                }
                break;
            case AstNodeType::Function:
                PRINT("Function");
                PRINT(".Params");
                print_list(ip[1], depth + 1);
                PRINT(".ReturnType");
                print_node(ip[2], depth + 1);
                if (header.flags & Ast_Node_HasMainBlock) {
                    PRINT(".Body");
                    print_list(ip[3], depth + 1);
                }
                break;
            case AstNodeType::Defer:
                PRINT("Defer");
                print_list(ip[1], depth + 1);
                break;
            case AstNodeType::Call:
            case AstNodeType::Subscript:
                PRINT("{}", get_ast_node_type_name(header.type));
                print_node(ip[1], depth + 1);
                PRINT(".Args");
                print_list(ip[2], depth + 1);
                break;
            case AstNodeType::While:
                PRINT("While");
                print_node(ip[1], depth + 1);
                PRINT(".Body");
                print_list(ip[2], depth + 1);
                if (header.flags & Ast_Node_HasAlternativeBlock) {
                    PRINT(".Else");
                    print_list(ip[3], depth + 1);
                }
                break;
            case AstNodeType::For:
                PRINT("For");
                print_node(ip[1], depth + 1);
                PRINT(".Params");
                print_list(ip[2], depth + 1);
                PRINT(".Body");
                print_list(ip[3], depth + 1);
                if (header.flags & Ast_Node_HasAlternativeBlock) {
                    PRINT(".Else");
                    print_list(ip[4], depth + 1);
                }
                break;
            case AstNodeType::If:
                PRINT("If");
                print_node(ip[1], depth + 1);
                PRINT(".Then");
                print_list(ip[2], depth + 1);
                if (header.flags & Ast_Node_HasAlternativeBlock) {
                    PRINT(".Else");
                    print_list(ip[3], depth + 1);
                }
                break;
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

    log_debug("unique identifiers = {} / {} ({:.2f}x)", constants.identifiers.unique_count(), constants.identifiers.total_lookups, f64(constants.identifiers.total_lookups) / constants.identifiers.unique_count());
    log_debug("unique strings     = {} / {} ({:.2f}x)", constants.strings.unique_count(), constants.strings.total_lookups, f64(constants.strings.total_lookups) / constants.strings.unique_count());
    log_debug("unique numbers     = {} / {} ({:.2f}x)", constants.numbers.unique_count(), constants.numbers.total_lookups, f64(constants.numbers.total_lookups) / constants.numbers.unique_count());

    log_debug("   source = {}", byte_size_to_string(source.size()));
    auto comparison_size = source.size();

	log_debug(" ast size = {:10} | {:.2f}x",
        byte_size_to_string(ast.code.size() * 4),
        f64(ast.code.size() * 4) / comparison_size);

    log_debug("compacted = {:10} | {:.2f}x",
        byte_size_to_string(compacted.code.size() * 4),
        f64(compacted.code.size() * 4) / comparison_size);
}

NAMESPACE_END
