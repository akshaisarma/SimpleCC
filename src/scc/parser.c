#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokens.h"
#include "globals.h"
#include "opcodes.h"

//Types
#define NOTTYPE		0
#define INTTYPE		1
#define REALTYPE	2
typedef int Type;

//Global vars
double fltlit;
int intlit;
int level = -1;
char* lvl_str = NULL;
int lvl_str_len = 10;
struct label_info* function_labels = NULL;
int gotocount = 0;
int mainhole;
int mainip = -1;

//Structs
struct listnode {
	char* name;
	struct listnode* next;
};

struct gotorecord {
	int hole_ip;
	int curcol;
	int curline;
	char* goto_block_info;
	struct gotorecord* next;
};

struct label_info {
	char* name;
	int addr;
	char seen;
	struct gotorecord* list_of_gotos;
	char* block_info;
	int curcol;
	int curline;
	struct label_info* next;
};

//Functions
void match(int tokentype);
Type gen(int op, Type t1, Type t2);
struct symtabrecord* lookupidentifier(char* wrd);
struct label_info* lookuplabel (char* wrd);
void emit_addr(int addr);
void finish();
void free_labels();

//LL1 Parsing: Grammar to Functions. Recursive Descent
//Compiler top
void start();
void multiple_procedure_decl();
void procedure_decl();
void procedure_body();
void global_statements();
void statements();
void complex_statement();
void simple_statement();
void while_statement();
void for_statement();
void do_while_statement();
void if_statement();
void switch_statement();
void goto_statement();
void print_statement();
void empty_statement();
void declaration();
void lab_declaration();
void lab_statement(struct label_info* srchres);
void proc_statement(struct symtabrecord* srchres);
void A(struct symtabrecord* srchres);
Type L();
Type E();
Type T();
Type F();

void compile()
{	
	/*
	//Test Scanner
	do {
		gettoken();
		printtoken();		
	} while ((fstack->record->curtoken != TK_EOF) || (fstack = fstack->next));
	*/
	
	if (setjmp(a)) {
		free_labels();
		if (lvl_str)
			free (lvl_str);
		compile_error = 1;
		return;
	}
	
	//Initialize cs, ds, ss
	dp = 0;
	ip = 0;
	stp = 0;
	cssize = 1024;
	dssize = 1024;
	sssize = 1024;
	cs = (char *)malloc(cssize*sizeof(char));
	ds = (char *)malloc(dssize*sizeof(char));
	ss = (char *)malloc(sssize*sizeof(char));
	//Start scanning and parsing
	gettoken();
	start();
}

