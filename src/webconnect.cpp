#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "exception.h"
#include "currenttime.h"
#include "webconnect.h"
#include "avrweather.h"
#include "queuemgr.h"
#include "logger.h"
#include "csvhelper.h"

extern "C" {
#include "mongoose.h"
}

#define INT_LENGTH				10

static void nullHandler(struct mg_connection * connection, int event, void * p)
{
	struct http_message *	message;
	const char * 			szMsg = "HTTP/1.1 200 OK\r\n\r\n[No handler registered for URI '%s']";
	char *					pszMethod;
	char *					pszURI;

	Logger & log = Logger::getInstance();

	switch (event) {
		case MG_EV_HTTP_REQUEST:
			message = (struct http_message *)p;

			pszMethod = (char *)malloc(message->method.len + 1);

			if (pszMethod == NULL) {
				throw new Exception("Failed to allocate memory for method...");
			}

			memcpy(pszMethod, message->method.p, message->method.len);
			pszMethod[message->method.len] = 0;

			pszURI = (char *)malloc(message->uri.len + 1);

			if (pszURI == NULL) {
				throw new Exception("Failed to allocate memory for URI...");
			}

			memcpy(pszURI, message->uri.p, message->uri.len);
			pszURI[message->uri.len] = 0;

			log.logInfo("Null Handler: Got %s request for '%s'", pszMethod, pszURI);
			mg_printf(connection, szMsg, pszURI);
			connection->flags |= MG_F_SEND_AND_CLOSE;

			free(pszMethod);
			free(pszURI);
			break;

		default:
			break;
	}
}

static void post(const char * pszHost, const int port, const char * pszPath, char * pszBody)
{
    struct hostent *	server;
    struct sockaddr_in 	serv_addr;
    int					sockfd;
    int					bytes;
    int					sent;
    int					received;
    int					total;
    int					message_size;
    char *				message;
    char				response[4096];
    const char *		pszMsgTemplate = "POST %s HTTP/1.0\r\nContent-Type: application/json\r\nContent-Length: %ld\r\n\r\n";

     /* How big is the message? */
    message_size = 0;
	message_size += strlen(pszMsgTemplate) + INT_LENGTH;
	message_size += strlen(pszPath);
	message_size += strlen(pszBody);

    /* allocate space for the message */
    message = (char *)malloc(message_size);

    if (message == NULL) {
    	throw new Exception("ERROR allocating message buffer");
    }

    /* fill in the parameters */
	sprintf(message, pszMsgTemplate, pszPath, strlen(pszBody));
	strcat(message, pszBody);

    /* create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
    	throw new Exception("ERROR opening socket");
    }

    /* lookup the ip address */
    server = gethostbyname(pszHost);

    if (server == NULL) {
    	throw new Exception("ERROR, no such host");
    }

    /* fill in the structure */
    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    /* connect the socket */
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        throw new Exception("ERROR connecting");
    }

    /* send the request */
    total = strlen(message);

    sent = 0;

    do {
        bytes = write(sockfd, message + sent, total - sent);

        if (bytes < 0) {
            throw new Exception("ERROR writing message to socket");
        }

        if (bytes == 0) {
            break;
        }

        sent += bytes;
    }
    while (sent < total);

    /* receive the response */
    memset(response, 0, sizeof(response));

    total = sizeof(response) - 1;

    received = 0;

    do {
        bytes = read(sockfd, response + received, total - received);

        if (bytes < 0) {
            throw new Exception("ERROR reading response from socket");
        }

        if (bytes == 0) {
            break;
        }

        received += bytes;
    }
    while (received < total);

    if (received == total) {
        throw new Exception("ERROR storing complete response from socket");
    }

    /* close the socket */
    close(sockfd);

    free(message);
}

WebConnector::WebConnector()
{
}

void WebConnector::setConfigLocation(char * pszConfigFile)
{
	strcpy(this->szConfigFile, pszConfigFile);

	queryConfig();
}

