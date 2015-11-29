/*
LINE:		a chain of line_t structures
- pivot:	the coordinate value shared by all line_t's
- first:	the first line_t in the chain (arbitrary)
- length:	the number of line_t's in the chain

LINE_T:		either horizontal or vertical
- head, tail:	the unique coordinate of each endpoint
- next:		another line of the same pivot value, if it exists

LINE LIST
- get:		an array of pointers to line_t chains
- index:	an array of indices in get to sort pointers in
		order of increasing pivot values
- length:	indicates the current number of line_t chains
*/


#ifndef _LINES_H_
#define _LINES_H_
#define MAXLENGTH 128

typedef struct line_struct{
	int head;   //first point on line
	int tail;   //last point on line
	line_struct* next; //for chains of equal pivots
}line_t;

typedef struct{ //these are bins of lines of equal pivots
	int pivot;
	line_t* first; //first line of the chain
	int length; //length of the chain
}lines_t;

typedef lines_t* line;

typedef struct{
	line get[MAXLENGTH]; //pointers to bins
	int index[MAXLENGTH]; //for sorting/searching
	int length; //size of struct
}line_list;

typedef line_list* llist;

//returns a pointer to a new line list
llist llmake();

//returns the line_t chain with the given pivot
line llget(llist ll, int pivot);

//adds and sorts a new line to a line list
//will add to a chain if the pivot is not new
void lladd(llist ll, int pivot, int head, int tail);

//removes the line_t with the given pivot
void llremove(llist ll, int pivot, int point);

//frees up all memory used by a line list
void lldestroy(llist ll);

#endif
