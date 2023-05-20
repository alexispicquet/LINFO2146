#include "contiki.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef PACKET_H
#define PACKET_H

#define SEARCH 0
#define SEARCH_ACK 1
#define TIMER 2
#define TIMER_ACK 3
#define DATA 4
#define DATA_ACK 5
#define PARENT 6
#define PARENT_ACK 7

#define COOR 1
#define SENSOR 0

#define TRUE 1
#define FALSE 0

#endif

typedef struct {
    uint8_t status;// : 3;
    uint8_t type;// : 1;
    uint8_t rank;// : 4;
    uint8_t rssi;
} packet_t;

typedef struct list {
    packet_t* head;
    struct list* next;
    linkaddr_t src;
} pkt_list_t;

packet_t* init_pkt(uint8_t status, uint8_t type, uint8_t rank){
    packet_t* pkt = malloc(sizeof(packet_t));
    if(pkt==NULL) return NULL;
    
    pkt -> status = status;
    pkt -> type = type;
    pkt -> rank = rank;
    return pkt;
}

packet_t* init_pkt2(packet_t* ptr){
    packet_t* pkt = malloc(sizeof(packet_t));
    if(pkt==NULL) return NULL;
    
    pkt -> status = ptr -> status;
    pkt -> type = ptr -> type;
    pkt -> rank = ptr -> rank;
    return pkt;
}


void free_pkt(packet_t* pkt) {
    free(pkt);
}

pkt_list_t* init_list(){
    pkt_list_t* l = malloc(sizeof(pkt_list_t));
    if(l==NULL) return NULL;
    l->head=NULL;
    l->next=NULL;
    return l;
}

uint8_t size_list(pkt_list_t* l) {
    uint8_t count = 0;
    pkt_list_t* temp = l;
    
    while(temp != NULL && temp->head != NULL) {
        count++;
        temp = temp->next;
    }
    return count;
}

uint8_t is_better(packet_t* pkt1, packet_t* pkt2) {
    if(pkt2 == NULL) { // end of the list
        return 1;
    }
    if(pkt1->type > pkt2->type) { //COOR > SENSOR
        return 1;
    } else if(pkt1->type==COOR && pkt1->rssi > pkt2->rssi) { // Strongest COOR
        return 1;
    } else if(pkt1->type==SENSOR && pkt1->rank < pkt2->rank) { // Shortest path SENSOR
        return 1;
    }
    // pkt2 <= pkt1
    return 0;
}

/* return value : 0 error, 1 no action, 2 update, 3 insertion*/
uint8_t add_acker(pkt_list_t* l, packet_t* pkt, linkaddr_t src) {
    //error
    if(l==NULL) {
        return 0;
    }
    // Empty list
    if(l->head==NULL) {
        l -> head = pkt;
        l -> src = src;
        return 3;
    }
    // insert 
    pkt_list_t* temp = l;
    
    while(temp != NULL) {
        if(linkaddr_cmp(&(temp->src), &src)!=0) {
            if(temp->head->status == pkt->status){
                return 1;
            } if(pkt->status==PARENT){
                temp->head->status = PARENT;
            } else if(pkt->status==SEARCH){
                temp->head->status = SEARCH;
            }
            return 2;
        }
        if(temp->next == NULL) {
            break;
        }
        temp = temp->next;
    }
    
    temp->next = init_list();
    if(temp->next==NULL) {
        return 0;
    }
    temp->next->head = pkt;
    temp->next->src = src;
    return 4;
}

uint8_t src_in_list(pkt_list_t* l, linkaddr_t src) {
    pkt_list_t* temp = l;
    uint8_t pos = 0;
    
    while(temp != NULL) {
        if(linkaddr_cmp(&(temp->src), &src)!=0) {
            return pos;
        }
        pos++;
        temp = temp->next;
    }
    return 100;
}

void remove_pos(pkt_list_t* l, uint8_t pos) {
    pkt_list_t* temp = l;
    if(pos == 0) {
        temp->head = temp->next->head;
        temp->src = temp->next->src;
    } else {
        uint8_t cnt = 0;
        while(cnt != pos) {
            cnt++;
            temp = temp->next;
        }
    }
    temp->next = temp->next->next;
    return;
}

/* return value : 0 error, 1 no action, 2 update pkt in list, 3 add pkt in list */
uint8_t add_parent(pkt_list_t* l, packet_t* pkt, linkaddr_t src) {
    uint8_t isplaced = FALSE;
    uint8_t canquit = FALSE;
    //error
    if(l==NULL) {
        return 0;
    }
    // Empty list
    if(l->head==NULL) {
        l -> head = pkt;
        l -> src = src;
        return 3;
    }
    pkt_list_t* temp = l;
    // new first
    if(linkaddr_cmp(&(temp->src), &src)==0 && is_better(pkt, temp->head)==1) {
        pkt_list_t* new_node = init_list();
        if(new_node==NULL) {
            return 0;
        }
        new_node->head = temp->head;
        new_node->src = temp->src;
        new_node->next = temp->next;
        
        temp->head = pkt;
        temp->src = src;
        temp->next = new_node;
        return 3;
    }
    //insert in list
    while(temp != NULL) {
        // we found the node
        if(linkaddr_cmp(&(temp->src), &src)!=0) {
        
            // node placed earlier in list -> remove old value
            if(isplaced == TRUE) {
                return 2;
            }
            
            // no changes to be done
            if(temp->head->rssi==pkt->rssi && temp->head->rank==pkt->rank) {
                return 1;
            }
            
            // new values still at this place
            if(is_better(pkt, temp->next->head)==1) {
                // update node signal and rank
                temp->head->rssi = pkt->rssi;
                temp->head->rank = pkt->rank;
                return 2;
            }
            
            // isplaced == FALSE && new values && should be placed later in list
            temp = temp -> next;
            canquit = TRUE;
            
        } else { // no match
            // pkt should be THE NEXT NODE
            if(is_better(pkt, temp->next->head)==1) {
                pkt_list_t* new_node = init_list();
                if(new_node==NULL) {
                    return 0;
                }
                new_node->head = pkt;
                new_node->src = src;
                new_node->next = temp->next;
                
                temp->next = new_node;
                
                //already passed old pkt value
                if(canquit==TRUE) {
                    return 2;
                }
                
                isplaced = TRUE;
                temp = temp->next;
            }
            
            //
            if(temp->next==NULL) {
                break;
            }
            temp = temp->next;
        }
    }
    // already added
    if(isplaced == TRUE) {
        return 4;
    }
    
    temp->next = init_list();
    if(temp->next==NULL) {
        return 0;
    }
    temp->next->head = pkt;
    temp->next->src = src;
    return 5;
}

void free_list(pkt_list_t* l) {
    pkt_list_t* temp;
    while(l != NULL) {
        free_pkt(l->head);
        temp = l;
        l = l->next;
        free(temp);
    }
}
