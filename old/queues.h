#ifndef _queue_h_
#define _queue_h_

typedef struct _qn_{
	int x;
	int y;
	int direction;
	int layer;
	_qn_* next;
}node;

typedef node* qnode;

typedef struct _q_{
	qnode first;
	qnode last;
	int length;
}queue_t;

typedef queue_t* queue;

queue qqcreate();
void qqadd(queue q, int x, int y, int direction, int layer);
void qqpop(queue q);
qnode qqfirst(queue q);
qnode qqlast(queue q);
int qqlength(queue q);
void qqdestroy(queue q);

#endif
