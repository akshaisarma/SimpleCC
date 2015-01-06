#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokens.h"
#include "globals.h"

void gettoken()
{	
	char* scanp;
	int curline;
	int curcol;
	
restart:
	scanp = fstack->record->scanp;
	curline = fstack->record->curline;
	curcol = fstack->record->curcol;
	
	switch (chartype[*scanp]) {
		case NUMBER: 
		{
			int val = 0;
			int base = 10;
			int fract = 0;
			double val2 = 0;
			int temp; // For overflow checking
			int dig;
			while (chartype[*scanp] == NUMBER)
			{
				dig = *scanp - '0';
				scanp++;
				curcol++;
				temp = val * base + dig;
				//Overflow
				if (temp < val) {
					printf ("Error: Overflow at %s at Line: %d and Column: %d. Aborting...", fstack->record->curfile, curline, curcol);
					longjmp(a, 1);
				}
				if ( ! (dig >= 0 && dig <= 9) ) {
					printf ("Error: Malformed decimal number at %s at Line: %d and Column: %d." 
							" Aborting...", fstack->record->curfile, curline, curcol);
					longjmp(a, 1);
				}
				val = temp;
			}
			if (*scanp == '#') 	{
				if (val > 35 || val < 1) 
					printf("Warning: Unsupported base detected at %s at Line: %d and Column: %d. \n", fstack->record->curfile, curline, curcol);
				base = val;
				scanp++;
				curcol++;
				val = 0;
				//No error checking is done on the number except Overflow
				dig = hextable[*scanp];
				while (dig >= 0 && dig <= 35) {
					scanp++;
					curcol++;
					temp = val * base + dig;
					//Overflow
					if (temp < val) {
						printf ("Error: Overflow at %s at Line: %d and Column: %d. Aborting...", fstack->record->curfile, curline, curcol);
						longjmp(a, 1);
					}
					val = temp;
					dig = hextable[*scanp];
				}
			}
		fractional:
			if (*scanp == 'e' || *scanp == '.') {
				scanp++;
				curcol++;
				fract = 1;
				dig = hextable[*scanp];
				while (dig >= 0 && dig <= 35) {
					scanp++;
					curcol++;
					val = val * base + dig;
					fract *= base;
					dig = hextable[*scanp];
				}
			}
			//If fract changed to anything but 0 ...
			if (fract) {
				fstack->record->curtoken = TK_REALLIT;
				val2 = (double) val/ (double) fract;
				fstack->record->curtokenvalue.fltliteral = val2;
			}
			else {
				fstack->record->curtoken = TK_INTLIT;
				fstack->record->curtokenvalue.intliteral = val;
			}
			break;
		}
		case LETTER:
		{
			//Collect letters and digits till significant 31 in C
			char token[32];
			int i = 0;
			while ( (chartype[*scanp] == NUMBER || chartype[*scanp] == LETTER) && i <= 31) 
				token[i++] = *scanp++;	
			token[i] = '\0';
			curcol += i;
			//Skip rest
			while (chartype[*scanp] == NUMBER || chartype[*scanp] == LETTER) {
				curcol++;
				scanp++;
			}
			i = lookupKeyword(token);
			if (i)
				fstack->record->curtoken = i;
			else {
				fstack->record->curtoken = TK_ID;
				if (!fstack->record->curname)
					fstack->record->curname = (char *)malloc(32*sizeof(char));
				strcpy (fstack->record->curname, token);
			}
			break;
		}
		case OPERATOR:
			switch (*scanp) {
				case '(':
				{
					fstack->record->curtoken = TK_LEFTPARAN;
					curcol++;
					scanp++;
					break;
				}
				case ')':
				{
					fstack->record->curtoken = TK_RIGHTPARAN;
					curcol++;
					scanp++;
					break;
				}
				case '[':
				{
					fstack->record->curtoken = TK_LEFTSQPARAN;
					curcol++;
					scanp++;
					break;
				}	
				case ']':
				{
					fstack->record->curtoken = TK_RIGHTSQPARAN;
					curcol++;
					scanp++;
					break;
				}
				case '-':
				{
					fstack->record->curtoken = TK_MINUS;
					curcol++;
					scanp++;
					if (*scanp == '>'){
						fstack->record->curtoken = TK_ARROW;
						curcol++;
						scanp++;
					}
					if (*scanp == '-') {
						fstack->record->curtoken = TK_MINUS2;
						curcol++;
						scanp++;
					}
					break;
				}
				case '.':
				{
					fstack->record->curtoken = TK_PERIOD;
					curcol++;
					scanp++;
					if (*scanp == '.') {
						fstack->record->curtoken = TK_ELLIPSES;
						curcol++;
						scanp++;
					}
					break;
				}
				case '+':
				{
					fstack->record->curtoken = TK_PLUS;
					curcol++;
					scanp++;
					if (*scanp == '+') {
						fstack->record->curtoken = TK_PLUS2;
						curcol++;
						scanp++;
					}
					break;
				}
				case '*':
				{
					fstack->record->curtoken = TK_STAR;
					curcol++;
					scanp++;
					break;
				}
				case '&':
				{
					fstack->record->curtoken = TK_AMPERSAND;
					curcol++;
					scanp++;
					if (*scanp == '&') {
						fstack->record->curtoken = TK_AMPERSAND2;
						curcol++;
						scanp++;
					}
					break;
				}
				case '~':
				{
					fstack->record->curtoken = TK_TILDE;
					curcol++;
					scanp++;
					break;
				}
				case ';':
				{
					fstack->record->curtoken = TK_SEMICOLON;
					curcol++;
					scanp++;
					break;
				}
				case '/':
				{
					if ((chartype [*(scanp+1)] != ENDOFFILE) && *(scanp+1) == '/') {
						scanp += 2;
						while ( (*scanp != '\r') && (*scanp != '\n') ) {
							curcol++;
							scanp++;
						}
						if (*scanp == '\n') {
							curcol = 0;
							curline++;
							scanp++;
						}
						else if ((*scanp == '\r') && (*(scanp+1) == '\n')) {
							curcol = 0;
							curline++;
							scanp += 2;
						}
						else  {
							curcol = 0;
							curline++;
							scanp++;
						}
						fstack->record->scanp = scanp;
						fstack->record->curcol = curcol;
						fstack->record->curline	= curline;
						goto restart;
					}
					if ((chartype [*(scanp+1)] != ENDOFFILE) && (*(scanp+1)) == '*') {
						scanp += 2;
						while (!( (*scanp == ENDOFFILE) || (*scanp == '*' && (*(scanp+1) == ENDOFFILE || *(scanp+1) == '/')) ) ) {
							if (*scanp == '\n') {
								curcol = 0;
								curline++;
								scanp++;
							}
							else if ((*scanp == '\r') && ((chartype [*(scanp+1)] != ENDOFFILE) && *(scanp+1) == '\n') ) {
								curcol = 0;
								curline++;
								scanp += 2;
							}
							else if (*scanp == '\r') {
								curcol = 0;
								curline++;
								scanp++;
							}
							else {
								curcol++;
								scanp++;
							}
						}
						if ( (chartype [*scanp] == ENDOFFILE) || (chartype[*(scanp+1)] == ENDOFFILE) ) {
							printf ("Error: End of file reached while reading comment in %s at Line: %d and Column %d." 
									" Aborting...\n", fstack->record->curfile, curline, curcol);
							longjmp(a, 1);
						}
						scanp += 2;
						fstack->record->scanp = scanp;
						fstack->record->curcol = curcol;
						fstack->record->curline	= curline;
						goto restart;
					}
					fstack->record->curtoken = TK_FORWARDSLASH;
					curcol++;
					scanp++;
					break;
				}  
				case '<':
				{
					fstack->record->curtoken = TK_LESSTHAN ;
					curcol++;
					scanp++;
					if (*scanp == '<') {
						fstack->record->curtoken = TK_LESSTHAN2;
						curcol++;
						scanp++;
					}
					else if (*scanp == '=') {
						fstack->record->curtoken = TK_LESSTHANEQ;
						curcol++;
						scanp++;
					}
					break;
				}
				case '>':
				{
					fstack->record->curtoken = TK_GREATERTHAN ;
					curcol++;
					scanp++;
					if (*scanp == '>') {
						fstack->record->curtoken = TK_GREATERTHAN2;
						curcol++;
						scanp++;
					}
					else if (*scanp == '=') {
						fstack->record->curtoken = TK_GREATERTHANEQ;
						curcol++;
						scanp++;
					}
					break;
				}
				case '=':
				{
					fstack->record->curtoken = TK_EQUAL ;
					curcol++;
					scanp++;
					if (*scanp == '=') {
						fstack->record->curtoken = TK_EQUAL2;
						curcol++;
						scanp++;
					}
					break;
				}
				case '!':
				{
					fstack->record->curtoken = TK_EXCLAMATION ;
					curcol++;
					scanp++;
					if (*scanp == '=') {
						fstack->record->curtoken = TK_NEQUAL;
						curcol++;
						scanp++;
					}
					break;
				}
				case '%':
				{
					fstack->record->curtoken = TK_PERCENT;
					curcol++;
					scanp++;
					break;
				}
				case '|':
				{
					fstack->record->curtoken = TK_VERTICALBAR;
					curcol++;
					scanp++;
					if (*scanp == '|') {
						fstack->record->curtoken = TK_VERTICALBAR2;
						curcol++;
						scanp++;
					}
					break;
				}
				case '^':
				{
					fstack->record->curtoken = TK_CARAT;
					curcol++;
					scanp++;
					break;
				}
				case '?':
				{
					fstack->record->curtoken = TK_QUESTION;
					curcol++;
					scanp++;
					break;
				}
				case ',':
				{
					fstack->record->curtoken = TK_COMMA ;
					curcol++;
					scanp++;
					break;
				}
				case '{':
				{
					fstack->record->curtoken = TK_LEFTCBRACE;
					curcol++;
					scanp++;
					break;
				}
				case '}':
				{
					fstack->record->curtoken = TK_RIGHTCBRACE;
					curcol++;
					scanp++;
					break;
				}
				case ':':
				{
					fstack->record->curtoken = TK_COLON;
					curcol++;
					scanp++;
					break;
				}
				default:
				{
					printf ("Error: Malformed char %c in %s at Line: %d and Column %d."
							" Aborting...\n", *scanp, fstack->record->curfile, curline, curcol);
					longjmp(a, 1);
				}
			}
			break;
		case QUOTE:
		{
			if (*scanp == '\'') {
				int val;
				scanp++;
				val = *scanp;
				scanp++;
				curcol += 2;
				if (*scanp != '\'') {
					printf ("Error: Malformed char in %s at Line: %d and Column %d."
							" Aborting...\n", fstack->record->curfile, curline, curcol);
					longjmp(a, 1);
				}
				scanp++;
				curcol++;
				fstack->record->curtoken = TK_INTLIT;
				fstack->record->curtokenvalue.intliteral = val;
			}
			else {
				char* begin = ++scanp;
				char* end;
				int count = 0; //count escapes
				curcol++;
				while (1) {
					//Skip characters after escape
					if (*scanp == '\\') {
						if (*(scanp+1) == 'x') {
							scanp += 4;
							curcol += 4;
							count += 3; // Hex: 3 chars become one char
						}
						else if ( ( (*(scanp+1) - '0') >= 0) && ( (*(scanp+1) - '0') <= 9) )  {
							scanp += 4;
							curcol += 4;
							count += 3; // Octal: 3 chars become one char
						}
						else {
							scanp += 2;
							curcol += 2;
							count ++;
						}
					}
					else if (*scanp == '\"') {
						curcol++;
						end = scanp;
						scanp++;
						break;
					}
					else {
						if (chartype[*scanp] == ENDOFFILE) {
							printf ("Error: End of file reached while reading string in %s at Line: %d and Column %d." 
									" Aborting...\n", fstack->record->curfile, curline, curcol);
							longjmp(a, 1);
						}
						curcol++;
						scanp++;
					}
				}
				int s = end-begin-count+1;
				char* sliteral = (char *)malloc(s*sizeof(char));
				mystrncpy (sliteral, begin, (s+count-1)); // Copy relevant chars from begin to end
				sliteral[--s] = '\0';
				fstack->record->strliteral = sliteral;
				fstack->record->curtoken = TK_STRLIT;														
			}			
			break;
		}
		case SPACELIKE:
		{
			while (chartype[*scanp] == SPACELIKE) {
				if (*scanp == '\n') {
					curcol = 0;
					curline++;
					scanp++;
				}
				else if ((*scanp == '\r') && (*(scanp+1) == '\n')) {
					curcol = 0;
					curline++;
					scanp += 2;
				}
				else if (*scanp == '\r') {
					curcol = 0;
					curline++;
					scanp++;
				}
				else {
					curcol++;
					scanp++;
				}
			}
			fstack->record->scanp = scanp;
			fstack->record->curcol = curcol;
			fstack->record->curline	= curline;
			goto restart;
			break;
		}
		case ENDOFFILE:
			fstack->record->curtoken = TK_EOF;
			curcol++;
			break;
		case HASH:
		{
			scanp++;
			char inc[] = "include \"";
			int i = 0;
			if (*inc == *scanp) {
				while (inc[i] == *scanp) {
					i++;
					scanp++;
					curcol++;
				}
				i = 0;
				char filename[260];	// Max filename length in Windows
				while (*scanp != '\"') {
					if (chartype [*scanp] == SPACELIKE) {
						printf ("Error: Your include directive should be #include \"...\" No spaces in ...\n"
								"The error is in %s at Line: %d. Aborting...", fstack->record->curfile, curline);
						longjmp(a, 1);
					}
					filename[i++] = *scanp;
					scanp++;
				}
				filename[i] = '\0';
				scanp++;
				fstack->record->scanp = scanp;
				fstack->record->curcol = curcol;
				fstack->record->curline	= curline;
				
				int success = createfilerecord(filename);
				if (!success)
					longjmp(a, 1);
				else
					goto restart;
			}
			else {
				printf ("Error: Your preprocessor directive is malformed. It should be #include \"...\" \n"
						"The error is in %s at line: %d. Aborting...", fstack->record->curfile, curline);
				longjmp(a, 1);
			}
			break;
		}
		case AMBIG: 
		{
			printf ("Error: You have a $, @, or  ` as part of a non-string literal" 
					" somewhere in %s at Column: %d and Line: %d. Aborting...", fstack->record->curfile, curcol, curline);
			longjmp(a, 1);

		}
		case BADCHAR:
		{
			printf ("Error: Bad char in input file. Aborting...\n");
			longjmp(a, 1);
		}
	}
	
	//Replace variables
	fstack->record->scanp = scanp;
	fstack->record->curcol = curcol;
	fstack->record->curline	= curline;
}

