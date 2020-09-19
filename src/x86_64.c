#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "x86_64.h"
#include "utils.h"


// increasing "cost"
enum ax64_reg {
	AX64_RDI,
	AX64_RSI,
	AX64_RDX,
	AX64_RCX,
	AX64_R9,
	AX64_R8,
	// retval
	AX64_RAX,
	// saved regs
	AX64_RBX,
	AX64_RBP,
	AX64_RSP,
	AX64_R10,
	AX64_R11,
	AX64_R12,
	AX64_R13,
	AX64_R14,
	AX64_R15,
	AX64_R16,
	// aliases
	AX64_RETVAL = AX64_RAX,
	AX64_ARG0 = AX64_RDI,
	AX64_ARG1 = AX64_RSI,
	AX64_ARG2 = AX64_RDX,
	AX64_ARG3 = AX64_RCX,
	AX64_ARG4 = AX64_R9,
	AX64_ARG5 = AX64_R8,
	AX64_REG_ARG_END = AX64_ARG5,
};

static const char *reg2str[] = {
	[AX64_RDI] = "%rdi",
	[AX64_RSI] = "%rsi",
	[AX64_RDX] = "%rdx",
	[AX64_RCX] = "%rcx",
	[AX64_R9 ] = "%r9 ",
	[AX64_R8 ] = "%r8 ",
	[AX64_RAX] = "%rax",
	[AX64_RBX] = "%rbx",
	[AX64_RBP] = "%rbp",
	[AX64_RSP] = "%rsp",
	[AX64_R10] = "%r10",
	[AX64_R11] = "%r11",
	[AX64_R12] = "%r12",
	[AX64_R13] = "%r13",
	[AX64_R14] = "%r14",
	[AX64_R15] = "%r15",
	[AX64_R16] = "%r16",
};

static const struct identifier ax64_retreg = { .len = 4, .name = "%rax", };

void ax64_gen_program(FILE *f, struct ir_program *pgrm)
{
	assert(buf_len(pgrm->defs));

	for (ssize_t i = 0; i < buf_len(pgrm->defs); i++) {
		struct identifier *id = &pgrm->defs[i].name;
		fprintf(f, ".globl %.*s\n", (int) id->len, id->name);
	}
	fputs("\n\n", f);

	for (ssize_t i = 0; i < buf_len(pgrm->defs); i++) {
		struct ir_definition *def = pgrm->defs + i;
		fprintf(f, "%.*s:\n", (int) def->name.len, def->name.name);
		for (ssize_t s = 0; s < buf_len(def->stmts); s++) {
			ax64_gen_statement(f, def, def->stmts + s);
		}
		fprintf(f, "\n\n");
	}
}

const char *label_str(ssize_t id)
{
	static char str[32] = ".L";
	sprintf(str + 2, "%zd", id);
	return str;
}

void ax64_gen_statement(FILE *f, struct ir_definition *def, struct ir_statement *stmt)
{
	for (ssize_t i = 0; i < buf_len(def->labels); i++) {
		struct ir_statement *s = def->stmts + def->labels[i];
		if (s == stmt) {
			fprintf(f, "%s:\n", label_str(def->labels[i]));
		}
	}
	if (stmt->instr == IRINSTR_LOCAL) return;
	fprintf(f, "\t");
	switch (stmt->instr) {
	case IRINSTR_SET:
		fprintf(f, "mov ");
		ax64_gen_operand(f, def, stmt->ops + 1); // src
		fprintf(f, ", ");
		ax64_gen_operand(f, def, stmt->ops + 0); // dst
		break;
	case IRINSTR_RET:
		fprintf(f, "mov ");
		ax64_gen_operand(f, def, stmt->ops + 0);
		fprintf(f, ", ");
		fprintf(f, "%.*s", (int) ax64_retreg.len, ax64_retreg.name);
		fprintf(f, "\n\t");
		fprintf(f, "ret");
		break;
	case IRINSTR_LOCAL:
		assert(0);
		return;
	case IRINSTR_ADD:
		if (id_cmp(stmt->ops[0].oid, stmt->ops[1].oid) != 0) {
			fprintf(f, "mov ");
			ax64_gen_operand(f, def, stmt->ops + 1);
			fprintf(f, ", ");
			ax64_gen_operand(f, def, stmt->ops + 0);
			fprintf(f, "\n\t");
		}
		fprintf(f, "add ");
		ax64_gen_operand(f, def, stmt->ops + 2);
		fprintf(f, ", ");
		ax64_gen_operand(f, def, stmt->ops + 0);
		break;
	case IRINSTR_JMP:
		fprintf(f, "jmp ");
		ax64_gen_operand(f, def, stmt->ops + 0);
		break;
	case IRINSTR_CMP:
		fprintf(f, "cmp ");
		if (stmt->ops[0].kind == IR_HEX) {
			ax64_gen_operand(f, def, stmt->ops + 0);
			fprintf(f, ", ");
			ax64_gen_operand(f, def, stmt->ops + 1);
		} else {
			ax64_gen_operand(f, def, stmt->ops + 1);
			fprintf(f, ", ");
			ax64_gen_operand(f, def, stmt->ops + 0);
		}
		break;
	case IRINSTR_JZ:
		fprintf(f, "je ");
		ax64_gen_operand(f, def, stmt->ops + 0);
		break;
	case IRINSTR_JNZ:
		fprintf(f, "jne ");
		ax64_gen_operand(f, def, stmt->ops + 0);
		break;
	case IRINSTR_JL:
		fprintf(f, "jl");
		ax64_gen_operand(f, def, stmt->ops + 0);
		break;
	default:
		assert(0);
	}
	fprintf(f, "\n");
}

static const char *local2str(struct ir_definition *def, struct identifier *id)
{
	struct identifier *match = id_find(def->params, *id);
	if (match)
		return reg2str[match - def->params + AX64_ARG0];
	match = id_find(def->locals, *id);
	if (match)
		return reg2str[match - def->locals + buf_len(def->params)];
	fprintf(stderr, "tried finding id \"%.*s\"\n", (int) id->len, id->name);
	assert(0);
}

void ax64_gen_operand(FILE *f, struct ir_definition *def, struct ir_operand *op)
{
	switch (op->kind) {
	case IR_HEX:
		fprintf(f, "$0x%" PRIx64, op->oint);
		break;
	case IR_VAR:
		fprintf(f, "%s", local2str(def, &op->oid));
		break;
	case IR_LABEL:
		fputs(label_str(op->olbl), f);
		break;
	default:
		assert(0);
	}
}

