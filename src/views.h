#ifndef _INCL_VIEWS
#define _INCL_VIEWS

/*
** View handlers
*/
void avrCommandHandler(struct mg_connection * connection, int event, void * p);
void avrViewHandler(struct mg_connection * connection, int event, void * p);
void cssHandler(struct mg_connection * connection, int event, void * p);

#endif
