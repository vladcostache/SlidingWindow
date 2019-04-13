#ifndef LIST
#define LIST

struct Node { 
  int data; 
  struct Node *next; 
}; 

void printList(struct Node *n) { 
  while (n != NULL) 
  { 
     printf("%d\n", n->data); 
     n = n->next; 
  } 
}

void append(struct Node** head_ref, int new_data) 
{ 
    struct Node* new_node = (struct Node*) malloc(sizeof(struct Node)); 
    struct Node *last = *head_ref; 
    new_node->data  = new_data; 
    new_node->next = NULL; 
    if (*head_ref == NULL) 
    { 
       *head_ref = new_node; 
       return; 
    }
    while (last->next != NULL) 
        last = last->next; 
    last->next = new_node; 
    return; 
}

void removeFirstNode(struct Node** head) 
{ 
    if (*head == NULL) 
       return; 
    *head = (*head)->next;   
}

void deleteNode(struct Node **head_ref, int key) 
{ 
    struct Node* temp = *head_ref, *prev; 
    if (temp != NULL && temp->data == key) 
    { 
        *head_ref = temp->next;   
        free(temp);               
        return; 
    } 

    while (temp != NULL && temp->data != key) 
    { 
        prev = temp; 
        temp = temp->next; 
    } 
    if (temp == NULL) return; 
  
    prev->next = temp->next; 
  
    free(temp); 
}
#endif