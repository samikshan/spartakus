
/* spartakus generates checksums for exported kernel symbols using sparse
 *
 * Copyright (C) 2014  Red Hat Inc.
 * Author: Samikshan Bairagya <sbairagy@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "lib.h"
#include "allocate.h"
#include "parse.h"
#include "symbol.h"
#include "expression.h"
#include "token.h"
#include "check_kabi.h"
#include "checksum.h"

static struct symb *symtab[HASH_BUCKETS];
static struct symbol_list *exported_symbols;

struct sym_using_typedef *tsym = NULL;
struct symbol *parsym = NULL; /* Parent sym for struct members */

static void clean_up_symbols(struct symbol_list *list)
{
    struct symbol *sym;

    FOR_EACH_PTR(list, sym)
    {
        expand_symbol(sym);
    } END_FOR_EACH_PTR(sym);
}

int starts_with(const char *a, const char *b)
{
    if(strncmp(a, b, strlen(b)) == 0)
    {
        return 1;
    }
    return 0;
}

char *substring(const char* input, int offset, int len, char* dest)
{
    int input_len = strlen (input);
    if(offset + len > input_len)
    {
        return NULL;
    }
    strncpy(dest, input + offset, len);
    return dest;
}

char *exp_sym_name(struct symbol *sym)
{
    char *symname;
    int offset = strlen("__ksymtab_");
    int len = strlen(sym->ident->name) - strlen("__ksymtab_");
    symname = malloc(len * sizeof(char));
    symname = substring(sym->ident->name, offset, len, symname);
    return symname;
}

void show_exp_sym_names()
{
    printf("\nList of exported symbols:\n");
    struct symbol *sym;
    FOR_EACH_PTR(exported_symbols, sym)
    {
        printf("%s\n", exp_sym_name(sym));
    }END_FOR_EACH_PTR(sym);
    printf("\n");
}

int is_exported(struct symbol *sym)
{
    //     printf("Symbol to check: %s\n", sym->ident->name);
    int res = 0;

    if (sym->ident == NULL)
        return res;

    struct symbol *expsym;
    FOR_EACH_PTR(exported_symbols, expsym)
    {
        // printf("Checking against %s\n", expsym->ident->name);
        if (strcmp(exp_sym_name(expsym), sym->ident->name) == 0)
        {
            res = 1;
            break;
        }
    }END_FOR_EACH_PTR(expsym);
    return res;
}

const char *sym_type(struct symbol *sym)
{
    enum type symtype = sym->type;
    switch(symtype) {
        case SYM_NODE:
        case SYM_ARRAY:
        case SYM_PTR:
        case SYM_FN:
        case SYM_BITFIELD:
            return sym_type(sym->ctype.base_type);
        case SYM_ENUM:
            return "enum";
        case SYM_STRUCT:
            return "struct";
        case SYM_UNION:
            return "union";
        case SYM_BASETYPE:
        case SYM_TYPEDEF:
            return show_typename(sym);
    }
}

struct symb *find_sym(struct symbol *sym)
{
    long unsigned int h = raw_crc32(sym->ident->name) % HASH_BUCKETS;

    if (!symtab[h]) {
        return NULL;
    }

    struct symb *s = (struct symb *) malloc(sizeof(struct symb));
    for (s = symtab[h]; s; s = s->next) {
        if (strcmp(s->sym_name, sym->ident->name) == 0 &&
            s->sym_type == sym->type) {
            break;
        }
    }
    return s;
}

struct symb *add_sym(struct symbol *sym)
{
//     printf("Adding symbol %s with checksum 0x%08lx\n", sym.ident->name, crc);
    long unsigned int h = raw_crc32(sym->ident->name) % HASH_BUCKETS;