void match(int tokentype)
{
	if (fstack->record->curtoken != tokentype) {
		printf ("Error: Found unexpected token (%d) while trying to match another token (%d) (lookup in tokens.h what they are)\n" 
				"in %s at Line: %d and Column %d. Aborting...\n", fstack->record->curtoken, tokentype, 
				fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
		longjmp(a, 1);
	}
	else
		gettoken();
}

struct symtabrecord* lookupidentifier(char* wrd)
{
	int i = hashvalue(wrd, 256);
	struct symtabstack* topsymtab = sstack;
	while (topsymtab) {
		struct symtabrecord* lctn = topsymtab->symtab[i];
		while (lctn) {
			if (strcmp(wrd, lctn->name) == 0) 
				return lctn;
			lctn = lctn->next;
		}
		topsymtab = topsymtab->nextsymtab;
	}
	return NULL;
}

struct label_info* lookuplabel(char* wrd)
{
	//Not hashing. Can get slow if immense number of labels in a function.
	struct label_info* t = function_labels;
	while (t) {
		if (strcmp(wrd, t->name) == 0)
			break;
		t = t->next;
	}
	return t;
}

void emit_addr(int addr) {
	*(int *) (cs + ip) = addr;
	ip += sizeof(int);
}

void finish()
{
	match (TK_EOF);
	if (mainip < 0) {
		printf("Error: Reached EOF and did not see main function. Aborting...\n");
		longjmp(a, 1);
	}
	else {
		int save = ip;
		ip = mainhole;
		emit_addr(mainip - ip);
		ip = save;
	}
	//Fix holes for function calls. We should only have one symtab (global) here. Can but don't want to clear symtab here...
	int i;
	for (i = 0; i < 256; i++) {
		struct symtabrecord* t = sstack->symtab[i];
		while (t) {
			if (t->tokentype == TK_FUNC) {
				if (t->funcdet->body_seen) {
					int addr = t->addr;
					int p;
					int save = ip;
					for (p = t->funcdet->numcalls - 1; p >=0; p--) {
						ip = t->funcdet->call_list[p];
						emit_addr(addr - ip);
					}
					ip = save;
				}
				else {
					if (t->funcdet->call_list) {
						printf("Error: Function call made to function %s without a body. Aborting...\n",t->name);
						longjmp(a, 1);
					}
					printf("Warning: Prototype declared for function %s without a body.\n", t->name);
				}
			}
			t = t->next;
		}
	}
	if (dp >= dssize) {
		dssize++; 
		char* newds = (char *)realloc(ds, (dssize+1)*sizeof(char));
		if (newds == NULL) {
			printf("Error: No more memory to allocate ds in %s at Line: %d and Column %d." 
				   " Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		ds = newds;	
	}
	if (ip >= cssize) {
		cssize++; 
		char* newcs = (char *)realloc(cs, (cssize+1)*sizeof(char));
		if (newcs == NULL) {
			printf("Error: No more memory to allocate cs in %s at Line: %d and Column %d." 
				   " Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);

			longjmp(a, 1);
		}
		cs = newcs;	
	}
	cs[ip++] = '\0';
	ds[dp++] = '\0';
		
	FILE* fileinp = fopen("output.o", "w");
	if (fileinp == NULL) {
		printf ("\nFile output.o could not be opened for writing output.\n\n");
		longjmp(a, 1);
	}
	fseek(fileinp, 0, SEEK_SET);
	fwrite(&ip, sizeof(int), 1, fileinp);
	fwrite(&dp, sizeof(int), 1, fileinp);
	fwrite(&stp, sizeof(int), 1, fileinp);
	fwrite(cs, sizeof(char), ip, fileinp);
	fwrite(ds, sizeof(char), dp, fileinp);
	fwrite(ss, sizeof(char), stp, fileinp);
	fclose(fileinp);
}

void free_labels()
{
	if (function_labels) {
		while (function_labels) {
			struct label_info* tmp = function_labels->next;
			free (function_labels->name);
			while (function_labels->list_of_gotos) {
				struct gotorecord* tmp =function_labels->list_of_gotos->next;
				free (function_labels->list_of_gotos->goto_block_info);
				free (function_labels->list_of_gotos);
				function_labels->list_of_gotos = tmp;
			}
			free(function_labels->block_info);
			free (function_labels);
			function_labels = tmp;
		}
		function_labels = NULL;
	}
}

void start()
{
	int t = fstack->record->curtoken;
	if (t == TK_INT || t == TK_DOUBLE || t == TK_CHAR || t == TK_FLOAT || t == TK_LONG || t == TK_UNSIGNED) {
		global_statements();
		multiple_procedure_decl();
	}
	else if (t == TK_VOID) 
		multiple_procedure_decl();
	else if (t == TK_EOF) 
		finish();
	else {
		printf("Error: Found invalid token outside all procedures while starting to parse\n"
			   "(all procedures must begin with void) in %s at Line: %d and Column %d. Aborting...\n", 
			   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
		longjmp(a, 1);
	}
}

void multiple_procedure_decl()
{
	gen(OP_JMP, NOTTYPE, NOTTYPE);
	mainhole = ip;		
	emit_addr(0);	
	while (1) {
		while (fstack->record->curtoken == TK_VOID)
			procedure_decl();
		if (fstack->record->curtoken == TK_EOF) {
			if (fstack->next == NULL)
				break;
			else {
				struct filestack* t = fstack->next;;
				if (fstack->record->curname)
					free (fstack->record->curname);
				if (fstack->record->strliteral)
					free (fstack->record->strliteral);
				if (fstack->record->curfile)
					free (fstack->record->curfile);
				if (fstack->record->filecontents)
					free (fstack->record->filecontents);
				free (fstack->record);
				free (fstack);
				fstack = t;			
				gettoken();
			}
		}	
		else {
			printf("Error: Found invalid token outside all procedures (if it's a global, declare all globals before\n"
				   "starting to declare procedures) in %s at Line: %d and Column %d. Aborting...\n", 
				   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
	}
	finish();
}

void procedure_decl()
{
	match(TK_VOID);
	if (fstack->record->curtoken != TK_ID) {
		printf("Error: Expecting identifier in %s at Line: %d and Column %d. Aborting...\n", 
			   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
		longjmp(a, 1);
	}
	char* name = (char *)malloc((strlen(fstack->record->curname)+1)*sizeof(char));
	strcpy (name, fstack->record->curname);
	free (fstack->record->curname);
	fstack->record->curname = NULL;
	match(TK_ID);
	match(TK_LEFTPARAN);
	if (fstack->record->curtoken == TK_ID) {
		printf("Error: Only procedures (no params allowed) Use global variables to pass and return values to \n and from procedures\n" 
			   "and do not redeclare globals inside the function body. Error is in %s at Line: %d and Column %d. Aborting...\n", 
			   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
		longjmp(a, 1);
	}
	match(TK_RIGHTPARAN);
	int i = hashvalue(name, 256);
	struct symtabrecord* lkp = sstack->symtab[i];
	if (fstack->record->curtoken == TK_LEFTCBRACE) {
		struct symtabrecord* loc;
		while (lkp) {
			if (!strcmp(lkp->name, name)) {
				loc = lkp;
				break;
			}
			lkp = lkp->next;
		}
		if (!strcmp(name, "main")) {
			if (lkp != NULL) {
				printf("Error: main redeclared in %s at Line: %d and Column: %d. Aborting...\n",
					   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
				free(name);
				longjmp(a, 1);
			}
			struct symtabrecord* loc = (struct symtabrecord*)malloc(sizeof(struct symtabrecord));
			struct func_details* fd = (struct func_details*)malloc(sizeof(struct func_details));
			fd->body_seen = 1;
			fd->call_list = NULL;
			fd->numcalls = 0;
			loc->funcdet = fd;
			loc->name = name;
			loc->tokentype = TK_FUNC;
			loc->addr = mainip = ip;
			loc->next = NULL;
			if (sstack->symtab[i]){
				loc->next = sstack->symtab[i];
				sstack->symtab[i] = loc;
			}
			else
				sstack->symtab[i] = loc;
			procedure_body();
			gen(OP_HALT, NOTTYPE, NOTTYPE);
			return;
		}
		if (lkp == NULL) {
			struct symtabrecord* loc = (struct symtabrecord*)malloc(sizeof(struct symtabrecord));
			struct func_details* fd = (struct func_details*)malloc(sizeof(struct func_details));
			fd->body_seen = 1;
			fd->call_list = NULL;
			fd->numcalls = 0;
			loc->funcdet = fd;
			loc->name = name;
			loc->tokentype = TK_FUNC;
			loc->addr = ip;
			loc->next = NULL;
			if (sstack->symtab[i]){
				loc->next = sstack->symtab[i];
				sstack->symtab[i] = loc;
			}
			else
				sstack->symtab[i] = loc;
			procedure_body();
			gen(OP_RET, NOTTYPE, NOTTYPE);
		}
		else if (loc->tokentype != TK_FUNC) {
			printf("Error: This identifier name has already been declared as a variable.\n"
				   "Error is in %s at Line: %d and Column %d. Aborting...\n", 
				   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			free(name);
			longjmp(a, 1);
		}
		else if (loc->funcdet->body_seen) {
			printf("Error: This function name has already been defined before.\n"
				   "Error is in %s at Line: %d and Column %d. Aborting...\n", 
				   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			free(name);
			longjmp(a, 1);
		}
		else {
			loc->addr = ip;
			loc->funcdet->body_seen = 1;
			procedure_body();
			gen(OP_RET, NOTTYPE, NOTTYPE);
		}
	}
	else {
		if (!strcmp(name, "main")) {
			printf("Error: main cannot be a prototype. Error is in %s at Line: %d and Column %d. Aborting...\n",
				   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			free(name);
			longjmp(a, 1);
		}
		while (lkp) {
			if (!strcmp(lkp->name, name)) {
				printf("Error: The name: %s has already been declared as a variable or a function. The error is in"
					   "%s at Line: %d and Column %d. Aborting...\n", name,
					   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
				longjmp(a, 1);
			}
			lkp = lkp->next;
		}
		match(TK_SEMICOLON);
		struct symtabrecord* loc = (struct symtabrecord*)malloc(sizeof(struct symtabrecord));
		struct func_details* fd = (struct func_details*)malloc(sizeof(struct func_details));
		fd->body_seen = 0;
		fd->call_list = NULL;
		fd->numcalls = 0;
		loc->funcdet = fd;
		loc->name = name;
		loc->tokentype = TK_FUNC;
		loc->next = NULL;
		if (sstack->symtab[i]){
			loc->next = sstack->symtab[i];
			sstack->symtab[i] = loc;
		}
		else
			sstack->symtab[i] = loc;
	}
}	

void global_statements()
{
	int t = fstack->record->curtoken;	
	while (1) {
		if (t == TK_INT || t == TK_DOUBLE || t == TK_CHAR || t == TK_FLOAT ||t == TK_LONG || t == TK_UNSIGNED || t == TK_ID) {
			while (t == TK_INT || t == TK_DOUBLE || t == TK_CHAR || t == TK_FLOAT || t == TK_LONG || t == TK_UNSIGNED) {
				declaration();
				t = fstack->record->curtoken;
			}
			while (t == TK_ID) {
				struct symtabrecord* srchres = lookupidentifier(fstack->record->curname);
				if (srchres == NULL) {			
					printf ("Error: Found unknown identifier: %s in %s at Line: %d and Column %d." 
							" Aborting...\n", fstack->record->curname, fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
					longjmp(a, 1);
				}
				int tt = srchres->tokentype;
				if (tt == TK_VAR || tt == TK_ARR) {
					A(srchres);
					match(TK_SEMICOLON);
				}
				else {
					printf ("Error: Cannot assign to var: %s of unknown type in %s at Line: %d and Column %d." 
							" Aborting...\n", fstack->record->curname, fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
					longjmp(a, 1);
				}
				t = fstack->record->curtoken;
			}
		}
		else 
			break;
	}
}

void procedure_body()
{
	//Set up level string for block information
	lvl_str = (char *) malloc ((lvl_str_len+1)*sizeof(char));
	strcpy(lvl_str, "1111111111");
	lvl_str[lvl_str_len] = '\0';
	
	complex_statement();
	
	//fix gotos
	if (function_labels) {
		struct label_info* tmp = function_labels;
		int save = ip;
		while (tmp) {
			if (!tmp->seen && tmp->list_of_gotos) {
				printf ("Error: Found unseen label: %s in %s at Line: %d and Column: %d referenced to by atleast\n" 
						"one goto, such as in Line: %d and Column: %d Aborting...\n", tmp->name, fstack->record->curfile, tmp->curline,
						tmp->curcol, tmp->list_of_gotos->curline, tmp->list_of_gotos->curcol);
				longjmp(a, 1);
			}
			int addr = tmp->addr;
			char* lbl_blck = tmp->block_info;
			struct gotorecord* t = tmp->list_of_gotos;
			while (t) {
				char* goto_blck = tmp->list_of_gotos->goto_block_info;
				// If the target is longer than the source
				if ( strlen(goto_blck) < strlen(lbl_blck ) ) {
					printf("Error: Invalid goto jump: Jumping into deeper block. Block violation in %s in Line: %d and Column: %d. Aborting...\n"
						   , fstack->record->curfile, t->curline, t->curcol);
					longjmp (a, 1);
				}
				int i;	
				for (i = 0; i < strlen(lbl_blck); i++) 
					if (goto_blck[i] != lbl_blck[i]) {
						printf("Error: Invalid goto jump. Jumping into lower but nested block.  Block violation in %s in Line: %d and Column: %d. Aborting...\n"
							   , fstack->record->curfile, t->curline, t->curcol);
						longjmp (a, 1);
					}
				ip = t->hole_ip;
				emit_addr(addr - ip);
				t = t->next;
			}
			tmp = tmp->next;
		}
		ip = save;
		free_labels();
	}
	
	free(lvl_str);
	lvl_str = NULL;
	lvl_str_len = 10;
	level = -1;
}

void statements()
{
	int t = fstack->record->curtoken;
	while (t == TK_LEFTCBRACE || t == TK_IF || t == TK_WHILE || t == TK_FOR || t == TK_DO || t == TK_SWITCH ||
		   t == TK_GOTO || t == TK_EOF || t== TK_PRINT || t == TK_SEMICOLON || t == TK_INT || t == TK_DOUBLE || 
		   t == TK_CHAR || t == TK_FLOAT || t == TK_LONG || t == TK_UNSIGNED || t == TK_ID) {
		simple_statement();
		t = fstack->record->curtoken;
	}
}

void complex_statement()
{
	int t = fstack->record->curtoken;
	int i;
	if (t == TK_LEFTCBRACE) {
		match (TK_LEFTCBRACE);
		// Up level
		level++;
		if (level >= lvl_str_len-1) {
			int s = lvl_str_len;
			lvl_str_len = level+1;
			char* tmp = (char *) realloc(lvl_str, (lvl_str_len+1)*sizeof(char));
			if (!tmp) {
				printf("Error: Ran out of memory while allocating level string...");
				longjmp(a, 1);
			}
			lvl_str = tmp;
			for (;s < lvl_str_len; s++)
				lvl_str[s] = '1';
			lvl_str[lvl_str_len] = '\0';
		}		
		
		//Set up new symtab on stack
		struct symtabstack* tmp = (struct symtabstack*) malloc(sizeof(struct symtabstack));
		for (i = 0; i < 256; i++) 
			tmp->symtab[i] = NULL;
	
		
		if (tmp == NULL) {
			printf("Error: No memory to allocate symbol table in %s at Line: %d and Column %d. Aborting...\n", 
				   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		tmp->nextsymtab = sstack;
		sstack = tmp;
		statements();
		match (TK_RIGHTCBRACE);
		
		// Down level and update level string
		lvl_str[level--]++;
		
		struct symtabstack* t = sstack;
		sstack = sstack->nextsymtab;
		for (i = 0; i < 256; i++) {
			while (t->symtab[i]) {
				if (t->symtab[i]->name)
					free (t->symtab[i]->name);
				struct symtabrecord* tmp = t->symtab[i]->next;
				free (t->symtab[i]);
				t->symtab[i] = tmp;
			}
		}
		free (t);
	}
	else {
		//It may not be '{' only if complex_statement was called automatically without checking token
		//for the beginning of a function. In this case, return an error.
		printf("Error: Expecting block statement in %s at Line: %d and Column %d. Aborting...\n", 
			   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
		longjmp(a, 1);
	}
}

void simple_statement()
{
	int t = fstack->record->curtoken;
	if (t == TK_LEFTCBRACE) 
		complex_statement();
	else if (t == TK_IF) 
		if_statement();
	else if (t == TK_WHILE) 
		while_statement();
	else if (t == TK_FOR) 
		for_statement();
	else if (t == TK_DO) 
		do_while_statement();
	else if (t == TK_SWITCH) 
		switch_statement();
	else if (t == TK_GOTO) 
		goto_statement();
	else if (t == TK_PRINT)
		print_statement();
	else if (t == TK_SEMICOLON)
		empty_statement();
	else if (t == TK_INT || t == TK_DOUBLE || t == TK_CHAR || t == TK_FLOAT || t == TK_LONG || t == TK_UNSIGNED) 
		declaration();
	else if (t == TK_ID) {
		char* curname = fstack->record->curname;
		// Check for label 
		char* peek = fstack->record->scanp;
		if (*peek) {
			if (*peek == ':') {
				struct label_info* srchres = lookuplabel(curname);
				if (srchres) {
					if (srchres->seen) {				
						printf ("Error: Found redeclared label: %s in %s at Line: %d and Column %d." 
								" Aborting...\n", curname, fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
						longjmp(a, 1);
					}
					lab_statement(srchres);
				}
				else
					lab_declaration();
				match (TK_ID);
				match(TK_COLON);
				return;
			}
		}
		struct symtabrecord* srchres = lookupidentifier(curname);
		if (srchres == NULL) {			
			printf ("Error: Found unknown identifier: %s in %s at Line: %d and Column %d." 
					" Aborting...\n", curname, fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		int tt = srchres->tokentype;
		if (tt == TK_VAR || tt == TK_ARR) {
			A(srchres);
			match(TK_SEMICOLON);
		}
		else if (tt == TK_FUNC)
			proc_statement(srchres);
		else {
			printf ("Error: Unknown variable type for identifier: %s in %s at" 
					" Line: %d and Column %d. Aborting...\n", srchres->name, 
					fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
	}
	else {
		printf ("Error: EOF detected before finished parsing in %s at Line: %d and Column %d. Aborting...\n", 
				fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
		longjmp(a, 1);
	}
}

void while_statement()
{
	match(TK_WHILE);
	match(TK_LEFTPARAN);
	int target = ip;
	L();
	match(TK_RIGHTPARAN);
	gen(OP_JFA, NOTTYPE, NOTTYPE);
	int hole = ip;
	emit_addr(0);
	if (fstack->record->curtoken == TK_LEFTCBRACE)
		complex_statement();
	else 
		simple_statement();	gen(OP_JMP, NOTTYPE, NOTTYPE);
	emit_addr(target - ip);
	int save = ip; 
	ip = hole; emit_addr(save - ip); ip = save;
}

void for_statement()
{
	int cond = 0; //Conditional exists
	int modif = 0;//Modifier exists
	int target, target2, hole, hole2, hole3;
	match(TK_FOR);
	match(TK_LEFTPARAN);
	if (fstack->record->curtoken == TK_ID) {
		struct symtabrecord* srchres = lookupidentifier(fstack->record->curname);
		A(srchres);
		while (fstack->record->curtoken == TK_COMMA){
			match(TK_COMMA);
			struct symtabrecord* srchres = lookupidentifier(fstack->record->curname);
			A(srchres);
		}
	}
	match (TK_SEMICOLON);
	target = ip; 
	if (fstack->record->curtoken == TK_ID) {
		cond = 1;
		L();
		gen(OP_JFA, NOTTYPE, NOTTYPE);
		hole = ip;
		emit_addr(0);
	}
	match(TK_SEMICOLON);
	if (fstack->record->curtoken == TK_ID) {
		modif = 1;
		gen(OP_JMP, NOTTYPE, NOTTYPE);
		hole2 = ip;
		emit_addr(0);
		target2 = ip;
		struct symtabrecord* srchres = lookupidentifier(fstack->record->curname);
		A(srchres);
		while (fstack->record->curtoken == TK_COMMA){
			match(TK_COMMA);
			struct symtabrecord* srchres = lookupidentifier(fstack->record->curname);
			A(srchres);
		}
		gen(OP_JMP, NOTTYPE, NOTTYPE);
		hole3 = ip;
		emit_addr(0);
	}
	match(TK_RIGHTPARAN);
	if (modif) {
		int save = ip; ip = hole2; emit_addr(save - ip);
		ip = save;
	}
	if (fstack->record->curtoken == TK_LEFTCBRACE)
		complex_statement();
	else 
		simple_statement();
	if (modif) {
		gen (OP_JMP, NOTTYPE, NOTTYPE);
		emit_addr(target2 - ip);
		int save = ip; ip = hole3; emit_addr(save - ip);
		ip = save;
	}
	gen(OP_JMP, NOTTYPE, NOTTYPE);
	emit_addr(target - ip);
	if (cond) { 
		int save = ip; ip = hole; emit_addr(save - ip);
		ip = save;
	}
	else {
		printf("Warning: Conditional does not exist for the FOR loop in File: %s, Line: %d, Column: %d. \n"
			   "The compiler does not support break or continue statements. Possibility of an infinite loop. \n"
			   "Use GOTOs to break out or else use at own risk. \n\n" , fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
	}

}

void do_while_statement()
{
	match(TK_DO);
	int target = ip;
	if (fstack->record->curtoken == TK_LEFTCBRACE)
		complex_statement();
	else 
		simple_statement();	match(TK_WHILE);
	match (TK_LEFTPARAN);
	L();
	match(TK_RIGHTPARAN);
	match(TK_SEMICOLON);
	gen(OP_JTR, NOTTYPE, NOTTYPE);
	emit_addr(target - ip);
}

void if_statement() 
{
	match (TK_IF);
	match (TK_LEFTPARAN);
	L();
	//No type checking for L() in C. Won't ever get NOTTYPE here since when gen returns NOTTYPE, it is for PUSH, TK_EQUAL etc
	//For PUSH etc, the caller returns the right type (F()) and you shouldn't use assignment in if as it is a statement.
	match (TK_RIGHTPARAN);
	gen (OP_JFA, NOTTYPE, NOTTYPE);
	int hole = ip;
	emit_addr(0);
	if (fstack->record->curtoken == TK_LEFTCBRACE)
		complex_statement();
	else 
		simple_statement();
	int save;
	if (fstack->record->curtoken == TK_ELSE) {
		save = ip; ip = hole; emit_addr((save+5) - ip); // save+5 for skipping the 5 byte jmp instruc
		ip = save;
		gen (OP_JMP, NOTTYPE, NOTTYPE);
		hole = ip;
		emit_addr(0);
		match (TK_ELSE);
		if (fstack->record->curtoken == TK_LEFTCBRACE)
			complex_statement();
		else 
			simple_statement();	}
	save = ip; ip = hole; emit_addr(save - ip);
	ip = save;	
}

void switch_statement()
{
	match(TK_SWITCH);
	match(TK_LEFTPARAN);
	Type t = E();
	if (t != INTTYPE) {
		printf ("Error: Invalid Type for Switch in %s at Line: %d and Column %d." 
				" Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
		longjmp(a, 1);
	}
	match(TK_RIGHTPARAN);
	match(TK_LEFTCBRACE);
	int lasthole = 0; 
	int lasthole2 = 0;
	int lsize = 10;
	int holp = 0;
	int* holelist = (int *)malloc(lsize*sizeof(int)); //Assuming about 10 cases on average
	while (fstack->record->curtoken == TK_CASE) {
		if (holp == lsize) { // No space to store 1 hole
			lsize <<= 1;
			int* t = (int *)realloc(holelist, lsize*sizeof(int));
			if (t)
				holelist = t;
			else {
				char* curfile = fstack->record->curfile;
				int curline = fstack->record->curline;
				int curcol = fstack->record->curcol;
				printf("Error: No more memory to allocate hole list array for Switch statement in %s"
					   " at Line: %d and Column %d. Aborting ...\n", curfile, curline, curcol);
				longjmp(a, 1);
			}
		}
		if (lasthole) {
			int save = ip;
			ip = lasthole;
			emit_addr(save - ip);
			if (lasthole2) {
				ip = lasthole2;
				emit_addr(save - ip);
			}
			ip = save;
		}
		gen(OP_DUP, NOTTYPE, NOTTYPE);				
		match(TK_CASE);
		if (fstack->record->curtoken != TK_INTLIT) {
			printf ("Error: Invalid Type for Case in %s at Line: %d and Column %d." 
					" Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		intlit = fstack->record->curtokenvalue.intliteral;
		gettoken();
		gen(OP_PUSHI, NOTTYPE, NOTTYPE);
		if (fstack->record->curtoken == TK_ELLIPSES) {
			gen(TK_LESSTHAN, INTTYPE, INTTYPE);
			gen(OP_JTR, NOTTYPE, NOTTYPE);
			lasthole = ip;
			emit_addr(0);
			gen(OP_DUP, NOTTYPE, NOTTYPE);
			match(TK_ELLIPSES);
			if (fstack->record->curtoken != TK_INTLIT) {
				printf ("Error: Expecting End for Range in Case in %s at Line: %d and Column %d." 
						" Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
				longjmp(a, 1);
			}
			intlit = fstack->record->curtokenvalue.intliteral;
			gettoken();
			gen(OP_PUSHI, NOTTYPE, NOTTYPE);
			gen(TK_GREATERTHAN, INTTYPE, INTTYPE);
			gen(OP_JTR, NOTTYPE, NOTTYPE);
			lasthole2 = ip;
			emit_addr(0);
		}
		else{
			gen(TK_EQUAL2, INTTYPE, INTTYPE);
			gen(OP_JFA, NOTTYPE, NOTTYPE);
			lasthole = ip;
			lasthole2 = 0;
			emit_addr(0);
		}
		match(TK_COLON);
		int numgotos = gotocount;
		if (fstack->record->curtoken == TK_LEFTCBRACE)
			complex_statement();
		else 
			simple_statement();
		gen(OP_JMP, NOTTYPE, NOTTYPE);
		holelist[holp++] = ip;
		emit_addr(0);
		if (numgotos != gotocount) {
			printf ("Error: You have GOTOs inside Case in %s at Line: %d and Column %d." 
					" Cannot balance stack so aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		
	}
	if (fstack->record->curtoken == TK_DEFAULT) {
		match(TK_DEFAULT);
		match(TK_COLON);
		if (lasthole) {
			int save = ip;
			ip = lasthole;
			emit_addr(save - ip);
			if (lasthole2) {
				ip = lasthole2;
				emit_addr(save - ip);
			}
			ip = save;
		}
		if (fstack->record->curtoken == TK_LEFTCBRACE)
			complex_statement();
		else 
			simple_statement();
	}
	gen(OP_POPE, NOTTYPE, NOTTYPE);
	match(TK_RIGHTCBRACE);
	int i;
	int save = ip;
	for (i = holp-1; i>=0; i--) {
		ip = holelist[i];
		emit_addr(save - ip);
	}
	ip = save;
	free(holelist);
}

void goto_statement()
{
	match(TK_GOTO);
	int t = fstack->record->curtoken;	
	if (t == TK_ID) {
		char* curname = fstack->record->curname;
		gen (OP_JMP, NOTTYPE, NOTTYPE);
		int hole = ip;
		emit_addr(0);
		struct gotorecord* gotonode = (struct gotorecord*) malloc (sizeof(struct gotorecord));
		if (gotonode == NULL) {
			printf("Error: No more memory to allocate gotorecord in %s at Line: %d and Column %d. Aborting...\n", 
				   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		gotonode->hole_ip = hole;
		gotonode->curcol = fstack->record->curcol;
		gotonode->curline = fstack->record->curline;
		gotonode->goto_block_info = (char *)malloc((level+2)*sizeof(char));
		strncpy(gotonode->goto_block_info, lvl_str, level+1);
		gotonode->goto_block_info[level+1] = '\0';
		gotonode->next= NULL;
		struct label_info* srchres = lookuplabel(curname);
		if (srchres) {			
			struct gotorecord* lofgotos = srchres->list_of_gotos;
			if (lofgotos == NULL) 
				srchres->list_of_gotos = gotonode;	
			else {
				gotonode->next = srchres->list_of_gotos;
				srchres->list_of_gotos = gotonode;
			}
		}
		else {
			char* name = (char *)malloc((strlen(curname)+1)*sizeof(char));
			strcpy (name, curname);
			struct label_info* loc = (struct label_info*)malloc(sizeof(struct label_info));
			loc->name = name;
			loc->addr = 0;
			loc->seen = 0;
			loc->list_of_gotos = gotonode;
			loc->next = NULL;
			
			if (function_labels){
				loc->next = function_labels;
				function_labels = loc;
			}
			else
				function_labels = loc;	
		}
		match(TK_ID);
		match(TK_SEMICOLON);
		gotocount++;
	}
	else {
		printf ("Error: Expecting identifier in %s at Line: %d and Column %d. Aborting...\n", 
				fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
		longjmp(a, 1);
	}
}

void print_statement()
{
	match (TK_PRINT);
	if (fstack->record->curtoken == TK_STRLIT) {
		char* curstr = fstack->record->strliteral;
		int len = strlen(curstr);
		if (stp + len + 1 > sssize) {
			sssize = stp + len + 1; 
			char* newss = (char *)realloc(ss, sssize*sizeof(char));
			if (newss == NULL) {
				printf("Error: No more memory to allocate ss in %s at Line: %d and Column %d. Aborting...\n", 
					   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
				longjmp(a, 1);
			}
			ss = newss;
		}
		strcpy((ss + stp), curstr);
		free (fstack->record->strliteral);
		fstack->record->strliteral = NULL;
		intlit = stp;
		stp += len+1;
		gen(OP_PRTS, NOTTYPE, NOTTYPE);
		gettoken();
	}
	else {
		Type t = L();
		if (t == INTTYPE)
			gen(OP_PRTI, NOTTYPE, NOTTYPE);
		else if (t == REALTYPE)
			gen(OP_PRTD, NOTTYPE, NOTTYPE);
		else {
			printf ("Error: Trying to print an invalid type in %s at Line: %d and Column %d." 
					" Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
	}
	match(TK_SEMICOLON);
}

void empty_statement()
{
	match(TK_SEMICOLON);
}

void lab_statement(struct label_info* srchres)
{
	srchres->addr = ip;
	srchres->seen = 1;
	srchres->block_info = (char *)malloc((level+2)*sizeof(char)); //Need to copy level + 1 (level is 0 based) + 1 for null terminator
	strncpy(srchres->block_info, lvl_str, level+1);
	(srchres->block_info)[level+1] = '\0';
	srchres->curcol = fstack->record->curcol;
	srchres->curline = fstack->record->curline;
}

void proc_statement(struct symtabrecord* srchres)
{
	char* curname = fstack->record->curname;
	match(TK_ID);
	match(TK_LEFTPARAN);
	match(TK_RIGHTPARAN);
	match(TK_SEMICOLON);
	if (!strcmp(curname, "main")) {
		printf("Error: Call to main detected in %s at Line: %d and Column %d. Aborting...\n", 
			   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
		longjmp(a, 1);
	}
	gen(OP_CALL, NOTTYPE, NOTTYPE);
	int hole = ip;
	emit_addr(0);
	int n = ++(srchres->funcdet->numcalls);
	int* t = (int *)realloc(srchres->funcdet->call_list, n*sizeof(int));
	if (t == NULL) {
		printf("Error: No more memory to allocate call list in %s at Line: %d and Column %d. Aborting...\n", 
			   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
		longjmp(a, 1);
	}
	t[n-1] = hole;
	srchres->funcdet->call_list = t;
}

void declaration()
{
	int decltype = fstack->record->curtoken;
	if (decltype == TK_INT) 
		match(TK_INT);
	else if (decltype == TK_CHAR)
		match (TK_CHAR);
	else if (decltype == TK_DOUBLE)
		match(TK_DOUBLE);
	else {
		printf("Error: Unsupported declaration type (supported: INT, CHAR (INT), DOUBLE) in %s at Line: %d and Column %d."
			   " Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
		longjmp(a, 1);
	}
	match(TK_ID); //This will cause var name matching to lag one behind.
	if (fstack->record->curtoken == TK_LEFTSQPARAN) {
		char* name = (char *)malloc((strlen(fstack->record->curname)+1)*sizeof(char));
		strcpy (name, fstack->record->curname);
		free (fstack->record->curname);
		fstack->record->curname = NULL;
		match(TK_LEFTSQPARAN);
		if (fstack->record->curtoken != TK_INTLIT) {
			printf("Error: Unsupported array size type (supported: INT LITERAL) in %s at Line: %d and Column %d."
				   " Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		int size = fstack->record->curtokenvalue.intliteral;
		gettoken();
		match(TK_RIGHTSQPARAN);
		int i = hashvalue(name, 256);
		struct symtabrecord* lkp = sstack->symtab[i];
		while (lkp) {
			if (strcmp(lkp->name, name) == 0) {
				printf("Error: Found variable name that already exists in this scope: %s \nin %s at Line: %d and"
					   " Column %d. Aborting...\n", name, fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
				longjmp(a, 1);
			}
			lkp = lkp->next;
		}
		
		struct symtabrecord* loc = (struct symtabrecord*)malloc(sizeof(struct symtabrecord));
		loc->name = name;
		loc->tokentype = TK_ARR;
		if ( (decltype == TK_CHAR) || (decltype == TK_INT)) {
			loc->size = size*sizeof(int);
			loc->vartype = INTTYPE;
		}
		else {
			loc->size = size*sizeof(double);
			loc->vartype = REALTYPE;
		}
		loc->funcdet = NULL;
		loc->next = NULL;
		
		if ( (dp + loc->size) >= dssize) {
			dssize += loc->size; 
			char* newds = (char *)realloc(ds, (dssize+1)*sizeof(char));
			if (newds == NULL) {
				printf("Error: No more memory to allocate ds in %s at Line: %d and Column %d. Aborting...\n",
					   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
				longjmp(a, 1);
			}
			ds = newds;
		}
		loc->addr = dp;
		dp += loc->size;
		
		if (sstack->symtab[i]){
			loc->next = sstack->symtab[i];
			sstack->symtab[i] = loc;
		}
		else
			sstack->symtab[i] = loc;
		if (fstack->record->curtoken == TK_COMMA){
			printf("Error: Comma after array declaration. Declare arrays one at a time. Aborting...\n",
				   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);	
		}
		match(TK_SEMICOLON);
	}
	else 
	{
		struct listnode* lvars = NULL;
		while (1) {
			struct listnode* tmp = (struct listnode*)malloc(sizeof(struct listnode));
			if (tmp == NULL) {
				printf("Error: No more memory to allocate variable details in declaration in %s at "
					   "Line: %d and Column %d. Aborting...\n", 
					   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
				longjmp(a, 1);
			}
			tmp->next = lvars;
			lvars = tmp;
			lvars->name = (char *)malloc((strlen(fstack->record->curname)+1)*sizeof(char));
			strcpy(lvars->name, fstack->record->curname);
			free (fstack->record->curname);
			fstack->record->curname = NULL;
			int curtoken = fstack->record->curtoken;		
			if (curtoken == TK_SEMICOLON) {
				match(TK_SEMICOLON);
				break;
			}
			else if (curtoken == TK_COMMA) {
				match(TK_COMMA);
				match(TK_ID);
			}
			else {
				if (curtoken == TK_LEFTSQPARAN) 
					printf("Error: Array declared with other scalars in %s at Line: %d and Column %d. Declare it in its own line. Aborting...\n",
						   fstack->record->curfile,fstack->record->curline, fstack->record->curcol);
				else if (curtoken == TK_LEFTPARAN) 
					printf("Error: Function declaration without void in %s Line: %d and Column %d. Aborting...\n",
						   fstack->record->curfile,fstack->record->curline, fstack->record->curcol);
				else if (curtoken == TK_EQUAL)
					printf("Error: Assignment in declaration in %s at Line: %d and Column %d. Do assignment in its own line. Aborting...\n",
						   fstack->record->curfile,fstack->record->curline, fstack->record->curcol);
				else 
					printf("Error: Missing semicolon in declaration. Current line is in %s at Line: %d and Column %d. Aborting...\n", 
						   fstack->record->curfile,fstack->record->curline, fstack->record->curcol);
				longjmp(a, 1);
			}
		}
		if (lvars == NULL) {
			printf("Error: Invalid syntax in declaration in %s at Line: %d and Column %d. Aborting...\n", 
				   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		
		while (lvars) {
			char* name = (char *)malloc((strlen(lvars->name)+1)*sizeof(char));
			strcpy (name, lvars->name);
			struct listnode* tmp = lvars->next;
			while (tmp) {
				if (strcmp(tmp->name, name) == 0) {
					printf("Error: Duplicated name %s in %s at Line: %d and Column %d. Aborting...\n", 
						   name, fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
					longjmp(a, 1);
				}
				tmp = tmp->next;
			}
			int i = hashvalue(name, 256);
			struct symtabrecord* lkp = sstack->symtab[i];
			while (lkp) {
				if (strcmp(lkp->name, name) == 0) {
					printf("Error: Found variable name that already exists in this scope: %s \nin %s at Line: %d and"
						   " Column %d. Aborting...\n", name, fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
					longjmp(a, 1);
				}
				lkp = lkp->next;
			}
			struct symtabrecord* loc = (struct symtabrecord*)malloc(sizeof(struct symtabrecord));
			loc->name = name;
			loc->tokentype = TK_VAR;
			if ( (decltype == TK_CHAR) || (decltype == TK_INT)) {
				loc->size = sizeof(int);
				loc->vartype = INTTYPE;
			}
			else {
				loc->size = sizeof(double);
				loc->vartype = REALTYPE;
			}
			loc->funcdet = NULL;
			loc->next = NULL;
			
			if ( (dp + loc->size) >= dssize) {
				dssize <<= 1; //At this point, since only doubles or ints are going to be stored, dssize is large enough to put one more atleast
				char* newds = (char *)realloc(ds, (dssize+1)*sizeof(char));
				if (newds == NULL) {
					printf("Error: No more memory to allocate ds in %s at Line: %d and Column %d. Aborting...\n",
						   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
					longjmp(a, 1);
				}
				ds = newds;
			}
			loc->addr = dp;
			dp += loc->size;
			
			if (sstack->symtab[i]){
				loc->next = sstack->symtab[i];
				sstack->symtab[i] = loc;
			}
			else
				sstack->symtab[i] = loc;
			tmp = lvars;
			lvars = lvars->next;
			free (tmp);
		}	
	}
}

void lab_declaration()
{
	struct label_info* lbl = (struct label_info*) malloc(sizeof(struct label_info));
	lbl->list_of_gotos = NULL;
	lbl->block_info = (char *)malloc((level+2)*sizeof(char));
	strncpy(lbl->block_info, lvl_str, level+1);
	(lbl->block_info)[level+2] = '\0';
	lbl->curcol = fstack->record->curcol;
	lbl->curline = fstack->record->curline;
	
	char* curname = fstack->record->curname;
	char* name = (char *)malloc((strlen(curname)+1)*sizeof(char));
	strcpy (name, curname);
	lbl->name = name;	
	lbl->addr = ip;
	lbl->seen = 1;
	lbl->next = NULL;
	if (function_labels){
		lbl->next = function_labels;
		function_labels = lbl;
	}
	else
		function_labels = lbl;	
}

void A(struct symtabrecord* srchres)
{
	// Checking for NULL because A is also called in for loop without checking
	if (srchres == NULL) {			
		printf ("Error: Found unknown identifier: %s in %s at Line: %d and Column %d." 
				" Aborting...\n", fstack->record->curname, fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
		longjmp(a, 1);
	}
	int tt = srchres->tokentype;
	match(TK_ID);
	Type t1 = srchres->vartype;
	if (tt == TK_ARR) {
		match(TK_LEFTSQPARAN);
		Type t = E();
		if (t != INTTYPE) {
			printf ("Error: Invalid indexing type for array in %s at Line: %d and Column %d. Aborting...\n", 
					fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);	
		}
		match(TK_RIGHTSQPARAN);
		intlit = sizeof(int);
		if (t1 == REALTYPE) 
			intlit = sizeof(double);
		gen (OP_PUSHI, NOTTYPE, NOTTYPE); 
		gen (TK_STAR, INTTYPE, INTTYPE);
		intlit = srchres->addr;
		gen (OP_PUSHI, NOTTYPE, NOTTYPE);
		gen (TK_PLUS, INTTYPE, INTTYPE);
		intlit = srchres->size;
		gen (OP_RGCHK, NOTTYPE, NOTTYPE);
		emit_addr(srchres->addr);
		match(TK_EQUAL);
		Type t2 = L();
		gen(OP_PUT, t1, t2);
	}
	else {
		match (TK_EQUAL);
		Type t2 = L();
		intlit = srchres->addr;	
		gen(TK_EQUAL, t1, t2);
	}
}

Type L()
{
	Type t1 = E();
	int curtoken = fstack->record->curtoken;
	int op;
	if (curtoken == TK_EQUAL2 || curtoken == TK_NEQUAL || curtoken == TK_LESSTHAN ||
		   curtoken == TK_GREATERTHAN || curtoken == TK_LESSTHANEQ || curtoken == TK_GREATERTHANEQ){
		match(op = curtoken);
		Type t2 = E();
		t1 = gen (op, t1, t2);
		curtoken = fstack->record->curtoken;
	}
	return t1;
}

Type E()
{
	
	Type t1 = T();
	int curtoken = fstack->record->curtoken;
	int op;
	while (curtoken == TK_PLUS || curtoken == TK_MINUS || curtoken == TK_VERTICALBAR || 
		   curtoken == TK_VERTICALBAR2 || curtoken == TK_CARAT) {
		match(op = curtoken);
		Type t2 = T();
		t1 = gen(op, t1, t2);
		curtoken = fstack->record->curtoken;		
	}
	return t1;
}

Type T()
{
	Type t1 = F();
	int curtoken = fstack->record->curtoken;
	int op;
	while (curtoken == TK_STAR || curtoken == TK_FORWARDSLASH || curtoken == TK_PERCENT || curtoken == TK_AMPERSAND ||
		   curtoken == TK_AMPERSAND2 || curtoken == TK_LESSTHAN2 || curtoken == TK_GREATERTHAN2) {
		match(op = curtoken);
		Type t2 = F();
		t1 = gen(op, t1, t2);
		curtoken = fstack->record->curtoken;
	}
	return t1;
}

Type F()
{
	int curtoken = fstack->record->curtoken;
	if (curtoken == TK_ID) {
		struct symtabrecord* res = lookupidentifier(fstack->record->curname);
		match (TK_ID);
		if (!res) {
			printf ("Error: Unknown identifier in %s at Line: %d and Column %d. Aborting...\n", 
					fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		else {
			if (res->tokentype == TK_ARR) {
				match(TK_LEFTSQPARAN);
				Type t = E();
				if (t != INTTYPE) {
					printf ("Error: Invalid indexing type for array in %s at Line: %d and Column %d. Aborting...\n", 
							fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
					longjmp(a, 1);	
				}
				match(TK_RIGHTSQPARAN);
				intlit = sizeof(int);
				if (res->vartype == REALTYPE) 
					intlit = sizeof(double);
				gen (OP_PUSHI, NOTTYPE, NOTTYPE); 
				gen (TK_STAR, INTTYPE, INTTYPE);
				intlit = res->addr;
				gen (OP_PUSHI, NOTTYPE, NOTTYPE);
				gen (TK_PLUS, INTTYPE, INTTYPE);
				intlit = res->size;
				gen (OP_RGCHK, NOTTYPE, NOTTYPE);
				emit_addr(res->addr);
				if (res->vartype == INTTYPE) {
					gen(OP_GET, NOTTYPE, NOTTYPE);
					return INTTYPE;
				}
				else {
					gen(OP_GETF, NOTTYPE, NOTTYPE);
					return REALTYPE;
				}
			}
			else if (res->tokentype == TK_VAR) {
				intlit = res->addr;
				if (res->vartype == INTTYPE) {
					gen (OP_PUSH, NOTTYPE, NOTTYPE); // Not using gen's ret type because that's NOTTYPE
					return INTTYPE;
				}
				gen (OP_PUSHF, NOTTYPE, NOTTYPE);
				return REALTYPE;
			}
			else 
				return NOTTYPE;
		}
	}
	else if (curtoken == TK_INTLIT) {
		intlit = fstack->record->curtokenvalue.intliteral;
		gen (OP_PUSHI, NOTTYPE, NOTTYPE); 
		gettoken();
		return INTTYPE;
	}
	else if (curtoken == TK_REALLIT) {
		//Do generation of code yourself as gen is only passed int values
		fltlit = fstack->record->curtokenvalue.fltliteral;
		gen (OP_PUSHIF, NOTTYPE, NOTTYPE); 
		gettoken();
		return REALTYPE;
	}
	else if (curtoken == TK_LEFTPARAN) {
		match(TK_LEFTPARAN);
		Type t = L();
		match(TK_RIGHTPARAN);
		return t;
	}
	else if (curtoken == TK_PLUS) {
		match(TK_PLUS);
		Type t = F();
		return t;
	}
	else if (curtoken == TK_MINUS) {
		match(TK_MINUS);
		Type t = F();
		return gen(TK_MINUS, t, NOTTYPE);
	}
	else if (curtoken == TK_EXCLAMATION) {
		match(TK_EXCLAMATION);
		Type t = F();
		return gen(TK_EXCLAMATION, t, NOTTYPE);
	}
	else if (curtoken == TK_TILDE) {
		match(TK_TILDE);
		Type t = F();
		return gen(TK_TILDE, t, NOTTYPE);
	}
	else {
		printf ("Error: Expecting Factor term in %s at Line: %d and Column %d."
				" Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
		longjmp(a, 1);
	}
}

Type gen(int op, Type t1, Type t2) 
{	
	// if not enough space for 4 instructions and a double
	if ( (ip + 4*sizeof(char) + sizeof (double)) >= cssize) { 
		//cssize*2 is large enough to store 4 chars and a double on all machines with the initial val being 1024
		cssize <<= 1; 
		char* newcs = (char *)realloc(cs, (cssize+1)*sizeof(char));
		if (newcs == NULL) {
			printf("Error: No more memory to allocate cs in %s at Line: %d and Column %d. Aborting...\n", 
				   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		cs = newcs;
	}
	
	//There should now be enough space to put any instruction except JTAB on cs
	switch (op) {
		case OP_PUSH:
		case OP_PUSHI:
		case OP_PUSHF:
		case OP_POP:
		case OP_POPF:
		case OP_RGCHK:
		case OP_PRTS:
		{	
			cs[ip++] = (char) op;
			*(int *) (cs + ip) = intlit;
			ip += sizeof(int);
			break;
		}
		case OP_PUSHIF:
		{				
			cs[ip++] = (char) op;
			*(double *) (cs + ip) = fltlit;
			ip += sizeof(double);
			break;
		}
		case OP_CVI:
		case OP_CVR:
		case OP_JTR:
		case OP_JFA:
		case OP_JMP:
		case OP_HALT:
		case OP_DUP:
		case OP_PRTI:
		case OP_PRTD:
		case OP_POPE:
		case OP_GET:
		case OP_GETF:
		case OP_CALL:			
		case OP_RET:
		{	
			cs[ip++] = (char) op;
			break;
		}
		case OP_PUT:
		{
			char o = op;
			if (t1 != t2)
				printf("Warning: Possible loss of information due to type conversion in %s at Line: %d and Column %d.\n",
					   fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			if (t1 == INTTYPE) {
					if (t2 == INTTYPE)
						;
					else 
						o = OP_PUTIF;
			}
			else {
				if (t2 == INTTYPE)
					o = OP_PUTFI;
				else
					o = OP_PUTF;
			}
			cs[ip++] = (char) o;
			break;
		}
		case TK_EQUAL:
		{
			//No return value needed as currently making assignment binary and not an expression.
			if (t1 == INTTYPE) {
				if (t2 == INTTYPE)  {
					gen(OP_POP, NOTTYPE, NOTTYPE);
					return INTTYPE;
				}
				else {
					gen(OP_CVI, NOTTYPE, NOTTYPE);
					gen(OP_POP, NOTTYPE, NOTTYPE);
					printf ("Warning: Possible loss of information due to type conversion in %s at Line: %d and Column %d.\n",
							fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
					return INTTYPE;
				}
			}
			else {
				if (t2 == INTTYPE) {
					gen(OP_CVR, NOTTYPE, NOTTYPE);
					gen(OP_POPF, NOTTYPE, NOTTYPE);
					printf ("Warning: Possible loss of information due to type conversion in %s at Line: %d and Column %d.\n",
							fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
					return REALTYPE;
				}
				else {
					gen (OP_POPF, NOTTYPE, NOTTYPE);
					return REALTYPE;
				}
			}
		}
		case TK_EQUAL2:
		{	
			if (t1 == INTTYPE) {
				if (t2 == INTTYPE)  {
					cs[ip++] = OP_EQL;
					return INTTYPE;
				}
				else {
					cs[ip++] = OP_EXCG;
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_EQLF;
				}
			}
			else {
				if (t2 == INTTYPE) {
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_EQLF;
				}
				else {
					cs[ip++] = OP_EQLF;
				}
			}
			return REALTYPE;
		}
		case TK_NEQUAL:
		{	
			if (t1 == INTTYPE) {
				if (t2 == INTTYPE)  {
					cs[ip++] = OP_NEQL;
					return INTTYPE;
				}
				else {
					cs[ip++] = OP_EXCG;
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_NEQLF;
				}
			}
			else {
				if (t2 == INTTYPE) {
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_NEQLF;
				}
				else {
					cs[ip++] = OP_NEQLF;
				}
			}
			return REALTYPE;
		}
		case TK_LESSTHAN:
		{	
			if (t1 == INTTYPE) {
				if (t2 == INTTYPE)  {
					cs[ip++] = OP_LSS;
					return INTTYPE;
				}
				else {
					cs[ip++] = OP_EXCG;
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_GTEF;
				}
			}
			else {
				if (t2 == INTTYPE) {
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_LSSF;
				}
				else {
					cs[ip++] = OP_LSSF;
				}
			}
			return REALTYPE;
		}
		case TK_GREATERTHAN:
		{	
			if (t1 == INTTYPE) {
				if (t2 == INTTYPE)  {
					cs[ip++] = OP_GTR;
					return INTTYPE;
				}
				else {
					cs[ip++] = OP_EXCG;
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_LSEF;
				}
			}
			else {
				if (t2 == INTTYPE) {
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_GTRF;
				}
				else {
					cs[ip++] = OP_GTRF;
				}
			}
			return REALTYPE;
		}
		case TK_LESSTHANEQ:
		{	
			if (t1 == INTTYPE) {
				if (t2 == INTTYPE)  {
					cs[ip++] = OP_LSE;
					return INTTYPE;
				}
				else {
					cs[ip++] = OP_EXCG;
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_GTRF;
				}
			}
			else {
				if (t2 == INTTYPE) {
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_LSEF;
				}
				else {
					cs[ip++] = OP_LSEF;
				}
			}
			return REALTYPE;
		}
		case TK_GREATERTHANEQ:
		{	
			if (t1 == INTTYPE) {
				if (t2 == INTTYPE)  {
					cs[ip++] = OP_GTE;
					return INTTYPE;
				}
				else {
					cs[ip++] = OP_EXCG;
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_LSSF;
				}
			}
			else {
				if (t2 == INTTYPE) {
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_GTEF;
				}
				else {
					cs[ip++] = OP_GTEF;
				}
			}
			return REALTYPE;
		}
		case TK_PLUS:
		{	
			if (t1 == INTTYPE) {
				if (t2 == INTTYPE)  {
					cs[ip++] = OP_ADD;
					return INTTYPE;
				}
				else {
					cs[ip++] = OP_EXCG;
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_ADDF;
				}
			}
			else {
				if (t2 == INTTYPE) {
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_ADDF;
				}
				else {
					cs[ip++] = OP_ADDF;
					return REALTYPE;
				}
			}
			printf ("Warning: Possible loss of information due to type conversion in %s at Line: %d and Column %d.\n",
					fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			return REALTYPE;
		}
		case TK_MINUS:
		{	
			if (t2 == NOTTYPE) {
				if (t1 == INTTYPE) {
					cs[ip++] = OP_NEG; 
					return INTTYPE;
				}
				printf ("Error: Cannot do Negation on float type in %s at Line: %d and Column %d. Aborting...\n", 
						fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
				longjmp(a, 1);
				
			}
			if (t1 == INTTYPE) {
				if (t2 == INTTYPE)  {
					cs[ip++] = OP_SUB;
					return INTTYPE;
				}
				else {
					cs[ip++] = OP_EXCG;
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_EXCG;
					cs[ip++] = OP_SUBF;
				}
			}
			else {
				if (t2 == INTTYPE) {
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_SUBF;
				}
				else {
					cs[ip++] = OP_SUBF;
					return REALTYPE;
				}
			}
			printf ("Warning: Possible loss of information due to type conversion in %s at Line: %d and Column %d.\n",
					fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			return REALTYPE;
		}
		case TK_VERTICALBAR:
		{	
			if (t1 == INTTYPE && t2 == INTTYPE) {
				cs[ip++] = OP_BOR;
				return INTTYPE;
			}
			printf ("Error: Cannot do the Binary Operation on float type in %s at Line: %d and Column %d." 
					" Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		case TK_AMPERSAND:
		{	
			if (t1 == INTTYPE && t2 == INTTYPE) {
				cs[ip++] = OP_BAND;
				return INTTYPE;
			}
			printf ("Error: Cannot do the Binary Operation on float type in %s at Line: %d and Column %d." 
					" Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		case TK_CARAT:
		{	
			if (t1 == INTTYPE && t2 == INTTYPE) {
				cs[ip++] = OP_BXOR;
				return INTTYPE;
			}
			printf ("Error: Cannot do the Binary Operation on float type in %s at Line: %d and Column %d." 
					" Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		case TK_VERTICALBAR2:
		{	
			if (t1 == INTTYPE) {
				if (t2 == INTTYPE)  {
					cs[ip++] = OP_OR;
					return INTTYPE;
				}
				else {
					cs[ip++] = OP_EXCG;
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_ORF;
				}
			}
			else {
				if (t2 == INTTYPE) {
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_ORF;
				}
				else {
					cs[ip++] = OP_ORF;
				}
			}
			return REALTYPE;
		}
		case TK_STAR:
		{	
			if (t1 == INTTYPE) {
				if (t2 == INTTYPE)  {
					cs[ip++] = OP_MUL;
					return INTTYPE;
				}
				else {
					cs[ip++] = OP_EXCG;
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_MULF;
				}
			}
			else {
				if (t2 == INTTYPE) {
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_MULF;
				}
				else {
					cs[ip++] = OP_MULF;
					return REALTYPE;
				}
			}
			printf ("Warning: Possible loss of information due to type conversion in %s at Line: %d and Column %d.\n",
					fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			return REALTYPE;
		}
		case TK_FORWARDSLASH:
		{	
			if (t1 == INTTYPE) {
				if (t2 == INTTYPE)  {
					cs[ip++] = OP_DIV;
					return INTTYPE;
				}
				else {
					cs[ip++] = OP_EXCG;
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_DIVF;
				}
			}
			else {
				if (t2 == INTTYPE) {
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_DIVF;
				}
				else {
					cs[ip++] = OP_DIVF;
					return REALTYPE;
				}
			}
			printf ("Warning: Possible loss of information due to type conversion in %s at Line: %d and Column %d.\n",
					fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			return REALTYPE;
		}
		case TK_PERCENT:
		{	
			if (t1 == INTTYPE && t2 == INTTYPE) {
				cs[ip++] = OP_MOD;
				return INTTYPE;
			}
			printf ("Error: Cannot do Modulus on float type in %s at Line: %d and Column %d." 
					" Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		case TK_AMPERSAND2:
		{	
			if (t1 == INTTYPE) {
				if (t2 == INTTYPE)  {
					cs[ip++] = OP_AND;
					return INTTYPE;
				}
				else {
					cs[ip++] = OP_EXCG;
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_ANDF;
				}
			}
			else {
				if (t2 == INTTYPE) {
					cs[ip++] = OP_CVR;
					cs[ip++] = OP_ANDF;
				}
				else {
					cs[ip++] = OP_ANDF;
				}
			}
			return REALTYPE;
		}
		case TK_LESSTHAN2:
		{				
			if (t1 == INTTYPE && t2 == INTTYPE) {
				cs[ip++] = OP_LSH;
				return INTTYPE;
			}
			printf ("Error: Cannot do Floating point Left Shift in %s at Line: %d and Column %d." 
					" Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		case TK_GREATERTHAN2:
		{				
			if (t1 == INTTYPE && t2 == INTTYPE) {
				cs[ip++] = OP_RSH;
				return INTTYPE;
			}
			printf ("Error: Cannot do Floating point Right Shift in %s at Line: %d and Column %d." 
					" Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		case TK_EXCLAMATION:
		{
			(t1 == INTTYPE)? (cs[ip++] = OP_NOT): (cs[ip++] = OP_NOTF);
			return t1;	
		}
		case TK_TILDE:
		{
			if (t1 == INTTYPE) {
				cs[ip++] = OP_BNOT;
				return INTTYPE;
			}
			printf ("Error: Cannot do Binary NOT on float type in %s at Line: %d and Column %d." 
					" Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);
		}
		default:
		{
			printf ("Error: I should not have tried to generate code for an unsupported op in %s at Line: %d and Column %d." 
					" Aborting...\n", fstack->record->curfile, fstack->record->curline, fstack->record->curcol);
			longjmp(a, 1);			
			break;
		}
	}
	return NOTTYPE;
}