void WebConnector::queryConfig()
{
	FILE *			fptr;
	char *			pszToken;
	char *			config = NULL;
	int				fileLength = 0;
	int				bytesRead = 0;
	const char *	tokens = "#=\n\r ";

	Logger & log = Logger::getInstance();

	fptr = fopen(this->szConfigFile, "rt");

	if (fptr == NULL) {
		log.logError("Failed to open config file %s", this->szConfigFile);
		throw new Exception("ERROR reading config");
	}

    fseek(fptr, 0L, SEEK_END);
    fileLength = ftell(fptr);
    rewind(fptr);

	config = (char *)malloc(fileLength);

	if (config == NULL) {
		log.logError("Failed to alocate %d bytes for config file %s", fileLength, this->szConfigFile);
		throw new Exception("Failed to allocate memory for config.");
	}

	/*
	** Read in the config file...
	*/
	bytesRead = fread(config, 1, fileLength, fptr);

	if (bytesRead != fileLength) {
		log.logError("Read %d bytes, but config file %s is %d bytes long", bytesRead, this->szConfigFile, fileLength);
		throw new Exception("Failed to read in config file.");
	}

	fclose(fptr);

	log.logInfo("Read %d bytes from config file %s", bytesRead, this->szConfigFile);

	pszToken = strtok(config, tokens);

	while (pszToken != NULL) {
		if (strcmp(pszToken, "web.host") == 0) {
			pszToken = strtok(NULL, tokens);
			strcpy(this->szHost, pszToken);

			log.logDebug("Got web.host as %s", this->szHost);
		}
		else if (strcmp(pszToken, "web.port") == 0) {
			pszToken = strtok(NULL, tokens);

			this->port = atoi(pszToken);

			log.logDebug("Got web.port as %d", this->port);
		}
		else if (strcmp(pszToken, "web.basepath") == 0) {
			pszToken = strtok(NULL, tokens);

			strcpy(this->szBasePath, pszToken);

			log.logDebug("Got web.basepath as %s", this->szBasePath);
		}
		else if (strcmp(pszToken, "admin.listenport") == 0) {
			pszToken = strtok(NULL, tokens);

			strcpy(this->szListenPort, pszToken);

			log.logDebug("Got admin.listenport as %s", this->szListenPort);
		}
		else if (strcmp(pszToken, "admin.docroot") == 0) {
			pszToken = strtok(NULL, tokens);

			strcpy(this->szDocRoot, pszToken);

			log.logDebug("Got admin.docroot as %s", this->szDocRoot);
		}
		else {
			pszToken = strtok(NULL, tokens);

			log.logDebug("Got token '%s'", pszToken);
		}
	}

	free(config);

	this->isConfigured = true;
}

void WebConnector::postTPH(const char * pszPathSuffix, bool save, char * pszTemperature, char * pszPressure, char * pszHumidity)
{
	char				szBody[128];
	char				szWebPath[256];

	CurrentTime time;
	Logger & log = Logger::getInstance();

	if (!this->isConfigured) {
		strcpy(this->szConfigFile, "./webconfig.cfg");
		queryConfig();
	}

	sprintf(
		szBody,
		"{\n\t\"time\": \"%s\",\n\t\"save\": \"%s\",\n\t\"temperature\": \"%s\",\n\t\"pressure\": \"%s\",\n\t\"humidity\": \"%s\"\n}",
		time.getTimeStamp(),
		save ? "true" : "false",
		pszTemperature,
		pszPressure,
		pszHumidity);

	strcpy(szWebPath, this->szBasePath);
	strcat(szWebPath, pszPathSuffix);

	log.logDebug("Posting to %s [%s]", szWebPath, szBody);
	post(this->getHost(), this->getPort(), szWebPath, szBody);
	log.logDebug("Finished post to %s", szWebPath);
}

void WebConnector::initListener()
{
	Logger & log = Logger::getInstance();

	mg_mgr_init(&mgr, NULL);

	log.logInfo("Setting up listener on port %s", szListenPort);

	connection = mg_bind(&mgr, szListenPort, nullHandler);

	if (connection == NULL) {
		log.logError("Failed to bind to port %s", szListenPort);
		throw new Exception("Faled to bind to address");
	}

	log.logInfo("Bound default handler...");

	mg_set_protocol_http_websocket(connection);
}

void WebConnector::listen()
{
	Logger & log = Logger::getInstance();

	log.logInfo("Listening...");

	while (1) {
		mg_mgr_poll(&mgr, 1000);
	}

	mg_mgr_free(&mgr);
}

void WebConnector::registerHandler(const char * pszURI, void (* handler)(struct mg_connection *, int, void *))
{
	mg_register_http_endpoint(connection, pszURI, handler);
}
