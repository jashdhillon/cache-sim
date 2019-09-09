#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>


// DEBUG STATES:
// 0: Debug mode off
// 1: Print main debug
// 2: Print some debug
// 3: Print all debug
#define DEBUG 0
#define MAX_STRING_LEN 1000
#define STRING_PARSE_FORMAT "%999s"

#define ADDRESS_SIZE 48
#define BITS_IN_BYTE 8

#define READ 0
#define WRITE 1

#define FIFO 0
#define LRU 1

//Definition of a CacheLine
typedef struct CacheLine
{
	long int tag;
	int isValid;
	int order;

} CacheLine;


//Returns if the integer is a power of 2 or not
int isPowerOfTwo(int i)
{
    return (i > 0) && !(i & (i - 1));
}

//Returns the log base 2 of the integer i
int logBase2(int i)
{
	return (i > 1) ? 1 + logBase2(i / 2) : 0;
}

//Check if string s1 and s2 are equal
int isStringEqual(char* s1, char* s2)
{
	return !strcmp(s1, s2);
}

//Returns the string representation of an integer as an integer
int getIntFromString(char* s)
{
	return atoi(s);
}

//Returns a hex-string converted to an integer
long int getIntFromHexString(char* s)
{
	return strtol(s, NULL, 0);
}

//Returns if the char is a digit
int isStringDigit(char c)
{
	return isdigit(c);
}


//Returns the next "word" from the file that is being read from (ignorning whitespace)
int getNextWord(FILE* file, char* result)
{
	if(!feof(file)) 
	{
		return fscanf(file, STRING_PARSE_FORMAT, result);
	}

	return -1;
}

//Removes the first n characters from the string
void removePrefix(char* str, int n)
{
    	int len = strlen(str);

	if(n > len) n = len;

	memmove(str, str + n, len - n + 1);
}

//Returns the index of the cacheline if it is contained in the cache and returns -1 otherwise
int contains(CacheLine** cache, int setIndex, long int tag, int cacheLineCount)
{
	int i;

	for(i = 0; i < cacheLineCount; i++)
	{
		if(cache[setIndex][i].tag == tag && cache[setIndex][i].isValid)
		{
			return i;
		}
	}

	return -1;
}

//Returns the next index in the given set to be replaced
int getNextIndex(CacheLine** cache, int setIndex, int cacheLineCount)
{
	int i, max = 0, result = 0;

	for(i = 0; i < cacheLineCount; i++)
	{
		if(cache[setIndex][i].order > max)
		{
			max = cache[setIndex][i].order;
			result = i;
		}
	}

	return result;
}

//Updates the order of the CacheLines in the given set by setting the given cachLineIndex to 0 and incrementing the rest by 1 to maintain unique order values for each CacheLine in the given set
void updateOrder(CacheLine** cache, int setIndex, int cacheLineIndex, int cacheLineCount)
{
    	int i;
	for(i = 0; i < cacheLineCount; i++)
	{
		cache[setIndex][i].order++;
	}

	cache[setIndex][cacheLineIndex].order = 0;
}

//Adds the tag to the given set in the given cacheLineIndex and updates the order
void add(CacheLine** cache, int setIndex, int cacheLineIndex, long int tag, int cacheLineCount)
{
	cache[setIndex][cacheLineIndex].tag = tag;
	cache[setIndex][cacheLineIndex].isValid = 1;

	updateOrder(cache, setIndex, cacheLineIndex, cacheLineCount);
}

