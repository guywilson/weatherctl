#ifndef _INCL_VIEWS
#define _INCL_VIEWS

/*
** View handlers
*/
void avrCmdViewHandler(struct mg_connection * connection, int event, void * p);
void avrCalibViewHandler(struct mg_connection * connection, int event, void * p);
void avrCmdCommandHandler(struct mg_connection * connection, int event, void * p);
void avrCalibCommandHandler(struct mg_connection * connection, int event, void * p);
void cssHandler(struct mg_connection * connection, int event, void * p);

#endif
