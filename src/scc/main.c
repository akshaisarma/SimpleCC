#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokens.h"
#include "globals.h"

//Functions
void freemem();
void inittables();

int main (int argc, char* argv[]) 
{
	if (argc != 2) {
		printf ("Error: Enter one file to compile!\n");
		return 1;
	}
	
	//Filestack
	fstack = (struct filestack*)malloc(sizeof(struct filestack));
	fstack->record = NULL;
	fstack->next = NULL;
	
	//Allocate filedetails struct for input file and push into stack. Then check if it was allocated, quitting if not.
	if (!createfilerecord (argv[1]))
		return 1;
	
	//Init tables
	inittables();
		
	//Start the scanning and parsing
	compile_error = 0;
	compile();
	
	if (!compile_error)
		printf("Compilation succeeeded. A file output.o has been produced. Place it next to the stackmachine executable and run it.\n");
	else 
		printf("Compilation failed...\n");
	
	//Free all dynamic global memory
	freemem();
	return 0;
}

void freemem() 
{
	int i;
	
	/* Free possibly allocated memory */
	while (fstack){
		if (fstack->record->curname)
			free (fstack->record->curname);
		if (fstack->record->strliteral)
			free (fstack->record->strliteral);
		if (fstack->record->curfile)
			free (fstack->record->curfile);
		if (fstack->record->filecontents)
			free (fstack->record->filecontents);
		free (fstack->record);
		struct filestack* temp = fstack->next;
		free (fstack);
		fstack = temp;
	}
	
	for (i = 0; i < 32; i++) {
		while (keywordtable[i]) {
			if (keywordtable[i]->name)
				free (keywordtable[i]->name);
			struct keywordnode* tmp = keywordtable[i]->next;
			free (keywordtable[i]);
			keywordtable[i] = tmp;
		}
	}	   
	
	while (sstack) {
		for (i = 0; i < 256; i++) {
			while (sstack->symtab[i]) {
				if (sstack->symtab[i]->name)
					free (sstack->symtab[i]->name);
				if (sstack->symtab[i]->funcdet) {
					free(sstack->symtab[i]->funcdet->call_list);
					free(sstack->symtab[i]->funcdet);
				}
				struct symtabrecord* tmp = sstack->symtab[i]->next;
				free (sstack->symtab[i]);
				sstack->symtab[i] = tmp;
			}
		}
		struct symtabstack* tmp = sstack->nextsymtab;
		free (sstack);
		sstack = tmp;
	}
	
	free (cs);
	free (ds);
	free (ss);
}

void inittables()
{
	int i;
	for (i = 0; i < 256; i++) {
		// Letter or Underscore
		if ( (i >= 65 && i <= 90) || (i	>= 97 && i <= 122) || (i == 95) )
			chartype[i] = LETTER;
		// Digit
		else if (i >= 48 && i <= 57)
			chartype [i] = NUMBER;
		// Operator
		else if (i == 33 || i == 37 || i == 38 || (i >= 40 && i <= 47) || 
				 (i >= 58 && i <= 63) || (i >= 91 && i <= 94) || (i >= 123 && i <= 126))
			chartype [i] = OPERATOR;
		// Spacelike
		else if (i == 9 || i == 10 || i == 12 || i == 13 || i == 32)
			chartype [i] = SPACELIKE;
		//Dollar, At, Backquote
		else if  (i == 36 || i == 64 || i == 96)
			chartype [i] = AMBIG;
		// Quote
		else if (i == 34 || i == 39)
			chartype [i] = QUOTE;
		// Hash
		else if (i == 35)
			chartype[i] = HASH;
		else if (i == 0)
			chartype[i] = ENDOFFILE;
		else 
			chartype [i] = BADCHAR;
		
		if (i >= '0' && i <= '9')
			hextable [i] = i - '0';
		else if (i >= 'A' && i <= 'Z')
			hextable[i] = i - 'A' + 10 ;
		else if (i >= 97 && i <= 122)
			hextable[i] = i - 'a' + 10;
		else 
			hextable [i] = 64;
		
		if (i < 32) 
			//Set keywordtable to null
			keywordtable[i] = NULL;	
	}
	char keywords[] = "auto$break$case$char$const$continue$default$do$double$else$enum$extern$float$"
	"for$goto$if$int$long$register$return$short$signed$sizeof$static$struct$switch$"
	"typedef$union$unsigned$void$volatile$while$print$";
	int start = 0;
	int end = 0;
	char c = *keywords;
	int tokentype = 310; //Keyword TKs start at 310 and go to 341. Order is same as keywords being read.
	while (c) {
		start = end;
		while (*(keywords+end) != '$') 
			end++;
		char* kwd = (char *) malloc((end-start+1)*sizeof(char));
		strncpy(kwd, keywords+start, end-start);
		end++; //Skip $
		c = *(keywords+end);
		i = hashvalue(kwd,32);
		struct keywordnode* loc = (struct keywordnode*)malloc(sizeof(struct keywordnode));
		loc->name = kwd;
		loc->tokentype = tokentype++;
		loc->next = NULL;
		if (keywordtable[i]){
			loc->next = keywordtable[i];
			keywordtable[i] = loc;
		}
		else
			keywordtable[i] = loc;
	}
	
	//Set up global symbol table. 
	sstack = (struct symtabstack*) malloc(sizeof(struct symtabstack));
	if (sstack == NULL) {
		printf("Error: No memory to allocate symbol table. Aborting...\n");
		longjmp(a, 1);
	}
	for (i = 0; i < 256; i++)
		sstack->symtab[i] = NULL;
	sstack->nextsymtab = NULL;
}

void printtoken()
{
	printf ("The token is: %d\n", fstack->record->curtoken);
	printf ("\tThe file is: %s\n", fstack->record->curfile);
	printf ("\tThe column is: %d\n", fstack->record->curcol);
	printf ("\tThe line is: %d\n", fstack->record->curline);
	if (fstack->record->curtoken == TK_STRLIT)
		printf("\tThe String literal is: %s\n", fstack->record->strliteral);
	if (fstack->record->curtoken == TK_INTLIT)
		printf("\tThe Int/Char literal is: %d\n", fstack->record->curtokenvalue.intliteral);
	if (fstack->record->curtoken == TK_REALLIT)
		printf("\tThe Float literal is: %f\n", fstack->record->curtokenvalue.fltliteral);
}


