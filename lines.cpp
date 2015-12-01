#include <stdlib.h>
#include <assert.h> 
#include "lines.h"

llist llmake(){
  llist out = (llist)malloc(sizeof(line_list));
  assert(out); //ensure the llist was made
  out->length = 0; //it is empty
  for(int i = 0; i < MAXLENGTH; i++){
    out->get[i] = 0; //initialise all as null
  }
  return out;
}

//binary search helper function
int bin_search(llist ll, int pivot, int lo, int hi){
  int mid = (lo+hi)/2;
  int piv = ll->get[ll->index[mid]]->pivot;
  if(piv == pivot)return mid; //index found
  if(lo == hi-1)return -1; //index not found
  //recurse
  return bin_search(ll,pivot,piv > pivot ? lo : mid,piv > pivot ? mid : hi);
}
//helper function to find the index
int llget_index(llist ll, int pivot){
  return bin_search(ll, pivot, 0, ll->length);
}

line llget(llist ll, int pivot){
  if(ll->length == 0)return 0;
  int toget = llget_index(ll,pivot); //find index
  return (toget == -1) ? 0 : ll->get[ll->index[toget]];
}

void lladd(llist ll, int pivot, int head, int tail, int layer){
  assert(ll->length < MAXLENGTH-1); //ensure list is not too big
  line_t* nline = (line_t*)malloc(sizeof(line_t));
  nline->head = head;
  nline->tail = tail;
  nline->layer = layer;
  nline->next = 0; //create the line_t to add
  
  if(ll->length == 0){
    ll->index[0] = 0;
    line chain = (line)malloc(sizeof(lines_t));
    chain->pivot = pivot;
    chain->first = nline;
    chain->length = 1;
    ll->get[0] = chain; //first line_t in the bin
  }else{
    int find = llget_index(ll,pivot);
    if(find != -1){ //if there exists a bin already
      nline->next = ll->get[ll->index[find]]->first;
      ll->get[ll->index[find]]->first = nline;
      ll->get[ll->index[find]]->length++;
      //put this new line_t at the beginning of the chain
      //no new bin added, so length does not increase
      return;
    }
    
    int index = ll->length;
    for( ; index >= 0; index--){
      if(ll->get[index] == 0)break; //find an unused bin
    }
    line chain = (line)malloc(sizeof(lines_t));
    chain->pivot = pivot;
    chain->first = nline;
    chain->length = 1;
    ll->get[index] = chain; //guaranteed to be the first line_t in bin
    
    for(int i = ll->length-1; i >= 0; i--){
      if(pivot < ll->get[ll->index[i]]->pivot){
        ll->index[i+1] = ll->index[i]; //shift larger bins up
      }else{
        ll->index[i+1] = index; //insert bin
        break;
      }
    }
  }
  ll->length++; //a new bin was added
}


int llremove(llist ll, int pivot, int point){
  int torem = llget_index(ll,pivot);
  if(torem == -1)return 0; //removing a line that doesn't exist
  if(ll->get[ll->index[torem]]->length == 1){
    //this is the only line in the bin, so remove the bin
    free(ll->get[ll->index[torem]]->first);
    free(ll->get[ll->index[torem]]);
    for(int i = torem; i < ll->length; i++){
      ll->index[i] = ll->index[i+1];
      //shift all remaining bins back
    }
    ll->length--; //a bin was removed
    return 1;
  }
  line_t* focus = ll->get[ll->index[torem]]->first;
  line_t* buf;
  if(focus->head == point || focus->tail == point){
    buf = focus->next;
    free(focus);
    ll->get[ll->index[torem]]->first = buf;
    ll->get[ll->index[torem]]->length--;
    //remove the first line in chain
    return 1;
  }
  while(focus->next){ //scan for match
    if(focus->next->head == point || focus->next->tail == point){
      buf = focus->next->next;
      free(focus->next);
      focus->next = buf;
      ll->get[ll->index[torem]]->length--;
      //remove a following line in chain
      return 1;
    }
    focus = focus->next;
  }
  //otherwise no match
}

void lldestroy(llist ll){
  for(int i = 0; i < ll->length; i++){
    if(ll->get[i] == 0)continue;
    line_t* focus = ll->get[i]->first;
    while(focus){
      line_t* buf = focus->next;
      free(focus);
      focus = buf;
    }
    free(ll->get[i]);
  }
  free(ll);
}
