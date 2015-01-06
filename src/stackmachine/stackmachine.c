#include <stdio.h>
#include <stdlib.h>
#include "opcodes.h"

//Globals for the virtual stack machine

//"Registers"
int ip; //Instruction Pointer
int dp; //Data Pointer
int bp; //Base Pointer
int sp; //Stack Pointer
int stp;//String Pointer
int hp; //Heap Pointer - To be Implemented later

//Stack
struct stackfield {
	union {
		int i;
		double d;
	} stackval;
};

struct stackfield* stack;

//Segments
char* cs; //Code Segment
char* ds; //Data Segment
char* ss; //String Segment

//Functions
void run();

int main()
{
	FILE* fileinp = fopen("output.o", "r");
	if (fileinp == NULL) {
		printf ("\nFile output.o could not be opened for reading input.\n\n");
		return 0;
	}
	fseek(fileinp, 0, SEEK_SET);
	fread(&ip, sizeof(int), 1, fileinp);
	fread(&dp, sizeof(int), 1, fileinp);
	fread(&stp, sizeof(int), 1, fileinp);
	cs = (char *)malloc(ip*sizeof(char));
	ds = (char *)malloc(dp*sizeof(char));
	ss = (char *)malloc(stp*sizeof(char));
	fread(cs, sizeof(char), ip, fileinp);
	fread(ds, sizeof(char), dp, fileinp);
	fread(ss, sizeof(char), stp, fileinp);
	fclose(fileinp);
	run();
	return 0;
}