//The main logic of the cache simulator
int main(int argc, char**argv)
{

	if(argc < 6)
	{
		printf("Oops! Something seems to have gone wrong here! : INVALID ARGUMENT COUNT!\n");
	}

	char* inputFileDir = argv[argc - 1];
	FILE* file = NULL;

	//Cache size (bytes), Set count, Cache lines per set, Block size (bytes)
	int C, S, A, B;

	//Address identifier size (bits), block size (bits), block offset (bits), Set address size (bits), Tag size (bits)
	int a, b, c, s, t;

	char* assoc = argv[2];
	char* repl = argv[3];
	int replPolicy = (isStringEqual(repl, "fifo") ? FIFO : LRU);

	C = getIntFromString(argv[1]);
	B = getIntFromString(argv[4]);

	if(isStringEqual(assoc, "direct"))
	{
	    	A = 1;
		S = C / B;
	}
	else if(isStringEqual(assoc, "assoc"))
	{
		S = 1;
		A = C / B;
	}
	else
	{	removePrefix(assoc, 6);

		A = getIntFromString(assoc);
		S = C / (A * B);
	}

	a = ADDRESS_SIZE;
	b = B * BITS_IN_BYTE;
	c = logBase2(B);
	s = logBase2(S);
	t = a - (c + s);

	//TODO Add code to see if the params are powers of 2's
	
	if(!isPowerOfTwo(C))
	{
		printf("Oops! Something seems to have gone wrong here! : INVALID ARGUMENT - CACHE SIZE IS NOT A POWER OF TWO\n");
	}

	if(!isPowerOfTwo(B))
	{
		printf("Oops! Something seems to have gone wrong here! : INVALID ARGUMENT - BLOCK SIZE IS NOT A POWER OF TWO\n");
	}


	if(!isPowerOfTwo(A))
	{
		printf("Oops! Something seems to have gone wrong here! : INVALID ARGUMENT - ASSOCIATIVITY COUNT IS NOT A POWER OF TWO\n");
	}


	if(DEBUG > 1)
	{
	    	printf("REPLACEMENT POLICY: %s\n\n", repl);

		printf("CACHE SIZE: %d bytes\n", C);
		printf("SET COUNT: %d sets\n", S);
		printf("SET ASSOCIATIVITY: %d cache lines per set\n", A);
		printf("BLOCK SIZE: %d bytes\n\n", B);

		printf("ADDRESS SIZE: %d bites\n", a);
		printf("BLOCK SIZE: %d bits\n", b);
		printf("BLOCK OFFSET: %d bits\n", c);
		printf("SET ADDRESS SIZE: %d bits\n", s);
		printf("TAG SIZE: %d bits\n\n", t);
	}

	int i, j;

	for(j = 0; j < 2; j++)
	{
		int isPrefEnabled = j;

		file = fopen(inputFileDir, "r");

		if(!file)
		{
			printf("Oops! Something seems to have gone wrong here! : FILE COULD NOT BE LOADED!\n");
			return 0;
		}

		CacheLine** cache = (CacheLine**) calloc(S, sizeof(CacheLine*));

		int cacheHits = 0, cacheMisses = 0, memReads = 0, memWrites = 0;

		for(i = 0; i < S; i++)
		{
			cache[i] = (CacheLine*) calloc(A, sizeof(CacheLine));
			cache[i]->isValid = 0;
			cache[i]->order = i;
		}
	
		char* str = calloc(MAX_STRING_LEN, sizeof(char));
		getNextWord(file, str);

		int flag = 0;
		int op = -1;

		while(!feof(file) && !isStringEqual(str, "#eof"))
		{
			if(DEBUG > 1) printf("WORD: %s\n", str);
		
			if(flag == 1)
			{
				//Operation
				op = (*str == 'R') ? READ : WRITE;
			}
			else if(flag == 2)
			{
		    		//Address
				long int address = getIntFromHexString(str);
				long int tag = address >> (s + c);
				long int setIndex = (address >> c) & (S - 1);
				long int blockOffset = address & (B - 1);
			
				if(DEBUG > 2)
				{
					printf("\tOPERATION: %d\n", op);
					printf("\tADDRESS: %lx\n", address);
					printf("\tTAG: %lx\n", tag);
					printf("\tSET INDEX: %lx\n", setIndex);
					printf("\tBLOCK OFFSET: %lx\n", blockOffset);
				}

				int cacheLineIndex = contains(cache, setIndex, tag, A);
			
				if(cacheLineIndex >= 0)
				{
					if(DEBUG > 2) printf("HIT!!!!\n");
					cacheHits++;

					if(replPolicy == LRU)
					{
						updateOrder(cache, setIndex, cacheLineIndex, A);
					}
				}
				else
				{
					if(DEBUG > 2) printf("MISS!!!!\n");
					cacheMisses++;
					memReads++;

					cacheLineIndex = getNextIndex(cache, setIndex, A);
				
					add(cache, setIndex, cacheLineIndex, tag, A);

					if(isPrefEnabled)
					{
					    	address = getIntFromHexString(str) + B;
						tag = address >> (s + c);
						setIndex = (address >> c) & (S - 1);
						blockOffset = address & (B - 1);

						if(setIndex < S)
						{
						    	cacheLineIndex = contains(cache, setIndex, tag, A);

							if(cacheLineIndex < 0)
							{
								cacheLineIndex = getNextIndex(cache, setIndex, A);

								memReads++;
								add(cache, setIndex, cacheLineIndex, tag, A);
							}
						}
					}
				}

				if(op == WRITE)
				{
					//Write operation
					memWrites++;
				}
			}

			getNextWord(file, str);
			
			if(flag == 2) 
			{
		  	  flag = 0;
			}
			else
			{
		 	   flag++;
			}
		}
		
		if(isPrefEnabled)
		{
			printf("with-prefetch\n");
		}
		else
		{
			printf("no-prefetch\n");
		}
		
		printf("Cache hits: %d\n", cacheHits);
		printf("Cache misses: %d\n", cacheMisses);
		printf("Memory reads: %d\n", memReads);
		printf("Memory writes: %d\n", memWrites);

		for(i = 0; i < S; i++)
		{
			free(cache[i]);
		}

		free(cache);
		free(str);

		fclose(file);
	}

	if(DEBUG) printf("END OF PROGRAM\n");
	
	return 0;
}
