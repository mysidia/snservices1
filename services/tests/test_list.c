#include <stdio.h>
#include <stdlib.h>

#include "../queue.h"

/*
 * declare the element that will go on our linked list.
 * Here, "ent" is a structure containing the list forward/next pointers, and
 * "str" is our own data.
 */
struct _foo {
  LIST_ENTRY(_foo) ent;
  LIST_ENTRY(_foo) ent2;
  char             *str;
};

/*
 * I prefer one-word types (foo_t rather than struct _foo) but the list
 * macros will not handle this, and it would be tricky to rewrite them to
 * do so.
 */
typedef struct _foo foo_t;

/*
 * define the list head.  The type created by this is "struct listhead", and
 * "tl" is a type of this.  If we need more, after this definition, we can
 * just use "struct listhead foo;" and get them.
 */
LIST_HEAD(listhead, _foo);
struct listhead tl, tl2;

void dump_list (char *, struct listhead *);

/*
 * a few elements.  Here, they are static elements.  In a real world they
 * would be malloc()'d, but this is example code.
 */
foo_t  a1 = {LIST_ENTRY_INITIALIZER, LIST_ENTRY_INITIALIZER, "a1"};
foo_t  a2 = {LIST_ENTRY_INITIALIZER, LIST_ENTRY_INITIALIZER, "a2"};
foo_t  a3 = {LIST_ENTRY_INITIALIZER, LIST_ENTRY_INITIALIZER, "a3"};

int
main(int argc, char **argv)
{
  /*
   * initialize the list heads
   */
  LIST_INIT(&tl);
  LIST_INIT(&tl2);

  dump_list("tl", &tl);
  dump_list("tl2", &tl2);

  /*
   * add the elements in the order a1, a2, a3.  Since each is inserted at
   * the list head, walking the list forward will generate "a3, a2, a1"
   * ordering.
   */

  printf("TL:   a1\n");
  printf("TL2:  a3\n");

  LIST_INSERT_HEAD(&tl, &a1, ent);
  LIST_INSERT_HEAD(&tl2, &a3, ent2);

  dump_list("tl", &tl);
  dump_list("tl2", &tl2);

  printf("TL:   a2 a1\n");
  printf("TL2:  a2 a3\n");

  LIST_INSERT_HEAD(&tl, &a2, ent);
  LIST_INSERT_HEAD(&tl2, &a2, ent2);

  dump_list("tl", &tl);
  dump_list("tl2", &tl2);

  printf("TL:   a3 a2 a1\n");
  printf("TL2:  a1 a2 a3\n");

  LIST_INSERT_HEAD(&tl, &a3, ent);
  LIST_INSERT_HEAD(&tl2, &a1, ent2);

  dump_list("tl", &tl);
  dump_list("tl2", &tl2);

  /*
   * remove the middle element, list 2
   */

  LIST_REMOVE(&a2, ent2);

  dump_list("tl", &tl);
  dump_list("tl2", &tl2);

  /*
   * remove the list head, list 2
   */

  LIST_REMOVE(&a1, ent2);

  dump_list("tl", &tl);
  dump_list("tl2", &tl2);

  /*
   * remove the only member.  This sets the head to NULL as well, list 2
   */

  LIST_REMOVE(&a3, ent2);

  dump_list("tl", &tl);
  dump_list("tl2", &tl2);

  /*
   * remove the middle element.
   */

  LIST_REMOVE(&a2, ent);

  dump_list("tl", &tl);
  dump_list("tl2", &tl2);

  /*
   * remove the list head
   */

  LIST_REMOVE(&a3, ent);

  dump_list("tl", &tl);
  dump_list("tl2", &tl2);

  /*
   * remove the only member.  This sets the head to NULL as well.
   */

  LIST_REMOVE(&a1, ent);

  dump_list("tl", &tl);
  dump_list("tl2", &tl2);

  return 0;
}

void
dump_list(char *msg, struct listhead *h)
{
  foo_t *ft;
  int    l1or2;

  if (h == &tl2)
    l1or2 = 2;
  else
    l1or2 = 1;

  printf("LIST DUMP: %s\n", msg);

  if (h == NULL) {
    printf("\tINVALID\n");
    return;
  }

  ft = h->lh_first;
  if (ft == NULL) {
    printf("\tEMPTY\n");
    return;
  }

  while (ft != NULL) {
    printf("\tENTRY:  %p CONTAINS %s\n", ft, ft->str);
    if (l1or2 == 1)
      ft = ft->ent.le_next;
    else
      ft = ft->ent2.le_next;
  }
}
