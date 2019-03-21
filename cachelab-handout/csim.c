#include "cachelab.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <math.h>

int hits, misses, evicts;

typedef struct {
	int valid;
	int tag;
	int LruNumber;
} Line;

typedef struct {
	Line* lines;
} Set;

typedef struct {
	int set_num;
	int line_num;
	Set* sets;
} Sim_Cache;

int getSet(int addr, int s, int b) {
	int baddr[64];
	int pos = 0;
	while (addr > 0) {
		baddr[pos] = addr % 2;
		addr /= 2;
		pos++;
	}
	int i;
	int set = 0;
	for (i = b; i < b+s; i++) {
		if (i >= pos) {
			break;
		}
		set += pow(2, i-b)*baddr[i];
	}
	return set;
}

int getTag(int addr, int s, int b) {
	int baddr[64];
	int pos = 0;
	while (addr > 0) {
		baddr[pos] = addr % 2;
		addr /= 2;
		pos++;
	}
	int tag = 0;
	int i;
	for (i = b+s; i < 64; i++) {
		if (i >= pos) {
			break;
		}
		tag += pow(2, i-b-s)*baddr[i];
	}
	return tag;
}

int findMinLruNumber(Sim_Cache* sim_cache, int setBits) {
	int E = sim_cache->line_num;
	Set set = sim_cache->sets[setBits];
	int lru = 999;
	int i;
	int pos = 0;
	for (i = 0; i < E; i++) {
		if (set.lines[i].LruNumber < lru) {
			lru = set.lines[i].LruNumber;
			pos = i;
		}
	}
	return pos;
}

void updateLruNumber(Sim_Cache* sim_cache, int setBits, int hitIndex) {
	sim_cache->sets[setBits].lines[hitIndex].LruNumber = 999;
	int i;
	for (i = 0; i < sim_cache->line_num; i++) {
		if (i != hitIndex) {
			sim_cache->sets[setBits].lines[i].LruNumber--;
		}
	}
}

int isMiss(Sim_Cache* sim_cache, int setBits, int tagBits) {
	int i;
	for (i = 0; i < sim_cache->line_num; i++) {
		if (tagBits == sim_cache->sets[setBits].lines[i].tag && sim_cache->sets[setBits].lines[i].valid == 1) {
			updateLruNumber(sim_cache, setBits, i);
			return 0;
		}
	}
	return 1;
}

int updateCache(Sim_Cache* sim_cache, int setBits, int tagBits, int isVerbose) {
	int i;
	for (i = 0; i < sim_cache->line_num; i++) {
		if (sim_cache->sets[setBits].lines[i].valid == 0) {
			sim_cache->sets[setBits].lines[i].tag = tagBits;
			sim_cache->sets[setBits].lines[i].valid = 1;
			updateLruNumber(sim_cache, setBits, i);
			return 0;
		}
	}
	int linenum = findMinLruNumber(sim_cache, setBits);
	sim_cache->sets[setBits].lines[linenum].tag = tagBits;
	sim_cache->sets[setBits].lines[linenum].valid = 1;
	updateLruNumber(sim_cache, setBits, linenum);
	if (isVerbose) {
		printf("evicts ");
	}
	//evicts++;
	return 1;
}

void loadData(Sim_Cache* sim_cache,int addr, int size, int setBits, int tagBits, int isVerbose) {
	if (isVerbose) {
		printf("L %d,%d set:%d, tag:%d ", addr, size, setBits, tagBits);
	}
	if (isMiss(sim_cache, setBits, tagBits) == 1) {
		misses++;
		if (isVerbose) {
			printf("misses ");
		}
		if (updateCache(sim_cache, setBits, tagBits, isVerbose) == 1) {
			evicts++;
		}
	}
	else {
		hits++;
		/*
		int i;
		for (i = 0; i < sim_cache->line_num; i++) {
			if (sim_cache->sets[setBits].lines[i].tag == tagBits && sim_cache->sets[setBits].lines[i].valid == 1) {
				updateLruNumber(sim_cache, setBits, i);
			}
		}
		
		int i;
		for (i = 0; i < sim_cache->line_num; i++) {
			if (sim_cache->sets[setBits].lines[i].tag == tagBits && sim_cache->sets[setBits].lines[i].valid == 1) {
				sim_cache->sets[setBits].lines[i].LruNumber++;
			}
		}
		*/
		if (isVerbose) {
			printf("hits ");
		}
	}
}

void storeData(Sim_Cache* sim_cache, int addr, int size, int setBits, int tagBits, int isVerbose) {
	/*
	if (isVerbose) {
		printf("S %d,%d set:%d, tag:%d ", addr, size, setBits, tagBits);
	}
	if (isMiss(sim_cache, setBits, tagBits) == 1) {
		misses++;
		if (isVerbose) {
			printf("misses ");
		}
		updateCache(sim_cache, setBits, tagBits, isVerbose);
		misses++;
	}
	else {
		hits++;
		if (isVerbose) {
			printf("hits ");
		}
	}
	*/
	loadData(sim_cache, addr, size, setBits, tagBits, isVerbose);
}

void modifyData(Sim_Cache* sim_cache, int addr, int size, int setBits, int tagBits, int isVerbose) {
	/*
	if (isVerbose) {
		printf("M %d,%d set:%d, tag:%d ", addr, size, setBits, tagBits);
	}
	loadData(sim_cache, addr, size, setBits, tagBits, 0);
	storeData(sim_cache, addr, size, setBits, tagBits, 0);
	*/
	loadData(sim_cache, addr, size, setBits, tagBits, isVerbose);
	storeData(sim_cache, addr, size, setBits, tagBits, isVerbose);
}

void init_SimCache(int S, int E, int b, Sim_Cache* sim_cache) {
	sim_cache->set_num = S;
	sim_cache->line_num = E;
	sim_cache->sets = (Set*)malloc(sizeof(Set)*sim_cache->set_num);
	int i;
	for (i = 0; i < sim_cache->set_num; i++) {
		sim_cache->sets[i].lines = (Line*)malloc(sizeof(Line)*sim_cache->line_num);
	}
}

int main(int argc, char** argv)
{
    int opt, s, E, b;
    
    char t[30];
    while (-1 != (opt = getopt(argc, argv, "s:e:b:t:"))) {
	    switch(opt) {
		    case 's':
			    s = atoi(optarg);
			    break;
		    case 'E':
			    E = atoi(optarg);
			    break;
		    case 'b':
			    b = atoi(optarg);
			    break;
		    case 't':
			    strcpy(t, optarg);
			    break;
	    }
    }
    int S = pow(2, s);
    Sim_Cache sim_cache;
	init_SimCache(S, E, b, &sim_cache);
    FILE* fp;
    fp = fopen(t, "r");
    char identifier;
    unsigned address;
    int size;
   
    while (fscanf(fp, "%c %x,%d", &identifier,&address, &size) > 0) {
    	int setBits = getSet(address, s, b);
    	int tagBits = getTag(address, s, b);
        switch(identifier) {
        	case 'L':
	        	loadData(&sim_cache, address, size, setBits, tagBits, 1);
	        	break;
        	case 'S':
	        	storeData(&sim_cache, address, size, setBits, tagBits, 1);
	        	break;
	        case 'M':
	        	modifyData(&sim_cache, address, size, setBits, tagBits, 1);
        	default:
        		break;
        }
        printf("\n");
    }
    fclose(fp);
    printSummary(hits, misses, evicts);
    return 0;
}