int lookupKeyword(char* kword) 
{
	int i = hashvalue(kword, 32);
	struct keywordnode* lctn = keywordtable[i];
	while (lctn) {
		if (strcmp(kword, lctn->name) == 0) 
			return lctn->tokentype;
		lctn = lctn->next;
	}
	return 0;
}

int hashvalue (char* str, int n) {
	int h = 0;
	int i;
	for (i = 0; i< strlen(str); i++)
		h = h+h+str[i];
	return h%n;
}

int createfilerecord(char* filename) 
{
	FILE* fileinp = fopen(filename,"r");
	if (fileinp == NULL) {
		printf ("\nFile: %s could not be found.\n\n", filename);
		return 0;
	}
	struct filedetails* init = (struct filedetails*)malloc(sizeof(struct filedetails));
	init->curfile = (char *) malloc (strlen(filename)+1);
	strcpy (init->curfile, filename);
	
	//Get size of file, store into array and initialize everything
	int c,size;
	for (size=0; (c = fgetc(fileinp)) != EOF; size++) 
		;
	init->filecontents = (char *) malloc (++size);
	fseek(fileinp, 0, SEEK_SET);
	for (c=0; (init->filecontents[c]= fgetc(fileinp)) != EOF; c++) 
		;
	init->filecontents[c] = '\0';
	fclose(fileinp);
	init->scanp = init->filecontents;
	init->curline = 1;
	init->curcol = 1;
	init->curtoken = 0;
	init->curname = NULL;
	init->strliteral = NULL;	
	if (fstack->record){
		struct filestack* temp = (struct filestack*)malloc(sizeof(struct filestack));
		temp->record = init;
		temp->next = fstack;
		fstack = temp;
	}
	else 
		fstack->record = init;
	return 1;
}