    struct symb *s = find_sym(sym);
    // If symbol is already in symbol table then return control
    if (s != NULL) {
        //printf("Symbol %s already processed...\n", s->sym_name);
        return s;
    } else {
        if (!symtab[h]) {
            // Table empty for selected hash
//             printf("Table empty for selected hash\n");
            struct symb *added_sym;
            added_sym = (struct symb *) malloc(sizeof(struct symb));
            added_sym->sym_name = sym->ident->name;
            added_sym->sym_type = sym->type;
            added_sym->next = symtab[h];
            symtab[h] = added_sym;
            return added_sym;
        } else {
            for (s = symtab[h]; s->next; s = s->next);
            struct symb *added_sym;
            added_sym = (struct symb *) malloc(sizeof(struct symb));
            added_sym->sym_name = sym->ident->name;
            added_sym->sym_type = sym->type;
            added_sym->next = s->next;
            s->next = added_sym;
            return added_sym;
        }
    }
}

void display_sym_table()
{
    printf("\nHash table storing symbols as an array of linked list:\n");
    int i;
    struct symb *sym;
    for (i=0; i < HASH_BUCKETS; i++) {
        sym = symtab[i];
        if (sym == 0) continue;
        else {
            while (sym) {
                printf("%s --> ", sym->sym_name);
                if (sym->next) {
                    sym = sym->next;
                }
                else
                    break;
            }
            printf("\n");
//             printf("%s\n", sym->symname);
//             continue;
        }
    }
}

int is_table_empty()
{
    int h, result = 1;
    for (h=0; h<HASH_BUCKETS; h++) {
        if (symtab[h] == 0)
            continue;
        else {
            result = 0;
            break;
        }
    }
    return result;
}

void clear_sym_table()
{
    struct symb *cur, *next;
    int h;

    if (is_table_empty())
        return;

    for (h=0; h<HASH_BUCKETS; h++) {
        cur = symtab[h];
        if (cur == NULL) continue;

        while (cur) {
            if (cur->next) {
                next = cur->next;
                cur->next = NULL;
                free(cur);
                symtab[h] = next;
                cur = symtab[h];
            } else {
                free(cur);
                symtab[h] = NULL;
                break;
            }
        }
    }
}

long unsigned int process_enum(struct symbol *sym, long unsigned int crc, int is_fn_param)
{
    struct symbol_list *enum_mems = sym->symbol_list;
    struct symbol *enum_mem;
    crc = crc32("{", crc);

    FOR_EACH_PTR(enum_mems, enum_mem) {
        crc = crc32(enum_mem->ident->name, crc);
        crc = crc32(",", crc);
    }END_FOR_EACH_PTR(enum_mem);

    crc = crc32("}", crc);
    return crc;
}

long unsigned int process_struct(struct symbol *sym, long unsigned int crc, int is_fn_param)
{
    int member_count = 0;
    struct symbol_list *members = sym->symbol_list;
    struct symbol *member;

    crc = crc32("{", crc);
    FOR_EACH_PTR(members, member) {
        member_count = member_count + 1;
        parsym = sym;
        crc = process_symbol(member, crc, is_fn_param);
        crc = crc32(";", crc);
    }END_FOR_EACH_PTR(member);
    if (member_count == 0) {
        crc = crc32("UNKNOWN", crc);
    }
    crc = crc32("}", crc);

    return crc;
}

long unsigned int process_params(struct symbol_list *params, long unsigned int crc)
{
    struct symbol *param;
    int counter = 0;
    FOR_EACH_PTR(params, param)
    {
        if (counter > 0) {
            crc = crc32(",", crc);
        }
        counter += 1;
        crc = process_symbol(param, crc, 1);
    }END_FOR_EACH_PTR(param);

    if (counter == 0) crc = crc32("void", crc); // No parameters

    return crc;
}

long unsigned int process_pointer(struct symbol *sym, long unsigned int crc, int is_fn_param)
{
    struct symbol *subsym = sym->ctype.base_type;
    enum type subsymtype = sym->ctype.base_type->type;

    if (subsymtype == SYM_FN) {
        crc = process_symbol(subsym->ctype.base_type, crc, 0);
        crc = crc32("(", crc);
        crc = crc32("*", crc);
        if (sym->ident)
            crc = crc32(sym->ident->name, crc);
        crc = crc32(")", crc);
        crc = crc32("(", crc);
        crc = process_params(sym->ctype.base_type->arguments, crc);
        crc = crc32(")", crc);
    } else {
        switch(subsymtype) {
            case SYM_PTR:
                crc = process_pointer(subsym, crc, 0);
                break;
            case SYM_UNION:
            case SYM_STRUCT:
                crc = process_symbol(subsym, crc, 0);
                break;
            case SYM_ENUM:
                crc = process_symbol(subsym, crc, 0);
                break;
            case SYM_BASETYPE:
            default:
                crc = crc32(sym_type(sym), crc);
        }
        crc = crc32("*", crc);
    }
    return crc;
}

