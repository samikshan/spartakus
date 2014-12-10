
/*
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

#include "symbol.h"

struct symb {
    char *sym_name;
    enum type sym_type;
    struct symb *next;
};

// Checks if a string starts with a particular string
int starts_with(const char *, const char *);

char *substring(const char *input, int offset, int len, char *dest);
void populate_exp_symlist(struct symbol_list *symlist);
void show_exp_sym_names();
int is_exported(struct symbol *sym);
char *exp_sym_name(struct symbol *sym);
const char *sym_type(struct symbol *sym);

void expand_and_add_symbol(struct symbol *sym);

struct symb *find_sym(struct symbol *sym);
struct symb *add_sym(struct symbol *sym);
void display_sym_table();
int is_table_empty();
void clear_sym_table();
long unsigned int get_crc (char *type, char *name, struct symbol_list *symlist, long unsigned int crc);
void process_files(struct string_list *filelist);
void process_symlist(struct symbol_list *symlist);
long unsigned int process_params(struct symbol_list *params, long unsigned int crc);
long unsigned int process_enum(struct symbol *sym, long unsigned int crc, int is_fn_param);
long unsigned int process_struct(struct symbol *sym, long unsigned int crc, int is_fn_param);
long unsigned int process_symbol(struct symbol *sym, long unsigned int crc, int is_fn_param);