void mystrncpy (char* dest, char* src, int num) {
	while (num--) {
		if (*src == '\\') {
			switch (*(src+1)) {
				case 'a':
				{
					*dest = '\a';
					src += 2;
					dest++;
					break;
				}
				case 'n':
				{
					*dest = '\n';
					src += 2;
					dest++;
					break;
				}
				case 't':
				{
					*dest = '\t';
					src += 2;
					dest++;
					break;
				}
				case 'v':
				{
					*dest = '\v';
					src += 2;
					dest++;
					break;
				}
				case 'b':
				{
					*dest = '\b';
					src += 2;
					dest++;
					break;
				}
				case 'r':
				{
					*dest = '\r';
					src += 2;
					dest++;
					break;
				}
				case 'f':
				{
					*dest = '\f';
					src += 2;
					dest++;
					break;
				}
				case '\\':
				{
					*dest = '\\';
					src += 2;
					dest++;
					break;
				}
				case '?':
				{
					*dest = '\?';
					src += 2;
					dest++;
					break;
				}
				case '\'':
				{
					*dest = '\'';
					src += 2;
					dest++;
					break;
				}
				case '\"':
				{
					*dest = '\"';
					src += 2;
					dest++;
					break;
				}
				case 'x':
				{	
					
					char h1 = hextable[*(src+2)];
					char h2 = hextable[*(src+3)];
					char res = h1 * 16 + h2;
					*dest = res;
					src += 4;
					dest++;
					break;
				}
					//Default is octal
				default:
				{
					char h1 = *(src+1) - '0';
					char h2 = *(src+2) - '0';
					char h3 = *(src+3) - '0';
					char res = h1 * 64 + h2*8 + h3;
					*dest = res;
					src += 4;
					dest++;
					break;
				}
			}
		}
		else 
			*dest++ = *src++;	
	}
}