long unsigned int process_array(struct symbol *sym, long unsigned int crc, int is_fn_param)
{
    char array_size[256];
    struct symbol *subsym = sym->ctype.base_type;
    enum type subsymtype = sym->ctype.base_type->type;

    sprintf(array_size, "%lld", get_expression_value_silent(sym->array_size));
    switch(subsymtype) {
        case SYM_PTR:
            subsym->ident = sym->ident;
            crc = process_symbol(subsym, crc, is_fn_param);
            if (sym->ident != NULL)
                crc = crc32(sym->ident->name, crc);
            break;
        case SYM_UNION:
        case SYM_STRUCT:
            crc = process_symbol(subsym, crc, is_fn_param);
            break;
        case SYM_BASETYPE:
            crc = crc32(sym_type(subsym), crc);
            if (sym->ident != NULL)
                crc = crc32(sym->ident->name, crc);
            break;
    }
    crc = crc32("[", crc);
    if (strcmp(array_size, "0") != 0)
        crc = crc32(array_size, crc);
    crc = crc32("]", crc);
    return crc;
}

long unsigned int process_typedef(struct typedef_sym *symtype, long unsigned int crc)
{
    printf("Processing typedef %s\n", symtype->name);
    struct decl_list *defn = symtype->defn;
    while (defn) {
        if (defn->str) {
            struct typedef_sym *tsym = find_typedef_sym_by_name(defn->str);
            if (tsym) {
                if (strcmp(symtype->name, tsym->name) == 0)
                    crc = crc32(defn->str, crc);
                else
                    crc = process_typedef(tsym, crc);
            } else {
                crc = crc32(defn->str, crc);
            }
        }
        defn = defn->next;
    }
    return crc;
}

long unsigned int process_symbol_using_typedef(struct symbol *sym, long unsigned int crc)
{
    printf("Symbol %s uses typedef defined type %s\n", sym->ident->name, tsym->type->name);
    struct typedef_sym *symtype = tsym->type;
    crc = process_typedef(symtype, crc);
    if (sym->ctype.base_type->type == SYM_PTR)
        crc = crc32("*", crc);
    crc = crc32(sym->ident->name, crc);
    return crc;
}