void run()
{
	ip = 0;
	sp = 0;
	
	int runtimeerr = 0;
	//Initialize stack. Maintaining convention that stack[sp] == First ununsed entry in stack
	stack = (struct stackfield*) realloc(stack, sizeof(struct stackfield));
	while (!runtimeerr) {
		switch (cs[ip++]) 
		{
			case OP_PUSH:
			{
				if (!(stack + sp+1)) {
					struct stackfield* tmp = (struct stackfield*) realloc(stack, (sp+2)*sizeof(struct stackfield));
					if (!tmp) {
						printf("Out of memory to allocate new entry on stack \n");
						runtimeerr = 1;
						break;
					}
					stack = tmp;
				}
				stack[sp++].stackval.i = *(int*)(ds + ( *(int*)(cs + ip)) );
				ip += sizeof(int);
				break;
			}
			case OP_PUSHI:
			{
				if (!(stack + sp+1)) {
					struct stackfield* tmp = (struct stackfield*) realloc(stack, (sp+2)*sizeof(struct stackfield));
					if (!tmp) {
						printf("Out of memory to allocate new entry on stack \n");
						runtimeerr = 1;
						break;
					}
					stack = tmp;
				}
				stack[sp++].stackval.i =  *(int*)(cs + ip);
				ip += sizeof(int);
				break;
			}
			case OP_PUSHF:
			{
				if (!(stack + sp+1)) {
					struct stackfield* tmp = (struct stackfield*) realloc(stack, (sp+2)*sizeof(struct stackfield));
					if (!tmp) {
						printf("Out of memory to allocate new entry on stack \n");
						runtimeerr = 1;
						break;
					}
					stack = tmp;
				}				
				stack[sp++].stackval.d = *(double*)(ds + ( *(int*)(cs + ip)) );
				ip += sizeof(int);
				break;
			}
			case OP_PUSHIF:
			{
				if (!(stack + sp+1)) {
					struct stackfield* tmp = (struct stackfield*) realloc(stack, (sp+2)*sizeof(struct stackfield));
					if (!tmp) {
						printf("Out of memory to allocate new entry on stack \n");
						runtimeerr = 1;
						break;
					}
					stack = tmp;
				}		
				stack[sp++].stackval.d =  *(double*)(cs + ip);
				ip += sizeof(double);
				break;
			}
			case OP_POP:
			{
				*(int *) (ds + (*(int*) (cs + ip)) ) = stack[--sp].stackval.i;
				ip += sizeof(int);
				break;
			}
			case OP_POPF:
			{
				*(double *)(ds + (*(int*) (cs + ip)) ) = stack[--sp].stackval.d;
				ip += sizeof(int);
				break;
			}
			case OP_EXCG:
			{
				int ti = stack[sp-2].stackval.i;
				double td = stack[sp-2].stackval.d;
				stack[sp-2].stackval.i = stack[sp-1].stackval.i;
				stack[sp-2].stackval.d = stack[sp-1].stackval.d;
				stack[sp-1].stackval.i = ti;
				stack[sp-1].stackval.d = td;
				break;
			}
			case OP_ADD: 
			{
				stack[sp-2].stackval.i += stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_SUB: 
			{
				stack[sp-2].stackval.i -= stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_MUL: 
			{
				stack[sp-2].stackval.i *= stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_DIV: 
			{
				stack[sp-2].stackval.i /= stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_MOD: 
			{
				stack[sp-2].stackval.i %= stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_AND: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.i && stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_OR: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.i || stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_LSH: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.i << stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_RSH: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.i >> stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_NEG: 
			{
				stack[sp-1].stackval.i = -stack[sp-1].stackval.i;
				break;
			}
			case OP_NEGF: 
			{
				stack[sp-1].stackval.d = -stack[sp-1].stackval.d;
				break;
			}	
			case OP_LSS: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.i < stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_LSE: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.i <= stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_GTR: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.i > stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_GTE: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.i >= stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_EQL: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.i == stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_NEQL: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.i != stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_ADDF: 
			{
				stack[sp-2].stackval.d += stack[sp-1].stackval.d;
				sp--;
				break;
			}
			case OP_SUBF: 
			{
				stack[sp-2].stackval.d -= stack[sp-1].stackval.d;
				sp--;
				break;
			}
			case OP_MULF: 
			{
				stack[sp-2].stackval.d *= stack[sp-1].stackval.d;
				sp--;
				break;
			}
			case OP_DIVF: 
			{
				stack[sp-2].stackval.d /= stack[sp-1].stackval.d;
				sp--;
				break;
			}
			case OP_ANDF: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.d && stack[sp-1].stackval.d;
				sp--;
				break;
			}
			case OP_ORF: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.d || stack[sp-1].stackval.d;
				sp--;
				break;
			}
			case OP_LSSF: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.d < stack[sp-1].stackval.d;
				sp--;
				break;
			}
			case OP_LSEF: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.d <= stack[sp-1].stackval.d;
				sp--;
				break;
			}
			case OP_GTRF: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.d > stack[sp-1].stackval.d;
				sp--;
				break;
			}
			case OP_GTEF: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.d >= stack[sp-1].stackval.d;
				sp--;
				break;
			}
			case OP_EQLF: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.d == stack[sp-1].stackval.d;
				sp--;
				break;
			}
			case OP_NEQLF: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.d != stack[sp-1].stackval.d;
				sp--;
				break;
			}
			case OP_BAND: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.i & stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_BOR: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.i | stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_BXOR: 
			{
				stack[sp-2].stackval.i = stack[sp-2].stackval.i ^ stack[sp-1].stackval.i;
				sp--;
				break;
			}
			case OP_BNOT: 
			{
				stack[sp-1].stackval.i = ~stack[sp-1].stackval.i;
				break;
			}
			case OP_JMP:
			{
				int t = *(int *) (cs + ip);
				ip += t;
				break;
			}
			case OP_JTR:
			{
				if (stack[sp-1].stackval.i) {
					sp--;
					int t = *(int *) (cs + ip);
					ip += t;
				}
				else {
					sp--;
					ip += sizeof (int);
				}
				break;
			}
			case OP_JFA:
			{
				if (!stack[sp-1].stackval.i) {
					sp--;
					int t = *(int *) (cs + ip);
					ip += t;
				}
				else {
					sp--;
					ip += sizeof (int);
				}
				break;
			}
			case OP_CVI:
			{
				stack[sp-1].stackval.i = (int)stack[sp-1].stackval.d;
				break;
			}
			case OP_CVR:
			{
				stack[sp-1].stackval.d = (double)stack[sp-1].stackval.i;
				break;
			}
			case OP_NOT:
			{
				(stack[sp-1].stackval.i == 0)? (stack[sp-1].stackval.i = 1) : (stack[sp-1].stackval.i = 0);
				break;
			}
			case OP_NOTF:
			{
				(stack[sp-1].stackval.d == 0.0) ? (stack[sp-1].stackval.i = 1.0) : (stack[sp-1].stackval.i = 0.0);
				break;
			}
			case OP_PRTI:
			{
				printf ("%d", stack[sp-1].stackval.i);
				sp--;
				break;
			}
			case OP_PRTD:
			{
				printf ("%f", stack[sp-1].stackval.d);
				sp--;
				break;
			}
			case OP_PRTS:
			{
				printf ("%s", (ss + *(int *) (cs + ip)));
				ip += sizeof(int);
				break;
			}
			case OP_POPE:
			{
				sp--;
				break;
			}
			case OP_HALT:
			{
				free (stack);
				exit (0);
			}
			case OP_DUP:
			{
				if (!(stack + sp+1)) {
					struct stackfield* tmp = (struct stackfield*) realloc(stack, (sp+2)*sizeof(struct stackfield));
					if (!tmp) {
						printf("Out of memory to allocate new entry on stack \n");
						runtimeerr = 1;
						break;
					}
					stack = tmp;
				}
				stack[sp].stackval.i = stack[sp-1].stackval.i;
				sp++;
				break;
			}
			case OP_GET:
			{
				stack[sp-1].stackval.i = *(int *) (ds + stack[sp-1].stackval.i);
				break;
			}
			case OP_PUT:
			{
				*(int *) (ds + stack[sp-2].stackval.i) = stack[sp-1].stackval.i;
				sp -= 2;
				break;
			}
			case OP_GETF:
			{
				stack[sp-1].stackval.d = *(double *) (ds + stack[sp-1].stackval.i);
				break;
			}
			case OP_PUTIF:
			{
				*(int *) (ds + stack[sp-2].stackval.i) = (int) stack[sp-1].stackval.d;
				sp -= 2;
				break;
			}
			case OP_PUTFI:
			{
				*(double *) (ds + stack[sp-2].stackval.i) = (double) stack[sp-1].stackval.i;
				sp -= 2;
				break;
			}	
			case OP_PUTF:
			{
				*(double *) (ds + stack[sp-2].stackval.i) = stack[sp-1].stackval.d;
				sp -= 2;
				break;
			}
			case OP_RGCHK:
			{
				if (stack[sp-1].stackval.i >=  (*(int *)(cs + ip)) + ( *(int *) (cs + ip + sizeof(int)) ) ) {
					printf("RUNTIME ERROR: Range Check Failed. Array index out of bounds\n");
					runtimeerr = 1;
				}
				ip += sizeof(int)<<1;
				break;
			}
			case OP_CALL:
			{
				if (!(stack + sp+1)) {
					struct stackfield* tmp = (struct stackfield*) realloc(stack, (sp+2)*sizeof(struct stackfield));
					if (!tmp) {
						printf("Out of memory to allocate new entry on stack \n");
						runtimeerr = 1;
						break;
					}
					stack = tmp;
				}
				stack[sp++].stackval.i =  ip + sizeof(int);
				ip += *(int *)(cs + ip);				
				break;
			}
			case OP_RET:
			{
				ip = stack[--sp].stackval.i;
				break;
			}
			//To be implemented
			case OP_JTAB:
			{
				break;
			}
			default:
			{
				printf("Unsupported operation was somehow generated by compiler. Aborting...");
				runtimeerr = 1;
			}
		}
	}
	if (runtimeerr)
		free (stack);
}

