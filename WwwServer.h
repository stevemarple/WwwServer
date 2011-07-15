#ifndef _WWWSERVER_H
#define _WWWSERVER_H

#define WWW_SERVER_MAX_METHOD_LEN 6
// Maximum length of a URL (excluding terminating null character)
#define WWW_SERVER_MAX_URL_LEN 40
#define WWW_SERVER_MAX_QUERY_LEN 20

#include <Server.h>
#include <SD.h>
#include <Ethernet.h>

#include <avr/pgmspace.h>

#include <IniFile.h>

class WwwServer
{
public:
  // This must match up with methodNames
  enum {
    methodHead = 0,
    methodGet = 1,
    //methodPost = ?,
    methodPut = 2,
    methodDelete = 3,
  };

  // This must match up with responseText and errorDocumentKeys
  enum{
    statusOK = 0, // 200
    //statusCreated, // 201
    statusMovedPermanently, // 301
    statusTemporaryRedirect, // 307
    statusBadRequest,
    //statusUnauthorized,
    statusForbidden,
    statusNotFound,
    statusRequestUriTooLong,
    statusInternalServerError,
  };
  
  enum {
    stateNoClient = 0,
    stateReadingMethod,
    stateGettingHandlerSetUp,
    stateGettingHandler,
    stateReadingHeaders,
    stateUrlToFilename,
    stateRedirectingToDirectory,
    stateFindingLocationSetUp,
    stateFindingLocation,
    stateFindingErrorDocumentSetUp,
    stateFindingErrorDocument,
    stateSendingStatusCode,
    stateRunningDefaultHandler,
    stateSendingFileMimeTypeSetUp,
    stateSendingFileMimeType,
    stateSendingDefaultMimeTypeSetUp,
    stateSendingDefaultMimeType,
    stateSendingFile,
    //stateSendingDirectoryListing,
    stateSendingDirectoryListingHeader,
    stateSendingDirectoryListingBody,
    stateSendingDirectoryListingFooter,
    stateRunningStatusHandler,
    stateClosingConnection,
    stateDisconnecting,
  };

  enum {
    errorNoError = 0,
    errorBufferTooShort = -1,
    errorBadRequest = -2,
    errorFileMissing = -3,
    errorUnknownState = -4,
    errorFileError = -5, // some file I/O error
    errorRequestUriTooLong = -6,
    errorDirectoryNoTrailingSlash = -7,
  };

  // This must match up with handlerNames
  enum {
    handlerDefault = 0,
    handlerForbidden,
    handlerMovedPermanently,
    handlerTemporaryRedirect,
    handlerStatus,
    handlerCgi,
    handlerDirectoryListing, // internal use only
  };

  typedef struct {
    unsigned long requestStarted;
    unsigned long requestCount; // total number of requests
    unsigned long requestTimeWorstCase; // longest duration of request (uS)
    unsigned long taskTimeWorstCase; // longest duration of task (uS)
    int8_t taskWorstCaseState; // corresponding task
  } stats_t;
  
  static char urlStart[];
  static char location[];
  static char contentType[];
  static char textHtml[];
  static char textPlain[];

  static char htmlToTitle[];
  static char titleToH1[];
  static char closeH1[];
  static char closeBodyHtml[];
  
  static const char* methodNames[];
  static const char* responseText[]; // HTTP response code
  static const char* errorDocumentKeys[]; // ini file keys for error docs
  static const char* handlerNames[];

  // Decode base 64 strings
  static boolean b64_decode(unsigned char* buffer, int len);

  //WwwServer(uint16_t port = 80);
  WwwServer(const char* iniFilename, uint16_t port = 80);
  
  boolean begin(char *buffer, int len);
  // void stop(void); // finish with socket and ini file

  void disconnect(void); // finish with current client and reset variables

  int readLineFromClient(char* buffer, int len);
  char* replaceCharByNull(char *s, char c);

  // len is the size of the buffer
  int8_t processRequest(char* buffer, int len);

  int8_t parseMethodUrlQueryString(char* buffer, int len);

  int8_t setHandler(char* buffer, int len);

  int8_t findString(const char** stringTable, char* str) const;

  // Search the ini file for the specified key, but try first with
  // section set to the URL, then try again with each of the parent
  // directories to /.
  int8_t getIniFileValueForUrl(const char* key, char* buffer, int len);

  int8_t getIniFileValueForUrl(const char *key, char* buffer, int len, int pos);
  void redirectToDirectory(void);
  int8_t findLocation(char* buffer, int len);
  int8_t findErrorDocument(char* buffer, int len);
  void sendStatusCode(void);


  int8_t urlToFilename(char *buffer, int len);
  int8_t defaultHandler(char* buffer, int len);

  void sendError(const char* s = NULL);
  int8_t sendFileMimeType(boolean defaultType, char* buffer, int len);
  int8_t sendFile(char* buffer, int len);

  void sendDirectoryListingHeader(void);
  int8_t sendDirectoryListingBody(char *buffer, int len);
  void sendDirectoryListingFooter(void);
  
  void sendStatus(void);
  
  void printHtmlPageHeader(const char* title);
  void printHtmlPageFooter(void);
  
  int8_t getState(void) const;
  const stats_t* getStats(void);
 
protected:
  void updateStats(unsigned long startMicros, int8_t state);
private:

  class GetIniFileValueForUrlState;
  
  // Keep a copy of the port since Server class has no accessor
  int16_t _port;
  IniFile _ini;

  int8_t _method;
  char _url[WWW_SERVER_MAX_URL_LEN+1];
  char _queryString[WWW_SERVER_MAX_QUERY_LEN+1];
  File _file; // The file to be sent. Kept open between requests
  int8_t _handler;
  int8_t _statusCode;
  boolean _isAuthenticated;

  // status information
  stats_t _stats;
  
  Server _server;
  Client _client;

  // State information when accessing ini file
  IniFileState _iniState; 

  // ***** State variables for some member functions. *****
  // Can't use static storage scope inside functions since that results
  // in a compiler error. It would also prevent runing two different
  // web servers (on different ports) simultaneously.

  // State information for processRequest()
  int8_t _state; 
  // In stateSendingFile this is the position in the file.  In
  // stateClosingConnection this is the millis at which the state was
  // entered in order to give a sufficient delay for the data to be
  // received.
  unsigned long _stateData;

  // State information for getIniFileValueForUrl()
  char *_lastSlash; // position of last '/'

};

// Matches #ifndef _WEBSERVER_H
#endif