long unsigned int process_symbol(struct symbol *sym, long unsigned int crc, int is_fn_param)
{
    if (sym->ident) {
        tsym = find_sym_using_typedef(sym->ident->name, parsym);
        if (tsym != NULL) { /* Symbol's type is defined by a typedef */
            crc = process_symbol_using_typedef(sym, crc);
            parsym = NULL;
            return crc;
        }
    }

    if (sym->type == SYM_NODE) {
        if (sym->ctype.base_type->type == SYM_PTR &&
            sym->ctype.base_type->ctype.base_type->type == SYM_FN) {
            if (sym->ident != NULL)
                sym->ctype.base_type->ident = sym->ident;
            crc = process_symbol(sym->ctype.base_type, crc, is_fn_param);
        } else if (sym->ctype.base_type->type == SYM_ARRAY) {
            if (sym->ident != NULL)
                sym->ctype.base_type->ident = sym->ident;
            crc = process_symbol(sym->ctype.base_type, crc, is_fn_param);
        } else {
            if (sym->ctype.base_type->type == SYM_FN) {
                sym->ctype.base_type->ident = sym->ident;
                crc = process_symbol(sym->ctype.base_type, crc, is_fn_param);
            } else {
                crc = process_symbol(sym->ctype.base_type, crc, is_fn_param);
                if (sym->ident != NULL && is_fn_param == 0)
                    crc = crc32(sym->ident->name, crc);
            }
        }

        return crc;
    }

    enum type symtype;

//     if (sym->ident != NULL)
        //printf("Symbol: %s | Type: %s\n", sym->ident->name, sym_type(sym));
    symtype = sym->type;

    if (symtype != SYM_PTR && symtype != SYM_ARRAY && symtype != SYM_FN) {
        if (sym->ident != NULL) {
            struct symb *s = find_sym(sym);

            if (s != NULL) {
                //printf("Symbol %s found in symbol table\n", s->sym_name);
                crc = crc32(sym_type(sym), crc);
                if (is_fn_param == 0 && sym->ident != NULL)
                    crc = crc32(sym->ident->name, crc);
                return crc;
            } else {
                struct symb *added_sym = add_sym(sym);
            }
        }
        crc = crc32(sym_type(sym), crc);
    }

    switch (symtype) {
        struct symbol_list *params;
        char bitfield_width[15];
        case SYM_PTR:
            crc = process_pointer(sym, crc, is_fn_param);
            break;
        case SYM_ARRAY:
            crc = process_array(sym, crc, is_fn_param);
            break;
        case SYM_FN:
            crc = process_symbol(sym->ctype.base_type, crc, 0);
            crc = crc32(sym->ident->name, crc);
            params = sym->arguments;
            crc = crc32("(", crc);
            crc = process_params(params, crc);
            crc = crc32(")", crc);
            break;
        case SYM_UNION:
            /* Do nothing. Unions and structs will have same implementations */
        case SYM_STRUCT:
            if (sym->ident != NULL)
                crc = crc32(sym->ident->name, crc);
            crc = process_struct(sym, crc, is_fn_param);
            break;
        case SYM_ENUM:
            if (sym->ident != NULL)
                crc = crc32(sym->ident->name, crc);
            crc = process_enum(sym, crc, is_fn_param);
            break;
        case SYM_BITFIELD:
            if (sym->ident != NULL)
                crc = crc32(sym->ident->name, crc);
            crc = crc32(":", crc);
            sprintf(bitfield_width, "%d", sym->bit_size);
            crc = crc32(bitfield_width, crc);
            break;
        case SYM_BASETYPE:
            if (is_fn_param == 0) {
                if (sym->ident != NULL)
                    crc = crc32(sym->ident->name, crc);
            }
        default:
            ;
    }

    return crc;
}

void process_symlist(struct symbol_list *symlist)
{
    struct symbol *sym;

    FOR_EACH_PTR(symlist, sym)
    {
        if(is_exported(sym) == 1) // If sym is in list of exported symbols
        {
            long unsigned int crc = process_symbol(sym,
                                                   0xffffffff, 0) ^ 0xffffffff;
            printf("__crc_%s = 0x%08lx ;\n", sym->ident->name, crc);
            clear_sym_table();
        }
    }END_FOR_EACH_PTR(sym);
    //display_sym_table();
}

void populate_exp_symlist(struct symbol_list *symlist)
{
    struct symbol *sym;
    FOR_EACH_PTR(symlist, sym)
    {
        // Symbol name beginning with __ksymtab_ is an exported symbol
        if (sym->ident == NULL)
            return;
        if (starts_with(sym->ident->name, "__ksymtab_"))
        {
            //printf("Adding %s to list of exported symbols\n", sym->ident->name);
            add_symbol(&exported_symbols, sym);
        }
    }END_FOR_EACH_PTR(sym);
    //show_exp_sym_names(); // show list of exported symbols
}

void process_files(struct string_list* filelist)
{
    char *file;
    struct symbol_list *symlist;

    FOR_EACH_PTR_NOTAG(filelist, file)
    {
        symlist = sparse(file);
//         display_typedef_symtab();
        clean_up_symbols(symlist);
        populate_exp_symlist(symlist);
        process_symlist(symlist);
        //display_sym_table();
        clear_typedef_symtab();
    }END_FOR_EACH_PTR_NOTAG(file);
}

int main(int argc, char **argv)
{
    struct symbol_list *symlist = NULL;
    struct string_list *filelist = NULL;

    symlist = sparse_initialize(argc, argv, &filelist);
    clean_up_symbols(symlist);
    clear_typedef_symtab();
    process_files(filelist);

    return 0;
}
